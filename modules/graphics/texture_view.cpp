#include "leaf/graphics/texture_view.hpp"

namespace rt {
	handle<texture_view> TextureView::Create() {
		rt_texture_view texture_view = rtTextureViewCreate();
		detail::check_rutile_error("failed to create texture view");
		return { texture_view };
	}

	handle<texture_view> TextureView::CreateFromTexture(view<texture> texture) {
		auto view_handle = Create();
		rtTextureViewSetTexture(view_handle, texture);
		detail::check_rutile_error("failed to bind texture view");
		return view_handle;
	}

	void TextureView::Destroy(handle<texture_view> texture_view) {
		rtTextureViewDestroy(texture_view);
	}
	void TextureView::SetTexture(view<texture_view> texture_view, view<texture> texture) {
		rtTextureViewSetTexture(texture_view, texture);
		detail::check_rutile_error("failed to set texture view texture");
	}

	rt_extent_3d TextureView::Extent(view<texture_view> texture_view) {
		rt_extent_3d extent = rtTextureViewExtent(texture_view);
		detail::check_rutile_error("failed to query texture view extent");
		return extent;
	}
} // namespace rt
