namespace lf {
	template<instantiation_of<identifier> Handle, typename... Component>
	void component_container<Handle, Component...>::Changes::record(const component_container<Handle, Component...>& source, Handle id) {
		auto found = std::ranges::find(entities, id, &Entity::id);
		if (found == entities.end()) {
			entities.emplace_back(Entity{ id });
			found = std::prev(entities.end());
		}

		found->present = source.exists(id);
		if (!found->present) {
			return;
		}

		std::apply([&]<typename... Value>(optional<Value>&... values) {
			((values = source.template has<Value>(id)
						   ? optional<Value>{ *source.template find<Value>(id) }
						   : optional<Value>{}),
			 ...);
		},
				   found->components);
	}

	template<instantiation_of<identifier> Handle, typename... Component>
	void component_container<Handle, Component...>::Changes::record_many(const component_container<Handle, Component...>& source, span<const Handle> ids) {
		if (ids.empty()) { return; }
		struct Hash {
			size_t operator()(Handle value) const { return std::hash<typename Handle::vnum_t>{}(value.get()); }
		};
		std::unordered_map<Handle, size_t, Hash> index;
		index.reserve(entities.size() + ids.size());
		for (size_t i = 0; i < entities.size(); ++i) { index.try_emplace(entities[i].id, i); }
		vector<bool> available(source.slots.size(), true);
		for (const size_t free_index : source.free) { available[free_index] = false; }
		for (const Handle id : ids) {
			const auto [found, inserted] = index.try_emplace(id, entities.size());
			if (inserted) { entities.emplace_back(Entity{ id }); }
			auto& entry = entities[found->second];
			entry.present = id && size_t(id) < available.size() && available[size_t(id)];
			if (!entry.present) { continue; }
			std::apply([&]<typename... Value>(optional<Value>&... values) {
				((values = source.template has<Value>(id) ? optional<Value>{ *source.template find<Value>(id) } : optional<Value>{}), ...);
			}, entry.components);
		}
	}

	template<instantiation_of<identifier> Handle, typename... Component>
	void component_container<Handle, Component...>::Changes::record_all(const component_container<Handle, Component...>& source) {
		vector<Handle> ids;
		ids.reserve(source.slots.size());
		vector<bool> available(source.slots.size(), true);
		for (const size_t free_index : source.free) { available[free_index] = false; }
		for (size_t index = 0; index < source.slots.size(); ++index) {
			const Handle id{ index };
			if (id && available[index]) {
				ids.push_back(id);
			}
		}
		record_many(source, ids);
	}

	template<instantiation_of<identifier> Handle, typename... Component>
	void component_container<Handle, Component...>::Changes::merge(const Changes& other) {
		if (other.entities.empty()) { return; }
		struct Hash {
			size_t operator()(Handle value) const { return std::hash<typename Handle::vnum_t>{}(value.get()); }
		};
		std::unordered_map<Handle, size_t, Hash> index;
		index.reserve(entities.size() + other.entities.size());
		for (size_t i = 0; i < entities.size(); ++i) { index.try_emplace(entities[i].id, i); }
		for (const auto& change : other.entities) {
			const auto [found, inserted] = index.try_emplace(change.id, entities.size());
			if (inserted) {
				entities.push_back(change);
			} else {
				entities[found->second] = change;
			}
		}
	}

	template<instantiation_of<identifier> Handle, typename... Component>
	void component_container<Handle, Component...>::Changes::apply(component_container<Handle, Component...>& destination) const {
		for (const auto& change : entities) {
			if (!change.present) {
				destination.destroy_if_present(change.id);
				continue;
			}

			destination.ensure(change.id);
			std::apply([&]<typename... Value>(const optional<Value>&... values) {
				([&] {
					if (values) {
						if (destination.template has<Value>(change.id)) {
							*destination.template find<Value>(change.id) = *values;
						} else {
							destination.template add<Value>(change.id, *values);
						}
					} else {
						destination.template erase<Value>(change.id);
					}
				}(),
				 ...);
			},
					   change.components);
		}
	}

	template<instantiation_of<identifier> Handle, typename... Component>
	void component_container<Handle, Component...>::Changes::reset() {
		entities.clear();
	}

	template<instantiation_of<identifier> Handle, typename... Component>
	bool component_container<Handle, Component...>::Changes::empty() const {
		return entities.empty();
	}
} // namespace lf
