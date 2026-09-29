#if !defined(LF_DYNAMIC_OBJECT_INCLUDING) && !defined(LF_SCHEMA_INCLUDING)
#pragma once

#include <expected>

namespace lf {
	struct error;
	template<typename T>
	using report = std::expected<T, error>;

	class object;

	namespace str {
		class reader {
		  public:
			explicit reader(string_view input);
			error read(object& target);

		  private:
			string_view input;
			size_t position = 0;
		};

		template<typename T>
		error assign(const object& source, T& target);

		template<typename T>
		error process(reader& input, T& target);

		template<typename T>
		error process(string_view input, T& target);

		template<typename T>
		report<T> read(string_view input);
	} // namespace str
} // namespace lf

#include "leaf/core/dynamic_object.hpp"

#include <algorithm>
#include <exception>
#include <tuple>
#include <type_traits>
#include <utility>

namespace lf::str {
	report<object> read_object(string_view input);

	template<typename T>
	struct vector_value_trait : std::false_type {};

	template<typename T, typename Allocator>
	struct vector_value_trait<vector<T, Allocator>> : std::true_type {
		using element_type = T;
	};

	template<typename T>
	concept vector_value = vector_value_trait<std::remove_cvref_t<T>>::value;

	namespace detail {
		template<typename T>
		error assign_from_object(const object& source, T& target);

		template<schema_value T>
		error assign_from_object(const object& source, T& target);

		template<vector_value T>
		error assign_from_object(const object& source, T& target);

		template<typename T, typename Default, typename... Args>
		error process_schema_node(const dict& source, const field_node<T, Default, Args...>& field, vector<string_view>& assigned, field_presence* presence) {
			const auto value = source.find(field.name);
			if (value == source.end()) {
				if constexpr (std::is_void_v<Default>) {
					return error(generic_errc::missing_field, lf::format("missing field '{}'", field.name));
				} else {
					field.value = field.default_value;
					if (presence) *presence = field_presence::absent;
					return {};
				}
			}
			if (auto value_error = lf::str::assign(value->second, field.value)) {
				value_error.add_context(lf::format("field '{}'", field.name));
				return value_error;
			}
			assigned.push_back(field.name);
			if (presence) *presence = field_presence::present;
			return {};
		}

		template<typename... Fields>
		error process_schema_node(const dict& source, const group_node<Fields...>& group, vector<string_view>& assigned, field_presence* presence) {
			field_presence result = field_presence::absent;
			error value_error;
			std::apply([&](const auto&... field) {
				auto process_field = [&](const auto& item) {
					if (value_error) return;
					field_presence item_presence = field_presence::absent;
					value_error = process_schema_node(source, item, assigned, &item_presence);
					if (item_presence == field_presence::present) result = field_presence::present;
				};
				(process_field(field), ...);
			},
					   group.fields);
			if (presence) *presence = result;
			return value_error;
		}

		template<typename Controller, typename Condition, typename... Children>
		error process_schema_node(const dict& source, const conditional_node<Controller, Condition, Children...>& conditional, vector<string_view>& assigned, field_presence* presence) {
			field_presence controller_presence = field_presence::absent;
			if (auto value_error = process_schema_node(source, conditional.controller, assigned, &controller_presence)) return value_error;
			bool active = false;
			if constexpr (requires { conditional.condition(controller_presence); }) {
				active = conditional.condition(controller_presence);
			} else if constexpr (requires { conditional.condition(conditional.controller.value); }) {
				active = controller_presence == field_presence::present && conditional.condition(conditional.controller.value);
			} else if constexpr (requires { conditional.condition(); }) {
				active = conditional.condition();
			} else {
				static_assert(lf::dependent_false<Condition>, "schema conditional must accept field presence, controller value, or no arguments");
			}
			if (!active) {
				if (presence) *presence = controller_presence;
				return {};
			}
			return std::apply([&](const auto&... field) {
				return process_schema_node(source, lf::group(field...), assigned, presence);
			},
							  conditional.children);
		}

		template<typename Condition, typename... Children>
		error process_schema_node(const dict& source, const when_node<Condition, Children...>& conditional, vector<string_view>& assigned, field_presence* presence) {
			if (!conditional.condition()) {
				if (presence) *presence = field_presence::absent;
				return {};
			}
			return std::apply([&](const auto&... field) {
				return process_schema_node(source, lf::group(field...), assigned, presence);
			},
							  conditional.children);
		}

		template<schema_value T>
		error assign_from_object(const object& source, T& target) {
			if (!source.is<dict>()) {
				return error(generic_errc::type_mismatch, lf::format("cannot assign {} to structured value", source.current_type_name()));
			}
			vector<string_view> assigned;
			if (auto value_error = process_schema_node(source.get<dict>(), lf::schema(target), assigned, nullptr)) return value_error;
			for (const auto& [name, value] : source.get<dict>()) {
				if (std::find(assigned.begin(), assigned.end(), string_view(name)) == assigned.end()) {
					return error(generic_errc::parse_error, lf::format("unknown or inactive structured field '{}'", name));
				}
			}
			return {};
		}

		template<vector_value T>
		error assign_from_object(const object& source, T& target) {
			if (!source.is<list>()) {
				return error(generic_errc::type_mismatch, lf::format("cannot assign {} to list", source.current_type_name()));
			}
			using element_type = typename vector_value_trait<std::remove_cvref_t<T>>::element_type;
			T result;
			result.reserve(source.get<list>().size());
			for (size_t index = 0; index < source.get<list>().size(); ++index) {
				element_type element{};
				if (auto value_error = lf::str::assign(source.get<list>()[index], element)) {
					value_error.add_context(lf::format("list index {}", index));
					return value_error;
				}
				result.push_back(std::move(element));
			}
			target = std::move(result);
			return {};
		}

		template<typename T>
		error assign_from_object(const object& source, T& target) {
			try {
				target = source.template parse<T>();
				return {};
			} catch (const error& value) {
				return value;
			} catch (const std::exception& exception) {
				return error(generic_errc::parse_error, exception.what());
			}
		}
	} // namespace detail

	template<typename T>
	struct mutation {
		static error assign(const object& source, T& target) {
			return detail::assign_from_object(source, target);
		}
	};

	template<typename T>
	error assign(const object& source, T& target) {
		return mutation<T>::assign(source, target);
	}

	template<typename T>
	error process(reader& input, T& target) {
		object source;
		if (auto value_error = input.read(source)) return value_error;
		return assign(source, target);
	}

	template<typename T>
	error process(string_view input, T& target) {
		auto source = reader{ input };
		return process(source, target);
	}

	template<typename T>
	report<T> read(string_view input) {
		T target{};
		if (auto value_error = process(input, target)) return unexpected(std::move(value_error));
		return target;
	}
} // namespace lf::str
#endif
