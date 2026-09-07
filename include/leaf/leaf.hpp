#pragma once

#include "leaf/core/error.hpp"
#include "leaf/core/progress.hpp"
#include "leaf/core/span.hpp"
#include "leaf/core/string.hpp"
#include <leaf/graphics/resource.hpp>
#include <leaf/manager/texture_atlas.hpp>
#include <leaf/application/window.hpp>

namespace lf {
	error Init(span<string_view> args);
	bool Update();
	void Exit();
}
