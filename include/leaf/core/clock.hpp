#pragma once

#include "leaf/core/time.hpp"

namespace lf {
	struct Clock {
		lf::frequency frequency;
		bool paused = false;
	};
} // namespace lf
