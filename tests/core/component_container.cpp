#include <leaf/core/component_container.hpp>
#include <leaf/core/string.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {
struct EntityTag;
using ID = lf::identifier<EntityTag, u32, void>;
struct Position { int x = 0; };
struct Name { lf::string value; };
using Entities = lf::component_container<ID, Position, Name>;
}

TEST_CASE("component container resolves zero-based identifiers") {
	Entities entities;
	REQUIRE_FALSE(entities.has<Position>(ID{}));
	REQUIRE(entities.find<Position>(ID{999}) == entities.end<Position>());
	const auto& empty = entities;
	REQUIRE(empty.find<Position>(ID{}) == empty.end<Position>());
	Entities::bundle bundle;
	bundle.component<Position>() = Position{42};
	const auto first = entities.create(std::move(bundle));
	const auto second = entities.create();
	REQUIRE(first == ID{0});
	REQUIRE(second == ID{1});
	REQUIRE(entities.find<Position>(first)->x == 42);
	REQUIRE_FALSE(entities.has<Position>(second));
	entities.add<Position>(second, Position{9});
	unsigned visited = 0;
	entities.each<Position>([&](ID id, const Position& value) {
		REQUIRE(entities.find<Position>(id)->x == value.x);
		++visited;
	});
	REQUIRE(visited == 2);
	entities.erase<Position>(first);
	REQUIRE_FALSE(entities.has<Position>(first));
	REQUIRE(entities.find<Position>(second)->x == 9);
}

TEST_CASE("component container reuses slots without mixing component owners") {
	Entities entities;
	const auto first = entities.create();
	const auto second = entities.create();
	entities.add<Position>(first, Position{3});
	entities.add<Position>(second, Position{7});
	entities.destroy(first);
	const auto reused = entities.create();
	REQUIRE(reused == first);
	REQUIRE_FALSE(entities.has<Position>(reused));
	REQUIRE(entities.find<Position>(second)->x == 7);
	entities.add<Name>(reused, Name{"camera"});
	REQUIRE(entities.find<Name>(reused)->value == "camera");
	entities.clear();
	REQUIRE(entities.create() == ID{0});
}
