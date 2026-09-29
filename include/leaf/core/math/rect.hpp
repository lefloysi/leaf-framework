#pragma once

#include "leaf/core/math/dim.hpp"
#include "leaf/core/math/pos.hpp"
#include "leaf/core/schema.hpp"
#include "leaf/core/types.hpp"

namespace lf {
	template<typename T>
	struct rect {
		pos2<T> pos;
		dim2<T> dim;
	};

	template<typename T, lf::version Version>
	struct schema_trait<rect<T>, Version> {
		static auto get(auto& value) {
			return group(
				field("x", value.pos.x),
				field("y", value.pos.y),
				field("width", value.dim.width),
				field("height", value.dim.height)
			);
		}
	};
} // namespace lf
