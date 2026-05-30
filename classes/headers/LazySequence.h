#ifndef FOURTHLAB_LAZY_SEQUENCE_H
#define FOURTHLAB_LAZY_SEQUENCE_H

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

#include "Ordinal.h"
#include "IEnumerator.h"
#include "MutableArraySequence.h"
#include "Sequence.h"

template<class T>
class LazySequence;

template<class T>
class LazySequence {
private:
	class Generator {
	public:
		virtual ~Generator() = default;

		[[nodiscard]] virtual Ordinal GetLength() const = 0;

		[[nodiscard]] virtual bool HasNext(Ordinal nextIndex) const = 0;

		virtual T GetNext(Ordinal nextIndex) = 0;

		virtual std::unique_ptr<Generator> Clone() const = 0;
	};

	template<class SourceT>
	class MapGeneratorFrom;

	class WhereGenerator;

	struct State {
		explicit State(std::unique_ptr<Generator> generator)
			: generator_(std::move(generator)), cache_() {
		}

		State(const State &other)
			: generator_(other.generator_->Clone()), cache_(other.cache_) {
		}

		State &operator=(const State &other) {
			if (this != &other) {
				generator_ = other.generator_->Clone();
				cache_ = other.cache_;
			}
			return *this;
		}

		std::unique_ptr<Generator> generator_;
		MutableArraySequence<T> cache_;
	};

	std::unique_ptr<State> state_;

	class EmptyGenerator : public Generator {
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

		std::unique_ptr<Generator> Clone() const override {
			return std::unique_ptr<Generator>(new EmptyGenerator(*this));
		}
	};

	class SequenceGenerator : public Generator {
	private:
		MutableArraySequence<T> data_;

	public:
		SequenceGenerator(const T *items, std::size_t count) : data_() {
			if (items == nullptr && count > 0) {
				throw std::invalid_argument("LazySequence source array is null");
			}

			for (std::size_t i = 0; i < count; ++i) {
				data_.Append(items[i]);
			}
		}

		explicit SequenceGenerator(const Sequence<T> &source) : data_() {
			auto *enumerator = source.GetEnumerator();
			while (enumerator->MoveNext()) {
				data_.Append(enumerator->Current());
			}
			delete enumerator;
		}

		[[nodiscard]] Ordinal GetLength() const override {
			return Ordinal::Finite(data_.GetLength());
		}

		[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
			return Ordinal::Finite(data_.GetLength()).ContainsIndex(nextIndex);
		}

		T GetNext(Ordinal nextIndex) override {
			if (!HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}
			return data_.Get(nextIndex.FinitePart());
		}

		std::unique_ptr<Generator> Clone() const override {
			return std::unique_ptr<Generator>(new SequenceGenerator(*this));
		}
	};

	class RecurrenceGenerator : public Generator {
	private:
		MutableArraySequence<T> generatedItems_;
		std::function<T(Sequence<T> *)> rule_;
		Ordinal length_;

	public:
		RecurrenceGenerator(std::function<T(Sequence<T> *)> rule, Sequence<T> *firstItems, Ordinal length)
			: generatedItems_(), rule_(std::move(rule)), length_(length) {
			if (!rule_) {
				throw std::invalid_argument("Recurrence rule is empty");
			}
			if (firstItems == nullptr) {
				throw std::invalid_argument("Recurrence seed sequence is null");
			}
			if (length_.IsFinite() && length_.FinitePart() < firstItems->GetLength()) {
				throw std::invalid_argument("Finite length is less than seed count");
			}

			auto *enumerator = firstItems->GetEnumerator();
			while (enumerator->MoveNext()) {
				generatedItems_.Append(enumerator->Current());
			}
			delete enumerator;
		}

		[[nodiscard]] Ordinal GetLength() const override {
			return length_;
		}

		[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
			return length_.ContainsIndex(nextIndex);
		}

		T GetNext(Ordinal nextIndex) override {
			if (!HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}
			if (!nextIndex.IsFinite()) {
				throw std::logic_error("Recurrence generator supports only finite indexes");
			}

			std::size_t finiteIndex = nextIndex.FinitePart();
			while (generatedItems_.GetLength() <= finiteIndex) {
				generatedItems_.Append(rule_(&generatedItems_));
			}
			return generatedItems_.Get(finiteIndex);
		}

		std::unique_ptr<Generator> Clone() const override {
			return std::unique_ptr<Generator>(new RecurrenceGenerator(*this));
		}
	};

