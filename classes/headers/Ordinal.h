#ifndef FOURTHLAB_ORDINAL_H
#define FOURTHLAB_ORDINAL_H

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>

class Ordinal {
private:
	std::size_t omegaCoefficient;
	std::size_t finitePart;

public:
	Ordinal() : omegaCoefficient(0), finitePart(0) {
	}

	Ordinal(std::size_t omegaCoefficient, std::size_t finitePart)
		: omegaCoefficient(omegaCoefficient), finitePart(finitePart) {
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
		return omegaCoefficient == 0;
	}

	bool IsOmega() const {
		return omegaCoefficient == 1 && finitePart == 0;
	}

	bool IsInfinite() const {
		return omegaCoefficient > 0;
	}

	std::size_t OmegaCoefficient() const {
		return omegaCoefficient;
	}

	std::size_t FinitePart() const {
		return finitePart;
	}


	std::string ToString() const {
		if (IsFinite()) {
			return std::to_string(finitePart);
		}
		if (omegaCoefficient == 1 && finitePart == 0) {
			return "omega";
		}
		std::string result = "omega";
		if (omegaCoefficient > 1) {
			result += " * " + std::to_string(omegaCoefficient);
		}
		if (finitePart > 0) {
			result += " + " + std::to_string(finitePart);
		}
		return result;
	}

	Ordinal operator+(const Ordinal &other) const {
		if (other.omegaCoefficient > 0) {
			if (omegaCoefficient > std::numeric_limits<std::size_t>::max() - other.omegaCoefficient) {
				throw std::overflow_error("Ordinal omega coefficient addition overflow");
			}
			return Ordinal(omegaCoefficient + other.omegaCoefficient, other.finitePart);
		}
		if (finitePart > std::numeric_limits<std::size_t>::max() - other.finitePart) {
			throw std::overflow_error("Ordinal finite part addition overflow");
		}
		return Ordinal(omegaCoefficient, finitePart + other.finitePart);
	}

	bool operator==(const Ordinal &other) const {
		return omegaCoefficient == other.omegaCoefficient && finitePart == other.finitePart;
	}

	bool operator!=(const Ordinal &other) const {
		return !(*this == other);
	}

	bool operator<(const Ordinal &other) const {
		if (omegaCoefficient != other.omegaCoefficient) {
			return omegaCoefficient < other.omegaCoefficient;
		}
		return finitePart < other.finitePart;
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
