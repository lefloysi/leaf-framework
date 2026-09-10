#include "leaf/manager/texture_atlas.hpp"

#include "leaf/core/cast.hpp"
#include "leaf/core/format.hpp"
#include "leaf/core/logging.hpp"
#include "leaf/core/filesystem.hpp"
#include "leaf/graphics/queue.hpp"
#include "leaf/graphics/command_buffer.hpp"
#include "leaf/graphics/sampler.hpp"
#include "leaf/graphics/texture.hpp"
#include "leaf/graphics/texture_view.hpp"
#include "leaf/graphics/timepoint.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <future>
#include <functional>
#include <limits>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_image.h>
#include <stb_rect_pack.h>

namespace lf {
	constexpr u32 minimum_atlas_extent = 64;

	struct atlas_frame {
		u32 texture_index = 0;
		u32 frame_index = 0;
		string path;
		std::shared_ptr<const vector<byte>> bytes;
		rect<u32> source{};
		stbrp_rect destination{};
	};

	struct atlas_upload_group {
		size_t first = 0;
		size_t count = 0;
	};

	struct packed_atlas_layout {
		u32 width = 0;
		u32 height = 0;
	};

	using stbi_image = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;

	vector<atlas_frame> inspect_frames(span<const atlas_source_frame> sources, texture_atlas_options& options) {
		vector<atlas_frame> frames;
		frames.reserve(sources.size());
		if (options.progress) {
			options.progress->add_total(static_cast<u64>(sources.size()));
		}
		std::unordered_map<string, std::pair<int, int>> extent;
		std::unordered_map<string, std::shared_ptr<const vector<byte>>> source_bytes;
		extent.reserve(sources.size());
		source_bytes.reserve(sources.size());
		for (size_t i = 0; i < sources.size(); ++i) {
			const atlas_source_frame& source = sources[i];
			if (source.path.empty()) {
				if (options.progress) {
					options.progress->advance();
				}
				continue;
			}

			const auto [bytes_it, loaded] = source_bytes.try_emplace(source.path);
			if (loaded) {
				report<fs::path> source_path = fs::path::parse(source.path);
				if (!source_path) {
					log::Warning("[textures] invalid texture frame '{}': {}", source.path, source_path.error().message);
					if (options.progress) {
						options.progress->advance();
					}
					continue;
				}
				report<vector<u08>> bytes = fs::read_all(*source_path);
				if (!bytes) {
					log::Warning("[textures] failed to load texture frame '{}': {}", source.path, bytes.error().message);
					if (options.progress) {
						options.progress->advance();
					}
					continue;
				}
				bytes_it->second = std::make_shared<const vector<byte>>(reinterpret_cast<const byte*>(bytes->data()), reinterpret_cast<const byte*>(bytes->data()) + bytes->size());
			}
			if (!bytes_it->second) {
				if (options.progress) {
					options.progress->advance();
				}
				continue;
			}

			const auto [dimension_it, inserted] = extent.try_emplace(source.path, 0, 0);
			if (inserted) {
				int components = 0;
				if (!stbi_info_from_memory(reinterpret_cast<const stbi_uc*>(bytes_it->second->data()), static_cast<int>(bytes_it->second->size()), &dimension_it->second.first, &dimension_it->second.second, &components)) {
					dimension_it->second = { 0, 0 };
				}
			}
			const int image_width = dimension_it->second.first;
			const int image_height = dimension_it->second.second;
			if (image_width <= 0 || image_height <= 0) {
				log::Warning("[textures] missing texture frame '{}'", source.path);
				if (options.progress) {
					options.progress->advance();
				}
				continue;
			}

			atlas_frame frame{ .texture_index = source.texture_index, .frame_index = source.frame_index, .path = source.path, .bytes = bytes_it->second };
			if (source.rect.dim.width == 0 || source.rect.dim.height == 0) {
				const u32 width = safe_cast<u32>(image_width);
				const u32 height = safe_cast<u32>(image_height);
				const u32 extent = std::max<u32>(options.max_frame_extent, 256u);
				const f32 scale = std::min(1.0f, static_cast<f32>(extent) / static_cast<f32>(std::max(width, height)));
				frame.source = { .pos = {}, .dim = { .width = std::max<u32>(1u, safe_cast<u32>(std::lround(width * scale))), .height = std::max<u32>(1u, safe_cast<u32>(std::lround(height * scale))) } };
				frames.emplace_back(std::move(frame));
			} else if (source.rect.pos.x >= safe_cast<u32>(image_width) || source.rect.pos.y >= safe_cast<u32>(image_height) ||
					   source.rect.dim.width > safe_cast<u32>(image_width) - source.rect.pos.x ||
					   source.rect.dim.height > safe_cast<u32>(image_height) - source.rect.pos.y) {
				log::Warning("[textures] invalid frame rect in '{}'", source.path);
			} else {
				frame.source = source.rect;
				frames.emplace_back(std::move(frame));
			}
			if (options.progress) {
				options.progress->advance();
			}
		}
		return frames;
	}

