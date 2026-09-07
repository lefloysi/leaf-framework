#include "leaf/manager/asset.hpp"

#include <algorithm>
#include <bit>
#include <future>
#include <leaf/core/thread_pool.hpp>
#include <leaf/graphics/command_buffer.hpp>
#include <leaf/graphics/sampler.hpp>
#include <leaf/graphics/texture.hpp>
#include <leaf/graphics/texture_view.hpp>
#include <leaf/graphics/timepoint.hpp>
#include <limits>
#include <mutex>
#include <stb_image.h>

namespace lf::asset {
	struct decoded_image {
		dim2<u32> size;
		vector<u08> pixels;
	};

	struct atlas_page {
		lifetime storage{};
		u32 extent{};
		usize allocations{};
		rt::unique<rt::texture> texture;
		rt::unique<rt::texture_view> view;
		rt::unique<rt::sampler> sampler;
		vector<rect<u32>> free_regions;
	};

	struct image_record {
		image::description description;
		usize demand{};
		state status{ state::idle };
		error result;
		std::future<report<decoded_image>> decoding;
		atlas_page* page{};
		rect<u32> allocation{};
		dim2<u32> size{};
		vector<rt::timepoint> retirement;
	};

	struct shader_record {
		shader::description description;
		usize demand{};
		state status{ state::idle };
		error result;
		rt::unique<rt::program> program;
		vector<rt::timepoint> retirement;
	};

	std::mutex records_mutex;
	unique_ptr<ThreadPool> workers;
	vector<unique_ptr<image_record>> images;
	vector<unique_ptr<shader_record>> shaders;
	vector<unique_ptr<atlas_page>> pages;
	vector<rt::timepoint> submissions;
	u32 generation{};

	report<decoded_image> decode(image::description description);
	image_record* find(image::ID image);
	shader_record* find(shader::ID shader);
	void allocate(image_record& record, const decoded_image& decoded, rt::view<rt::command_buffer> commands);

	error init(usize worker_count) {
		std::scoped_lock lock{ records_mutex };
		if (workers || !worker_count) {
			return error{ generic_errc::input_error, "invalid asset manager initialization" };
		}

		workers = make_unique<ThreadPool>(worker_count);
		++generation;
		return {};
	}

	void exit() {
		workers.reset();
		std::scoped_lock lock{ records_mutex };
		images.clear();
		shaders.clear();
		pages.clear();
		submissions.clear();
	}

	image::ID add(image::description description) {
		std::scoped_lock lock{ records_mutex };
		if (!workers) {
			throw runtime_exception("asset manager is not initialized");
		}

		for (usize index{}; index < images.size(); ++index) {
			const auto& existing{ images[index]->description };
			const auto same_crop{ !existing.crop && !description.crop || existing.crop && description.crop &&
																			 existing.crop->pos.x == description.crop->pos.x && existing.crop->pos.y == description.crop->pos.y &&
																			 existing.crop->dim.width == description.crop->dim.width && existing.crop->dim.height == description.crop->dim.height };
			if (existing.source == description.source && existing.storage == description.storage && same_crop) {
				return image::ID{ static_cast<u32>(index + 1), generation };
			}
		}

		auto record{ make_unique<image_record>() };
		record->description = std::move(description);
		images.emplace_back(std::move(record));
		return image::ID{ static_cast<u32>(images.size()), generation };
	}

	shader::description::description(fs::path value, string_view entry)
		: source{ std::move(value) }, entry_point{ entry } {}

	shader::description::description(rt::program_bytes value, string_view entry)
		: entry_point{ entry } {
		set_embedded(value);
	}

	void shader::description::set_embedded(rt::program_bytes value) {
		if (value.data && value.size) {
			bytes.assign(value.data, value.data + value.size);
		} else {
			bytes.clear();
		}
		source = {};
	}

	report<shader::ID> add(shader::description description) {
		std::scoped_lock lock{ records_mutex };
		if (!workers) {
			return unexpected(error{ generic_errc::input_error, "asset manager is not initialized" });
		}
		if (description.entry_point.empty()) {
			return unexpected(error{ generic_errc::missing_field, "shader entry point is empty" });
		}
		if (description.source.empty() == description.bytes.empty()) {
			return unexpected(error{ generic_errc::input_error, "shader requires exactly one source" });
		}

		auto record{ make_unique<shader_record>() };
		record->description = std::move(description);
		shaders.emplace_back(std::move(record));
		return shader::ID{ static_cast<u32>(shaders.size()), generation };
	}

