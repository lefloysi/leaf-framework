#ifndef LEAF_GRAPHICS_TEXTURE_VIEW_HPP
#define LEAF_GRAPHICS_TEXTURE_VIEW_HPP

#include <leaf/graphics/resource.hpp>

namespace rt::TextureView {
	handle<texture_view> Create();
	handle<texture_view> CreateFromTexture(view<texture> texture);
	void Destroy(handle<texture_view> texture_view);
	void SetTexture(view<texture_view> texture_view, view<texture> texture);
	rt_extent_3d Extent(view<texture_view> texture_view);
} // namespace rt::TextureView

#endif /* LEAF_GRAPHICS_TEXTURE_VIEW_HPP */
