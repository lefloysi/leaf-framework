#pragma once

#include <leaf/core/span.hpp>

#include <sol/sol.hpp>
#include <leaf/core/error.hpp>

#include <functional>

namespace lf {
	using script_installer = std::function<void(sol::state&)>;

	sol::state CreateState();
<<<<<<< Updated upstream
	void PrepareState(sol::state& state, span<const script_installer> installers);
=======
	error InstallScriptInterfaces(sol::state& state);
>>>>>>> Stashed changes
}
