#ifndef SECONDLAB_MUTABLELISTSEQUNCE_H
#define SECONDLAB_MUTABLELISTSEQUNCE_H

#include "ListSequence.h"

template<typename T>
class MutableListSequence : public ListSequence<T> {
public:
    MutableListSequence() : ListSequence<T>() {}

    MutableListSequence(const T *items, size_t count) : ListSequence<T>(items, count) {}

    MutableListSequence(const MutableListSequence<T> &other) : ListSequence<T>(other) {}

    MutableListSequence(LinkedList<T> *list, size_t size) : ListSequence<T>(list, size) {}

protected:
    Sequence<T> *Instance() override {
        return this;
    }
};

#endif //SECONDLAB_MUTABLELISTSEQUNCE_H
