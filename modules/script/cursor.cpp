#include "leaf/resource/prototypes/cursor.hpp"

#include "leaf/core/format.hpp"
#include "leaf/core/logging.hpp"
#include "leaf/platform/platform.hpp"
#include "leaf/core/filesystem.hpp"

#include <stb_image.h>

#include <memory>

namespace lf {
	CursorPrototype::CursorPrototype(const dict& data) : Prototype{ data } {
		data.assign(schema(*this));
	}

	error CursorPrototype::load() {
		if (handle || path.empty()) {
			return {};
		}

		report<fs::path> image_path = fs::path::parse(path);
		if (!image_path) {
			log::Warning("{}", lf::format("[cursor] {}", image_path.error().message));
			return {};
		}
		report<vector<u08>> image = fs::read_all(*image_path);
		if (!image) {
			log::Warning("{}", lf::format("[cursor] {}", image.error().message));
			return {};
		}

		int width = 0;
		int height = 0;
		int components = 0;
		std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels{
			stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(image->data()), static_cast<int>(image->size()), &width, &height, &components, 4),
			stbi_image_free,
		};
		if (!pixels || width <= 0 || height <= 0) {
			log::Warning("{}", lf::format("[cursor] failed to load cursor image '{}'", path));
			return {};
		}

		if (hotspot_x >= static_cast<u32>(width) || hotspot_y >= static_cast<u32>(height)) {
			log::Warning("{}", lf::format("[cursor] hotspot outside cursor image '{}'", Database<CursorPrototype>::name(id)));
			return {};
		}

		handle = create_platform_cursor(pixels.get(), static_cast<u32>(width), static_cast<u32>(height), hotspot_x, hotspot_y);
		return {};
	}

	CursorPrototype::~CursorPrototype() {
		destroy_platform_cursor(handle);
	}
} // namespace lf
