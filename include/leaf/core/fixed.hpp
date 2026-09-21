#pragma once

#include "leaf/core/error.hpp"
#include "leaf/core/string.hpp"
#include "leaf/core/types.hpp"

#include <compare>

namespace lf {
	struct fixed {
		static constexpr i64 scale = 1'000'000'000;

		constexpr fixed() = default;

		static constexpr fixed from_raw(i64 raw_value) {
			fixed value;
			value.raw_value = raw_value;
			return value;
		}

		static report<fixed> from_integer(i64 value);
		static report<fixed> from_ratio(i64 numerator, i64 denominator);
		static report<fixed> parse(string_view text);

		constexpr i64 raw() const {
			return raw_value;
		}

		string to_string() const;

		constexpr fixed operator+() const {
			return *this;
		}
		report<fixed> checked_negated() const;
		report<fixed> checked_add(fixed other) const;
		report<fixed> checked_subtract(fixed other) const;
		report<fixed> checked_multiply(fixed other) const;
		report<fixed> checked_divide(fixed other) const;
		report<fixed> checked_mul_div(fixed multiplier, fixed divisor) const;
		fixed mul_div(fixed multiplier, fixed divisor) const;
		report<fixed> checked_sqrt() const;
		fixed sqrt() const;

		fixed operator-() const;
		fixed operator+(fixed other) const;
		fixed operator-(fixed other) const;
		fixed operator*(fixed other) const;
		fixed operator/(fixed other) const;

		constexpr bool operator==(fixed other) const {
			return raw_value == other.raw_value;
		}

		constexpr std::strong_ordering operator<=>(fixed other) const {
			return raw_value <=> other.raw_value;
		}

	  private:
		i64 raw_value = 0;
	};
} // namespace lf
