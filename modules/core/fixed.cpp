#include "leaf/core/fixed.hpp"

#include "leaf/core/exception.hpp"
#include "leaf/core/format.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <limits>
#include <system_error>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

namespace lf {
	static constexpr u64 sign_magnitude(i64 value) {
			if (value >= 0) {
				return static_cast<u64>(value);
			}
			return static_cast<u64>(-(value + 1)) + 1u;
		}

	static report<i64> signed_from_magnitude(u64 magnitude, bool negative) {
			constexpr u64 max_positive = static_cast<u64>(std::numeric_limits<i64>::max());
			constexpr u64 max_negative = max_positive + 1u;
			if (!negative) {
				if (magnitude > max_positive) {
					return unexpected(error(generic_errc::out_of_range, "fixed value out of range"));
				}
				return static_cast<i64>(magnitude);
			}
			if (magnitude > max_negative) {
				return unexpected(error(generic_errc::out_of_range, "fixed value out of range"));
			}
			if (magnitude == max_negative) {
				return std::numeric_limits<i64>::min();
			}
			return -static_cast<i64>(magnitude);
		}

	static report<i64> checked_scaled_integer(i64 value) {
			if (value > std::numeric_limits<i64>::max() / fixed::scale ||
				value < std::numeric_limits<i64>::min() / fixed::scale) {
				return unexpected(error(generic_errc::out_of_range, "fixed integer value out of range"));
			}
			return value * fixed::scale;
		}

#if defined(__SIZEOF_INT128__)
	static report<i64> scaled_multiply(i64 lhs, i64 rhs) {
			__int128 product = static_cast<__int128>(lhs) * static_cast<__int128>(rhs);
			__int128 scaled = product / static_cast<__int128>(fixed::scale);
			if (scaled > static_cast<__int128>(std::numeric_limits<i64>::max()) ||
				scaled < static_cast<__int128>(std::numeric_limits<i64>::min())) {
				return unexpected(error(generic_errc::arithmetic_error, "fixed multiplication overflow"));
			}
			return static_cast<i64>(scaled);
		}

	static report<i64> scaled_divide(i64 lhs, i64 rhs) {
			if (rhs == 0) {
				return unexpected(error(generic_errc::arithmetic_error, "fixed division by zero"));
			}
			__int128 dividend = static_cast<__int128>(lhs) * static_cast<__int128>(fixed::scale);
			__int128 quotient = dividend / static_cast<__int128>(rhs);
			if (quotient > static_cast<__int128>(std::numeric_limits<i64>::max()) ||
				quotient < static_cast<__int128>(std::numeric_limits<i64>::min())) {
				return unexpected(error(generic_errc::arithmetic_error, "fixed division overflow"));
			}
			return static_cast<i64>(quotient);
		}
#elif defined(_MSC_VER)
	static report<i64> divide_unsigned_128(u64 high, u64 low, u64 divisor, bool negative, string_view overflow_message) {
			if (divisor == 0) {
				return unexpected(error(generic_errc::arithmetic_error, "fixed division by zero"));
			}
			if (high >= divisor) {
				return unexpected(error(generic_errc::arithmetic_error, string(overflow_message)));
			}
			u64 remainder = 0;
			u64 quotient = _udiv128(high, low, divisor, &remainder);
			return signed_from_magnitude(quotient, negative);
		}

	static report<i64> scaled_multiply(i64 lhs, i64 rhs) {
			const bool negative = (lhs < 0) != (rhs < 0);
			const u64 lhs_abs = sign_magnitude(lhs);
			const u64 rhs_abs = sign_magnitude(rhs);
			u64 high = 0;
			u64 low = _umul128(lhs_abs, rhs_abs, &high);
			return divide_unsigned_128(high, low, static_cast<u64>(fixed::scale), negative, "fixed multiplication overflow");
		}

	static report<i64> scaled_divide(i64 lhs, i64 rhs) {
			if (rhs == 0) {
			return unexpected(error(generic_errc::arithmetic_error, "fixed division by zero"));
			}
			const bool negative = (lhs < 0) != (rhs < 0);
			const u64 lhs_abs = sign_magnitude(lhs);
			const u64 rhs_abs = sign_magnitude(rhs);
			u64 high = 0;
			u64 low = _umul128(lhs_abs, static_cast<u64>(fixed::scale), &high);
			return divide_unsigned_128(high, low, rhs_abs, negative, "fixed division overflow");
		}
#else
#error "lf::fixed requires __int128 or MSVC 128-bit integer intrinsics"
#endif

	static report<i64> checked_add_raw(i64 lhs, i64 rhs) {
			if ((rhs > 0 && lhs > std::numeric_limits<i64>::max() - rhs) ||
				(rhs < 0 && lhs < std::numeric_limits<i64>::min() - rhs)) {
				return unexpected(error(generic_errc::arithmetic_error, "fixed addition overflow"));
			}
			return lhs + rhs;
		}

	static bool ascii_digit(char value) {
			return value >= '0' && value <= '9';
		}

	report<fixed> fixed::from_integer(i64 value) {
		report<i64> raw = checked_scaled_integer(value);
		if (!raw) {
			return unexpected(raw.error());
		}
		return fixed::from_raw(*raw);
	}

	report<fixed> fixed::from_ratio(i64 numerator, i64 denominator) {
		if (denominator == 0) {
				return unexpected(error(generic_errc::arithmetic_error, "fixed ratio denominator is zero"));
		}
		report<i64> raw = scaled_divide(numerator, denominator);
		if (!raw) {
			return unexpected(raw.error());
		}
		return fixed::from_raw(*raw);
	}

