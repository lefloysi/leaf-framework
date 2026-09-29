#pragma once
#define LF_SCHEMA_INCLUDING 1

#include "leaf/core/error.hpp"
#include "leaf/core/string_types.hpp"
#include "leaf/core/version.hpp"

#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>

namespace lf {

	template<typename T, lf::version Version = lf::version{}>
	struct schema_trait;

	template<typename T, lf::version TargetVersion>
	struct migrate_trait;

	// Compile-time schema selection metadata forwarded through nested field and
	// container processing. This is distinct from an encoded lf::version value.
	template<lf::version Version>
	struct schema_version {
		static constexpr lf::version value = Version;
	};

	template<typename T>
	struct is_schema_version : std::false_type {};

	template<lf::version Version>
	struct is_schema_version<schema_version<Version>> : std::true_type {};

	template<typename T>
	concept schema_version_argument = is_schema_version<std::remove_cvref_t<T>>::value;

	namespace detail {
		struct missing_migration_source {};
	} // namespace detail

	// Specialize this for each target schema version that has an immediately
	// preceding source schema version.
	template<typename T, lf::version TargetVersion>
	constexpr auto migration_source = detail::missing_migration_source{};

	namespace detail {
		template<typename T, lf::version TargetVersion>
		concept has_migration_source = !std::same_as<std::remove_cvref_t<decltype(migration_source<T, TargetVersion>)>, missing_migration_source>;
	}

	template<typename T>
	auto schema(T& value) {
		return schema_trait<std::remove_cvref_t<T>>::get(value);
	}

	template<typename T>
	concept schema_value = requires(T& value) {
		schema_trait<std::remove_cvref_t<T>>::get(value);
	};

	template<lf::version Version, typename T>
	auto schema(T& value) {
		return schema_trait<std::remove_cvref_t<T>, Version>::get(value);
	}

	template<lf::version TargetVersion, typename T>
	error migrate(T& value, lf::version current_version) {
		using value_type = std::remove_cvref_t<T>;
		if (current_version == TargetVersion) {
			return {};
		}
		if constexpr (detail::has_migration_source<value_type, TargetVersion>) {
			constexpr lf::version source_version = migration_source<value_type, TargetVersion>;
			if (auto err = migrate<source_version>(value, current_version); err) {
				return err;
			}
			return migrate_trait<value_type, TargetVersion>::apply(value);
		} else {
			return error(generic_errc::parse_error, "unsupported schema version");
		}
	}

	enum class field_presence : u08 {
		absent,
		present
	};

	template<typename T, typename Default = void, typename... Args>
	struct field_node;

	template<typename... Fields>
	struct group_node;

	template<typename Controller, typename Condition, typename... Children>
	struct conditional_node;

	template<typename Condition, typename... Children>
	struct when_node;

	template<typename T>
	struct is_schema_node : std::false_type {};

	template<typename T, typename Default, typename... Args>
	struct is_schema_node<field_node<T, Default, Args...>> : std::true_type {};

	template<typename... Fields>
	struct is_schema_node<group_node<Fields...>> : std::true_type {};

	template<typename Controller, typename Condition, typename... Children>
	struct is_schema_node<conditional_node<Controller, Condition, Children...>> : std::true_type {};

	template<typename Condition, typename... Children>
	struct is_schema_node<when_node<Condition, Children...>> : std::true_type {};

	template<typename T>
	concept schema_node = is_schema_node<std::remove_cvref_t<T>>::value;

	template<typename T>
	concept non_schema_node = !schema_node<T>;

	struct present_condition {
		bool operator()(field_presence value) const { return value == field_presence::present; }
	};

	struct absent_condition {
		bool operator()(field_presence value) const { return value == field_presence::absent; }
	};

	template<typename T, typename Default, typename... Args>
	struct field_storage;

	template<typename T, typename... Args>
	struct field_storage<T, void, Args...> {
		string_view name;
		T& value;
		std::tuple<Args&...> args;
		field_storage(string_view name, T& value, Args&... args) : name(name), value(value), args(args...) {}
	};

	template<typename T, typename Default, typename... Args>
	struct field_storage {
		string_view name;
		T& value;
		Default default_value;
		std::tuple<Args&...> args;
		field_storage(string_view name, T& value, Default default_value, Args&... args) : name(name), value(value), default_value(std::move(default_value)), args(args...) {}
	};

	template<typename T, lf::version Version, typename... Args>
	struct field_storage<T, void, schema_version<Version>, Args...> {
		string_view name;
		T& value;
		std::tuple<schema_version<Version>, Args&...> args;
		field_storage(string_view name, T& value, schema_version<Version> version, Args&... args)
			: name(name), value(value), args(std::move(version), args...) {}
	};

	template<typename T, typename Default, typename... Args>
	struct field_node : field_storage<T, Default, Args...> {
		using field_storage<T, Default, Args...>::field_storage;
		using field_storage<T, Default, Args...>::name;
		using field_storage<T, Default, Args...>::value;

