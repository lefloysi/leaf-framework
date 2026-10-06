#pragma once
#include "leaf/core/filesystem.hpp"
#include "leaf/core/string.hpp"
#include "leaf/core/version.hpp"
#include "leaf/script/mod_info.hpp"
#include <unordered_map>

namespace lf {
	struct ModEnabledInfo {
		bool enabled;
		version mod_version;
	};

	// Loads enabled mods from YAML file
	std::unordered_map<string, ModEnabledInfo> load_enabled_mods(const fs::path& yaml_path);
	// Saves enabled mods to YAML file
	error save_enabled_mods(const fs::path& yaml_path, const std::unordered_map<string, ModEnabledInfo>& mods);
	// Synchronizes enabled mods file with discovered mods
	error sync_enabled_mods(const fs::path& yaml_path, const vector<ModInfo>& discovered_mods);
	// Set mod enabled/disabled
	error set_mod_enabled(const fs::path& yaml_path, const string& mod_name, bool enabled);
} // namespace lf
