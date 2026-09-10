#include "leaf/application/scene.hpp"
#include "application/rml/backend.hpp"

#include <leaf/core/exception.hpp>
#include <leaf/core/format.hpp>
#include <leaf/core/logging.hpp>
#include <leaf/core/register.hpp>
#include <leaf/core/scope.hpp>
#include <leaf/graphics/command_buffer.hpp>
#include <leaf/platform/platform.hpp>
#include <leaf/script/settings.hpp>
#include <algorithm>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/StringUtilities.h>

namespace lf {
	namespace {
		string key_name(input_key key) {
			if (key >= KEY_A && key <= KEY_Z) {
				return string(1, static_cast<char>('a' + key - KEY_A));
			}
			if (key >= KEY_0 && key <= KEY_9) {
				return string(1, static_cast<char>('0' + key - KEY_0));
			}
			if (key >= KEY_F1 && key <= KEY_F24) {
				return lf::format("f{}", static_cast<int>(key - KEY_F1 + 1));
			}

			switch (key) {
			case KEY_ESCAPE: return "escape";
			case KEY_TAB: return "tab";
			case KEY_ENTER: return "enter";
			case KEY_SPACE: return "space";
			case KEY_BACKSPACE: return "backspace";
			case KEY_DELETE: return "delete";
			case KEY_INSERT: return "insert";
			case KEY_HOME: return "home";
			case KEY_END: return "end";
			case KEY_PAGE_UP: return "page-up";
			case KEY_PAGE_DOWN: return "page-down";
			case KEY_LEFT_ARROW: return "left";
			case KEY_RIGHT_ARROW: return "right";
			case KEY_UP_ARROW: return "up";
			case KEY_DOWN_ARROW: return "down";
			case KEY_ALT_LEFT: return "alt-left";
			case KEY_ALT_RIGHT: return "alt-right";
			case KEY_CTRL_LEFT: return "ctrl-left";
			case KEY_CTRL_RIGHT: return "ctrl-right";
			case KEY_SHIFT_LEFT: return "shift-left";
			case KEY_SHIFT_RIGHT: return "shift-right";
			case KEY_SUPER_LEFT: return "super-left";
			case KEY_SUPER_RIGHT: return "super-right";
			case KEY_BACKQUOTE: return "backquote";
			case KEY_BACKSLASH: return "backslash";
			case KEY_BRACKET_LEFT: return "bracket-left";
			case KEY_BRACKET_RIGHT: return "bracket-right";
			case KEY_COMMA: return "comma";
			case KEY_EQUAL: return "equal";
			case KEY_HASH: return "hash";
			case KEY_MINUS: return "minus";
			case KEY_PERIOD: return "period";
			case KEY_QUOTE: return "quote";
			case KEY_SEMICOLON: return "semicolon";
			case KEY_SLASH: return "slash";
			default: return {};
			}
		}

	}

	class SceneElement final {
	  public:
		SceneElement(Scene& scene, string_view id) : scene(&scene), id(id) {}

		string get_value() const {
			return form_control().GetValue();
		}

		void set_value(string_view value) {
			form_control().SetValue(Rml::String{ value });
		}

		string get_inner_rml() const {
			return element().GetInnerRML();
		}

		void set_inner_rml(string_view rml) {
			element().SetInnerRML(Rml::String{ rml });
		}

		void set_text(string_view text) {
			element().SetInnerRML(Rml::StringUtilities::EncodeRml(Rml::String{ text }));
		}

		string get_property(string_view name) const {
			const Rml::Property* property = element().GetProperty(Rml::String{ name });
			return property ? property->ToString() : string{};
		}

		bool set_property(string_view name, string_view value) {
			return element().SetProperty(Rml::String{ name }, Rml::String{ value });
		}

		string get_attribute(string_view name) const {
			return element().GetAttribute<Rml::String>(Rml::String{ name }, "");
		}

		bool has_attribute(string_view name) const {
			return element().HasAttribute(Rml::String{ name });
		}

		void set_attribute(string_view name, string_view value) {
			element().SetAttribute(Rml::String{ name }, Rml::String{ value });
		}

		void remove_attribute(string_view name) {
			element().RemoveAttribute(Rml::String{ name });
		}