	const stbi_uc* source_pixel(const stbi_uc* image, u32 image_width, u32 x, u32 y) {
		return image + (safe_cast<size_t>(x) + safe_cast<size_t>(y) * image_width) * 4u;
	}

	stbi_uc* atlas_pixel(vector<stbi_uc>& pixels, u32 atlas_width, u32 x, u32 y) {
		return pixels.data() + (safe_cast<size_t>(x) + safe_cast<size_t>(y) * atlas_width) * 4u;
	}

	void copy_frame_to_atlas(vector<stbi_uc>& atlas_pixels, u32 atlas_width, const atlas_frame& frame, const stbi_uc* image, u32 image_width, u32 padding) {
		const u32 x = safe_cast<u32>(frame.destination.x) + padding;
		const u32 y = safe_cast<u32>(frame.destination.y) + padding;

		for (u32 row = 0; row < frame.source.dim.height; ++row) {
			const stbi_uc* source = source_pixel(image, image_width, frame.source.pos.x, frame.source.pos.y + row);
			std::memcpy(atlas_pixel(atlas_pixels, atlas_width, x, y + row), source, safe_cast<size_t>(frame.source.dim.width) * 4u);
		}

		if (padding == 0) {
			return;
		}

		const u32 left = x - padding;
		const u32 right = x + frame.source.dim.width;
		const u32 top = y - padding;
		const u32 bottom = y + frame.source.dim.height;
		for (u32 row = 0; row < frame.source.dim.height; ++row) {
			const stbi_uc* first = source_pixel(image, image_width, frame.source.pos.x, frame.source.pos.y + row);
			const stbi_uc* last = source_pixel(image, image_width, frame.source.pos.x + frame.source.dim.width - 1u, frame.source.pos.y + row);
			for (u32 pad = 0; pad < padding; ++pad) {
				std::memcpy(atlas_pixel(atlas_pixels, atlas_width, left + pad, y + row), first, 4u);
				std::memcpy(atlas_pixel(atlas_pixels, atlas_width, right + pad, y + row), last, 4u);
			}
		}
		for (u32 column = 0; column < frame.source.dim.width + padding * 2u; ++column) {
			const stbi_uc* top_source = atlas_pixel(atlas_pixels, atlas_width, left + column, y);
			const stbi_uc* bottom_source = atlas_pixel(atlas_pixels, atlas_width, left + column, y + frame.source.dim.height - 1u);
			for (u32 pad = 0; pad < padding; ++pad) {
				std::memcpy(atlas_pixel(atlas_pixels, atlas_width, left + column, top + pad), top_source, 4u);
				std::memcpy(atlas_pixel(atlas_pixels, atlas_width, left + column, bottom + pad), bottom_source, 4u);
			}
		}
	}

	packed_atlas_layout pack_frames(span<atlas_frame> frames, const texture_atlas_options& options) {
		const u32 padding = options.padding;
		u64 area = 0;
		u32 max_width = 1;
		for (const atlas_frame& frame : frames) {
			const u32 padded_width = frame.source.dim.width + padding * 2;
			const u32 padded_height = frame.source.dim.height + padding * 2;
			area += static_cast<u64>(padded_width) * padded_height;
			max_width = std::max(max_width, padded_width);
		}

		u32 width = std::max(std::bit_ceil(std::max(minimum_atlas_extent, static_cast<u32>(std::ceil(std::sqrt(static_cast<f64>(area)))))), std::bit_ceil(max_width));
		u32 height = width;
		while (true) {
			if (width > safe_cast<u32>(std::numeric_limits<int>::max()) || height > safe_cast<u32>(std::numeric_limits<int>::max()) || frames.size() > safe_cast<size_t>(std::numeric_limits<int>::max())) {
				log::Warning("[textures] atlas is too large to pack: {}x{}, {} frames", width, height, frames.size());
				return { .width = minimum_atlas_extent, .height = minimum_atlas_extent };
			}
			auto nodes = std::vector<stbrp_node>(width);
			auto rects = std::vector<stbrp_rect>(frames.size());
			stbrp_context context{};
			stbrp_init_target(&context, safe_cast<int>(width), safe_cast<int>(height), nodes.data(), safe_cast<int>(nodes.size()));
			for (size_t i = 0; i < frames.size(); ++i) {
				const u32 padded_width = frames[i].source.dim.width + padding * 2;
				const u32 padded_height = frames[i].source.dim.height + padding * 2;
				if (padded_width > std::numeric_limits<unsigned short>::max() || padded_height > std::numeric_limits<unsigned short>::max()) {
					log::Warning("[textures] atlas frame is too large to pack: {}x{}", padded_width, padded_height);
					return { .width = minimum_atlas_extent, .height = minimum_atlas_extent };
				}
				rects[i].id = safe_cast<int>(i);
				rects[i].w = safe_cast<unsigned short>(padded_width);
				rects[i].h = safe_cast<unsigned short>(padded_height);
			}
			if (stbrp_pack_rects(&context, rects.data(), safe_cast<int>(rects.size()))) {
				for (const stbrp_rect& rect : rects) {
					frames[safe_cast<size_t>(rect.id)].destination = rect;
				}
				return { .width = width, .height = height };
			}
			width <= height ? width *= 2 : height *= 2;
		}
	}

