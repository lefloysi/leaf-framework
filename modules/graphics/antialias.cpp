#include <leaf/graphics/antialias.hpp>
#include <leaf/core/profiler.hpp>

namespace rt {
	antialias::antialias()
		: value{ rtAntialiasCreate() } {
		detail::check_rutile_error("failed to create anti-aliasing");
		if (!value) {
			throw runtime_exception("failed to create anti-aliasing");
		}
	}

	antialias::~antialias() {
		rtAntialiasDestroy(value);
	}

	void antialias::begin(view<command_buffer> commands, dim2<u32> extent) {
		LF_PROFILE_SCOPE("frame.antialias-begin");
		rtAntialiasBegin(value, commands, extent.width, extent.height);
		detail::check_rutile_error("failed to begin anti-aliasing");
	}

	void antialias::resolve(view<command_buffer> commands, view<framebuffer> destination) {
		LF_PROFILE_SCOPE("frame.antialias-resolve");
		rtAntialiasResolve(value, commands, destination);
		detail::check_rutile_error("failed to resolve anti-aliasing");
	}
} // namespace rt
