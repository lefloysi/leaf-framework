#ifndef LEAF_GRAPHICS_COMMAND_BUFFER_HPP
#define LEAF_GRAPHICS_COMMAND_BUFFER_HPP

#include <leaf/graphics/resource.hpp>

namespace rt {
	struct access {
		stage_flag stage;
		access_type type;
	};

	struct texture_range {
		texture_aspect_flag aspects;
		usize base_mip;
		usize mip_count;
		usize base_layer;
		usize layer_count;
		rt_extent_3d extent;
		rt_extent_3d offset;
	};
} // namespace rt

namespace rt::Cmd {
	handle<command_buffer> Create();
	void Destroy(handle<command_buffer> command_buffer);
	void Reset(view<command_buffer> command_buffer);
	void Begin(view<command_buffer> command_buffer);
	void Continue(view<command_buffer> command_buffer);
	void ContinueRendering(view<command_buffer> command_buffer);
	void BeginRendering(view<command_buffer> command_buffer, view<framebuffer> framebuffer);
	void ClearColor(view<command_buffer> command_buffer, location location, f32 r, f32 g, f32 b, f32 a);
	void ClearDepth(view<command_buffer> command_buffer, f32 depth);
	void ClearStencil(view<command_buffer> command_buffer, u64 stencil);
	void Clear(view<command_buffer> command_buffer, clear_flag attachments);
	void UseProgram(view<command_buffer> command_buffer, view<program> program);
	void SetViewport(view<command_buffer> command_buffer, u64 x, u64 y, u64 width, u64 height, f32 min_depth, f32 max_depth);
	void SetScissor(view<command_buffer> command_buffer, u32 x, u32 y, u32 width, u32 height);
	void UniformData(view<command_buffer> command_buffer, location location, const u08* data, u64 size);
	void StorageData(view<command_buffer> command_buffer, location location, const u08* data, u64 size);
	void BindBuffer(view<command_buffer> command_buffer, location location, view<buffer> buffer, rt_buffer_range range);
	void VertexBuffer(view<command_buffer> command_buffer, location location, view<buffer> buffer, rt_buffer_range range);
	void IndexBuffer(view<command_buffer> command_buffer, view<buffer> buffer, rt_buffer_range range, index_format format);
	void BindTexture(view<command_buffer> command_buffer, location location, view<texture_view> texture_view);
	void BindSampler(view<command_buffer> command_buffer, location location, view<sampler> sampler);
	void BufferData(view<command_buffer> command_buffer, view<buffer> buffer, rt_buffer_range range, const u08* data);
	void BufferBarrier(view<command_buffer> command_buffer, view<buffer> buffer, rt_buffer_range range, access src, access dst);
	void TextureData(view<command_buffer> command_buffer, view<texture> texture, texture_range range, const u08* data);
	void TextureBarrier(view<command_buffer> command_buffer, view<texture> texture, texture_range range, access src, access dst);
	void Execute(view<command_buffer> commands, view<command_buffer> executed_commands);
	void Draw(view<command_buffer> command_buffer, u32 vertex_count, u32 first_vertex);
	void DrawIndexed(view<command_buffer> command_buffer, u32 index_count, u32 first_index, i32 vertex_offset);
	void EndRendering(view<command_buffer> command_buffer);
	void End(view<command_buffer> command_buffer);
} // namespace rt::Cmd

#endif /* LEAF_GRAPHICS_COMMAND_BUFFER_HPP */
