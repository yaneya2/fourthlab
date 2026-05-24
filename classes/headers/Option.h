#ifndef SECONDLAB_OPTION_H
#define SECONDLAB_OPTION_H

#include <stdexcept>

template<typename T>
class Option {
private:
    bool hasValue;
    T value;

public:
    Option() : hasValue(false) {}

    Option(const T &value) : hasValue(true), value(value) {}

    bool HasValue() const { return hasValue; }

    T GetValue() const {
        if (!hasValue) throw std::logic_error("Option has no value");
        return value;
    }

    T GetValueOrDefault(const T &defaultValue = T()) const {
        return hasValue ? value : defaultValue;
    }
};
#endif //SECONDLAB_OPTION_H
