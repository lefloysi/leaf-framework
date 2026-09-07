#pragma once
#include <leaf/core/register.hpp>
#include "leaf/core/dynamic_object.hpp"
#include "leaf/core/exception.hpp"
#include "leaf/core/format.hpp"
#include "leaf/core/string.hpp"
#include "leaf/core/vector.hpp"
#include "leaf/resource/database.hpp"
#include "leaf/resource/prototype.hpp"

namespace lf {
	struct PrototypeIdentity {
		string_view type;
		u64 id = 0;
		string_view name;
		string_view local_name_key;
		string_view local_description_key;
	};

	struct PrototypeTypeFunctions {
		void (*clear)() = nullptr;
		void (*create)(string_view) = nullptr;
		void (*init)(string_view, const dict&) = nullptr;
		string_view (*type)() = nullptr;
		size_t (*count)() = nullptr;
		PrototypeIdentity (*identity)(size_t) = nullptr;
		error (*load_assets)() = nullptr;
	};

	struct PrototypeTypeRegistry : Singleton<PrototypeTypeRegistry> {
		template<typename T>
		static void RegisterType() {
			using db = Database<T>;
			static_assert(requires(T& prototype) { schema_trait<T>::get(prototype); }, "registered prototype types must provide lf::schema_trait<T>::get(T&)");
			const auto identity = [](size_t index) {
				const auto id = typename T::ID{static_cast<typename T::ID::vnum_t>(index + 1)};
				const T& prototype = db::get(id);
				return PrototypeIdentity{
					db::type(), static_cast<u64>(id.get()), db::name(id), prototype.local_name.key, prototype.local_description.key,
				};
			};
			functions.push_back({&db::clear, &db::create, &db::init, &db::type, &db::count, identity, &db::load_assets});
		}

		inline static vector<PrototypeTypeFunctions> functions = {};
	};

	inline error LoadAssetPrototypes() {
		for (const PrototypeTypeFunctions& functions : PrototypeTypeRegistry::functions) {
			if (functions.load_assets) {
				if (error result{ functions.load_assets() }) {
					return result;
				}
			}
		}
		return {};
	}
} // namespace lf

