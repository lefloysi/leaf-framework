#pragma once

#include "leaf/core/array.hpp"
#include "leaf/core/identifier.hpp"
#include "leaf/core/optional.hpp"
#include "leaf/core/types.hpp"
#include "leaf/core/vector.hpp"

#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>

namespace lf {
	template<typename Container, typename Handle>
	class entity {
	  public:
		using ID = Handle;

		explicit operator bool() const {
			return value;
		}

		operator Handle() const {
			return value;
		}

		bool operator==(Handle other) const {
			return value == other;
		}

		Handle id() const {
			return value;
		}

		template<typename T>
		T& emplace() {
			return owner->template emplace<T>(value);
		}

		template<typename T>
		bool has() const {
			return owner->template has<T>(value);
		}

		template<typename T>
		T& get() {
			return *owner->template find<T>(value);
		}

		template<typename T>
		const T& get() const {
			return *static_cast<const Container*>(owner)->template find<T>(value);
		}

		entity(Container& owner, Handle value)
			: owner(&owner), value(value) {}

	  private:
		Container* owner;
		Handle value;
	};

	template<instantiation_of<identifier> Handle, typename... Component>
	class component_container {
	  public:
		using handle = Handle;

		class entity {
		  public:
			using ID = Handle;

			explicit operator bool() const {
				return value;
			}

			operator Handle() const {
				return value;
			}

			bool operator==(Handle other) const {
				return value == other;
			}

			Handle id() const {
				return value;
			}

			template<typename T>
			T& emplace() {
				return owner->emplace<T>(value);
			}

			template<typename T>
			bool has() const {
				return owner->has<T>(value);
			}

			template<typename T>
			T& get() {
				return *owner->find<T>(value);
			}

			template<typename T>
			const T& get() const {
				return *static_cast<const component_container*>(owner)->find<T>(value);
			}

		  private:
			entity(component_container& owner, Handle value)
				: owner(&owner), value(value) {}

			component_container* owner;
			Handle value;

			friend component_container;
		};

		struct bundle {
			template<typename T>
			optional<T>& component() {
				return std::get<optional<T>>(values);
			}

			template<typename T>
			const optional<T>& component() const {
				return std::get<optional<T>>(values);
			}

		  private:
			std::tuple<optional<Component>...> values;
			friend component_container;
		};

		struct slot {
			array<size_t, sizeof...(Component)> components{};
		};

		component_container() {
			(std::get<component_index<Component>>(components).first.emplace_back(), ...);
			(std::get<component_index<Component>>(components).second.emplace_back(), ...);
		}

		entity create(bundle bundle = {}) {
			size_t index;
			if (free.empty()) {
				index = slots.size();
				slots.emplace_back();
			} else {
				index = free.back();
				free.pop_back();
			}
			slots[index] = {};
			const Handle handle{ index };
			([&] {
				auto& component = std::get<optional<Component>>(bundle.values);
				if (component) {
					add(handle, std::move(*component));
				}
			}(),
			 ...);
			return entity{ *this, handle };
		}

		entity get(Handle value) {
			return entity{ *this, value };
		}

		template<typename T>
		T& emplace(Handle entity) {
			static_assert(component_index<T> < sizeof...(Component));
			return add<T>(entity);
		}

		void destroy(Handle value) {
			(erase<Component>(value), ...);
			slots[usize(value)] = {};
			free.push_back(usize(value));
		}

		void clear() {
			*this = component_container{};
		}

		template<typename T>
		bool has(Handle value) const {
			return value && usize(value) < slots.size() && slots[usize(value)].components[component_index<T>] != 0;
		}

		template<typename T>
		T& add(Handle value, T component = {}) {
			auto& [values, owners] = std::get<component_index<T>>(components);
			size_t& index = slots[usize(value)].components[component_index<T>];
			if (index != 0) {
				return values[index];
			}
			index = values.size();
			owners.push_back(usize(value));
			return values.emplace_back(std::move(component));
		}

		template<typename T>
		auto find(Handle value) {
			auto& values = std::get<component_index<T>>(components).first;
			if (!has<T>(value)) { return values.end(); }
			const size_t index = slots[usize(value)].components[component_index<T>];
			return index == 0 ? values.end() : values.begin() + index;
		}

		template<typename T>
		auto find(Handle value) const {
			const auto& values = std::get<component_index<T>>(components).first;
			if (!has<T>(value)) { return values.end(); }
			const size_t index = slots[usize(value)].components[component_index<T>];
			return index == 0 ? values.end() : values.begin() + index;
		}

		template<typename T>
		auto end() {
			return std::get<component_index<T>>(components).first.end();
		}

		template<typename T>
		auto end() const {
			return std::get<component_index<T>>(components).first.end();
		}

		template<typename T, typename F>
		void each(F&& function) {
			auto& [values, owners] = std::get<component_index<T>>(components);
			for (size_t index = 1; index < values.size(); ++index) {
				function(Handle{owners[index]}, values[index]);
			}
		}

		template<typename T, typename F>
		void each(F&& function) const {
			const auto& [values, owners] = std::get<component_index<T>>(components);
			for (size_t index = 1; index < values.size(); ++index) {
				function(Handle{owners[index]}, values[index]);
			}
		}

		template<typename Function>
		void each_component(Function&& function) {
			(function.template operator()<Component>(), ...);
		}

		template<typename T>
		void erase(Handle value) {
			size_t index = slots[usize(value)].components[component_index<T>];
			if (index == 0) {
				return;
			}

			auto& [values, owners] = std::get<component_index<T>>(components);
			size_t last = values.size() - 1;
			size_t dense = index;
			if (dense != last) {
				values[dense] = std::move(values[last]);
				owners[dense] = owners[last];
				slots[owners[dense]].components[component_index<T>] = index;
			}
			values.pop_back();
			owners.pop_back();
			slots[usize(value)].components[component_index<T>] = 0;
		}

	  private:
		template<typename T>
		static constexpr size_t component_index = [] {
			constexpr array matches{ std::same_as<T, Component>... };
			for (size_t index = 0; index < matches.size(); ++index) {
				if (matches[index]) {
					return index;
				}
			}
			return matches.size();
		}();

		vector<slot> slots;
		vector<size_t> free;
		std::tuple<std::pair<vector<Component>, vector<size_t>>...> components;
	};

} // namespace lf