	  private:
		Rml::Element& element() const {
			Rml::Element* result = scene->document().GetElementById(id);
			if (!result) {
				throw runtime_exception(lf::format("scene document has no element '{}'", id));
			}
			return *result;
		}

		Rml::ElementFormControl& form_control() const {
			Rml::ElementFormControl* result = rmlui_dynamic_cast<Rml::ElementFormControl*>(&element());
			if (!result) {
				throw runtime_exception(lf::format("scene element '{}' is not a form control", id));
			}
			return *result;
		}

		Scene* scene;
		string id;
	};

	class SceneScriptInstaller {
		static error install(Scene& scene) {
			sol::state& lua = scene.script_state();
			lua.new_usertype<SceneElement>("leaf.scene_element",
				"get_value", &SceneElement::get_value,
				"set_value", &SceneElement::set_value,
				"get_inner_rml", &SceneElement::get_inner_rml,
				"set_inner_rml", &SceneElement::set_inner_rml,
				"set_text", &SceneElement::set_text,
				"get_property", &SceneElement::get_property,
				"set_property", &SceneElement::set_property,
				"get_attribute", &SceneElement::get_attribute,
				"has_attribute", &SceneElement::has_attribute,
				"set_attribute", &SceneElement::set_attribute,
				"remove_attribute", &SceneElement::remove_attribute
			);
			sol::table window = lua.create_table_with(
				"set_title", [&scene](string_view title) { scene.window().set_title(title); },
				"set_fullscreen", [&scene](bool enabled) { scene.window().set_fullscreen(enabled); },
				"get_fullscreen", [&scene] { return scene.window().fullscreen(); },
				"set_vsync", [&scene](bool enabled) { scene.window().set_vsync(enabled); },
				"close", [&scene] { scene.window().set_should_close(true); }
			);
			lua["scene"] = lua.create_table_with(
				"document", [&scene](string_view id) { return SceneElement{ scene, id }; },
				"escape", [](string_view text) { return Rml::StringUtilities::EncodeRml(Rml::String{ text }); },
				"window", window
			);
			return {};
		}

		SceneScriptInstaller() {
			Register<Scene>::add(install);
		}

		static SceneScriptInstaller instance;
	};

	SceneScriptInstaller SceneScriptInstaller::instance{};

