#include <leaf/script/state.hpp>
#include <leaf/script/mod_loader.hpp>
#include <leaf/core/register.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("script states install shared interfaces and retain independent globals", "[script]") {
	auto first{ lf::CreateState() };
	auto second{ lf::CreateState() };
	REQUIRE(first.safe_script("assert(type(settings.get) == 'function'); assert(type(mods.enabled) == 'function'); assert(type(localization.languages) == 'function'); assert(fs.join('/saves', 'world.ooo') == '/saves/world.ooo')", sol::script_pass_on_error).valid());
	first["scene_local"] = 42;
	REQUIRE(second["scene_local"].get<sol::object>().get_type() == sol::type::lua_nil);
	lf::Register<sol::state, lf::error(sol::state&)>::add([](sol::state& state) -> lf::error { state["extra"] = true; return {}; });
	auto third{ lf::CreateState() };
	REQUIRE(third["extra"].get<bool>());
	REQUIRE(second["extra"].get<sol::object>().get_type() == sol::type::lua_nil);
}

TEST_CASE("script objects preserve nested settings values", "[script]") {
	auto state{ lf::CreateState() };
	lf::list nested;
	nested.emplace_back(12);
	nested.emplace_back("name");
	lf::dict fields;
	fields.emplace("enabled", lf::object{ true });
	fields.emplace("nested", lf::object{ nested });
	const lf::object value{ fields };
	state["value"] = lf::object_to_sol(state, value);
	REQUIRE(state.safe_script("assert(value.enabled); assert(value.nested[1] == 12); assert(value.nested[2] == 'name')", sol::script_pass_on_error).valid());
	const auto result{ lf::sol_to_object(state["value"].get<sol::object>()) };
	REQUIRE(result.at("enabled").as<bool>());
	REQUIRE(result.at("nested").at(0).as<i64>() == 12);
}
