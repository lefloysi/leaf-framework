#pragma once

#include "leaf/core/time.hpp"

namespace lf {
	struct Clock {
		frequency frequency;
		bool paused = false;
	};
} // namespace lf
