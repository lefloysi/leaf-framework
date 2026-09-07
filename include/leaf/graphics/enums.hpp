#pragma once

#include <rutile.h>

namespace rt {
	enum class memory_type : u32 {
		host = RT_HOST_MEMORY,
		device = RT_DEVICE_MEMORY,
	};

	enum class clear_flag : u32 {
		none = RT_CLEAR_NONE,
		color = RT_CLEAR_COLOR,
		depth = RT_CLEAR_DEPTH,
		stencil = RT_CLEAR_STENCIL,
	};

	enum class stage_flag : u32 {
		none = RT_STAGE_NONE,
		transfer = RT_STAGE_TRANSFER,
		vertex = RT_STAGE_VERTEX,
		fragment = RT_STAGE_FRAGMENT,
		compute = RT_STAGE_COMPUTE,
		color_attachment = RT_STAGE_COLOR_ATTACHMENT,
		depth_stencil_attachment = RT_STAGE_DEPTH_STENCIL_ATTACHMENT,
		all = RT_STAGE_ALL,
	};

	enum class access_type : u32 {
		none = RT_ACCESS_NONE,
		read = RT_ACCESS_READ,
		write = RT_ACCESS_WRITE,
	};

	enum class texture_type : u32 {
		unknown = RT_TEXTURE_UNKNOWN,
		d1 = RT_TEXTURE_1D,
		d2 = RT_TEXTURE_2D,
		d3 = RT_TEXTURE_3D,
		d1_array = RT_TEXTURE_1D_ARRAY,
		d2_array = RT_TEXTURE_2D_ARRAY,
	};

	enum class texture_aspect_flag : u32 {
		none = RT_TEXTURE_ASPECT_NONE,
		color = RT_TEXTURE_ASPECT_COLOR,
		depth = RT_TEXTURE_ASPECT_DEPTH,
		stencil = RT_TEXTURE_ASPECT_STENCIL,
	};

	enum class filter : u32 {
		nearest = RT_FILTER_NEAREST,
		linear = RT_FILTER_LINEAR,
	};

	enum class mip_filter : u32 {
		none = RT_MIP_FILTER_NONE,
		nearest = RT_MIP_FILTER_NEAREST,
		linear = RT_MIP_FILTER_LINEAR,
	};

	enum class address_mode : u32 {
		clamp = RT_ADDRESS_CLAMP,
		repeat = RT_ADDRESS_REPEAT,
		mirror = RT_ADDRESS_MIRROR,
	};

	enum class queue_capability : u32 {
		transfer = RT_QUEUE_TRANSFER,
		compute = RT_QUEUE_COMPUTE,
		graphics = RT_QUEUE_GRAPHICS,
	};

	enum class index_format : u32 {
		u16 = RT_INDEX_U16,
		u32 = RT_INDEX_U32,
	};

	enum class vertex_rate : u32 {
		vertex = RT_VERTEX_RATE_VERTEX,
		instance = RT_VERTEX_RATE_INSTANCE,
	};

	enum class cull_mode : u32 {
		none = RT_CULL_NONE,
		front = RT_CULL_FRONT,
		back = RT_CULL_BACK,
	};

	enum class front_face : u32 {
		ccw = RT_FRONT_FACE_CCW,
		cw = RT_FRONT_FACE_CW,
	};

	enum class fill_mode : u32 {
		solid = RT_FILL_SOLID,
		wireframe = RT_FILL_WIREFRAME,
	};

	enum class blend_factor : u32 {
		zero = RT_BLEND_ZERO,
		one = RT_BLEND_ONE,
		src_color = RT_BLEND_SRC_COLOR,
		one_minus_src_color = RT_BLEND_ONE_MINUS_SRC_COLOR,
		dst_color = RT_BLEND_DST_COLOR,
		one_minus_dst_color = RT_BLEND_ONE_MINUS_DST_COLOR,
		src_alpha = RT_BLEND_SRC_ALPHA,
		one_minus_src_alpha = RT_BLEND_ONE_MINUS_SRC_ALPHA,
		dst_alpha = RT_BLEND_DST_ALPHA,
		one_minus_dst_alpha = RT_BLEND_ONE_MINUS_DST_ALPHA,
	};

