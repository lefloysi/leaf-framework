#ifndef LEAF_GRAPHICS_GRAPHICS_PROGRAM_HPP
#define LEAF_GRAPHICS_GRAPHICS_PROGRAM_HPP

#include <leaf/core/math/dim.hpp>
#include <leaf/core/math/pos.hpp>
#include <leaf/core/math/vec.hpp>
#include <leaf/core/span.hpp>
#include <leaf/core/string.hpp>
#include <leaf/core/vector.hpp>
#include <leaf/graphics/resource.hpp>

#include <concepts>
#include <memory>
#include <type_traits>

namespace rt {
	template<typename T, usize Components>
	struct vertex_format_trait;

	template<>
	struct vertex_format_trait<f32, 1> {
		static constexpr format value = format::r32_sfloat;
	};
	template<>
	struct vertex_format_trait<f32, 2> {
		static constexpr format value = format::rg32_sfloat;
	};
	template<>
	struct vertex_format_trait<f32, 3> {
		static constexpr format value = format::rgb32_sfloat;
	};
	template<>
	struct vertex_format_trait<f32, 4> {
		static constexpr format value = format::rgba32_sfloat;
	};

	template<>
	struct vertex_format_trait<i32, 1> {
		static constexpr format value = format::r32_sint;
	};
	template<>
	struct vertex_format_trait<i32, 2> {
		static constexpr format value = format::rg32_sint;
	};
	template<>
	struct vertex_format_trait<i32, 3> {
		static constexpr format value = format::rgb32_sint;
	};
	template<>
	struct vertex_format_trait<i32, 4> {
		static constexpr format value = format::rgba32_sint;
	};

	template<>
	struct vertex_format_trait<u32, 1> {
		static constexpr format value = format::r32_uint;
	};
	template<>
	struct vertex_format_trait<u32, 2> {
		static constexpr format value = format::rg32_uint;
	};
	template<>
	struct vertex_format_trait<u32, 3> {
		static constexpr format value = format::rgb32_uint;
	};
	template<>
	struct vertex_format_trait<u32, 4> {
		static constexpr format value = format::rgba32_uint;
	};

	template<typename T>
	struct vertex_format_of;

	template<>
	struct vertex_format_of<f32> {
		static constexpr format value = vertex_format_trait<f32, 1>::value;
	};

	template<>
	struct vertex_format_of<i32> {
		static constexpr format value = vertex_format_trait<i32, 1>::value;
	};

	template<>
	struct vertex_format_of<u32> {
		static constexpr format value = vertex_format_trait<u32, 1>::value;
	};

	template<typename T>
	struct vertex_format_of<lf::pos2<T>> {
		static constexpr format value = vertex_format_trait<T, 2>::value;
	};

	template<typename T>
	struct vertex_format_of<lf::dim2<T>> {
		static constexpr format value = vertex_format_trait<T, 2>::value;
	};

	template<glm::length_t Components, typename T, glm::qualifier Qualifier>
	struct vertex_format_of<glm::vec<Components, T, Qualifier>> {
		static constexpr format value = vertex_format_trait<T, Components>::value;
	};

	template<typename T, usize Components>
	struct vertex_format_of<T[Components]> {
		static constexpr format value = vertex_format_trait<T, Components>::value;
	};

	template<typename V, typename M>
	struct vertex_attribute_member;

	struct vertex_attribute {
		string name;
		u32 offset = 0;
		rt::format format = rt::format::unknown;

		static vertex_attribute Make(string_view name, u32 offset, rt::format format) {
			return { string(name), offset, format };
		}

		template<typename V, typename M>
		static vertex_attribute_member<V, M> Make(string_view name, M V::* member);
	};

	template<typename V, typename M>
	struct vertex_attribute_member {
		using vertex_type = V;

		string name;
		M V::* member;

		static vertex_attribute_member Make(string_view name, M V::* member) {
			return { string(name), member };
		}

		vertex_attribute Attribute() const {
			static_assert(std::is_standard_layout_v<V>);
			static_assert(std::is_default_constructible_v<V>);
			V vertex{};
			const char* base = reinterpret_cast<const char*>(std::addressof(vertex));
			const char* address = reinterpret_cast<const char*>(std::addressof(vertex.*member));
			return vertex_attribute::Make(name, static_cast<u32>(address - base), vertex_format_of<M>::value);
		}
	};

	template<typename V, typename M>
	vertex_attribute_member<V, M> vertex_attribute::Make(string_view name, M V::* member) {
		return vertex_attribute_member<V, M>::Make(name, member);
	}

	struct vertex_input {
		vector<vertex_attribute> attributes;
		u32 stride = 0;
		vertex_rate rate = rt::vertex_rate::vertex;

		template<typename V, typename... Attributes>
			requires(sizeof...(Attributes) != 0 && (std::same_as<typename Attributes::vertex_type, V> && ...))
		static vertex_input Make(const Attributes&... attributes) {
			static_assert(std::is_standard_layout_v<V>);
			vertex_input input;
			input.stride = sizeof(V);
			input.attributes.reserve(sizeof...(Attributes));
			(input.attributes.emplace_back(attributes.Attribute()), ...);
			return input;
		}
	};

	struct vertex_layout {
		span<const vertex_input> inputs;

		static vertex_layout Make(span<const vertex_input> inputs) {
			return { inputs };
		}
	};

	struct program_bytes {
		const u08* data = nullptr;
		usize size = 0;
	};

	namespace Program {
		handle<program> Create();
		void Destroy(handle<program> program);
		void VertexLayout(view<program> program, const vertex_layout& layout);
		void Source(view<program> program, string_view entry_point, program_bytes data);
		void Source(view<program> program, string_view entry_point, span<const byte> data);
		void RasterState(view<program> program, cull_mode cull_mode, front_face front_face, fill_mode fill_mode);
		void BlendState(view<program> program, bool enabled, blend_factor src_color, blend_factor dst_color, blend_op color_op, blend_factor src_alpha, blend_factor dst_alpha, blend_op alpha_op);
		void Finalize(view<program> program);
		location UniformLocation(view<program> program, string_view name);
		location InputLocation(view<program> program, span<const vertex_attribute> attributes);
		location OutputLocation(view<program> program, string_view name = {});
	} // namespace Program

} // namespace rt

#endif /* LEAF_GRAPHICS_GRAPHICS_PROGRAM_HPP */
