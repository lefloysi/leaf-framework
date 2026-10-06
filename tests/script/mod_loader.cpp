#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <leaf/core/scope.hpp>
#include <leaf/core/register.hpp>

#include <leaf/script/settings.hpp>
#include <leaf/script/mod_enabled.hpp>
#include <leaf/script/mod_loader.hpp>
#include <leaf/script/state.hpp>

namespace lf::tests {
	namespace {
		auto completed_loads = usize(0);
		std::function<void()> initialize;
		const auto register_completion = [] {
			Register<Mod>::add([](Mod&) {
				++completed_loads;
				if (initialize) { initialize(); }
				return error();
			});
			return 0;
		}();
	}

	TEST_CASE("Lua mod changes report persistence failures", "[mods][script]") {
		const auto root{ std::filesystem::absolute("mod-enabled-test-work") };
		REQUIRE(root.parent_path() == std::filesystem::current_path());
		auto storage{ fs::native_volume(root, fs::native_volume_options{ fs::access_mode::read_write, fs::missing_action::create }) };
		REQUIRE(storage);
		scope_exit cleanup([&] { std::error_code error; std::filesystem::remove_all(root, error); });
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
	)",
									   sol::script_pass_on_error) };
		if (!result.valid()) {
			const sol::error error = result;
			INFO(error.what());
			REQUIRE(result.valid());
		}
		REQUIRE(result.valid());
	}

	TEST_CASE("mod discovery uses mapped sources and releases failed loads", "[mods][filesystem]") {
		const auto root{ std::filesystem::absolute("mod-source-test-work") };
		REQUIRE(root.parent_path() == std::filesystem::current_path());
		auto storage{ fs::native_volume(root, fs::native_volume_options(fs::access_mode::read_write, fs::missing_action::create)) };
		REQUIRE(storage);
		scope_exit cleanup([&] { std::error_code error; std::filesystem::remove_all(root, error); });
		auto appdata{ fs::mount("/", *storage) };
		auto sources{ fs::mount("/test-sources", *storage) };
		REQUIRE(appdata);
		REQUIRE(sources);
		scope_exit unload([] { Mod::unload(); });
		REQUIRE(fs::create_directories("/test-sources/mods/example"));
		const string info{ "name: example\nmod_version: 1.0.0\n" };
		REQUIRE(fs::write_all("/test-sources/mods/example/info.yaml", span<const u08>{ reinterpret_cast<const u08*>(info.data()), info.size() }));
		save_enabled_mods(fs::path{ "/enabled_mods.yaml" }, { { "example", { true, version{ 1, 0, 0 } } } });
		const auto initial_loads = completed_loads;
		const auto load_mods = [](span<const Mod::Source> sources) { Mod::load(sources); return Mod::result().get(); };
		const Mod::Source source[]{ { .path = "/test-sources/mods" } };
		for (int load{}; load < 2; ++load) {
			const auto error{ load_mods(source) };
			INFO(error.message);
			REQUIRE_FALSE(error);
			REQUIRE(Mod::progress().value() == 1.0f);
			REQUIRE(completed_loads == initial_loads + load + 1);
			REQUIRE(Mod::loaded().size() == 1);
			REQUIRE_FALSE(Mod::loaded().front().privileged);
			REQUIRE(Mod::loaded().front().location.text() == "/example");
			REQUIRE(fs::exists("/example/info.yaml"));
		}
				const auto valid_settings = string(R"(
 data:extend({
  {type='bool-setting', name='enabled', setting_type='startup', default_value=true},
  {type='string-setting', name='label', setting_type='runtime-per-user', default_value='abc', minimum_length=2, allowed_values={'abc','def'}}
 })
)");
		REQUIRE(fs::write_all("/test-sources/mods/example/settings.lua", span<const u08>(reinterpret_cast<const u08*>(valid_settings.data()), valid_settings.size())));
		const auto content = string("assert(settings.startup.enabled.value == true)");
		REQUIRE(fs::write_all("/test-sources/mods/example/data.lua", span<const u08>(reinterpret_cast<const u08*>(content.data()), content.size())));
		REQUIRE_FALSE(load_mods(source));
		REQUIRE(completed_loads == initial_loads + 3);
		const auto label = LoadSetting("example", "label");
		REQUIRE(label);
		REQUIRE(label->as<string>() == "abc");
		REQUIRE_FALSE(SaveSetting("example", "label", "def"));
		REQUIRE_FALSE(load_mods(source));
		REQUIRE(LoadSetting("example", "label")->as<string>() == "def");
		const auto invalid_settings = string("data:extend({{type='int-setting',name='bad',setting_type='startup',default_value=5,maximum_value=2}})");
		REQUIRE(fs::write_all("/test-sources/mods/example/settings.lua", span<const u08>(reinterpret_cast<const u08*>(invalid_settings.data()), invalid_settings.size())));
		REQUIRE(load_mods(source));
		REQUIRE(completed_loads == initial_loads + 4);
		REQUIRE(Mod::loaded().empty());
		REQUIRE_FALSE(fs::exists("/example/info.yaml"));
		REQUIRE_THROWS(Mod::result());
		const Mod::Source duplicate[]{ source[0], source[0] };
		REQUIRE(load_mods(duplicate));
		REQUIRE(Mod::loaded().empty());
		REQUIRE_FALSE(fs::exists("/example/info.yaml"));
		const Mod::Source missing[]{ { .path = "/test-sources/missing" } };
		const auto listing{ fs::list(missing[0].path) };
		REQUIRE_FALSE(listing);
		const auto error{ load_mods(missing) };
		REQUIRE(error.code == listing.error().code);
		const Mod::Source privileged[]{ { .path = source[0].path, .privileged = true } };
		REQUIRE(load_mods(privileged));
		REQUIRE_FALSE(fs::exists("/example/info.yaml"));
		for (const char* name : { "settings", "enabled_mods.yaml" }) {
			const string collision{ string("name: ") + name + "\nmod_version: 1.0.0\n" };
			REQUIRE(fs::write_all("/test-sources/mods/example/info.yaml", span<const u08>{ reinterpret_cast<const u08*>(collision.data()), collision.size() }));
			REQUIRE(load_mods(source));
			REQUIRE(Mod::loaded().empty());
		}
		const string malformed{ "name: [" };
		REQUIRE(fs::write_all("/test-sources/mods/example/info.yaml", span<const u08>{ reinterpret_cast<const u08*>(malformed.data()), malformed.size() }));
		REQUIRE(load_mods(source));
		REQUIRE(Mod::loaded().empty());

		REQUIRE(fs::write_all("/test-sources/mods/example/info.yaml", span<const u08>(reinterpret_cast<const u08*>(info.data()), info.size())));
		REQUIRE(fs::write_all("/test-sources/mods/example/settings.lua", span<const u08>(reinterpret_cast<const u08*>(valid_settings.data()), valid_settings.size())));
		std::promise<void> entered;
		std::promise<void> release;
		auto ready = entered.get_future();
		auto resume = release.get_future();
		initialize = [&] { entered.set_value(); resume.wait(); };
		scope_exit reset = scope_exit([&] { initialize = {}; });
		Mod::load(source);
		auto result = Mod::result();
		ready.wait();
		CHECK(Mod::progress().value() >= .98f);
		CHECK(Mod::progress().value() < 1.0f);
		CHECK(result.wait_for(std::chrono::seconds(0)) == std::future_status::timeout);
		Mod::cancel();
		release.set_value();
		CHECK(result.get().code == generic_errc::cancelled);
		CHECK(Mod::progress().value() == 1.0f);
		CHECK(Mod::loaded().empty());
		CHECK_FALSE(fs::exists("/example/info.yaml"));
	}

} // namespace lf::tests