	enum class blend_op : u32 {
		add = RT_BLEND_OP_ADD,
		subtract = RT_BLEND_OP_SUBTRACT,
		reverse_subtract = RT_BLEND_OP_REVERSE_SUBTRACT,
		min = RT_BLEND_OP_MIN,
		max = RT_BLEND_OP_MAX,
	};

	enum class format : u32 {
		unknown = RT_FORMAT_UNKNOWN,
		r8_unorm = RT_R8_UNORM,
		rg8_unorm = RT_RG8_UNORM,
		rgb8_unorm = RT_RGB8_UNORM,
		rgba8_unorm = RT_RGBA8_UNORM,
		r16_unorm = RT_R16_UNORM,
		rg16_unorm = RT_RG16_UNORM,
		rgb16_unorm = RT_RGB16_UNORM,
		rgba16_unorm = RT_RGBA16_UNORM,
		r16_sfloat = RT_R16_SFLOAT,
		rg16_sfloat = RT_RG16_SFLOAT,
		rgb16_sfloat = RT_RGB16_SFLOAT,
		rgba16_sfloat = RT_RGBA16_SFLOAT,
		r32_sfloat = RT_R32_SFLOAT,
		rg32_sfloat = RT_RG32_SFLOAT,
		rgb32_sfloat = RT_RGB32_SFLOAT,
		rgba32_sfloat = RT_RGBA32_SFLOAT,
		r8_sint = RT_R8_SINT,
		rg8_sint = RT_RG8_SINT,
		rgb8_sint = RT_RGB8_SINT,
		rgba8_sint = RT_RGBA8_SINT,
		r16_sint = RT_R16_SINT,
		rg16_sint = RT_RG16_SINT,
		rgb16_sint = RT_RGB16_SINT,
		rgba16_sint = RT_RGBA16_SINT,
		r32_sint = RT_R32_SINT,
		rg32_sint = RT_RG32_SINT,
		rgb32_sint = RT_RGB32_SINT,
		rgba32_sint = RT_RGBA32_SINT,
		r8_uint = RT_R8_UINT,
		rg8_uint = RT_RG8_UINT,
		rgb8_uint = RT_RGB8_UINT,
		rgba8_uint = RT_RGBA8_UINT,
		r16_uint = RT_R16_UINT,
		rg16_uint = RT_RG16_UINT,
		rgb16_uint = RT_RGB16_UINT,
		rgba16_uint = RT_RGBA16_UINT,
		r32_uint = RT_R32_UINT,
		rg32_uint = RT_RG32_UINT,
		rgb32_uint = RT_RGB32_UINT,
		rgba32_uint = RT_RGBA32_UINT,
		d16_unorm = RT_D16_UNORM,
		d32_sfloat = RT_D32_SFLOAT,
		s8_uint = RT_S8_UINT,
		d24_unorm_s8_uint = RT_D24_UNORM_S8_UINT,
		d32_sfloat_s8_uint = RT_D32_SFLOAT_S8_UINT,
	};

	constexpr clear_flag operator|(clear_flag left, clear_flag right) {
		return static_cast<clear_flag>(static_cast<u32>(left) | static_cast<u32>(right));
	}

	constexpr clear_flag operator&(clear_flag left, clear_flag right) {
		return static_cast<clear_flag>(static_cast<u32>(left) & static_cast<u32>(right));
	}

	constexpr stage_flag operator|(stage_flag left, stage_flag right) {
		return static_cast<stage_flag>(static_cast<u32>(left) | static_cast<u32>(right));
	}

	constexpr stage_flag operator&(stage_flag left, stage_flag right) {
		return static_cast<stage_flag>(static_cast<u32>(left) & static_cast<u32>(right));
	}

	constexpr texture_aspect_flag operator|(texture_aspect_flag left, texture_aspect_flag right) {
		return static_cast<texture_aspect_flag>(static_cast<u32>(left) | static_cast<u32>(right));
	}

	constexpr texture_aspect_flag operator&(texture_aspect_flag left, texture_aspect_flag right) {
		return static_cast<texture_aspect_flag>(static_cast<u32>(left) & static_cast<u32>(right));
	}

} // namespace rt