	image_record* find(image::ID image) {
		if (image.gen() != generation || !image || image.get() > images.size()) {
			return nullptr;
		}
		return images[image.get() - 1].get();
	}

	shader_record* find(shader::ID shader) {
		if (shader.gen() != generation || !shader || shader.get() > shaders.size()) {
			return nullptr;
		}
		return shaders[shader.get() - 1].get();
	}

	group::~group() {
		unload(*this);
	}

	void group::include(image::ID image) {
		if (std::find(image_declarations.begin(), image_declarations.end(), image) == image_declarations.end()) {
			image_declarations.push_back(image);
		}
	}

	void group::include(shader::ID shader) {
		if (std::find(shader_declarations.begin(), shader_declarations.end(), shader) == shader_declarations.end()) {
			shader_declarations.push_back(shader);
		}
	}

	void load(group& group, Progress progress) {
		std::scoped_lock lock{ records_mutex };
		group.result = {};
		group.completed_images.clear();
		group.completed_shaders.clear();
		group.progress.emplace(std::move(progress));
		group.progress->add_total(group.image_declarations.size() + group.shader_declarations.size());
		group.status = state::loading;

		for (const auto image : group.image_declarations) {
			auto* record{ find(image) };
			if (!record) {
				group.result = error{ generic_errc::invalid_id, "invalid image declaration" };
				group.status = state::failed;
				return;
			}

			if (std::find(group.image_demands.begin(), group.image_demands.end(), image) == group.image_demands.end()) {
				group.image_demands.push_back(image);
				++record->demand;
				record->retirement.clear();
			}

			if (record->status == state::idle || record->status == state::failed) {
				record->result = {};
				record->decoding = workers->submit([description{ record->description }] { return decode(description); });
				record->status = state::loading;
			}
		}

		for (const auto shader : group.shader_declarations) {
			auto* record{ find(shader) };
			if (!record) {
				group.result = error{ generic_errc::invalid_id, "invalid shader declaration" };
				group.status = state::failed;
				return;
			}

			if (std::find(group.shader_demands.begin(), group.shader_demands.end(), shader) == group.shader_demands.end()) {
				group.shader_demands.push_back(shader);
				++record->demand;
				record->retirement.clear();
			}

			if (record->status == state::ready) {
				continue;
			}

			record->result = {};
			try {
				record->program = rt::unique{ rt::Program::Create() };
				if (!record->description.source.empty()) {
					const auto bytes{ fs::read_all(record->description.source) };
					if (!bytes) {
						record->result = bytes.error();
						record->status = state::failed;
						group.result = record->result;
						group.status = state::failed;
						return;
					}
					rt::Program::Source(record->program, record->description.entry_point, { reinterpret_cast<const byte*>(bytes->data()), bytes->size() });
				} else {
					rt::Program::Source(record->program, record->description.entry_point, { record->description.bytes.data(), record->description.bytes.size() });
				}
				rt::Program::VertexLayout(record->program, rt::vertex_layout::Make(record->description.inputs));
				rt::Program::RasterState(record->program, record->description.cull, record->description.front, record->description.fill);
				rt::Program::BlendState(record->program, record->description.blend_enabled, record->description.source_color, record->description.destination_color, record->description.color_operation, record->description.source_alpha, record->description.destination_alpha, record->description.alpha_operation);
				rt::Program::Finalize(record->program);
				record->status = state::ready;
			} catch (const std::exception& exception) {
				record->program = {};
				record->result = error{ generic_errc::unknown, exception.what() };
				record->status = state::failed;
				group.result = record->result;
				group.status = state::failed;
				return;
			}
		}
	}

	void unload(group& group) {
		std::scoped_lock lock{ records_mutex };
		for (const auto image : group.image_demands) {
			if (auto* record{ find(image) }; record && record->demand && --record->demand == 0) {
				record->retirement = submissions;
			}
		}
		for (const auto shader : group.shader_demands) {
			if (auto* record{ find(shader) }; record && record->demand && --record->demand == 0) {
				record->retirement = submissions;
			}
		}
		group.image_demands.clear();
		group.completed_images.clear();
		group.shader_demands.clear();
		group.completed_shaders.clear();
		group.progress.reset();
		group.status = state::idle;
	}

