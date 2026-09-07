#include "leaf/application/scene.hpp"
#include "application/rml/backend.hpp"

#include <leaf/core/exception.hpp>
#include <leaf/core/filesystem.hpp>
#include <leaf/core/format.hpp>
#include <leaf/core/logging.hpp>
#include <leaf/core/register.hpp>
#include <leaf/core/scope.hpp>
#include <leaf/graphics/command_buffer.hpp>
#include <leaf/platform/platform.hpp>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/StringUtilities.h>

namespace lf {
	class SceneElement final {
	  public:
		SceneElement(Scene& scene, string_view id) : scene(&scene), id(id) {}

		string get_value() const {
			return form_control().GetValue();
		}

		void set_value(string_view value) {
			form_control().SetValue(Rml::String{ value });
		}

		bool get_checked() const {
			return element().HasAttribute("checked");
		}

		void set_checked(bool checked) {
			if (checked) {
				element().SetAttribute("checked", "checked");
			} else {
				element().RemoveAttribute("checked");
			}
		}

		bool get_disabled() const {
			return element().HasAttribute("disabled");
		}

		void set_disabled(bool disabled) {
			if (disabled) {
				element().SetAttribute("disabled", "disabled");
			} else {
				element().RemoveAttribute("disabled");
			}
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

		void set_attribute(string_view name, string_view value) {
			element().SetAttribute(Rml::String{ name }, Rml::String{ value });
		}

		void remove_attribute(string_view name) {
			element().RemoveAttribute(Rml::String{ name });
		}

	  private:
		Rml::Element& element() const {
			Rml::Element* result = scene->document().GetElementById(id);
			if (!result) throw runtime_exception(lf::format("scene document has no element '{}'", id));
			return *result;
		}

		Rml::ElementFormControl& form_control() const {
			Rml::ElementFormControl* result = rmlui_dynamic_cast<Rml::ElementFormControl*>(&element());
			if (!result) throw runtime_exception(lf::format("scene element '{}' is not a form control", id));
			return *result;
		}

		Scene* scene;
		string id;
	};

	const bool scene_script_registration{ [] {
		Register<Scene>::add([](Scene& scene) -> error {
			sol::state& lua = scene.script_state();
			lua.new_usertype<SceneElement>("leaf.scene_element",
				"get_value", &SceneElement::get_value,
				"set_value", &SceneElement::set_value,
				"get_checked", &SceneElement::get_checked,
				"set_checked", &SceneElement::set_checked,
				"get_disabled", &SceneElement::get_disabled,
				"set_disabled", &SceneElement::set_disabled,
				"get_inner_rml", &SceneElement::get_inner_rml,
				"set_inner_rml", &SceneElement::set_inner_rml,
				"set_text", &SceneElement::set_text,
				"get_property", &SceneElement::get_property,
				"set_property", &SceneElement::set_property,
				"get_attribute", &SceneElement::get_attribute,
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
		});
		return true;
	}() };

	static Rml::Input::KeyIdentifier rml_key(input_key key) {
		if (key >= KEY_A && key <= KEY_Z) return static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_A + key - KEY_A);
		if (key >= KEY_0 && key <= KEY_9) return static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_0 + key - KEY_0);
		if (key >= KEY_F1 && key <= KEY_F24) return static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_F1 + key - KEY_F1);
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
		if (modifiers.has(INPUT_MODIFIER_CTRL)) result |= Rml::Input::KM_CTRL;
		if (modifiers.has(INPUT_MODIFIER_SHIFT)) result |= Rml::Input::KM_SHIFT;
		if (modifiers.has(INPUT_MODIFIER_ALT)) result |= Rml::Input::KM_ALT;
		if (modifiers.has(INPUT_MODIFIER_SUPER)) result |= Rml::Input::KM_META;
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

	Scene::Scene(Window& display)
		: display(display) {
		const dim2<u32> size = this->display.size();
		context = Rml::CreateContext(lf::format("scene-{}", static_cast<const void*>(this)), { static_cast<i32>(size.width), static_cast<i32>(size.height) });
		if (!context) throw runtime_exception("failed to create RML scene context");
		scope_exit rollback{ [this] { Rml::RemoveContext(context->GetName()); } };
		if (auto err = Register<Scene>::install(*this); err) {
			throw runtime_exception(err.message);
		}
		rollback.release();
	}

