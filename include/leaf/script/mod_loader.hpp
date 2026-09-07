#pragma once

#include "leaf/core/error.hpp"
#include "leaf/core/progress.hpp"
#include "leaf/core/yaml.hpp"
#include "leaf/script/localization.hpp"
#include "leaf/script/mod_info.hpp"

#include <sol/sol.hpp>

namespace lf {
	/*!
	** @ingroup modding
	** @brief Options produced by the most recent successful mod load, keyed by mod name.
	*/
	extern object Options;
	object sol_to_object(const sol::object& value);
}

namespace lf::mod {
	struct Source {
		fs::path path;
		bool privileged = false;
	};

	/*!
	** @ingroup modding
	** @brief Loads mods from directories in the virtual filesystem.
	** @return An error if loading fails, or an empty error on success.
	*/
	error Load(span<const Source> sources, Progress progress = Progress{});

	/*!
	** @ingroup modding
	** @brief Gets the mods from the most recent successful mod load.
	*/
	const vector<ModInfo>& Loaded();

	/*!
	** @ingroup modding
	** @brief Clears loaded mods and registered prototypes.
	*/
	void Unload();
} // namespace lf::mod

