#include <cstdio>
#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>

#include <gtest/gtest.h>

#include "ArraySequence.h"
#include "LazySequence.h"
#include "MutableArraySequence.h"
#include "Streams.h"

int SequenceAt(Sequence<int>* sequence, std::size_t index) {
    auto* enumerator = sequence->GetEnumerator();
    for (std::size_t i = 0; i <= index; ++i) {
        if (!enumerator->MoveNext()) {
            delete enumerator;
            throw std::out_of_range("Index out of range");
        }
    }

    int value = enumerator->Current();
    delete enumerator;
    return value;
}

int FibonacciRule(Sequence<int>* generated) {
    std::size_t length = generated->GetLength();
    if (length < 2) {
        throw std::logic_error("Need at least two seed items");
    }
    return SequenceAt(generated, length - 1) + SequenceAt(generated, length - 2);
}

TEST(LazySequence, ArrayMaterializationIsLazy) {
    int items[] = {10, 20, 30};
    LazySequence<int> sequence(items, 3);

    EXPECT_EQ(sequence.GetLength(), Cardinal::Finite(3));
    EXPECT_EQ(sequence.GetMaterializedCount(), static_cast<std::size_t>(0));
    EXPECT_EQ(sequence.Get(1), 20);
    EXPECT_EQ(sequence.GetMaterializedCount(), static_cast<std::size_t>(2));
    EXPECT_EQ(sequence.GetLast(), 30);
    EXPECT_THROW(sequence.Get(3), std::out_of_range);
}

TEST(LazySequence, BasicOperationsRemainLazy) {
    int items[] = {2, 3};
    LazySequence<int> base(items, 2);

    auto prepended = base.Prepend(1);
    EXPECT_EQ(prepended->Get(0), 1);
    EXPECT_EQ(prepended->Get(2), 3);

    auto appended = prepended->Append(4);
    EXPECT_EQ(appended->GetLength(), Cardinal::Finite(4));
    EXPECT_EQ(appended->Get(3), 4);

    auto inserted = appended->InsertAt(99, 2);
    EXPECT_EQ(inserted->Get(2), 99);
    EXPECT_EQ(inserted->Get(3), 3);

    auto removed = inserted->RemoveAt(2);
    EXPECT_EQ(removed->Get(2), 3);

    int otherItems[] = {5, 6};
    LazySequence<int> other(otherItems, 2);
    auto concatenated = removed->Concat(other);
    EXPECT_EQ(concatenated->GetLength(), Cardinal::Finite(6));
    EXPECT_EQ(concatenated->Get(4), 5);
}

TEST(LazySequence, InsertAtEndIsRejectedBySpecification) {
    int items[] = {1, 2, 3};
    LazySequence<int> sequence(items, 3);

    EXPECT_THROW(sequence.InsertAt(4, 3), std::out_of_range);
}

TEST(LazySequence, RemoveRangeAndInsertSequence) {
    int items[] = {1, 2, 5};
    LazySequence<int> base(items, 3);

    int insertedItems[] = {3, 4};
    MutableArraySequence<int> insertSequence(insertedItems, 2);
    auto inserted = base.InsertAt(insertSequence, 2);

    ASSERT_EQ(inserted->GetLength(), Cardinal::Finite(5));
    EXPECT_EQ(inserted->Get(0), 1);
    EXPECT_EQ(inserted->Get(1), 2);
    EXPECT_EQ(inserted->Get(2), 3);
    EXPECT_EQ(inserted->Get(3), 4);
    EXPECT_EQ(inserted->Get(4), 5);

    auto removed = inserted->RemoveRange(1, 3);
    ASSERT_EQ(removed->GetLength(), Cardinal::Finite(2));
    EXPECT_EQ(removed->Get(0), 1);
    EXPECT_EQ(removed->Get(1), 5);
    EXPECT_THROW(inserted->RemoveRange(4, 2), std::out_of_range);
}

TEST(LazySequence, InfiniteAndRecurrence) {
    auto naturals = LazySequence<int>::Infinite([](std::size_t index) { return static_cast<int>(index + 1); });

    EXPECT_TRUE(naturals->GetLength().IsOmega());
    EXPECT_EQ(naturals->Get(999), 1000);
    EXPECT_EQ(naturals->Get(Cardinal::Finite(4)), 5);
    EXPECT_THROW(naturals->Get(Cardinal::Omega()), std::logic_error);
    EXPECT_THROW(naturals->GetLast(), std::logic_error);

    auto tail = naturals->GetSubsequence(Cardinal::Finite(10), Cardinal::Omega());
    EXPECT_TRUE(tail->GetLength().IsOmega());
    EXPECT_EQ(tail->Get(Cardinal::Finite(0)), 11);
    EXPECT_EQ(tail->Get(Cardinal::Finite(4)), 15);

    int seedItems[] = {0, 1};
    MutableArraySequence<int> seeds(seedItems, 2);
    LazySequence<int> fibonacci(FibonacciRule, &seeds);
    EXPECT_EQ(fibonacci.Get(10), 55);
    EXPECT_EQ(fibonacci.GetMaterializedCount(), static_cast<std::size_t>(11));
}

