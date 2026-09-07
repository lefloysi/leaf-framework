#include <catch2/catch_test_macros.hpp>

#include <leaf/core/dynamic_object.hpp>
#include <leaf/core/schema.hpp>
#include <leaf/resource/database.hpp>
#include <leaf/script/mod_loader.hpp>
#include <leaf/resource/prototype.hpp>
#include <leaf/script/prototype.hpp>
#include <leaf/script/state.hpp>

#include <limits>

namespace leaf_test::prototype_lua {
	struct TestPrototype final : lf::Prototype<lf::identifier<TestPrototype, u16, void>> {
		static constexpr lf::string_view type() noexcept { return "test-prototype"; }

		explicit TestPrototype(const lf::dict& data) : Prototype(data) {}

		u32 value = 7;
		u64 seed = 11400714819323198485ull;
	};
} // namespace leaf_test::prototype_lua

template<>
struct lf::schema_trait<leaf_test::prototype_lua::TestPrototype> {
	static auto get(auto& value) {
		return lf::group(
			lf::schema(lf::PrototypeBase::base(value)),
			lf::field("value", value.value),
			lf::field("seed", value.seed)
		);
	}
};

TEST_CASE("prototype Lua export provides ordered and named records") {
	using leaf_test::prototype_lua::TestPrototype;

	lf::Database<TestPrototype>::clear();
	lf::Database<TestPrototype>::create("alpha");
	lf::dict data;
	data.emplace("name", "Alpha");
	lf::Database<TestPrototype>::init("alpha", data);

	sol::state lua;
	lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
	lf::ExportPrototypeTable<TestPrototype>(lua);

	sol::object prototype_tables = lua["prototypes"];
	REQUIRE(prototype_tables.valid());
	REQUIRE(prototype_tables.is<sol::table>());
	const sol::table prototypes = prototype_tables.as<sol::table>();

	sol::object records_value = prototypes["test-prototype"];
	REQUIRE(records_value.valid());
	REQUIRE(records_value.is<sol::table>());
	const sol::table records = records_value.as<sol::table>();

	sol::object record_by_id_value = records[1];
	REQUIRE(record_by_id_value.valid());
	REQUIRE(record_by_id_value.is<sol::table>());
	const sol::table record_by_id = record_by_id_value.as<sol::table>();

	sol::object record_by_name_value = records["alpha"];
	REQUIRE(record_by_name_value.valid());
	REQUIRE(record_by_name_value.is<sol::table>());
	const sol::table record_by_name = record_by_name_value.as<sol::table>();

	REQUIRE(record_by_id.get<u16>("id") == 1);
	REQUIRE(record_by_id.get<lf::string>("name") == "alpha");
	REQUIRE(record_by_id.get<u32>("value") == 7);
	REQUIRE(record_by_id.get<lf::string>("seed") == "11400714819323198485");
	REQUIRE(record_by_name.get<u16>("id") == 1);

	lf::Database<TestPrototype>::clear();
}

TEST_CASE("Lua object conversion preserves integers and requires dense lists") {
	sol::state lua;
	lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
	REQUIRE(lua.safe_script("integer = 9007199254740993; sparse = { [1] = true, [3] = true }; enormous = { [1e20] = true }; nonfinite = { [math.huge] = true }", sol::script_pass_on_error).valid());

	const lf::object integer{ lf::sol_to_object(lua["integer"].get<sol::object>()) };
	REQUIRE(integer.is<i64>());
	REQUIRE(integer.get<i64>() == 9007199254740993ll);
	REQUIRE_THROWS_AS(lf::sol_to_object(lua["sparse"].get<sol::object>()), lf::runtime_exception);
	REQUIRE_THROWS_AS(lf::sol_to_object(lua["enormous"].get<sol::object>()), lf::runtime_exception);
	REQUIRE_THROWS_AS(lf::sol_to_object(lua["nonfinite"].get<sol::object>()), lf::runtime_exception);

	if constexpr (std::numeric_limits<lua_Integer>::digits < std::numeric_limits<u64>::digits) {
		REQUIRE_THROWS_AS(lf::object_to_sol(lua, lf::object{ static_cast<u64>(std::numeric_limits<lua_Integer>::max()) + 1 }), lf::runtime_exception);
	}
}
