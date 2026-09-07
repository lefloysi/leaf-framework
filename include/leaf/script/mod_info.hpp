#pragma once
#include "leaf/core/filesystem.hpp"
#include "leaf/core/string.hpp"
#include "leaf/core/vector.hpp"
#include "leaf/core/version.hpp"

namespace lf {
	/*!
	** @ingroup modding
	** @brief Parsed dependency constraint from a mod metadata file.
	*/
	struct ModDependency {
		static ModDependency parse(string_view str);

		string name;
		enum class Type {
			Required,
			Optional,
			Forbidden,
		} type;
		enum class Operator {
			Any,
			Equal,
			NotEqual,
			Greater,
			GreaterEqual,
			Less,
			LessEqual,
		} op;

		version required_version;
	};

	/*!
	** @ingroup modding
	** @brief Metadata and resolved location for one mod.
	*/
	struct ModInfo {
		string name;
		version mod_version;
		string title;
		string author;
		string contact;
		string homepage;
		string description;
		version game_version;
		vector<ModDependency> dependencies;
		fs::path location;
		bool privileged;
	};

	/*!
	** @ingroup modding
	** @brief Parses mod metadata from a mod info file.
	** @param path Path to the metadata file.
	** @param priviledged Whether the mod is loaded from a privileged root.
	*/
	report<ModInfo> parse_mod_info(string_view path, bool priviledged);
} // namespace lf
