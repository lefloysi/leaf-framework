#include "leaf/script/state.hpp"

#include "leaf/script/extensions.hpp"

namespace lf {
	sol::state CreateState() {
		sol::state state;
		state.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
<<<<<<< Updated upstream
		script_system::install(state);
=======
		if (const auto error{ Register<sol::state, error(sol::state&)>::install(state) }) { throw runtime_exception(error.message); }
>>>>>>> Stashed changes
		return state;
	}

	void PrepareState(sol::state& state, span<const script_installer> installers) {
		for (const script_installer& install : installers) {
			install(state);
		}
	}
}
