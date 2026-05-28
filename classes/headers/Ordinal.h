#ifndef FOURTHLAB_ORDINAL_H
#define FOURTHLAB_ORDINAL_H

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>

class Ordinal {
private:
	std::size_t omegaCoefficient_;
	std::size_t finitePart_;

public:
	Ordinal() : omegaCoefficient_(0), finitePart_(0) {
	}

	Ordinal(std::size_t omegaCoefficient, std::size_t finitePart)
		: omegaCoefficient_(omegaCoefficient), finitePart_(finitePart) {
	}

	static Ordinal Finite(std::size_t value) {
		return Ordinal(0, value);
	}

	static Ordinal Omega() {
		return Ordinal(1, 0);
	}

	static Ordinal Omega(std::size_t omegaCoefficient) {
		return Ordinal(omegaCoefficient, 0);
	}

	static Ordinal FromParts(std::size_t omegaCoefficient, std::size_t finitePart) {
		return Ordinal(omegaCoefficient, finitePart);
	}

	bool IsFinite() const {
		return omegaCoefficient_ == 0;
	}

	bool IsOmega() const {
		return omegaCoefficient_ == 1 && finitePart_ == 0;
	}

	bool IsInfinite() const {
		return omegaCoefficient_ > 0;
	}

	std::size_t OmegaCoefficient() const {
		return omegaCoefficient_;
	}

	std::size_t FinitePart() const {
		return finitePart_;
	}

	bool ContainsIndex(std::size_t index) const {
		return IsInfinite() || index < finitePart_;
	}

	bool ContainsIndex(const Ordinal &index) const {
		return index < *this;
	}

	std::string ToString() const {
		if (IsFinite()) {
			return std::to_string(finitePart_);
		}
		if (omegaCoefficient_ == 1 && finitePart_ == 0) {
			return "omega";
		}
		std::string result = "omega";
		if (omegaCoefficient_ > 1) {
			result += " * " + std::to_string(omegaCoefficient_);
		}
		if (finitePart_ > 0) {
			result += " + " + std::to_string(finitePart_);
		}
		return result;
	}

	Ordinal operator+(const Ordinal &other) const {
		if (other.omegaCoefficient_ > 0) {
			if (omegaCoefficient_ > std::numeric_limits<std::size_t>::max() - other.omegaCoefficient_) {
				throw std::overflow_error("Ordinal omega coefficient addition overflow");
			}
			return Ordinal(omegaCoefficient_ + other.omegaCoefficient_, other.finitePart_);
		}
		if (finitePart_ > std::numeric_limits<std::size_t>::max() - other.finitePart_) {
			throw std::overflow_error("Ordinal finite part addition overflow");
		}
		return Ordinal(omegaCoefficient_, finitePart_ + other.finitePart_);
	}

	bool operator==(const Ordinal &other) const {
		return omegaCoefficient_ == other.omegaCoefficient_ && finitePart_ == other.finitePart_;
	}

	bool operator!=(const Ordinal &other) const {
		return !(*this == other);
	}

	bool operator<(const Ordinal &other) const {
		if (omegaCoefficient_ != other.omegaCoefficient_) {
			return omegaCoefficient_ < other.omegaCoefficient_;
		}
		return finitePart_ < other.finitePart_;
	}

	bool operator<=(const Ordinal &other) const {
		return *this < other || *this == other;
	}

	bool operator>(const Ordinal &other) const {
		return !(*this <= other);
	}

	bool operator>=(const Ordinal &other) const {
		return !(*this < other);
	}
};

#endif
