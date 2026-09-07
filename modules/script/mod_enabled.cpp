#include "leaf/script/mod_enabled.hpp"
#include "leaf/core/exception.hpp"
#include <algorithm>
#include <yaml-cpp/yaml.h>

namespace lf {
	std::unordered_map<string, ModEnabledInfo> load_enabled_mods(const fs::path& yaml_path) {
		std::unordered_map<string, ModEnabledInfo> result;
		report<vector<u08>> bytes = fs::read_all(yaml_path);
		if (!bytes) {
			if (bytes.error().code == make_error_code(fs::error_code::not_found)) { return result; }
			throw runtime_exception(bytes.error().message);
		}
		YAML::Node node = YAML::Load(string(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
		if (!node["mods"]) {
			return result;
		}
		for (const auto& entry : node["mods"]) {
			string name = entry.first.as<string>();
			bool enabled = entry.second["enabled"].as<bool>();
			version mod_version = version::from_string(entry.second["version"].as<string>());
			result[name] = ModEnabledInfo{ enabled, mod_version };
		}
		return result;
	}

	error save_enabled_mods(const fs::path& yaml_path, const std::unordered_map<string, ModEnabledInfo>& mods) {
		YAML::Emitter emitter;
		emitter << YAML::BeginMap;
		emitter << "mods" << YAML::BeginMap;
		vector<string> names;
		names.reserve(mods.size());
		for (const auto& [name, info] : mods) {
			names.emplace_back(name);
		}
		std::sort(names.begin(), names.end());
		for (const string& name : names) {
			const ModEnabledInfo& info = mods.at(name);
			emitter << name << YAML::BeginMap;
			emitter << "enabled" << info.enabled;
			emitter << "version"
					<< std::to_string(info.mod_version.major) + "." +
						   std::to_string(info.mod_version.minor) + "." +
						   std::to_string(info.mod_version.patch);
			emitter << YAML::EndMap;
		}
		emitter << YAML::EndMap;
		emitter << YAML::EndMap;
		const string output(emitter.c_str());
		const auto* bytes = reinterpret_cast<const u08*>(output.data());
		if (report<void> created = fs::create_directories(yaml_path.parent()); !created) {
			return created.error();
		}
		if (report<void> result = fs::write_all(yaml_path, span<const u08>(bytes, output.size())); !result) {
			return result.error();
		}
		return {};
	}

	error sync_enabled_mods(const fs::path& yaml_path, const vector<ModInfo>& discovered_mods) {
		auto enabled_mods = load_enabled_mods(yaml_path);
		// Add new mods as disabled, update versions, remove missing mods
		std::unordered_map<string, ModEnabledInfo> updated;
		for (const auto& mod : discovered_mods) {
			auto it = enabled_mods.find(mod.name);
			if (it != enabled_mods.end()) {
				// Update version if changed
				updated[mod.name] = ModEnabledInfo{ it->second.enabled, mod.mod_version };
			} else {
				updated[mod.name] = ModEnabledInfo{ mod.privileged, mod.mod_version };
			}
		}
		return save_enabled_mods(yaml_path, updated);
	}

	error set_mod_enabled(const fs::path& yaml_path, const string& mod_name, bool enabled) {
		auto enabled_mods = load_enabled_mods(yaml_path);
		auto it = enabled_mods.find(mod_name);
		if (it != enabled_mods.end()) {
			it->second.enabled = enabled;
			return save_enabled_mods(yaml_path, enabled_mods);
		}
		return { generic_errc::input_error, lf::format("unknown mod '{}'", mod_name) };
	}
} // namespace lf
