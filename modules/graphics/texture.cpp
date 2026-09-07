#include "leaf/graphics/texture.hpp"

namespace rt {
	handle<texture> Texture::Create() {
		rt_texture texture = rtTextureCreate();
		detail::check_rutile_error("failed to create texture");
		return { texture };
	}

	void Texture::Destroy(handle<texture> texture) {
		rtTextureDestroy(texture);
	}
	void Texture::Resize(view<texture> texture, texture_type type, format format, rt_extent_3d extent, u64 mip_count) {
		rtTextureResize(texture, static_cast<rt_texture_type>(type), static_cast<rt_format>(format), extent, mip_count);
		detail::check_rutile_error("failed to resize texture");
	}
} // namespace rt
