#pragma once

#include "leaf/core/distance.hpp"
#include "leaf/core/error.hpp"
#include "leaf/core/math/rect.hpp"
#include "leaf/core/vector.hpp"
#include "leaf/resource/prototype.hpp"

namespace lf {
	struct TextureFramePrototype final : public Prototype<identifier<TextureFramePrototype, u16, void>> {
		static constexpr string_view type() noexcept { return "texture-frame"; }
		TextureFramePrototype(const dict& data);
		error load() override;

		string path;
		rect<u32> rect{};
	};

	struct TexturePrototype final : public Prototype<identifier<TexturePrototype, u16, void>> {
		static constexpr string_view type() noexcept { return "texture"; }
		TexturePrototype(const dict& data);
		~TexturePrototype();
		error load() override;

		lf::distance distance = lf::distance::from_quantum(1);
		u32 frames_per_second = 0;
		vector<TextureFramePrototype::ID> frames;
	};

	template<>
	struct schema_trait<TextureFramePrototype> {
		static auto get(auto& value) {
			return group(
				schema(PrototypeBase::base(value)),
				field("path", value.path, value.path),
				field("rect", value.rect, value.rect)
			);
		}
	};

	template<>
	struct schema_trait<TexturePrototype> {
		static auto get(auto& value) {
			return group(
				schema(PrototypeBase::base(value)),
				field("distance", value.distance),
				field("fps", value.frames_per_second),
				field("frames", value.frames, value.frames)
			);
		}
	};
} // namespace lf