	class FunctionGenerator : public Generator {
	private:
		std::function<T(std::size_t)> rule_;
		Ordinal length_;

	public:
		FunctionGenerator(std::function<T(std::size_t)> rule, Ordinal length)
			: rule_(std::move(rule)), length_(length) {
			if (!rule_) {
				throw std::invalid_argument("Index function rule is empty");
			}
		}

		[[nodiscard]] Ordinal GetLength() const override {
			return length_;
		}

		[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
			return length_.ContainsIndex(nextIndex);
		}

		T GetNext(Ordinal nextIndex) override {
			if (!HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}
			if (!nextIndex.IsFinite()) {
				throw std::logic_error("Index function generator supports only finite indexes");
			}
			return rule_(nextIndex.FinitePart());
		}

		std::unique_ptr<Generator> Clone() const override {
			return std::unique_ptr<Generator>(new FunctionGenerator(*this));
		}
	};

	static bool CanAddOne(std::size_t value) {
		return value < std::numeric_limits<std::size_t>::max();
	}

	static Ordinal AddOneToFinitePart(Ordinal value) {
		if (!CanAddOne(value.FinitePart())) {
			throw std::overflow_error("Ordinal finite part addition overflow");
		}
		return Ordinal::FromParts(value.OmegaCoefficient(), value.FinitePart() + 1);
	}

	static Ordinal solveNewLength(Ordinal index, Ordinal prefixLength, bool includeRightBound = false) {
		if (index < prefixLength) {
			throw std::out_of_range("Invalid ordinal subtraction");
		}
		Ordinal result = Ordinal::Finite(0);
		if (prefixLength.IsFinite()) {
			if (index.IsFinite()) {
				result = Ordinal::Finite(index.FinitePart() - prefixLength.FinitePart());
			} else {
				result = index;
			}
		} else if (index.OmegaCoefficient() == prefixLength.OmegaCoefficient()) {
			result = Ordinal::Finite(index.FinitePart() - prefixLength.FinitePart());
		} else {
			result = Ordinal::FromParts(index.OmegaCoefficient() - prefixLength.OmegaCoefficient(), index.FinitePart());
		}
		return includeRightBound ? AddOneToFinitePart(result) : result;
	}

	class PrependGenerator : public Generator {
	private:
		std::shared_ptr<LazySequence<T> > source_;
		std::shared_ptr<LazySequence<T> > prepended_;

	public:
		PrependGenerator(std::shared_ptr<LazySequence<T> > source,
		                 std::shared_ptr<LazySequence<T> > prepended)
			: source_(std::move(source)), prepended_(std::move(prepended)) {
		}

		[[nodiscard]] Ordinal GetLength() const override {
			return prepended_->GetLength() + source_->GetLength();
		}

		[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
			return GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(Ordinal nextIndex) override {
			if (!HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}

			Ordinal prependedLength = prepended_->GetLength();
			if (prependedLength.ContainsIndex(nextIndex)) {
				return prepended_->Get(nextIndex);
			}
			return source_->Get(solveNewLength(nextIndex, prependedLength));
		}

		std::unique_ptr<Generator> Clone() const override {
			return std::unique_ptr<Generator>(new PrependGenerator(*this));
		}
	};

	class AppendGenerator : public Generator {
	private:
		std::shared_ptr<LazySequence<T> > source_;
		std::shared_ptr<LazySequence<T> > appended_;

	public:
		AppendGenerator(std::shared_ptr<LazySequence<T> > source,
		                std::shared_ptr<LazySequence<T> > appended)
			: source_(std::move(source)), appended_(std::move(appended)) {
		}

		[[nodiscard]] Ordinal GetLength() const override {
			return source_->GetLength() + appended_->GetLength();
		}

		[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
			return GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(Ordinal nextIndex) override {
			if (!HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}

			Ordinal length = source_->GetLength();
			if (length.ContainsIndex(nextIndex)) {
				return source_->Get(nextIndex);
			}
			return appended_->Get(solveNewLength(nextIndex, length));
		}

		std::unique_ptr<Generator> Clone() const override {
			return std::unique_ptr<Generator>(new AppendGenerator(*this));
		}
	};

	class InsertGenerator : public Generator {
	private:
		std::shared_ptr<LazySequence<T> > source_;
		std::shared_ptr<LazySequence<T> > inserted_;
		Ordinal index_;

