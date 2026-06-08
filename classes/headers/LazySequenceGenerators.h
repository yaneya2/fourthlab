#ifndef FOURTHLAB_LAZY_SEQUENCE_GENERATORS_H
#define FOURTHLAB_LAZY_SEQUENCE_GENERATORS_H

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

#include "Generator.h"
#include "Ordinal.h"
#include "MutableArraySequence.h"
#include "Sequence.h"

template<class T>
class LazySequence;

inline Ordinal CalcRangeLength(
	Ordinal endIndex,
	Ordinal startIndex,
	bool includeEndIndex = false
) {
	if (endIndex < startIndex) {
		throw std::out_of_range("Invalid ordinal subtraction");
	}

	Ordinal result = Ordinal::Finite(0);

	if (startIndex.IsFinite()) {
		if (endIndex.IsFinite()) {
			result = Ordinal::Finite(endIndex.FinitePart() - startIndex.FinitePart());
		} else {
			result = endIndex;
		}
	} else if (endIndex.OmegaCoefficient() == startIndex.OmegaCoefficient()) {
		result = Ordinal::Finite(endIndex.FinitePart() - startIndex.FinitePart());
	} else {
		result = Ordinal::FromParts(
			endIndex.OmegaCoefficient() - startIndex.OmegaCoefficient(),
			endIndex.FinitePart()
		);
	}

	return includeEndIndex
		       ? Ordinal::FromParts(result.OmegaCoefficient(), result.FinitePart() + 1)
		       : result;
}

template<class T>
class EmptyGenerator : public Generator<T> {
public:
	[[nodiscard]] Ordinal GetLength() const override {
		return Ordinal::Finite(0);
	}

	[[nodiscard]] bool HasNext(Ordinal) const override {
		return false;
	}

	T GetNext(Ordinal) override {
		throw std::out_of_range("LazySequence is empty");
	}

	std::unique_ptr<Generator<T> > Clone() const override {
		return std::unique_ptr<Generator<T> >(new EmptyGenerator<T>(*this));
	}
};

template<class T>
class SequenceGenerator : public Generator<T> {
private:
	MutableArraySequence<T> data;

public:
	SequenceGenerator(const T *items, std::size_t count) : data() {
		if (items == nullptr && count > 0) {
			throw std::invalid_argument("LazySequence source array is null");
		}

		for (std::size_t i = 0; i < count; ++i) {
			data.Append(items[i]);
		}
	}

	explicit SequenceGenerator(const Sequence<T> &source) : data() {
		auto *enumerator = source.GetEnumerator();
		while (enumerator->MoveNext()) {
			data.Append(enumerator->Current());
		}
		delete enumerator;
	}

	[[nodiscard]] Ordinal GetLength() const override {
		return Ordinal::Finite(data.GetLength());
	}

	[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
		return nextIndex < Ordinal::Finite(data.GetLength());
	}

	T GetNext(Ordinal nextIndex) override {
		if (!HasNext(nextIndex)) {
			throw std::out_of_range("Index out of range");
		}
		return data.Get(nextIndex.FinitePart());
	}

	std::unique_ptr<Generator<T> > Clone() const override {
		return std::unique_ptr<Generator<T> >(new SequenceGenerator<T>(*this));
	}
};

template<class T>
class RecurrenceGenerator : public Generator<T> {
private:
	MutableArraySequence<T> generatedItems;
	std::function<T(Sequence<T> *)> rule;
	Ordinal length;

public:
	RecurrenceGenerator(std::function<T(Sequence<T> *)> rule, Sequence<T> *firstItems, Ordinal length)
		: generatedItems(), rule(std::move(rule)), length(length) {
		if (!this->rule) {
			throw std::invalid_argument("Recurrence rule is empty");
		}
		if (firstItems == nullptr) {
			throw std::invalid_argument("Recurrence seed sequence is null");
		}
		if (length.IsFinite() && length.FinitePart() < firstItems->GetLength()) {
			throw std::invalid_argument("Finite length is less than seed count");
		}

		auto *enumerator = firstItems->GetEnumerator();
		while (enumerator->MoveNext()) {
			generatedItems.Append(enumerator->Current());
		}
		delete enumerator;
	}

	[[nodiscard]] Ordinal GetLength() const override {
		return length;
	}

	[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
		return nextIndex < length;
	}

	T GetNext(Ordinal nextIndex) override {
		if (!HasNext(nextIndex)) {
			throw std::out_of_range("Index out of range");
		}
		if (!nextIndex.IsFinite()) {
			throw std::logic_error("Recurrence generator supports only finite indexes");
		}

		std::size_t finiteIndex = nextIndex.FinitePart();
		while (generatedItems.GetLength() <= finiteIndex) {
			generatedItems.Append(rule(&generatedItems));
		}
		return generatedItems.Get(finiteIndex);
	}

