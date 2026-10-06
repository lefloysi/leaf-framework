#pragma once

#include "leaf/core/array.hpp"
#include "leaf/core/binary.hpp"
#include "leaf/core/fixed.hpp"
#include "leaf/core/format.hpp"
#include "leaf/core/string.hpp"
#include "leaf/core/unit.hpp"

#include <compare>

namespace lf {
	struct object;
	template<typename T>
	struct object_trait;

	using timespan = unit<struct timespan_tag, i64>;

	/*!
	** @brief A wall-clock point expressed as nanoseconds since the Unix epoch.
	**
	** A timepoint is suitable for persisted and filesystem metadata.
	*/
	class timepoint {
	  public:
		constexpr timepoint() = default;

		/*! @brief Creates a timepoint from elapsed time since 1970-01-01 UTC. */
		static constexpr timepoint from_unix_epoch(timespan value) {
			return timepoint(value);
		}

		/*! @brief Gets elapsed time since 1970-01-01 UTC. */
		constexpr timespan since_unix_epoch() const {
			return elapsed;
		}

		constexpr bool operator==(const timepoint&) const = default;
		constexpr auto operator<=>(const timepoint&) const = default;

	  private:
		constexpr explicit timepoint(timespan value) : elapsed(value) {}

		timespan elapsed{};
	};

	using hertz = unit<struct hertz_tag, fixed>;

	class frequency {
	  public:
		constexpr frequency() = default;

		constexpr explicit frequency(hertz value)
			: value(value) {}

		constexpr hertz in_hertz() const {
			return value;
		}

		timespan period() const;

		constexpr bool operator==(const frequency&) const = default;
		constexpr auto operator<=>(const frequency&) const = default;

	  private:
		hertz value{};
	};

	timespan now();
	void sleep_for(timespan timespan);
	void sleep_until(timespan deadline);

	/*! @brief Gets the current UTC wall-clock time. */
	timepoint wall_now();

	constexpr timepoint operator+(timepoint point, timespan vector) {
		return timepoint::from_unix_epoch(point.since_unix_epoch() + vector);
	}

	constexpr timepoint operator-(timepoint point, timespan vector) {
		return timepoint::from_unix_epoch(point.since_unix_epoch() - vector);
	}

	constexpr timespan operator-(timepoint lhs, timepoint rhs) {
		return lhs.since_unix_epoch() - rhs.since_unix_epoch();
	}

	template<>
	struct pretty_string_trait<timespan> {
		static string to_string(const timespan& value);
		static timespan from_string(string_view str);
	};

	inline constexpr array<std::pair<i64, string_view>, 4> duration_units = {
		{ { 86'400'000'000'000LL, "d" },
		  { 3'600'000'000'000LL, "h" },
		  { 60'000'000'000LL, "m" },
		  { 1'000'000'000LL, "s" } }
	};

	template<>
	struct object_trait<timespan> {
		static timespan parse(const object& value);
	};
} // namespace lf

namespace lf::bin {
	template<byte_stream Stream, data<frequency> frequency>
	error process(Stream& stream, frequency& value) {
		i64 raw_hertz = value.in_hertz().quantum_count().raw();
		if (error error = stream(lf::field("hertz", raw_hertz))) {
			return error;
		}
		if constexpr (readable_byte_stream<Stream>) {
			value = frequency{ hertz::from_raw(raw_hertz) };
		}
		return {};
	}
} // namespace lf::bin