	report<fixed> fixed::parse(string_view text) {
		size_t begin = 0;
		size_t end = text.size();
		while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
			++begin;
		}
		while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
			--end;
		}
		if (begin == end) {
				return unexpected(error(generic_errc::parse_error, "fixed text is empty"));
		}

		bool negative = false;
		if (text[begin] == '+' || text[begin] == '-') {
			negative = text[begin] == '-';
			++begin;
			if (begin == end) {
				return unexpected(error(generic_errc::parse_error, "fixed text has no digits"));
			}
		}

		u64 whole = 0;
		bool saw_digit = false;
		while (begin < end && ascii_digit(text[begin])) {
			saw_digit = true;
			const u64 digit = static_cast<u64>(text[begin] - '0');
			if (whole > (std::numeric_limits<u64>::max() - digit) / 10u) {
				return unexpected(error(generic_errc::out_of_range, "fixed whole part out of range"));
			}
			whole = whole * 10u + digit;
			++begin;
		}

		u64 fraction = 0;
		u32 fraction_digits = 0;
		if (begin < end && text[begin] == '.') {
			++begin;
			while (begin < end && ascii_digit(text[begin])) {
				if (fraction_digits >= 9) {
				return unexpected(error(generic_errc::parse_error, "fixed supports at most 9 fractional digits"));
				}
				fraction = fraction * 10u + static_cast<u64>(text[begin] - '0');
				++fraction_digits;
				++begin;
				saw_digit = true;
			}
		}

		if (!saw_digit) {
			return unexpected(error(generic_errc::parse_error, "fixed text has no digits"));
		}
		if (begin != end) {
			return unexpected(error(generic_errc::parse_error, lf::format("invalid fixed character '{}'", text[begin])));
		}
		while (fraction_digits < 9) {
			fraction *= 10u;
			++fraction_digits;
		}

		if (whole > std::numeric_limits<u64>::max() / static_cast<u64>(scale)) {
			return unexpected(error(generic_errc::out_of_range, "fixed value out of range"));
		}
		u64 magnitude = whole * static_cast<u64>(scale);
		if (magnitude > std::numeric_limits<u64>::max() - fraction) {
			return unexpected(error(generic_errc::out_of_range, "fixed value out of range"));
		}
		magnitude += fraction;

		report<i64> raw = signed_from_magnitude(magnitude, negative);
		if (!raw) {
			return unexpected(raw.error());
		}
		return fixed::from_raw(*raw);
	}

	string fixed::to_string() const {
		if (raw_value == 0) {
			return "0";
		}
		const bool negative = raw_value < 0;
		u64 magnitude = sign_magnitude(raw_value);
		const u64 whole = magnitude / static_cast<u64>(scale);
		u64 fraction = magnitude % static_cast<u64>(scale);
		string out = negative ? "-" : "";
		out += std::to_string(whole);
		if (fraction == 0) {
			return out;
		}
		string fraction_text = std::to_string(fraction);
		if (fraction_text.size() < 9) {
			fraction_text.insert(fraction_text.begin(), 9 - fraction_text.size(), '0');
		}
		while (!fraction_text.empty() && fraction_text.back() == '0') {
			fraction_text.pop_back();
		}
		out += ".";
		out += fraction_text;
		return out;
	}

	report<fixed> fixed::checked_negated() const {
		if (raw_value == std::numeric_limits<i64>::min()) {
			return unexpected(error(generic_errc::arithmetic_error, "fixed negation overflow"));
		}
		return fixed::from_raw(-raw_value);
	}

	report<fixed> fixed::checked_add(fixed other) const {
		report<i64> raw = checked_add_raw(raw_value, other.raw_value);
		if (!raw) {
			return unexpected(raw.error());
		}
		return fixed::from_raw(*raw);
	}

	report<fixed> fixed::checked_subtract(fixed other) const {
		report<fixed> negated = other.checked_negated();
		if (!negated) {
			return unexpected(negated.error());
		}
		return checked_add(*negated);
	}

	report<fixed> fixed::checked_multiply(fixed other) const {
		report<i64> raw = scaled_multiply(raw_value, other.raw_value);
		if (!raw) {
			return unexpected(raw.error());
		}
		return fixed::from_raw(*raw);
	}

	report<fixed> fixed::checked_divide(fixed other) const {
		report<i64> raw = scaled_divide(raw_value, other.raw_value);
		if (!raw) {
			return unexpected(raw.error());
		}
		return fixed::from_raw(*raw);
	}

	fixed fixed::operator-() const {
		report<fixed> result = checked_negated();
		if (!result) {
			throw runtime_exception(result.error().message);
		}
		return *result;
	}

	fixed fixed::operator+(fixed other) const {
		report<fixed> result = checked_add(other);
		if (!result) {
			throw runtime_exception(result.error().message);
		}
		return *result;
	}

	fixed fixed::operator-(fixed other) const {
		report<fixed> result = checked_subtract(other);
		if (!result) {
			throw runtime_exception(result.error().message);
		}
		return *result;
	}

	fixed fixed::operator*(fixed other) const {
		report<fixed> result = checked_multiply(other);
		if (!result) {
			throw runtime_exception(result.error().message);
		}
		return *result;
	}

	fixed fixed::operator/(fixed other) const {
		report<fixed> result = checked_divide(other);
		if (!result) {
			throw runtime_exception(result.error().message);
		}
		return *result;
	}
} // namespace lf