	public:
		InsertGenerator(std::shared_ptr<LazySequence<T> > source,
		                std::shared_ptr<LazySequence<T> > inserted,
		                Ordinal index)
			: source_(std::move(source)), inserted_(std::move(inserted)), index_(index) {
			Ordinal length = source_->GetLength();
			if (!length.ContainsIndex(index_)) {
				throw std::out_of_range("Insert index out of range");
			}
		}

		[[nodiscard]] Ordinal GetLength() const override {
			Ordinal prefixLength = index_;
			Ordinal insertedLength = inserted_->GetLength();
			Ordinal suffixLength = solveNewLength(source_->GetLength(), prefixLength);
			return prefixLength + insertedLength + suffixLength;
		}

		[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
			return GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(Ordinal nextIndex) override {
			if (!HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}
			if (nextIndex < index_) {
				return source_->Get(nextIndex);
			}

			Ordinal localInsertedIndex = solveNewLength(nextIndex, index_);
			Ordinal insertedLength = inserted_->GetLength();
			if (insertedLength.ContainsIndex(localInsertedIndex)) {
				return inserted_->Get(localInsertedIndex);
			}

			Ordinal sourceSuffixIndex = solveNewLength(localInsertedIndex, insertedLength);
			return source_->Get(index_ + sourceSuffixIndex);
		}

		std::unique_ptr<Generator> Clone() const override {
			return std::unique_ptr<Generator>(new InsertGenerator(*this));
		}
	};

	class ConcatGenerator : public Generator {
	private:
		std::shared_ptr<LazySequence<T> > first_;
		std::shared_ptr<LazySequence<T> > second_;

	public:
		ConcatGenerator(std::shared_ptr<LazySequence<T> > first, std::shared_ptr<LazySequence<T> > second)
			: first_(std::move(first)), second_(std::move(second)) {
		}

		[[nodiscard]] Ordinal GetLength() const override {
			return first_->GetLength() + second_->GetLength();
		}

		[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
			return GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(Ordinal nextIndex) override {
			if (!HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}
			Ordinal firstLength = first_->GetLength();
			if (firstLength.ContainsIndex(nextIndex)) {
				return first_->Get(nextIndex);
			}
			return second_->Get(solveNewLength(nextIndex, firstLength));
		}

		std::unique_ptr<Generator> Clone() const override {
			return std::unique_ptr<Generator>(new ConcatGenerator(*this));
		}
	};

	class SubsequenceGenerator : public Generator {
	private:
		std::shared_ptr<LazySequence<T> > source_;
		Ordinal start_;
		Ordinal length_;

	public:
		SubsequenceGenerator(std::shared_ptr<LazySequence<T> > source, Ordinal start, Ordinal length)
			: source_(std::move(source)), start_(start), length_(length) {
		}

		[[nodiscard]] Ordinal GetLength() const override {
			return length_;
		}

		[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
			return length_.ContainsIndex(nextIndex);
		}

		T GetNext(Ordinal nextIndex) override {
			if (!HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}
			Ordinal sourceIndex = start_ + nextIndex;
			return source_->Get(sourceIndex);
		}

		std::unique_ptr<Generator> Clone() const override {
			return std::unique_ptr<Generator>(new SubsequenceGenerator(*this));
		}
	};

	template<class SourceT>
	class MapGeneratorFrom : public Generator {
	private:
		std::shared_ptr<LazySequence<SourceT> > source_;
		std::function<T(SourceT)> mapper_;

	public:
		MapGeneratorFrom(std::shared_ptr<LazySequence<SourceT> > source, std::function<T(SourceT)> mapper)
			: source_(std::move(source)), mapper_(std::move(mapper)) {
		}

		[[nodiscard]] Ordinal GetLength() const override {
			return source_->GetLength();
		}

		[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
			return source_->GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(Ordinal nextIndex) override {
			return mapper_(source_->Get(nextIndex));
		}

		std::unique_ptr<Generator> Clone() const override {
			return std::unique_ptr<Generator>(new MapGeneratorFrom(*this));
		}
	};

	class WhereGenerator : public Generator {
	private:
		std::shared_ptr<LazySequence<T> > source_;
		std::function<bool(T)> predicate_;
		std::size_t sourceIndex_;

	public:
		WhereGenerator(std::shared_ptr<LazySequence<T> > source, std::function<bool(T)> predicate)
			: source_(std::move(source)), predicate_(std::move(predicate)), sourceIndex_(0) {
		}

