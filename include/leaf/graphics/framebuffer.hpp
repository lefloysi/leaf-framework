#ifndef LEAF_GRAPHICS_FRAMEBUFFER_HPP
#define LEAF_GRAPHICS_FRAMEBUFFER_HPP

#include <leaf/graphics/resource.hpp>

namespace rt::Framebuffer {
	handle<framebuffer> Create();
	void Destroy(handle<framebuffer> framebuffer);
	view<texture_view> ColorView(view<framebuffer> framebuffer, location location);
	void SetColorView(view<framebuffer> framebuffer, location location, view<texture_view> attachment);
	void SetDepthView(view<framebuffer> framebuffer, view<texture_view> attachment);
} // namespace rt::Framebuffer

#endif /* LEAF_GRAPHICS_FRAMEBUFFER_HPP */
