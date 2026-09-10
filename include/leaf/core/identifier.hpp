#pragma once

#include "leaf/core/concepts.hpp"
#include "leaf/core/types.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace lf {
	template<typename T, typename vnum, typename gnum>
	class identifier {
	  public:
		using value_type = T;
		using vnum_t = vnum;
		using gnum_t = gnum;

		static const identifier null;

		constexpr explicit identifier() = default;
		constexpr explicit identifier(usize index, gnum_t generation) : gen_value(generation) {
			if (std::cmp_greater_equal(index, std::numeric_limits<vnum_t>::max())) {
				throw std::out_of_range("identifier index is out of range");
			}
			idx_value = static_cast<vnum_t>(index + 1);
		}
		constexpr operator usize() const {
			if (!idx_value) {
				throw std::out_of_range("null identifier has no index");
			}
			return idx_value - 1;
		}
		static constexpr identifier from_raw(vnum_t value, gnum_t generation) {
			identifier result;
			result.idx_value = value;
			result.gen_value = generation;
			return result;
		}
		constexpr explicit operator bool() const {
			return idx_value;
		}

		constexpr vnum get() const {
			return idx_value;
		}
		constexpr gnum gen() const {
			return gen_value;
		}
		friend constexpr bool operator==(const identifier& lhs, const identifier& rhs) {
			return lhs.idx_value == rhs.idx_value && lhs.gen_value == rhs.gen_value;
		}
		friend constexpr bool operator!=(const identifier& lhs, const identifier& rhs) {
			return !(lhs == rhs);
		}

	  private:
		vnum idx_value = 0;
		gnum gen_value = 0;
	};

	template<typename T, typename vnum>
	class identifier<T, vnum, void> {
	  public:
		using value_type = T;
		using vnum_t = vnum;
		using gnum_t = void;

		static const identifier null;

		constexpr explicit identifier() = default;
		constexpr explicit identifier(usize index) {
			if (std::cmp_greater_equal(index, std::numeric_limits<vnum_t>::max())) {
				throw std::out_of_range("identifier index is out of range");
			}
			idx_value = static_cast<vnum_t>(index + 1);
		}
		constexpr operator usize() const {
			if (!idx_value) {
				throw std::out_of_range("null identifier has no index");
			}
			return idx_value - 1;
		}
		static constexpr identifier from_raw(vnum_t value) {
			identifier result;
			result.idx_value = value;
			return result;
		}

		constexpr vnum get() const {
			return idx_value;
		}

		constexpr explicit operator bool() const {
			return idx_value;
		}
		friend constexpr bool operator==(const identifier& lhs, const identifier& rhs) {
			return lhs.idx_value == rhs.idx_value;
		}
		friend constexpr bool operator!=(const identifier& lhs, const identifier& rhs) {
			return !(lhs == rhs);
		}

	  private:
		vnum idx_value = 0;
	};
	template<typename T, typename vnum, typename gnum>
	inline constexpr identifier<T, vnum, gnum> identifier<T, vnum, gnum>::null{};

	template<typename T, typename vnum>
	inline constexpr identifier<T, vnum, void> identifier<T, vnum, void>::null{};
} // namespace lf