		[[nodiscard]] Ordinal GetLength() const override {
			Ordinal sourceLength = source_->GetLength();
			if (sourceLength.IsInfinite()) {
				return sourceLength;
			}

			std::size_t count = 0;
			for (std::size_t i = 0; i < sourceLength.FinitePart(); ++i) {
				if (predicate_(source_->Get(Ordinal::Finite(i)))) {
					++count;
				}
			}
			return Ordinal::Finite(count);
		}

		[[nodiscard]] bool HasNext(Ordinal nextIndex) const override {
			return GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(Ordinal nextIndex) override {
			if (!nextIndex.IsFinite()) {
				throw std::logic_error("Where generator supports only finite indexes");
			}
			Ordinal sourceLength = source_->GetLength();
			while (sourceLength.IsInfinite() || sourceIndex_ < sourceLength.FinitePart()) {
				T current = source_->Get(Ordinal::Finite(sourceIndex_));
				++sourceIndex_;
				if (predicate_(current)) {
					return current;
				}
			}
			throw std::out_of_range("No next element satisfies predicate");
		}

		std::unique_ptr<Generator> Clone() const override {
			return std::unique_ptr<Generator>(new WhereGenerator(*this));
		}
	};

	void EnsureMaterialized(std::size_t index) const {
		if (!state_->generator_->HasNext(Ordinal::Finite(index))) {
			throw std::out_of_range("Index out of range");
		}

		while (state_->cache_.GetLength() <= index) {
			std::size_t nextIndex = state_->cache_.GetLength();
			Ordinal ordinalIndex = Ordinal::Finite(nextIndex);
			if (!state_->generator_->HasNext(ordinalIndex)) {
				throw std::out_of_range("Index out of range");
			}
			T value = state_->generator_->GetNext(ordinalIndex);
			state_->cache_.Append(value);
		}
	}

	std::shared_ptr<LazySequence<T> > SharedCopy() const {
		return std::make_shared<LazySequence<T> >(*this);
	}

	static std::shared_ptr<LazySequence<T> > createSeqFromElem(const T &item) {
		T items[] = {item};
		return std::make_shared<LazySequence<T> >(items, 1);
	}

	explicit LazySequence(std::unique_ptr<Generator> generator)
		: state_(std::unique_ptr<State>(new State(std::move(generator)))) {
	}

	template<class U>
	friend class LazySequence;

public:
	LazySequence() : state_(std::unique_ptr<State>(new State(std::unique_ptr<Generator>(new EmptyGenerator())))) {
	}

	LazySequence(const T *items, std::size_t count)
		: state_(std::unique_ptr<State>(
			new State(std::unique_ptr<Generator>(new SequenceGenerator(items, count))))) {
	}

	explicit LazySequence(const Sequence<T> &sequence)
		: state_(std::unique_ptr<State>(
			new State(std::unique_ptr<Generator>(new SequenceGenerator(sequence))))) {
	}

	explicit LazySequence(Sequence<T> *sequence) {
		if (sequence == nullptr) {
			throw std::invalid_argument("LazySequence source sequence is null");
		}
		this->state_ = std::unique_ptr<State>(
			new State(std::unique_ptr<Generator>(new SequenceGenerator(*sequence))));
	}

	LazySequence(std::function<T(Sequence<T> *)> recurrenceRule, Sequence<T> *firstItems,
	             Ordinal length = Ordinal::Omega())
		: state_(std::unique_ptr<State>(new State(std::unique_ptr<Generator>(
			new RecurrenceGenerator(std::move(recurrenceRule), firstItems, length))))) {
	}

	LazySequence(T (*recurrenceRule)(Sequence<T> *), Sequence<T> *firstItems,
	             Ordinal length = Ordinal::Omega())
		: LazySequence(std::function<T(Sequence<T> *)>(recurrenceRule), firstItems, length) {
	}

	LazySequence(std::function<T(std::size_t)> indexRule, Ordinal length)
		: state_(std::unique_ptr<State>(new State(std::unique_ptr<Generator>(
			new FunctionGenerator(std::move(indexRule), length))))) {
	}

	LazySequence(const LazySequence &other)
		: state_(std::unique_ptr<State>(new State(*other.state_))) {
	}

	LazySequence(LazySequence &&other) noexcept = default;

	LazySequence &operator=(const LazySequence &other) {
		if (this != &other) {
			state_ = std::unique_ptr<State>(new State(*other.state_));
		}
		return *this;
	}