TEST(LazySequence, MapWhereReduce) {
    LazySequence<int> finite([](std::size_t index) { return static_cast<int>(index + 1); }, Cardinal::Finite(10));

    auto squares = finite.Map<int>(std::function<int(int)>([](int value) { return value * value; }));
    EXPECT_EQ(squares->Get(3), 16);

    auto evens = finite.Where(std::function<bool(int)>([](int value) { return value % 2 == 0; }));
    EXPECT_EQ(evens->GetLength(), Cardinal::Finite(5));
    EXPECT_EQ(evens->Get(0), 2);
    EXPECT_EQ(evens->Get(4), 10);

    int sum = finite.Reduce<int>(0, std::function<int(int, int)>([](int acc, int value) {
        return acc + value;
    }));
    EXPECT_EQ(sum, 55);
}

TEST(Streams, SequenceAndLazySequenceReadStreams) {
    MutableArraySequence<int> sequence;
    sequence.Append(7)->Append(8)->Append(9);

    SequenceReadStream<int> sequenceStream(sequence);
    EXPECT_THROW(sequenceStream.Read(), StreamException);
    sequenceStream.Open();
    EXPECT_EQ(sequenceStream.Read(), 7);
    EXPECT_EQ(sequenceStream.Seek(1), static_cast<std::size_t>(1));
    EXPECT_EQ(sequenceStream.Read(), 8);
    sequenceStream.Close();

    int lazyItems[] = {1, 2, 3};
    LazySequence<int> lazy(lazyItems, 3);
    LazySequenceReadStream<int> lazyStream(lazy);
    lazyStream.Open();
    EXPECT_EQ(lazyStream.Read(), 1);
    EXPECT_EQ(lazyStream.Seek(2), static_cast<std::size_t>(2));
    EXPECT_EQ(lazyStream.Read(), 3);
    EXPECT_THROW(lazyStream.Read(), EndOfStream);
    lazyStream.Close();
}

TEST(Streams, StringAndSequenceWriteStreams) {
    StringReadStream<int> stringStream("4 5 6", DeserializeInt);
    stringStream.Open();
    EXPECT_EQ(stringStream.Read(), 4);
    EXPECT_EQ(stringStream.Seek(2), static_cast<std::size_t>(2));
    EXPECT_EQ(stringStream.Read(), 6);
    stringStream.Close();

    MutableArraySequence<int> destination;
    SequenceWriteStream<int> writeStream(destination);
    writeStream.Open();
    EXPECT_EQ(writeStream.Write(11), static_cast<std::size_t>(1));
    EXPECT_EQ(writeStream.Write(12), static_cast<std::size_t>(2));
    EXPECT_EQ(destination.GetLength(), static_cast<std::size_t>(2));
    EXPECT_EQ(destination.Get(1), 12);
    writeStream.Close();
}

TEST(Streams, FileStreams) {
    const char* filename = "lab4_stream_test.tmp";

    FileWriteStream<int> writer(filename, SerializeInt);
    writer.Open();
    writer.Write(21);
    writer.Write(34);
    writer.Close();

    FileReadStream<int> reader(filename, DeserializeInt);
    reader.Open();
    EXPECT_EQ(reader.Read(), 21);
    EXPECT_EQ(reader.Read(), 34);
    EXPECT_THROW(reader.Read(), EndOfStream);
    reader.Close();

    std::remove(filename);
}

TEST(Load, MillionElementLazyStream) {
    const std::size_t count = 1000000;
    auto naturals = LazySequence<long long>::Infinite([](std::size_t index) {
        return static_cast<long long>(index + 1);
    });
    LazySequenceReadStream<long long> stream(*naturals);
    stream.Open();

    long long sum = 0;
    for (std::size_t i = 0; i < count; ++i) {
        sum += stream.Read();
    }
    stream.Close();

    EXPECT_EQ(sum, 500000500000LL);
    EXPECT_EQ(naturals->GetMaterializedCount(), count);
}
