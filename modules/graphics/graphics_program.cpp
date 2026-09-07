#include "leaf/graphics/graphics_program.hpp"

namespace rt {
	handle<program> Program::Create() {
		rt_program program = rtProgramCreate();
		detail::check_rutile_error("failed to create program");
		return { program };
	}

	void Program::Destroy(handle<program> program) {
		rtProgramDestroy(program);
	}

	void Program::VertexLayout(view<program> program, const vertex_layout& layout) {
		vector<rt_vertex_input> inputs;
		vector<vector<rt_vertex_attribute>> attributes;
		inputs.reserve(layout.inputs.size());
		attributes.reserve(layout.inputs.size());
		for (const vertex_input& input : layout.inputs) {
			vector<rt_vertex_attribute>& native_attributes = attributes.emplace_back();
			native_attributes.reserve(input.attributes.size());
			for (const vertex_attribute& attribute : input.attributes) {
				native_attributes.push_back({ attribute.name.c_str(), attribute.offset, static_cast<rt_format>(attribute.format) });
			}
			inputs.push_back({ native_attributes.data(), native_attributes.size(), input.stride, static_cast<rt_vertex_rate>(input.rate) });
		}
		rt_vertex_layout rutile_layout = { inputs.data(), inputs.size() };
		rtProgramSetLayout(program, &rutile_layout);
		detail::check_rutile_error("failed to set program vertex layout");
	}

	void Program::Source(view<program> program, string_view entry_point, span<const byte> data) {
		rtProgramSource(program, string(entry_point).c_str(), reinterpret_cast<const u08*>(data.data()), data.size());
		detail::check_rutile_error("failed to set program source");
	}

	void Program::Source(view<program> program, string_view entry_point, program_bytes data) {
		rtProgramSource(program, string(entry_point).c_str(), data.data, data.size);
		detail::check_rutile_error("failed to set program source");
	}

	void Program::RasterState(view<program> program, cull_mode cull_mode, front_face front_face, fill_mode fill_mode) {
		rtProgramSetRasterState(program, static_cast<rt_cull_mode>(cull_mode), static_cast<rt_front_face>(front_face), static_cast<rt_fill_mode>(fill_mode));
		detail::check_rutile_error("failed to set program raster state");
	}

	void Program::BlendState(view<program> program, bool enabled, blend_factor src_color, blend_factor dst_color, blend_op color_op, blend_factor src_alpha, blend_factor dst_alpha, blend_op alpha_op) {
		rtProgramSetBlendState(program, enabled, static_cast<rt_blend_factor>(src_color), static_cast<rt_blend_factor>(dst_color), static_cast<rt_blend_op>(color_op), static_cast<rt_blend_factor>(src_alpha), static_cast<rt_blend_factor>(dst_alpha), static_cast<rt_blend_op>(alpha_op));
		detail::check_rutile_error("failed to set program blend state");
	}

	void Program::Finalize(view<program> program) {
		rtProgramFinalize(program);
		detail::check_rutile_error("failed to finalize program");
	}

	location Program::UniformLocation(view<program> program, string_view name) {
		location result = rtProgramUniformLocation(program, string(name).c_str());
		detail::check_rutile_error("failed to query program uniform location");
		if (!result) {
			throw runtime_exception(lf::format("program has no uniform '{}'", name));
		}
		return result;
	}

	location Program::InputLocation(view<program> program, span<const vertex_attribute> attributes) {
		vector<rt_vertex_attribute> native_attributes;
		native_attributes.reserve(attributes.size());
		for (const vertex_attribute& attribute : attributes) {
			native_attributes.push_back({ attribute.name.c_str(), attribute.offset, static_cast<rt_format>(attribute.format) });
		}
		location result = rtProgramInputLocation(program, native_attributes.data(), native_attributes.size());
		detail::check_rutile_error("failed to query program input location");
		if (!result) {
			throw runtime_exception("program has no matching vertex input");
		}
		return result;
	}

	location Program::OutputLocation(view<program> program, string_view name) {
		location result = rtProgramOutputLocation(program, name.empty() ? nullptr : string(name).c_str());
		detail::check_rutile_error("failed to query program output location");
		return result;
	}

} // namespace rt
