#pragma once

#include <leaf/core/distance.hpp>
#include <leaf/core/identifier.hpp>
#include <leaf/core/math/rect.hpp>
#include <leaf/core/schema.hpp>
#include <leaf/core/vector.hpp>
#include <leaf/graphics/graphics_program.hpp>
#include <leaf/graphics/resource.hpp>
#include <leaf/resource/database.hpp>
#include <leaf/resource/prototype.hpp>

#include <sol/sol.hpp>

#include <concepts>
#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>

namespace lf::prototype_lua {
	inline void write_value(sol::state_view, sol::table destination, string_view name, string_view value) {
		destination[string(name)] = string(value);
	}

	inline void write_value(sol::state_view, sol::table destination, string_view name, const string& value) {
		destination[string(name)] = value;
	}

	inline void write_value(sol::state_view, sol::table destination, string_view name, distance value) {
		destination[string(name)] = value.quantum_count();
	}

	inline void write_value(sol::state_view, sol::table destination, string_view name, byte value) {
		destination[string(name)] = std::to_integer<u08>(value);
	}

	inline void write_value(sol::state_view, sol::table destination, string_view name, u64 value) {
		destination[string(name)] = std::to_string(value);
	}

	template<std::integral Value>
	void write_value(sol::state_view, sol::table destination, string_view name, Value value) {
		destination[string(name)] = value;
	}

	template<std::floating_point Value>
	void write_value(sol::state_view, sol::table destination, string_view name, Value value) {
		destination[string(name)] = value;
	}

	template<typename ReferencedPrototype, typename ValueNumber, typename GenerationNumber>
	void write_value(
		sol::state_view,
		sol::table destination,
		string_view name,
		identifier<ReferencedPrototype, ValueNumber, GenerationNumber> value
	) {
		destination[string(name)] = value ? string(Database<ReferencedPrototype>::name(value)) : string{};
	}

	void write_value(sol::state_view lua, sol::table destination, string_view name, const rect<u32>& value);
	void write_value(sol::state_view, sol::table destination, string_view name, rt::format value);

	template<typename Value>
	void write_value(sol::state_view lua, sol::table destination, string_view name, const std::optional<Value>& value) {
		if (!value) {
			destination[string(name)] = sol::lua_nil;
			return;
		}
		write_value(lua, destination, name, *value);
	}

	template<typename Value>
	void write_list_value(sol::state_view lua, sol::table destination, size_t index, const Value& value);

	template<schema_value Value>
	void write_value(sol::state_view lua, sol::table destination, string_view name, const Value& value);

	template<typename Value, typename Allocator>
	void write_value(sol::state_view lua, sol::table destination, string_view name, const vector<Value, Allocator>& values) {
		sol::table result = lua.create_table();
		for (size_t index = 0; index < values.size(); ++index) {
			write_list_value(lua, result, index + 1, values[index]);
		}
		destination[string(name)] = result;
	}

	template<typename Value>
	void write_list_value(sol::state_view lua, sol::table destination, size_t index, const Value& value) {
		sol::table field = lua.create_table();
		write_value(lua, field, "value", value);
		destination[index] = field["value"];
	}

	inline void write_value(sol::state_view lua, sol::table destination, string_view name, const rect<u32>& value) {
		destination[string(name)] = lua.create_table_with(
			"x", value.pos.x,
			"y", value.pos.y,
			"width", value.dim.width,
			"height", value.dim.height
		);
	}

	inline void write_value(sol::state_view, sol::table destination, string_view name, rt::format value) {
		destination[string(name)] = static_cast<u32>(value);
	}

	template<schema_value Value>
	void write_value(sol::state_view lua, sol::table destination, string_view name, const Value& value) {
		sol::table result = lua.create_table();
		visit_schema_fields(schema(value), [&](const auto& field) {
			write_value(lua, result, field.name, field.value);
		});
		destination[string(name)] = result;
	}
} // namespace lf::prototype_lua

namespace lf {
	template<>
	struct object_trait<rt::format> {
		static rt::format parse(const object& value);
	};

	template<>
	struct schema_trait<rt::vertex_attribute> {
		static auto get(auto& value) {
			return group(
				field("name", value.name),
				field("offset", value.offset),
				field("format", value.format)
			);
		}
	};

	// This exporter accepts only Leaf prototype objects that provide an explicit
	// schema. Callers choose concrete types; it performs no global discovery.
	template<typename Prototype>
		requires std::derived_from<Prototype, PrototypeBase> && schema_value<Prototype>
	void ExportPrototypeTable(sol::state& lua) {
		using database = Database<Prototype>;
		sol::object existing = lua["prototypes"];
		sol::table prototypes = existing.valid() && existing.is<sol::table>() ? existing.as<sol::table>() : lua.create_table();
		sol::table records = lua.create_table();
		for (size_t index = 0; index < database::count(); ++index) {
			const auto id = typename Prototype::ID(static_cast<typename Prototype::ID::vnum_t>(index + 1));
			const Prototype& prototype = database::get(id);
			sol::table record = lua.create_table_with(
				"id", id.get(),
				"type", string(database::type()),
				"name", string(database::name(id))
			);
			visit_schema_fields(schema(prototype), [&](const auto& field) {
				prototype_lua::write_value(lua, record, field.name, field.value);
			});
			records[index + 1] = record;
			records[string(database::name(id))] = record;
		}
		prototypes[string(database::type())] = records;
		lua["prototypes"] = prototypes;
	}
} // namespace lf
