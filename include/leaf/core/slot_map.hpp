#pragma once

#include "leaf/core/identifier.hpp"
#include "leaf/core/vector.hpp"

#include <algorithm>
#include <memory>
#include <utility>

namespace lf {
	template<instantiation_of<identifier> Handle, typename Value>
	class slot_map {
	  public:
		slot_map() {
			slots.emplace_back();
		}

		Handle reserve() {
			if (!free.empty()) {
				const Handle id{ free.back() };
				free.pop_back();
				return id;
			}
			slots.emplace_back();
			return Handle{ slots.size() - 1 };
		}

		template<typename... Argument>
		Value& emplace(Handle id, Argument&&... argument) {
			if (id >= slots.size()) {
				const usize first = slots.size();
				slots.resize(id + 1);
				for (usize index = first; index < id; ++index) {
					free.emplace_back(index);
				}
			} else {
				std::erase(free, static_cast<usize>(id));
			}
			auto& slot = slots.at(id);
			slot = std::make_unique<Value>(std::forward<Argument>(argument)...);
			return *slot;
		}

		Value* find(Handle id) {
			return id && id < slots.size() ? slots[id].get() : nullptr;
		}

		const Value* find(Handle id) const {
			return const_cast<slot_map*>(this)->find(id);
		}

		void erase(Handle id) {
			if (!find(id)) {
				return;
			}
			slots[id].reset();
			free.emplace_back(id);
		}

		void clear() {
			slots.clear();
			free.clear();
			slots.emplace_back();
		}

		usize size() const {
			return slots.size() - free.size() - 1;
		}

		template<typename Function>
		void each(Function&& function) {
			for (usize index = 1; index < slots.size(); ++index) {
				if (slots[index]) {
					function(Handle{ index }, *slots[index]);
				}
			}
		}

		template<typename Function>
		void each(Function&& function) const {
			for (usize index = 1; index < slots.size(); ++index) {
				if (slots[index]) {
					function(Handle{ index }, *slots[index]);
				}
			}
		}

	  private:
		vector<std::unique_ptr<Value>> slots;
		vector<usize> free;
	};
} // namespace lf

#include "leaf/core/binary.hpp"

namespace lf::bin {
	template<byte_stream Stream, instantiation_of<identifier> Handle, typename Value>
	error process(Stream& stream, slot_map<Handle, Value>& values) {
		vector<Value> serialized;
		if constexpr (writable_byte_stream<Stream>) {
			values.each([&](Handle, const Value& value) {
				serialized.emplace_back(value);
			});
			return stream(lf::field("values", serialized));
		} else {
			if (auto error = stream(lf::field("values", serialized))) {
				return error;
			}
			values.clear();
			for (Value& value : serialized) {
				values.emplace(value.id, std::move(value));
			}
			return {};
		}
	}

	template<byte_stream Stream, instantiation_of<identifier> Handle, typename Value>
	error process(Stream& stream, const slot_map<Handle, Value>& values) {
		vector<Value> serialized;
		values.each([&](Handle, const Value& value) {
			serialized.emplace_back(value);
		});
		return stream(lf::field("values", serialized));
	}
} // namespace lf::bin
