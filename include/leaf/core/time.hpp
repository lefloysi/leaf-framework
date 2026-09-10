#pragma once

#include "leaf/core/array.hpp"
#include "leaf/core/binary.hpp"
#include "leaf/core/fixed.hpp"
#include "leaf/core/format.hpp"
#include "leaf/core/string.hpp"

#include <chrono>
#include <compare>
#include <concepts>
#include <cmath>

namespace lf {
	class timespan {
	  public:
		constexpr timespan() = default;

		static constexpr timespan from_quantum(i64 quantum_count) {
			return timespan(quantum_count);
		}

		constexpr i64 quantum_count() const {
			return value;
		}

		constexpr bool operator==(const timespan&) const = default;
		constexpr auto operator<=>(const timespan&) const = default;

	  private:
		constexpr explicit timespan(i64 quantum_count) : value(quantum_count) {}

		i64 value{};
	};

	class duration {
	  public:
		constexpr duration() = default;

		static constexpr duration from_quantum(i64 quantum_count) {
			return duration(quantum_count);
		}

		template<typename Rep, typename Period>
		static constexpr duration from_chrono(std::chrono::duration<Rep, Period> value) {
			if constexpr (std::floating_point<Rep>) {
				const f64 nanoseconds = std::chrono::duration<f64, std::nano>(value).count();
				return from_quantum(static_cast<i64>(std::llround(nanoseconds)));
			}
			return from_quantum(std::chrono::duration_cast<std::chrono::nanoseconds>(value).count());
		}

		template<typename Rep, typename Period>
		requires(std::integral<Rep> || std::floating_point<Rep>)
		constexpr std::chrono::duration<Rep, Period> to_chrono() const {
			return std::chrono::duration_cast<std::chrono::duration<Rep, Period>>(
				std::chrono::nanoseconds(quantum_count())
			);
		}

		constexpr i64 quantum_count() const {
			return value;
		}

		constexpr duration operator+() const {
			return *this;
		}

		constexpr duration operator-() const {
			return from_quantum(-value);
		}

		constexpr duration& operator+=(duration other) {
			value += other.value;
			return *this;
		}

		constexpr duration& operator-=(duration other) {
			value -= other.value;
			return *this;
		}

		constexpr duration& operator*=(i64 scalar) {
			value *= scalar;
			return *this;
		}

		constexpr duration& operator/=(i64 scalar) {
			value /= scalar;
			return *this;
		}

		constexpr duration operator+(duration other) const {
			duration result = *this;
			result += other;
			return result;
		}

		constexpr duration operator-(duration other) const {
			duration result = *this;
			result -= other;
			return result;
		}

		constexpr duration operator*(i64 scalar) const {
			duration result = *this;
			result *= scalar;
			return result;
		}

		constexpr duration operator/(i64 scalar) const {
			duration result = *this;
			result /= scalar;
			return result;
		}

		constexpr bool operator==(const duration&) const = default;
		constexpr auto operator<=>(const duration&) const = default;

	  private:
		constexpr explicit duration(i64 quantum_count) : value(quantum_count) {}

		i64 value{};
	};

	constexpr duration operator*(i64 scalar, duration vector) {
		return vector * scalar;
	}

	class instant {
	  public:
		constexpr instant() = default;

		static constexpr instant from_quantum(i64 quantum_count) {
			return instant(quantum_count);
		}

		constexpr i64 quantum_count() const {
			return value;
		}

		constexpr bool operator==(const instant&) const = default;
		constexpr auto operator<=>(const instant&) const = default;

	  private:
		constexpr explicit instant(i64 quantum_count) : value(quantum_count) {}

		i64 value{};
	};

	/*!
	** @brief A wall-clock point expressed as nanoseconds since the Unix epoch.
	**
	** Unlike instant, a timepoint is suitable for persisted and filesystem metadata.
	*/
	class timepoint {
	  public:
		constexpr timepoint() = default;

		/*! @brief Creates a timepoint from elapsed time since 1970-01-01 UTC. */
		static constexpr timepoint from_unix_epoch(duration value) {
			return timepoint(value);
		}

		/*! @brief Gets elapsed time since 1970-01-01 UTC. */
		constexpr duration since_unix_epoch() const {
			return elapsed;
		}

		constexpr bool operator==(const timepoint&) const = default;
		constexpr auto operator<=>(const timepoint&) const = default;

	  private:
		constexpr explicit timepoint(duration value) : elapsed(value) {}

		duration elapsed{};
	};

	class frequency {
	  public:
		constexpr frequency() = default;

		static constexpr frequency from_hertz(fixed value) {
			return frequency(value);
		}

		static constexpr frequency from_hertz(i64 value) {
			return from_hertz(fixed::from_raw(value * fixed::scale));
		}

		template<std::floating_point Rep>
		static constexpr frequency from_hertz(Rep value) {
			return from_hertz(fixed::from_raw(static_cast<i64>(value * static_cast<Rep>(fixed::scale))));
		}

		constexpr fixed hertz() const {
			return value;
		}

		constexpr f64 hertz_value() const {
			return static_cast<f64>(value.raw()) / static_cast<f64>(fixed::scale);
		}

		duration period() const;

		constexpr bool operator==(const frequency&) const = default;
		constexpr auto operator<=>(const frequency&) const = default;

	  private:
		constexpr explicit frequency(fixed value) : value(value) {}

		fixed value;
	};

	instant now();
	void sleep_for(duration duration);
	void sleep_until(instant deadline);

	/*! @brief Gets the current UTC wall-clock time. */
	timepoint wall_now();

	constexpr instant operator+(instant point, duration vector) {
		return instant::from_quantum(point.quantum_count() + vector.quantum_count());
	}

	constexpr instant operator-(instant point, duration vector) {
		return instant::from_quantum(point.quantum_count() - vector.quantum_count());
	}

	constexpr duration operator-(instant lhs, instant rhs) {
		return duration::from_quantum(lhs.quantum_count() - rhs.quantum_count());
	}

	constexpr timepoint operator+(timepoint point, duration vector) {
		return timepoint::from_unix_epoch(point.since_unix_epoch() + vector);
	}

	constexpr timepoint operator-(timepoint point, duration vector) {
		return timepoint::from_unix_epoch(point.since_unix_epoch() - vector);
	}

	constexpr duration operator-(timepoint lhs, timepoint rhs) {
		return lhs.since_unix_epoch() - rhs.since_unix_epoch();
	}

	template<>
	struct pretty_string_trait<duration> {
		static string to_string(const duration& value);
		static duration from_string(string_view str);
	};

	inline constexpr array<std::pair<i64, string_view>, 4> duration_units = {
		{ { 86'400'000'000'000LL, "d" },
		  { 3'600'000'000'000LL, "h" },
		  { 60'000'000'000LL, "m" },
		  { 1'000'000'000LL, "s" } }
	};
} // namespace lf

namespace lf::bin {
	template<byte_stream Stream, data<frequency> frequency>
	error process(Stream& stream, frequency& value) {
		i64 hertz = value.hertz().raw();
		if (error error = stream(lf::field("hertz", hertz))) {
			return error;
		}
		if constexpr (readable_byte_stream<Stream>) {
			value = frequency::from_hertz(fixed::from_raw(hertz));
		}
		return {};
	}
} // namespace lf::bin
