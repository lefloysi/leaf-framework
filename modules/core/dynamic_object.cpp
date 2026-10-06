#include "leaf/core/yaml.hpp"
#include "leaf/core/fixed.hpp"

#include <yaml-cpp/yaml.h>

#include <cmath>
#include <stdexcept>

namespace lf {

	object::object() : object_underlying(std::monostate{}) {}
	object::object(bool value) : object_underlying(value) {}
	object::object(i08 value) : object_underlying(static_cast<i64>(value)) {}
	object::object(i16 value) : object_underlying(static_cast<i64>(value)) {}
	object::object(i32 value) : object_underlying(static_cast<i64>(value)) {}
	object::object(i64 value) : object_underlying(static_cast<i64>(value)) {}
	object::object(u08 value) : object_underlying(static_cast<u64>(value)) {}
	object::object(u16 value) : object_underlying(static_cast<u64>(value)) {}
	object::object(u32 value) : object_underlying(static_cast<u64>(value)) {}
	object::object(u64 value) : object_underlying(value) {}
	object::object(f64 value) : object_underlying(value) {}
	object::object(const char* value) : object_underlying(string(value)) {}
	object::object(const string& value) : object_underlying(value) {}
	object::object(string_view value) : object_underlying(string(value)) {}
	object::object(const dict& value) : object_underlying(value) {}
	object::object(const list& value) : object_underlying(value) {}

	f32 object_trait<f32>::parse(const object& obj) {
		if (obj.is<f64>()) {
			const f64 value = obj.get<f64>();
			if (value < static_cast<f64>(std::numeric_limits<f32>::lowest()) || value > static_cast<f64>(std::numeric_limits<f32>::max())) {
				throw runtime_exception(lf::format("value {} out of range for f32", value));
			}
			return static_cast<f32>(value);
		}
		if (obj.is<i64>()) {
			const i64 value = obj.get<i64>();
			if (static_cast<f64>(value) < static_cast<f64>(std::numeric_limits<f32>::lowest()) || static_cast<f64>(value) > static_cast<f64>(std::numeric_limits<f32>::max())) {
				throw runtime_exception(lf::format("value {} out of range for f32", value));
			}
			return static_cast<f32>(value);
		}
		if (obj.is<u64>()) {
			const u64 value = obj.get<u64>();
			if (static_cast<f64>(value) > static_cast<f64>(std::numeric_limits<f32>::max())) {
				throw runtime_exception(lf::format("value {} out of range for f32", value));
			}
			return static_cast<f32>(value);
		}
		throw runtime_exception(lf::format("cannot convert type '{}' to f32", obj.current_type_name()));
	}

	f64 object_trait<f64>::parse(const object& obj) {
		if (obj.is<f64>()) { return obj.get<f64>(); }
		if (obj.is<i64>()) { return static_cast<f64>(obj.get<i64>()); }
		if (obj.is<u64>()) { return static_cast<f64>(obj.get<u64>()); }
		throw runtime_exception(lf::format("cannot convert type '{}' to f64", obj.current_type_name()));
	}

	bool object_trait<bool>::parse(const object& obj) {
		if (obj.is<bool>()) { return obj.get<bool>(); }
		throw runtime_exception(lf::format("cannot convert type '{}' to bool", obj.current_type_name()));
	}

	i64 object_trait<i64>::parse(const object& obj) {
		if (obj.is<i64>()) { return obj.get<i64>(); }
		if (obj.is<u64>()) {
			const u64 value = obj.get<u64>();
			if (value > static_cast<u64>(std::numeric_limits<i64>::max())) {
				throw runtime_exception(lf::format("value {} out of range for i64", value));
			}
			return static_cast<i64>(value);
		}
		if (obj.is<f64>()) { return static_cast<i64>(obj.get<f64>()); }
		throw runtime_exception(lf::format("cannot convert type '{}' to i64", obj.current_type_name()));
	}

	distance object_trait<distance>::parse(const object& obj) {
		return distance::from_quantum(object_trait<i64>::parse(obj));
	}

