#include "leaf/graphics/framebuffer.hpp"

namespace rt {
	handle<framebuffer> Framebuffer::Create() {
		rt_framebuffer framebuffer = rtFramebufferCreate();
		detail::check_rutile_error("failed to create framebuffer");
		return { framebuffer };
	}

	void Framebuffer::Destroy(handle<framebuffer> framebuffer) {
		rtFramebufferDestroy(framebuffer);
	}

	view<texture_view> Framebuffer::ColorView(view<framebuffer> framebuffer, location location) {
		rt_texture_view value = rtFramebufferColorView(framebuffer, location);
		detail::check_rutile_error("failed to get framebuffer color view");
		view<texture_view> result;
		result.value = value;
		return result;
	}

	void Framebuffer::SetColorView(view<framebuffer> framebuffer, location location, view<texture_view> attachment) {
		rtFramebufferSetColorView(framebuffer, attachment, location);
		detail::check_rutile_error("failed to set framebuffer color view");
	}

	void Framebuffer::SetDepthView(view<framebuffer> framebuffer, view<texture_view> attachment) {
		rtFramebufferSetDepthView(framebuffer, attachment);
		detail::check_rutile_error("failed to set framebuffer depth view");
	}
} // namespace rt
