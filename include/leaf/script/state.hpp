#pragma once

#include <sol/sol.hpp>

namespace lf {
	class object;
	object sol_to_object(const sol::object& value);
	sol::object object_to_sol(sol::state_view state, const object& value);
	sol::state CreateState();
} // namespace lf

