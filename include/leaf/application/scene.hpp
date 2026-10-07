#pragma once

#include "leaf/application/window.hpp"
#include "leaf/core/error.hpp"
#include "leaf/core/string.hpp"
#include "leaf/script/state.hpp"

#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/ObserverPtr.h>

#include <functional>

namespace Rml {
	class Context;
	class Element;
	class ElementDocument;
	class Event;
} // namespace Rml

namespace lf {
	class Scene final : private Rml::EventListener {
	  public:
		struct RecordedContent {
			rt::view<rt::command_buffer> content;
			rt::view<rt::command_buffer> interface;
		};

		explicit Scene(Window& window);
		~Scene();

		void show();
		bool update();
		bool update(span<const input_event> events);
		void render();
		rt::view<rt::command_buffer> record(rt::view<rt::command_buffer> uploads);
		RecordedContent record(rt::view<rt::command_buffer> uploads, const std::function<void()>& content);
		RecordedContent record(rt::view<rt::command_buffer> uploads, const std::function<void()>& content, const std::function<void()>& interface);
		void set_rml(string_view source);
		void set_rml(const char* source, usize size);
		Rml::ElementDocument& document();
		sol::state& script_state();
		error execute_script(string_view source);

		Window& window();
		const Window& window() const;

		void unload_document();

	  private:
		void input(span<const input_event> events);
		void keybinds(input_control control, bool down);
		void control_input(const input_event& event);
		bool binding_active(Rml::Element& binding, input_control control) const;
		void refresh_keybinds();
		struct KeyBinding {
			Rml::ObserverPtr<Rml::Element> element;
			string mod;
			string action;
			string key;
			string resolved_key;
			u64 revision = 0;
		};
		vector<KeyBinding> resolved_keybinds;
		vector<std::pair<input_control, Rml::ObserverPtr<Rml::Element>>> held_keybinds;

		void ProcessEvent(Rml::Event& event) override;

		Window& display;
		Rml::Context* context = nullptr;
		Rml::ElementDocument* rml_document = nullptr;
		sol::state lua = CreateState();
	};
} // namespace lf