	packed_atlas_frame packed_frame_from(const atlas_frame& frame, u32 atlas_width, u32 atlas_height, u32 padding) {
		return { .texture_index = frame.texture_index, .frame_index = frame.frame_index, .rect = { .pos = { .x = safe_cast<f32>(frame.destination.x + safe_cast<i32>(padding)) / safe_cast<f32>(atlas_width), .y = safe_cast<f32>(frame.destination.y + safe_cast<i32>(padding)) / safe_cast<f32>(atlas_height) }, .dim = { .width = safe_cast<f32>(frame.source.dim.width) / safe_cast<f32>(atlas_width), .height = safe_cast<f32>(frame.source.dim.height) / safe_cast<f32>(atlas_height) } } };
	}

	texture_atlas build_texture_atlas(rt::view<rt::queue> queue, span<const atlas_source_frame> source_frames, texture_atlas_options setup) {
		texture_atlas_options options{ .padding = setup.padding, .max_frame_extent = setup.max_frame_extent };
		if (setup.progress) {
			setup.progress->add("inspect");
			setup.progress->add("pack");
			setup.progress->add("decode");
			setup.progress->add("upload");
			options.progress.emplace((*setup.progress)());
		}
		const auto build_start = std::chrono::steady_clock::now();
		texture_atlas atlas;
		vector<atlas_frame> frames = inspect_frames(source_frames, options);
		const auto inspect_done = std::chrono::steady_clock::now();
		u32 max_frame_width = 0;
		u32 max_frame_height = 0;
		for (const atlas_frame& frame : frames) {
			max_frame_width = std::max(max_frame_width, frame.source.dim.width);
			max_frame_height = std::max(max_frame_height, frame.source.dim.height);
		}
		log::Info("[textures] atlas inspect: {} valid frames max={}x{} in {:.3f}s", frames.size(), max_frame_width, max_frame_height, std::chrono::duration<f64>(inspect_done - build_start).count());

		if (setup.progress) { options.progress.emplace((*setup.progress)()); }
		packed_atlas_layout layout = pack_frames(frames, options);
		const auto pack_done = std::chrono::steady_clock::now();
		log::Info("[textures] atlas pack: {}x{} in {:.3f}s", layout.width, layout.height, std::chrono::duration<f64>(pack_done - inspect_done).count());

		vector<size_t> upload_order;
		upload_order.reserve(frames.size());
		for (size_t i = 0; i < frames.size(); ++i) {
			upload_order.emplace_back(i);
		}
		std::sort(upload_order.begin(), upload_order.end(), [&](size_t a, size_t b) {
			return frames[a].path < frames[b].path;
		});

		vector<atlas_upload_group> upload_groups;
		for (size_t first = 0; first < upload_order.size();) {
			size_t last = first + 1;
			const atlas_frame& frame = frames[upload_order[first]];
			while (last < upload_order.size()) {
				const atlas_frame& next = frames[upload_order[last]];
				if (next.path != frame.path) {
					break;
				}
				++last;
			}
			upload_groups.emplace_back(atlas_upload_group{ .first = first, .count = last - first });
			first = last;
		}

		if (options.progress) {
			options.progress.emplace((*setup.progress)());
			options.progress->add_total(static_cast<u64>(frames.size()));
		}
		vector<stbi_uc> atlas_pixels;
		atlas_pixels.resize(safe_cast<size_t>(layout.width) * safe_cast<size_t>(layout.height) * 4u, 0);
		struct decoded_group {
			std::vector<stbi_uc> pixels;
			u32 width = 0;
			u32 height = 0;
		};
		const unsigned worker_count = std::max(1u, std::thread::hardware_concurrency());
		auto decode = [](const vector<byte>& bytes, u32 target_width, u32 target_height) {
			decoded_group result;
			int components = 0;
			int source_width = 0;
			int source_height = 0;
			stbi_image image{ stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()), static_cast<int>(bytes.size()), &source_width, &source_height, &components, 4), stbi_image_free };
			if (image && source_width > 0 && source_height > 0) {
				const u32 source_width_u32 = safe_cast<u32>(source_width);
				const u32 source_height_u32 = safe_cast<u32>(source_height);
				result.width = target_width;
				result.height = target_height;
				result.pixels.resize(safe_cast<size_t>(result.width) * result.height * 4u);
				for (u32 y = 0; y < result.height; ++y) {
					const u32 source_y = y * source_height_u32 / result.height;
					for (u32 x = 0; x < result.width; ++x) {
						const u32 source_x = x * source_width_u32 / result.width;
						std::memcpy(result.pixels.data() + (safe_cast<size_t>(y) * result.width + x) * 4u, image.get() + (safe_cast<size_t>(source_y) * source_width_u32 + source_x) * 4u, 4u);
					}
				}
			}
			return result;
		};

		for (size_t batch_start = 0; batch_start < upload_groups.size(); batch_start += worker_count) {
			const size_t batch_end = std::min(upload_groups.size(), batch_start + worker_count);
			auto decoded = std::vector<std::future<decoded_group>>();
			decoded.reserve(batch_end - batch_start);
			for (size_t group_index = batch_start; group_index < batch_end; ++group_index) {
				const atlas_frame& group_frame = frames[upload_order[upload_groups[group_index].first]];
				decoded.emplace_back(std::async(std::launch::async, decode, std::cref(*group_frame.bytes), group_frame.source.dim.width, group_frame.source.dim.height));
			}
			for (size_t group_index = batch_start; group_index < batch_end; ++group_index) {
				const atlas_upload_group& group = upload_groups[group_index];
				const atlas_frame& group_frame = frames[upload_order[group.first]];
				decoded_group decoded_image = decoded[group_index - batch_start].get();
				const u32 image_width = decoded_image.width;
				const u32 image_height = decoded_image.height;
				if (decoded_image.pixels.empty() || image_width <= 0 || image_height <= 0) {
					log::Warning("[textures] failed to load packed texture '{}'", group_frame.path);
					if (options.progress) {
						options.progress->advance(static_cast<u64>(group.count));
					}
					continue;
				}

				for (size_t group_offset = 0; group_offset < group.count; ++group_offset) {
					const atlas_frame& frame = frames[upload_order[group.first + group_offset]];
					copy_frame_to_atlas(atlas_pixels, layout.width, frame, decoded_image.pixels.data(), image_width, options.padding);
					if (options.progress) {
						options.progress->advance();
					}
				}
			}
		}
		const auto decode_done = std::chrono::steady_clock::now();
		log::Info("[textures] atlas decode/copy: {} groups in {:.3f}s", upload_groups.size(), std::chrono::duration<f64>(decode_done - pack_done).count());

		if (setup.progress) { options.progress.emplace((*setup.progress)()); }
		atlas.atlas_texture = rt::unique{ rt::Texture::Create() };
		rt::Texture::Resize(atlas.atlas_texture, rt::texture_type::d2, rt::format::rgba8_unorm, { layout.width, layout.height, 1 });
		rt::unique<rt::command_buffer> upload_commands(rt::CommandBuffer::Create());
		rt::Cmd::Begin(upload_commands);
		rt::Cmd::TextureData(upload_commands, atlas.atlas_texture, { rt::texture_aspect_flag::color, 0, 1, 0, 1, { layout.width, layout.height, 1 }, {} }, atlas_pixels.data());
		rt::Cmd::End(upload_commands);
		rt::Timepoint::Wait(rt::Queue::Submit(queue, upload_commands));
		const auto upload_done = std::chrono::steady_clock::now();
		log::Info("[textures] atlas GPU upload: {:.3f}s", std::chrono::duration<f64>(upload_done - decode_done).count());

		for (const atlas_frame& frame : frames) {
			atlas.frames.emplace_back(packed_frame_from(frame, layout.width, layout.height, options.padding));
		}

		atlas.view = rt::unique(rt::TextureView::CreateFromTexture(atlas.atlas_texture));
		atlas.sampler = rt::unique(rt::Sampler::Create());
		rt::Sampler::SetFilter(atlas.sampler, rt::filter::nearest, rt::filter::nearest, rt::mip_filter::none);
		rt::Sampler::SetAddress(atlas.sampler, rt::address_mode::clamp, rt::address_mode::clamp, rt::address_mode::clamp);
		const auto view_done = std::chrono::steady_clock::now();
		log::Info("[textures] atlas view/finalize: {:.3f}s, total {:.3f}s", std::chrono::duration<f64>(view_done - upload_done).count(), std::chrono::duration<f64>(view_done - build_start).count());
		log::Debug("[textures] atlas {}x{} with {} frames in {} source groups", layout.width, layout.height, frames.size(), upload_groups.size());
		return atlas;
	}
} // namespace lf
