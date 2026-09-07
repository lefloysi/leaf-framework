#pragma once

#include <sol/sol.hpp>

namespace lf {
	class object;
	sol::object object_to_sol(sol::state_view state, const object& value);
	sol::state CreateState();
}
