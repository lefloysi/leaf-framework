#include "leaf/graphics/command_buffer.hpp"

namespace rt::Cmd {
	handle<command_buffer> Create() {
		rt_command_buffer command_buffer = rtCommandBufferCreate();
		detail::check_rutile_error("failed to create command buffer");
		return { command_buffer };
	}

	void Destroy(handle<command_buffer> command_buffer) {
		rtCommandBufferDestroy(command_buffer);
	}

	void Reset(view<command_buffer> command_buffer) {
		rtCommandBufferReset(command_buffer);
		detail::check_rutile_error("failed to reset command buffer");
	}

	void Begin(view<command_buffer> command_buffer) {
		rtCommandBufferBegin(command_buffer);
		detail::check_rutile_error("failed to begin command buffer");
	}

	void Continue(view<command_buffer> command_buffer) {
		rtCommandBufferContinue(command_buffer);
		detail::check_rutile_error("failed to continue command buffer");
	}

	void ContinueRendering(view<command_buffer> command_buffer) {
		rtCommandBufferContinueRendering(command_buffer);
		detail::check_rutile_error("failed to continue rendering command buffer");
	}

	void BeginRendering(view<command_buffer> command_buffer, view<framebuffer> framebuffer) {
		rtCmdBeginRendering(command_buffer, framebuffer);
		detail::check_rutile_error("failed to begin rendering");
	}

	void ClearColor(view<command_buffer> command_buffer, location location, f32 r, f32 g, f32 b, f32 a) {
		rtCmdClearColor(command_buffer, location, r, g, b, a);
		detail::check_rutile_error("failed to clear color");
	}

	void ClearDepth(view<command_buffer> command_buffer, f32 depth) {
		rtCmdClearDepth(command_buffer, depth);
		detail::check_rutile_error("failed to clear depth");
	}

	void ClearStencil(view<command_buffer> command_buffer, u64 stencil) {
		rtCmdClearStencil(command_buffer, stencil);
		detail::check_rutile_error("failed to clear stencil");
	}

	void Clear(view<command_buffer> command_buffer, clear_flag attachments) {
		rtCmdClear(command_buffer, static_cast<rt_clear_flag>(attachments));
		detail::check_rutile_error("failed to clear attachments");
	}

	void UseProgram(view<command_buffer> command_buffer, view<program> program) {
		rtCmdUseProgram(command_buffer, program);
		detail::check_rutile_error("failed to bind program");
	}

	void SetViewport(view<command_buffer> command_buffer, u64 x, u64 y, u64 width, u64 height, f32 min_depth, f32 max_depth) {
		rtCmdSetViewport(command_buffer, x, y, width, height, min_depth, max_depth);
		detail::check_rutile_error("failed to set viewport");
	}

	void SetScissor(view<command_buffer> command_buffer, u32 x, u32 y, u32 width, u32 height) {
		rtCmdSetScissor(command_buffer, x, y, width, height);
		detail::check_rutile_error("failed to set scissor");
	}

	void UniformData(view<command_buffer> command_buffer, location location, const u08* data, u64 size) {
		rtCmdUniformData(command_buffer, location, data, size);
		detail::check_rutile_error("failed to set uniform data");
	}

	void StorageData(view<command_buffer> command_buffer, location location, const u08* data, u64 size) {
		rtCmdStorageData(command_buffer, location, data, size);
		detail::check_rutile_error("failed to set storage data");
	}

	void BindBuffer(view<command_buffer> command_buffer, location location, view<buffer> buffer, rt_buffer_range range) {
		rtCmdBindBuffer(command_buffer, location, buffer, range);
		detail::check_rutile_error("failed to bind buffer");
	}

	void VertexBuffer(view<command_buffer> command_buffer, location location, view<buffer> buffer, rt_buffer_range range) {
		rtCmdVertexBuffer(command_buffer, location, buffer, range);
		detail::check_rutile_error("failed to bind vertex buffer");
	}

	void IndexBuffer(view<command_buffer> command_buffer, view<buffer> buffer, rt_buffer_range range, index_format format) {
		rtCmdIndexBuffer(command_buffer, buffer, range, static_cast<rt_index_format>(format));
		detail::check_rutile_error("failed to bind index buffer");
	}

	void BindTexture(view<command_buffer> command_buffer, location location, view<texture_view> texture_view) {
		rtCmdBindTexture(command_buffer, location, texture_view);
		detail::check_rutile_error("failed to bind texture");
	}

	void BindSampler(view<command_buffer> command_buffer, location location, view<sampler> sampler) {
		rtCmdBindSampler(command_buffer, location, sampler);
		detail::check_rutile_error("failed to bind sampler");
	}

	void BufferData(view<command_buffer> command_buffer, view<buffer> buffer, rt_buffer_range range, const u08* data) {
		rtCmdBufferData(command_buffer, buffer, range, data);
		detail::check_rutile_error("failed to upload buffer data");
	}

	void BufferBarrier(view<command_buffer> command_buffer, view<buffer> buffer, rt_buffer_range range, access src, access dst) {
		rtCmdBufferBarrier(command_buffer, buffer, range, { static_cast<rt_stage_flag>(src.stage), static_cast<rt_access_type>(src.type) }, { static_cast<rt_stage_flag>(dst.stage), static_cast<rt_access_type>(dst.type) });
		detail::check_rutile_error("failed to transition buffer");
	}

	void TextureData(view<command_buffer> command_buffer, view<texture> texture, texture_range range, const u08* data) {
		rtCmdTextureData(command_buffer, texture, { static_cast<rt_texture_aspect_flag>(range.aspects), range.base_mip, range.mip_count, range.base_layer, range.layer_count, range.extent, range.offset }, data);
		detail::check_rutile_error("failed to upload texture data");
	}

	void TextureBarrier(view<command_buffer> command_buffer, view<texture> texture, texture_range range, access src, access dst) {
		rtCmdTextureBarrier(command_buffer, texture, { static_cast<rt_texture_aspect_flag>(range.aspects), range.base_mip, range.mip_count, range.base_layer, range.layer_count, range.extent, range.offset }, { static_cast<rt_stage_flag>(src.stage), static_cast<rt_access_type>(src.type) }, { static_cast<rt_stage_flag>(dst.stage), static_cast<rt_access_type>(dst.type) });
		detail::check_rutile_error("failed to transition texture");
	}

	void Execute(view<command_buffer> commands, view<command_buffer> executed_commands) {
		rtCmdExecute(commands, executed_commands);
		detail::check_rutile_error("failed to execute command buffer");
	}

	void Draw(view<command_buffer> command_buffer, u32 vertex_count, u32 first_vertex) {
		rtCmdDraw(command_buffer, vertex_count, first_vertex);
		detail::check_rutile_error("failed to draw");
	}

	void DrawIndexed(view<command_buffer> command_buffer, u32 index_count, u32 first_index, i32 vertex_offset) {
		rtCmdDrawIndexed(command_buffer, index_count, first_index, vertex_offset);
		detail::check_rutile_error("failed to record indexed draw");
	}

	void EndRendering(view<command_buffer> command_buffer) {
		rtCmdEndRendering(command_buffer);
		detail::check_rutile_error("failed to end rendering");
	}

	void End(view<command_buffer> command_buffer) {
		rtCommandBufferEnd(command_buffer);
		detail::check_rutile_error("failed to end command buffer");
	}
} // namespace rt::Cmd