	LazySequence &operator=(LazySequence &&other) noexcept = default;

	static std::unique_ptr<LazySequence<T> > FromIndexFunction(std::function<T(std::size_t)> indexRule,
	                                                           Ordinal length) {
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(std::move(indexRule), length));
	}

	static std::unique_ptr<LazySequence<T> > Infinite(std::function<T(std::size_t)> indexRule) {
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(std::move(indexRule), Ordinal::Omega()));
	}

	T GetFirst() const {
		return Get(Ordinal::Finite(0));
	}

	T GetLast() const {
		Ordinal length = GetLength();
		if (length.IsInfinite() && length.FinitePart() != 0) {
			return Get(Ordinal(length.OmegaCoefficient(), length.FinitePart() - 1));
		}
		if (length.IsInfinite()) {
			throw std::logic_error("Cannot get last element of omega-length LazySequence");
		}
		if (length.FinitePart() == 0) {
			throw std::out_of_range("LazySequence is empty");
		}
		return Get(Ordinal::Finite(length.FinitePart() - 1));
	}

	T Get(Ordinal index) const {
		if (!state_->generator_->HasNext(index)) {
			throw std::out_of_range("Index out of range");
		}
		if (!index.IsFinite()) {
			return state_->generator_->GetNext(index);
		}

		std::size_t finiteIndex = index.FinitePart();
		EnsureMaterialized(finiteIndex);
		return state_->cache_.Get(finiteIndex);
	}

	std::unique_ptr<LazySequence<T> > GetSubsequence(Ordinal startIndex, Ordinal endIndex) const {
		Ordinal sourceLength = GetLength();
		if (endIndex < startIndex) {
			throw std::out_of_range("Invalid subsequence bounds");
		}
		if (sourceLength.IsInfinite() && endIndex == sourceLength) {
			if (!sourceLength.ContainsIndex(startIndex)) {
				throw std::out_of_range("Subsequence index out of range");
			}
			Ordinal count = solveNewLength(sourceLength, startIndex);
			return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(
				std::unique_ptr<Generator>(new SubsequenceGenerator(SharedCopy(), startIndex, count))));
		}
		if (!sourceLength.ContainsIndex(startIndex) || !sourceLength.ContainsIndex(endIndex)) {
			throw std::out_of_range("Subsequence index out of range");
		}

		Ordinal count = solveNewLength(endIndex, startIndex, true);
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(
			std::unique_ptr<Generator>(new SubsequenceGenerator(SharedCopy(), startIndex, count))));
	}

	std::unique_ptr<LazySequence<T> > GetSubsequence(std::size_t startIndex, std::size_t endIndex) const {
		return GetSubsequence(Ordinal::Finite(startIndex), Ordinal::Finite(endIndex));
	}

	std::unique_ptr<LazySequence<T> > GetSubsequence(int startIndex, int endIndex) const {
		if (startIndex < 0 || endIndex < 0) {
			throw std::out_of_range("Invalid subsequence bounds");
		}
		return GetSubsequence(static_cast<std::size_t>(startIndex), static_cast<std::size_t>(endIndex));
	}

	[[nodiscard]] Ordinal GetLength() const {
		return state_->generator_->GetLength();
	}

	[[nodiscard]] std::size_t GetMaterializedCount() const {
		return state_->cache_.GetLength();
	}

	std::unique_ptr<LazySequence<T> > Append(const T &item) const {
		return Append(*createSeqFromElem(item));
	}

	std::unique_ptr<LazySequence<T> > Prepend(const T &item) const {
		return Prepend(*createSeqFromElem(item));
	}

	std::unique_ptr<LazySequence<T> > Append(const Sequence<T> &items) const {
		return Append(LazySequence<T>(items));
	}

	std::unique_ptr<LazySequence<T> > Append(const LazySequence<T> &items) const {
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(std::unique_ptr<Generator>(
			new AppendGenerator(SharedCopy(), std::make_shared<LazySequence<T> >(items)))));
	}

	std::unique_ptr<LazySequence<T> > Prepend(const Sequence<T> &items) const {
		return Prepend(LazySequence<T>(items));
	}

	std::unique_ptr<LazySequence<T> > Prepend(const LazySequence<T> &items) const {
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(std::unique_ptr<Generator>(
			new PrependGenerator(SharedCopy(), std::make_shared<LazySequence<T> >(items)))));
	}

