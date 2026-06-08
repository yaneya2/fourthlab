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
#include "Generator.h"
#include "LazySequenceGenerators.h"

template<class T>
class LazySequence {
private:
	std::unique_ptr<Generator<T> > generator;
	mutable MutableArraySequence<T> cache;

	void EnsureMaterialized(std::size_t index) const{
		while (cache.GetLength() <= index) {
			std::size_t nextIndex = cache.GetLength();
			Ordinal ordinalIndex = Ordinal::Finite(nextIndex);
			if (!generator->HasNext(ordinalIndex)) {
				throw std::out_of_range("Index out of range");
			}
			T value = generator->GetNext(ordinalIndex);
			cache.Append(value);
		}
	}

	std::shared_ptr<LazySequence<T> > SharedCopy() const {
		return std::make_shared<LazySequence<T> >(*this);
	}

	static std::shared_ptr<LazySequence<T> > CreateSeqFromElem(const T &item) {
		T items[] = {item};
		return std::make_shared<LazySequence<T> >(items, 1);
	}

	explicit LazySequence(std::unique_ptr<Generator<T> > generatorPtr)
		: generator(std::move(generatorPtr)), cache() {
	}

	template<class U>
	friend class LazySequence;

public:
	LazySequence()
		: generator(std::unique_ptr<Generator<T> >(new EmptyGenerator<T>())), cache() {
	}

	LazySequence(const T *items, std::size_t count)
		: generator(std::unique_ptr<Generator<T> >(new SequenceGenerator<T>(items, count))), cache() {
	}

	explicit LazySequence(const Sequence<T> &sequence)
		: generator(std::unique_ptr<Generator<T> >(new SequenceGenerator<T>(sequence))), cache() {
	}

	explicit LazySequence(Sequence<T> *sequence) {
		if (sequence == nullptr) {
			throw std::invalid_argument("LazySequence source sequence is null");
		}
		this->generator = std::unique_ptr<Generator<T> >(new SequenceGenerator<T>(*sequence));
		cache = MutableArraySequence<T>();
	}

	LazySequence(std::function<T(Sequence<T> *)> recurrenceRule, Sequence<T> *firstItems,
	             Ordinal length = Ordinal::Omega())
		: generator(std::unique_ptr<Generator<T> >(
			new RecurrenceGenerator<T>(std::move(recurrenceRule), firstItems, length))), cache() {
	}

	LazySequence(T (*recurrenceRule)(Sequence<T> *), Sequence<T> *firstItems,
	             Ordinal length = Ordinal::Omega())
		: LazySequence(std::function<T(Sequence<T> *)>(recurrenceRule), firstItems, length) {
	}

	LazySequence(std::function<T(std::size_t)> indexRule, Ordinal length)
		: generator(std::unique_ptr<Generator<T> >(new FunctionGenerator<T>(std::move(indexRule), length))), cache() {
	}

	LazySequence(const LazySequence &other)
		: generator(other.generator->Clone()), cache(other.cache) {
	}

	LazySequence(LazySequence &&other) noexcept = default;

