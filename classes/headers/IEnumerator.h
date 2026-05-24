#ifndef SECONDLAB_IENUMERATOR_H

template<typename T>
class IEnumerator {
public:
    virtual ~IEnumerator() = default;

    virtual bool MoveNext() = 0;

    virtual T Current() const = 0;

    virtual void Reset() = 0;
};

#define SECONDLAB_IENUMERATOR_H

#endif //SECONDLAB_IENUMERATOR_H
