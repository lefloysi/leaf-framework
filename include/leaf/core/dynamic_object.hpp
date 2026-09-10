#pragma once
#define LF_DYNAMIC_OBJECT_INCLUDING 1
#include "leaf/core/cast.hpp"
#include "leaf/core/concepts.hpp"
#include "leaf/core/distance.hpp"
#include "leaf/core/normalized.hpp"
#include "leaf/core/error.hpp"
#include "leaf/core/exception.hpp"
#include "leaf/core/schema.hpp"
#include "leaf/core/string_types.hpp"
#include "leaf/core/typename.hpp"
#include "leaf/core/types.hpp"
#include "leaf/core/unordered_map.hpp"
#include "leaf/core/vector.hpp"
#include <limits>

#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace lf {

	template<typename T>
	struct object_trait;

	class dict;
	class list;
	class object;



	using dict_underlying = unordered_map_string<object>;
	using list_underlying = vector<object>;
	using object_underlying = std::variant<std::monostate, bool, i64, u64, f64, string, dict, list>;

	template<typename T>
	concept scalar_object_value = std::integral<T> || std::floating_point<T> || std::same_as<T, string>;

	class dict : public dict_underlying {
	template<typename T, typename Default, typename... Args>
		field_presence assign_schema(const field_node<T, Default, Args...>& field) const;

		template<typename... Fields>
		field_presence assign_schema(const group_node<Fields...>& group) const;

		template<typename Condition, typename... Children>
		field_presence assign_schema(const when_node<Condition, Children...>& conditional) const;

		template<typename Controller, typename Condition, typename... Children>
		field_presence assign_schema(const conditional_node<Controller, Condition, Children...>& conditional) const;

	  public:
		template<schema_node... Fields>
		void assign(Fields&&... fields) const;

		template<typename T>
		T parse_field(string_view index) const;
	};
	class list : public list_underlying {
	  public:
		template<typename T>
		T parse_index(size_t index) const;
	};
	template<>
	struct object_trait<dict>;
	template<>
	struct object_trait<list>;

	class object : object_underlying {
	  public:
		using value_type = object_underlying;
		object();
		object(bool value);
		object(i08 value);
		object(i16 value);
		object(i32 value);
		object(i64 value);
		object(u08 value);
		object(u16 value);
		object(u32 value);
		object(u64 value);
		object(f64 value);
		object(const char* value);
		object(string_view value);
		object(const string& value);
		object(const dict& value);
		object(const list& value);

		template<contains_type<object::value_type> T>
		bool is() const;

		template<typename T>
		bool convertible() const;

		template<contains_type<object::value_type> T>
		T& get();

		template<contains_type<object::value_type> T>
		const T& get() const;

		string_view current_type_name() const;

		object& at(string_view key);
		const object& at(string_view key) const;

		object& at(size_t index);
		const object& at(size_t index) const;

		object& operator[](string_view key);
		const object& operator[](string_view key) const;

		object& operator[](size_t index);
		const object& operator[](size_t index) const;

		template<scalar_object_value T>
		T as() const;

		template<typename T>
		T parse() const;

		template<typename Visitor>
		decltype(auto) visit(Visitor&& visitor) const;
	};

	template<typename>
	inline constexpr bool dependent_false = false;

	template<typename T>
	struct object_trait {
		static T parse(const object& obj) {
			if constexpr (requires(T& value) { schema_trait<T>::get(value); } && std::is_default_constructible_v<T>) {
				if (!obj.is<dict>()) {
					throw runtime_exception(lf::format("cannot convert type '{}' to structured value", obj.current_type_name()));
				}
				const dict& data = obj.get<dict>();
				T value{};
				data.assign(schema(value));
				return value;
			} else if constexpr (requires(const dict& data) { T{ data }; }) {
				return T{ object_trait<dict>::parse(obj) };
			} else {
				static_assert(dependent_false<T>, "no object trait parser for this type");
			}
		}
		static dict to_dict(const T& value) = delete;
	};

	template<typename T, typename Default, typename... Args>
	field_presence dict::assign_schema(const field_node<T, Default, Args...>& field) const {
		const auto iterator = find(field.name);
		if (iterator == end()) {
			if constexpr (!std::is_void_v<Default>) {
				field.value = field.default_value;
			} else {
				throw runtime_exception(lf::format("missing field '{}'", field.name));
			}
			return field_presence::absent;
		}
		const object& object_value = iterator->second;
		field.value = parse_field<T>(field.name);
		return field_presence::present;
	}

	template<typename... Fields>
	field_presence dict::assign_schema(const group_node<Fields...>& group) const {
		field_presence result = field_presence::absent;
		std::apply(
			[this, &result](const auto&... field) {
				((result = assign_schema(field) == field_presence::present ? field_presence::present : result), ...);
			},
			group.fields
		);
		return result;
	}

	template<typename Controller, typename Condition, typename... Children>
	field_presence dict::assign_schema(const conditional_node<Controller, Condition, Children...>& conditional) const {
		const field_presence controller_presence = assign_schema(conditional.controller);
		bool should_process = false;
		if constexpr (requires { conditional.condition(controller_presence); }) {
			should_process = conditional.condition(controller_presence);
		} else if constexpr (requires { conditional.condition(conditional.controller.value); }) {
			if (controller_presence == field_presence::present) {
				should_process = conditional.condition(conditional.controller.value);
			}
		} else if constexpr (requires { conditional.condition(); }) {
			should_process = conditional.condition();
		} else {
			static_assert(dependent_false<Condition>, "schema conditional must accept field presence, controller value, or no arguments");
		}
		if (!should_process) {
			return controller_presence;
		}
		field_presence result = field_presence::absent;
		std::apply(
			[this, &result](const auto&... field) {
				((result = assign_schema(field) == field_presence::present ? field_presence::present : result), ...);
			},
			conditional.children
		);
		return result;
	}

	template<typename Condition, typename... Children>
	field_presence dict::assign_schema(const when_node<Condition, Children...>& conditional) const {
		if (!conditional.condition()) {
			return field_presence::absent;
		}
		field_presence result = field_presence::absent;
		std::apply(
			[this, &result](const auto&... field) {
				((result = assign_schema(field) == field_presence::present ? field_presence::present : result), ...);
			},
			conditional.children
		);
		return result;
	}

	template<schema_node... Fields>
	void dict::assign(Fields&&... fields) const {
		(assign_schema(fields), ...);
	}

	template<typename T>
	T dict::parse_field(string_view field) const {
		auto it = this->find(field);
		if (it == this->end()) {
			throw lf::error(lf::generic_errc::missing_field, lf::format("missing field '{}'", field));
		}
		try {
			return object_trait<T>::parse(it->second);
		} catch (...) {
			rethrow_with_context(lf::format("field '{}' to struct '{}'", field, type_name<T>()));
		}
	}

	template<typename T>
	T list::parse_index(size_t index) const {
		try {
			return object_trait<T>::parse(at(index));
		} catch (...) {
			rethrow_with_context(lf::format("when parsing list index {} to '{}'", index, typeid(T).name()));
		}
	}

	template<contains_type<object::value_type> T>
	bool object::is() const {
		return std::holds_alternative<T>(*this);
	}

	template<typename T>
	bool object::convertible() const {
		if constexpr (std::is_same_v<T, bool>) {
			return is<bool>();
		} else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
			return is<i64>() || is<u64>() || is<f64>();
		} else if constexpr (std::is_same_v<T, string>) {
			return is<string>();
		} else {
			return false;
		}
	}

	template<contains_type<object::value_type> T>
	T& object::get() {
		return std::get<T>(*this);
	}

	template<contains_type<object::value_type> T>
	const T& object::get() const {
		return std::get<T>(*static_cast<const object_underlying*>(this));
	}

	template<scalar_object_value T>
	inline T object::as() const {
		if (!convertible<T>()) {
			throw runtime_exception(lf::format("cannot convert type '{}' to '{}'", current_type_name(), type_name<T>()));
		}
		if constexpr (std::is_same_v<T, bool>) {
			return get<bool>();
		} else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
			if (is<i64>()) {
				return static_cast<T>(get<i64>());
			}
			if (is<u64>()) {
				return static_cast<T>(get<u64>());
			}
			if (is<f64>()) {
				return static_cast<T>(get<f64>());
			}
		} else if constexpr (std::is_same_v<T, string>) {
			return get<string>();
		}
		throw runtime_exception(lf::format("cannot convert type '{}' to '{}'", current_type_name(), type_name<T>()));
	}

	template<typename T>
	T object::parse() const {
		return object_trait<T>::parse(*this);
	}

	template<typename Visitor>
	decltype(auto) object::visit(Visitor&& visitor) const {
		return std::visit(std::forward<Visitor>(visitor), *static_cast<const object_underlying*>(this));
	}

	template<>
	struct type_name_trait<dict> {
		static constexpr const char* get() {
			return "dict";
		}
	};
	template<>
	struct type_name_trait<list> {
		static constexpr const char* get() {
			return "list";
		}
	};
	template<>
	struct type_name_trait<object> {
		static constexpr const char* get() {
			return "object";
		}
	};

	template<>
	struct object_trait<f32> {
		static f32 parse(const object& obj) {
			if (obj.is<f64>()) {
				f64 d = obj.get<f64>();
				if (d < static_cast<f64>(std::numeric_limits<f32>::lowest()) ||
					d > static_cast<f64>(std::numeric_limits<f32>::max())) {
					throw lf::runtime_exception(lf::format("value {} out of range for f32", d));
				}
				return static_cast<f32>(d);
			} else if (obj.is<i64>()) {
				i64 i = obj.get<i64>();
				if (static_cast<f64>(i) < static_cast<f64>(std::numeric_limits<f32>::lowest()) ||
					static_cast<f64>(i) > static_cast<f64>(std::numeric_limits<f32>::max())) {
					throw lf::runtime_exception(lf::format("value {} out of range for f32", i));
				}
				return static_cast<f32>(i);
			} else if (obj.is<u64>()) {
				u64 u = obj.get<u64>();
				if (static_cast<f64>(u) > static_cast<f64>(std::numeric_limits<f32>::max())) {
					throw lf::runtime_exception(lf::format("value {} out of range for f32", u));
				}
				return static_cast<f32>(u);
			} else {
				throw lf::runtime_exception(lf::format("cannot convert type '{}' to f32", obj.current_type_name()));
			}
		}
	};
	template<>
	struct object_trait<f64> {
		static f64 parse(const object& obj) {
			if (obj.is<f64>()) {
				return obj.get<f64>();
			} else if (obj.is<i64>()) {
				i64 i = obj.get<i64>();
				return static_cast<f64>(i);
			} else if (obj.is<u64>()) {
				u64 u = obj.get<u64>();
				return static_cast<f64>(u);
			} else {
				throw lf::runtime_exception(lf::format("cannot convert type '{}' to f64", obj.current_type_name()));
			}
		}
	};

	template<normalized_integer T>
	struct object_trait<normalized<T>> {
		static normalized<T> parse(const object& obj) {
			const f64 value = object_trait<f64>::parse(obj);
			constexpr f64 minimum = std::is_signed_v<T> ? -1.0 : 0.0;
			if (!(value >= minimum && value <= 1.0)) {
				throw runtime_exception("normalized value is outside its range");
			}
			const f64 scale = value < 0 ? -f64(normalized<T>::minimum) : f64(normalized<T>::maximum);
			return normalized<T>{ static_cast<T>(value * scale) };
		}
	};
	template<>
	struct object_trait<bool> {
		static bool parse(const object& obj) {
			if (obj.is<bool>()) {
				return obj.get<bool>();
			}
			throw lf::runtime_exception(lf::format("cannot convert type '{}' to bool", obj.current_type_name()));
		}
	};

	template<>
	struct object_trait<i64> {
		static i64 parse(const object& obj) {
			if (obj.is<i64>()) {
				return obj.get<i64>();
			}
			if (obj.is<u64>()) {
				u64 v = obj.get<u64>();
				if (v > static_cast<u64>(std::numeric_limits<i64>::max())) {
					throw lf::runtime_exception(lf::format("value {} out of range for i64", v));
				}
				return static_cast<i64>(v);
			}
			if (obj.is<f64>()) {
				return static_cast<i64>(obj.get<f64>());
			}
			throw lf::runtime_exception(lf::format("cannot convert type '{}' to i64", obj.current_type_name()));
		}
	};
	template<>
	struct object_trait<distance> {
		static distance parse(const object& obj) {
			return distance::from_quantum(object_trait<i64>::parse(obj));
		}
	};
	template<>
	struct object_trait<u64> {
		static u64 parse(const object& obj) {
			if (obj.is<u64>()) {
				return obj.get<u64>();
			}
			if (obj.is<i64>()) {
				i64 v = obj.get<i64>();
				if (v < 0) {
					throw lf::runtime_exception(lf::format("value {} out of range for u64", v));
				}
				return static_cast<u64>(v);
			}
			if (obj.is<f64>()) {
				f64 v = obj.get<f64>();
				if (v < 0 || v > static_cast<f64>(std::numeric_limits<u64>::max())) {
					throw lf::runtime_exception(lf::format("value {} out of range for u64", v));
				}
				return static_cast<u64>(v);
			}
			throw lf::runtime_exception(lf::format("cannot convert type '{}' to u64", obj.current_type_name()));
		}
	};

	template<>
	struct object_trait<i32> {
		static i32 parse(const object& obj) {
			i64 v = object_trait<i64>::parse(obj);
			if (v < static_cast<i64>(std::numeric_limits<i32>::min()) ||
				v > static_cast<i64>(std::numeric_limits<i32>::max())) {
				throw lf::runtime_exception(lf::format("value {} out of range for i32", v));
			}
			return static_cast<i32>(v);
		}
	};
	template<>
	struct object_trait<u32> {
		static u32 parse(const object& obj) {
			u64 v = object_trait<u64>::parse(obj);
			if (v > static_cast<u64>(std::numeric_limits<u32>::max())) {
				throw lf::runtime_exception(lf::format("value {} out of range for u32", v));
			}
			return static_cast<u32>(v);
		}
	};
	template<>
	struct object_trait<i16> {
		static i16 parse(const object& obj) {
			i64 v = object_trait<i64>::parse(obj);
			if (v < static_cast<i64>(std::numeric_limits<i16>::min()) ||
				v > static_cast<i64>(std::numeric_limits<i16>::max())) {
				throw lf::runtime_exception(lf::format("value {} out of range for i16", v));
			}
			return static_cast<i16>(v);
		}
	};
	template<>
	struct object_trait<u16> {
		static u16 parse(const object& obj) {
			u64 v = object_trait<u64>::parse(obj);
			if (v > static_cast<u64>(std::numeric_limits<u16>::max())) {
				throw lf::runtime_exception(lf::format("value {} out of range for u16", v));
			}
			return static_cast<u16>(v);
		}
	};
	template<>
	struct object_trait<i08> {
		static i08 parse(const object& obj) {
			i64 v = object_trait<i64>::parse(obj);
			if (v < static_cast<i64>(std::numeric_limits<i08>::min()) ||
				v > static_cast<i64>(std::numeric_limits<i08>::max())) {
				throw lf::runtime_exception(lf::format("value {} out of range for i08", v));
			}
			return static_cast<i08>(v);
		}
	};
	template<>
	struct object_trait<u08> {
		static u08 parse(const object& obj) {
			u64 v = object_trait<u64>::parse(obj);
			if (v > static_cast<u64>(std::numeric_limits<u08>::max())) {
				throw lf::runtime_exception(lf::format("value {} out of range for u08", v));
			}
			return static_cast<u08>(v);
		}
	};
	template<>
	struct object_trait<byte> {
		static byte parse(const object& obj) {
			return static_cast<byte>(object_trait<u08>::parse(obj));
		}
	};

	template<>
	struct object_trait<string> {
		static string parse(const object& obj) {
			if (obj.is<string>()) {
				return obj.get<string>();
			}
			throw runtime_exception(lf::format("cannot convert type '{}' to string", obj.current_type_name()));
		}
	};
	template<>
	struct object_trait<dict> {
		static dict parse(const object& obj) {
			if (obj.is<dict>()) {
				return obj.get<dict>();
			}
			throw runtime_exception(lf::format("cannot convert type '{}' to dict", obj.current_type_name()));
		}
	};
	template<>
	struct object_trait<list> {
		static list parse(const object& obj) {
			if (obj.is<list>()) {
				return obj.get<list>();
			}
			throw runtime_exception(lf::format("cannot convert type '{}' to list", obj.current_type_name()));
		}
	};
	template<typename T>
	struct object_trait<vector<T>> {
		static vector<T> parse(const object& obj) {
			const list& values = object_trait<list>::parse(obj);
			vector<T> result;
			result.reserve(values.size());
			for (size_t index = 0; index < values.size(); ++index) {
				result.push_back(values.parse_index<T>(index));
			}
			return result;
		}
	};

	template<typename T>
	struct object_trait<std::optional<T>> {
		static std::optional<T> parse(const object& obj) {
			if (obj.is<std::monostate>()) {
				return std::nullopt;
			}
			return object_trait<T>::parse(obj);
		}
	};
} // namespace lf
#undef LF_DYNAMIC_OBJECT_INCLUDING
#include "leaf/core/string_api.hpp"
