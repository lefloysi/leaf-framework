#include "leaf/script/state.hpp"
#include <leaf/script/settings.hpp>
#include <leaf/script/mod_loader.hpp>
#include <leaf/script/mod_enabled.hpp>
#include <leaf/script/localization.hpp>
#include <leaf/core/exception.hpp>

#include <limits>

namespace lf {
	sol::object object_to_sol(sol::state_view state, const object& value) {
		return value.visit([&](const auto& item) -> sol::object {
			using T = std::decay_t<decltype(item)>;
			if constexpr (std::same_as<T, std::monostate>) {
				return sol::make_object(state, sol::nil);
			} else if constexpr (std::same_as<T, dict>) {
				auto result{ state.create_table() };
				for (const auto& [key, entry] : item) { result[key] = object_to_sol(state, entry); }
				return result;
			} else if constexpr (std::same_as<T, list>) {
				auto result{ state.create_table() };
				for (usize index{}; index < item.size(); ++index) { result[index + 1] = object_to_sol(state, item[index]); }
				return result;
			} else if constexpr (std::same_as<T, u64>) {
				if (item > static_cast<u64>(std::numeric_limits<lua_Integer>::max())) {
					throw runtime_exception(lf::format("u64 value '{}' is out of range for lua_Integer", item));
				}
				return sol::make_object(state, static_cast<lua_Integer>(item));
			} else {
				return sol::make_object(state, item);
			}
		});
	}

	error InstallScriptInterfaces(sol::state& state) {
		auto settings{ state.create_named_table("settings") };
		settings.set_function("get", [](const string& mod, const string& key, sol::object fallback, sol::this_state lua) {
			const auto value{ LoadSetting(mod, key, sol_to_object(fallback)) };
			if (!value) { throw runtime_exception(value.error().message); }
			return object_to_sol(sol::state_view{ lua }, *value);
		});
		settings.set_function("set", [](const string& mod, const string& key, sol::object value) {
			if (const auto error{ SaveSetting(mod, key, sol_to_object(value)) }) { throw runtime_exception(error.message); }
		});
		auto files{ state.create_named_table("fs") };
		files.set_function("exists", [](const string& path) { return fs::exists(path); });
		files.set_function("join", [](const string& base, const string& child) { return string{ (fs::path{ base } / child).text() }; });
		files.set_function("list", [](const string& path, sol::this_state lua) {
			const auto entries{ fs::list(path) };
			if (!entries) { throw runtime_exception(entries.error().message); }
			sol::state_view state{ lua };
			auto result{ state.create_table() };
			usize index{};
			for (const auto& entry : *entries) {
				result[++index] = state.create_table_with("name", string{ entry.name() }, "path", string{ (fs::path{ path } / entry.name()).text() }, "is_directory", entry.status().type() == fs::node_type::directory);
			}
			return result;
		});
		auto locale{ state.create_named_table("localization") };
		locale.set_function("language", [] { return string{ LoadedLanguage() }; });
		locale.set_function("set_language", [](const string& language) {
			if (const auto error{ SetLanguage(language) }) { throw runtime_exception(error.message); }
		});
		locale.set_function("get", [](const string& section, const string& key) { return Localize(section, key); });
		locale.set_function("languages", [](sol::this_state lua) {
			sol::state_view state{ lua };
			auto result{ state.create_table() };
			usize index{};
			for (const auto& language : AvailableLanguages()) { result[++index] = state.create_table_with("id", language.id, "name", language.name, "native_name", language.native_name); }
			return result;
		});
		auto mods{ state.create_named_table("mods") };
		mods.set_function("enabled", [](const string& path, sol::this_state lua) {
			sol::state_view state{ lua };
			auto result{ state.create_table() };
			for (const auto& [name, info] : load_enabled_mods(fs::path{ path })) {
				result[name] = state.create_table_with("enabled", info.enabled, "version", lf::format("{}.{}.{}", info.mod_version.major, info.mod_version.minor, info.mod_version.patch));
			}
			return result;
		});
		mods.set_function("set_enabled", [](const string& path, const string& name, bool enabled) {
			if (const auto error{ set_mod_enabled(fs::path{ path }, name, enabled) }) { throw runtime_exception(error.message); }
		});
		return {};
	}
}
