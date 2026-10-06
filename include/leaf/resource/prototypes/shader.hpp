#pragma once

#include <leaf/core/memory.hpp>
#include <leaf/manager/asset.hpp>
#include <leaf/resource/prototype.hpp>
#include <leaf/script/prototype.hpp>

namespace lf {
	struct ShaderPrototype final : public Prototype<identifier<ShaderPrototype, u16, void>> {
		static constexpr string_view type() noexcept { return "shader"; }

		ShaderPrototype(const dict& data);
		error load() override;
		void include(asset::group& group) const;

		string path;
		vector<u08> bytes;
		string entry_point;
		u32 stride{};
		vector<rt::vertex_attribute> attributes;
		asset::shader::ID shader;
		unique_ptr<asset::group> group;
	};

	template<>
	struct schema_trait<ShaderPrototype> {
		static auto get(auto& value) {
			return group(
				schema(PrototypeBase::base(value)),
				field("path", value.path, value.path),
				field("bytes", value.bytes, value.bytes),
				field("entry_point", value.entry_point),
				field("stride", value.stride),
				field("layout", value.attributes)
			);
		}
	};
} // namespace lf
