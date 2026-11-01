#pragma once

#include "leaf/resource/prototypes/texture.hpp"

namespace lf {
	struct IconPrototype final : public Prototype<identifier<IconPrototype, u16, void>> {
		static constexpr string_view type() noexcept { return "icon"; }
		explicit IconPrototype(const dict& data);
		error load() override;

		vector<TextureFramePrototype::ID> frames;
	};

	template<>
	struct schema_trait<IconPrototype> {
		static auto get(auto& value) {
			return group(
				schema(PrototypeBase::base(value)),
				field("frames", value.frames, value.frames)
			);
		}
	};
}
