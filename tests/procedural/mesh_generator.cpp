#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <leaf/procedural/mesh_generator.hpp>
#include <leaf/core/vector.hpp>
#include <thread>

struct alignas(128) Vertex {
	unsigned value = 0;
};

template<typename Index>
struct Generator : lf::MeshGenerator<Vertex, Index> {
	void generate();
};

template<typename Index>
void Generator<Index>::generate() {
	this->reserve(3, 6);
	for (unsigned i = 0; i < 3; ++i) {
		this->current.value = i;
		std::memcpy(this->vertex_cursor++, &this->current, sizeof(Vertex));
	}
	for (unsigned i = 0; i < 6; ++i) {
		*this->index_cursor++ = static_cast<Index>(i % 3);
	}
	this->publish();
}

TEMPLATE_TEST_CASE("procedural index widths and explicit persistent copy", "", u08, u16, u32, u64) {
	Generator<TestType> generator;
	generator.generate();
	REQUIRE(generator.mesh.indices.size() == 6);
	REQUIRE(generator.mesh.indices[5] == 2);
	REQUIRE(reinterpret_cast<std::uintptr_t>(generator.mesh.indices.data()) % alignof(TestType) == 0);
	const lf::vector<Vertex> owned{ generator.mesh.vertices.begin(), generator.mesh.vertices.end() };
	generator.reserve(1, 1, lf::Primitive::points);
	generator.current.value = 99;
	std::memcpy(generator.vertex_cursor++, &generator.current, sizeof(Vertex));
	*generator.index_cursor++ = 0;
	generator.publish();
	REQUIRE(owned[0].value == 0);
	if constexpr (sizeof(TestType) < sizeof(usize)) {
		REQUIRE_THROWS(generator.reserve(static_cast<usize>(std::numeric_limits<TestType>::max()) + 2));
	}
}

TEST_CASE("procedural copies aligned state and publishes counts") {
	lf::MeshGenerator<Vertex, void> generator;
	generator.reserve(6);
	REQUIRE(reinterpret_cast<std::uintptr_t>(generator.vertex_cursor) % alignof(Vertex) == 0);
	for (unsigned i = 0; i < 3; ++i) {
		generator.current.value = i;
		std::memcpy(generator.vertex_cursor++, &generator.current, sizeof(Vertex));
	}
	generator.current.value = 99;
	generator.publish();
	REQUIRE(generator.mesh.vertices.size() == 3);
	REQUIRE(generator.mesh.vertices[2].value == 2);
	REQUIRE_THROWS(generator.publish());
	generator.reserve(600);
	REQUIRE(generator.mesh.vertices.empty());
	generator.publish();
	REQUIRE(generator.mesh.vertices.empty());
}

TEST_CASE("procedural rejects invalid bounds and topology") {
	lf::MeshGenerator<Vertex, u08> generator;
	REQUIRE_THROWS(generator.reserve(257));
	REQUIRE_THROWS(generator.reserve(std::numeric_limits<usize>::max()));
	generator.reserve(256, 3);
	generator.vertex_cursor += 256;
	generator.index_cursor += 2;
	REQUIRE_THROWS(generator.publish());
	generator.index_cursor += 1;
	generator.publish();
	REQUIRE(generator.mesh.vertices.size() == 256);
	generator.reserve(3, 3);
	generator.vertex_cursor = reinterpret_cast<Vertex*>(reinterpret_cast<std::uintptr_t>(generator.vertex_cursor) + sizeof(Vertex) * 4);
	REQUIRE_THROWS(generator.publish());
	lf::MeshGenerator<Vertex, void> plain;
	REQUIRE_THROWS(plain.reserve(3, 1));
	REQUIRE_THROWS(plain.scratch<unsigned>(1));
}

TEST_CASE("procedural leases are independent and reuse buffers") {
	lf::procedural::trim_scratch();
	{
		lf::MeshGenerator<Vertex, void> outer;
		outer.reserve(3);
		auto* data = outer.vertex_cursor;
		outer.current.value = 42;
		std::memcpy(outer.vertex_cursor++, &outer.current, sizeof(Vertex));
		{
			lf::MeshGenerator<Vertex, void> inner;
			inner.reserve(300);
			REQUIRE(inner.vertex_cursor != data);
			auto first = inner.scratch<Vertex>(2);
			const Vertex value{ 17 };
			std::memcpy(first.data(), &value, sizeof(Vertex));
			inner.scratch<Vertex>(200);
			REQUIRE(first[0].value == 17);
		}
		REQUIRE(data->value == 42);
		const auto allocations = lf::procedural::scratch_statistics().allocations;
		{
			lf::MeshGenerator<Vertex, void> again;
			again.reserve(300);
			again.scratch<Vertex>(2);
			again.scratch<Vertex>(200);
		}
		REQUIRE(lf::procedural::scratch_statistics().allocations == allocations);
		lf::procedural::trim_scratch();
		REQUIRE(data->value == 42);
	}
	lf::procedural::trim_scratch();
	REQUIRE(lf::procedural::scratch_statistics().retained_bytes == 0);
}

TEST_CASE("procedural thread ownership and isolation") {
	lf::MeshGenerator<Vertex, void> main;
	main.reserve(3);
	bool independent = false;
	bool rejected = false;
	std::jthread worker{ [&] {
		lf::MeshGenerator<Vertex, void> other;
		other.reserve(3);
		independent = other.vertex_cursor != main.vertex_cursor;
		try {
			main.reserve(3);
		} catch (const lf::runtime_exception&) { rejected = true; }
	} };
	worker.join();
	REQUIRE(independent);
	REQUIRE(rejected);
}
