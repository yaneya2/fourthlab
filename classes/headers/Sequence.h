#ifndef SEQUENCE_H
#define SEQUENCE_H

#include <any>
#include <stdexcept>
#include "Option.h"
#include "IEnumerator.h"
#include <limits>
#include <tuple>

#include "DynamicArray.h"

template<typename T>
class MutableArraySequence;

template<typename T>
class Sequence {
protected:
    virtual Sequence<T> *Instance() = 0;

    virtual Sequence<T> *AppendImpl(const T &elem) = 0;

    virtual Sequence<T> *PrependImpl(const T &elem) = 0;

    virtual Sequence<T> *InsertAtImpl(const T &elem, size_t index) = 0;

    virtual Sequence<T> *ConcatImpl(const Sequence<T> &other) = 0;

    virtual Sequence<T> *DelImpl(size_t index) = 0;

    virtual Sequence<T> *CreateEmpty() const = 0;

public:
    virtual T GetFirst() const = 0;

    virtual T GetLast() const = 0;

    virtual size_t GetLength() const = 0;

    virtual Sequence<T> *GetSubsequence(size_t startIndex, size_t endIndex) const = 0;

    virtual IEnumerator<T> *GetEnumerator() const = 0;

    Sequence<T> *Append(const T &elem) {
        return Instance()->AppendImpl(elem);
    }

    Sequence<T> *Prepend(const T &elem) {
        return Instance()->PrependImpl(elem);
    }

    Sequence<T> *InsertAt(const T &elem, size_t index) {
        return Instance()->InsertAtImpl(elem, index);
    }

    Sequence<T> *Concat(const Sequence<T> &list) {
        return Instance()->ConcatImpl(list);
    }

    Sequence<T> *Del(size_t index) {
        return Instance()->DelImpl(index);
    }

    template<typename T2>
    Sequence<T> *Map(T2 (&func)(T)) {
        Sequence<T> *result = CreateEmpty();
        auto enumerator = this->GetEnumerator();
        while (enumerator->MoveNext()) {
            result = result->Append(func(enumerator->Current()));
        }
        return result;
    }

    Sequence<T> *Where(bool (&func)(T)) {
        Sequence<T> *result = CreateEmpty();
        auto enumerator = this->GetEnumerator();
        while (enumerator->MoveNext()) {
            if (func(enumerator->Current())) {
                result = result->Append(enumerator->Current());
            }
        }
        return result;
    }

    template<typename T2>
    T Reduce(T2 (&func)(T2, T)) {
        size_t len = GetLength();
        if (len == 0)
            throw std::out_of_range("Cannot reduce empty sequence");
        auto enumerator = this->GetEnumerator();
        enumerator->MoveNext();
        T acc = enumerator->Current();
        while (enumerator->MoveNext()) {
            acc = func(acc, enumerator->Current());
        }
        return acc;
    }

    Option<T> GetFirstP(bool (&func)(T)) {
        auto enumerator = this->GetEnumerator();
        while (enumerator->MoveNext()) {
            T elem = enumerator->Current();
            if (func(elem)) return Option<T>(elem);
        }
        return Option<T>();
    }

    Option<T> GetLastP(bool (&func)(T)) {
        auto enumerator = this->GetEnumerator();
        T res{};
        bool flag = false;
        while (enumerator->MoveNext()) {
            T elem = enumerator->Current();
            if (func(elem)) {
                res = elem;
                flag = true;
            }
        }
        if (flag)
            return Option<T>(res);
        return Option<T>();
    }

    // template<typename ... Args>
    // static Sequence<Sequence<std::any>> *zip(const Args& ... param) {
    //
    //
    // }


