#pragma once

#include <leaf/core/dynamic_object.hpp>
#include <leaf/core/identifier.hpp>
#include <leaf/core/string.hpp>
#include <leaf/resource/database.hpp>
#include <leaf/script/local_string.hpp>

#include <type_traits>

namespace lf {
	template<typename T, typename VNum>
	struct object_trait<identifier<T, VNum, void>> {
		static identifier<T, VNum, void> parse(const object& value) {
			return Database<T>::find(value.as<string>());
		}
	};

	struct PrototypeBase {
		template<typename Derived>
		static decltype(auto) base(Derived& value) {
			if constexpr (std::is_const_v<Derived>) {
				return static_cast<const PrototypeBase&>(value);
			} else {
				return static_cast<PrototypeBase&>(value);
			}
		}

		PrototypeBase(const dict& data) {
			string name;
			data.assign(
				field("name", name),
				field("local_name", local_name, name),
				field("local_description", local_description, local_name),
				field("order", order, "1")
			);
		}
		virtual ~PrototypeBase() = default;

		string order = "1";
		local_string local_name;
		local_string local_description;
	};

	template<>
	struct schema_trait<PrototypeBase> {
		static auto get(auto& value) {
			return group(
				field("order", value.order, value.order),
				field("local_name", value.local_name.key, value.local_name.key),
				field("local_description", value.local_description.key, value.local_description.key)
			);
		}
	};

	template<typename T>
	struct Prototype : public PrototypeBase {
		using prototype_marker = void;
		Prototype(const dict& data) : PrototypeBase(data) {}
		virtual ~Prototype() = default;
		using ID = T;
		ID id;

		virtual error load() {
			return {};
		};
	};
} // namespace lf
