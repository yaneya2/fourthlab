#include <cstdio>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "ArraySequence.h"
#include "ForecastCorrection.h"
#include "LazySequence.h"
#include "MutableArraySequence.h"
#include "Streams.h"

inline int DeserializeInt(const std::string &text) {
	return std::stoi(text);
}

inline std::string SerializeInt(const int &value) {
	return std::to_string(value);
}

inline double DeserializeDouble(const std::string &text) {
	return std::stod(text);
}

inline std::string SerializeDouble(const double &value) {
	std::ostringstream out;
	out << value;
	return out.str();
}

int SequenceAt(Sequence<int> *sequence, std::size_t index) {
	auto *enumerator = sequence->GetEnumerator();
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

int FibonacciRule(Sequence<int> *generated) {
	std::size_t length = generated->GetLength();
	if (length < 2) {
		throw std::logic_error("Need at least two seed items");
	}
	return SequenceAt(generated, length - 1) + SequenceAt(generated, length - 2);
}

TEST(Ordinal, FiniteAndOmegaBehaveAsOrderedOrdinals) {
	Ordinal zero;
	Ordinal three = Ordinal::Finite(3);
	Ordinal five = Ordinal::Finite(5);
	Ordinal omega = Ordinal::Omega();

	EXPECT_TRUE(zero.IsFinite());
	EXPECT_EQ(zero.FinitePart(), static_cast<std::size_t>(0));
	EXPECT_TRUE(omega.IsOmega());
	EXPECT_TRUE(omega.IsInfinite());
	EXPECT_EQ(omega.FinitePart(), static_cast<std::size_t>(0));

	EXPECT_TRUE(three.ContainsIndex(2));
	EXPECT_FALSE(three.ContainsIndex(3));
	EXPECT_TRUE(omega.ContainsIndex(std::numeric_limits<std::size_t>::max()));

	EXPECT_EQ(three + Ordinal::Finite(2), five);
	EXPECT_TRUE((three + omega).IsInfinite());
	EXPECT_TRUE(three < omega);
	EXPECT_TRUE(omega > five);
	EXPECT_TRUE(three <= Ordinal::Finite(3));
	EXPECT_TRUE(five >= three);
	EXPECT_NE(three, five);
}

TEST(Ordinal, FiniteAdditionChecksOverflow) {
	Ordinal almostMax = Ordinal::Finite(std::numeric_limits<std::size_t>::max());

	EXPECT_THROW(almostMax + Ordinal::Finite(1), std::overflow_error);
}

TEST(Ordinal, RepresentsOmegaCoefficientAndFinitePart) {
	Ordinal omega = Ordinal::Omega();
	Ordinal omegaTwicePlusThree = Ordinal::FromParts(2, 3);
	Ordinal omegaPlusFive = Ordinal::FromParts(1, 5);

	EXPECT_EQ(omegaTwicePlusThree.OmegaCoefficient(), static_cast<std::size_t>(2));
	EXPECT_EQ(omegaTwicePlusThree.FinitePart(), static_cast<std::size_t>(3));
	EXPECT_FALSE(omegaTwicePlusThree.IsOmega());
	EXPECT_TRUE(omegaTwicePlusThree.IsInfinite());
	EXPECT_EQ(Ordinal::Finite(7).ToString(), "7");
	EXPECT_EQ(omega.ToString(), "omega");
	EXPECT_EQ(omegaTwicePlusThree.ToString(), "omega * 2 + 3");

	EXPECT_GT(omegaTwicePlusThree, Ordinal::FromParts(1, 100));
	EXPECT_GT(omegaPlusFive, omega);
	EXPECT_EQ(Ordinal::Finite(5) + omega, omega);
	EXPECT_EQ(omega + Ordinal::Finite(5), omegaPlusFive);
	EXPECT_EQ(Ordinal::FromParts(2, 7) + Ordinal::FromParts(3, 4), Ordinal::FromParts(5, 4));
}

TEST(LazySequence, ArrayMaterializationIsLazy) {
	int items[] = {10, 20, 30};
	LazySequence<int> sequence(items, 3);

	EXPECT_EQ(sequence.GetLength(), Ordinal::Finite(3));
	EXPECT_EQ(sequence.GetMaterializedCount(), static_cast<std::size_t>(0));
	EXPECT_EQ(sequence.Get(Ordinal::Finite(1)), 20);
	EXPECT_EQ(sequence.GetMaterializedCount(), static_cast<std::size_t>(2));
	EXPECT_EQ(sequence.GetLast(), 30);
	EXPECT_THROW(sequence.Get(Ordinal::Finite(3)), std::out_of_range);
}

TEST(LazySequence, EmptySequenceRejectsElementAccess) {
	LazySequence<int> empty;

	EXPECT_EQ(empty.GetLength(), Ordinal::Finite(0));
	EXPECT_EQ(empty.GetMaterializedCount(), static_cast<std::size_t>(0));
	EXPECT_THROW(empty.GetFirst(), std::out_of_range);
	EXPECT_THROW(empty.GetLast(), std::out_of_range);
	EXPECT_THROW(empty.Get(Ordinal::Finite(0)), std::out_of_range);
}

TEST(LazySequence, BasicOperationsRemainLazy) {
	int items[] = {2, 3};
	LazySequence<int> base(items, 2);

	auto prepended = base.Prepend(1);
	EXPECT_EQ(prepended->Get(Ordinal::Finite(0)), 1);
	EXPECT_EQ(prepended->Get(Ordinal::Finite(2)), 3);

	auto appended = prepended->Append(4);
	EXPECT_EQ(appended->GetLength(), Ordinal::Finite(4));
	EXPECT_EQ(appended->Get(Ordinal::Finite(3)), 4);

	auto inserted = appended->InsertAt(99, 2);
	EXPECT_EQ(inserted->Get(Ordinal::Finite(2)), 99);
	EXPECT_EQ(inserted->Get(Ordinal::Finite(3)), 3);

	int otherItems[] = {5, 6};
	LazySequence<int> other(otherItems, 2);
	auto concatenated = inserted->Concat(other);
	EXPECT_EQ(concatenated->GetLength(), Ordinal::Finite(7));
	EXPECT_EQ(concatenated->Get(Ordinal::Finite(5)), 5);
}

TEST(LazySequence, InsertAtEndIsRejectedBySpecification) {
	int items[] = {1, 2, 3};
	LazySequence<int> sequence(items, 3);

	EXPECT_THROW(sequence.InsertAt(4, 3), std::out_of_range);
}

TEST(LazySequence, SubsequenceChecksFiniteAndInfiniteBounds) {
	int items[] = {10, 20, 30, 40, 50};
	LazySequence<int> finite(items, 5);

	auto middle = finite.GetSubsequence(Ordinal::Finite(1), Ordinal::Finite(3));
	ASSERT_EQ(middle->GetLength(), Ordinal::Finite(3));
	EXPECT_EQ(middle->Get(Ordinal::Finite(0)), 20);
	EXPECT_EQ(middle->Get(Ordinal::Finite(2)), 40);

	EXPECT_THROW(finite.GetSubsequence(Ordinal::Finite(3), Ordinal::Finite(1)), std::out_of_range);
	EXPECT_THROW(finite.GetSubsequence(Ordinal::Finite(0), Ordinal::Finite(5)), std::out_of_range);
	EXPECT_THROW(finite.GetSubsequence(Ordinal::Omega(), Ordinal::Finite(3)), std::out_of_range);
	EXPECT_THROW(finite.GetSubsequence(Ordinal::Finite(1), Ordinal::Omega()), std::out_of_range);
}

TEST(LazySequence, InsertsSequence) {
	int items[] = {1, 2, 5};
	LazySequence<int> base(items, 3);

	int insertedItems[] = {3, 4};
	MutableArraySequence<int> insertSequence(insertedItems, 2);
	auto inserted = base.InsertAt(insertSequence, Ordinal::Finite(2));

	ASSERT_EQ(inserted->GetLength(), Ordinal::Finite(5));
	EXPECT_EQ(inserted->Get(Ordinal::Finite(0)), 1);
	EXPECT_EQ(inserted->Get(Ordinal::Finite(1)), 2);
	EXPECT_EQ(inserted->Get(Ordinal::Finite(2)), 3);
	EXPECT_EQ(inserted->Get(Ordinal::Finite(3)), 4);
	EXPECT_EQ(inserted->Get(Ordinal::Finite(4)), 5);
}

TEST(LazySequence, InfiniteAndRecurrence) {
	auto naturals = LazySequence<int>::Infinite([](std::size_t index) { return static_cast<int>(index + 1); });

	EXPECT_TRUE(naturals->GetLength().IsInfinite());
	EXPECT_EQ(naturals->Get(Ordinal::Finite(999)), 1000);
	EXPECT_EQ(naturals->Get(Ordinal::Finite(4)), 5);
	EXPECT_THROW(naturals->Get(Ordinal::Omega()), std::out_of_range);
	EXPECT_THROW(naturals->GetLast(), std::logic_error);

	auto tail = naturals->GetSubsequence(Ordinal::Finite(10), Ordinal::Omega());
	EXPECT_TRUE(tail->GetLength().IsInfinite());
	EXPECT_EQ(tail->Get(Ordinal::Finite(0)), 11);
	EXPECT_EQ(tail->Get(Ordinal::Finite(4)), 15);

	int seedItems[] = {0, 1};
	MutableArraySequence<int> seeds(seedItems, 2);
	LazySequence<int> fibonacci(FibonacciRule, &seeds);
	EXPECT_EQ(fibonacci.Get(Ordinal::Finite(10)), 55);
	EXPECT_EQ(fibonacci.GetMaterializedCount(), static_cast<std::size_t>(11));
}

TEST(LazySequence, InfiniteAppendPrependAndConcatKeepOrdinalShape) {
	auto naturals = LazySequence<int>::Infinite([](std::size_t index) { return static_cast<int>(index + 1); });
	auto shiftedNaturals = LazySequence<int>::Infinite([](std::size_t index) { return static_cast<int>(index + 101); });

	auto appended = naturals->Append(999);
	EXPECT_EQ(appended->GetLength(), Ordinal::FromParts(1, 1));
	EXPECT_EQ(appended->Get(Ordinal::Finite(0)), 1);
	EXPECT_EQ(appended->Get(Ordinal::Finite(10)), 11);
	EXPECT_EQ(appended->Get(Ordinal::Omega()), 999);

	auto prepended = naturals->Prepend(0);
	EXPECT_EQ(prepended->GetLength(), Ordinal::Omega());
	EXPECT_EQ(prepended->Get(Ordinal::Finite(0)), 0);
	EXPECT_EQ(prepended->Get(Ordinal::Finite(1)), 1);

	int finiteItems[] = {100, 200};
	LazySequence<int> finite(finiteItems, 2);
	auto infiniteFirst = naturals->Concat(finite);
	EXPECT_EQ(infiniteFirst->GetLength(), Ordinal::FromParts(1, 2));
	EXPECT_EQ(infiniteFirst->Get(Ordinal::Finite(4)), 5);
	EXPECT_EQ(infiniteFirst->Get(Ordinal::Omega()), 100);
	EXPECT_EQ(infiniteFirst->Get(Ordinal::FromParts(1, 1)), 200);

	auto finiteFirst = finite.Concat(*naturals);
	EXPECT_EQ(finiteFirst->GetLength(), Ordinal::Omega());
	EXPECT_EQ(finiteFirst->Get(Ordinal::Finite(0)), 100);
	EXPECT_EQ(finiteFirst->Get(Ordinal::Finite(1)), 200);
	EXPECT_EQ(finiteFirst->Get(Ordinal::Finite(2)), 1);
	EXPECT_THROW(finite.Concat(nullptr), std::invalid_argument);

	auto twoInfiniteParts = naturals->Concat(*shiftedNaturals);
	EXPECT_EQ(twoInfiniteParts->GetLength(), Ordinal::Omega(2));
	EXPECT_EQ(twoInfiniteParts->Get(Ordinal::FromParts(1, 7)), 108);
}

TEST(LazySequence, MapWhereReduce) {
	LazySequence<int> finite([](std::size_t index) { return static_cast<int>(index + 1); }, Ordinal::Finite(10));

	auto squares = finite.Map<int>(std::function<int(int)>([](int value) { return value * value; }));
	EXPECT_EQ(squares->Get(Ordinal::Finite(3)), 16);

	auto evens = finite.Where(std::function<bool(int)>([](int value) { return value % 2 == 0; }));
	EXPECT_EQ(evens->GetLength(), Ordinal::Finite(5));
	EXPECT_EQ(evens->Get(Ordinal::Finite(0)), 2);
	EXPECT_EQ(evens->Get(Ordinal::Finite(4)), 10);

	int sum = finite.Reduce<int>(0, std::function<int(int, int)>([](int acc, int value) {
		return acc + value;
	}));
	EXPECT_EQ(sum, 55);
}

TEST(LazySequence, MapWhereReduceValidateErrorsAndInfiniteReduce) {
	LazySequence<int> finite([](std::size_t index) { return static_cast<int>(index + 1); }, Ordinal::Finite(5));

	EXPECT_THROW(finite.Map<int>(std::function<int(int)>()), std::invalid_argument);
	EXPECT_THROW(finite.Where(std::function<bool(int)>()), std::invalid_argument);
	EXPECT_THROW(finite.Reduce<int>(0, std::function<int(int, int)>()), std::invalid_argument);
	EXPECT_THROW(finite.ReduceFirstN<int>(3, 0, std::function<int(int, int)>()), std::invalid_argument);

	auto none = finite.Where(std::function<bool(int)>([](int) { return false; }));
	EXPECT_EQ(none->GetLength(), Ordinal::Finite(0));
	EXPECT_THROW(none->Get(Ordinal::Finite(0)), std::out_of_range);

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
	                           Ordinal::Omega());

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
	auto sharedSequence = std::shared_ptr<Sequence<int> >(new MutableArraySequence<int>());
	sharedSequence->Append(1)->Append(2);

	SequenceReadStream<int> sequenceStream(sharedSequence);
	sequenceStream.Open();
	EXPECT_EQ(sequenceStream.Seek(2), static_cast<std::size_t>(2));
	EXPECT_THROW(sequenceStream.Read(), EndOfStream);
	EXPECT_THROW(sequenceStream.Seek(3), EndOfStream);
	sequenceStream.Close();
	EXPECT_THROW(sequenceStream.Read(), StreamException);

	EXPECT_THROW(SequenceReadStream<int>(std::shared_ptr<Sequence<int>>()), std::invalid_argument);

	auto sharedLazy = std::shared_ptr<LazySequence<int> >(
		new LazySequence<int>([](std::size_t index) { return static_cast<int>(index + 1); }, Ordinal::Finite(2)));
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
	auto destination = std::shared_ptr<Sequence<int> >(new MutableArraySequence<int>());
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
	const char *filename = "lab4_stream_test.tmp";

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

TEST(ForecastCorrection, HistoryBufferKeepsOnlyLatestEvents) {
	HistoryBuffer history(3);
	history.Add(Event{1, 1, EventType::CpuLoad, 10.0, "test"});
	history.Add(Event{2, 2, EventType::CpuLoad, 20.0, "test"});
	history.Add(Event{3, 3, EventType::CpuLoad, 30.0, "test"});
	history.Add(Event{4, 4, EventType::CpuLoad, 40.0, "test"});

	EXPECT_EQ(history.GetLength(), static_cast<std::size_t>(3));
	EXPECT_DOUBLE_EQ(history.Get(0).value, 20.0);
	EXPECT_DOUBLE_EQ(history.GetFromEnd(0).value, 40.0);
	EXPECT_TRUE(history.CanPredict(3));
	EXPECT_THROW(history.GetFromEnd(3), std::out_of_range);
}

TEST(ForecastCorrection, DifferenceModelPredictsFirstAndSecondOrder) {
	HistoryBuffer history(4);
	history.Add(Event{1, 1, EventType::CpuLoad, 10.0, "test"});
	history.Add(Event{2, 2, EventType::CpuLoad, 12.0, "test"});

	DifferenceForecastModel firstOrder(1);
	Prediction first = firstOrder.PredictNext(history, EventType::CpuLoad);
	ASSERT_TRUE(first.hasPrediction);
	EXPECT_DOUBLE_EQ(first.predictedValue, 14.0);

	DifferenceForecastModel secondOrder(2);
	EXPECT_FALSE(secondOrder.PredictNext(history, EventType::CpuLoad).hasPrediction);
	history.Add(Event{3, 3, EventType::CpuLoad, 15.0, "test"});
	Prediction second = secondOrder.PredictNext(history, EventType::CpuLoad);
	ASSERT_TRUE(second.hasPrediction);
	EXPECT_DOUBLE_EQ(second.predictedValue, 19.0);

	EXPECT_THROW(DifferenceForecastModel(3), std::invalid_argument);
}

TEST(ForecastCorrection, CorrectionAndDecisionClassifyDeviation) {
	CorrectionService correctionService(10.0, 25.0);
	DecisionMaker decisionMaker;
	Prediction prediction{true, EventType::CpuLoad, 55.0, 0.75};

	Event normal{1, 1, EventType::CpuLoad, 55.0, "server"};
	Correction normalCorrection = correctionService.Compare(prediction, normal);
	EXPECT_DOUBLE_EQ(normalCorrection.error, 0.0);
	EXPECT_EQ(decisionMaker.MakeReaction(normal, normalCorrection).type, ReactionType::Normal);

	Event warning{2, 2, EventType::CpuLoad, 70.0, "server"};
	Correction warningCorrection = correctionService.Compare(prediction, warning);
	EXPECT_DOUBLE_EQ(warningCorrection.error, 15.0);
	EXPECT_EQ(decisionMaker.MakeReaction(warning, warningCorrection).type, ReactionType::Warning);

	Event critical{3, 3, EventType::CpuLoad, 100.0, "server"};
	Correction criticalCorrection = correctionService.Compare(prediction, critical);
	EXPECT_DOUBLE_EQ(criticalCorrection.absoluteError, 45.0);
	EXPECT_EQ(decisionMaker.MakeReaction(critical, criticalCorrection).type, ReactionType::Critical);

	Correction absent = correctionService.Compare(Prediction{}, normal);
	EXPECT_EQ(decisionMaker.MakeReaction(normal, absent).type, ReactionType::NoPrediction);
	EXPECT_THROW(CorrectionService(20.0, 10.0), std::invalid_argument);
}

TEST(ForecastCorrection, ProcessorPredictsCorrectsAndSeparatesEventTypes) {
	ForecastCorrectionProcessor processor(1, 4, 5.0, 20.0);

	ProcessingResult first = processor.Process(Event{1, 1, EventType::CpuLoad, 10.0, "server"});
	ProcessingResult second = processor.Process(Event{2, 2, EventType::CpuLoad, 20.0, "server"});
	ProcessingResult memory = processor.Process(Event{3, 3, EventType::MemoryLoad, 90.0, "server"});
	ProcessingResult third = processor.Process(Event{4, 4, EventType::CpuLoad, 35.0, "server"});

	EXPECT_EQ(first.reaction.type, ReactionType::NoPrediction);
	EXPECT_EQ(second.reaction.type, ReactionType::NoPrediction);
	ASSERT_TRUE(second.nextPrediction.hasPrediction);
	EXPECT_DOUBLE_EQ(second.nextPrediction.predictedValue, 30.0);
	EXPECT_EQ(memory.reaction.type, ReactionType::NoPrediction);
	EXPECT_EQ(third.reaction.type, ReactionType::Warning);
	EXPECT_DOUBLE_EQ(third.correction.error, 5.0);
	EXPECT_DOUBLE_EQ(third.nextPrediction.predictedValue, 50.0);
	EXPECT_EQ(processor.GetHistory(EventType::CpuLoad).GetLength(), static_cast<std::size_t>(3));
	EXPECT_EQ(processor.GetHistory(EventType::MemoryLoad).GetLength(), static_cast<std::size_t>(1));

	EXPECT_THROW(ForecastCorrectionProcessor(2, 2, 5.0, 10.0), std::invalid_argument);
}

TEST(ForecastCorrection, LazyEventGeneratorProcessesAndWritesResults) {
	auto events = EventGenerator::WithSpike(EventType::CpuLoad, 5, 10.0, 10.0, 3, 100.0, "test");
	LazySequenceReadStream<Event> input(*events);
	MutableArraySequence<ProcessingResult> results;
	SequenceWriteStream<ProcessingResult> output(results);
	ForecastCorrectionProcessor processor(1, 4, 10.0, 25.0);
	ProcessingStatistics statistics;

	input.Open();
	output.Open();
	while (!input.IsEndOfStream()) {
		ProcessingResult result = processor.Process(input.Read());
		output.Write(result);
		statistics.Add(result);
	}
	input.Close();
	output.Close();

	EXPECT_EQ(results.GetLength(), static_cast<std::size_t>(5));
	EXPECT_EQ(statistics.GetTotal(), static_cast<std::size_t>(5));
	EXPECT_EQ(statistics.GetCriticalCount(), static_cast<std::size_t>(2));
	EXPECT_EQ(events->GetMaterializedCount(), static_cast<std::size_t>(5));
	EXPECT_EQ(results.Get(3).reaction.type, ReactionType::Critical);
	EXPECT_NE(SerializeProcessingResult(results.Get(3)).find("CRITICAL"), std::string::npos);
}

TEST(ForecastCorrection, EventCsvSerializationRoundTrips) {
	Event event{17, 5000, EventType::Temperature, 36.5, "sensor-1"};

	Event restored = DeserializeEvent(SerializeEvent(event));

	EXPECT_EQ(restored.id, event.id);
	EXPECT_EQ(restored.timestamp, event.timestamp);
	EXPECT_EQ(restored.type, event.type);
	EXPECT_DOUBLE_EQ(restored.value, event.value);
	EXPECT_EQ(restored.source, event.source);
	EXPECT_THROW(DeserializeEvent("broken,line"), std::invalid_argument);
}

TEST(ForecastCorrection, ProcessesLargeLinearLazyEventStream) {
	const std::size_t count = 100000;
	auto events = EventGenerator::Linear(EventType::CpuLoad, count, 10.0, 0.5, "large-linear");
	LazySequenceReadStream<Event> input(*events);
	ForecastCorrectionProcessor processor(1, 8, 1.0, 5.0);
	ProcessingStatistics statistics;

	input.Open();
	while (!input.IsEndOfStream()) {
		statistics.Add(processor.Process(input.Read()));
	}
	input.Close();

	EXPECT_EQ(statistics.GetTotal(), count);
	EXPECT_EQ(statistics.GetNoPredictionCount(), static_cast<std::size_t>(2));
	EXPECT_EQ(statistics.GetNormalCount(), count - 2);
	EXPECT_EQ(statistics.GetWarningCount(), static_cast<std::size_t>(0));
	EXPECT_EQ(statistics.GetCriticalCount(), static_cast<std::size_t>(0));
	EXPECT_DOUBLE_EQ(statistics.GetMaximumAbsoluteError(), 0.0);
	EXPECT_EQ(events->GetMaterializedCount(), count);
}