	state poll(group& group) {
		if (group.progress && group.progress->cancelled()) {
			unload(group);
			group.status = state::cancelled;
			return group.status;
		}

		std::scoped_lock lock{ records_mutex };
		if (group.status != state::loading) {
			return group.status;
		}

		for (const auto image : group.image_demands) {
			auto* record{ find(image) };
			if (!record || record->status == state::failed) {
				group.result = record ? record->result : error{ generic_errc::invalid_id, "invalid image declaration" };
				return group.status = state::failed;
			}
			if (record->status == state::ready && std::find(group.completed_images.begin(), group.completed_images.end(), image) == group.completed_images.end()) {
				group.completed_images.push_back(image);
				group.progress->advance();
			}
		}
		for (const auto shader : group.shader_demands) {
			auto* record{ find(shader) };
			if (!record || record->status == state::failed) {
				group.result = record ? record->result : error{ generic_errc::invalid_id, "invalid shader declaration" };
				return group.status = state::failed;
			}
			if (record->status == state::ready && std::find(group.completed_shaders.begin(), group.completed_shaders.end(), shader) == group.completed_shaders.end()) {
				group.completed_shaders.push_back(shader);
				group.progress->advance();
			}
		}

		if (group.completed_images.size() == group.image_demands.size() && group.completed_shaders.size() == group.shader_demands.size()) {
			group.status = state::ready;
		}
		return group.status;
	}

	error failure(const group& group) {
		return group.result;
	}

	report<decoded_image> decode(image::description description) {
		const auto bytes{ fs::read_all(description.source) };
		if (!bytes) {
			return unexpected(bytes.error());
		}
		if (bytes->size() > std::numeric_limits<int>::max()) {
			return unexpected(error{ generic_errc::input_error, "image file is too large" });
		}

		int width{}, height{};
		unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels{ stbi_load_from_memory(bytes->data(), static_cast<int>(bytes->size()), &width, &height, nullptr, 4), stbi_image_free };
		if (!pixels || width <= 0 || height <= 0) {
			return unexpected(error{ generic_errc::parse_error, lf::format("cannot decode image '{}'", description.source.text()) });
		}

		const rect<u32> crop{ description.crop.value_or(rect<u32>{ {}, { static_cast<u32>(width), static_cast<u32>(height) } }) };
		if (!crop.dim.width || !crop.dim.height || crop.pos.x >= static_cast<u32>(width) || crop.pos.y >= static_cast<u32>(height) ||
			crop.dim.width > static_cast<u32>(width) - crop.pos.x || crop.dim.height > static_cast<u32>(height) - crop.pos.y ||
			crop.dim.width > 8190 || crop.dim.height > 8190) {
			return unexpected(error{ generic_errc::input_error, "invalid image region" });
		}

		decoded_image result{ crop.dim, {} };
		const usize padded_width{ crop.dim.width + 2 };
		result.pixels.resize(padded_width * (crop.dim.height + 2) * 4);
		for (u32 y{}; y < crop.dim.height + 2; ++y) {
			const u32 source_y{ crop.pos.y + std::clamp(y, 1u, crop.dim.height) - 1 };
			for (u32 x{}; x < crop.dim.width + 2; ++x) {
				const u32 source_x{ crop.pos.x + std::clamp(x, 1u, crop.dim.width) - 1 };
				std::copy_n(pixels.get() + (static_cast<usize>(source_y) * width + source_x) * 4, 4, result.pixels.data() + (y * padded_width + x) * 4);
			}
		}
		return result;
	}

