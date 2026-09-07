#include <catch2/catch_test_macros.hpp>

#include <cstddef>

#include "leaf/core/math/pos.hpp"
#include "leaf/graphics/graphics_program.hpp"

namespace lf::test {
	struct Vertex {
		lf::pos2<f32> position;
		f32 color[4];
	};

	struct OtherVertex {
		lf::pos2<f32> position;
	};

	template<typename V, typename Attribute>
	concept vertex_input_accepts = requires(const Attribute& attribute) {
		rt::vertex_input::Make<V>(attribute);
	};

	template<typename V>
	concept vertex_input_accepts_empty = requires {
		rt::vertex_input::Make<V>();
	};
} // namespace lf::test

using VertexPositionAttribute = decltype(rt::vertex_attribute::Make("position", &lf::test::Vertex::position));

static_assert(lf::test::vertex_input_accepts<lf::test::Vertex, VertexPositionAttribute>);
static_assert(!lf::test::vertex_input_accepts<lf::test::OtherVertex, VertexPositionAttribute>);
static_assert(!lf::test::vertex_input_accepts_empty<lf::test::Vertex>);

TEST_CASE("graphics vertex input infers member attributes", "[graphics]") {
	const rt::vertex_input input = rt::vertex_input::Make<lf::test::Vertex>(
		rt::vertex_attribute::Make("position", &lf::test::Vertex::position),
		rt::vertex_attribute::Make("color", &lf::test::Vertex::color)
	);

	REQUIRE(input.stride == sizeof(lf::test::Vertex));
	REQUIRE(input.attributes.size() == 2);
	CHECK(input.attributes[0].name == "position");
	CHECK(input.attributes[0].offset == offsetof(lf::test::Vertex, position));
	CHECK(input.attributes[0].format == rt::format::rg32_sfloat);
	CHECK(input.attributes[1].name == "color");
	CHECK(input.attributes[1].offset == offsetof(lf::test::Vertex, color));
	CHECK(input.attributes[1].format == rt::format::rgba32_sfloat);
}
