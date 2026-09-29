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
	void component_container<Handle, Component...>::Changes::record_all(const component_container<Handle, Component...>& source) {
		for (size_t index = 1; index < source.slots.size(); ++index) {
			const Handle id{ index };
			if (source.exists(id)) {
				record(source, id);
			}
		}
	}

	template<instantiation_of<identifier> Handle, typename... Component>
	void component_container<Handle, Component...>::Changes::merge(const Changes& other) {
		for (const auto& change : other.entities) {
			auto found = std::ranges::find(entities, change.id, &Entity::id);
			if (found == entities.end()) {
				entities.push_back(change);
			} else {
				*found = change;
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
