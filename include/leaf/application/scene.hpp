#pragma once

#include "leaf/core/error.hpp"
#include "leaf/core/string.hpp"
#include "leaf/application/window.hpp"
#include "leaf/script/state.hpp"

#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/ObserverPtr.h>

namespace Rml {
	class Context;
	class Element;
	class ElementDocument;
	class Event;
}

namespace lf {
	class Scene final : private Rml::EventListener {
	  public:
		explicit Scene(Window& window);
		~Scene();

		void show();
		bool update();
		bool update(span<const input_event> events);
		void render();
		rt::view<rt::command_buffer> record(rt::view<rt::command_buffer> uploads);
		void set_rml(string_view source);
		void set_rml(const char* source, usize size) {
			set_rml(string_view(source, size));
		}
		Rml::ElementDocument& document();
		sol::state& script_state();
		error execute_script(string_view source);

		Window& window();
		const Window& window() const;

		void unload_document();
	  private:
		void input(span<const input_event> events);
		void keybinds(input_key key, bool down);
		vector<std::pair<input_key, Rml::ObserverPtr<Rml::Element>>> held_keybinds;


		void ProcessEvent(Rml::Event& event) override;

		Window& display;
		Rml::Context* context = nullptr;
		Rml::ElementDocument* rml_document = nullptr;
		sol::state lua = CreateState();
	};
} // namespace lf