	std::unique_ptr<LazySequence<T> > InsertAt(const T &item, std::size_t index) const {
		return InsertAt(*createSeqFromElem(item), Ordinal::Finite(index));
	}

	std::unique_ptr<LazySequence<T> > InsertAt(const T &item, int index) const {
		if (index < 0) {
			throw std::out_of_range("Negative index");
		}
		return InsertAt(item, static_cast<std::size_t>(index));
	}

	std::unique_ptr<LazySequence<T> > InsertAt(const Sequence<T> &items, Ordinal index) const {
		return InsertAt(LazySequence<T>(items), index);
	}

	std::unique_ptr<LazySequence<T> > InsertAt(const LazySequence<T> &items, Ordinal index) const {
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(std::unique_ptr<Generator>(
			new InsertGenerator(SharedCopy(), std::make_shared<LazySequence<T> >(items), index))));
	}

	std::unique_ptr<LazySequence<T> > Concat(const LazySequence<T> &other) const {
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(std::unique_ptr<Generator>(
			new ConcatGenerator(SharedCopy(), std::make_shared<LazySequence<T> >(other)))));
	}

	std::unique_ptr<LazySequence<T> > Concat(const LazySequence<T> *other) const {
		if (other == nullptr) {
			throw std::invalid_argument("Concat argument is null");
		}
		return Concat(*other);
	}

	template<class T2>
	std::unique_ptr<LazySequence<T2> > Map(std::function<T2(T)> mapper) const {
		if (!mapper) {
			throw std::invalid_argument("Map function is empty");
		}

		std::unique_ptr<typename LazySequence<T2>::Generator> generator(
			new typename LazySequence<T2>::template MapGeneratorFrom<T>(SharedCopy(), std::move(mapper)));
		return std::unique_ptr<LazySequence<T2> >(new LazySequence<T2>(std::move(generator)));
	}

	std::unique_ptr<LazySequence<T> > Where(std::function<bool(T)> predicate) const {
		if (!predicate) {
			throw std::invalid_argument("Where predicate is empty");
		}

		return std::unique_ptr<LazySequence<T> >(
			new LazySequence<T>(std::unique_ptr<Generator>(
				new WhereGenerator(SharedCopy(), std::move(predicate)))));
	}

	template<class TResult>
	TResult ReduceFirstN(std::size_t count, TResult initialValue,
	                     std::function<TResult(TResult, T)> reducer) const {
		if (!reducer) {
			throw std::invalid_argument("Reduce function is empty");
		}

		TResult result = initialValue;
		for (std::size_t i = 0; i < count; ++i) {
			result = reducer(result, Get(Ordinal::Finite(i)));
		}
		return result;
	}

	template<class TResult>
	TResult Reduce(TResult initialValue, std::function<TResult(TResult, T)> reducer) const {
		Ordinal length = GetLength();
		if (length.IsInfinite()) {
			throw std::logic_error("Cannot fully reduce omega-length LazySequence. Use ReduceFirstN instead.");
		}
		return ReduceFirstN(length.FinitePart(), initialValue, std::move(reducer));
	}

	std::unique_ptr<IEnumerator<T> > GetEnumerator() const {
		class LazyEnumerator : public IEnumerator<T> {
		private:
			const LazySequence<T> *sequence_;
			std::size_t index_;
			bool currentValid_;

		public:
			explicit LazyEnumerator(const LazySequence<T> *sequence)
				: sequence_(sequence), index_(0), currentValid_(false) {
			}

			bool MoveNext() override {
				if (!sequence_->GetLength().ContainsIndex(index_)) {
					currentValid_ = false;
					return false;
				}
				++index_;
				currentValid_ = true;
				return true;
			}

			T Current() const override {
				if (!currentValid_ || index_ == 0) {
					throw std::out_of_range("Enumerator out of range");
				}
			return sequence_->Get(Ordinal::Finite(index_ - 1));
			}

			void Reset() override {
				index_ = 0;
				currentValid_ = false;
			}
		};

		return std::unique_ptr<IEnumerator<T> >(new LazyEnumerator(this));
	}

	std::unique_ptr<Sequence<T> > Take(std::size_t count) const {
		std::unique_ptr<Sequence<T> > result(new MutableArraySequence<T>());
		for (std::size_t i = 0; i < count; ++i) {
			result->Append(Get(Ordinal::Finite(i)));
		}
		return result;
	}
};

#endif
