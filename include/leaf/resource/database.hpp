#pragma once

#include "leaf/core/binary.hpp"
#include "leaf/core/dynamic_object.hpp"
#include "leaf/core/error.hpp"
#include "leaf/core/indexed_map.hpp"
#include "leaf/core/vector.hpp"

namespace lf {
	template<typename T>
	struct Database;

	template<typename Prototype, typename VNum>
	concept prototype_identifier = requires(identifier<Prototype, VNum, void> id, string_view name) {
		typename Prototype::ID;
		typename Prototype::prototype_marker;
		requires std::same_as<typename Prototype::ID, identifier<Prototype, VNum, void>>;
		{ Database<Prototype>::name(id) } -> std::same_as<string_view>;
		{ Database<Prototype>::find(name) } -> std::same_as<typename Prototype::ID>;
		{ Database<Prototype>::type() } -> std::same_as<string_view>;
	};

	template<typename T>
	struct Database {
		static vector<T> prototypes;
		static indexed_map<string> names;
		static void create(string_view name);
		static void init(string_view name, const dict& data);
		static void clear();
		static size_t count();
		static string_view name(typename T::ID id);
		static error load_assets();
		static typename T::ID find(string_view name);
		static T& get(typename T::ID id);
		static string_view type();
	};

	template<typename T>
	vector<T> Database<T>::prototypes = {};
	template<typename T>
	indexed_map<string> Database<T>::names = {};

	template<typename T>
	void Database<T>::create(string_view name) {
		if (!names.insert(string(name)).second) {
			throw runtime_exception(lf::format("duplicate {} prototype '{}'", type(), name));
		}
	}

	template<typename T>
	void Database<T>::init(string_view name, const dict& data) {
		const auto name_it = names.find(string(name));
		if (name_it == names.end()) {
			throw runtime_exception(lf::format("{} prototype '{}' was not created", type(), name));
		}
		const size_t index = names.index_of(name_it);
		if (index != prototypes.size()) {
			throw runtime_exception(lf::format("{} prototype '{}' was initialized out of creation order", type(), name));
		}
		T& prototype = prototypes.emplace_back(data);
		prototype.id = typename T::ID{ static_cast<typename T::ID::vnum_t>(index + 1) };
	}

	template<typename T>
	void Database<T>::clear() {
		prototypes.clear();
		names.clear();
	}
	template<typename T>
	size_t Database<T>::count() { return prototypes.size(); }

	template<typename T>
	string_view Database<T>::name(typename T::ID id) {
		if (!id || id.get() > prototypes.size()) {
			throw runtime_exception(lf::format("{} prototype id {} is invalid", type(), id.get()));
		}
		return names[static_cast<size_t>(id.get() - 1)];
	}

	template<typename T>
	error Database<T>::load_assets() {
		for (T& prototype : prototypes) {
			if (error result{ prototype.load() }) {
				return result;
			}
		}
		return {};
	}

	template<typename T>
	typename T::ID Database<T>::find(string_view name) {
		using id_type = typename T::ID;
		const auto it = names.find(string(name));
		return it == names.end() ? id_type{} : id_type{ static_cast<typename id_type::vnum_t>(names.index_of(it) + 1) };
	}

	template<typename T>
	T& Database<T>::get(typename T::ID id) {
		if (!id || id.get() > prototypes.size()) {
			throw runtime_exception(lf::format("{} prototype id {} out of range", type(), id.get()));
		}
		return prototypes[id.get() - 1];
	}

	template<typename T>
	string_view Database<T>::type() { return T::type(); }

	template<bin::byte_stream Stream, bin::specialization_of<identifier> ID>
		requires std::same_as<typename std::remove_cvref_t<ID>::gnum_t, void> &&
				 prototype_identifier<typename std::remove_cvref_t<ID>::value_type, typename std::remove_cvref_t<ID>::vnum_t> &&
				 (!bin::readable_byte_stream<Stream> || !std::is_const_v<ID>)
	error process(Stream& stream, ID& id) {
		using id_type = std::remove_cvref_t<ID>;
		using prototype_type = typename id_type::value_type;
		string name;
		if constexpr (bin::writable_byte_stream<Stream>) {
			if (id) {
				name = Database<prototype_type>::name(id);
			}
		}
		if (error result = stream(field("name", name))) {
			return result;
		}
		if constexpr (bin::readable_byte_stream<Stream>) {
			id = name.empty() ? id_type{} : Database<prototype_type>::find(name);
			if (!name.empty() && !id) {
				return error(generic_errc::parse_error, lf::format("save requires missing {} prototype '{}'", Database<prototype_type>::type(), name));
			}
		}
		return {};
	}
} // namespace lf
