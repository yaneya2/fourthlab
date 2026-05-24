#ifndef FOURTHLAB_CARDINAL_H
#define FOURTHLAB_CARDINAL_H

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>

class Cardinal {
private:
	enum class Kind {
		Finite,
		Omega
	};

	Kind kind_;
	std::size_t value_;

	Cardinal(Kind kind, std::size_t value) : kind_(kind), value_(value) {
	}

public:
	Cardinal() : kind_(Kind::Finite), value_(0) {
	}

	static Cardinal Finite(std::size_t value) {
		return Cardinal(Kind::Finite, value);
	}

	static Cardinal Omega() {
		return Cardinal(Kind::Omega, 0);
	}

	bool IsFinite() const {
		return kind_ == Kind::Finite;
	}

	bool IsOmega() const {
		return kind_ == Kind::Omega;
	}

	bool IsInfinite() const {
		return IsOmega();
	}

	std::size_t Value() const {
		if (IsOmega()) {
			throw std::logic_error("Omega has no finite value");
		}
		return value_;
	}

	bool ContainsIndex(std::size_t index) const {
		return IsOmega() || index < value_;
	}

	std::string ToString() const {
		if (IsOmega()) {
			return "omega";
		}
		return std::to_string(value_);
	}

	Cardinal operator+(const Cardinal &other) const {
		if (IsOmega() || other.IsOmega()) {
			return Cardinal::Omega();
		}
		if (value_ > std::numeric_limits<std::size_t>::max() - other.value_) {
			throw std::overflow_error("Cardinal finite addition overflow");
		}
		return Cardinal::Finite(value_ + other.value_);
	}

	bool operator==(const Cardinal &other) const {
		if (kind_ != other.kind_) {
			return false;
		}
		return IsOmega() || value_ == other.value_;
	}

	bool operator!=(const Cardinal &other) const {
		return !(*this == other);
	}

	bool operator<(const Cardinal &other) const {
		if (IsOmega()) {
			return false;
		}
		if (other.IsOmega()) {
			return true;
		}
		return value_ < other.value_;
	}

	bool operator<=(const Cardinal &other) const {
		return *this < other || *this == other;
	}

	bool operator>(const Cardinal &other) const {
		return !(*this <= other);
	}

	bool operator>=(const Cardinal &other) const {
		return !(*this < other);
	}
};

#endif