	void allocate(image_record& record, const decoded_image& decoded, rt::view<rt::command_buffer> commands) {
		const dim2<u32> size{ decoded.size.width + 2, decoded.size.height + 2 };
		atlas_page* page{};
		usize slot{};
		for (const auto& candidate : pages) {
			if (candidate->storage != record.description.storage) {
				continue;
			}
			for (usize index{}; index < candidate->free_regions.size(); ++index) {
				const auto& region{ candidate->free_regions[index] };
				if (region.dim.width >= size.width && region.dim.height >= size.height) {
					page = candidate.get();
					slot = index;
					break;
				}
			}
			if (page) {
				break;
			}
		}

		if (!page) {
			auto incoming{ make_unique<atlas_page>() };
			incoming->storage = record.description.storage;
			incoming->extent = std::max(1024u, std::bit_ceil(std::max(size.width, size.height)));
			incoming->texture = rt::unique{ rt::Texture::Create() };
			rt::Texture::Resize(incoming->texture, rt::texture_type::d2, rt::format::rgba8_unorm, { incoming->extent, incoming->extent, 1 });
			incoming->view = rt::unique{ rt::TextureView::CreateFromTexture(incoming->texture) };
			incoming->sampler = rt::unique{ rt::Sampler::Create() };
			rt::Sampler::SetFilter(incoming->sampler, rt::filter::linear, rt::filter::linear, rt::mip_filter::none);
			rt::Sampler::SetAddress(incoming->sampler, rt::address_mode::clamp, rt::address_mode::clamp, rt::address_mode::clamp);
			incoming->free_regions.push_back({ {}, { incoming->extent, incoming->extent } });
			page = incoming.get();
			pages.emplace_back(std::move(incoming));
		}

		const auto available{ page->free_regions[slot] };
		page->free_regions.erase(page->free_regions.begin() + slot);
		if (available.dim.width > size.width) {
			page->free_regions.push_back({ { available.pos.x + size.width, available.pos.y }, { available.dim.width - size.width, size.height } });
		}
		if (available.dim.height > size.height) {
			page->free_regions.push_back({ { available.pos.x, available.pos.y + size.height }, { available.dim.width, available.dim.height - size.height } });
		}

		record.page = page;
		record.allocation = { available.pos, size };
		record.size = decoded.size;
		++page->allocations;
		const rt::texture_range range{ rt::texture_aspect_flag::color, 0, 1, 0, 1, { size.width, size.height, 1 }, { available.pos.x, available.pos.y, 0 } };
		rt::Cmd::TextureData(commands, page->texture, range, decoded.pixels.data());
		rt::Cmd::TextureBarrier(commands, page->texture, range, { rt::stage_flag::transfer, rt::access_type::write }, { rt::stage_flag::fragment, rt::access_type::read });
	}

	void update(rt::view<rt::command_buffer> commands, usize maximum_images) {
		collect();
		std::scoped_lock lock{ records_mutex };
		for (const auto& record : images) {
			if (!maximum_images) {
				break;
			}
			if (record->status != state::loading || !record->decoding.valid() || record->decoding.wait_for(std::chrono::seconds{}) != std::future_status::ready) {
				continue;
			}

			--maximum_images;
			try {
				auto decoded{ record->decoding.get() };
				if (!record->demand) {
					record->status = state::idle;
				} else if (!decoded) {
					record->result = decoded.error();
					record->status = state::failed;
				} else {
					allocate(*record, *decoded, commands);
					record->status = state::ready;
				}
			} catch (const std::exception& error) {
				record->result = lf::error{ generic_errc::unknown, error.what() };
				record->status = state::failed;
			}
		}
	}

	optional<image_view> get(image::ID image) {
		std::scoped_lock lock{ records_mutex };
		const auto* record{ find(image) };
		if (!record || record->status != state::ready || !record->demand) {
			return {};
		}
		const auto* page{ record->page };
		const f32 extent{ static_cast<f32>(page->extent) };
		return image_view{ page->view, page->sampler, { { (record->allocation.pos.x + 1) / extent, (record->allocation.pos.y + 1) / extent }, { record->size.width / extent, record->size.height / extent } }, record->size };
	}

	optional<rt::view<rt::program>> get(shader::ID shader) {
		std::scoped_lock lock{ records_mutex };
		const auto* record{ find(shader) };
		if (!record || record->status != state::ready || !record->demand) {
			return {};
		}
		return record->program;
	}

	void submitted(rt::timepoint completion) {
		std::scoped_lock lock{ records_mutex };
		submissions.push_back(completion);
	}

	void collect() {
		std::scoped_lock lock{ records_mutex };
		std::erase_if(submissions, [](rt::timepoint completion) { return rt::Timepoint::Reached(completion); });
		for (const auto& record : images) {
			if (record->demand || !record->page) {
				continue;
			}
			if (!std::all_of(record->retirement.begin(), record->retirement.end(), [](rt::timepoint completion) { return rt::Timepoint::Reached(completion); })) {
				continue;
			}

			record->page->free_regions.push_back(record->allocation);
			--record->page->allocations;
			record->page = nullptr;
			record->retirement.clear();
			record->status = state::idle;
		}
		for (const auto& record : shaders) {
			if (record->demand || !record->program) {
				continue;
			}
			if (!std::all_of(record->retirement.begin(), record->retirement.end(), [](rt::timepoint completion) { return rt::Timepoint::Reached(completion); })) {
				continue;
			}

			record->program = {};
			record->retirement.clear();
			record->status = state::idle;
		}
		std::erase_if(pages, [](const auto& page) { return !page->allocations; });
	}

	usize resident_pages() {
		std::scoped_lock lock{ records_mutex };
		return pages.size();
	}
} // namespace lf::asset
