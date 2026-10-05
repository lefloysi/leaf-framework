#include "leaf/resource/prototypes/texture.hpp"
#include "leaf/core/filesystem.hpp"

namespace lf {
	TextureFramePrototype::TextureFramePrototype(const dict& data)
		: Prototype<identifier<TextureFramePrototype, u16, void>>{ data } {
		data.assign(schema(*this));
	}

	error TextureFramePrototype::load() {
		const auto source = fs::path::parse(path);
		if (!source) {
			return source.error();
		}
		return {};
	}

	TexturePrototype::TexturePrototype(const dict& data)
		: Prototype<identifier<TexturePrototype, u16, void>>{ data } {
		data.assign(schema(*this));

	}

	TexturePrototype::~TexturePrototype() = default;

	error TexturePrototype::load() {
		for (TextureFramePrototype::ID frame : frames) {
			if (!frame) {
				return error{ generic_errc::invalid_id, "texture references a missing frame" };
			}
		}
		return {};
	}
} // namespace lf