	static Rml::Input::KeyIdentifier rml_key(input_key key) {
		if (key >= KEY_A && key <= KEY_Z) {
			return static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_A + key - KEY_A);
		}
		if (key >= KEY_0 && key <= KEY_9) {
			return static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_0 + key - KEY_0);
		}
		if (key >= KEY_F1 && key <= KEY_F24) {
			return static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_F1 + key - KEY_F1);
		}
		switch (key) {
		case KEY_ESCAPE: return Rml::Input::KI_ESCAPE;
		case KEY_ENTER: return Rml::Input::KI_RETURN;
		case KEY_TAB: return Rml::Input::KI_TAB;
		case KEY_SPACE: return Rml::Input::KI_SPACE;
		case KEY_BACKSPACE: return Rml::Input::KI_BACK;
		case KEY_DELETE: return Rml::Input::KI_DELETE;
		case KEY_LEFT_ARROW: return Rml::Input::KI_LEFT;
		case KEY_RIGHT_ARROW: return Rml::Input::KI_RIGHT;
		case KEY_UP_ARROW: return Rml::Input::KI_UP;
		case KEY_DOWN_ARROW: return Rml::Input::KI_DOWN;
		case KEY_HOME: return Rml::Input::KI_HOME;
		case KEY_END: return Rml::Input::KI_END;
		case KEY_PAGE_UP: return Rml::Input::KI_PRIOR;
		case KEY_PAGE_DOWN: return Rml::Input::KI_NEXT;
		default: return Rml::Input::KI_UNKNOWN;
		}
	}

	static int rml_modifiers(input_modifiers modifiers) {
		int result = 0;
		if (modifiers.has(INPUT_MODIFIER_CTRL)) {
			result |= Rml::Input::KM_CTRL;
		}
		if (modifiers.has(INPUT_MODIFIER_SHIFT)) {
			result |= Rml::Input::KM_SHIFT;
		}
		if (modifiers.has(INPUT_MODIFIER_ALT)) {
			result |= Rml::Input::KM_ALT;
		}
		if (modifiers.has(INPUT_MODIFIER_SUPER)) {
			result |= Rml::Input::KM_META;
		}
		return result;
	}

	static int rml_button(input_button button) {
		switch (button) {
		case BUTTON_LEFT: return 0;
		case BUTTON_RIGHT: return 1;
		case BUTTON_MIDDLE: return 2;
		default: return static_cast<int>(button - BUTTON_1);
		}
	}

	Scene::Scene(Window& display) : display(display) {
		const dim2<u32> size = this->display.size();
		context = Rml::CreateContext(lf::format("scene-{}", static_cast<const void*>(this)), { static_cast<i32>(size.width), static_cast<i32>(size.height) });
		if (!context) {
			throw runtime_exception("failed to create RML scene context");
		}
		scope_exit rollback{ [this] { Rml::RemoveContext(context->GetName()); } };
		if (auto err = Register<Scene>::install(*this); err) {
			throw runtime_exception(err.message);
		}
		rollback.release();
	}

	Scene::~Scene() {
		unload_document();
		if (context) {
			Rml::RemoveContext(context->GetName());
		}
	}

	void Scene::show() {
		if (rml_document) {
			rml_document->Show();
		}
		display.show();
	}

	void Scene::set_rml(string_view source) {
		unload_document();
		rml_document = context->LoadDocumentFromMemory(Rml::String{ source });
		if (!rml_document) {
			throw runtime_exception("failed to load RML document");
		}
		for (Rml::EventId event : { Rml::EventId::Keydown, Rml::EventId::Keyup, Rml::EventId::Click, Rml::EventId::Change, Rml::EventId::Mousedown, Rml::EventId::Mousemove, Rml::EventId::Mouseup }) {
			rml_document->AddEventListener(event, this);
		}
		rml_document->Show();
	}

	Rml::ElementDocument& Scene::document() {
		if (!rml_document) {
			throw runtime_exception("scene has no RML document");
		}
		return *rml_document;
	}

	sol::state& Scene::script_state() {
		return lua;
	}

	error Scene::execute_script(string_view source) {
		auto result = lua.safe_script(source, sol::script_pass_on_error);
		if (!result.valid()) {
			const sol::error failure = result;
			return { generic_errc::parse_error, failure.what() };
		}
		return {};
	}

	Window& Scene::window() {
		return display;
	}

	const Window& Scene::window() const {
		return display;
	}

	void Scene::ProcessEvent(Rml::Event& event) {
		const Rml::String& name = event.GetType();
		for (Rml::Element* element = event.GetTargetElement(); element; element = element->GetParentNode()) {
			if (element->HasAttribute("disabled")) {
				return;
			}
			const Rml::String source = element->GetAttribute<Rml::String>(name, "");
			if (!source.empty()) {
				if (const auto error = execute_script(source)) {
					log::Error("[scene] {}: {}", name, error.message);
				}
				event.StopPropagation();
				return;
			}
		}
	}

	void Scene::keybinds(input_key key, bool down) {
		if (!down) {
			auto held = held_keybinds;
			std::erase_if(held_keybinds, [key](const auto& binding) { return binding.first == key; });
			for (const auto& binding : held) {
				if (binding.first == key && binding.second) { binding.second->DispatchEvent("keyup", {}); }
			}
			return;
		}
		if (std::ranges::any_of(held_keybinds, [key](const auto& binding) { return binding.first == key; })) { return; }
		Rml::Element* focus = context->GetFocusElement();
		if (focus && (focus->GetTagName() == "input" || focus->GetTagName() == "textarea" || focus->GetTagName() == "select")) { return; }
		Rml::ElementList bindings;
		rml_document->GetElementsByTagName(bindings, "keybind");
		for (Rml::Element* binding : bindings) {
			Rml::Element* scope = binding->GetParentNode();
			if (!scope || !scope->IsVisible() || binding->HasAttribute("disabled")) { continue; }
			bool active = scope == rml_document || scope->HasAttribute("input-fallback");
			for (auto* ancestor = focus; ancestor; ancestor = ancestor->GetParentNode()) {
				active |= ancestor == scope;
			}
			if (!active) { continue; }
			string name = binding->GetAttribute<Rml::String>("key", "");
			const string action = binding->GetAttribute<Rml::String>("action", "");
			if (!action.empty()) {
				auto setting = LoadInputSetting(binding->GetAttribute<Rml::String>("mod", "core"), action);
				if (setting) { name = *setting; }
			}
			if (name != key_name(key)) { continue; }
			held_keybinds.emplace_back(key, binding->GetObserverPtr());
			binding->DispatchEvent("keydown", {});
		}
	}

	void Scene::input(span<const input_event> events) {
		if (!rml_document) {
			return;
		}
		for (const input_event& event : events) {
			switch (event.type) {
			case INPUT_EVENT_CONTROL:
				if (event.control.type == INPUT_CONTROL_BUTTON) {
					const int button = rml_button(static_cast<input_button>(event.control.value));
					if (event.state == input_state::Pressed) {
						context->ProcessMouseButtonDown(button, rml_modifiers(event.modifiers));
					}
					if (event.state == input_state::Released) {
						context->ProcessMouseButtonUp(button, rml_modifiers(event.modifiers));
					}
				} else if (event.control.type == INPUT_CONTROL_KEY) {
					const input_key code = static_cast<input_key>(event.control.value);
					const Rml::Input::KeyIdentifier key = rml_key(code);
					if (event.state == input_state::Pressed && (key == Rml::Input::KI_UNKNOWN || context->ProcessKeyDown(key, rml_modifiers(event.modifiers)))) {
						keybinds(code, true);
					}
					if (event.state == input_state::Released) {
						if (key != Rml::Input::KI_UNKNOWN) { context->ProcessKeyUp(key, rml_modifiers(event.modifiers)); }
						keybinds(code, false);
					}
				}
				break;
			case INPUT_EVENT_CURSOR_MOVE:
				context->ProcessMouseMove(static_cast<i32>(event.position.x), static_cast<i32>(event.position.y), rml_modifiers(event.modifiers));
				break;
			case INPUT_EVENT_SCROLL:
				context->ProcessMouseWheel({ -event.delta.x, -event.delta.y }, rml_modifiers(event.modifiers));
				break;
			case INPUT_EVENT_TEXT:
				context->ProcessTextInput(static_cast<Rml::Character>(event.character));
				break;
			case INPUT_EVENT_CURSOR_ENTER:
			case INPUT_EVENT_FOCUS:
				if (event.state == input_state::Up) {
					context->ProcessMouseLeave();
					if (event.type == INPUT_EVENT_FOCUS) {
						while (!held_keybinds.empty()) { keybinds(held_keybinds.back().first, false); }
						if (auto* focus = context->GetFocusElement()) { focus->DispatchEvent("blur", {}); }
					}
				}
				break;
			case INPUT_EVENT_DROP:
				break;
			}
		}
		display.update_input();
	}

	void Scene::render() {
		if (!display.drawable()) {
			return;
		}
		const auto commands = display.begin_frame();
		if (!commands) {
			return;
		}
		const auto ui = record(commands);
		display.begin_rendering();
		rt::Cmd::Execute(commands, ui);
		display.end_frame();
	}

	rt::view<rt::command_buffer> Scene::record(rt::view<rt::command_buffer> commands) {
		const dim2<u32> size = display.size();
		if (context->GetDimensions() != Rml::Vector2i{ static_cast<i32>(size.width), static_cast<i32>(size.height) }) {
			context->SetDimensions({ static_cast<i32>(size.width), static_cast<i32>(size.height) });
		}
		context->Update();
		rml_backend->renderer.begin(commands, size);
		context->Render();
		rml_backend->renderer.end();

		return rml_backend->renderer.commands();
	}

	bool Scene::update() {
		const auto events = display.input_events();
		return update(events);
	}
	bool Scene::update(span<const input_event> events) {
		input(events);
		return !display.should_close();
	}

	void Scene::unload_document() {
		held_keybinds.clear();
		if (rml_document) {
			context->UnloadDocument(rml_document);
		}
		rml_document = nullptr;
	}
} // namespace lf


