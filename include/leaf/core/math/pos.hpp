#pragma once

#include "leaf/core/math/vec.hpp"

namespace lf {
	template<typename T>
	struct pos2 {
		T x{};
		T y{};

		constexpr pos2& operator+=(vec2<T> vector) {
			x += vector.x;
			y += vector.y;
			return *this;
		}

		constexpr pos2 operator+(vec2<T> vector) const {
			pos2 result = *this;
			result += vector;
			return result;
		}

		constexpr pos2& operator-=(vec2<T> vector) {
			x -= vector.x;
			y -= vector.y;
			return *this;
		}

		constexpr pos2 operator-(vec2<T> vector) const {
			pos2 result = *this;
			result -= vector;
			return result;
		}

		constexpr vec2<T> operator-(pos2 other) const {
			return { x - other.x, y - other.y };
		}
	};

	template<typename T>
	constexpr pos2<T> operator+(vec2<T> vector, pos2<T> position) {
		return position + vector;
	}
} // namespace lf