	Scene::~Scene() {
		unload_document();
		if (context) Rml::RemoveContext(context->GetName());
	}

	void Scene::show() {
		if (rml_document) rml_document->Show();
		display.show();
	}

	void Scene::set_rml(string_view source) {
		unload_document();
		rml_document = context->LoadDocumentFromMemory(Rml::String{ source });
		if (!rml_document) throw runtime_exception("failed to load RML document");
		for (Rml::EventId event : { Rml::EventId::Click, Rml::EventId::Change, Rml::EventId::Mousedown, Rml::EventId::Mousemove, Rml::EventId::Mouseup }) {
			rml_document->AddEventListener(event, this);
		}
		rml_document->Show();
	}

	Rml::ElementDocument& Scene::document() {
		if (!rml_document) throw runtime_exception("scene has no RML document");
		return *rml_document;
	}

	sol::state& Scene::script_state() {
		return lua;
	}

	error Scene::execute_script(fs::path_view path) {
		report<vector<u08>> bytes = fs::read_all(path);
		if (!bytes) {
			return bytes.error().add_context(lf::format("loading scene script '{}'", path.text()));
		}
		const string source(reinterpret_cast<const char*>(bytes->data()), bytes->size());
		if (!execute_script(source, path.text())) {
			return error{ generic_errc::parse_error, lf::format("scene script '{}' failed", path.text()) };
		}
		return {};
	}

	void Scene::set_render_rate(frequency rate) {
		frame_rate.limit(rate);
	}

	frequency Scene::render_rate() const {
		return frequency::from_hertz(frame_rate.rate());
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
			if (element->HasAttribute("disabled")) return;
			const Rml::String source = element->GetAttribute<Rml::String>(name, "");
			if (!source.empty()) {
				execute_script(source, name);
				return;
			}
		}
	}

	bool Scene::execute_script(string_view source, string_view source_name) {
		sol::protected_function_result result = lua.safe_script(string{ source }, sol::script_pass_on_error);
		if (result.valid()) return true;
		sol::error error = result;
		log::Error("[scene] {}: {}", source_name, error.what());
		return false;
	}

	void Scene::input(span<const input_event> events) {
		if (!rml_document) return;
		for (const input_event& event : events) {
			switch (event.type) {
			case INPUT_EVENT_CONTROL:
				if (event.control.type == INPUT_CONTROL_BUTTON) {
					const int button = rml_button(static_cast<input_button>(event.control.value));
					if (event.state == input_state::Pressed) context->ProcessMouseButtonDown(button, rml_modifiers(event.modifiers));
					if (event.state == input_state::Released) context->ProcessMouseButtonUp(button, rml_modifiers(event.modifiers));
				} else if (event.control.type == INPUT_CONTROL_KEY) {
					const input_key code = static_cast<input_key>(event.control.value);
					const Rml::Input::KeyIdentifier key = rml_key(code);
					if (key != Rml::Input::KI_UNKNOWN) {
						if (event.state == input_state::Pressed) context->ProcessKeyDown(key, rml_modifiers(event.modifiers));
						if (event.state == input_state::Released) context->ProcessKeyUp(key, rml_modifiers(event.modifiers));
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
				if (event.state == input_state::Up) context->ProcessMouseLeave();
				break;
			case INPUT_EVENT_DROP:
				break;
			}
		}
		display.update_input();
	}

	void Scene::render() {
		if (!display.drawable()) return;
		const auto commands{ display.begin_frame() };
		if (!commands) return;
		const auto ui{ record(commands) };
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

		frame_rate.mark();
		return rml_backend->renderer.commands();
	}

	bool Scene::update() {
		const auto events{ display.input_events() };
		return update(events);
	}
	bool Scene::update(span<const input_event> events) {
		frame_rate.wait();
		input(events);


		return !display.should_close();
	}

	void Scene::unload_document() {
		if (rml_document) context->UnloadDocument(rml_document);
		rml_document = nullptr;
	}
} // namespace lf


