#include "leaf/application/scene.hpp"
#include "application/rml/backend.hpp"
#include <leaf/core/profiler.hpp>

#include <algorithm>
#include <leaf/core/exception.hpp>
#include <leaf/core/format.hpp>
#include <leaf/core/logging.hpp>
#include <leaf/core/register.hpp>
#include <leaf/core/scope.hpp>
#include <leaf/graphics/command_buffer.hpp>
#include <leaf/platform/platform.hpp>
#include <leaf/script/settings.hpp>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/RenderManager.h>
#include <RmlUi/Core/StringUtilities.h>

namespace lf {
	namespace {
		bool same_control(input_control first, input_control second) {
			return first.type == second.type && first.value == second.value;
		}

		string button_name(input_button button) {
			switch (button) {
			case BUTTON_LEFT: return "mouse-left";
			case BUTTON_RIGHT: return "mouse-right";
			case BUTTON_MIDDLE: return "mouse-middle";
			default: return lf::format("mouse-{}", static_cast<u16>(button));
			}
		}

		string special_key_name(input_key key) {
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

		string key_name(input_key key) {
			if (key >= KEY_A && key <= KEY_Z) { return string(1, static_cast<char>('a' + key - KEY_A)); }
			if (key >= KEY_0 && key <= KEY_9) { return string(1, static_cast<char>('0' + key - KEY_0)); }
			if (key >= KEY_F1 && key <= KEY_F24) { return lf::format("f{}", static_cast<int>(key - KEY_F1 + 1)); }
			if (key >= KEY_NUMPAD_0 && key <= KEY_NUMPAD_9) { return lf::format("numpad-{}", static_cast<int>(key - KEY_NUMPAD_0)); }
			const string name = special_key_name(key);
			return name.empty() ? lf::format("key-{}", static_cast<u16>(key)) : name;
		}

	} // namespace

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
			lua.new_usertype<SceneElement>("leaf.scene_element", "get_value", &SceneElement::get_value, "set_value", &SceneElement::set_value, "get_inner_rml", &SceneElement::get_inner_rml, "set_inner_rml", &SceneElement::set_inner_rml, "set_text", &SceneElement::set_text, "get_property", &SceneElement::get_property, "set_property", &SceneElement::set_property, "get_attribute", &SceneElement::get_attribute, "has_attribute", &SceneElement::has_attribute, "set_attribute", &SceneElement::set_attribute, "remove_attribute", &SceneElement::remove_attribute);
			sol::table window = lua.create_table();
			window.set_function("set_title", [&scene](string_view title) { scene.window().set_title(title); });
			window.set_function("set_fullscreen", [&scene](bool enabled) { scene.window().set_fullscreen(enabled); });
			window.set_function("get_fullscreen", [&scene] { return scene.window().fullscreen(); });
			window.set_function("set_vsync", [&scene](bool enabled) { scene.window().set_vsync(enabled); });
			window.set_function("close", [&scene] { scene.window().set_should_close(true); });

			sol::table interface = lua.create_named_table("scene");
			interface.set_function("document", [&scene](string_view id) { return SceneElement{ scene, id }; });
			interface.set_function("escape", [](string_view text) { return Rml::StringUtilities::EncodeRml(Rml::String{ text }); });
			interface["window"] = window;
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
		const dim2<u32> extent = this->display.extent();
		context = Rml::CreateContext(lf::format("scene-{}", static_cast<const void*>(this)), { static_cast<i32>(extent.width), static_cast<i32>(extent.height) });
		if (!context) {
			throw runtime_exception("failed to create RML scene context");
		}
		scope_exit rollback([this] { Rml::RemoveContext(context->GetName()); });
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
		refresh_keybinds();
	}

