#pragma once

#include "leaf/core/concepts.hpp"

namespace lf {
	template<typename T, typename vnum, typename gnum>
	class identifier {
	  public:
		using value_type = T;
		using vnum_t = vnum;
		using gnum_t = gnum;

		static const identifier null;

		constexpr explicit identifier() = default;
		constexpr explicit identifier(vnum_t idx, gnum_t gen) : idx_value(idx), gen_value(gen) {}
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
		constexpr explicit identifier(vnum idx) : idx_value(idx) {}

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
