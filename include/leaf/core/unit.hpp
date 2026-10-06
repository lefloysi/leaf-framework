#pragma once
#include "leaf/core/types.hpp"

#include <chrono>
#include <cmath>
#include <concepts>

namespace lf {
	template<typename Tag, typename Rep = i64>
	struct unit {
		using tag_type = Tag;
		using rep_type = Rep;
		static constexpr bool fixed_representation = requires(i64 raw) {
			{ rep_type::from_raw(raw) } -> std::same_as<rep_type>;
			rep_type::scale;
		};

		constexpr unit() = default;
		constexpr explicit unit(rep_type quantum_count) : value(quantum_count) {}

		template<std::integral Value>
			requires fixed_representation
		constexpr explicit unit(Value value)
			: value(rep_type::from_raw(static_cast<i64>(value) * rep_type::scale)) {}

		template<std::floating_point Value>
			requires fixed_representation
		constexpr explicit unit(Value value)
			: value(rep_type::from_raw(static_cast<i64>(value * static_cast<Value>(rep_type::scale)))) {}

		static constexpr unit from_quantum(rep_type quantum_count) {
			return unit{ quantum_count };
		}

		static constexpr unit from_raw(i64 value)
			requires fixed_representation
		{
			return unit{ rep_type::from_raw(value) };
		}

		template<typename Value, typename Period>
		static constexpr unit from_chrono(std::chrono::duration<Value, Period> value)
			requires std::same_as<rep_type, i64>
		{
			if constexpr (std::floating_point<Value>) {
				return from_quantum(static_cast<i64>(std::llround(std::chrono::duration<f64, std::nano>(value).count())));
			}
			return from_quantum(std::chrono::duration_cast<std::chrono::nanoseconds>(value).count());
		}

		constexpr f64 as_f64() const
			requires fixed_representation
		{
			return static_cast<f64>(quantum_count().raw()) / static_cast<f64>(rep_type::scale);
		}

		constexpr rep_type quantum_count() const {
			return value;
		}

		template<typename Value, typename Period>
			requires(std::same_as<rep_type, i64> && (std::integral<Value> || std::floating_point<Value>))
		constexpr std::chrono::duration<Value, Period> to_chrono() const {
			return std::chrono::duration_cast<std::chrono::duration<Value, Period>>(
				std::chrono::nanoseconds(quantum_count())
			);
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
