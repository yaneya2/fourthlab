#ifndef FOURTHLAB_GENERATOR_H
#define FOURTHLAB_GENERATOR_H

#include <memory>
#include "Ordinal.h"

template<class T>
class Generator {
public:
	virtual ~Generator() = default;

	[[nodiscard]] virtual Ordinal GetLength() const = 0;

	[[nodiscard]] virtual bool HasNext(Ordinal nextIndex) const = 0;

	virtual T GetNext(Ordinal nextIndex) = 0;

	virtual std::unique_ptr<Generator<T> > Clone() const = 0;
};

#endif
