#pragma once

#include "leaf/core/string.hpp"
#include "leaf/core/types.hpp"
#include "leaf/resource/prototype.hpp"

namespace lf {
	struct SoundGroupPrototype final : public Prototype<identifier<SoundGroupPrototype, u16, void>> {
		static constexpr string_view type() noexcept { return "sound-group"; }

		explicit SoundGroupPrototype(const dict& data) : Prototype(data) {}

		f32 volume = 1.0f;
	};

	struct SoundPrototype final : public Prototype<identifier<SoundPrototype, u16, void>> {
		static constexpr string_view type() noexcept { return "sound"; }

		explicit SoundPrototype(const dict& data);

		string path;
		SoundGroupPrototype::ID group;
		f32 volume = 1.0f;
	};

	template<>
	struct schema_trait<SoundGroupPrototype> {
		static auto get(auto& value) {
			return group(
				schema(PrototypeBase::base(value)),
				field("volume", value.volume)
			);
		}
	};

	template<>
	struct schema_trait<SoundPrototype> {
		static auto get(auto& value) {
			return group(
				schema(PrototypeBase::base(value)),
				field("path", value.path),
				field("sound_group", value.group),
				field("volume", value.volume)
			);
		}
	};

} // namespace lf
