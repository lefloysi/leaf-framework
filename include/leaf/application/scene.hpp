#pragma once

#include "leaf/core/error.hpp"
#include "leaf/core/filesystem.hpp"
#include "leaf/core/rate.hpp"
#include "leaf/core/string.hpp"
#include "leaf/core/time.hpp"
#include "leaf/application/window.hpp"
#include "leaf/script/state.hpp"

#include <RmlUi/Core/EventListener.h>

namespace Rml {
	class Context;
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
		Rml::ElementDocument& document();
		sol::state& script_state();
		/*! @brief Executes one Lua source file from Leaf's virtual filesystem. */
		error execute_script(fs::path_view path);

		void set_render_rate(frequency rate);
		frequency render_rate() const;

		Window& window();
		const Window& window() const;

	  private:
		void input(span<const input_event> events);

		void unload_document();
		bool execute_script(string_view source, string_view source_name);
		void ProcessEvent(Rml::Event& event) override;

		Window& display;
		Rml::Context* context = nullptr;
		Rml::ElementDocument* rml_document = nullptr;
		sol::state lua = CreateState();
		RateMeter frame_rate;
	};
} // namespace lf

