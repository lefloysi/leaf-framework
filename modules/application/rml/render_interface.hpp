#pragma once

#include "leaf/core/array.hpp"
#include "leaf/core/math/dim.hpp"
#include "leaf/core/math/pos.hpp"
#include "leaf/core/memory.hpp"
#include "leaf/core/vector.hpp"
#include "leaf/graphics/graphics_program.hpp"
#include "leaf/graphics/resource.hpp"

#include <RmlUi/Core/RenderInterface.h>

#include <cstddef>

namespace lf {
	struct alignas(16) UiUniform {
		f32 viewport_size[2] = { 1.0f, 1.0f };
		f32 translation[2] = { 0.0f, 0.0f };
		f32 texture_mode = 0.0f;
	};
	static_assert(sizeof(UiUniform) == 32);

	class Renderer final : public Rml::RenderInterface {
	  public:
		Renderer();

		void begin(rt::view<rt::command_buffer> upload_commands, dim2<u32> viewport_size);
		void end();
		rt::view<rt::command_buffer> commands() const;

		Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
		void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
		void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
		Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
		Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
		void ReleaseTexture(Rml::TextureHandle texture) override;
		void EnableScissorRegion(bool enable) override;
		void SetScissorRegion(Rml::Rectanglei region) override;

	  private:
		struct UiVertex {
			pos2<f32> position;
			pos2<f32> uv;
			f32 color[4];
		};

		inline static const rt::vertex_input vertex_input = rt::vertex_input::Make<UiVertex>(
			rt::vertex_attribute::Make("position", &UiVertex::position),
			rt::vertex_attribute::Make("uv", &UiVertex::uv),
			rt::vertex_attribute::Make("color", &UiVertex::color)
		);

		struct Geometry {
			vector<UiVertex> vertices;
			u32 vertex_count = 0;
		};

		struct TextureData {
			rt::unique<rt::texture> image;
			rt::unique<rt::texture_view> view;
			rt::unique<rt::sampler> sampler;
			dim2<u32> size{};
			vector<u08> pixels;
		};

		struct QueuedGeometry {
			Geometry* geometry = nullptr;
			TextureData* texture = nullptr;
			pos2<f32> translation{};
			pos2<u32> scissor_position{};
			dim2<u32> scissor_size{};
		};

		unique_ptr<TextureData> create_texture_data(u32 width, u32 height);
		void upload_texture(TextureData& texture_data, const void* pixels);
		void upload_textures();
		void draw_geometry(Geometry* geometry, pos2<f32> translation, Rml::TextureHandle texture);
		void flush_queued_geometry();
		void draw_batch(const vector<UiVertex>& vertices, TextureData* texture, pos2<u32> scissor_position, dim2<u32> scissor_size);

		rt::unique<rt::command_buffer> draw_commands;
		rt::view<rt::command_buffer> frame_upload_commands;
		rt::view<rt::command_buffer> current_command_buffer;
		dim2<u32> current_framebuffer_size{};

		rt::unique<rt::program> program;
		rt::location vertex_location{};
		rt::location uniform_location{};
		rt::location texture_location{};

		TextureData* white_texture = nullptr;
		vector<unique_ptr<TextureData>> textures;
		vector<unique_ptr<TextureData>> released_textures;
		vector<TextureData*> texture_uploads;
		TextureData* bound_texture = nullptr;

		Rml::Rectanglei scissor{};
		bool scissor_enabled = false;

		vector<QueuedGeometry> queued_geometry;
		vector<UiVertex> batch_vertices;
		usize batch_vertex_buffer_index = 0;
		vector<rt::unique<rt::buffer>> batch_vertex_buffers;
		vector<u64> batch_vertex_buffer_sizes;
	};

} // namespace lf
