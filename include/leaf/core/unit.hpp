#pragma once
#include "leaf/core/types.hpp"

#include <concepts>

namespace lf {
	template<typename Tag, typename Rep = i64>
	struct unit {
		using tag_type = Tag;
		using rep_type = Rep;

		constexpr unit() = default;
		constexpr explicit unit(rep_type quantum_count) : value(quantum_count) {}

		static constexpr unit from_quantum(rep_type quantum_count) {
			return unit{ quantum_count };
		}

		constexpr rep_type quantum_count() const {
			return value;
		}

		constexpr unit operator+() const {
			return *this;
		}

		constexpr unit operator-() const {
			return unit{ -value };
		}

		constexpr unit& operator+=(unit other) {
			value += other.value;
			return *this;
		}

		constexpr unit& operator-=(unit other) {
			value -= other.value;
			return *this;
		}

		constexpr unit& operator*=(rep_type scalar) {
			value *= scalar;
			return *this;
		}

		constexpr unit& operator/=(rep_type scalar) {
			value /= scalar;
			return *this;
		}

		constexpr unit operator+(unit other) const {
			unit result = *this;
			result += other;
			return result;
		}

		constexpr unit operator-(unit other) const {
			unit result = *this;
			result -= other;
			return result;
		}

		constexpr unit operator*(rep_type scalar) const {
			unit result = *this;
			result *= scalar;
			return result;
		}

		constexpr unit operator/(rep_type scalar) const {
			unit result = *this;
			result /= scalar;
			return result;
		}

		constexpr bool operator==(unit other) const {
			return value == other.value;
		}

		constexpr bool operator!=(unit other) const {
			return !(*this == other);
		}

		constexpr bool operator<(unit other) const {
			return value < other.value;
		}

		constexpr bool operator<=(unit other) const {
			return value <= other.value;
		}

		constexpr bool operator>(unit other) const {
			return value > other.value;
		}

		constexpr bool operator>=(unit other) const {
			return value >= other.value;
		}

		rep_type value;
	};

	template<typename Tag, typename Rep, std::integral Scalar>
	constexpr unit<Tag, Rep> operator*(Scalar scalar, unit<Tag, Rep> rhs) {
		rhs *= static_cast<Rep>(scalar);
		return rhs;
	}

	template<typename T>
	struct unit_trait;
} // namespace lf