	std::unique_ptr<Generator<T> > Clone() const override {
		return std::unique_ptr<Generator<T> >(new RecurrenceGenerator<T>(*this));
	}
};

template<class T>
class FunctionGenerator : public Generator<T> {
private:
	std::function<T(std::size_t)> rule;
	Ordinal length;

public:
	FunctionGenerator(std::function<T(std::size_t)> rule, Ordinal length)
		: rule(std::move(rule)), length(length) {
		if (!this->rule) {
			throw std::invalid_argument("Index function rule is empty");
		}
	}

	[[nodiscard]] Ordinal GetLength() const override {
		return length;
	}

	[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
		return nextIndex < length;
	}

	T GetNext(Ordinal nextIndex) override {
		if (!HasNext(nextIndex)) {
			throw std::out_of_range("Index out of range");
		}
		if (!nextIndex.IsFinite()) {
			throw std::logic_error("Index function generator supports only finite indexes");
		}
		return rule(nextIndex.FinitePart());
	}

	std::unique_ptr<Generator<T> > Clone() const override {
		return std::unique_ptr<Generator<T> >(new FunctionGenerator<T>(*this));
	}
};

template<class T>
class PrependGenerator : public Generator<T> {
private:
	std::shared_ptr<LazySequence<T> > source;
	std::shared_ptr<LazySequence<T> > prepended;

public:
	PrependGenerator(std::shared_ptr<LazySequence<T> > source,
	                 std::shared_ptr<LazySequence<T> > prepended)
		: source(std::move(source)), prepended(std::move(prepended)) {
	}

	[[nodiscard]] Ordinal GetLength() const override {
		return prepended->GetLength() + source->GetLength();
	}

	[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
		return nextIndex < GetLength();
	}

	T GetNext(Ordinal nextIndex) override {
		if (!HasNext(nextIndex)) {
			throw std::out_of_range("Index out of range");
		}

		Ordinal prependedLength = prepended->GetLength();
		if (nextIndex < prependedLength) {
			return prepended->Get(nextIndex);
		}
		return source->Get(CalcRangeLength(nextIndex, prependedLength));
	}

	std::unique_ptr<Generator<T> > Clone() const override {
		return std::unique_ptr<Generator<T> >(new PrependGenerator<T>(*this));
	}
};

template<class T>
class AppendGenerator : public Generator<T> {
private:
	std::shared_ptr<LazySequence<T> > source;
	std::shared_ptr<LazySequence<T> > appended;

public:
	AppendGenerator(std::shared_ptr<LazySequence<T> > source,
	                std::shared_ptr<LazySequence<T> > appended)
		: source(std::move(source)), appended(std::move(appended)) {
	}

	[[nodiscard]] Ordinal GetLength() const override {
		return source->GetLength() + appended->GetLength();
	}

	[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
		return nextIndex < GetLength();
	}

	T GetNext(Ordinal nextIndex) override {
		if (!HasNext(nextIndex)) {
			throw std::out_of_range("Index out of range");
		}

		Ordinal length = source->GetLength();
		if (nextIndex < length) {
			return source->Get(nextIndex);
		}
		return appended->Get(CalcRangeLength(nextIndex, length));
	}

	std::unique_ptr<Generator<T> > Clone() const override {
		return std::unique_ptr<Generator<T> >(new AppendGenerator<T>(*this));
	}
};

template<class T>
class InsertGenerator : public Generator<T> {
private:
	std::shared_ptr<LazySequence<T> > source;
	std::shared_ptr<LazySequence<T> > inserted;
	Ordinal index;

public:
	InsertGenerator(std::shared_ptr<LazySequence<T> > sourcePtr,
	                std::shared_ptr<LazySequence<T> > insertedPtr,
	                Ordinal index)
		: source(std::move(sourcePtr)),
		  inserted(std::move(insertedPtr)),
		  index(index) {
		if (!source) {
			throw std::invalid_argument("Insert source is null");
		}
		if (!inserted) {
			throw std::invalid_argument("Inserted sequence is null");
		}

		Ordinal length = this->source->GetLength();
		if (!(index < length)) {
			throw std::out_of_range("Insert index out of range");
		}
	}

	[[nodiscard]] Ordinal GetLength() const override {
		Ordinal prefixLength = index;
		Ordinal insertedLength = inserted->GetLength();
		Ordinal suffixLength = CalcRangeLength(source->GetLength(), prefixLength);
		return prefixLength + insertedLength + suffixLength;
	}

	[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
		return nextIndex < GetLength();
	}

	T GetNext(Ordinal nextIndex) override {
		if (!HasNext(nextIndex)) {
			throw std::out_of_range("Index out of range");
		}
		if (nextIndex < index) {
			return source->Get(nextIndex);
		}

		Ordinal localInsertedIndex = CalcRangeLength(nextIndex, index);
		Ordinal insertedLength = inserted->GetLength();
		if (localInsertedIndex < insertedLength) {
			return inserted->Get(localInsertedIndex);
		}

		Ordinal sourceSuffixIndex = CalcRangeLength(localInsertedIndex, insertedLength);
		return source->Get(index + sourceSuffixIndex);
	}

