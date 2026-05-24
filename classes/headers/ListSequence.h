#ifndef SECONDLAB_LISTSEQUENCE_H
#define SECONDLAB_LISTSEQUENCE_H

#include "Sequence.h"
#include "LinkedList.h"

template<typename T>
class MutableListSequence;

template<class T>
class ListSequence : public Sequence<T> {
public:
    ListSequence() : data(new LinkedList<T>()), size(0) {}

    ListSequence(const T *items, size_t count) : data(new LinkedList<T>(items, count)), size(count) {}

    ListSequence(const ListSequence<T> &other) : data(new LinkedList<T>(*(other.data))), size(other.size) {}

    ListSequence<T>& operator=(const ListSequence<T>& other) {
        if (this != &other) {
            delete data;
            data = new LinkedList<T>(*(other.data));
            size = other.size;
        }
        return *this;
    }

    ~ListSequence() override { delete data; }

    T GetFirst() const override {
        if (size == 0) throw std::out_of_range("Sequence is empty");
        return data->GetFirst();
    }

    T GetLast() const override {
        if (size == 0) throw std::out_of_range("Sequence is empty");
        return data->GetLast();
    }

    size_t GetLength() const override {
        return size;
    }

    Sequence<T>* DelImpl(size_t index) override {
        if (size == 0) throw std::out_of_range("Sequence is empty");
        data->Del(index);
        --size;
        return this;
    }

    Sequence<T> *AppendImpl(const T &elem) override {
        data->Append(elem);
        ++size;
        return this;
    }

    Sequence<T> *PrependImpl(const T &elem) override {
        data->Prepend(elem);
        ++size;
        return this;
    }

    Sequence<T> *InsertAtImpl(const T &elem, size_t index) override {
        if (index > size) throw std::out_of_range("Index out of range");
        data->InsertAt(index, elem);
        ++size;
        return this;
    }

    Sequence<T> *ConcatImpl(const Sequence<T>& other) override {
        size_t otherLen = other.GetLength();
        auto enumerator = other.GetEnumerator();
        while (enumerator->MoveNext()) {
            data->Append(enumerator->Current());
        }
        size += otherLen;
        return this;
    }

    Sequence<T> *CreateEmpty() const override {
        return new MutableListSequence<T>();
    }

    Sequence<T> *GetSubsequence(size_t startIndex, size_t endIndex) const override {
        if (endIndex >= this->size || startIndex > endIndex)
            throw std::out_of_range("Invalid subsequence bounds");

        LinkedList<T> *sublist = this->data->GetSubList(startIndex, endIndex);
        return new MutableListSequence<T>(new LinkedList<T>(*sublist), sublist->GetSize());
    }

    IEnumerator<T>* GetEnumerator()const override {
        return data->GetEnumerator();
    }

protected:
    LinkedList<T> *data;
    size_t size;

    ListSequence(LinkedList<T> *list, size_t size) : data(list), size(size) {}
};

#endif //SECONDLAB_LISTSEQUENCE_H
