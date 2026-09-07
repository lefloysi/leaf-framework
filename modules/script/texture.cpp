#include "leaf/resource/prototypes/texture.hpp"

namespace lf {
	TexturePrototype::TexturePrototype(const dict& data)
		: Prototype<identifier<TexturePrototype, u16, void>>{ data } {
		data.assign(schema(*this));

		if (frames.empty()) {
			frames.push_back({ .path = path });
		}
	}

	TexturePrototype::~TexturePrototype() = default;

	error TexturePrototype::load() {
		images.clear();

		for (const TextureSourceFrame& frame : frames) {
			const auto source{ fs::path::parse(frame.path) };

			if (!source) {
				return source.error();
			}

			images.push_back(asset::add({
				.source = *source,
				.crop = frame.rect,
				.storage = storage,
			}));
		}

		return {};
	}

	void TexturePrototype::include(asset::group& group) const {
		for (const auto image : images) {
			group.include(image);
		}
	}
} // namespace lf