	std::unique_ptr<Generator<T> > Clone() const override {
		return std::unique_ptr<Generator<T> >(new InsertGenerator<T>(*this));
	}
};

template<class T>
class ConcatGenerator : public Generator<T> {
private:
	std::shared_ptr<LazySequence<T> > first;
	std::shared_ptr<LazySequence<T> > second;

public:
	ConcatGenerator(std::shared_ptr<LazySequence<T> > first, std::shared_ptr<LazySequence<T> > second)
		: first(std::move(first)), second(std::move(second)) {
	}

	[[nodiscard]] Ordinal GetLength() const override {
		return first->GetLength() + second->GetLength();
	}

	[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
		return nextIndex < GetLength();
	}

	T GetNext(Ordinal nextIndex) override {
		if (!HasNext(nextIndex)) {
			throw std::out_of_range("Index out of range");
		}
		Ordinal firstLength = first->GetLength();
		if (nextIndex < firstLength) {
			return first->Get(nextIndex);
		}
		return second->Get(CalcRangeLength(nextIndex, firstLength));
	}

	std::unique_ptr<Generator<T> > Clone() const override {
		return std::unique_ptr<Generator<T> >(new ConcatGenerator<T>(*this));
	}
};

template<class T>
class SubsequenceGenerator : public Generator<T> {
private:
	std::shared_ptr<LazySequence<T> > source;
	Ordinal start;
	Ordinal length;

public:
	SubsequenceGenerator(std::shared_ptr<LazySequence<T> > source, Ordinal start, Ordinal length)
		: source(std::move(source)), start(start), length(length) {
	}

	[[nodiscard]] Ordinal GetLength() const override {
		return length;
	}

	[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
		return nextIndex < length;
	}

	T GetNext(Ordinal nextIndex) override {
		if (!HasNext(nextIndex)) {
			throw std::out_of_range("Index out of range");
		}
		Ordinal sourceIndex = start + nextIndex;
		return source->Get(sourceIndex);
	}

	std::unique_ptr<Generator<T> > Clone() const override {
		return std::unique_ptr<Generator<T> >(new SubsequenceGenerator<T>(*this));
	}
};

template<class T, class SourceT>
class MapGeneratorFrom : public Generator<T> {
private:
	std::shared_ptr<LazySequence<SourceT> > source;
	std::function<T(SourceT)> mapper;

public:
	MapGeneratorFrom(std::shared_ptr<LazySequence<SourceT> > source, std::function<T(SourceT)> mapper)
		: source(std::move(source)), mapper(std::move(mapper)) {
	}

	[[nodiscard]] Ordinal GetLength() const override {
		return source->GetLength();
	}

	[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
		return nextIndex < source->GetLength();
	}

	T GetNext(Ordinal nextIndex) override {
		return mapper(source->Get(nextIndex));
	}

	std::unique_ptr<Generator<T> > Clone() const override {
		return std::unique_ptr<Generator<T> >(new MapGeneratorFrom<T, SourceT>(*this));
	}
};

template<class T>
class WhereGenerator : public Generator<T> {
private:
	std::shared_ptr<LazySequence<T> > source;
	std::function<bool(T)> predicate;
	std::size_t sourceIndex;

public:
	WhereGenerator(std::shared_ptr<LazySequence<T> > source, std::function<bool(T)> predicate)
		: source(std::move(source)), predicate(std::move(predicate)), sourceIndex(0) {
	}

	[[nodiscard]] Ordinal GetLength() const override {
		Ordinal sourceLength = source->GetLength();
		if (sourceLength.IsInfinite()) {
			return sourceLength;
		}

		std::size_t count = 0;
		for (std::size_t i = 0; i < sourceLength.FinitePart(); ++i) {
			if (predicate(source->Get(Ordinal::Finite(i)))) {
				++count;
			}
		}
		return Ordinal::Finite(count);
	}

	[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
		return nextIndex < GetLength();
	}

	T GetNext(Ordinal nextIndex) override {
		if (!nextIndex.IsFinite()) {
			throw std::logic_error("Where generator supports only finite indexes");
		}
		Ordinal sourceLength = source->GetLength();
		while (sourceLength.IsInfinite() || sourceIndex < sourceLength.FinitePart()) {
			T current = source->Get(Ordinal::Finite(sourceIndex));
			++sourceIndex;
			if (predicate(current)) {
				return current;
			}
		}
		throw std::out_of_range("No next element satisfies predicate");
	}

	std::unique_ptr<Generator<T> > Clone() const override {
		return std::unique_ptr<Generator<T> >(new WhereGenerator<T>(*this));
	}
};

#endif