	LazySequence &operator=(const LazySequence &other) {
		if (this != &other) {
			generator = other.generator->Clone();
			cache = other.cache;
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
		if (!generator->HasNext(index)) {
			throw std::out_of_range("Index out of range");
		}
		if (!index.IsFinite()) {
			return generator->GetNext(index);
		}

		std::size_t finiteIndex = index.FinitePart();
		EnsureMaterialized(finiteIndex);
		return cache.Get(finiteIndex);
	}

	std::unique_ptr<LazySequence<T> > GetSubsequence(Ordinal startIndex, Ordinal endIndex) const {
		Ordinal sourceLength = GetLength();
		if (endIndex < startIndex) {
			throw std::out_of_range("Invalid subsequence bounds");
		}
		if (sourceLength.IsInfinite() && endIndex == sourceLength) {
			if (!(startIndex < sourceLength)) {
				throw std::out_of_range("Subsequence index out of range");
			}
			Ordinal count = CalcRangeLength(sourceLength, startIndex);
			return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(
				std::unique_ptr<Generator<T> >(new SubsequenceGenerator<T>(SharedCopy(), startIndex, count))));
		}
		if (!(startIndex < sourceLength) || !(endIndex < sourceLength)) {
			throw std::out_of_range("Subsequence index out of range");
		}

		Ordinal count = CalcRangeLength(endIndex, startIndex, true);
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(
			std::unique_ptr<Generator<T> >(new SubsequenceGenerator<T>(SharedCopy(), startIndex, count))));
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
		return generator->GetLength();
	}

	[[nodiscard]] std::size_t GetMaterializedCount() const {
		return cache.GetLength();
	}

	std::unique_ptr<LazySequence<T> > Append(const T &item) const {
		return Append(*CreateSeqFromElem(item));
	}

	std::unique_ptr<LazySequence<T> > Prepend(const T &item) const {
		return Prepend(*CreateSeqFromElem(item));
	}

	std::unique_ptr<LazySequence<T> > Append(const Sequence<T> &items) const {
		return Append(LazySequence<T>(items));
	}

	std::unique_ptr<LazySequence<T> > Append(const LazySequence<T> &items) const {
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(std::unique_ptr<Generator<T> >(
			new AppendGenerator<T>(SharedCopy(), std::make_shared<LazySequence<T> >(items)))));
	}

	std::unique_ptr<LazySequence<T> > Prepend(const Sequence<T> &items) const {
		return Prepend(LazySequence<T>(items));
	}

	std::unique_ptr<LazySequence<T> > Prepend(const LazySequence<T> &items) const {
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(std::unique_ptr<Generator<T> >(
			new PrependGenerator<T>(SharedCopy(), std::make_shared<LazySequence<T> >(items)))));
	}

	std::unique_ptr<LazySequence<T> > InsertAt(const T &item, std::size_t index) const {
		return InsertAt(*CreateSeqFromElem(item), Ordinal::Finite(index));
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
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(std::unique_ptr<Generator<T> >(
			new InsertGenerator<T>(SharedCopy(), std::make_shared<LazySequence<T> >(items), index))));
	}

	std::unique_ptr<LazySequence<T> > Concat(const LazySequence<T> &other) const {
		return std::unique_ptr<LazySequence<T> >(new LazySequence<T>(std::unique_ptr<Generator<T> >(
			new ConcatGenerator<T>(SharedCopy(), std::make_shared<LazySequence<T> >(other)))));
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

		std::unique_ptr<Generator<T2> > generator(
			new MapGeneratorFrom<T2, T>(SharedCopy(), std::move(mapper)));
		return std::unique_ptr<LazySequence<T2> >(new LazySequence<T2>(std::move(generator)));
	}

	std::unique_ptr<LazySequence<T> > Where(std::function<bool(T)> predicate) const {
		if (!predicate) {
			throw std::invalid_argument("Where predicate is empty");
		}

		return std::unique_ptr<LazySequence<T> >(
			new LazySequence<T>(std::unique_ptr<Generator<T> >(
				new WhereGenerator<T>(SharedCopy(), std::move(predicate)))));
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
			const LazySequence<T> *sequence;
			std::size_t index;
			bool currentValid;

		public:
			explicit LazyEnumerator(const LazySequence<T> *sequence)
				: sequence(sequence), index(0), currentValid(false) {
			}

			bool MoveNext() override {
				if (!(Ordinal::Finite(index) < sequence->GetLength())) {
					currentValid = false;
					return false;
				}
				++index;
				currentValid = true;
				return true;
			}

			T Current() const override {
				if (!currentValid || index == 0) {
					throw std::out_of_range("Enumerator out of range");
				}
				return sequence->Get(Ordinal::Finite(index - 1));
			}

			void Reset() override {
				index = 0;
				currentValid = false;
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
