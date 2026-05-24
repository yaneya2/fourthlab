#ifndef FOURTHLAB_LAZY_SEQUENCE_H
#define FOURTHLAB_LAZY_SEQUENCE_H

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "Cardinal.h"
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

		virtual Cardinal GetLength() const = 0;

		virtual bool HasNext(std::size_t nextIndex) const = 0;

		virtual T GetNext(std::size_t nextIndex, Sequence<T> *materializedPrefix) = 0;
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
		Cardinal GetLength() const override {
			return Cardinal::Finite(0);
		}

		bool HasNext(std::size_t) const override {
			return false;
		}

		T GetNext(std::size_t, Sequence<T> *) override {
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

		Cardinal GetLength() const override {
			return Cardinal::Finite(data_.GetLength());
		}

		bool HasNext(std::size_t nextIndex) const override {
			return nextIndex < data_.GetLength();
		}

		T GetNext(std::size_t nextIndex, Sequence<T> *) override {
			if (!HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}
			return data_.Get(nextIndex);
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

		Cardinal GetLength() const override {
			return Cardinal::Finite(source_->GetLength());
		}

		bool HasNext(std::size_t nextIndex) const override {
			return nextIndex < source_->GetLength();
		}

		T GetNext(std::size_t nextIndex, Sequence<T> *) override {
			if (!HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}

			if (!enumerator_ || nextIndex < position_) {
				enumerator_.reset(source_->GetEnumerator());
				position_ = 0;
			}

			while (position_ <= nextIndex) {
				if (!enumerator_->MoveNext()) {
					throw std::out_of_range("Index out of range");
				}

				if (position_ == nextIndex) {
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
		MutableArraySequence<T> firstItems_;
		std::function<T(Sequence<T> *)> rule_;
		Cardinal length_;

	public:
		RecurrenceGenerator(std::function<T(Sequence<T> *)> rule, Sequence<T> *firstItems, Cardinal length)
			: firstItems_(), rule_(std::move(rule)), length_(length) {
			if (!rule_) {
				throw std::invalid_argument("Recurrence rule is empty");
			}
			if (firstItems == nullptr) {
				throw std::invalid_argument("Recurrence seed sequence is null");
			}
			if (length_.IsFinite() && length_.Value() < firstItems->GetLength()) {
				throw std::invalid_argument("Finite length is less than seed count");
			}

			auto *enumerator = firstItems->GetEnumerator();
			while (enumerator->MoveNext()) {
				firstItems_.Append(enumerator->Current());
			}
			delete enumerator;
		}

		Cardinal GetLength() const override {
			return length_;
		}

		bool HasNext(std::size_t nextIndex) const override {
			return length_.ContainsIndex(nextIndex);
		}

		T GetNext(std::size_t nextIndex, Sequence<T> *materializedPrefix) override {
			if (!HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}
			if (nextIndex < firstItems_.GetLength()) {
				return firstItems_.Get(nextIndex);
			}
			return rule_(materializedPrefix);
		}
	};

	class IndexFunctionGenerator : public Generator {
	private:
		std::function<T(std::size_t)> rule_;
		Cardinal length_;

	public:
		IndexFunctionGenerator(std::function<T(std::size_t)> rule, Cardinal length)
			: rule_(std::move(rule)), length_(length) {
			if (!rule_) {
				throw std::invalid_argument("Index function rule is empty");
			}
		}

		Cardinal GetLength() const override {
			return length_;
		}

		bool HasNext(std::size_t nextIndex) const override {
			return length_.ContainsIndex(nextIndex);
		}

		T GetNext(std::size_t nextIndex, Sequence<T> *) override {
			if (!HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}
			return rule_(nextIndex);
		}
	};

	class PrependGenerator : public Generator {
	private:
		std::shared_ptr<LazySequence<T> > source_;
		T item_;

	public:
		PrependGenerator(std::shared_ptr<LazySequence<T> > source, const T &item)
			: source_(std::move(source)), item_(item) {
		}

		Cardinal GetLength() const override {
			return Cardinal::Finite(1) + source_->GetLength();
		}

		bool HasNext(std::size_t nextIndex) const override {
			return GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(std::size_t nextIndex, Sequence<T> *) override {
			if (nextIndex == 0) {
				return item_;
			}
			return source_->Get(nextIndex - 1);
		}
	};

	class AppendGenerator : public Generator {
	private:
		std::shared_ptr<LazySequence<T> > source_;
		T item_;

	public:
		AppendGenerator(std::shared_ptr<LazySequence<T> > source, const T &item)
			: source_(std::move(source)), item_(item) {
		}

		Cardinal GetLength() const override {
			Cardinal length = source_->GetLength();
			if (length.IsOmega()) {
				return Cardinal::Omega();
			}
			return Cardinal::Finite(length.Value() + 1);
		}

		bool HasNext(std::size_t nextIndex) const override {
			return GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(std::size_t nextIndex, Sequence<T> *) override {
			Cardinal length = source_->GetLength();
			if (length.IsOmega()) {
				return source_->Get(nextIndex);
			}
			if (nextIndex < length.Value()) {
				return source_->Get(nextIndex);
			}
			if (nextIndex == length.Value()) {
				return item_;
			}
			throw std::out_of_range("Index out of range");
		}
	};

	class InsertGenerator : public Generator {
	private:
		std::shared_ptr<LazySequence<T> > source_;
		T item_;
		std::size_t index_;

	public:
		InsertGenerator(std::shared_ptr<LazySequence<T> > source, const T &item, std::size_t index)
			: source_(std::move(source)), item_(item), index_(index) {
			Cardinal length = source_->GetLength();
			if (length.IsFinite() && index_ >= length.Value()) {
				throw std::out_of_range("Insert index out of range");
			}
		}

		Cardinal GetLength() const override {
			Cardinal length = source_->GetLength();
			if (length.IsOmega()) {
				return Cardinal::Omega();
			}
			return Cardinal::Finite(length.Value() + 1);
		}

		bool HasNext(std::size_t nextIndex) const override {
			return GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(std::size_t nextIndex, Sequence<T> *) override {
			if (nextIndex < index_) {
				return source_->Get(nextIndex);
			}
			if (nextIndex == index_) {
				return item_;
			}
			return source_->Get(nextIndex - 1);
		}
	};

	class RemoveAtGenerator : public Generator {
	private:
		std::shared_ptr<LazySequence<T> > source_;
		std::size_t index_;

	public:
		RemoveAtGenerator(std::shared_ptr<LazySequence<T> > source, std::size_t index)
			: source_(std::move(source)), index_(index) {
			Cardinal length = source_->GetLength();
			if (length.IsFinite() && index_ >= length.Value()) {
				throw std::out_of_range("Remove index out of range");
			}
		}

		Cardinal GetLength() const override {
			Cardinal length = source_->GetLength();
			if (length.IsOmega()) {
				return Cardinal::Omega();
			}
			return Cardinal::Finite(length.Value() - 1);
		}

		bool HasNext(std::size_t nextIndex) const override {
			return GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(std::size_t nextIndex, Sequence<T> *) override {
			if (nextIndex < index_) {
				return source_->Get(nextIndex);
			}
			return source_->Get(nextIndex + 1);
		}
	};

	class RemoveRangeGenerator : public Generator {
	private:
		std::shared_ptr<LazySequence<T> > source_;
		std::size_t start_;
		std::size_t count_;

	public:
		RemoveRangeGenerator(std::shared_ptr<LazySequence<T> > source, std::size_t start, std::size_t count)
			: source_(std::move(source)), start_(start), count_(count) {
			Cardinal length = source_->GetLength();
			if (length.IsFinite()) {
				if (count_ > 0 && start_ >= length.Value()) {
					throw std::out_of_range("Remove range start is out of range");
				}
				if (count_ > length.Value() - start_) {
					throw std::out_of_range("Remove range is out of range");
				}
			}
		}

		Cardinal GetLength() const override {
			Cardinal length = source_->GetLength();
			if (length.IsOmega()) {
				return Cardinal::Omega();
			}
			return Cardinal::Finite(length.Value() - count_);
		}

		bool HasNext(std::size_t nextIndex) const override {
			return GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(std::size_t nextIndex, Sequence<T> *) override {
			if (nextIndex < start_) {
				return source_->Get(nextIndex);
			}
			return source_->Get(nextIndex + count_);
		}
	};

	class InsertSequenceGenerator : public Generator {
	private:
		std::shared_ptr<LazySequence<T> > source_;
		std::shared_ptr<LazySequence<T> > inserted_;
		std::size_t index_;

	public:
		InsertSequenceGenerator(std::shared_ptr<LazySequence<T> > source,
		                        std::shared_ptr<LazySequence<T> > inserted,
		                        std::size_t index)
			: source_(std::move(source)), inserted_(std::move(inserted)), index_(index) {
			Cardinal length = source_->GetLength();
			if (length.IsFinite() && index_ >= length.Value()) {
				throw std::out_of_range("Insert index out of range");
			}
		}

		Cardinal GetLength() const override {
			Cardinal sourceLength = source_->GetLength();
			Cardinal insertedLength = inserted_->GetLength();
			if (sourceLength.IsOmega() || insertedLength.IsOmega()) {
				return Cardinal::Omega();
			}
			return Cardinal::Finite(sourceLength.Value() + insertedLength.Value());
		}

		bool HasNext(std::size_t nextIndex) const override {
			return GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(std::size_t nextIndex, Sequence<T> *) override {
			Cardinal insertedLength = inserted_->GetLength();
			if (nextIndex < index_) {
				return source_->Get(nextIndex);
			}
			if (insertedLength.IsOmega()) {
				return inserted_->Get(nextIndex - index_);
			}
			if (nextIndex < index_ + insertedLength.Value()) {
				return inserted_->Get(nextIndex - index_);
			}
			return source_->Get(nextIndex - insertedLength.Value());
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

		Cardinal GetLength() const override {
			return first_->GetLength() + second_->GetLength();
		}

		bool HasNext(std::size_t nextIndex) const override {
			return GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(std::size_t nextIndex, Sequence<T> *) override {
			Cardinal firstLength = first_->GetLength();
			if (firstLength.IsOmega()) {
				return first_->Get(nextIndex);
			}
			if (nextIndex < firstLength.Value()) {
				return first_->Get(nextIndex);
			}
			return second_->Get(nextIndex - firstLength.Value());
		}
	};

	class SubsequenceGenerator : public Generator {
	private:
		std::shared_ptr<LazySequence<T> > source_;
		std::size_t start_;
		Cardinal length_;

	public:
		SubsequenceGenerator(std::shared_ptr<LazySequence<T> > source, std::size_t start, Cardinal length)
			: source_(std::move(source)), start_(start), length_(length) {
		}

		Cardinal GetLength() const override {
			return length_;
		}

		bool HasNext(std::size_t nextIndex) const override {
			return length_.ContainsIndex(nextIndex);
		}

		T GetNext(std::size_t nextIndex, Sequence<T> *) override {
			if (!HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}
			Cardinal sourceIndex = Cardinal::Finite(start_) + Cardinal::Finite(nextIndex);
			return source_->Get(sourceIndex);
		}
	};

	static std::size_t RequireFiniteIndex(Cardinal index, const char *message) {
		if (index.IsOmega()) {
			throw std::logic_error(message);
		}
		return index.Value();
	}

	void EnsureMaterialized(std::size_t index) const {
		if (!state_->generator_->HasNext(index)) {
			throw std::out_of_range("Index out of range");
		}

		while (state_->cache_.GetLength() <= index) {
			std::size_t nextIndex = state_->cache_.GetLength();
			if (!state_->generator_->HasNext(nextIndex)) {
				throw std::out_of_range("Index out of range");
			}
			T value = state_->generator_->GetNext(nextIndex, &state_->cache_);
			state_->cache_.Append(value);
		}
	}

	std::shared_ptr<LazySequence<T> > SharedCopy() const {
		return std::make_shared<LazySequence<T> >(*this);
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
	             Cardinal length = Cardinal::Omega())
		: state_(std::make_shared<State>(
			std::make_shared<RecurrenceGenerator>(std::move(recurrenceRule), firstItems, length))) {
	}

	LazySequence(T (*recurrenceRule)(Sequence<T> *), Sequence<T> *firstItems,
	             Cardinal length = Cardinal::Omega())
		: LazySequence(std::function<T(Sequence<T> *)>(recurrenceRule), firstItems, length) {
	}

	LazySequence(std::function<T(std::size_t)> indexRule, Cardinal length)
		: state_(std::make_shared<State>(
			std::make_shared<IndexFunctionGenerator>(std::move(indexRule), length))) {
	}

	LazySequence(const LazySequence &other) = default;

	LazySequence &operator=(const LazySequence &other) = default;

	static std::unique_ptr<LazySequence<T> > FromIndexFunction(std::function<T(std::size_t)> indexRule,
	                                                           Cardinal length) {
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(std::move(indexRule), length));
	}

	static std::unique_ptr<LazySequence<T> > Infinite(std::function<T(std::size_t)> indexRule) {
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(std::move(indexRule), Cardinal::Omega()));
	}

	T GetFirst() const {
		return Get(Cardinal::Finite(0));
	}

	T GetLast() const {
		Cardinal length = GetLength();
		if (length.IsOmega()) {
			throw std::logic_error("Cannot get last element of omega-length LazySequence");
		}
		if (length.Value() == 0) {
			throw std::out_of_range("LazySequence is empty");
		}
		return Get(Cardinal::Finite(length.Value() - 1));
	}

	T Get(Cardinal index) const {
		std::size_t finiteIndex = RequireFiniteIndex(index, "Omega cannot be used as LazySequence index");
		EnsureMaterialized(finiteIndex);
		return state_->cache_.Get(finiteIndex);
	}

	T Get(std::size_t index) const {
		return Get(Cardinal::Finite(index));
	}

	T Get(int index) const {
		if (index < 0) {
			throw std::out_of_range("Negative index");
		}
		return Get(static_cast<std::size_t>(index));
	}

	std::unique_ptr<LazySequence<T> > GetSubsequence(Cardinal startIndex, Cardinal endIndex) const {
		std::size_t finiteStart = RequireFiniteIndex(startIndex, "Omega cannot be used as subsequence start");
		Cardinal sourceLength = GetLength();

		if (endIndex.IsFinite()) {
			std::size_t finiteEnd = endIndex.Value();
			if (finiteStart > finiteEnd) {
				throw std::out_of_range("Invalid subsequence bounds");
			}
			if (sourceLength.IsFinite() && finiteEnd >= sourceLength.Value()) {
				throw std::out_of_range("Subsequence index out of range");
			}
			Cardinal count = Cardinal::Finite(finiteEnd - finiteStart) + Cardinal::Finite(1);
			return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(
				std::make_shared<SubsequenceGenerator>(SharedCopy(), finiteStart, count)));
		}

		if (sourceLength.IsFinite()) {
			throw std::out_of_range("Subsequence index out of range");
		}

		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(
			std::make_shared<SubsequenceGenerator>(SharedCopy(), finiteStart, Cardinal::Omega())));
	}

	std::unique_ptr<LazySequence<T> > GetSubsequence(std::size_t startIndex, std::size_t endIndex) const {
		return GetSubsequence(Cardinal::Finite(startIndex), Cardinal::Finite(endIndex));
	}

	std::unique_ptr<LazySequence<T> > GetSubsequence(int startIndex, int endIndex) const {
		if (startIndex < 0 || endIndex < 0) {
			throw std::out_of_range("Invalid subsequence bounds");
		}
		return GetSubsequence(static_cast<std::size_t>(startIndex), static_cast<std::size_t>(endIndex));
	}

	Cardinal GetLength() const {
		return state_->generator_->GetLength();
	}

	std::size_t GetMaterializedCount() const {
		return state_->cache_.GetLength();
	}

	std::unique_ptr<LazySequence<T> > Append(const T &item) const {
		return std::unique_ptr<LazySequence<T> >(
			new LazySequence<T>(std::make_shared<AppendGenerator>(SharedCopy(), item)));
	}

	std::unique_ptr<LazySequence<T> > Prepend(const T &item) const {
		return std::unique_ptr<LazySequence<T> >(
			new LazySequence<T>(std::make_shared<PrependGenerator>(SharedCopy(), item)));
	}

	std::unique_ptr<LazySequence<T> > InsertAt(const T &item, std::size_t index) const {
		return std::unique_ptr<LazySequence<T> >(
			new LazySequence<T>(std::make_shared<InsertGenerator>(SharedCopy(), item, index)));
	}

	std::unique_ptr<LazySequence<T> > InsertAt(const T &item, int index) const {
		if (index < 0) {
			throw std::out_of_range("Negative index");
		}
		return InsertAt(item, static_cast<std::size_t>(index));
	}

	std::unique_ptr<LazySequence<T> > RemoveAt(std::size_t index) const {
		return std::unique_ptr<LazySequence<T> >(
			new LazySequence<T>(std::make_shared<RemoveAtGenerator>(SharedCopy(), index)));
	}

	std::unique_ptr<LazySequence<T> > RemoveRange(std::size_t startIndex, std::size_t count) const {
		return std::unique_ptr<LazySequence<T> >(
			new LazySequence<T>(std::make_shared<RemoveRangeGenerator>(SharedCopy(), startIndex, count)));
	}

	std::unique_ptr<LazySequence<T> > InsertAt(const Sequence<T> &items, std::size_t index) const {
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(std::make_shared<InsertSequenceGenerator>(
			SharedCopy(), std::make_shared<LazySequence<T> >(items), index)));
	}

	std::unique_ptr<LazySequence<T> > InsertAt(const LazySequence<T> &items, std::size_t index) const {
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(std::make_shared<InsertSequenceGenerator>(
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
		Cardinal length = GetLength();
		if (length.IsOmega()) {
			throw std::logic_error("Cannot fully reduce omega-length LazySequence. Use ReduceFirstN instead.");
		}
		return ReduceFirstN(length.Value(), initialValue, std::move(reducer));
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

		Cardinal GetLength() const override {
			return source_->GetLength();
		}

		bool HasNext(std::size_t nextIndex) const override {
			return source_->GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(std::size_t nextIndex, Sequence<T> *) override {
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

		Cardinal GetLength() const override {
			Cardinal sourceLength = source_->GetLength();
			if (sourceLength.IsOmega()) {
				return Cardinal::Omega();
			}

			std::size_t count = 0;
			for (std::size_t i = 0; i < sourceLength.Value(); ++i) {
				if (predicate_(source_->Get(i))) {
					++count;
				}
			}
			return Cardinal::Finite(count);
		}

		bool HasNext(std::size_t nextIndex) const override {
			return GetLength().ContainsIndex(nextIndex);
		}

		T GetNext(std::size_t, Sequence<T> *) override {
			Cardinal sourceLength = source_->GetLength();
			while (sourceLength.IsOmega() || sourceIndex_ < sourceLength.Value()) {
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
