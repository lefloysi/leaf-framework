#pragma once

#include "leaf/core/error.hpp"
#include "leaf/core/span.hpp"
#include "leaf/core/string.hpp"
#include "leaf/graphics/resource.hpp"

#include <rutile.h>

namespace rt {
	string_view GraphicsBackendName();

	// Initializes presentation extensions too; Vulkan failure falls back to OpenGL.
	error init_graphics(span<string_view> args);
	error init_graphics_extensions();
	bool graphics_available();
	void exit_graphics();
	error rutile_error();
} // namespace rt