		template<typename... Children>
		auto present(Children&&... children) const { return conditional_node{ *this, present_condition{}, std::forward<Children>(children)... }; }
		template<typename... Children>
		auto absent(Children&&... children) const { return conditional_node{ *this, absent_condition{}, std::forward<Children>(children)... }; }
		template<typename Predicate, typename... Children>
		auto when(Predicate&& predicate, Children&&... children) const { return conditional_node{ *this, std::forward<Predicate>(predicate), std::forward<Children>(children)... }; }
	};

	template<typename... Fields>
	struct group_node {
		std::tuple<Fields...> fields;

		template<typename... Children>
		auto present(Children&&... children) const { return conditional_node{ *this, present_condition{}, std::forward<Children>(children)... }; }
		template<typename... Children>
		auto absent(Children&&... children) const { return conditional_node{ *this, absent_condition{}, std::forward<Children>(children)... }; }
		template<typename Predicate, typename... Children>
		auto when(Predicate&& predicate, Children&&... children) const { return conditional_node{ *this, std::forward<Predicate>(predicate), std::forward<Children>(children)... }; }
	};

	template<typename Controller, typename Condition, typename... Children>
	struct conditional_node {
		Controller controller;
		Condition condition;
		std::tuple<Children...> children;

		template<typename... ChildArgs>
		conditional_node(Controller controller, Condition condition, ChildArgs&&... children) : controller(std::move(controller)), condition(std::move(condition)), children(std::forward<ChildArgs>(children)...) {}
	};

	template<typename Condition, typename... Children>
	struct when_node {
		Condition condition;
		std::tuple<Children...> children;
	};

	template<typename Controller, typename Condition, typename... Children>
	conditional_node(Controller, Condition, Children&&...) -> conditional_node<Controller, Condition, std::remove_cvref_t<Children>...>;

	template<typename Condition, typename... Children>
	when_node(Condition, Children&&...) -> when_node<Condition, std::remove_cvref_t<Children>...>;

	template<typename T>
	field_node<T> field(string_view name, T& value) { return { name, value }; }

	template<typename T, typename Default>
		requires(!schema_version_argument<Default>)
	field_node<T, T> field(string_view name, T& value, Default&& default_value) { return { name, value, T{ std::forward<Default>(default_value) } }; }

	template<typename T, lf::version Version, typename... Args>
	field_node<T, void, schema_version<Version>, std::remove_cvref_t<Args>...> field(string_view name, T& value, schema_version<Version> version, Args&... args) {
		return { name, value, std::move(version), args... };
	}

	// Extra arguments belong to the shared node so every processor can forward
	// them in the established process(Stream&, Value&, Args&...) order.
	template<typename T, typename... Args>
	field_node<T, void, std::remove_cvref_t<Args>...> field(string_view name, T& value, Args&... args) { return { name, value, args... }; }

	template<typename... Fields>
	group_node<std::remove_cvref_t<Fields>...> group(Fields&&... fields) { return { { std::forward<Fields>(fields)... } }; }

	template<typename Condition, typename... Children>
	when_node<std::remove_cvref_t<Condition>, std::remove_cvref_t<Children>...> when(Condition&& condition, Children&&... children) {
		return { std::forward<Condition>(condition), { std::forward<Children>(children)... } };
	}

	namespace detail {
		template<typename>
		inline constexpr bool schema_dependent_false = false;

		template<typename Visitor, typename T, typename Default, typename... Args>
		void visit_schema_fields(const field_node<T, Default, Args...>& field, Visitor& visitor) {
			visitor(field);
		}

		template<typename Visitor, typename... Fields>
		void visit_schema_fields(const group_node<Fields...>& group, Visitor& visitor) {
			std::apply([&](const auto&... field) { (visit_schema_fields(field, visitor), ...); }, group.fields);
		}

		template<typename Visitor, typename Controller, typename Condition, typename... Children>
		void visit_schema_fields(const conditional_node<Controller, Condition, Children...>& conditional, Visitor& visitor) {
			visit_schema_fields(conditional.controller, visitor);
			bool active = false;
			if constexpr (requires { conditional.condition(conditional.controller.value); }) {
				active = conditional.condition(conditional.controller.value);
			} else if constexpr (requires { conditional.condition(field_presence::present); }) {
				// Visiting exports the current in-memory value. The controller is
				// therefore present even when a decoder would distinguish an
				// absent input field.
				active = conditional.condition(field_presence::present);
			} else if constexpr (requires { conditional.condition(); }) {
				active = conditional.condition();
			} else {
				static_assert(schema_dependent_false<Condition>, "schema conditional must accept a controller value, field presence, or no arguments");
			}
			if (active) {
				std::apply([&](const auto&... child) { (visit_schema_fields(child, visitor), ...); }, conditional.children);
			}
		}

		template<typename Visitor, typename Condition, typename... Children>
		void visit_schema_fields(const when_node<Condition, Children...>& conditional, Visitor& visitor) {
			if (conditional.condition()) {
				std::apply([&](const auto&... child) { (visit_schema_fields(child, visitor), ...); }, conditional.children);
			}
		}
	} // namespace detail

	template<schema_node Node, typename Visitor>
	void visit_schema_fields(const Node& node, Visitor&& visitor) {
		detail::visit_schema_fields(node, visitor);
	}

} // namespace lf
#undef LF_SCHEMA_INCLUDING
#include "leaf/core/string_api.hpp"
