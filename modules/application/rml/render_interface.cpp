#include "application/rml/render_interface.hpp"

#include "leaf/core/array.hpp"
#include "leaf/core/filesystem.hpp"
#include "leaf/core/format.hpp"
#include "leaf/core/logging.hpp"
#include "leaf/graphics/buffer.hpp"
#include "leaf/graphics/command_buffer.hpp"
#include "leaf/graphics/graphics_program.hpp"
#include "leaf/graphics/queue.hpp"
#include "leaf/graphics/sampler.hpp"
#include "leaf/graphics/texture.hpp"
#include "leaf/graphics/texture_view.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cstddef>
#include <memory>

extern "C" const rt::program_bytes leaf_application_shader;

namespace lf {

	Renderer::Renderer() {
		draw_commands = rt::unique(rt::Cmd::Create());
		program = rt::unique(rt::Program::Create());
		rt::Program::Source(program, "rml", leaf_application_shader);
		const rt::vertex_layout layout = rt::vertex_layout::Make({ &vertex_input, 1 });
		rt::Program::VertexLayout(program, layout);
		rt::Program::RasterState(program, rt::cull_mode::none, rt::front_face::ccw, rt::fill_mode::solid);
		rt::Program::BlendState(program, true, rt::blend_factor::one, rt::blend_factor::one_minus_src_alpha, rt::blend_op::add, rt::blend_factor::one, rt::blend_factor::one_minus_src_alpha, rt::blend_op::add);
		rt::Program::Finalize(program);
		vertex_location = rt::Program::InputLocation(program, vertex_input.attributes);
		uniform_location = rt::Program::UniformLocation(program, "ui_draw");
		texture_location = rt::Program::UniformLocation(program, "UiTexture");
		const array<u08, 4> white_pixel{ 255, 255, 255, 255 };
		textures.emplace_back(create_texture_data(1, 1));
		white_texture = textures.back().get();
		upload_texture(*white_texture, white_pixel.data());
	}
	void Renderer::begin(rt::view<rt::command_buffer> upload_commands, dim2<u32> viewport_size) {
		released_textures.clear();
		frame_upload_commands = upload_commands;
		current_framebuffer_size = viewport_size;
		batch_vertex_buffer_index = 0;
		bound_texture = nullptr;
		queued_geometry.clear();
		rt::Cmd::Reset(draw_commands);
		rt::Cmd::ContinueRendering(draw_commands);
		upload_textures();
		current_command_buffer = draw_commands;
	}
	void Renderer::end() {
		flush_queued_geometry();
		rt::Cmd::End(draw_commands);
		frame_upload_commands = {};
		current_command_buffer = {};
	}
	rt::view<rt::command_buffer> Renderer::commands() const {
		return draw_commands;
	}
	Rml::CompiledGeometryHandle Renderer::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
		auto geometry = make_unique<Geometry>();
		vector<UiVertex> flattened;
		flattened.reserve(indices.size());
		for (int index : indices) {
			const Rml::Vertex& source = vertices[static_cast<size_t>(index)];
			UiVertex vertex;
			vertex.position = { source.position.x, source.position.y };
			vertex.uv = { source.tex_coord.x, source.tex_coord.y };
			vertex.color[0] = static_cast<f32>(source.colour.red) / 255.0f;
			vertex.color[1] = static_cast<f32>(source.colour.green) / 255.0f;
			vertex.color[2] = static_cast<f32>(source.colour.blue) / 255.0f;
			vertex.color[3] = static_cast<f32>(source.colour.alpha) / 255.0f;
			flattened.push_back(vertex);
		}
		geometry->vertices = std::move(flattened);
		geometry->vertex_count = static_cast<u32>(geometry->vertices.size());
		return reinterpret_cast<Rml::CompiledGeometryHandle>(geometry.release());
	}
	void Renderer::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) {
		draw_geometry(reinterpret_cast<Geometry*>(geometry), { translation.x, translation.y }, texture);
	}
	void Renderer::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
		flush_queued_geometry();
		delete reinterpret_cast<Geometry*>(geometry);
	}
	Rml::TextureHandle Renderer::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
		report<fs::path> source_path = fs::path::parse(source);
		if (!source_path) {
			log::Warning("{}", lf::format("[rml] invalid texture '{}': {}", source, source_path.error().message));
			return 0;
		}
		report<vector<u08>> image = fs::read_all(*source_path);
		if (!image) {
			log::Warning("{}", lf::format("[rml] failed to load texture '{}': {}", source, image.error().message));
			return 0;
		}

		i32 width = 0;
		i32 height = 0;
		auto pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>{ stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(image->data()), static_cast<int>(image->size()), &width, &height, nullptr, 4), stbi_image_free };
		if (!pixels || width <= 0 || height <= 0) {
			return 0;
		}
		texture_dimensions = { width, height };
		return GenerateTexture({ reinterpret_cast<const Rml::byte*>(pixels.get()), static_cast<usize>(width) * height * 4 }, texture_dimensions);
	}
	Rml::TextureHandle Renderer::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) {
		if (source_dimensions.x <= 0 || source_dimensions.y <= 0 || source.empty()) {
			return 0;
		}
		textures.emplace_back(create_texture_data(static_cast<u32>(source_dimensions.x), static_cast<u32>(source_dimensions.y)));
		TextureData* texture_data = textures.back().get();
		upload_texture(*texture_data, source.data());
		return reinterpret_cast<Rml::TextureHandle>(texture_data);
	}
	void Renderer::ReleaseTexture(Rml::TextureHandle texture) {
		TextureData* texture_data = reinterpret_cast<TextureData*>(texture);
		auto owned = std::find_if(textures.begin(), textures.end(), [texture_data](const unique_ptr<TextureData>& candidate) {
			return candidate.get() == texture_data;
		});
		if (owned == textures.end()) {
			return;
		}
		std::erase(texture_uploads, texture_data);
		if (bound_texture == texture_data) {
			bound_texture = nullptr;
		}
		released_textures.emplace_back(std::move(*owned));
		textures.erase(owned);
	}
	void Renderer::EnableScissorRegion(bool enable) {
		scissor_enabled = enable;
	}
	void Renderer::SetScissorRegion(Rml::Rectanglei region) {
		scissor = region;
	}
	unique_ptr<Renderer::TextureData> Renderer::create_texture_data(u32 width, u32 height) {
		unique_ptr<TextureData> texture_data = make_unique<TextureData>();
		texture_data->size = { width, height };
		texture_data->image = rt::unique(rt::Texture::Create());
		rt::Texture::Resize(texture_data->image, rt::texture_type::d2, rt::format::rgba8_unorm, { width, height, 1 });
		texture_data->view = rt::unique(rt::TextureView::CreateFromTexture(texture_data->image));
		texture_data->sampler = rt::unique(rt::Sampler::Create());
		rt::Sampler::SetFilter(texture_data->sampler, rt::filter::linear, rt::filter::linear, rt::mip_filter::none);
		rt::Sampler::SetAddress(texture_data->sampler, rt::address_mode::clamp, rt::address_mode::clamp, rt::address_mode::clamp);
		return texture_data;
	}

	void Renderer::upload_texture(TextureData& texture_data, const void* pixels) {
		const usize byte_count = static_cast<usize>(texture_data.size.width) * texture_data.size.height * 4;
		const u08* source = reinterpret_cast<const u08*>(pixels);
		texture_data.pixels.assign(source, source + byte_count);
		texture_uploads.emplace_back(&texture_data);
	}

	void Renderer::upload_textures() {
		for (TextureData* texture_data : texture_uploads) {
			const rt::texture_range range{ rt::texture_aspect_flag::color, 0, 1, 0, 1, { texture_data->size.width, texture_data->size.height, 1 }, {} };
			rt::Cmd::TextureData(frame_upload_commands, texture_data->image, range, texture_data->pixels.data());
			rt::Cmd::TextureBarrier(frame_upload_commands, texture_data->image, range, { rt::stage_flag::transfer, rt::access_type::write }, { rt::stage_flag::fragment, rt::access_type::read });
			texture_data->pixels.clear();
		}
		texture_uploads.clear();
	}
	void Renderer::draw_geometry(Geometry* geometry, pos2<f32> translation, Rml::TextureHandle texture) {
		if (!geometry || geometry->vertex_count == 0 || !current_command_buffer) {
			return;
		}
		TextureData* texture_data = texture ? reinterpret_cast<TextureData*>(texture) : white_texture;
		const int max_x = static_cast<int>(current_framebuffer_size.width);
		const int max_y = static_cast<int>(current_framebuffer_size.height);
		int left = 0;
		int top = 0;
		int right = max_x;
		int bottom = max_y;
		if (scissor_enabled && scissor.Valid()) {
			left = std::clamp(scissor.Left(), 0, max_x);
			top = std::clamp(scissor.Top(), 0, max_y);
			right = std::clamp(scissor.Right(), left, max_x);
			bottom = std::clamp(scissor.Bottom(), top, max_y);
		}
		queued_geometry.push_back({
			.geometry = geometry,
			.texture = texture_data,
			.translation = translation,
			.scissor_position = { static_cast<u32>(left), static_cast<u32>(top) },
			.scissor_size = { static_cast<u32>(right - left), static_cast<u32>(bottom - top) },
		});
	}
	void Renderer::flush_queued_geometry() {
		if (queued_geometry.empty() || !current_command_buffer) {
			return;
		}
		upload_textures();

		usize first = 0;
		while (first < queued_geometry.size()) {
			const QueuedGeometry& first_item = queued_geometry[first];
			usize last = first + 1;
			while (last < queued_geometry.size()) {
				const QueuedGeometry& item = queued_geometry[last];
				if (item.texture != first_item.texture ||
					item.scissor_position.x != first_item.scissor_position.x ||
					item.scissor_position.y != first_item.scissor_position.y ||
					item.scissor_size.width != first_item.scissor_size.width ||
					item.scissor_size.height != first_item.scissor_size.height) {
					break;
				}
				++last;
			}

			batch_vertices.clear();
			u64 vertex_count = 0;
			for (usize index = first; index < last; ++index) {
				vertex_count += queued_geometry[index].geometry->vertices.size();
			}
			batch_vertices.reserve(static_cast<usize>(vertex_count));
			for (usize index = first; index < last; ++index) {
				const QueuedGeometry& item = queued_geometry[index];
				for (UiVertex vertex : item.geometry->vertices) {
					vertex.position.x += item.translation.x;
					vertex.position.y += item.translation.y;
					batch_vertices.push_back(vertex);
				}
			}
			draw_batch(batch_vertices, first_item.texture, first_item.scissor_position, first_item.scissor_size);
			first = last;
		}
		queued_geometry.clear();
	}
	void Renderer::draw_batch(const vector<UiVertex>& vertices, TextureData* texture_data, pos2<u32> scissor_position, dim2<u32> scissor_size) {
		if (vertices.empty() || !texture_data) {
			return;
		}

		const u64 vertex_bytes = static_cast<u64>(vertices.size() * sizeof(UiVertex));
		if (batch_vertex_buffer_index >= batch_vertex_buffers.size()) {
			batch_vertex_buffers.emplace_back(rt::Buffer::Create());
			batch_vertex_buffer_sizes.push_back(vertex_bytes);
			rt::Buffer::Resize(batch_vertex_buffers.back(), rt::memory_type::device, vertex_bytes);
		} else if (batch_vertex_buffer_sizes[batch_vertex_buffer_index] < vertex_bytes) {
			batch_vertex_buffer_sizes[batch_vertex_buffer_index] = vertex_bytes;
			rt::Buffer::Resize(batch_vertex_buffers[batch_vertex_buffer_index], rt::memory_type::device, vertex_bytes);
		}
		rt::view<rt::buffer> draw_vertices = batch_vertex_buffers[batch_vertex_buffer_index];
		++batch_vertex_buffer_index;

		UiUniform uniform{};
		uniform.viewport_size[0] = static_cast<f32>(std::max(1u, current_framebuffer_size.width));
		uniform.viewport_size[1] = static_cast<f32>(std::max(1u, current_framebuffer_size.height));
		uniform.translation[0] = 0.0f;
		uniform.translation[1] = 0.0f;
		uniform.texture_mode = texture_data == white_texture ? 0.0f : 1.0f;
		rt::Cmd::UseProgram(current_command_buffer, program);

		rt::Cmd::BufferData(frame_upload_commands, draw_vertices, { vertex_bytes, 0 }, reinterpret_cast<const u08*>(vertices.data()));
		rt::Cmd::BufferBarrier(
			frame_upload_commands,
			draw_vertices,
			{ vertex_bytes, 0 },
			{ rt::stage_flag::transfer, rt::access_type::write },
			{ rt::stage_flag::vertex, rt::access_type::read }
		);
		rt::Cmd::SetScissor(current_command_buffer, scissor_position.x, scissor_position.y, scissor_size.width, scissor_size.height);
		rt::Cmd::UniformData(current_command_buffer, uniform_location, reinterpret_cast<const u08*>(&uniform), sizeof(uniform));
		if (bound_texture != texture_data) {
			rt::Cmd::BindTexture(current_command_buffer, texture_location, texture_data->view);
			rt::Cmd::BindSampler(current_command_buffer, texture_location, texture_data->sampler);
			bound_texture = texture_data;
		}
		rt::Cmd::VertexBuffer(current_command_buffer, vertex_location, draw_vertices, { vertex_bytes, 0 });
		rt::Cmd::Draw(current_command_buffer, static_cast<u32>(vertices.size()), 0);
	}
} // namespace lf