	fixed object_trait<fixed>::parse(const object& obj) {
		if (obj.is<string>()) {
			const auto value = fixed::parse(obj.get<string>());
			if (!value) { throw runtime_exception(value.error().message); }
			return *value;
		}
		if (obj.is<i64>()) {
			const auto value = fixed::from_integer(obj.get<i64>());
			if (!value) { throw runtime_exception(value.error().message); }
			return *value;
		}
		if (obj.is<u64>()) {
			const u64 source = obj.get<u64>();
			if (source > static_cast<u64>(std::numeric_limits<i64>::max() / fixed::scale)) {
				throw runtime_exception(lf::format("value {} out of range for fixed", source));
			}
			const auto value = fixed::from_integer(static_cast<i64>(source));
			if (!value) { throw runtime_exception(value.error().message); }
			return *value;
		}
		if (obj.is<f64>()) {
			const f64 source = obj.get<f64>();
			const f64 raw = source * static_cast<f64>(fixed::scale);
			if (!std::isfinite(raw) || raw < static_cast<f64>(std::numeric_limits<i64>::min()) || raw >= static_cast<f64>(std::numeric_limits<i64>::max())) {
				throw runtime_exception(lf::format("value {} out of range for fixed", source));
			}
			return fixed::from_raw(static_cast<i64>(raw));
		}
		throw runtime_exception(lf::format("cannot convert type '{}' to fixed", obj.current_type_name()));
	}

	u64 object_trait<u64>::parse(const object& obj) {
		if (obj.is<u64>()) { return obj.get<u64>(); }
		if (obj.is<i64>()) {
			const i64 value = obj.get<i64>();
			if (value < 0) { throw runtime_exception(lf::format("value {} out of range for u64", value)); }
			return static_cast<u64>(value);
		}
		if (obj.is<f64>()) {
			const f64 value = obj.get<f64>();
			if (value < 0 || value > static_cast<f64>(std::numeric_limits<u64>::max())) {
				throw runtime_exception(lf::format("value {} out of range for u64", value));
			}
			return static_cast<u64>(value);
		}
		throw runtime_exception(lf::format("cannot convert type '{}' to u64", obj.current_type_name()));
	}

	namespace {
		template<typename Value>
		Value parse_signed(const object& obj, string_view type) {
			const i64 value = object_trait<i64>::parse(obj);
			if (value < static_cast<i64>(std::numeric_limits<Value>::min()) || value > static_cast<i64>(std::numeric_limits<Value>::max())) {
				throw runtime_exception(lf::format("value {} out of range for {}", value, type));
			}
			return static_cast<Value>(value);
		}

		template<typename Value>
		Value parse_unsigned(const object& obj, string_view type) {
			const u64 value = object_trait<u64>::parse(obj);
			if (value > static_cast<u64>(std::numeric_limits<Value>::max())) {
				throw runtime_exception(lf::format("value {} out of range for {}", value, type));
			}
			return static_cast<Value>(value);
		}
	} // namespace

	i32 object_trait<i32>::parse(const object& obj) { return parse_signed<i32>(obj, "i32"); }
	u32 object_trait<u32>::parse(const object& obj) { return parse_unsigned<u32>(obj, "u32"); }
	i16 object_trait<i16>::parse(const object& obj) { return parse_signed<i16>(obj, "i16"); }
	u16 object_trait<u16>::parse(const object& obj) { return parse_unsigned<u16>(obj, "u16"); }
	i08 object_trait<i08>::parse(const object& obj) { return parse_signed<i08>(obj, "i08"); }
	u08 object_trait<u08>::parse(const object& obj) { return parse_unsigned<u08>(obj, "u08"); }

	byte object_trait<byte>::parse(const object& obj) {
		return static_cast<byte>(object_trait<u08>::parse(obj));
	}

	string object_trait<string>::parse(const object& obj) {
		if (obj.is<string>()) { return obj.get<string>(); }
		throw runtime_exception(lf::format("cannot convert type '{}' to string", obj.current_type_name()));
	}

