#include "leaf/resource/prototypes/texture.hpp"
#include "leaf/resource/prototypes/icon.hpp"
#include "leaf/core/filesystem.hpp"

namespace lf {
	IconPrototype::IconPrototype(const dict& data)
		: Prototype<identifier<IconPrototype, u16, void>>{ data } {
		data.assign(schema(*this));
	}

	error IconPrototype::load() {
		if (frames.empty()) {
			return error{ generic_errc::invalid_id, "icon requires an image frame" };
		}
		for (TextureFramePrototype::ID frame : frames) {
			if (!frame) {
				return error{ generic_errc::invalid_id, "icon references a missing frame" };
			}
		}
		return {};
	}
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