    static Sequence<Sequence<T> *> *Zip(const Sequence<Sequence<T> *> &outerSequence) {
        size_t outerLength = outerSequence.GetLength();
        if (outerLength == 0) {
            return new MutableArraySequence<Sequence<T> *>();
        }

        size_t minLength = std::numeric_limits<size_t>::max();
        auto *outerEnumerator = outerSequence.GetEnumerator();
        while (outerEnumerator->MoveNext()) {
            Sequence<T> *innerSequence = outerEnumerator->Current();
            if (innerSequence->GetLength() < minLength) {
                minLength = innerSequence->GetLength();
            }
        }
        delete outerEnumerator;

        if (minLength == 0) {
            return new MutableArraySequence<Sequence<T> *>();
        }

        DynamicArray<IEnumerator<T> *> innerEnumerators(outerLength);
        {
            size_t sourceIdx = 0;
            auto *tempOuterEnumerator = outerSequence.GetEnumerator();
            while (tempOuterEnumerator->MoveNext()) {
                innerEnumerators.Set(sourceIdx++,tempOuterEnumerator->Current()->GetEnumerator());
            }
            delete tempOuterEnumerator;
        }

        Sequence<Sequence<T> *> *result = new MutableArraySequence<Sequence<T> *>();
        for (size_t targetIdx = 0; targetIdx < minLength; ++targetIdx) {
            Sequence<T> *targetSequence = new MutableArraySequence<T>();
            for (size_t sourceIdx = 0; sourceIdx < outerLength; ++sourceIdx) {
                innerEnumerators.Get(sourceIdx)->MoveNext();
                targetSequence = targetSequence->Append(innerEnumerators.Get(sourceIdx)->Current());

            }
            result = result->Append(targetSequence);
        }

        for (size_t targetIdx = 0; targetIdx < outerLength; ++targetIdx) {
            delete innerEnumerators.Get(targetIdx);
        }

        return result;
    }

    static Sequence<Sequence<T> *> *Unzip(const Sequence<Sequence<T> *> &outerSequence) {
        size_t outerLength = outerSequence.GetLength();
        if (outerLength == 0) {
            return new MutableArraySequence<Sequence<T> *>();
        }

        size_t minLength = std::numeric_limits<size_t>::max();
        auto *outerEnumerator = outerSequence.GetEnumerator();
        while (outerEnumerator->MoveNext()) {
            Sequence<T> *innerSequence = outerEnumerator->Current();
            if (innerSequence->GetLength() < minLength) {
                minLength = innerSequence->GetLength();
            }
        }
        delete outerEnumerator;

        if (minLength == 0) {
            return new MutableArraySequence<Sequence<T> *>();
        }

        DynamicArray<IEnumerator<T> *> innerEnumerators(outerLength);
        {
            size_t sourceIdx = 0;
            auto *tempOuterEnumerator = outerSequence.GetEnumerator();
            while (tempOuterEnumerator->MoveNext()) {
                innerEnumerators.Set(sourceIdx++,tempOuterEnumerator->Current()->GetEnumerator());
            }
            delete tempOuterEnumerator;
        }

        Sequence<Sequence<T> *> *result = new MutableArraySequence<Sequence<T> *>();
        for (size_t targetIdx = 0; targetIdx < minLength; ++targetIdx) {
            Sequence<T> *targetSequence = new MutableArraySequence<T>();
            for (size_t sourceIdx = 0; sourceIdx < outerLength; ++sourceIdx) {
                innerEnumerators.Get(sourceIdx)->MoveNext();
                targetSequence = targetSequence->Append(innerEnumerators.Get(sourceIdx)->Current());
            }
            result = result->Append(targetSequence);
        }

        for (size_t targetIdx = 0; targetIdx < outerLength; ++targetIdx) {
            delete innerEnumerators.Get(targetIdx);
        }

        return result;
    }

    Sequence<T> *Skip(size_t skipCount) const {
        if (skipCount >= GetLength()) {
            return CreateEmpty();
        }

        Sequence<T> *result = new MutableArraySequence<T>();
        auto *enumerator = GetEnumerator();
        size_t skippedCount = 0;

        while (enumerator->MoveNext()) {
            if (skippedCount < skipCount) {
                ++skippedCount;
            } else {
                result = result->Append(enumerator->Current());
            }
        }

        delete enumerator;
        return result;
    }

