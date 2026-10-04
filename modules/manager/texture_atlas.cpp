#include "leaf/manager/texture_atlas.hpp"
#include "leaf/core/profiler.hpp"

#include "leaf/core/cast.hpp"
#include "leaf/core/filesystem.hpp"
#include "leaf/core/format.hpp"
#include "leaf/core/logging.hpp"
#include "leaf/core/thread_pool.hpp"
#include "leaf/graphics/command_buffer.hpp"
#include "leaf/graphics/queue.hpp"
#include "leaf/graphics/sampler.hpp"
#include "leaf/graphics/texture.hpp"
#include "leaf/graphics/texture_view.hpp"
#include "leaf/graphics/timepoint.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <future>
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
	constexpr usize atlas_upload_batch_byte_limit = 8u * 1024u * 1024u;
	constexpr unsigned max_texture_pixel_workers = 4;

	struct atlas_frame {
		u32 texture_index = 0;
		u32 frame_index = 0;
		string path;
		std::shared_ptr<const vector<byte>> bytes;
		rect<u32> source{};
		rect<u32> image_region{};
		stbrp_rect destination{};
	};

	struct atlas_upload_group {
		size_t first = 0;
		size_t count = 0;
	};

	// These are CPU pixels that are ready to hand directly to the atlas.  The
	// offsets avoid a vector per mip while keeping a group's preparation local
	// to one worker.
	struct prepared_atlas_frame {
		size_t frame_index = 0;
		vector<usize> mip_offsets;
		vector<stbi_uc> pixels;
	};

	struct prepared_atlas_group {
		vector<prepared_atlas_frame> frames;
		usize skipped_frames = 0;
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
			frame.image_region = { {}, { safe_cast<u32>(image_width), safe_cast<u32>(image_height) } };
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
				frame.image_region = source.rect;
				const u32 extent = std::max<u32>(options.max_frame_extent, 256u);
				const f32 scale = std::min(1.0f, static_cast<f32>(extent) / static_cast<f32>(std::max(source.rect.dim.width, source.rect.dim.height)));
				frame.source.dim = { std::max<u32>(1u, safe_cast<u32>(std::lround(source.rect.dim.width * scale))), std::max<u32>(1u, safe_cast<u32>(std::lround(source.rect.dim.height * scale))) };
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

	void append_padded_frame_pixels(vector<stbi_uc>& pixels, const atlas_frame& frame, const stbi_uc* image, u32 image_width, u32 padding) {
		LF_PROFILE_SCOPE("atlas.resize-and-pad");
		const u32 width = frame.source.dim.width + padding * 2u;
		const u32 height = frame.source.dim.height + padding * 2u;
		const usize offset = pixels.size();
		pixels.resize(offset + safe_cast<size_t>(width) * height * 4u);
		stbi_uc* output = pixels.data() + offset;
		const bool preserves_width = frame.image_region.dim.width == frame.source.dim.width;
		for (u32 y = 0; y < height; ++y) {
			const u32 content_y = std::clamp(y, padding, padding + frame.source.dim.height - 1u) - padding;
			const u32 source_y = frame.image_region.pos.y + safe_cast<u32>(u64(content_y) * frame.image_region.dim.height / frame.source.dim.height);
			stbi_uc* row = output + safe_cast<size_t>(y) * width * 4u;
			const stbi_uc* source_row = source_pixel(image, image_width, frame.image_region.pos.x, source_y);
			if (preserves_width) {
				std::memcpy(row + safe_cast<size_t>(padding) * 4u, source_row, safe_cast<size_t>(frame.source.dim.width) * 4u);
			} else {
				for (u32 content_x = 0; content_x < frame.source.dim.width; ++content_x) {
					const u32 source_x = safe_cast<u32>(u64(content_x) * frame.image_region.dim.width / frame.source.dim.width);
					std::memcpy(row + safe_cast<size_t>(padding + content_x) * 4u, source_row + safe_cast<size_t>(source_x) * 4u, 4u);
				}
			}
			for (u32 x = 0; x < padding; ++x) {
				std::memcpy(row + safe_cast<size_t>(x) * 4u, row + safe_cast<size_t>(padding) * 4u, 4u);
				std::memcpy(row + safe_cast<size_t>(padding + frame.source.dim.width + x) * 4u, row + safe_cast<size_t>(padding + frame.source.dim.width - 1u) * 4u, 4u);
			}
		}
	}

	void append_mip_pixels(
		vector<stbi_uc>& pixels,
		span<const stbi_uc> base_pixels,
		dim2<u32> base_size,
		pos2<u32> base_position,
		u32 scale,
		dim2<u32> mip_size,
		pos2<u32> mip_position
	) {
		const usize offset = pixels.size();
		pixels.resize(offset + safe_cast<size_t>(mip_size.width) * mip_size.height * 4u);
		const u64 sample_count = safe_cast<u64>(scale) * scale;
		LF_PROFILE_SCOPE("atlas.mip-pixels");
		for (u32 y = 0; y < mip_size.height; ++y) {
			for (u32 x = 0; x < mip_size.width; ++x) {
				const u32 first_x = (mip_position.x + x) * scale;
				const u32 first_y = (mip_position.y + y) * scale;
				u64 sums[4] = {};
				for (u32 source_y = 0; source_y < scale; ++source_y) {
					const u32 local_y = std::clamp(first_y + source_y, base_position.y, base_position.y + base_size.height - 1u) - base_position.y;
					const stbi_uc* row = base_pixels.data() + usize(local_y) * base_size.width * 4u;
					for (u32 source_x = 0; source_x < scale; ++source_x) {
						const u32 local_x = std::clamp(first_x + source_x, base_position.x, base_position.x + base_size.width - 1u) - base_position.x;
						const stbi_uc* pixel = row + usize(local_x) * 4u;
						sums[0] += pixel[0];
						sums[1] += pixel[1];
						sums[2] += pixel[2];
						sums[3] += pixel[3];
					}
				}
				for (u32 channel = 0; channel < 4; ++channel) {
					pixels[offset + (usize(y) * mip_size.width + x) * 4u + channel] = safe_cast<stbi_uc>((sums[channel] + sample_count / 2u) / sample_count);
				}
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

	usize atlas_frame_upload_bytes(
		const packed_atlas_layout& layout,
		const atlas_frame& frame,
		u32 padding,
		u32 levels
	) {
		const pos2<u32> base_position{ safe_cast<u32>(frame.destination.x), safe_cast<u32>(frame.destination.y) };
		const dim2<u32> base_size{ frame.source.dim.width + padding * 2u, frame.source.dim.height + padding * 2u };
		usize bytes = 0;
		for (u32 level = 0; level < levels; ++level) {
			const u32 scale = 1u << level;
			const dim2<u32> atlas_size{ std::max(1u, layout.width >> level), std::max(1u, layout.height >> level) };
			const pos2<u32> mip_position{ base_position.x / scale, base_position.y / scale };
			const u32 end_x = std::min(atlas_size.width, safe_cast<u32>((u64(base_position.x + base_size.width) + scale - 1u) / scale));
			const u32 end_y = std::min(atlas_size.height, safe_cast<u32>((u64(base_position.y + base_size.height) + scale - 1u) / scale));
			bytes += safe_cast<usize>(end_x - mip_position.x) * (end_y - mip_position.y) * 4u;
		}
		return bytes;
	}

	void upload_prepared_frame_to_atlas(
		rt::view<rt::command_buffer> commands,
		rt::view<rt::texture> atlas_texture,
		const packed_atlas_layout& layout,
		const atlas_frame& frame,
		const prepared_atlas_frame& prepared,
		u32 padding,
		u32 levels
	) {
		const pos2<u32> base_position{ safe_cast<u32>(frame.destination.x), safe_cast<u32>(frame.destination.y) };
		const dim2<u32> base_size{ frame.source.dim.width + padding * 2u, frame.source.dim.height + padding * 2u };

		for (u32 level = 0; level < levels; ++level) {
			const u32 scale = 1u << level;
			const dim2<u32> atlas_size{ std::max(1u, layout.width >> level), std::max(1u, layout.height >> level) };
			const pos2<u32> mip_position{ base_position.x / scale, base_position.y / scale };
			const u32 end_x = std::min(atlas_size.width, safe_cast<u32>((u64(base_position.x + base_size.width) + scale - 1u) / scale));
			const u32 end_y = std::min(atlas_size.height, safe_cast<u32>((u64(base_position.y + base_size.height) + scale - 1u) / scale));
			const dim2<u32> mip_size{ end_x - mip_position.x, end_y - mip_position.y };
			const rt::texture_range range{
				rt::texture_aspect_flag::color,
				level,
				1,
				0,
				1,
				{ mip_size.width, mip_size.height, 1 },
				{ mip_position.x, mip_position.y, 0 }
			};
			rt::Cmd::TextureData(commands, atlas_texture, range, prepared.pixels.data() + prepared.mip_offsets[level]);
		}
	}

	struct decoded_group {
		stbi_image pixels{ nullptr, stbi_image_free };
		u32 width = 0;
		u32 height = 0;
	};

	decoded_group decode_group(const vector<byte>& bytes) {
		LF_PROFILE_SCOPE("atlas.decode-image");
		decoded_group result;
		int components = 0;
		int source_width = 0;
		int source_height = 0;
		result.pixels.reset(stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()), static_cast<int>(bytes.size()), &source_width, &source_height, &components, 4));
		if (result.pixels && source_width > 0 && source_height > 0) {
			result.width = safe_cast<u32>(source_width);
			result.height = safe_cast<u32>(source_height);
		}
		return result;
	}

	prepared_atlas_group prepare_atlas_group(
		const vector<atlas_frame>& frames,
		const vector<size_t>& upload_order,
		atlas_upload_group group,
		const packed_atlas_layout& layout,
		u32 padding,
		u32 levels
	) {
		prepared_atlas_group result;
		result.frames.reserve(group.count);
		const atlas_frame& group_frame = frames[upload_order[group.first]];
		decoded_group decoded = decode_group(*group_frame.bytes);
		if (!decoded.pixels || decoded.width == 0 || decoded.height == 0) {
			log::Warning("[textures] failed to load packed texture '{}'", group_frame.path);
			result.skipped_frames = group.count;
			return result;
		}

		for (size_t group_offset = 0; group_offset < group.count; ++group_offset) {
			const size_t frame_index = upload_order[group.first + group_offset];
			const atlas_frame& frame = frames[frame_index];
			if (frame.image_region.pos.x >= decoded.width || frame.image_region.pos.y >= decoded.height ||
				frame.image_region.dim.width > decoded.width - frame.image_region.pos.x ||
				frame.image_region.dim.height > decoded.height - frame.image_region.pos.y) {
				log::Warning("[textures] decoded frame rect exceeds image '{}'", frame.path);
				++result.skipped_frames;
				continue;
			}

			prepared_atlas_frame prepared{ .frame_index = frame_index };
			prepared.mip_offsets.reserve(levels);
			prepared.pixels.reserve(atlas_frame_upload_bytes(layout, frame, padding, levels));
			prepared.mip_offsets.emplace_back(0);
			append_padded_frame_pixels(prepared.pixels, frame, decoded.pixels.get(), decoded.width, padding);

			const pos2<u32> base_position{ safe_cast<u32>(frame.destination.x), safe_cast<u32>(frame.destination.y) };
			const dim2<u32> base_size{ frame.source.dim.width + padding * 2u, frame.source.dim.height + padding * 2u };
			const span<const stbi_uc> base_pixels{ prepared.pixels.data(), safe_cast<usize>(base_size.width) * base_size.height * 4u };
			for (u32 level = 1; level < levels; ++level) {
				const u32 scale = 1u << level;
				const dim2<u32> atlas_size{ std::max(1u, layout.width >> level), std::max(1u, layout.height >> level) };
				const pos2<u32> mip_position{ base_position.x / scale, base_position.y / scale };
				const u32 end_x = std::min(atlas_size.width, safe_cast<u32>((u64(base_position.x + base_size.width) + scale - 1u) / scale));
				const u32 end_y = std::min(atlas_size.height, safe_cast<u32>((u64(base_position.y + base_size.height) + scale - 1u) / scale));
				prepared.mip_offsets.emplace_back(prepared.pixels.size());
				append_mip_pixels(prepared.pixels, base_pixels, base_size, base_position, scale, { end_x - mip_position.x, end_y - mip_position.y }, mip_position);
			}
			result.frames.emplace_back(std::move(prepared));
		}
		return result;
	}

	texture_atlas build_texture_atlas(rt::view<rt::queue> queue, span<const atlas_source_frame> source_frames, texture_atlas_options setup) {
		texture_atlas_options options{ .padding = setup.padding, .max_frame_extent = setup.max_frame_extent, .smooth = setup.smooth, .mip_levels = setup.mip_levels };
		if (setup.progress) {
			// Relative costs measured with the bundled textures; individual files
			// and frames still advance these phases as their work completes.
			setup.progress->add("Reading texture files", 20);
			setup.progress->add("Arranging textures");
			setup.progress->add("Preparing atlas texture pixels", 78);
			setup.progress->add("Uploading texture atlas");
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

		atlas.atlas_texture = rt::unique{ rt::Texture::Create() };
		// Keep at least one padding texel at the coarsest atlas level.
		u32 levels = 1;
		while (levels < options.mip_levels && (1u << levels) <= options.padding && (layout.width >> levels) && (layout.height >> levels)) {
			++levels;
		}
		rt::Texture::Resize(atlas.atlas_texture, rt::texture_type::d2, rt::format::rgba8_unorm, { layout.width, layout.height, 1 }, levels);
		rt::unique<rt::command_buffer> upload_commands(rt::CommandBuffer::Create());
		rt::Cmd::Begin(upload_commands);
		usize upload_batch_bytes = 0;
		usize upload_batch_count = 0;
		usize prepared_pixel_bytes = 0;
		auto submit_upload_batch = [&] {
			if (!upload_batch_bytes) {
				return;
			}
			rt::Cmd::End(upload_commands);
			rt::Timepoint::Wait(rt::Queue::Submit(queue, upload_commands));
			++upload_batch_count;
		};

		if (options.progress) {
			options.progress.emplace((*setup.progress)());
			options.progress->add_total(static_cast<u64>(frames.size()));
		}
		const unsigned worker_count = std::clamp(std::thread::hardware_concurrency(), 1u, max_texture_pixel_workers);
		ThreadPool pixel_workers = ThreadPool(worker_count);
		vector<std::future<prepared_atlas_group>> prepared_groups;
		const auto prepare_group = [&](usize index) {
			return pixel_workers.submit([&frames, &upload_order, layout, padding = options.padding, levels, group = upload_groups[index]] {
				return prepare_atlas_group(frames, upload_order, group, layout, padding, levels);
			});
		};
		const usize pending_count = std::min<usize>(worker_count, upload_groups.size());
		prepared_groups.reserve(pending_count);
		for (usize index = 0; index < pending_count; ++index) {
			prepared_groups.emplace_back(prepare_group(index));
		}
		for (usize index = 0; index < upload_groups.size(); ++index) {
			prepared_atlas_group prepared_group = prepared_groups[index % pending_count].get();
			if (index + pending_count < upload_groups.size()) {
				prepared_groups[index % pending_count] = prepare_group(index + pending_count);
			}
			for (const prepared_atlas_frame& prepared : prepared_group.frames) {
				const atlas_frame& frame = frames[prepared.frame_index];
				const usize frame_upload_bytes = prepared.pixels.size();
				if (upload_batch_bytes && upload_batch_bytes + frame_upload_bytes > atlas_upload_batch_byte_limit) {
					submit_upload_batch();
					rt::Cmd::Reset(upload_commands);
					rt::Cmd::Begin(upload_commands);
					upload_batch_bytes = 0;
				}
				upload_prepared_frame_to_atlas(upload_commands, atlas.atlas_texture, layout, frame, prepared, options.padding, levels);
				upload_batch_bytes += frame_upload_bytes;
				prepared_pixel_bytes += frame_upload_bytes;
				if (options.progress) {
					options.progress->advance();
				}
			}
			if (options.progress && prepared_group.skipped_frames) {
				options.progress->advance(static_cast<u64>(prepared_group.skipped_frames));
			}
		}
		const auto decode_done = std::chrono::steady_clock::now();
		log::Info("[textures] atlas pixel preparation: {} files, {} frames, {:.1f} MiB across {} workers in {:.3f}s", upload_groups.size(), frames.size(), static_cast<f64>(prepared_pixel_bytes) / (1024.0 * 1024.0), worker_count, std::chrono::duration<f64>(decode_done - pack_done).count());

		if (setup.progress) { options.progress.emplace((*setup.progress)()); }
		if (upload_batch_bytes) {
			const rt::texture_range atlas_range{
				rt::texture_aspect_flag::color,
				0,
				levels,
				0,
				1,
				{ layout.width, layout.height, 1 },
				{}
			};
			rt::Cmd::TextureBarrier(upload_commands, atlas.atlas_texture, atlas_range, { rt::stage_flag::transfer, rt::access_type::write }, { rt::stage_flag::fragment, rt::access_type::read });
			submit_upload_batch();
		}
		const auto upload_done = std::chrono::steady_clock::now();
		log::Info("[textures] atlas GPU upload: {} bounded batches in {:.3f}s", upload_batch_count, std::chrono::duration<f64>(upload_done - decode_done).count());

		for (const atlas_frame& frame : frames) {
			atlas.frames.emplace_back(packed_frame_from(frame, layout.width, layout.height, options.padding));
		}

		atlas.view = rt::unique(rt::TextureView::CreateFromTexture(atlas.atlas_texture));
		atlas.sampler = rt::unique(rt::Sampler::Create());
		rt::Sampler::SetFilter(atlas.sampler, options.smooth ? rt::filter::linear : rt::filter::nearest, options.smooth ? rt::filter::linear : rt::filter::nearest, levels > 1 ? rt::mip_filter::linear : rt::mip_filter::none);
		rt::Sampler::SetAddress(atlas.sampler, rt::address_mode::clamp, rt::address_mode::clamp, rt::address_mode::clamp);
		const auto view_done = std::chrono::steady_clock::now();
		log::Info("[textures] atlas view/finalize: {:.3f}s, total {:.3f}s", std::chrono::duration<f64>(view_done - upload_done).count(), std::chrono::duration<f64>(view_done - build_start).count());
		log::Debug("[textures] atlas {}x{} with {} frames in {} source groups", layout.width, layout.height, frames.size(), upload_groups.size());
		return atlas;
	}
} // namespace lf

namespace lf {
	texture_atlas& loaded_texture_atlas() {
		static auto atlas = texture_atlas();
		return atlas;
	}
}
