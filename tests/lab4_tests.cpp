#include <cstdio>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

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

TEST(Cardinal, FiniteAndOmegaBehaveAsOrderedCardinals) {
    Cardinal zero;
    Cardinal three = Cardinal::Finite(3);
    Cardinal five = Cardinal::Finite(5);
    Cardinal omega = Cardinal::Omega();

    EXPECT_TRUE(zero.IsFinite());
    EXPECT_EQ(zero.Value(), static_cast<std::size_t>(0));
    EXPECT_TRUE(omega.IsOmega());
    EXPECT_THROW(omega.Value(), std::logic_error);

    EXPECT_TRUE(three.ContainsIndex(2));
    EXPECT_FALSE(three.ContainsIndex(3));
    EXPECT_TRUE(omega.ContainsIndex(std::numeric_limits<std::size_t>::max()));

    EXPECT_EQ(three + Cardinal::Finite(2), five);
    EXPECT_TRUE((three + omega).IsOmega());
    EXPECT_TRUE(three < omega);
    EXPECT_TRUE(omega > five);
    EXPECT_TRUE(three <= Cardinal::Finite(3));
    EXPECT_TRUE(five >= three);
    EXPECT_NE(three, five);
}

TEST(Cardinal, FiniteAdditionChecksOverflow) {
    Cardinal almostMax = Cardinal::Finite(std::numeric_limits<std::size_t>::max());

    EXPECT_THROW(almostMax + Cardinal::Finite(1), std::overflow_error);
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

TEST(LazySequence, EmptySequenceRejectsElementAccess) {
    LazySequence<int> empty;

    EXPECT_EQ(empty.GetLength(), Cardinal::Finite(0));
    EXPECT_EQ(empty.GetMaterializedCount(), static_cast<std::size_t>(0));
    EXPECT_THROW(empty.GetFirst(), std::out_of_range);
    EXPECT_THROW(empty.GetLast(), std::out_of_range);
    EXPECT_THROW(empty.Get(0), std::out_of_range);
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

TEST(LazySequence, SubsequenceChecksFiniteAndInfiniteBounds) {
    int items[] = {10, 20, 30, 40, 50};
    LazySequence<int> finite(items, 5);

    auto middle = finite.GetSubsequence(Cardinal::Finite(1), Cardinal::Finite(3));
    ASSERT_EQ(middle->GetLength(), Cardinal::Finite(3));
    EXPECT_EQ(middle->Get(0), 20);
    EXPECT_EQ(middle->Get(2), 40);

    EXPECT_THROW(finite.GetSubsequence(Cardinal::Finite(3), Cardinal::Finite(1)), std::out_of_range);
    EXPECT_THROW(finite.GetSubsequence(Cardinal::Finite(0), Cardinal::Finite(5)), std::out_of_range);
    EXPECT_THROW(finite.GetSubsequence(Cardinal::Omega(), Cardinal::Finite(3)), std::logic_error);
    EXPECT_THROW(finite.GetSubsequence(Cardinal::Finite(1), Cardinal::Omega()), std::out_of_range);
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

TEST(LazySequence, InfiniteAppendPrependConcatAndRemoveKeepLazyShape) {
    auto naturals = LazySequence<int>::Infinite([](std::size_t index) { return static_cast<int>(index + 1); });

    auto appended = naturals->Append(999);
    EXPECT_TRUE(appended->GetLength().IsOmega());
    EXPECT_EQ(appended->Get(0), 1);
    EXPECT_EQ(appended->Get(10), 11);

    auto prepended = naturals->Prepend(0);
    EXPECT_TRUE(prepended->GetLength().IsOmega());
    EXPECT_EQ(prepended->Get(0), 0);
    EXPECT_EQ(prepended->Get(1), 1);

    int finiteItems[] = {100, 200};
    LazySequence<int> finite(finiteItems, 2);
    auto infiniteFirst = naturals->Concat(finite);
    EXPECT_TRUE(infiniteFirst->GetLength().IsOmega());
    EXPECT_EQ(infiniteFirst->Get(4), 5);

    auto finiteFirst = finite.Concat(*naturals);
    EXPECT_TRUE(finiteFirst->GetLength().IsOmega());
    EXPECT_EQ(finiteFirst->Get(0), 100);
    EXPECT_EQ(finiteFirst->Get(1), 200);
    EXPECT_EQ(finiteFirst->Get(2), 1);
    EXPECT_THROW(finite.Concat(nullptr), std::invalid_argument);

    auto removedOne = naturals->RemoveAt(2);
    EXPECT_TRUE(removedOne->GetLength().IsOmega());
    EXPECT_EQ(removedOne->Get(0), 1);
    EXPECT_EQ(removedOne->Get(1), 2);
    EXPECT_EQ(removedOne->Get(2), 4);

    auto removedRange = naturals->RemoveRange(1, 3);
    EXPECT_TRUE(removedRange->GetLength().IsOmega());
    EXPECT_EQ(removedRange->Get(0), 1);
    EXPECT_EQ(removedRange->Get(1), 5);
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

TEST(LazySequence, MapWhereReduceValidateErrorsAndInfiniteReduce) {
    LazySequence<int> finite([](std::size_t index) { return static_cast<int>(index + 1); }, Cardinal::Finite(5));

    EXPECT_THROW(finite.Map<int>(std::function<int(int)>()), std::invalid_argument);
    EXPECT_THROW(finite.Where(std::function<bool(int)>()), std::invalid_argument);
    EXPECT_THROW(finite.Reduce<int>(0, std::function<int(int, int)>()), std::invalid_argument);
    EXPECT_THROW(finite.ReduceFirstN<int>(3, 0, std::function<int(int, int)>()), std::invalid_argument);

    auto none = finite.Where(std::function<bool(int)>([](int) { return false; }));
    EXPECT_EQ(none->GetLength(), Cardinal::Finite(0));
    EXPECT_THROW(none->Get(0), std::out_of_range);

    auto naturals = LazySequence<int>::Infinite([](std::size_t index) { return static_cast<int>(index + 1); });
    EXPECT_THROW(naturals->Reduce<int>(0, std::function<int(int, int)>([](int acc, int value) {
        return acc + value;
    })), std::logic_error);
    EXPECT_EQ(naturals->ReduceFirstN<int>(5, 0, std::function<int(int, int)>([](int acc, int value) {
        return acc + value;
    })), 15);
}

TEST(LazySequence, TakeMaterializesRequestedPrefix) {
    LazySequence<int> sequence([](std::size_t index) { return static_cast<int>((index + 1) * 10); },
                               Cardinal::Omega());

    auto taken = sequence.Take(4);

    ASSERT_EQ(taken->GetLength(), static_cast<std::size_t>(4));
    EXPECT_EQ(SequenceAt(taken.get(), 0), 10);
    EXPECT_EQ(SequenceAt(taken.get(), 3), 40);
    EXPECT_EQ(sequence.GetMaterializedCount(), static_cast<std::size_t>(4));
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

TEST(Streams, ReadStreamsValidateStateSeekingAndSharedSources) {
    auto sharedSequence = std::shared_ptr<Sequence<int>>(new MutableArraySequence<int>());
    sharedSequence->Append(1)->Append(2);

    SequenceReadStream<int> sequenceStream(sharedSequence);
    sequenceStream.Open();
    EXPECT_EQ(sequenceStream.Seek(2), static_cast<std::size_t>(2));
    EXPECT_THROW(sequenceStream.Read(), EndOfStream);
    EXPECT_THROW(sequenceStream.Seek(3), EndOfStream);
    sequenceStream.Close();
    EXPECT_THROW(sequenceStream.Read(), StreamException);

    EXPECT_THROW(SequenceReadStream<int>(std::shared_ptr<Sequence<int>>()), std::invalid_argument);

    auto sharedLazy = std::shared_ptr<LazySequence<int>>(
        new LazySequence<int>([](std::size_t index) { return static_cast<int>(index + 1); }, Cardinal::Finite(2)));
    LazySequenceReadStream<int> lazyStream(sharedLazy);
    lazyStream.Open();
    EXPECT_EQ(lazyStream.Seek(2), static_cast<std::size_t>(2));
    EXPECT_THROW(lazyStream.Read(), EndOfStream);
    EXPECT_THROW(lazyStream.Seek(3), EndOfStream);
    lazyStream.Close();
    EXPECT_THROW(lazyStream.Read(), StreamException);

    EXPECT_THROW(LazySequenceReadStream<int>(std::shared_ptr<LazySequence<int>>()), std::invalid_argument);
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

TEST(Streams, StringStreamHandlesEmptyInputAndSeekPastEnd) {
    StringReadStream<int> empty("", DeserializeInt);
    empty.Open();
    EXPECT_THROW(empty.Read(), EndOfStream);
    EXPECT_THROW(empty.Read(), EndOfStream);
    empty.Close();
    EXPECT_THROW(empty.Read(), StreamException);

    StringReadStream<int> stream("1 2", DeserializeInt);
    stream.Open();
    EXPECT_THROW(stream.Seek(3), EndOfStream);
    stream.Close();
}

TEST(Streams, SequenceWriteStreamValidatesStateAndSharedDestination) {
    auto destination = std::shared_ptr<Sequence<int>>(new MutableArraySequence<int>());
    SequenceWriteStream<int> stream(destination);

    EXPECT_THROW(stream.Write(10), StreamException);
    stream.Open();
    EXPECT_EQ(stream.Write(10), static_cast<std::size_t>(1));
    EXPECT_EQ(stream.Write(20), static_cast<std::size_t>(2));
    stream.Close();
    EXPECT_THROW(stream.Write(30), StreamException);

    ASSERT_EQ(destination->GetLength(), static_cast<std::size_t>(2));
    EXPECT_EQ(SequenceAt(destination.get(), 0), 10);
    EXPECT_EQ(SequenceAt(destination.get(), 1), 20);

    EXPECT_THROW(SequenceWriteStream<int>(std::shared_ptr<Sequence<int>>()), std::invalid_argument);
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

TEST(Streams, FileStreamsValidateErrorsAndEndState) {
    const std::string missingFile = "lab4_missing_input_file.tmp";
    std::remove(missingFile.c_str());

    FileReadStream<int> missingReader(missingFile, DeserializeInt);
    EXPECT_THROW(missingReader.Open(), StreamException);

    FileReadStream<int> closedReader(missingFile, DeserializeInt);
    EXPECT_THROW(closedReader.Read(), StreamException);

    const std::string filename = "lab4_stream_error_test.tmp";
    {
        FileWriteStream<int> writer(filename, SerializeInt);
        EXPECT_THROW(writer.Write(1), StreamException);
        writer.Open();
        writer.Write(7);
        writer.Close();
        EXPECT_THROW(writer.Write(8), StreamException);
    }

    FileReadStream<int> reader(filename, DeserializeInt);
    reader.Open();
    EXPECT_EQ(reader.Read(), 7);
    EXPECT_THROW(reader.Read(), EndOfStream);
    EXPECT_THROW(reader.Read(), EndOfStream);
    reader.Close();

    FileReadStream<int> seekReader(filename, DeserializeInt);
    seekReader.Open();
    EXPECT_THROW(seekReader.Seek(2), EndOfStream);
    seekReader.Close();

    const std::string invalidPath = "lab4_missing_directory/stream_test.tmp";
    FileWriteStream<int> invalidWriter(invalidPath, SerializeInt);
    EXPECT_THROW(invalidWriter.Open(), StreamException);

    std::remove(filename.c_str());
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
