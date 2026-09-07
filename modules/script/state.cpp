#include "leaf/script/state.hpp"
#include <leaf/core/register.hpp>
#include <leaf/core/exception.hpp>

namespace lf {
	error InstallScriptInterfaces(sol::state& state);
	static const auto interfaces_registered{ [] {
		Register<sol::state>::add(InstallScriptInterfaces);
		return true;
	}() };

	sol::state CreateState() {
		sol::state state{};
		state.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
		if (const auto error{ Register<sol::state>::install(state) }) { throw runtime_exception(error.message); }
		return state;
	}
}
