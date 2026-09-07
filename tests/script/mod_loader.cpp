#include <leaf/script/mod_loader.hpp>
#include <leaf/script/mod_enabled.hpp>
#include <leaf/script/state.hpp>
#include <leaf/core/scope.hpp>
#include <catch2/catch_test_macros.hpp>

namespace lf::tests {

TEST_CASE("Lua mod changes report persistence failures", "[mods][script]") {
	const auto root{ std::filesystem::absolute("mod-enabled-test-work") };
	REQUIRE(root.parent_path() == std::filesystem::current_path());
	auto storage{ fs::native_volume(root, fs::native_volume_options{ fs::access_mode::read_write, fs::missing_action::create }) };
	REQUIRE(storage);
	scope_exit cleanup{ [&] { std::error_code error; std::filesystem::remove_all(root, error); } };
	auto writable{ fs::mount("/test-enabled", *storage) };
	REQUIRE(writable);
	REQUIRE_FALSE(save_enabled_mods(fs::path{ "/test-enabled/mods.yaml" }, { { "example", { true, version{ 1 } } } }));
	auto readonly_storage{ fs::native_volume(root) };
	REQUIRE(readonly_storage);
	auto readonly{ fs::mount("/test-enabled-readonly", *readonly_storage) };
	REQUIRE(readonly);
	auto state{ CreateState() };
	auto result{ state.safe_script(R"(
		assert(mods.enabled('/test-enabled/mods.yaml').example.enabled)
		mods.set_enabled('/test-enabled/mods.yaml', 'example', false)
		assert(not mods.enabled('/test-enabled/mods.yaml').example.enabled)
		assert(not pcall(mods.set_enabled, '/test-enabled/mods.yaml', 'missing', true))
		assert(not pcall(mods.set_enabled, '/test-enabled-readonly/mods.yaml', 'example', true))
		assert(not mods.enabled('/test-enabled/mods.yaml').example.enabled)
	)", sol::script_pass_on_error) };
	if (!result.valid()) { const sol::error error = result; INFO(error.what()); REQUIRE(result.valid()); }
	REQUIRE(result.valid());
}

TEST_CASE("mod discovery uses mapped sources and releases failed loads", "[mods][filesystem]") {
	const auto root{ std::filesystem::absolute("mod-source-test-work") };
	REQUIRE(root.parent_path() == std::filesystem::current_path());
	auto storage{ fs::native_volume(root, fs::native_volume_options(fs::access_mode::read_write, fs::missing_action::create)) };
	REQUIRE(storage);
	scope_exit cleanup{ [&] { std::error_code error; std::filesystem::remove_all(root, error); } };
	auto appdata{ fs::mount("/", *storage) };
	auto sources{ fs::mount("/test-sources", *storage) };
	REQUIRE(appdata);
	REQUIRE(sources);
	scope_exit unload{ [] { mod::Unload(); } };
	REQUIRE(fs::create_directories("/test-sources/mods/example"));
	const string info{ "name: example\nmod_version: 1.0.0\n" };
	REQUIRE(fs::write_all("/test-sources/mods/example/info.yaml", span<const u08>{ reinterpret_cast<const u08*>(info.data()), info.size() }));
	save_enabled_mods(fs::path{ "/enabled_mods.yaml" }, { { "example", { true, version{ 1, 0, 0 } } } });
	const mod::Source source[]{ { .path = "/test-sources/mods" } };
	for (int load{}; load < 2; ++load) {
		const auto error{ mod::Load(source) };
		INFO(error.message);
		REQUIRE_FALSE(error);
		REQUIRE(mod::Loaded().size() == 1);
		REQUIRE_FALSE(mod::Loaded().front().privileged);
		REQUIRE(mod::Loaded().front().location.text() == "/example");
		REQUIRE(fs::exists("/example/info.yaml"));
	}
	const mod::Source duplicate[]{ source[0], source[0] };
	REQUIRE(mod::Load(duplicate));
	REQUIRE(mod::Loaded().empty());
	REQUIRE_FALSE(fs::exists("/example/info.yaml"));
	const mod::Source missing[]{ { .path = "/test-sources/missing" } };
	const auto listing{ fs::list(missing[0].path) };
	REQUIRE_FALSE(listing);
	const auto error{ mod::Load(missing) };
	REQUIRE(error.code == listing.error().code);
	const mod::Source privileged[]{ { .path = source[0].path, .privileged = true } };
	REQUIRE(mod::Load(privileged));
	REQUIRE_FALSE(fs::exists("/example/info.yaml"));
	for (const char* name : { "settings", "enabled_mods.yaml" }) {
		const string collision{ string("name: ") + name + "\nmod_version: 1.0.0\n" };
		REQUIRE(fs::write_all("/test-sources/mods/example/info.yaml", span<const u08>{ reinterpret_cast<const u08*>(collision.data()), collision.size() }));
		REQUIRE(mod::Load(source));
		REQUIRE(mod::Loaded().empty());
	}
	const string malformed{ "name: [" };
	REQUIRE(fs::write_all("/test-sources/mods/example/info.yaml", span<const u08>{ reinterpret_cast<const u08*>(malformed.data()), malformed.size() }));
	REQUIRE(mod::Load(source));
	REQUIRE(mod::Loaded().empty());
}

}
