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

		virtual Ordinal GetLength() const = 0;

		virtual bool HasNext(Ordinal nextIndex) const = 0;

		virtual T GetNext(Ordinal nextIndex) = 0;
	};

	template<class SourceT>
	class MapGeneratorFrom;

	class WhereGenerator;

	struct State {
		explicit State(std::shared_ptr<Generator> generator)
			: generator_(std::move(generator)), cache_() {
		}

		std::shared_ptr<Generator> generator_;
		MutableArraySequence<T> cache_;
	};

	std::shared_ptr<State> state_;

	class EmptyGenerator : public Generator {
	public:
		Ordinal GetLength() const override {
			return Ordinal::Finite(0);
		}

		bool HasNext(Ordinal) const override {
			return false;
		}

		T GetNext(Ordinal) override {
			throw std::out_of_range("LazySequence is empty");
		}
	};

	class ArrayGenerator : public Generator {
	private:
		MutableArraySequence<T> data_;

	public:
		ArrayGenerator(const T *items, std::size_t count) : data_() {
			if (items == nullptr && count > 0) {
				throw std::invalid_argument("LazySequence source array is null");
			}

			for (std::size_t i = 0; i < count; ++i) {
				data_.Append(items[i]);
			}
		}

		explicit ArrayGenerator(const Sequence<T> &source) : data_() {
			auto *enumerator = source.GetEnumerator();
			while (enumerator->MoveNext()) {
				data_.Append(enumerator->Current());
			}
			delete enumerator;
		}

		Ordinal GetLength() const override {
			return Ordinal::Finite(data_.GetLength());
		}

		bool HasNext(Ordinal nextIndex) const override {
			return Ordinal::Finite(data_.GetLength()).ContainsIndex(nextIndex);
		}

		T GetNext(Ordinal nextIndex) override {
			if (!HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}
			return data_.Get(nextIndex.FinitePart());
		}
	};

	class SequenceGenerator : public Generator {
	private:
		Sequence<T> *source_;
		std::unique_ptr<IEnumerator<T> > enumerator_;
		std::size_t position_;

	public:
		explicit SequenceGenerator(Sequence<T> *source)
			: source_(source), enumerator_(nullptr), position_(0) {
			if (source_ == nullptr) {
				throw std::invalid_argument("SequenceGenerator source is null");
			}
		}

		Ordinal GetLength() const override {
			return Ordinal::Finite(source_->GetLength());
		}

		bool HasNext(Ordinal nextIndex) const override {
			return Ordinal::Finite(source_->GetLength()).ContainsIndex(nextIndex);
		}

		T GetNext(Ordinal nextIndex) override {
			if (!HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}
			std::size_t finiteIndex = nextIndex.FinitePart();

			if (!enumerator_ || finiteIndex < position_) {
				enumerator_.reset(source_->GetEnumerator());
				position_ = 0;
			}

			while (position_ <= finiteIndex) {
				if (!enumerator_->MoveNext()) {
					throw std::out_of_range("Index out of range");
				}

				if (position_ == finiteIndex) {
					T result = enumerator_->Current();
					++position_;
					return result;
				}

				++position_;
			}

			throw std::out_of_range("Index out of range");
		}
	};

	class RecurrenceGenerator : public Generator {
	private:
		MutableArraySequence<T> generatedItems_;
		std::function<T(Sequence<T> *)> rule_;
		Ordinal length_;

	public:
		RecurrenceGenerator(std::function<T(Sequence<T> *)> rule, Sequence<T> *firstItems, Ordinal length)//заменить на ссылку
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

		Ordinal GetLength() const override {
			return length_;
		}

		bool HasNext(Ordinal nextIndex) const override {
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
	};

	class IndexFunctionGenerator : public Generator {
	private:
		std::function<T(std::size_t)> rule_;
		Ordinal length_;

	public:
		IndexFunctionGenerator(std::function<T(std::size_t)> rule, Ordinal length)
			: rule_(std::move(rule)), length_(length) {
			if (!rule_) {
				throw std::invalid_argument("Index function rule is empty");
			}
		}

		Ordinal GetLength() const override {
			return length_;
		}

		bool HasNext(Ordinal nextIndex) const override {
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

	static Ordinal SubtractPrefix(Ordinal index, Ordinal prefixLength, const char *message) {
		if (index < prefixLength) {
			throw std::out_of_range(message);
		}
		if (prefixLength.IsFinite()) {
			if (index.IsFinite()) {
				return Ordinal::Finite(index.FinitePart() - prefixLength.FinitePart());
			}
			return index;
		}
		if (index.OmegaCoefficient() == prefixLength.OmegaCoefficient()) {
			return Ordinal::Finite(index.FinitePart() - prefixLength.FinitePart());
		}
		return Ordinal::FromParts(index.OmegaCoefficient() - prefixLength.OmegaCoefficient(), index.FinitePart());
	}

	static Ordinal InclusiveIntervalLength(Ordinal startIndex, Ordinal endIndex) {
		if (endIndex < startIndex) {
			throw std::out_of_range("Invalid subsequence bounds");
		}
		if (startIndex.OmegaCoefficient() == endIndex.OmegaCoefficient()) {
			if (!CanAddOne(endIndex.FinitePart() - startIndex.FinitePart())) {
				throw std::overflow_error("Subsequence length overflow");
			}
			return Ordinal::Finite(endIndex.FinitePart() - startIndex.FinitePart() + 1);
		}
		return AddOneToFinitePart(Ordinal::FromParts(
			endIndex.OmegaCoefficient() - startIndex.OmegaCoefficient(), endIndex.FinitePart()));
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

		Ordinal GetLength() const override {
			return prepended_->GetLength() + source_->GetLength();
		}

		bool HasNext(Ordinal nextIndex) const override {
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
			return source_->Get(SubtractPrefix(nextIndex, prependedLength, "Index out of range"));
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

		Ordinal GetLength() const override {
			return source_->GetLength() + appended_->GetLength();
		}

		bool HasNext(Ordinal nextIndex) const override {
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
			return appended_->Get(SubtractPrefix(nextIndex, length, "Index out of range"));
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

		Ordinal GetLength() const override {
			Ordinal prefixLength = index_;
			Ordinal insertedLength = inserted_->GetLength();
			Ordinal suffixLength = SubtractPrefix(source_->GetLength(), prefixLength, "Insert index out of range");
			return prefixLength + insertedLength + suffixLength;
		}

		bool HasNext(Ordinal nextIndex) const override {
			return GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(Ordinal nextIndex) override {
			if (!HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}
			if (nextIndex < index_) {
				return source_->Get(nextIndex);
			}

			Ordinal localInsertedIndex = SubtractPrefix(nextIndex, index_, "Index out of range");
			Ordinal insertedLength = inserted_->GetLength();
			if (insertedLength.ContainsIndex(localInsertedIndex)) {
				return inserted_->Get(localInsertedIndex);
			}

			Ordinal sourceSuffixIndex = SubtractPrefix(localInsertedIndex, insertedLength, "Index out of range");
			return source_->Get(index_ + sourceSuffixIndex);
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

		Ordinal GetLength() const override {
			return first_->GetLength() + second_->GetLength();
		}

		bool HasNext(Ordinal nextIndex) const override {
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
			return second_->Get(SubtractPrefix(nextIndex, firstLength, "Index out of range"));
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

		Ordinal GetLength() const override {
			return length_;
		}

		bool HasNext(Ordinal nextIndex) const override {
			return length_.ContainsIndex(nextIndex);
		}

		T GetNext(Ordinal nextIndex) override {
			if (!HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}
			Ordinal sourceIndex = start_ + nextIndex;
			return source_->Get(sourceIndex);
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

	static std::shared_ptr<LazySequence<T> > Singleton(const T &item) {
		T items[] = {item};
		return std::make_shared<LazySequence<T> >(items, 1);
	}

	explicit LazySequence(std::shared_ptr<Generator> generator)
		: state_(std::make_shared<State>(std::move(generator))) {
	}

public:
	LazySequence() : state_(std::make_shared<State>(std::make_shared<EmptyGenerator>())) {
	}

	LazySequence(const T *items, std::size_t count)
		: state_(std::make_shared<State>(std::make_shared<ArrayGenerator>(items, count))) {
	}

	explicit LazySequence(const Sequence<T> &sequence)
		: state_(std::make_shared<State>(std::make_shared<ArrayGenerator>(sequence))) {
	}

	explicit LazySequence(Sequence<T> *sequence)
		: state_(std::make_shared<State>(std::make_shared<SequenceGenerator>(sequence))) {
	}

	LazySequence(std::function<T(Sequence<T> *)> recurrenceRule, Sequence<T> *firstItems,
	             Ordinal length = Ordinal::Omega())
		: state_(std::make_shared<State>(
			std::make_shared<RecurrenceGenerator>(std::move(recurrenceRule), firstItems, length))) {
	}

	LazySequence(T (*recurrenceRule)(Sequence<T> *), Sequence<T> *firstItems,
	             Ordinal length = Ordinal::Omega())
		: LazySequence(std::function<T(Sequence<T> *)>(recurrenceRule), firstItems, length) {
	}

	LazySequence(std::function<T(std::size_t)> indexRule, Ordinal length)
		: state_(std::make_shared<State>(
			std::make_shared<IndexFunctionGenerator>(std::move(indexRule), length))) {
	}

	LazySequence(const LazySequence &other) = default;

	LazySequence &operator=(const LazySequence &other) = default;

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

	T Get(std::size_t index) const {
		return Get(Ordinal::Finite(index));
	}

	T Get(int index) const {
		if (index < 0) {
			throw std::out_of_range("Negative index");
		}
		return Get(static_cast<std::size_t>(index));
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
			Ordinal count = SubtractPrefix(sourceLength, startIndex, "Subsequence index out of range");
			return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(
				std::make_shared<SubsequenceGenerator>(SharedCopy(), startIndex, count)));
		}
		if (!sourceLength.ContainsIndex(startIndex) || !sourceLength.ContainsIndex(endIndex)) {
			throw std::out_of_range("Subsequence index out of range");
		}

		Ordinal count = InclusiveIntervalLength(startIndex, endIndex);
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(
			std::make_shared<SubsequenceGenerator>(SharedCopy(), startIndex, count)));
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

	Ordinal GetLength() const {
		return state_->generator_->GetLength();
	}

	std::size_t GetMaterializedCount() const {
		return state_->cache_.GetLength();
	}

	std::unique_ptr<LazySequence<T> > Append(const T &item) const {
		return Append(*Singleton(item));
	}

	std::unique_ptr<LazySequence<T> > Prepend(const T &item) const {
		return Prepend(*Singleton(item));
	}

	std::unique_ptr<LazySequence<T> > Append(const Sequence<T> &items) const {
		return Append(LazySequence<T>(items));
	}

	std::unique_ptr<LazySequence<T> > Append(const LazySequence<T> &items) const {
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(std::make_shared<AppendGenerator>(
			SharedCopy(), std::make_shared<LazySequence<T> >(items))));
	}

	std::unique_ptr<LazySequence<T> > Prepend(const Sequence<T> &items) const {
		return Prepend(LazySequence<T>(items));
	}

	std::unique_ptr<LazySequence<T> > Prepend(const LazySequence<T> &items) const {
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(std::make_shared<PrependGenerator>(
			SharedCopy(), std::make_shared<LazySequence<T> >(items))));
	}

	std::unique_ptr<LazySequence<T> > InsertAt(const T &item, std::size_t index) const {
		return InsertAt(*Singleton(item), Ordinal::Finite(index));
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
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(std::make_shared<InsertGenerator>(
			SharedCopy(), std::make_shared<LazySequence<T> >(items), index)));
	}

	std::unique_ptr<LazySequence<T> > Concat(const LazySequence<T> &other) const {
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(
			std::make_shared<ConcatGenerator>(SharedCopy(), std::make_shared<LazySequence<T> >(other))));
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

		auto generator =
				std::make_shared<typename LazySequence<T2>::template MapGeneratorFrom<T> >(SharedCopy(),
					std::move(mapper));
		return std::unique_ptr<LazySequence<T2> >(new LazySequence<T2>(generator));
	}

	std::unique_ptr<LazySequence<T> > Where(std::function<bool(T)> predicate) const {
		if (!predicate) {
			throw std::invalid_argument("Where predicate is empty");
		}

		return std::unique_ptr<LazySequence<T> >(
			new LazySequence<T>(std::make_shared<WhereGenerator>(SharedCopy(), std::move(predicate))));
	}

	template<class TResult>
	TResult ReduceFirstN(std::size_t count, TResult initialValue,
	                     std::function<TResult(TResult, T)> reducer) const {
		if (!reducer) {
			throw std::invalid_argument("Reduce function is empty");
		}

		TResult result = initialValue;
		for (std::size_t i = 0; i < count; ++i) {
			result = reducer(result, Get(i));
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
				return sequence_->Get(index_ - 1);
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
			result->Append(Get(i));
		}
		return result;
	}

private:
	template<class SourceT>
	class MapGeneratorFrom : public Generator {
	private:
		std::shared_ptr<LazySequence<SourceT> > source_;
		std::function<T(SourceT)> mapper_;

	public:
		MapGeneratorFrom(std::shared_ptr<LazySequence<SourceT> > source, std::function<T(SourceT)> mapper)
			: source_(std::move(source)), mapper_(std::move(mapper)) {
		}

		Ordinal GetLength() const override {
			return source_->GetLength();
		}

		bool HasNext(Ordinal nextIndex) const override {
			return source_->GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(Ordinal nextIndex) override {
			return mapper_(source_->Get(nextIndex));
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

		Ordinal GetLength() const override {
			Ordinal sourceLength = source_->GetLength();
			if (sourceLength.IsInfinite()) {
				return Ordinal::Omega();
			}

			std::size_t count = 0;
			for (std::size_t i = 0; i < sourceLength.FinitePart(); ++i) {
				if (predicate_(source_->Get(i))) {
					++count;
				}
			}
			return Ordinal::Finite(count);
		}

		bool HasNext(Ordinal nextIndex) const override {
			return GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(Ordinal nextIndex) override {
			if (!nextIndex.IsFinite()) {
				throw std::logic_error("Where generator supports only finite indexes");
			}
			Ordinal sourceLength = source_->GetLength();
			while (sourceLength.IsInfinite() || sourceIndex_ < sourceLength.FinitePart()) {
				T current = source_->Get(sourceIndex_);
				++sourceIndex_;
				if (predicate_(current)) {
					return current;
				}
			}
			throw std::out_of_range("No next element satisfies predicate");
		}
	};

	template<class U>
	friend class LazySequence;
};

#endif