    Sequence<Sequence<T> *> *Split(const T &delimiter) const {
        Sequence<Sequence<T> *> *result = new MutableArraySequence<Sequence<T> *>();
        Sequence<T> *currentSequence = new MutableArraySequence<T>();

        auto *enumerator = GetEnumerator();
        while (enumerator->MoveNext()) {
            T element = enumerator->Current();

            if (element == delimiter) {
                result = result->Append(currentSequence);
                currentSequence = new MutableArraySequence<T>();
            } else {
                currentSequence = currentSequence->Append(element);
            }
        }
        delete enumerator;

        if (currentSequence->GetLength() != 0) {
            result = result->Append(currentSequence);
        } else {
            delete currentSequence;
        }
        return result;
    }

    Sequence<T> *Splice(size_t startIndex, size_t removeCount, const Sequence<T> &insertSequence) const {
        size_t originalLength = GetLength();
        if (startIndex > originalLength) {
            throw std::out_of_range("Splice: startIndex out of range");
        }

        if (removeCount > originalLength - startIndex) {
            removeCount = originalLength - startIndex;
        }

        Sequence<T> *resultSequence = new MutableArraySequence<T>();
        auto *originalEnumerator = GetEnumerator();
        size_t currentIndex = 0;

        while (originalEnumerator->MoveNext() && currentIndex < startIndex) {
            resultSequence = resultSequence->Append(originalEnumerator->Current());
            ++currentIndex;
        }

        if (insertSequence.GetLength() > 0) {
            auto *insertEnumerator = insertSequence.GetEnumerator();
            while (insertEnumerator->MoveNext()) {
                resultSequence = resultSequence->Append(insertEnumerator->Current());
            }
            delete insertEnumerator;
        }

        while (originalEnumerator->MoveNext() && currentIndex < startIndex + removeCount) {
            ++currentIndex;
        }

        while (originalEnumerator->MoveNext()) {
            resultSequence = resultSequence->Append(originalEnumerator->Current());
        }

        delete originalEnumerator;
        return resultSequence;
    }

    template<typename U>
    Sequence<U> *FlatMap(Sequence<U> * (*transformFunc)(T)) const {
        Sequence<U> *resultSequence = new MutableArraySequence<U>();
        auto *enumerator = GetEnumerator();

        while (enumerator->MoveNext()) {
            Sequence<U> *innerSequence = transformFunc(enumerator->Current());

            if (innerSequence != nullptr) {
                auto *innerEnumerator = innerSequence->GetEnumerator();
                while (innerEnumerator->MoveNext()) {
                    resultSequence = resultSequence->Append(innerEnumerator->Current());
                }
                delete innerEnumerator;
                delete innerSequence;
            }
        }

        delete enumerator;
        return resultSequence;
    }

    static Sequence<T> *Range(T startValue, T endValue, T stepValue) {
        if (stepValue == T{}) {
            throw std::invalid_argument("Range: stepValue cannot be zero");
        }

        Sequence<T> *resultSequence = new MutableArraySequence<T>();
        T currentValue = startValue;

        if (stepValue > T{}) {
            while (currentValue < endValue) {
                resultSequence = resultSequence->Append(currentValue);
                currentValue += stepValue;
            }
        } else {
            while (currentValue > endValue) {
                resultSequence = resultSequence->Append(currentValue);
                currentValue += stepValue;
            }
        }

        return resultSequence;
    }

    std::tuple<T, T, double> GetMinMaxAvg(Sequence<T>* seq) {
        if (!seq || seq->GetLength() == 0) {
            throw std::out_of_range("Cannot compute stats on empty sequence");
        }

        auto* enumerator = seq->GetEnumerator();
        enumerator->MoveNext();

        T min_val = enumerator->Current();
        T max_val = enumerator->Current();
        T sum     = enumerator->Current();
        size_t count = 1;

        while (enumerator->MoveNext()) {
            T val = enumerator->Current();
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
            sum += val;
            ++count;
        }

        delete enumerator;

        double avg = sum / static_cast<double>(count);
        return std::make_tuple(min_val, max_val, avg);
    }

    

    virtual ~Sequence() = default;
};

#endif
