#ifndef MUTABLE_ARRAY_SEQUENCE_H
#define MUTABLE_ARRAY_SEQUENCE_H

#include "ArraySequence.h"

template<typename T>
class MutableArraySequence : public ArraySequence<T> {
public:
    MutableArraySequence() : ArraySequence<T>() {}

    MutableArraySequence(const T *items, size_t count) : ArraySequence<T>(items, count) {}

    MutableArraySequence(const MutableArraySequence<T> &other) : ArraySequence<T>(other) {}

    MutableArraySequence(DynamicArray<T> *arr, size_t size) : ArraySequence<T>(arr, size) {}

protected:
    Sequence<T> *Instance() override {
        return this;
    }
};

#endif // MUTABLE_ARRAY_SEQUENCE_H
