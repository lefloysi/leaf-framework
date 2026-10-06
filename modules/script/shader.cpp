#include "leaf/resource/prototypes/shader.hpp"

#include <leaf/core/exception.hpp>
#include <magic_enum/magic_enum.hpp>

namespace lf {
	ShaderPrototype::ShaderPrototype(const dict& data)
		: Prototype<identifier<ShaderPrototype, u16, void>>{ data } {
		data.assign(schema(*this));
	}

	rt::format object_trait<rt::format>::parse(const object& value) {
		const string& format{ value.as<string>() };
		if (const auto parsed{ magic_enum::enum_cast<rt::format>(format) }) {
			return *parsed;
		}
		throw runtime_exception(lf::format("unknown vertex format '{}'", format));
	}

	error ShaderPrototype::load() {
		if (!path.empty() && !bytes.empty()) {
			return error{ generic_errc::invalid_argument, "shader has both a source path and embedded bytes" };
		}
		asset::shader::description description{};
		if (bytes.empty()) {
			if (path.empty()) {
				return error{ generic_errc::missing_field, "shader has no source path or embedded bytes" };
			}
			const auto source{ fs::path::parse(path) };
			if (!source) {
				return source.error();
			}
			description = asset::shader::description{ *source, entry_point };
		} else {
			description = asset::shader::description{ { bytes.data(), bytes.size() }, entry_point };
		}

		rt::vertex_input input{};
		input.stride = stride;
		for (const rt::vertex_attribute& attribute : attributes) {
			input.attributes.push_back(attribute);
		}
		description.inputs.push_back(std::move(input));
		description.blend_enabled = true;
		description.destination_color = rt::blend_factor::one_minus_src_alpha;
		description.destination_alpha = rt::blend_factor::one_minus_src_alpha;

		const auto created{ asset::add(std::move(description)) };
		if (!created) {
			return created.error();
		}
		shader = *created;
		group = make_unique<asset::group>();
		group->include(shader);
		asset::load(*group, Progress{});
		if (asset::poll(*group) == asset::state::failed) {
			return asset::failure(*group);
		}
		return {};
	}

	void ShaderPrototype::include(asset::group& group) const {
		if (shader) {
			group.include(shader);
		}
	}
} // namespace lf
