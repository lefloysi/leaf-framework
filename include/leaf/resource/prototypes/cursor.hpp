#pragma once

#include "leaf/core/string.hpp"
#include "leaf/core/types.hpp"
#include "leaf/resource/prototype.hpp"

namespace lf {
	struct PlatformCursor;

	struct CursorPrototype final : public Prototype<identifier<CursorPrototype, u16, void>> {
		static constexpr string_view type() noexcept { return "cursor"; }

		string path;
		u32 hotspot_x = 0;
		u32 hotspot_y = 0;
		PlatformCursor* handle = nullptr;

		explicit CursorPrototype(const dict& data);
		~CursorPrototype();

		error load() override;
	};

	template<>
	struct schema_trait<CursorPrototype> {
		static auto get(auto& value) {
			return group(
				schema(PrototypeBase::base(value)),
				field("path", value.path),
				field("hotspot_x", value.hotspot_x),
				field("hotspot_y", value.hotspot_y)
			);
		}
	};
} // namespace lf
