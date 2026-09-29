#include "leaf/script/prototype.hpp"

namespace lf::prototype_lua {
	void write_value(sol::state_view, sol::table destination, string_view name, string_view value) {
		destination[string(name)] = string(value);
	}

	void write_value(sol::state_view, sol::table destination, string_view name, const string& value) {
		destination[string(name)] = value;
	}

	void write_value(sol::state_view, sol::table destination, string_view name, distance value) {
		destination[string(name)] = value.quantum_count();
	}

	void write_value(sol::state_view, sol::table destination, string_view name, byte value) {
		destination[string(name)] = std::to_integer<u08>(value);
	}

	void write_value(sol::state_view, sol::table destination, string_view name, u64 value) {
		destination[string(name)] = std::to_string(value);
	}

	void write_value(sol::state_view lua, sol::table destination, string_view name, const rect<u32>& value) {
		destination[string(name)] = lua.create_table_with(
			"x", value.pos.x,
			"y", value.pos.y,
			"width", value.dim.width,
			"height", value.dim.height
		);
	}

	void write_value(sol::state_view, sol::table destination, string_view name, rt::format value) {
		destination[string(name)] = static_cast<u32>(value);
	}
} // namespace lf::prototype_lua