	void Scene::set_rml(const char* source, usize size) {
		set_rml(string_view(source, size));
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

	void Scene::refresh_keybinds() {
		std::erase_if(resolved_keybinds, [](const auto& binding) { return !binding.element; });
		const u64 revision = InputSettingsRevision();
		Rml::ElementList elements;
		rml_document->GetElementsByTagName(elements, "keybind");
		for (Rml::Element* element : elements) {
			const string mod = element->GetAttribute<Rml::String>("mod", "core");
			const string action = element->GetAttribute<Rml::String>("action", "");
			const string key = element->GetAttribute<Rml::String>("key", "");
			auto found = std::ranges::find_if(resolved_keybinds, [element](const auto& binding) { return binding.element.get() == element; });
			if (found != resolved_keybinds.end() && found->revision == revision && found->mod == mod && found->action == action && found->key == key) { continue; }
			KeyBinding binding;
			binding.element = element->GetObserverPtr();
			binding.mod = mod;
			binding.action = action;
			binding.key = key;
			binding.resolved_key = key;
			binding.revision = revision;
			if (!action.empty()) {
				const auto setting = LoadInputSetting(mod, action);
				if (setting) { binding.resolved_key = *setting; }
			}
			if (found == resolved_keybinds.end()) {
				resolved_keybinds.emplace_back(std::move(binding));
			} else {
				*found = std::move(binding);
			}
		}
	}

	bool Scene::binding_active(Rml::Element& binding, input_control control) const {
		Rml::Element* scope = binding.GetParentNode();
		if (!scope || !scope->IsVisible() || binding.HasAttribute("disabled")) { return false; }
		const bool mouse = control.type == INPUT_CONTROL_BUTTON;
		Rml::Element* target = mouse ? context->GetHoverElement() : context->GetFocusElement();
		if (!mouse && target && (target->GetTagName() == "input" || target->GetTagName() == "textarea" || target->GetTagName() == "select")) { return false; }
		if (scope == rml_document || (!mouse && scope->HasAttribute("input-fallback"))) { return true; }
		for (auto* ancestor = target; ancestor; ancestor = ancestor->GetParentNode()) {
			if (ancestor == scope) { return true; }
		}
		return false;
	}

	void Scene::keybinds(input_control control, bool down) {
		LF_PROFILE_SCOPE("input.keybinds");
		if (!down) {
			auto held = held_keybinds;
			std::erase_if(held_keybinds, [control](const auto& binding) { return same_control(binding.first, control); });
			for (const auto& binding : held) {
				if (same_control(binding.first, control) && binding.second) { binding.second->DispatchEvent("keyup", {}); }
			}
			return;
		}
		if (std::ranges::any_of(held_keybinds, [control](const auto& binding) { return same_control(binding.first, control); })) { return; }
		refresh_keybinds();
		const string name = control.type == INPUT_CONTROL_BUTTON ? button_name(static_cast<input_button>(control.value)) : key_name(static_cast<input_key>(control.value));
		const auto bindings = resolved_keybinds;
		for (const auto& resolved : bindings) {
			Rml::Element* binding = resolved.element.get();
			if (!binding || binding->GetOwnerDocument() != rml_document || resolved.resolved_key != name) { continue; }
			if (!binding_active(*binding, control)) { continue; }
			held_keybinds.emplace_back(control, binding->GetObserverPtr());
			binding->DispatchEvent("keydown", {});
		}
	}

	void Scene::control_input(const input_event& event) {
		if (event.control.type == INPUT_CONTROL_BUTTON) {
			const int button = rml_button(static_cast<input_button>(event.control.value));
			if (event.state == input_state::Pressed) {
				context->ProcessMouseButtonDown(button, rml_modifiers(event.modifiers));
				keybinds(event.control, true);
			}
			if (event.state == input_state::Released) {
				context->ProcessMouseButtonUp(button, rml_modifiers(event.modifiers));
				keybinds(event.control, false);
			}
		} else if (event.control.type == INPUT_CONTROL_KEY) {
			const auto key = rml_key(static_cast<input_key>(event.control.value));
			if (event.state == input_state::Pressed && (key == Rml::Input::KI_UNKNOWN || context->ProcessKeyDown(key, rml_modifiers(event.modifiers)))) {
				keybinds(event.control, true);
			}
			if (event.state == input_state::Released) {
				if (key != Rml::Input::KI_UNKNOWN) { context->ProcessKeyUp(key, rml_modifiers(event.modifiers)); }
				keybinds(event.control, false);
			}
		}
	}

	void Scene::input(span<const input_event> events) {
		if (!rml_document) {
			return;
		}
		for (const input_event& event : events) {
			switch (event.type) {
			case INPUT_EVENT_CONTROL:
				control_input(event);
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
						while (!held_keybinds.empty()) {
							keybinds(held_keybinds.back().first, false);
						}
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
		LF_PROFILE_SCOPE("frame.total");
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
		LF_PROFILE_SCOPE("frame.record-ui");
		const dim2<u32> extent = display.frame_extent();
		if (context->GetDimensions() != Rml::Vector2i{ static_cast<i32>(extent.width), static_cast<i32>(extent.height) }) {
			{ LF_PROFILE_SCOPE("ui.resize-dimensions"); context->SetDimensions({ static_cast<i32>(extent.width), static_cast<i32>(extent.height) }); }
		}
		{
			LF_PROFILE_SCOPE("ui.context-update");
			context->Update();
		}
		{
			LF_PROFILE_SCOPE("ui.renderer-begin");
			rml_backend->renderer.begin(commands, extent);
		}
		{
			LF_PROFILE_SCOPE("ui.context-render");
			context->Render();
		}
		{
			LF_PROFILE_SCOPE("ui.renderer-end");
			rml_backend->renderer.end();
		}

		return rml_backend->renderer.commands();
	}

	Scene::RecordedContent Scene::record(rt::view<rt::command_buffer> commands, const std::function<void()>& content) {
		return record(commands, content, {});
	}

	Scene::RecordedContent Scene::record(rt::view<rt::command_buffer> commands, const std::function<void()>& content, const std::function<void()>& interface) {
		LF_PROFILE_SCOPE("frame.record-segmented-ui");
		const dim2<u32> extent = display.extent();
		if (context->GetDimensions() != Rml::Vector2i{ static_cast<i32>(extent.width), static_cast<i32>(extent.height) }) {
			context->SetDimensions({ static_cast<i32>(extent.width), static_cast<i32>(extent.height) });
		}
		{
			LF_PROFILE_SCOPE("ui.context-update");
			context->Update();
		}
		rml_backend->renderer.begin(commands, extent);
		context->GetRenderManager().PrepareRender({ static_cast<i32>(extent.width), static_cast<i32>(extent.height) });
		content();
		RecordedContent recorded;
		recorded.content = rml_backend->renderer.checkpoint();
		{
			LF_PROFILE_SCOPE("ui.context-render");
			context->Render();
		}
		if (interface) {
			interface();
		}
		rml_backend->renderer.end();
		recorded.interface = rml_backend->renderer.interface_commands();
		return recorded;
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
		resolved_keybinds.clear();
		if (rml_document) {
			context->UnloadDocument(rml_document);
		}
		rml_document = nullptr;
	}
} // namespace lf