	dict object_trait<dict>::parse(const object& obj) {
		if (obj.is<dict>()) { return obj.get<dict>(); }
		throw runtime_exception(lf::format("cannot convert type '{}' to dict", obj.current_type_name()));
	}

	list object_trait<list>::parse(const object& obj) {
		if (obj.is<list>()) { return obj.get<list>(); }
		throw runtime_exception(lf::format("cannot convert type '{}' to list", obj.current_type_name()));
	}

	object& object::at(string_view key) {
		if (!is<dict>()) {
			throw std::runtime_error(
				lf::format("object::at: not a dict (type = {})", current_type_name())
			);
		}
		return get<dict>().at(string(key));
	}

	const object& object::at(string_view key) const {
		if (!is<dict>()) {
			throw std::runtime_error(
				lf::format("object::at: not a dict (type = {})", current_type_name())
			);
		}
		return get<dict>().at(string(key));
	}

	object& object::at(size_t index) {
		if (!is<list>()) {
			throw std::runtime_error(
				lf::format("object::at: not a list (type = {})", current_type_name())
			);
		}
		return get<list>().at(index);
	}

	const object& object::at(size_t index) const {
		if (!is<list>()) {
			throw std::runtime_error(
				lf::format("object::at: not a list (type = {})", current_type_name())
			);
		}
		return get<list>().at(index);
	}

	object& object::operator[](string_view key) {
		return get<dict>()[string(key)];
	}
	object& object::operator[](size_t index) {
		return get<list>()[index];
	}
	const object& object::operator[](size_t index) const {
		return get<list>()[index];
	}

	string_view object::current_type_name() const {
		return std::visit(
			[](auto&& v) -> string_view {
				using T = std::decay_t<decltype(v)>;
				if constexpr (std::is_same_v<T, std::monostate>) {
					return "null";
				} else if constexpr (std::is_same_v<T, bool>) {
					return "bool";
				} else if constexpr (std::is_same_v<T, i64>) {
					return "i64";
				} else if constexpr (std::is_same_v<T, u64>) {
					return "u64";
				} else if constexpr (std::is_same_v<T, f64>) {
					return "f64";
				} else if constexpr (std::is_same_v<T, string>) {
					return "string";
				} else if constexpr (std::is_same_v<T, dict>) {
					return "dict";
				} else if constexpr (std::is_same_v<T, list>) {
					return "list";
				}
			},
			*static_cast<const object_underlying*>(this)
		);
	}

	void EmitYaml(YAML::Emitter& out, const object& value) {
		value.visit([&out](const auto& typed) {
			using T = std::decay_t<decltype(typed)>;
			if constexpr (std::is_same_v<T, std::monostate>) {
				out << YAML::Null;
			} else if constexpr (std::is_same_v<T, dict>) {
				out << YAML::BeginMap;
				for (const auto& [key, child] : typed) {
					out << YAML::Key << key;
					out << YAML::Value;
					EmitYaml(out, child);
				}
				out << YAML::EndMap;
			} else if constexpr (std::is_same_v<T, list>) {
				out << YAML::BeginSeq;
				for (const object& child : typed) {
					EmitYaml(out, child);
				}
				out << YAML::EndSeq;
			} else {
				out << typed;
			}
		});
	}

	object ObjectFromYaml(const YAML::Node& node) {
		if (node.IsNull()) {
			return {};
		}
		if (node.IsMap()) {
			dict result;
			for (const auto& entry : node) {
				result[entry.first.as<string>()] = ObjectFromYaml(entry.second);
			}
			return result;
		}
		if (node.IsSequence()) {
			list result;
			for (const auto& entry : node) {
				result.push_back(ObjectFromYaml(entry));
			}
			return result;
		}
		try {
			return object(node.as<bool>());
		} catch (const YAML::Exception&) {}
		try {
			return object(node.as<i64>());
		} catch (const YAML::Exception&) {}
		try {
			return object(node.as<u64>());
		} catch (const YAML::Exception&) {}
		try {
			return object(node.as<f64>());
		} catch (const YAML::Exception&) {}
		return object(node.as<string>());
	}

} // namespace lf
