#include "leaf/script/mod_loader.hpp"

#include "leaf/core/logging.hpp"
#include "leaf/script/localization.hpp"
#include "leaf/script/mod_enabled.hpp"
#include "leaf/resource/prototypes/texture.hpp"
#include "leaf/resource/registry.hpp"
#include "leaf/script/settings.hpp"
#include "leaf/core/filesystem.hpp"
#include "leaf/core/scope.hpp"

#include <leaf/graphics/graphics.hpp>
#include <leaf/graphics/queue.hpp>

#include <sol/sol.hpp>
#include <sol/utility/is_integer.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace lf {
	object Options;

	namespace detail {
		struct ModSettings {
			std::unordered_map<string, object> values;
			std::unordered_map<string, string> input;
		};

		vector<ModInfo> loaded_mods;
		vector<fs::mapping> mod_mappings;
		dict loaded_startup_settings;
		std::unordered_map<string, ModSettings> loaded_settings;

		fs::path settings_path(string_view mod_name) {
			return fs::path("/settings").append(string(mod_name.empty() ? "core" : mod_name) + ".yaml");
		}

		error write_mod_settings(string_view mod_name) {
			const fs::path path = settings_path(mod_name);
			if (report<void> created = fs::create_directories(path.parent()); !created) {
				return created.error();
			}
			const ModSettings& settings = loaded_settings[string(mod_name)];
			YAML::Emitter out;
			out << YAML::BeginMap;
			out << YAML::Key << "settings" << YAML::Value << YAML::BeginMap;
			for (const auto& [name, value] : settings.values) {
				out << YAML::Key << name << YAML::Value;
				EmitYaml(out, value);
			}
			out << YAML::EndMap;
			out << YAML::Key << "input" << YAML::Value << YAML::BeginMap;
			for (const auto& [action, key] : settings.input) {
				out << YAML::Key << action << YAML::Value << key;
			}
			out << YAML::EndMap;
			out << YAML::EndMap;
			string text(out.c_str());
			const auto* bytes = reinterpret_cast<const u08*>(text.data());
			if (auto err = fs::write_all(path, span<const u08>(bytes, text.size())); !err) {
				return err.error();
			}
			return {};
		}

		report<ModSettings> read_mod_settings(string_view mod_name) {
			ModSettings settings;
			const fs::path path = settings_path(mod_name);
			if (!fs::exists(path)) {
				return settings;
			}

			try {
				report<vector<u08>> bytes = fs::read_all(path);
				if (!bytes) {
					return unexpected(bytes.error());
				}
				YAML::Node root = YAML::Load(string(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
				if (YAML::Node values = root["settings"]) {
					for (const auto& entry : values) {
						settings.values[entry.first.as<string>()] = ObjectFromYaml(entry.second);
					}
				}
				if (YAML::Node input = root["input"]) {
					for (const auto& entry : input) {
						settings.input[entry.first.as<string>()] = entry.second.as<string>();
					}
				}
			} catch (const YAML::Exception& e) {
				return unexpected(error(generic_errc::parse_error, lf::format("loading '{}': {}", path.text(), e.what())));
			}
			return settings;
		}

		error load_mod_settings_cache(string_view mod_name) {
			auto settings = read_mod_settings(mod_name);
			if (!settings) {
				return settings.error();
			}
			loaded_settings[string(mod_name)] = std::move(*settings);
			return error::no_error;
		}
	} // namespace detail

	class DataScriptRunner {
	  public:
		explicit DataScriptRunner(sol::state& lua) : lua(lua) {}

		void run_file(const fs::path& path) {
			report<string> source = expanded_source(path);
			if (!source) {
				throw runtime_exception(source.error().message);
			}

			sol::protected_function_result result = lua.safe_script(*source, sol::script_pass_on_error);
			if (!result.valid()) {
				sol::error script_error = result;
				throw runtime_exception(script_error.what());
			}
		}

	  private:
		sol::state& lua;

		static bool include_line(string_view line, string& path) {
			size_t cursor = 0;
			while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) {
				++cursor;
			}
			constexpr string_view prefix = "include";
			if (line.substr(cursor, prefix.size()) != prefix) {
				return false;
			}
			cursor += prefix.size();
			while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) {
				++cursor;
			}
			if (cursor >= line.size() || line[cursor++] != '(') {
				return false;
			}
			while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) {
				++cursor;
			}
			if (cursor >= line.size() || (line[cursor] != '"' && line[cursor] != '\'')) {
				return false;
			}
			char quote = line[cursor++];
			size_t end = line.find(quote, cursor);
			if (end == string_view::npos) {
				return false;
			}
			path = string(line.substr(cursor, end - cursor));
			return true;
		}

		static report<string> expanded_source(const fs::path& path) {
			report<vector<u08>> bytes = fs::read_all(path);
			if (!bytes) {
				return unexpected(bytes.error());
			}
			string source(reinterpret_cast<const char*>(bytes->data()), bytes->size());

			string expanded;
			size_t line_begin = 0;
			while (line_begin <= source.size()) {
				size_t line_end = source.find('\n', line_begin);
				if (line_end == string::npos) {
					line_end = source.size();
				}
				string_view line = string_view(source).substr(line_begin, line_end - line_begin);
				string include_path;
				if (include_line(line, include_path)) {
					fs::path include_file;
					if (include_path.starts_with('/')) {
						report<fs::path> parsed = fs::path::parse(include_path);
						if (!parsed) {
							return unexpected(parsed.error());
						}
						include_file = *parsed;
					} else {
						report<fs::path> parsed = fs::path::parse(include_path);
						if (!parsed) {
							return unexpected(parsed.error());
						}
						include_file = path.parent().append(*parsed);
					}
					report<string> included = expanded_source(include_file);
					if (!included) {
						return unexpected(included.error());
					}
					expanded += *included;
					if (!expanded.empty() && expanded.back() != '\n') {
						expanded += '\n';
					}
				} else {
					expanded += line;
					if (line_end < source.size()) {
						expanded += '\n';
					}
				}
				if (line_end == source.size()) {
					break;
				}
				line_begin = line_end + 1;
			}
			return expanded;
		}
	};

	string operator_to_string(ModDependency::Operator op) {
		switch (op) {
		case ModDependency::Operator::Equal: return "==";
		case ModDependency::Operator::NotEqual: return "!=";
		case ModDependency::Operator::Greater: return ">";
		case ModDependency::Operator::GreaterEqual: return ">=";
		case ModDependency::Operator::Less: return "<";
		case ModDependency::Operator::LessEqual: return "<=";
		case ModDependency::Operator::Any: return "any";
		default: return "?";
		}
	}
	const char* sol_type_name(sol::type t) {
		switch (t) {
		case sol::type::lua_nil: return "nil";
		case sol::type::boolean: return "boolean";
		case sol::type::number: return "number";
		case sol::type::string: return "string";
		case sol::type::table: return "table";
		case sol::type::function: return "function";
		case sol::type::userdata: return "userdata";
		case sol::type::lightuserdata: return "lightuserdata";
		case sol::type::thread: return "thread";
		default: return "unknown";
		}
	}
	string sol_object_to_string(const sol::object& v) {
		using sol::type;
		switch (v.get_type()) {
		case type::lua_nil: return "nil";
		case type::boolean: return v.as<bool>() ? "true" : "false";
		case type::number: return lf::format("{}", v.as<double>());
		case type::string: return "\"" + v.as<string>() + "\"";
		case type::table: {
			string result;
			for (auto& [key, value] : v.as<sol::table>()) {
				result += "[" + sol_object_to_string(key) + "] = " + sol_object_to_string(value) + ", ";
			}
			return result;
		}
		default: return lf::format("<{}>", sol_type_name(v.get_type()));
		}
	}

	string sol_setting_to_string(const sol::object& v) {
		using sol::type;
		switch (v.get_type()) {
		case type::boolean: return v.as<bool>() ? "true" : "false";
		case type::number: return lf::format("{}", v.as<double>());
		case type::string: return v.as<string>();
		default: return sol_object_to_string(v);
		}
	}

	object sol_to_object(const sol::object& v) {
		using sol::type;
		switch (v.get_type()) {
		case type::lua_nil: return object();
		case type::boolean: return object(v.as<bool>());
		case type::number: {
			if (sol::utility::is_integer(v)) {
				const lua_Integer integer{ v.as<lua_Integer>() };
				if constexpr (std::is_signed_v<lua_Integer>) {
					if constexpr (std::numeric_limits<lua_Integer>::digits > std::numeric_limits<i64>::digits) {
						if (integer < static_cast<lua_Integer>(std::numeric_limits<i64>::min()) ||
							integer > static_cast<lua_Integer>(std::numeric_limits<i64>::max())) {
							throw runtime_exception(lf::format("lua integer '{}' is out of range for object", integer));
						}
					}
					return object(static_cast<i64>(integer));
				} else {
					if constexpr (std::numeric_limits<lua_Integer>::digits <= std::numeric_limits<i64>::digits) {
						return object(static_cast<i64>(integer));
					} else if (integer <= static_cast<lua_Integer>(std::numeric_limits<i64>::max())) {
						return object(static_cast<i64>(integer));
					} else {
						return object(static_cast<u64>(integer));
					}
				}
			}
			return object(v.as<double>());
		}
		case type::string: return object(v.as<string>());
		case type::table: {
			sol::table t = v.as<sol::table>();
			bool seen_string_keys = false;
			bool seen_number_keys = false;
			for (const auto& kv : t) {
				sol::type kt = kv.first.get_type();
				if (kt == type::string) {
					seen_string_keys = true;
				} else if (kt == type::number) {
					seen_number_keys = true;
				} else {
					throw runtime_exception(lf::format("invalid table key type '{}' when converting lua table to object", sol_type_name(kt)));
				}
				if (seen_string_keys && seen_number_keys) {
					throw runtime_exception("mixed string and numeric keys in lua table are not supported: " + sol_object_to_string(t));
				}
			}
			if (seen_number_keys) {
				struct Entry {
					usize index;
					object val;
				};
				std::vector<Entry> entries;
				entries.reserve(t.size());
				for (const auto& kv : t) {
					usize index{};
					if (sol::utility::is_integer(kv.first)) {
						const lua_Integer integer{ kv.first.as<lua_Integer>() };
						if constexpr (std::is_signed_v<lua_Integer>) {
							if (integer < 1) {
								throw runtime_exception(lf::format("list index '{}' must be >= 1", integer));
							}
						}
						if constexpr (std::numeric_limits<lua_Integer>::digits > std::numeric_limits<usize>::digits) {
							if (integer > static_cast<lua_Integer>(std::numeric_limits<usize>::max())) {
								throw runtime_exception(lf::format("list index '{}' is out of range", integer));
							}
						}
						index = static_cast<usize>(integer);
					} else {
						const double number{ kv.first.as<double>() };
						if (!std::isfinite(number) || std::trunc(number) != number || number < 1 ||
							number >= static_cast<double>(std::numeric_limits<usize>::max())) {
							throw runtime_exception(lf::format("list index '{}' must be a finite positive integer in range", number));
						}
						index = static_cast<usize>(number);
					}
					entries.emplace_back(Entry{ index, sol_to_object(kv.second) });
				}
				lf::list lst;
				lst.resize(entries.size());
				for (const auto& e : entries) {
					if (e.index > lst.size()) {
						throw runtime_exception("sparse lua lists are not supported");
					}
					lst[e.index - 1] = e.val;
				}
				return object(lst);
			}
			// dict
			lf::dict d;
			for (const auto& kv : t) {
				string k = kv.first.as<string>();
				d.emplace(k, sol_to_object(kv.second));
			}
			return object(d);
		}
		default:
			throw runtime_exception(lf::format("unsupported lua type '{}' when converting to object", sol_type_name(v.get_type())));
		}
	}
	string version_to_string(const version& v) {
		string s =
			std::to_string(v.major) + "." + std::to_string(v.minor) + "." + std::to_string(v.patch);
		if (v.snapshot != 0) {
			s += "-" + std::to_string(v.snapshot);
		}
		return s;
	}
	const char* dependency_type_name(ModDependency::Type type) {
		switch (type) {
		case ModDependency::Type::Required: return "required";
		case ModDependency::Type::Optional: return "optional";
		case ModDependency::Type::Forbidden: return "forbidden";
		default: return "unknown";
		}
	}
	string dependency_to_string(const ModDependency& dependency) {
		return lf::format("{} {} {} {}", dependency_type_name(dependency.type), dependency.name, operator_to_string(dependency.op), version_to_string(dependency.required_version));
	}
	string dependencies_to_string(const vector<ModDependency>& dependencies) {
		if (dependencies.empty()) {
			return "none";
		}
		string result;
		for (size_t i = 0; i < dependencies.size(); ++i) {
			if (i != 0) {
				result += "; ";
			}
			result += dependency_to_string(dependencies[i]);
		}
		return result;
	}
	string prototype_counts_to_string() {
		string result;
		for (const auto& fn : PrototypeTypeRegistry::functions) {
			const size_t count = fn.count();
			if (count == 0) {
				continue;
			}
			if (!result.empty()) {
				result += ", ";
			}
			result += string(fn.type()) + "=" + std::to_string(count);
		}
		return result.empty() ? "none" : result;
	}
	string mod_names_to_string(span<const ModInfo> mods) {
		if (mods.empty()) {
			return "none";
		}
		string result;
		for (size_t i = 0; i < mods.size(); ++i) {
			if (i != 0) {
				result += ", ";
			}
			result += mods[i].name;
		}
		return result;
	}
	void set_lua_value(sol::table table, string_view name, const object& value) {
		string key(name);
		if (value.is<bool>()) {
			table[key] = value.get<bool>();
		} else if (value.is<i64>()) {
			table[key] = value.get<i64>();
		} else if (value.is<u64>()) {
			table[key] = value.get<u64>();
		} else if (value.is<f64>()) {
			table[key] = value.get<f64>();
		} else if (value.is<string>()) {
			table[key] = value.get<string>();
		}
	}

	void initialize_prototype_lua(sol::state& lua) {
		lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
		lua.script(R"(
data = {}
data["raw"] = {}
function data:extend(prototypes)
	for i = 1, #prototypes do
		local prototype = prototypes[i]
		if prototype.mod == nil then
			prototype.mod = __leaf_current_mod
		end
		local type_table = self.raw[prototype.type]
		if type_table == nil then
			error("data.raw[" .. tostring(prototype.type) .. "] does not exist")
		end
		if type_table[prototype.name] ~= nil then
			error("duplicate prototype '" .. tostring(prototype.type) .. "/" .. tostring(prototype.name) .. "'")
		end
		type_table[prototype.name] = prototype
	end
end
option = {}
option["scene"] = {}
settings = {}
settings["startup"] = {}
)");
		for (auto& type : PrototypeTypeRegistry::functions) {
			lua["data"]["raw"][type.type()] = lua.create_table();
		}
		sol::table startup = lua["settings"]["startup"];
		for (const auto& [name, value] : detail::loaded_startup_settings) {
			sol::table setting = lua.create_table();
			set_lua_value(setting, "value", value);
			startup[name] = setting;
		}
	}

	const ModInfo* find_mod(span<const ModInfo> mods, string_view name) {
		for (const ModInfo& mod : mods) {
			if (mod.name == name) {
				return &mod;
			}
		}
		return nullptr;
	}

	error load_prototype_script(sol::state& lua, const ModInfo& mod, string_view file_name, bool required = false) {
		auto path = mod.location.append(file_name);
		if (!fs::exists(path)) {
			if (required) {
				log::Error("{}", lf::format("[mod-loader] missing required script: {}/{} ({})", mod.name, file_name, path.text()));
				return error(generic_errc::not_found, "Missing required prototype script " + mod.name + "/" + string(file_name));
			}
			log::Debug("{}", lf::format("[mod-loader] script skipped: {}/{} (missing optional)", mod.name, file_name));
			return error::no_error;
		}

		try {
			DataScriptRunner runner(lua);
			lua["__leaf_current_mod"] = mod.name;
			log::Debug("{}", lf::format("[mod-loader] script begin: {}/{} ({})", mod.name, file_name, path.text()));
			runner.run_file(path);
			log::Info("{}", lf::format("[mod-loader] script: \"{}\"/{}", mod.name, file_name));
			return error::no_error;
		} catch (const lf::exception& e) {
			log::Error("{}", lf::format("[mod-loader] error: {}/{}: {}", mod.name, file_name, e.what()));
			return error(generic_errc::parse_error, "Failed to execute " + mod.name + "/" + string(file_name) + ": " + e.what());
		}
	}

	void initialize_settings_lua(
		sol::state& lua,
		vector<std::pair<string, object>>& setting_defaults,
		vector<std::pair<string, string>>& input_defaults
	) {
		lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
		lua.set_function("__leaf_set_local_input_binding", [&input_defaults](string_view action, string_view key) {
			input_defaults.emplace_back(string(action), string(key));
		});
		lua.set_function("__leaf_declare_setting", [&setting_defaults, &input_defaults](string_view type, string_view name, string_view setting_type, sol::object default_value) {
			if (name.empty()) {
				throw runtime_exception(lf::format("{} is missing name", type));
			}
			if (default_value.get_type() == sol::type::lua_nil) {
				throw runtime_exception(lf::format("{} '{}' is missing default_value", type, name));
			}

			object value = sol_to_object(default_value);
			if (setting_type == "startup") {
				detail::loaded_startup_settings[string(name)] = value;
			}
			if (type == "input-setting") {
				if (setting_type != "runtime-per-user") {
					throw runtime_exception(lf::format("input-setting '{}' must use setting_type='runtime-per-user'", name));
				}
				input_defaults.emplace_back(string(name), value.as<string>());
			} else if (setting_type == "runtime-per-user") {
				setting_defaults.emplace_back(string(name), sol_to_object(default_value));
			}
		});
		lua.script(R"(
data = {}
local setting_types = {
	["bool-setting"] = true,
	["int-setting"] = true,
	["double-setting"] = true,
	["string-setting"] = true,
	["input-setting"] = true,
}
local setting_scopes = {
	["startup"] = true,
	["runtime-global"] = true,
	["runtime-per-user"] = true,
}

function data:extend(settings)
	for i = 1, #settings do
		local setting = settings[i]
		if setting_types[setting.type] ~= true then
			error("unknown setting type '" .. tostring(setting.type) .. "'")
		end
		if setting.name == nil then
			error(tostring(setting.type) .. " is missing name")
		end
		if setting.setting_type == nil then
			error(tostring(setting.type) .. " '" .. tostring(setting.name) .. "' is missing setting_type")
		end
		if setting_scopes[setting.setting_type] ~= true then
			error(tostring(setting.type) .. " '" .. tostring(setting.name) .. "' has invalid setting_type '" .. tostring(setting.setting_type) .. "'")
		end
		if setting.type == "int-setting" or setting.type == "double-setting" then
			if setting.minimum_value ~= nil and setting.default_value < setting.minimum_value then
				error(tostring(setting.type) .. " '" .. tostring(setting.name) .. "' default_value is below minimum_value")
			end
			if setting.maximum_value ~= nil and setting.default_value > setting.maximum_value then
				error(tostring(setting.type) .. " '" .. tostring(setting.name) .. "' default_value is above maximum_value")
			end
		end
		if setting.type == "string-setting" then
			local default = tostring(setting.default_value)
			if setting.minimum_length ~= nil and #default < setting.minimum_length then
				error("string-setting '" .. tostring(setting.name) .. "' default_value is shorter than minimum_length")
			end
			if setting.maximum_length ~= nil and #default > setting.maximum_length then
				error("string-setting '" .. tostring(setting.name) .. "' default_value is longer than maximum_length")
			end
			if setting.allowed_values ~= nil then
				if type(setting.allowed_values) ~= "table" then
					error("string-setting '" .. tostring(setting.name) .. "' allowed_values must be a table")
				end
				local allowed = false
				for _, value in ipairs(setting.allowed_values) do
					if default == tostring(value) then
						allowed = true
						break
					end
				end
				if not allowed then
					error("string-setting '" .. tostring(setting.name) .. "' default_value is not in allowed_values")
				end
			end
		end
		__leaf_declare_setting(tostring(setting.type), tostring(setting.name), tostring(setting.setting_type), setting.default_value)
	end
end
)");
	}

	error sync_loaded_mod_settings(span<const ModInfo> mods) {
		for (const ModInfo& mod : mods) {
			if (error err = detail::load_mod_settings_cache(mod.name)) {
				return err.add_context(lf::format("loading settings for '{}'", mod.name));
			}
			log::Debug("{}", lf::format("[mod-loader] settings sync: {}", mod.name));
			vector<std::pair<string, object>> setting_defaults;
			vector<std::pair<string, string>> input_defaults;
			sol::state lua;
			initialize_settings_lua(lua, setting_defaults, input_defaults);
			if (error err = load_prototype_script(lua, mod, "settings.lua")) {
				return err.add_context("loading settings defaults");
			}
			for (const auto& [action, key] : input_defaults) {
				detail::loaded_settings[mod.name].input.try_emplace(action, key);
			}
			for (const auto& [name, value] : setting_defaults) {
				detail::loaded_settings[mod.name].values.try_emplace(name, value);
			}
			if (error err = detail::write_mod_settings(mod.name)) {
				return err.add_context(lf::format("saving settings for '{}'", mod.name));
			}
		}
		return error::no_error;
	}

	bool version_compare(const version& a, const version& b, ModDependency::Operator op) {
		auto cmp = [](const version& lhs, const version& rhs) {
			if (lhs.major != rhs.major) {
				return lhs.major < rhs.major ? -1 : 1;
			}
			if (lhs.minor != rhs.minor) {
				return lhs.minor < rhs.minor ? -1 : 1;
			}
			if (lhs.patch != rhs.patch) {
				return lhs.patch < rhs.patch ? -1 : 1;
			}
			if (lhs.snapshot != rhs.snapshot) {
				return lhs.snapshot < rhs.snapshot ? -1 : 1;
			}
			return 0;
		};
		int res = cmp(a, b);
		switch (op) {
		case ModDependency::Operator::Equal: return res == 0;
		case ModDependency::Operator::NotEqual: return res != 0;
		case ModDependency::Operator::Greater: return res > 0;
		case ModDependency::Operator::GreaterEqual: return res > 0 || res == 0;
		case ModDependency::Operator::Less: return res < 0;
		case ModDependency::Operator::LessEqual: return res < 0 || res == 0;
		case ModDependency::Operator::Any: return true;
		default: return false;
		}
	}
	error sort_mods_by_dependency(vector<ModInfo>& mods) {
		log::Debug("{}", lf::format("[mod-loader] dependency resolution begin: {} enabled mod(s)", mods.size()));
		std::unordered_map<string, size_t> mod_index;
		for (size_t i = 0; i < mods.size(); ++i) {
			mod_index[mods[i].name] = i;
			log::Debug("{}", lf::format("[mod-loader] dependency input: {} v{} deps=[{}]", mods[i].name, version_to_string(mods[i].mod_version), dependencies_to_string(mods[i].dependencies)));
		}

		std::vector<std::vector<size_t>> graph(mods.size());
		std::vector<string> errors;
		for (size_t i = 0; i < mods.size(); ++i) {
			for (const auto& dep : mods[i].dependencies) {
				auto it = mod_index.find(dep.name);
				if (dep.type == ModDependency::Type::Forbidden) {
					if (it != mod_index.end()) {
						// Check forbidden version constraint
						if (version_compare(mods[it->second].mod_version, dep.required_version, dep.op)) {
							string msg =
								"Mod '" + mods[i].name + "' forbids mod '" + dep.name +
								"' (version " + version_to_string(mods[it->second].mod_version) +
								") matching forbidden constraint '" + operator_to_string(dep.op) +
								" " + version_to_string(dep.required_version) + "'.";
							errors.emplace_back(msg);
						}
					}
					continue;
				}
				if (dep.type == ModDependency::Type::Required ||
					dep.type == ModDependency::Type::Optional) {
					if (it != mod_index.end()) {
						// Check required/optional version constraint
						if (!version_compare(mods[it->second].mod_version, dep.required_version, dep.op)) {
							string msg = "Mod '" + mods[i].name + "' requires mod '" + dep.name +
										 " " + operator_to_string(dep.op) + " version " +
										 version_to_string(dep.required_version) + "' but '" +
										 dep.name + "' is '" +
										 version_to_string(mods[it->second].mod_version) + "'.";
							errors.emplace_back(msg);
						}
						// Reverse edge: dependency -> mod
						graph[it->second].emplace_back(i);
						log::Debug("{}", lf::format("[mod-loader] dependency edge: {} -> {}", dep.name, mods[i].name));
					} else if (dep.type == ModDependency::Type::Required) {
						// Required dependency missing
						string msg = "Mod '" + mods[i].name + "' requires mod '" + dep.name +
									 "' but it is missing.";
						errors.emplace_back(msg);
					}
				}
			}
		}

		// If any errors, return all
		if (!errors.empty()) {
			string all_errors;
			for (const auto& e : errors) {
				log::Error("{}", lf::format("[mod-loader] dependency error: {}", e));
				all_errors += e + "\n";
			}
			return error(generic_errc::unknown, all_errors);
		}

		// Topological sort with cycle detection
		std::vector<bool> visited(mods.size(), false);
		std::vector<bool> on_stack(mods.size(), false);
		std::vector<size_t> order;
		std::vector<size_t> cycle;
		std::function<bool(size_t)> dfs = [&](size_t u) {
			visited[u] = true;
			on_stack[u] = true;
			for (size_t v : graph[u]) {
				if (!visited[v]) {
					if (dfs(v)) {
						if (cycle.empty() || cycle.front() != v) {
							cycle.emplace_back(v);
						}
						return true;
					}
				} else if (on_stack[v]) {
					// Cycle detected
					cycle.emplace_back(v);
					return true;
				}
			}
			on_stack[u] = false;
			order.emplace_back(u);
			return false;
		};
		for (size_t i = 0; i < mods.size(); ++i) {
			if (!visited[i]) {
				if (dfs(i)) {
					// Cycle detected, build error message
					string msg = "Circular dependency detected: ";
					for (size_t idx : cycle) {
						msg += mods[idx].name + " -> ";
					}
					msg += mods[cycle.front()].name;
					return error(generic_errc::unknown).add_context(msg);
				}
			}
		}

		// Reorder mods vector in dependency order
		std::reverse(order.begin(), order.end());
		vector<ModInfo> sorted;
		sorted.reserve(mods.size());
		for (size_t idx : order) {
			sorted.emplace_back(std::move(mods[idx]));
		}
		mods = std::move(sorted);
		log::Debug("{}", "[mod-loader] dependency resolution complete");
		for (size_t i = 0; i < mods.size(); ++i) {
			log::Debug("{}", lf::format("[mod-loader] load-order[{}]: {} v{}", i + 1, mods[i].name, version_to_string(mods[i].mod_version)));
		}
		return error::no_error;
	}

	error LoadDataRaw(const sol::state& lua) {
		object data_raw_obj = sol_to_object(lua["data"]["raw"]);
		if (!data_raw_obj.is<dict>()) {
			return error(generic_errc::type_mismatch, "data.raw must be a table/dict");
		}
		dict& data_raw = data_raw_obj.get<dict>();

		for (auto& fn : lf::PrototypeTypeRegistry::functions) {
			auto it = data_raw.find(fn.type());
			if (it != data_raw.end()) {
				if (!it->second.is<dict>()) {
					return error(generic_errc::type_mismatch, "data.raw." + string(fn.type()) + " must be a table/dict");
				}
				dict& type_table = it->second.get<dict>();
				// Iterate in sorted name order so prototype ids are deterministic;
				// dict is an unordered map and its iteration order is unspecified.
				vector<string_view> names;
				names.reserve(type_table.size());
				for (const auto& [name, data] : type_table) {
					names.emplace_back(name);
				}
				std::sort(names.begin(), names.end());
				for (string_view name : names) {
					try {
						fn.create(name);
					} catch (const lf::error& e) {
						return error(e).add_context(lf::format("registering prototype '{}' (type:{})", name, fn.type()));
					} catch (const lf::exception& e) {
						return error(generic_errc::parse_error, e.what()).add_context(lf::format("registering prototype '{}' (type:{})", name, fn.type()));
					}
				}
			}
		}

		for (auto& fn : lf::PrototypeTypeRegistry::functions) {
			auto it = data_raw.find(fn.type());
			if (it != data_raw.end()) {
				dict& type_table = it->second.get<dict>();
				vector<string_view> names;
				names.reserve(type_table.size());
				for (const auto& [name, data] : type_table) {
					names.emplace_back(name);
				}
				std::sort(names.begin(), names.end());
				for (string_view name : names) {
					const object& data = type_table.find(name)->second;
					if (!data.is<dict>()) {
						return error(generic_errc::type_mismatch, lf::format("data.raw[{}][{}] must be a table/dict", fn.type(), name));
					}
					try {
						fn.init(name, data.get<dict>());
					} catch (const lf::error& e) {
						return error(e).add_context(lf::format("creating prototype '{}' (type:{})", name, fn.type()));
					} catch (const lf::exception& e) {
						return error(generic_errc::parse_error, e.what()).add_context(lf::format("creating prototype '{}' (type:{})", name, fn.type()));
					}
				}
			}
		}
		return error::no_error;
	}

	error LoadOptions(const sol::state& lua, string_view mod_name) {
		sol::object option_obj = lua["option"];
		if (!option_obj.is<sol::table>()) {
			return error(generic_errc::missing_field, lf::format("missing required global 'option' in mod '{}'", mod_name));
		}

		Options[mod_name] = sol_to_object(option_obj);
		return error::no_error;
	}

	error mod::Load(span<const Source> sources, Progress progress) {
#define CANCELLED_ERROR error(generic_errc::cancelled, "startup cancelled")


		log::Info("{}", "[mod-loader] loading mods");
		Unload();
		scope_exit rollback{ [] { Unload(); } };
		Options = dict{};
		log::Debug("{}", lf::format("[mod-loader] reset runtime state: prototype_types={}", PrototypeTypeRegistry::functions.size()));

		progress.add("collecting-sorting-mods");
		progress.add("loading-settings");
		progress.add("loading-localization");
		progress.add("loading-mod-prototypes");

		progress.add("finalizing-startup");
		Progress phase = progress();
		phase.add_total(3);
		if (progress.cancelled()) {
			return CANCELLED_ERROR;
		}

		vector<ModInfo> mods;
		for (const Source& source : sources) {
			if (progress.cancelled()) {
				return CANCELLED_ERROR;
			}
			auto entries{ fs::list(source.path) };
			if (!entries) {
				return entries.error().add_context(lf::format("discovering mods in '{}'", source.path.text()));
			}
			for (const fs::directory_entry& entry : *entries) {
				if (entry.status().type() != fs::node_type::directory) {
					continue;
				}
				if (progress.cancelled()) {
					return CANCELLED_ERROR;
				}
				const auto mod_dir{ source.path.append(entry.name()) };
				auto directory{ fs::open_volume(mod_dir) };
				if (!directory) { return directory.error(); }
				if (source.privileged && directory->access() != fs::access_mode::read_only) {
					return error{ generic_errc::invalid_state, lf::format("privileged mod source '{}' must be read-only", mod_dir.text()) };
				}
				const auto info_path{ mod_dir.append("info.yaml") };
				const auto info_status{ fs::status(info_path) };
				if (!info_status) {
					if (info_status.error().code == lf::make_error_code(fs::error_code::not_found)) {
						continue;
					}
					return info_status.error();
				}
				auto parsed{ parse_mod_info(info_path.text(), source.privileged) };
				if (!parsed) { return parsed.error().add_context(lf::format("reading '{}'", info_path.text())); }
				ModInfo info{ std::move(*parsed) };
				if (find_mod(mods, info.name)) {
					return error{ generic_errc::conflict, lf::format("duplicate mod '{}'", info.name) };
				}
				log::Debug("[mod-loader] discovered {} v{} at {}", info.name, version_to_string(info.mod_version), info.location.text());
				mods.emplace_back(std::move(info));
			}
		}
		if (progress.cancelled()) {
			return CANCELLED_ERROR;
		}
		log::Info("{}", lf::format("[mod-loader] discovered {} mod(s): {}", mods.size(), mod_names_to_string(mods)));

		phase.advance();
		if (progress.cancelled()) {
			return CANCELLED_ERROR;
		}
		fs::path enabled_mods_path("/enabled_mods.yaml");
		if (const auto error{ sync_enabled_mods(enabled_mods_path, mods) }) { return error; }
		auto enabled_mods = load_enabled_mods(enabled_mods_path);
		log::Debug("{}", lf::format("[mod-loader] enabled mods file={} entries={}", enabled_mods_path.text(), enabled_mods.size()));
		if (const auto status{ fs::status(enabled_mods_path) }; !status) { return status.error(); }
		if (const auto created{ fs::create_directories("/settings") }; !created) { return created.error(); }
		for (ModInfo& mod : mods) {
			auto directory{ fs::open_volume(mod.location) };
			if (!directory) { return directory.error(); }
			const auto destination{ fs::path("/").append(mod.name) };
			const auto status{ fs::status(destination) };
			if (status) {
				return error{ generic_errc::conflict, lf::format("mod namespace '{}' is already in use", destination.text()) };
			}
			if (status.error().code != lf::make_error_code(fs::error_code::not_found) &&
				status.error().code != lf::make_error_code(fs::error_code::not_mapped)) { return status.error(); }
			auto mounted{ fs::mount(destination, *directory) };
			if (!mounted) { return mounted.error(); }
			detail::mod_mappings.emplace_back(std::move(*mounted));
			mod.location = destination;
		}

		vector<ModInfo> enabled_mod_list;
		for (const auto& mod : mods) {
			auto it = enabled_mods.find(mod.name);
			if (it != enabled_mods.end() && it->second.enabled) {
				log::Debug("{}", lf::format("[mod-loader] selected: {} v{} enabled=true", mod.name, version_to_string(mod.mod_version)));
				enabled_mod_list.emplace_back(mod);
			} else {
				log::Debug("{}", lf::format("[mod-loader] selected: {} v{} enabled=false", mod.name, version_to_string(mod.mod_version)));
			}
		}
		log::Info("{}", lf::format("[mod-loader] enabled {} mod(s): {}", enabled_mod_list.size(), mod_names_to_string(enabled_mod_list)));

		phase.advance();
		if (progress.cancelled()) {
			return CANCELLED_ERROR;
		}
		error sort_result = sort_mods_by_dependency(enabled_mod_list);
		if (sort_result) {
			return sort_result;
		}
		phase.advance();

		log::Info("{}", "[mod-loader] load order:");
		for (size_t i = 0; i < enabled_mod_list.size(); ++i) {
			if (progress.cancelled()) {
				return CANCELLED_ERROR;
			}
			const auto& mod = enabled_mod_list[i];
			log::Info("{}", lf::format("[mod-loader]   {}. \"{}\" v{}", i + 1, mod.name, version_to_string(mod.mod_version)));
		}

		if (error err = detail::load_mod_settings_cache("core")) {
			return err.add_context("loading settings/core.yaml");
		}
		auto language_result = LoadSetting("core", "language", object("en-US"));
		if (!language_result) {
			return language_result.error().add_context("loading settings/core.yaml");
		}
		string selected_language = language_result->as<string>();
		log::Info("{}", lf::format("[mod-loader] language: {}", selected_language));

		phase = progress();
		phase.add_total(2);
		phase.advance();
		if (progress.cancelled()) {
			return CANCELLED_ERROR;
		}
		log::Debug("{}", "[mod-loader] loading mod settings");
		if (error settings_err = sync_loaded_mod_settings(span<const ModInfo>(enabled_mod_list.data(), enabled_mod_list.size()))) {
			return settings_err.add_context("syncing mod settings");
		}
		phase.advance();
		log::Info("{}", lf::format("[mod-loader] settings: {} startup setting(s)", detail::loaded_startup_settings.size()));

		phase = progress();
		phase.add_total(1);
		if (progress.cancelled()) {
			return CANCELLED_ERROR;
		}
		log::Info("{}", lf::format("[mod-loader] loading locale: {}", selected_language));
		error locale_error = LoadLocaleFiles(span<const ModInfo>(enabled_mod_list.data(), enabled_mod_list.size()), selected_language);
		if (locale_error) {
			return locale_error.add_context("loading locale files");
		}
		phase.advance();

		sol::state lua;
		initialize_prototype_lua(lua);
		phase = progress();
		phase.add_total(static_cast<u64>(enabled_mod_list.size() + 1));

		log::Info("{}", "[mod-loader] loading data scripts");
		for (size_t i = 0; i < enabled_mod_list.size(); ++i) {
			if (progress.cancelled()) {
				return CANCELLED_ERROR;
			}
			const auto& mod = enabled_mod_list[i];
			lua["option"] = lua.create_table();
			lua["option"]["scene"] = lua.create_table();
			log::Debug("{}", lf::format("[mod-loader] data.lua {}/{}: {}", i + 1, enabled_mod_list.size(), mod.name));
			if (error err = load_prototype_script(lua, mod, "data.lua")) {
				return err;
			}
			if (error err = LoadOptions(lua, mod.name)) {
				return err.add_context(lf::format("loading options from {}/data.lua", mod.name));
			}
			phase.advance();
		}

		if (progress.cancelled()) {
			return CANCELLED_ERROR;
		}
		log::Debug("{}", "[mod-loader] loaded mod options");
		detail::loaded_mods = enabled_mod_list;
		if (progress.cancelled()) {
			return CANCELLED_ERROR;
		}
		log::Info("{}", "[mod-loader] creating prototypes from data.raw");
		auto err = LoadDataRaw(lua);
		if (err) {
			return err.add_context("loading prototypes from mods' data.lua");
		}
		phase.advance();
		log::Debug("{}", lf::format("[mod-loader] prototypes: {}", prototype_counts_to_string()));

		phase = progress();
		phase.add_total(1);
		phase.advance();
#undef CANCELLED_ERROR
		rollback.release();
		return error::no_error;
	}

	const vector<ModInfo>& mod::Loaded() {
		return detail::loaded_mods;
	}

	report<object> LoadSetting(string_view mod_name, string_view name, object fallback) {
		const string mod_key(mod_name.empty() ? "core" : mod_name);
		auto mod = detail::loaded_settings.find(mod_key);
		if (mod == detail::loaded_settings.end()) {
			return fallback;
		}
		if (auto value = mod->second.values.find(string(name)); value != mod->second.values.end()) {
			return value->second;
		}
		return fallback;
	}

	error SaveSetting(string_view mod_name, string_view name, object value) {
		const string mod_key(mod_name.empty() ? "core" : mod_name);
		detail::loaded_settings[mod_key].values[string(name)] = std::move(value);
		return detail::write_mod_settings(mod_key);
	}

	error EnsureSetting(string_view mod_name, string_view name, object value) {
		const string mod_key(mod_name.empty() ? "core" : mod_name);
		detail::loaded_settings[mod_key].values.try_emplace(string(name), std::move(value));
		return error::no_error;
	}

	report<string> LoadInputSetting(string_view mod_name, string_view action) {
		const string mod_key(mod_name.empty() ? "core" : mod_name);
		auto mod = detail::loaded_settings.find(mod_key);
		if (mod == detail::loaded_settings.end()) {
			return string();
		}
		if (auto value = mod->second.input.find(string(action)); value != mod->second.input.end()) {
			return value->second;
		}
		return string();
	}

	error EnsureInputSetting(string_view mod_name, string_view action, string_view key) {
		const string mod_key(mod_name.empty() ? "core" : mod_name);
		detail::loaded_settings[mod_key].input.try_emplace(string(action), string(key));
		return error::no_error;
	}

	void mod::Unload() {
		for (const auto& func : PrototypeTypeRegistry::functions) {
			func.clear();
		}
		ClearLocalization();
		Options = {};
		detail::mod_mappings.clear();
		detail::loaded_mods.clear();
		detail::loaded_startup_settings.clear();
		detail::loaded_settings.clear();
	}
} // namespace lf


