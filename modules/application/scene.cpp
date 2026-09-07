#include "leaf/application/scene.hpp"
#include <leaf/graphics/window.hpp>
#include "leaf/application/elements/window.hpp"

#include <leaf/application/rml_backend.hpp>
#include <leaf/audio/sound.hpp>
#include <leaf/core/exception.hpp>
#include <leaf/core/format.hpp>
#include <leaf/resource/database.hpp>
#include <leaf/script/extensions.hpp>
#include <leaf/core/logging.hpp>
#include <leaf/core/memory.hpp>
#include <leaf/core/messages.hpp>
#include <leaf/core/profiler.hpp>
#include <leaf/graphics/command_buffer.hpp>
#include <leaf/graphics/framebuffer.hpp>
#include <leaf/graphics/graphics.hpp>
#include <leaf/graphics/texture_view.hpp>
#include <leaf/graphics/window.hpp>
#include <leaf/platform/platform.hpp>
#include <leaf/script/localization.hpp>
#include <leaf/script/mod_loader.hpp>
#include <leaf/script/prototype_inspector_script.hpp>
#include <leaf/script/settings.hpp>
#include <leaf/script/virtual_filesystem.hpp>

#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <utility>

namespace lf {
	namespace {
		using namespace rt;

		string rml_escape(string_view value) {
			string escaped;
			escaped.reserve(value.size());
			for (char c : value) {
				switch (c) {
				case '&': escaped += "&amp;"; break;
				case '<': escaped += "&lt;"; break;
				case '>': escaped += "&gt;"; break;
				case '"': escaped += "&quot;"; break;
				case '\'': escaped += "&#39;"; break;
				default: escaped += c; break;
				}
			}
			return escaped;
		}

		string lua_value_to_string(const sol::object& value) {
			switch (value.get_type()) {
			case sol::type::lua_nil: return "nil";
			case sol::type::boolean: return value.as<bool>() ? "true" : "false";
			case sol::type::number: return lf::format("{}", value.as<double>());
			case sol::type::string: return value.as<string>();
			default: return lf::format("{}", sol::type_name(value.lua_state(), value.get_type()));
			}
		}

		string lua_escape(string_view value) {
			string escaped;
			escaped.reserve(value.size());
			for (char c : value) {
				switch (c) {
				case '\\': escaped += "\\\\"; break;
				case '"': escaped += "\\\""; break;
				case '\n': escaped += "\\n"; break;
				case '\r': escaped += "\\r"; break;
				case '\t': escaped += "\\t"; break;
				default: escaped += c; break;
				}
			}
			return escaped;
		}

		bool element_persists_for_scene(const Rml::Element& element) {
			return element.GetAttribute<Rml::String>("persist", "") == "scene";
		}

		void copy_persistent_placeholder_attributes(Rml::Element& element, const Rml::Element& placeholder) {
			element.SetAttributes(placeholder.GetAttributes());
		}

		string filter_text_for_element(const Rml::Element& element, string_view text) {
			if (element.GetAttribute<Rml::String>("input-filter", "") != "digits") {
				return string(text);
			}

			string filtered;
			filtered.reserve(text.size());
			for (char c : text) {
				if (c >= '0' && c <= '9') {
					filtered += c;
				}
			}
			return filtered;
		}

		Rml::Dictionary lua_event_parameters(sol::optional<sol::table> table) {
			Rml::Dictionary parameters;
			if (!table) {
				return parameters;
			}

			for (const auto& entry : *table) {
				if (!entry.first.is<string>()) {
					continue;
				}

				Rml::String key = entry.first.as<string>();
				const sol::object& value = entry.second;
				if (value.is<bool>()) {
					parameters[key] = Rml::Variant(value.as<bool>());
				} else if (value.is<i32>()) {
					parameters[key] = Rml::Variant(value.as<i32>());
				} else if (value.is<f32>()) {
					parameters[key] = Rml::Variant(value.as<f32>());
				} else if (value.is<f64>()) {
					parameters[key] = Rml::Variant(value.as<f64>());
				} else if (value.is<string>()) {
					parameters[key] = Rml::Variant(Rml::String(value.as<string>()));
				}
			}
			return parameters;
		}

		Rml::Dictionary mouse_parameters(Rml::Element& element, f32 x, f32 y, i32 button = 0) {
			Rml::Dictionary parameters;
			if (x < 0.0f) {
				x = element.GetAbsoluteLeft() + element.GetOffsetWidth() * 0.5f;
			}
			if (y < 0.0f) {
				y = element.GetAbsoluteTop() + element.GetOffsetHeight() * 0.5f;
			}
			parameters["mouse_x"] = Rml::Variant(x);
			parameters["mouse_y"] = Rml::Variant(y);
			parameters["button"] = Rml::Variant(button);
			return parameters;
		}

		string lua_event_table(string_view type, std::initializer_list<std::pair<string_view, string>> fields) {
			string script = "event={type=\"";
			script += lua_escape(type);
			script += "\"";
			for (const auto& [name, value] : fields) {
				script += ",";
				script += name;
				script += "=";
				script += value;
			}
			script += "}";
			script += ";";
			return script;
		}

		string lua_event_number(f32 value) {
			return lf::format("{}", value);
		}

		string lua_event_number(i32 value) {
			return lf::format("{}", value);
		}

		string lua_event_string(string_view value) {
			return lf::format("\"{}\"", lua_escape(value));
		}

		bool element_has_script_event(Rml::Element* start, string_view attribute) {
			for (Rml::Element* element = start; element; element = element->GetParentNode()) {
				if (!element->GetAttribute<Rml::String>(Rml::String(attribute), "").empty()) {
					return true;
				}
			}
			return false;
		}

		string normalized_key_name(string_view value) {
			string normalized;
			normalized.reserve(value.size());
			for (char c : value) {
				if (c == '_') {
					normalized += '-';
				} else if (c >= 'A' && c <= 'Z') {
					normalized += static_cast<char>(c - 'A' + 'a');
				} else {
					normalized += c;
				}
			}
			if (normalized.starts_with("key-")) {
				normalized.erase(0, 4);
			}
			return normalized;
		}

		string normalized_action_name(string_view value) {
			string normalized;
			normalized.reserve(value.size());
			for (char c : value) {
				if (c == '-') {
					normalized += '_';
				} else if (c >= 'a' && c <= 'z') {
					normalized += static_cast<char>(c - 'a' + 'A');
				} else {
					normalized += c;
				}
			}
			return normalized;
		}

		string input_binding_for_action(string_view action) {
			const string action_name(action);
			auto core_binding = LoadInputSetting("core", action_name);
			if (core_binding && !core_binding->empty()) {
				return *core_binding;
			}
			if (!core_binding) {
				log::Warning("{}", lf::format("[settings] {}", core_binding.error().message));
			}

			for (const ModInfo& mod : LoadedMods()) {
				if (mod.name == "core") {
					continue;
				}
				auto binding = LoadInputSetting(mod.name, action_name);
				if (!binding) {
					log::Warning("{}", lf::format("[settings] {}", binding.error().message));
					continue;
				}
				if (!binding->empty()) {
					return *binding;
				}
			}

			return {};
		}

		string key_name(rt::input_key key) {
			if (key >= rt::KEY_A && key <= rt::KEY_Z) {
				return string(1, static_cast<char>('a' + key - rt::KEY_A));
			}
			if (key >= rt::KEY_0 && key <= rt::KEY_9) {
				return string(1, static_cast<char>('0' + key - rt::KEY_0));
			}
			if (key >= rt::KEY_F1 && key <= rt::KEY_F24) {
				return lf::format("f{}", static_cast<int>(key - rt::KEY_F1 + 1));
			}

			switch (key) {
			case rt::KEY_ESCAPE: return "escape";
			case rt::KEY_TAB: return "tab";
			case rt::KEY_ENTER: return "enter";
			case rt::KEY_SPACE: return "space";
			case rt::KEY_BACKSPACE: return "backspace";
			case rt::KEY_DELETE: return "delete";
			case rt::KEY_INSERT: return "insert";
			case rt::KEY_HOME: return "home";
			case rt::KEY_END: return "end";
			case rt::KEY_PAGE_UP: return "page-up";
			case rt::KEY_PAGE_DOWN: return "page-down";
			case rt::KEY_LEFT_ARROW: return "left";
			case rt::KEY_RIGHT_ARROW: return "right";
			case rt::KEY_UP_ARROW: return "up";
			case rt::KEY_DOWN_ARROW: return "down";
			case rt::KEY_ALT_LEFT: return "alt-left";
			case rt::KEY_ALT_RIGHT: return "alt-right";
			case rt::KEY_CTRL_LEFT: return "ctrl-left";
			case rt::KEY_CTRL_RIGHT: return "ctrl-right";
			case rt::KEY_SHIFT_LEFT: return "shift-left";
			case rt::KEY_SHIFT_RIGHT: return "shift-right";
			case rt::KEY_SUPER_LEFT: return "super-left";
			case rt::KEY_SUPER_RIGHT: return "super-right";
			case rt::KEY_BACKQUOTE: return "backquote";
			case rt::KEY_BACKSLASH: return "backslash";
			case rt::KEY_BRACKET_LEFT: return "bracket-left";
			case rt::KEY_BRACKET_RIGHT: return "bracket-right";
			case rt::KEY_COMMA: return "comma";
			case rt::KEY_EQUAL: return "equal";
			case rt::KEY_HASH: return "hash";
			case rt::KEY_MINUS: return "minus";
			case rt::KEY_PERIOD: return "period";
			case rt::KEY_QUOTE: return "quote";
			case rt::KEY_SEMICOLON: return "semicolon";
			case rt::KEY_SLASH: return "slash";
			default: return {};
			}
		}

		bool keybind_matches_text(const Rml::Element& keybind, u32 character) {
			string key = keybind.GetAttribute<Rml::String>("key", "");
			if (key.empty()) {
				key = keybind.GetAttribute<Rml::String>("character", "");
			}
			if (key.empty()) {
				return false;
			}
			if (key.size() == 1) {
				return static_cast<u32>(static_cast<unsigned char>(key[0])) == character;
			}

			string normalized = normalized_key_name(key);
			return (character == '#' && normalized == "hash") ||
				   (character == ' ' && normalized == "space");
		}

		bool keybind_matches_key(const Rml::Element& keybind, rt::input_key key) {
			string requested = keybind.GetAttribute<Rml::String>("key", "");
			if (requested.empty()) {
				return false;
			}

			string actual = key_name(key);
			return !actual.empty() && normalized_key_name(requested) == actual;
		}

		bool keybind_matches_action_name(const Rml::Element& keybind, string_view actual_key, string& out_action) {
			string requested = keybind.GetAttribute<Rml::String>("action", "");
			if (requested.empty()) {
				return false;
			}

			const string action = normalized_action_name(requested);
			const string bound_key = input_binding_for_action(action);
			if (bound_key.empty()) {
				return false;
			}

			if (actual_key.empty() || normalized_key_name(bound_key) != normalized_key_name(actual_key)) {
				return false;
			}
			out_action = action;
			return true;
		}

		bool keybind_matches_action(const Rml::Element& keybind, rt::input_key key, string& out_action) {
			return keybind_matches_action_name(keybind, key_name(key), out_action);
		}

		string text_key_name(u32 character) {
			if (character == '#') {
				return "hash";
			}
			if (character == ' ') {
				return "space";
			}
			if (character > 0 && character <= 0x7f) {
				return string(1, static_cast<char>(character));
			}
			return {};
		}

		void collect_keybinds(Rml::Element& root, Rml::ElementList& out) {
			out.clear();
			for (int index = 0; index < root.GetNumChildren(); ++index) {
				Rml::Element* child = root.GetChild(index);
				if (child && child->GetTagName() == "keybind") {
					out.push_back(child);
				}
			}
		}

		void append_direct_keybinds(Rml::Element& root, Rml::ElementList& out) {
			if (!root.GetAttribute<Rml::String>("keydown", "").empty() ||
				!root.GetAttribute<Rml::String>("keyup", "").empty()) {
				out.push_back(&root);
			}
			for (int index = 0; index < root.GetNumChildren(); ++index) {
				Rml::Element* child = root.GetChild(index);
				if (child && child->GetTagName() == "keybind") {
					out.push_back(child);
				}
			}
		}

		bool input_scope_candidate(const Rml::Element& element) {
			if (element.GetAttribute<Rml::String>("input-scope", "") == "parent") {
				return false;
			}
			return element.GetTagName() == "window" ||
				   element.GetTagName() == "body" ||
				   element.HasAttribute("tabindex");
		}

		Rml::Element* containing_input_scope(Rml::ElementDocument& document, Rml::Element& focused) {
			if (input_scope_candidate(focused)) {
				return &focused;
			}
			for (Rml::Element* element = &focused; element; element = element->GetParentNode()) {
				if (element == &focused) {
					continue;
				}
				if (input_scope_candidate(*element)) {
					return element;
				}
			}
			return &document;
		}

		Rml::ElementList focused_keybinds(Rml::ElementDocument& document) {
			Rml::ElementList keybinds;
			Rml::Context* context = document.GetContext();
			Rml::Element* focused = context ? context->GetFocusElement() : nullptr;
			if (focused) {
				Rml::Element& scope = *containing_input_scope(document, *focused);
				append_direct_keybinds(scope, keybinds);
			}
			return keybinds;
		}

		string strip_inline_scripts(string_view source) {
			constexpr string_view open = "<script type=\"text/lua\"";
			constexpr string_view close = "</script>";
			string out;
			size_t offset = 0;
			while (true) {
				size_t begin = source.find(open, offset);
				if (begin == string_view::npos) {
					out += source.substr(offset);
					return out;
				}
				out += source.substr(offset, begin - offset);
				size_t end = source.find(close, begin + open.size());
				if (end == string_view::npos) {
					return out;
				}
				offset = end + close.size();
			}
		}

<<<<<<< Updated upstream
		string load_core_setting(string_view name, string_view fallback = {}) {
			auto value = LoadSetting("core", name, object(fallback));
			if (!value) {
				log::Warning("{}", lf::format("[settings] {}", value.error().message));
				return string(fallback);
			}
			return value->as<string>();
		}

		f32 load_core_setting_f32(string_view name, f32 fallback) {
			auto value = LoadSetting("core", name, object(static_cast<f64>(fallback)));
			if (!value) {
				log::Warning("{}", lf::format("[settings] {}", value.error().message));
				return fallback;
			}
			return value->as<f32>();
		}

		bool load_core_setting_bool(string_view name, bool fallback) {
			auto value = LoadSetting("core", name, object(fallback));
			if (!value) {
				log::Warning("{}", lf::format("[settings] {}", value.error().message));
				return fallback;
			}
			return value->as<bool>();
		}

		string script_attribute(string_view tag, string_view name) {
			string_view needle = name;
			size_t offset = tag.find(needle);
			while (offset != string_view::npos) {
				bool valid_prefix = offset == 0 || std::isspace(static_cast<unsigned char>(tag[offset - 1]));
				size_t cursor = offset + needle.size();
				if (valid_prefix) {
					while (cursor < tag.size() && std::isspace(static_cast<unsigned char>(tag[cursor]))) {
						++cursor;
					}
					if (cursor < tag.size() && tag[cursor] == '=') {
						++cursor;
						while (cursor < tag.size() && std::isspace(static_cast<unsigned char>(tag[cursor]))) {
							++cursor;
						}
						if (cursor < tag.size() && (tag[cursor] == '"' || tag[cursor] == '\'')) {
							char quote = tag[cursor++];
							size_t end = tag.find(quote, cursor);
							if (end != string_view::npos) {
								return string(tag.substr(cursor, end - cursor));
							}
						}
					}
				}
				offset = tag.find(needle, offset + needle.size());
			}
			return {};
		}

		Rml::Input::KeyIdentifier rml_key(rt::input_key key) {
			if (key >= rt::KEY_A && key <= rt::KEY_Z) return static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_A + key - rt::KEY_A);
			if (key >= rt::KEY_0 && key <= rt::KEY_9) return static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_0 + key - rt::KEY_0);
			if (key >= rt::KEY_NUMPAD_0 && key <= rt::KEY_NUMPAD_9) return static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_NUMPAD0 + key - rt::KEY_NUMPAD_0);
			if (key >= rt::KEY_F1 && key <= rt::KEY_F24) return static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_F1 + key - rt::KEY_F1);
			switch (key) {
			case rt::KEY_ESCAPE: return Rml::Input::KI_ESCAPE;
			case rt::KEY_TAB: return Rml::Input::KI_TAB;
			case rt::KEY_ENTER: return Rml::Input::KI_RETURN;
			case rt::KEY_SPACE: return Rml::Input::KI_SPACE;
			case rt::KEY_BACKSPACE: return Rml::Input::KI_BACK;
			case rt::KEY_DELETE: return Rml::Input::KI_DELETE;
			case rt::KEY_INSERT: return Rml::Input::KI_INSERT;
			case rt::KEY_HOME: return Rml::Input::KI_HOME;
			case rt::KEY_END: return Rml::Input::KI_END;
			case rt::KEY_PAGE_UP: return Rml::Input::KI_PRIOR;
			case rt::KEY_PAGE_DOWN: return Rml::Input::KI_NEXT;
			case rt::KEY_LEFT_ARROW: return Rml::Input::KI_LEFT;
			case rt::KEY_RIGHT_ARROW: return Rml::Input::KI_RIGHT;
			case rt::KEY_UP_ARROW: return Rml::Input::KI_UP;
			case rt::KEY_DOWN_ARROW: return Rml::Input::KI_DOWN;
			case rt::KEY_ALT_LEFT: return Rml::Input::KI_LMENU;
			case rt::KEY_ALT_RIGHT: return Rml::Input::KI_RMENU;
			case rt::KEY_CTRL_LEFT: return Rml::Input::KI_LCONTROL;
			case rt::KEY_CTRL_RIGHT: return Rml::Input::KI_RCONTROL;
			case rt::KEY_SHIFT_LEFT: return Rml::Input::KI_LSHIFT;
			case rt::KEY_SHIFT_RIGHT: return Rml::Input::KI_RSHIFT;
			case rt::KEY_SUPER_LEFT: return Rml::Input::KI_LMETA;
			case rt::KEY_SUPER_RIGHT: return Rml::Input::KI_RMETA;
			case rt::KEY_BACKQUOTE: return Rml::Input::KI_OEM_3;
			case rt::KEY_BACKSLASH: return Rml::Input::KI_OEM_5;
			case rt::KEY_BRACKET_LEFT: return Rml::Input::KI_OEM_4;
			case rt::KEY_BRACKET_RIGHT: return Rml::Input::KI_OEM_6;
			case rt::KEY_COMMA: return Rml::Input::KI_OEM_COMMA;
			case rt::KEY_EQUAL: return Rml::Input::KI_OEM_PLUS;
			case rt::KEY_MINUS: return Rml::Input::KI_OEM_MINUS;
			case rt::KEY_PERIOD: return Rml::Input::KI_OEM_PERIOD;
			case rt::KEY_QUOTE: return Rml::Input::KI_OEM_7;
			case rt::KEY_SEMICOLON: return Rml::Input::KI_OEM_1;
			case rt::KEY_SLASH: return Rml::Input::KI_OEM_2;
			default: return Rml::Input::KI_UNKNOWN;
			}
		}

		int rml_modifiers(rt::input_modifiers modifiers) {
			int rml = 0;
			if (modifiers.has(rt::INPUT_MODIFIER_CTRL)) rml |= Rml::Input::KM_CTRL;
			if (modifiers.has(rt::INPUT_MODIFIER_SHIFT)) rml |= Rml::Input::KM_SHIFT;
			if (modifiers.has(rt::INPUT_MODIFIER_ALT)) rml |= Rml::Input::KM_ALT;
			if (modifiers.has(rt::INPUT_MODIFIER_SUPER)) rml |= Rml::Input::KM_META;
			return rml;
		}

		int rml_button(rt::input_button button) {
			switch (button) {
			case rt::BUTTON_LEFT: return 0;
			case rt::BUTTON_RIGHT: return 1;
			case rt::BUTTON_MIDDLE: return 2;
			default: return static_cast<int>(button - rt::BUTTON_1);
			}
		}

		bool key_generates_text(rt::input_key key) {
			return (key >= rt::KEY_A && key <= rt::KEY_Z) ||
				   (key >= rt::KEY_0 && key <= rt::KEY_9) ||
				   (key >= rt::KEY_NUMPAD_0 && key <= rt::KEY_NUMPAD_9) ||
				   key == rt::KEY_SPACE || key == rt::KEY_BACKQUOTE || key == rt::KEY_BACKSLASH ||
				   key == rt::KEY_BRACKET_LEFT || key == rt::KEY_BRACKET_RIGHT || key == rt::KEY_COMMA ||
				   key == rt::KEY_EQUAL || key == rt::KEY_MINUS || key == rt::KEY_PERIOD ||
				   key == rt::KEY_QUOTE || key == rt::KEY_SEMICOLON || key == rt::KEY_SLASH;
		}

		bool text_key_should_skip_control_event(rt::input_key key, rt::input_modifiers modifiers) {
			if (modifiers.has(rt::INPUT_MODIFIER_CTRL) || modifiers.has(rt::INPUT_MODIFIER_ALT) || modifiers.has(rt::INPUT_MODIFIER_SUPER)) return false;
			return key_generates_text(key);
		}

		Rml::Context& create_scene_context(string_view context_name, rt::view<rt::window> display) {
			dim2<u32> size = rt::Window::Size(display);
			Rml::Context* context = Rml::CreateContext(Rml::String(context_name), { static_cast<int>(size.width), static_cast<int>(size.height) });
			if (!context) {
				throw runtime_exception(lf::format("failed to create RML context '{}'", context_name));
			}
			return *context;
		}

		string next_scene_context_name() {
			static u64 next_scene_id = 0;
			return lf::format("leaf-scene-{}", next_scene_id++);
		}

	} // namespace

	Scene::Scene()
		: Scene(rt::Window::Create()) {}

	void Scene::launch(
		string_view initial,
		string_view args,
		span<const ScriptInstaller> script_installers,
		span<const FixedUpdater> fixed_updaters
	) {
		if (!display) {
			display.reset(rt::Window::Create());
		}
		if (!context) {
			owned_context_name = next_scene_context_name();
			context = &create_scene_context(owned_context_name, this->display);
		}
		this->script_installers.assign(script_installers.begin(), script_installers.end());
		this->fixed_updaters.assign(fixed_updaters.begin(), fixed_updaters.end());
		load(initial, args);
		// Start the render and fixed-update threads BEFORE rt::Window::Show so the
		// first visible frame is already a real rendered frame. Showing the
		// window first leaves a window-of-time where the OS composites whatever
		// the framebuffer happened to contain (typically black), which was the
		// "startup is just black for a moment" symptom.
		start_threads();
		rt::Window::Show(display);
	}

	Scene::Scene(
		rt::handle<rt::window> display
	)
		: display(std::move(display)),
		  owned_context_name(next_scene_context_name()) {
		try {
			context = &create_scene_context(owned_context_name, this->display);
		} catch (...) {
			Rml::RemoveContext(owned_context_name);
			owned_context_name.clear();
			context = nullptr;
			throw;
		}
	}

	void Scene::launch(
		rt::handle<rt::window> display,
		string_view initial,
		string_view args,
		span<const ScriptInstaller> script_installers,
		span<const FixedUpdater> fixed_updaters
	) {
		this->display.reset(display);
		if (!context) {
			owned_context_name = next_scene_context_name();
			context = &create_scene_context(owned_context_name, this->display);
		}
		this->script_installers.assign(script_installers.begin(), script_installers.end());
		this->fixed_updaters.assign(fixed_updaters.begin(), fixed_updaters.end());
		load(initial, args);
		// See the no-display launch overload above for why threads start before
		// rt::Window::Show — first visible frame must be a real rendered frame.
		start_threads();
		rt::Window::Show(this->display);
	}

	rt::unique<rt::window> Scene::release_window() {
		// Stop the render + fixed-update threads BEFORE moving the window out.
		// Both threads dereference display every frame (rt::Window::ShouldClose,
		// rt::Window::BeginFrame, etc.) — if we let the move happen while they
		// were still running, their next iteration would see an empty unique
		// and crash on a null window_t pointer. This was the cause of the
		// "Access violation reading 0x0 in rt::Window::ShouldClose" at startup
		// handoff from the loading scene to run_main's gameplay scene.
		stop();
		return std::move(display);
	}

	rt::view<rt::window> Scene::window_view() const {
		return display;
	}

	Scene::ScriptEventListener::ScriptEventListener(Scene& scene) : scene(scene) {}

	void Scene::ScriptEventListener::ProcessEvent(Rml::Event& event) {
		string attribute;
		if (event == "click") {
			attribute = "click";
		} else if (event == "change") {
			attribute = "change";
		} else if (event == "input") {
			attribute = "input";
		} else if (event == "mousedown") {
			attribute = "mousedown";
		} else if (event == "mousemove") {
			attribute = "mousemove";
		} else if (event == "mouseup") {
			attribute = "mouseup";
		} else if (event == "window-close") {
			attribute = "window-close";
		} else if (event == "window-resize") {
			attribute = "window-resize";
		}
		if (attribute.empty()) {
			return;
		}

		scene.last_mouse_position = {
			event.GetParameter<f32>("mouse_x", 0.0f),
			event.GetParameter<f32>("mouse_y", 0.0f),
		};
		scene.has_mouse_position = true;

		if (attribute == "mousedown") {
			scene.update_cursor(event.GetTargetElement(), true);
		} else if (attribute == "mouseup") {
			scene.update_cursor(event.GetTargetElement(), false, true);
		} else if (attribute == "mousemove") {
			scene.update_cursor(event.GetTargetElement(), false);
		}

		for (Rml::Element* element = event.GetTargetElement(); element; element = element->GetParentNode()) {
			string script = element->GetAttribute<Rml::String>(attribute, "");
			if (script.empty()) {
				continue;
			}

			string event_script = lua_event_table(attribute, {
																 { "mouse_x", lua_event_number(event.GetParameter<f32>("mouse_x", 0.0f)) },
																 { "mouse_y", lua_event_number(event.GetParameter<f32>("mouse_y", 0.0f)) },
																 { "button", lua_event_number(event.GetParameter<int>("button", -1)) },
															 });
			event_script += script;
			scene.pending_scripts.push_back({ "event", std::move(event_script) });
			return;
		}
	}

	Scene::Scene(
		Rml::Context& context,
		rt::handle<rt::window> display
	)
		: context(&context),
		  display(std::move(display)) {}

	void Scene::install_handlers(
		span<const ScriptInstaller> new_script_installers,
		span<const FixedUpdater> new_fixed_updaters
	) {
		std::lock_guard lock(scene_mutex);
		this->script_installers.assign(new_script_installers.begin(), new_script_installers.end());
		this->fixed_updaters.assign(new_fixed_updaters.begin(), new_fixed_updaters.end());
	}

	void Scene::load(
		string_view initial,
		string_view args
	) {
		std::lock_guard lock(scene_mutex);
		// Keep scene-persistent custom elements alive across document swaps.
		// In particular, the world element owns expensive GPU resources and
		// live view state; destroying and recreating it on every loading/game
		// transition caused visible stalls and discarded its prepared state.
		vector<std::pair<string, Rml::ElementPtr>> persistent_elements;
		if (document) {
			Rml::ElementList persistent;
			document->QuerySelectorAll(persistent, "[persist='scene']");
			clear_script_events();
			for (Rml::Element* element : persistent) {
				if (!element || element->GetId().empty() || !element_persists_for_scene(*element)) {
					continue;
				}
				if (Rml::Element* parent = element->GetParentNode()) {
					persistent_elements.emplace_back(string(element->GetId()), parent->RemoveChild(element));
				}
			}
		}
		unload_document();
		scene_args = string(args);
		pending_load_request.reset();
		pending_scripts.clear();
		script_events.reset();
		title_text.clear();
		active_pressed_cursor = CursorPrototype::ID{};
		last_mouse_position = { 0.0f, 0.0f };
		has_mouse_position = false;
		script_events_bound = false;
		start_time = std::chrono::steady_clock::now();
		lua = CreateState();
		lua["scene_args"] = string(this->scene_args);

		lua.set_function("print", [](sol::variadic_args args) {
			string message;
			for (const sol::object& arg : args) {
				if (!message.empty()) {
					message += "\t";
				}
				message += lua_value_to_string(arg);
			}
			log::Info("{}", message);
		});

		RegisterWindowElement();
		string document_source = install_window_defaults(strip_inline_scripts(initial));
		document = context->LoadDocumentFromMemory(Rml::String(document_source));
		if (!document) {
			throw runtime_exception("failed to load RML document from scene source");
		}
		for (auto& [id, persistent] : persistent_elements) {
			Rml::Element* placeholder = document->GetElementById(Rml::String(id));
			if (!placeholder || !persistent || !element_persists_for_scene(*placeholder)) {
				continue;
			}
			copy_persistent_placeholder_attributes(*persistent, *placeholder);
			if (Rml::Element* parent = placeholder->GetParentNode()) {
				parent->ReplaceChild(std::move(persistent), placeholder);
			}
		}
		refresh_title();

		lua.set_function("input_value", [this](string_view id) -> string {
			Rml::Element* element = find_element(id);
			if (!element) {
				return {};
			}

			if (auto* input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(element)) {
				return string(input->GetValue());
			}
=======
	const bool scene_script_registration{ [] {
		Register<Scene, error(Scene&)>::add([](Scene& scene) -> error {
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
>>>>>>> Stashed changes
			return {};
		});

		lua.set_function("ui_value", [this](string_view id) -> string {
			Rml::Element* element = find_element(id);
			if (!element) {
				return {};
			}
			if (auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>(element)) {
				return string(control->GetValue());
			}
			return string(element->GetInnerRML());
		});

		lua.set_function("ui_blur", [this](string_view id) -> bool {
			Rml::Element* element = find_element(id);
			if (!element) {
				return false;
			}
			element->Blur();
			return true;
		});

		lua.set_function("mouse_down", [this](sol::optional<string_view> button) -> bool {
			if (!this->display) {
				return false;
			}
			string_view value = button.value_or("left");
			if (value == "left") {
				return rt::Window::MouseDown(this->display, rt::BUTTON_LEFT);
			}
			if (value == "right") {
				return rt::Window::MouseDown(this->display, rt::BUTTON_RIGHT);
			}
			if (value == "middle") {
				return rt::Window::MouseDown(this->display, rt::BUTTON_MIDDLE);
			}
			return false;
		});

		lua.set_function("mouse_x", [this]() -> f32 {
			return this->has_mouse_position ? this->last_mouse_position.x : 0.0f;
		});

		lua.set_function("mouse_y", [this]() -> f32 {
			return this->has_mouse_position ? this->last_mouse_position.y : 0.0f;
		});

		lua.set_function("ui_click", [this](string_view id) -> bool {
			Rml::Element* element = require_element(id, "ui_click");
			if (!element) {
				return false;
			}
			return run_element_script_event(element, "click", mouse_parameters(*element, -1.0f, -1.0f, 0), "ui-click");
		});

		lua.set_function("ui_click_selector", [this](string_view selector, sol::optional<i32> index) -> bool {
			Rml::Element* element = find_selector(selector, index.value_or(1));
			if (!element) {
				log::Error("{}", lf::format("[ui] missing selector '{}' at {}", selector, index.value_or(1)));
				return false;
			}
			return run_element_script_event(element, "click", mouse_parameters(*element, -1.0f, -1.0f, 0), "ui-click-selector");
		});

		lua.set_function("ui_event", [this](string_view id, string_view event_name, sol::optional<sol::table> table) -> bool {
			return run_element_script_event(id, event_name, lua_event_parameters(table), "ui-event");
		});

		lua.set_function("ui_event_selector", [this](string_view selector, string_view event_name, sol::optional<i32> index, sol::optional<sol::table> table) -> bool {
			Rml::Element* element = find_selector(selector, index.value_or(1));
			if (!element) {
				log::Error("{}", lf::format("[ui] missing selector '{}' at {}", selector, index.value_or(1)));
				return false;
			}
			return run_element_script_event(element, event_name, lua_event_parameters(table), "ui-event-selector");
		});

		lua.set_function("ui_dispatch", [this](string_view id, string_view event_name, sol::optional<sol::table> table) -> bool {
			Rml::Element* element = require_element(id, "ui_dispatch");
			if (!element) {
				return false;
			}
			return element->DispatchEvent(Rml::String(event_name), lua_event_parameters(table));
		});

		lua.set_function("ui_set_value", [this](string_view id, string_view value, sol::optional<bool> fire_change) -> bool {
			Rml::ElementFormControl* control = require_form_control(id, "ui_set_value");
			if (!control) {
				return false;
			}

			Rml::Element* element = control;
			string filtered = filter_text_for_element(*element, value);
			control->SetValue(Rml::String(filtered));
			element->SetAttribute("value", Rml::String(filtered));
			if (fire_change.value_or(true) && element_has_script_event(element, "change")) {
				run_element_script_event(id, "change", Rml::Dictionary(), "ui-set-value");
			}
			return true;
		});

		lua.set_function("ui_type", [this](string_view id, string_view text, sol::optional<bool> replace, sol::optional<bool> fire_change) -> bool {
			Rml::ElementFormControl* control = require_form_control(id, "ui_type");
			if (!control) {
				return false;
			}

			Rml::Element* element = control;
			string next_value;
			if (!replace.value_or(true)) {
				next_value = string(control->GetValue());
			}
			next_value += filter_text_for_element(*element, text);
			control->SetValue(Rml::String(next_value));
			element->SetAttribute("value", Rml::String(next_value));
			element->Focus(true);
			if (fire_change.value_or(true) && element_has_script_event(element, "change")) {
				run_element_script_event(id, "change", Rml::Dictionary(), "ui-type");
			}
			return true;
		});

		lua.set_function("ui_slider", [this](string_view id, f32 value) -> bool {
			Rml::Element* element = require_element(id, "ui_slider");
			if (!element) {
				return false;
			}

			value = std::clamp(value, 0.0f, 1.0f);
			const f32 x = element->GetAbsoluteLeft() + element->GetOffsetWidth() * value;
			const f32 y = element->GetAbsoluteTop() + element->GetOffsetHeight() * 0.5f;
			Rml::Dictionary parameters = mouse_parameters(*element, x, y, 0);
			bool began = run_element_script_event(id, "mousedown", parameters, "ui-slider");
			if (element_has_script_event(element, "mouseup")) {
				run_element_script_event(id, "mouseup", parameters, "ui-slider");
			}
			return began;
		});

		lua.set_function("ui_info", [this](string_view id) {
			sol::table info = lua.create_table();
			Rml::Element* element = find_element(id);
			info["exists"] = element != nullptr;
			if (!element) {
				return info;
			}

			info["id"] = string(element->GetId());
			info["tag"] = string(element->GetTagName());
			info["class"] = string(element->GetClassNames());
			info["left"] = element->GetAbsoluteLeft();
			info["top"] = element->GetAbsoluteTop();
			info["width"] = element->GetOffsetWidth();
			info["height"] = element->GetOffsetHeight();
			info["children"] = element->GetNumChildren();
			if (auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>(element)) {
				info["value"] = string(control->GetValue());
			}
			return info;
		});

		lua.set_function("ui_find", [this](string_view selector) {
			sol::table list = lua.create_table();
			Rml::ElementList elements;
			document->QuerySelectorAll(elements, Rml::String(selector));
			i32 index = 1;
			for (Rml::Element* element : elements) {
				sol::table item = lua.create_table();
				item["id"] = string(element->GetId());
				item["tag"] = string(element->GetTagName());
				item["class"] = string(element->GetClassNames());
				item["left"] = element->GetAbsoluteLeft();
				item["top"] = element->GetAbsoluteTop();
				item["width"] = element->GetOffsetWidth();
				item["height"] = element->GetOffsetHeight();
				list[index++] = item;
			}
			return list;
		});

		lua.set_function("ui_dump", [this](sol::optional<string_view> id, sol::optional<i32> depth) -> bool {
			Rml::Element* root = id ? find_element(*id) : document;
			if (!root) {
				log::Error("{}", lf::format("[ui] missing element '{}'", id ? string(*id) : string("<document>")));
				return false;
			}

			const i32 max_depth = std::max(0, depth.value_or(2));
			std::function<void(Rml::Element*, i32)> dump = [&](Rml::Element* element, i32 level) {
				if (!element || level > max_depth) {
					return;
				}
				string indent(static_cast<size_t>(level) * 2, ' ');
				string element_id = string(element->GetId());
				string class_names = string(element->GetClassNames());
				string id_text = element_id.empty() ? "" : lf::format("#{}", element_id);
				string class_text = class_names.empty() ? "" : lf::format(".{}", class_names);
				log::Info("{}", lf::format("[ui] {}{}{}{} {}x{} at {},{}", indent, string(element->GetTagName()), id_text, class_text, element->GetOffsetWidth(), element->GetOffsetHeight(), element->GetAbsoluteLeft(), element->GetAbsoluteTop()));
				for (int i = 0; i < element->GetNumChildren(); ++i) {
					dump(element->GetChild(i), level + 1);
				}
			};
			dump(root, 0);
			return true;
		});

		lua.set_function("set_document_title", [this](string_view title) {
			document->SetTitle(Rml::String(title));
			refresh_title();
		});

		sol::table game = lua.create_table();
		lua.globals()["game"] = game;
		lua["_G"]["game"] = game;
		game["load"] = [this](sol::object, string_view path, sol::optional<string_view> args) -> bool {
			pending_load_request = LoadRequest{ string(path), string(args.value_or("")) };
			return true;
		};
		game["exit"] = [this](sol::object) {
			if (this->display) {
				rt::Window::SetShouldClose(this->display, true);
			}
		};
		// The `scene` helper table is exposed as a top-level Lua global rather
		// than a method on `game`. RML attribute-script chunks (mousedown=...,
		// click=..., etc.) execute as freshly-compiled chunks with no upvalue
		// access to whatever locals the main scene file declared. Globals
		// survive across chunks; method results captured into `local scene` do
		// not. Building it once here and assigning to globals also avoids
		// allocating a fresh table on every Lua call.
		sol::table scene_table = lua.create_table();
		scene_table["set_rml"] = [this](sol::object, string_view id, string_view rml) {
			set_rml(id, rml);
		};
		scene_table["set_text"] = [this](sol::object, string_view id, string_view text) {
			if (Rml::Element* element = document->GetElementById(Rml::String(id))) {
				Rml::String next(rml_escape(text));
				if (element->GetInnerRML() != next) {
					element->SetInnerRML(next);
				}
			}
		};
		scene_table["set_property"] = [this](sol::object, string_view id, string_view name, string_view value) {
			if (Rml::Element* element = find_element(id)) {
				element->SetProperty(Rml::String(name), Rml::String(value));
			}
		};
		scene_table["set_attribute"] = [this](sol::object, string_view id, string_view name, sol::object value) {
			Rml::Element* element = find_element(id);
			if (element) {
				if (value.is<f32>()) {
					set_attribute(id, name, value.as<f32>());
				} else if (value.is<f64>()) {
					set_attribute(id, name, static_cast<f32>(value.as<f64>()));
				} else {
					set_attribute(id, name, lua_value_to_string(value));
				}
			}
		};
		scene_table["focus"] = [this](sol::object, string_view id) -> bool {
			Rml::Element* element = find_element(id);
			return element ? element->Focus(true) : false;
		};
		// Sets a form input's value in place without rebuilding the element.
		// Used by list-driven menus (save browser) where clicking a row must
		// fill a fixed input field without destroying/recreating it — which
		// would drop focus and visually move it.
		scene_table["set_input_value"] = [this](sol::object, string_view id, string_view value) -> bool {
			Rml::Element* element = find_element(id);
			if (!element) {
				return false;
			}
			if (auto* input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(element)) {
				input->SetValue(Rml::String(value));
				return true;
			}
			if (auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>(element)) {
				control->SetValue(Rml::String(value));
				return true;
			}
			return false;
		};
		// Toggles a CSS class on an element. Lets list selection update only
		// the highlighted row instead of re-rendering the whole list.
		scene_table["set_class"] = [this](sol::object, string_view id, string_view class_name, bool on) -> bool {
			Rml::Element* element = find_element(id);
			if (!element) {
				return false;
			}
			element->SetClass(Rml::String(class_name), on);
			return true;
		};
		scene_table["focused"] = [this](sol::object) -> string {
			if (!document) {
				return {};
			}
			Rml::Context* context = document->GetContext();
			Rml::Element* focused = context ? context->GetFocusElement() : nullptr;
			return focused ? string(focused->GetId()) : string();
		};
		scene_table["remove_element"] = [this](sol::object, string_view id) {
			Rml::Element* element = find_element(id);
			if (!element) {
				return;
			}
			if (Rml::Element* parent = element->GetParentNode()) {
				parent->RemoveChild(element);
			}
		};
		scene_table["element_exists"] = [this](sol::object, string_view tag_or_id, sol::optional<string_view> maybe_id) -> bool {
			Rml::Element* element = document->GetElementById(Rml::String(maybe_id.value_or(tag_or_id)));
			if (!element) {
				return false;
			}
			if (!maybe_id) {
				return true;
			}
			return element->GetTagName() == Rml::String(tag_or_id);
		};
		scene_table["element_left"] = [this](sol::object, string_view id) -> f32 {
			if (Rml::Element* element = find_element(id)) {
				return element->GetAbsoluteLeft();
			}
			return 0.0f;
		};
		scene_table["element_top"] = [this](sol::object, string_view id) -> f32 {
			if (Rml::Element* element = find_element(id)) {
				return element->GetAbsoluteTop();
			}
			return 0.0f;
		};
		scene_table["element_width"] = [this](sol::object, string_view id) -> f32 {
			if (Rml::Element* element = find_element(id)) {
				return element->GetOffsetWidth();
			}
			return 0.0f;
		};
		scene_table["element_height"] = [this](sol::object, string_view id) -> f32 {
			if (Rml::Element* element = find_element(id)) {
				return element->GetOffsetHeight();
			}
			return 0.0f;
		};
		scene_table["context_width"] = [this](sol::object) -> i32 {
			return document->GetContext()->GetDimensions().x;
		};
		scene_table["context_height"] = [this](sol::object) -> i32 {
			return document->GetContext()->GetDimensions().y;
		};
		lua.globals()["scene"] = scene_table;
		lua["_G"]["scene"] = scene_table;

		lua.set_function("rml_escape", [](sol::object value) {
			if (value.get_type() == sol::type::lua_nil) {
				return string();
			}
			if (value.is<string>()) {
				return rml_escape(value.as<string>());
			}
			return rml_escape(lua_value_to_string(value));
		});

		sol::table sound_group = lua.create_table();
		sound_group["ui"] = lua.create_table_with("id", "ui");
		sound_group["effects"] = lua.create_table_with("id", "effects");
		sound_group["music"] = lua.create_table_with("id", "music");
		sound_group["master"] = lua.create_table_with("id", "master");
		lua["sound_group"] = sound_group;
		InstallPrototypeInspectorScript(lua);

		lua.set_function("localize", [](string_view section, string_view key, sol::variadic_args args) {
			vector<string> parameters;
			for (const sol::object& arg : args) {
				parameters.push_back(lua_value_to_string(arg));
			}
			return Localize(section, key, span<const string>(parameters.data(), parameters.size()));
		});

		lua.set_function("loaded_language", []() {
			return string(LoadedLanguage());
		});

		lua.set_function("available_language_count", []() -> i32 {
			return static_cast<i32>(AvailableLanguages().size());
		});

		lua.set_function("available_languages", [this]() {
			sol::table list = lua.create_table();
			i32 index = 1;
			for (const LanguageInfo& language : AvailableLanguages()) {
				sol::table item = lua.create_table();
				item["id"] = language.id;
				item["name"] = language.name;
				item["native_name"] = language.native_name;
				list[index++] = item;
			}
			return list;
		});

		lua.set_function("available_language_id", [](i32 index) {
			span<const LanguageInfo> languages = AvailableLanguages();
			if (index < 1 || static_cast<size_t>(index) > languages.size()) {
				return string();
			}
			return languages[static_cast<size_t>(index - 1)].id;
		});

		lua.set_function("available_language_name", [](i32 index) {
			span<const LanguageInfo> languages = AvailableLanguages();
			if (index < 1 || static_cast<size_t>(index) > languages.size()) {
				return string();
			}
			return languages[static_cast<size_t>(index - 1)].name;
		});

		lua.set_function("available_language_native_name", [](i32 index) {
			span<const LanguageInfo> languages = AvailableLanguages();
			if (index < 1 || static_cast<size_t>(index) > languages.size()) {
				return string();
			}
			return languages[static_cast<size_t>(index - 1)].native_name;
		});

		lua.set_function("loaded_language_native_name", []() {
			return string(AvailableLanguageNativeName(LoadedLanguage()));
		});

		lua.set_function("set_language", [](string_view language) -> bool {
			if (error err = SetLanguage(language)) {
				log::Error("{}", lf::format("[settings] {}", err.message));
				return false;
			}
			return true;
		});

		lua.set_function("settings_master_volume", []() -> f32 {
			return load_core_setting_f32("sound.master", 1.0f);
		});

		lua.set_function("settings_music_volume", []() -> f32 {
			return load_core_setting_f32("sound.music", 0.8f);
		});

		lua.set_function("settings_effects_volume", []() -> f32 {
			return load_core_setting_f32("sound.effects", 1.0f);
		});

		lua.set_function("settings_fullscreen", []() -> bool {
			return load_core_setting_bool("graphics.fullscreen", false);
		});

		lua.set_function("settings_vsync", []() -> bool {
			return load_core_setting_bool("graphics.vsync", false);
		});

		lua.set_function("settings", [](sol::this_state state) -> sol::table {
			sol::state_view lua(state);

			sol::table sound = lua.create_table();
			sound["master"] = load_core_setting_f32("sound.master", 1.0f);
			sound["music"] = load_core_setting_f32("sound.music", 0.8f);
			sound["effects"] = load_core_setting_f32("sound.effects", 1.0f);

			sol::table graphics = lua.create_table();
			graphics["fullscreen"] = load_core_setting_bool("graphics.fullscreen", false);
			graphics["vsync"] = load_core_setting_bool("graphics.vsync", false);
			graphics["max_fps"] = load_core_setting_f32("graphics.max-fps", 60.0f);

			sol::table result = lua.create_table();
			result["language"] = load_core_setting("language", "en-US");
			result["sound"] = sound;
			result["graphics"] = graphics;
			return result;
		});

		lua.set_function("graphics_backend", []() -> string_view {
			return "vulkan";
		});

		lua.set_function("include", [this](string_view path) {
			string include_path(path);
			report<string> script = ReadVirtualTextFile(include_path);
			if (!script) {
				throw runtime_exception(lf::format("include '{}': {}", include_path, script.error().message));
			}
			if (!execute_script(*script, include_path)) {
				throw runtime_exception(lf::format("include '{}' failed", include_path));
			}
		});

		// Current wall-clock time as unix seconds. The Lua `os` library is not
		// opened (it exposes os.execute and friends), so menus that need to
		// show relative timestamps ("7 months ago") get the reference time
		// from here instead of os.time().
		lua.set_function("now_unix", []() -> double {
			return static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
										   std::chrono::system_clock::now().time_since_epoch()
			)
										   .count());
		});

		lua.set_function("profile", [](bool enabled) {
			SetProfilerEnabled(enabled);
		});

		lua.set_function("profile_reset", []() {
			ResetProfiler();
		});

		lua.set_function("profile_log", [](sol::optional<string_view> label) {
			LogProfileSnapshot(label.value_or("profile"));
		});

		lua.set_function("fullscreen", [this]() -> bool {
			return this->display ? rt::Window::Fullscreen(this->display) : false;
		});

		lua.set_function("set_fullscreen", [this](bool fullscreen) {
			if (this->display) {
				rt::Window::RequestFullscreen(this->display, fullscreen);
			}
		});

		lua.set_function("save_settings", [this](f32 master, f32 music, f32 effects, bool fullscreen, bool vsync, sol::object max_fps_value) -> bool {
			f32 max_fps = load_core_setting_f32("graphics.max-fps", 60.0f);
			if (max_fps_value.is<f32>()) {
				max_fps = max_fps_value.as<f32>();
			} else if (max_fps_value.is<f64>()) {
				max_fps = static_cast<f32>(max_fps_value.as<f64>());
			} else if (max_fps_value.is<i32>()) {
				max_fps = static_cast<f32>(max_fps_value.as<i32>());
			}
			bool fullscreen_changed = load_core_setting_bool("graphics.fullscreen", false) != fullscreen;
			if (error err = SaveSetting("core", "sound.master", object(static_cast<f64>(master)))) {
				log::Error("{}", lf::format("[settings] {}", err.message));
				return false;
			}
			if (error err = SaveSetting("core", "sound.music", object(static_cast<f64>(music)))) {
				log::Error("{}", lf::format("[settings] {}", err.message));
				return false;
			}
			if (error err = SaveSetting("core", "sound.effects", object(static_cast<f64>(effects)))) {
				log::Error("{}", lf::format("[settings] {}", err.message));
				return false;
			}
			if (error err = SaveSetting("core", "graphics.fullscreen", object(fullscreen))) {
				log::Error("{}", lf::format("[settings] {}", err.message));
				return false;
			}
			if (error err = SaveSetting("core", "graphics.vsync", object(vsync))) {
				log::Error("{}", lf::format("[settings] {}", err.message));
				return false;
			}
			if (error err = SaveSetting("core", "graphics.max-fps", object(static_cast<f64>(max_fps)))) {
				log::Error("{}", lf::format("[settings] {}", err.message));
				return false;
			}
			if (this->display) {
				rt::Window::SetVsync(this->display, vsync);
			}
			if (fullscreen_changed && this->display) {
				rt::Window::RequestFullscreen(this->display, fullscreen);
			}
			return true;
		});

		PrepareState(lua, this->script_installers);
		install_ui_automation_helpers();

		script_events = std::make_unique<ScriptEventListener>(*this);
		run_inline_scripts(initial);
		bind_script_events();
		if (const char* automation_path = std::getenv("LEAF_UI_AUTOMATION")) {
			std::ifstream file(automation_path, std::ios::binary);
			if (file) {
				std::ostringstream buffer;
				buffer << file.rdbuf();
				run_script(buffer.str(), automation_path);
			} else {
				log::Error("{}", lf::format("[scene] failed to open UI automation script '{}'", automation_path));
			}
		}
<<<<<<< Updated upstream
		document->Show();
=======
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
		if (const auto error{ InstallScriptInterfaces(lua) }) {
			throw runtime_exception(error.message);
		}
		const dim2<u32> size = this->display.size();
		context = Rml::CreateContext(lf::format("scene-{}", static_cast<const void*>(this)), { static_cast<i32>(size.width), static_cast<i32>(size.height) });
		if (!context) throw runtime_exception("failed to create RML scene context");
		scope_exit rollback{ [this] { Rml::RemoveContext(context->GetName()); } };
		if (auto err = Register<Scene, error(Scene&)>::install(*this); err) {
			throw runtime_exception(err.message);
		}
		rollback.release();
>>>>>>> Stashed changes
	}

	Scene::~Scene() {
		// Stop threads BEFORE tearing down anything they could be touching.
		// jthread destruction would normally join automatically, but we need
		// the join to happen here (while document/context are still valid)
		// rather than after member destruction order has already started
		// invalidating things underneath the threads.
		stop();
		unload_document();
		if (!owned_context_name.empty()) {
			Rml::RemoveContext(owned_context_name);
			owned_context_name.clear();
			context = nullptr;
		}
	}

	void Scene::set_render_rate(double hz) {
		configured_render_hz.store(hz);
	}

	void Scene::set_fixed_update_rate(double hz) {
		configured_fixed_update_hz.store(hz);
	}

	void Scene::set_pre_fixed_update(std::function<void()> callback) {
		// Locked because the fixed-update thread reads pre_fixed_update each
		// iteration; without the lock, replacing it on the main thread races
		// with std::function's copy/destruct on the worker.
		std::lock_guard lock(scene_mutex);
		pre_fixed_update = std::move(callback);
	}

	void Scene::set_fixed_update_error_handler(std::function<void(const std::exception&)> handler) {
		std::lock_guard lock(scene_mutex);
		fixed_update_error_handler = std::move(handler);
	}

	double Scene::render_rate_hz() const {
		return render_rate.rate();
	}

	void Scene::stop() {
		// Idempotent: jthread::request_stop / join on a default-constructed
		// or already-joined thread are no-ops, but the threads_started gate
		// keeps the bool flag consistent for anyone polling it.
		if (render_thread.joinable()) {
			render_thread.request_stop();
		}
		if (fixed_update_thread.joinable()) {
			fixed_update_thread.request_stop();
		}
		if (render_thread.joinable()) {
			render_thread.join();
		}
		if (fixed_update_thread.joinable()) {
			fixed_update_thread.join();
		}
		threads_started = false;
	}

	void Scene::start_threads() {
		if (threads_started.exchange(true)) {
			return;
		}
		render_thread = std::jthread([this](std::stop_token stop) { render_loop(stop); });
		if (configured_fixed_update_hz.load() > 0.0) {
			fixed_update_thread = std::jthread([this](std::stop_token stop) { fixed_update_loop(stop); });
		}
	}

	void Scene::render_loop(std::stop_token stop) {
		double applied_hz = -1.0;
		while (!stop.stop_requested()) {
			// Re-read the configured rate every iteration so set_render_rate
			// from any thread takes effect on the very next frame.
			const double desired_hz = configured_render_hz.load();
			if (desired_hz != applied_hz) {
				render_rate.limit(desired_hz);
				applied_hz = desired_hz;
			}
			render_rate.wait();
			if (stop.stop_requested()) {
				return;
			}
			if (!render_frame()) {
				return;
			}
			render_rate.mark();
		}
	}

	void Scene::fixed_update_loop(std::stop_token stop) {
		using clock = std::chrono::steady_clock;
		double applied_hz = -1.0;
		auto interval = clock::duration::zero();
		auto next_tick = clock::now();
		while (!stop.stop_requested()) {
			const double desired_hz = configured_fixed_update_hz.load();
			if (desired_hz <= 0.0) {
				// Hot-disabled: yield and recheck. The loop stays alive so
				// re-enabling later just works.
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				next_tick = clock::now();
				continue;
			}
			if (desired_hz != applied_hz) {
				interval = std::chrono::duration_cast<clock::duration>(
					std::chrono::duration<double>(1.0 / desired_hz)
				);
				applied_hz = desired_hz;
				next_tick = clock::now();
			}
			next_tick += interval;
			std::this_thread::sleep_until(next_tick);
			if (stop.stop_requested()) {
				return;
			}
			try {
				std::unique_lock lock(scene_mutex);
				if (pre_fixed_update) {
					pre_fixed_update();
				}
				fixed_update();
			} catch (const std::exception& e) {
				if (fixed_update_error_handler) {
					fixed_update_error_handler(e);
				} else {
					log::Error("[scene] fixed update threw: {}", e.what());
				}
				return;
			}
		}
	}

	void Scene::unload_document() {
		SetCursorPrototype(display, CursorPrototype::ID{});
		if (document) {
			Rml::Context* context = document->GetContext();
			ReleaseWindowDocumentEvents(*document);
			clear_script_events();
			context->UnloadDocument(document);
			document = nullptr;
		}
		pending_scripts.clear();
		lua = CreateState();
	}

	void Scene::clear_script_events() {
		if (!script_events || !script_events_bound || !document) {
			return;
		}
		document->RemoveEventListener("click", script_events.get());
		document->RemoveEventListener("change", script_events.get());
		document->RemoveEventListener("input", script_events.get());
		document->RemoveEventListener("mousedown", script_events.get());
		document->RemoveEventListener("mousemove", script_events.get());
		document->RemoveEventListener("mouseup", script_events.get());
		document->RemoveEventListener("window-close", script_events.get());
		document->RemoveEventListener("window-resize", script_events.get());
		script_events_bound = false;
	}

	void Scene::bind_script_events() {
		if (!script_events || script_events_bound || !document) {
			return;
		}
		document->AddEventListener("click", script_events.get());
		document->AddEventListener("change", script_events.get());
		document->AddEventListener("input", script_events.get());
		document->AddEventListener("mousedown", script_events.get());
		document->AddEventListener("mousemove", script_events.get());
		document->AddEventListener("mouseup", script_events.get());
		document->AddEventListener("window-close", script_events.get());
		document->AddEventListener("window-resize", script_events.get());
		script_events_bound = true;
	}

	void Scene::run_inline_scripts(string_view source) {
		constexpr string_view open = "<script type=\"text/lua\"";
		constexpr string_view close = "</script>";
		size_t offset = 0;
		while (true) {
			size_t begin = source.find(open, offset);
			if (begin == string_view::npos) {
				return;
			}
			size_t tag_end = source.find('>', begin + open.size());
			if (tag_end == string_view::npos) {
				return;
			}
			string_view tag = source.substr(begin, tag_end - begin + 1);
			size_t body_begin = tag_end + 1;
			size_t end = source.find(close, body_begin);
			if (end == string_view::npos) {
				return;
			}

			string src = script_attribute(tag, "src");
			if (!src.empty()) {
				report<string> script = ReadVirtualTextFile(src);
				if (!script) {
					log::Error("{}", lf::format("[scene] {}: {}", src, script.error().message));
				} else {
					run_script(*script, src);
				}
			} else {
				run_script(source.substr(body_begin, end - body_begin), "inline script");
			}
			offset = end + close.size();
		}
	}

	void Scene::run_script(string_view script, string_view source_name) {
		execute_script(script, source_name);
	}

	bool Scene::execute_script(string_view script, string_view source_name) {
		sol::protected_function_result result = lua.safe_script(string(script), sol::script_pass_on_error);
		if (!result.valid()) {
			sol::error error = result;
			log::Error("{}", lf::format("[scene] {}: {}", source_name, error.what()));
			return false;
		}
		return true;
	}

	bool Scene::run_pending_scripts() {
		std::vector<PendingScript> scripts;
		scripts.swap(pending_scripts);

		bool ran_scripts = false;
		for (const PendingScript& pending : scripts) {
			ran_scripts = true;
			execute_script(pending.script, pending.source_name);
		}

		if (ran_scripts) {
			update_cursor_at_last_mouse();
		}
		return ran_scripts;
	}

	void Scene::install_ui_automation_helpers() {
		constexpr string_view helpers = R"lua(
__leaf_ui_automation = {
	queue = {},
	next_id = 1,
	last_error = "",
}

local function __leaf_compile_automation(value, label)
	if type(value) == "function" then
		return value
	end
	if type(value) == "string" then
		local fn, err = load(value, label or "ui-automation", "t", _G)
		if fn == nil then
			__leaf_ui_automation.last_error = tostring(err)
			print("[ui-wait] " .. tostring(err))
			return nil
		end
		return fn
	end
	__leaf_ui_automation.last_error = "expected function or string"
	print("[ui-wait] expected function or string")
	return nil
end

local function __leaf_schedule_wait(condition, action, timeout_seconds, label)
	local condition_fn = __leaf_compile_automation(condition, label or "ui-wait-condition")
	local action_fn = __leaf_compile_automation(action, label or "ui-wait-action")
	if condition_fn == nil or action_fn == nil then
		return 0
	end

	local id = __leaf_ui_automation.next_id
	__leaf_ui_automation.next_id = id + 1
	__leaf_ui_automation.queue[#__leaf_ui_automation.queue + 1] = {
		id = id,
		condition = condition_fn,
		action = action_fn,
		start = __leaf_ui_automation.now or 0,
		timeout = timeout_seconds,
		label = label or "ui-wait",
	}
	return id
end

function ui_wait_until(condition, action, timeout_seconds)
	return __leaf_schedule_wait(condition, action, timeout_seconds, "ui-wait-until")
end

function ui_wait_element(id, action, timeout_seconds)
	return ui_wait_until(function()
		return scene:element_exists(id)
	end, action, timeout_seconds)
end

function ui_wait_not_element(id, action, timeout_seconds)
	return ui_wait_until(function()
		return not scene:element_exists(id)
	end, action, timeout_seconds)
end

function ui_wait_selector(selector, action, timeout_seconds)
	return ui_wait_until(function()
		return #ui_find(selector) > 0
	end, action, timeout_seconds)
end

function ui_wait_value(id, value, action, timeout_seconds)
	return ui_wait_until(function()
		return ui_value(id) == tostring(value)
	end, action, timeout_seconds)
end

function ui_wait_frames(frames, action)
	frames = math.max(0, math.floor(tonumber(frames) or 0))
	local remaining = frames
	return ui_wait_until(function()
		remaining = remaining - 1
		return remaining <= 0
	end, action)
end

function ui_wait_seconds(seconds, action)
	local duration = math.max(0, tonumber(seconds) or 0)
	local start_time = __leaf_ui_automation.now or 0
	return ui_wait_until(function()
		return ((__leaf_ui_automation.now or 0) - start_time) >= duration
	end, action)
end

function ui_automation_pending()
	return #__leaf_ui_automation.queue
end

function ui_automation_clear()
	__leaf_ui_automation.queue = {}
end

function ui_automation_last_error()
	return __leaf_ui_automation.last_error
end

function __leaf_ui_automation_update(time_seconds)
	__leaf_ui_automation.now = time_seconds
	local next_queue = {}
	for _, wait in ipairs(__leaf_ui_automation.queue) do
		local keep = true
		local ok, ready = pcall(wait.condition)
		if not ok then
			__leaf_ui_automation.last_error = tostring(ready)
			print("[ui-wait] condition failed: " .. tostring(ready))
			keep = false
		elseif ready then
			local action_ok, action_error = pcall(wait.action)
			if not action_ok then
				__leaf_ui_automation.last_error = tostring(action_error)
				print("[ui-wait] action failed: " .. tostring(action_error))
			end
			keep = false
		elseif wait.timeout ~= nil and (time_seconds - wait.start) >= wait.timeout then
			__leaf_ui_automation.last_error = "timeout: " .. tostring(wait.label)
			print("[ui-wait] timeout: " .. tostring(wait.label))
			keep = false
		end
		if keep then
			next_queue[#next_queue + 1] = wait
		end
	end
	__leaf_ui_automation.queue = next_queue
end
)lua";
		run_script(helpers, "ui-automation-helpers");
	}

	void Scene::update_ui_automation(f64 elapsed) {
		sol::object update_object = lua["__leaf_ui_automation_update"];
		if (update_object.get_type() != sol::type::function) {
			return;
		}

		sol::protected_function update = update_object;
		sol::protected_function_result result = update(elapsed);
		if (!result.valid()) {
			sol::error error = result;
			log::Error("{}", lf::format("[scene] ui-automation: {}", error.what()));
		}
	}

	void Scene::update_cursor(Rml::Element* start, bool pressed, bool released) {
		if (released) {
			active_pressed_cursor = CursorPrototype::ID{};
		} else if (active_pressed_cursor) {
			SetCursorPrototype(display, active_pressed_cursor);
			return;
		}

		for (Rml::Element* element = start; element; element = element->GetParentNode()) {
			string cursor;
			if (pressed) {
				cursor = element->GetAttribute<Rml::String>("pressed-cursor", "");
				if (cursor.empty()) {
					cursor = element->GetAttribute<Rml::String>("click-cursor", "");
				}
			}
			if (cursor.empty()) {
				cursor = element->GetAttribute<Rml::String>("hover-cursor", "");
			}
			if (cursor.empty()) {
				cursor = element->GetAttribute<Rml::String>("cursor", "");
			}
			if (!cursor.empty()) {
				const auto id = Database<CursorPrototype>::find(cursor);
				if (pressed) {
					active_pressed_cursor = id;
				}
				SetCursorPrototype(display, id);
				return;
			}
		}
		if (pressed) {
			active_pressed_cursor = CursorPrototype::ID{};
		}
		SetCursorPrototype(display, CursorPrototype::ID{});
	}

	void Scene::update_cursor_at_last_mouse() {
		if (!document || !has_mouse_position) {
			return;
		}
		Rml::Context* context = document->GetContext();
		if (!context) {
			return;
		}
		update_cursor(context->GetElementAtPoint(last_mouse_position), false);
	}

	Rml::Element* Scene::find_element(string_view id) const {
		return document ? document->GetElementById(Rml::String(id)) : nullptr;
	}

	Rml::Element* Scene::find_selector(string_view selector, i32 index) const {
		if (!document || index < 1) {
			return nullptr;
		}

		Rml::ElementList elements;
		document->QuerySelectorAll(elements, Rml::String(selector));
		const size_t offset = static_cast<size_t>(index - 1);
		if (offset >= elements.size()) {
			return nullptr;
		}
		return elements[offset];
	}

	Rml::Element* Scene::require_element(string_view id, string_view api_name) const {
		Rml::Element* element = find_element(id);
		if (!element) {
			log::Error("{}", lf::format("[ui] {} missing element '{}'", api_name, id));
		}
		return element;
	}

	Rml::ElementFormControl* Scene::require_form_control(string_view id, string_view api_name) const {
		Rml::Element* element = require_element(id, api_name);
		if (!element) {
			return nullptr;
		}

		auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>(element);
		if (!control) {
			log::Error("{}", lf::format("[ui] {} element '{}' is not a form control", api_name, id));
		}
		return control;
	}

	bool Scene::run_element_script_event(Rml::Element* start, string_view attribute, const Rml::Dictionary& parameters, string_view source_name) {
		if (!start) {
			log::Error("[ui] missing element");
			return false;
		}

		for (Rml::Element* element = start; element; element = element->GetParentNode()) {
			string script = element->GetAttribute<Rml::String>(Rml::String(attribute), "");
			if (script.empty()) {
				continue;
			}

			auto mouse_x = parameters.find("mouse_x");
			auto mouse_y = parameters.find("mouse_y");
			auto button = parameters.find("button");
			string event_script = lua_event_table(attribute, {
																 { "mouse_x", lua_event_number(mouse_x != parameters.end() ? mouse_x->second.Get<f32>(0.0f) : 0.0f) },
																 { "mouse_y", lua_event_number(mouse_y != parameters.end() ? mouse_y->second.Get<f32>(0.0f) : 0.0f) },
																 { "button", lua_event_number(button != parameters.end() ? button->second.Get<i32>(-1) : -1) },
															 });
			event_script += script;
			pending_scripts.push_back({ string(source_name), std::move(event_script) });
			return true;
		}

		log::Error("{}", lf::format("[ui] element has no '{}' script", attribute));
		return false;
	}

	bool Scene::run_element_script_event(string_view id, string_view attribute, const Rml::Dictionary& parameters, string_view source_name) {
		Rml::Element* start = find_element(id);
		if (!start) {
			log::Error("{}", lf::format("[ui] missing element '{}'", id));
			return false;
		}
		return run_element_script_event(start, attribute, parameters, source_name);
	}

	bool Scene::update() {
		// The render thread is rate-limited, but this UI/input loop used to spin
		// as fast as it could, repeatedly running Lua and contending for the DOM
		// mutex. Cap it to the configured presentation rate as well.
		const double desired_hz = configured_render_hz.load();
		if (desired_hz != applied_update_hz) {
			update_rate.limit(desired_hz);
			applied_update_hz = desired_hz;
		}
		update_rate.wait();
		// Lock against the render / fixed-update threads. process_input mutates
		// the RmlUi context (ProcessMouseButton* etc.) and the Lua `update`
		// callback typically mutates the DOM via set_rml / set_property —
		// none of which is safe while render_thread is inside context->Update
		// or context->Render. Same mutex everyone else uses.
		std::unique_lock lock(scene_mutex);
		process_input();
		if (run_pending_scripts() && pending_load_request) {
			return !rt::Window::ShouldClose(display);
		}

		auto now = std::chrono::steady_clock::now();
		f64 elapsed = std::chrono::duration<f64>(now - start_time).count();
		update_ui_automation(elapsed);

		sol::object update_object = lua["update"];
		if (update_object.get_type() != sol::type::function) {
			return !rt::Window::ShouldClose(display);
		}

		sol::protected_function update = update_object;
		sol::protected_function_result result = update(elapsed);
		if (!result.valid()) {
			sol::error error = result;
			log::Error("{}", lf::format("[scene] {}", error.what()));
		}
		return !rt::Window::ShouldClose(display);
	}

	void Scene::input(const rt::input_event& event) {
		if (!document) {
			return;
		}

		if (event.type == INPUT_EVENT_SCROLL) {
			last_mouse_position = { event.position.x, event.position.y };
			has_mouse_position = true;
			Rml::Context* context = document->GetContext();
			if (!context) {
				return;
			}

			Rml::Element* start = context->GetElementAtPoint({ event.position.x, event.position.y });
			update_cursor(start, false);
			for (Rml::Element* element = start; element; element = element->GetParentNode()) {
				string script = element->GetAttribute<Rml::String>("mousescroll", "");
				if (script.empty()) {
					continue;
				}

				string event_script = lua_event_table("mousescroll", {
																		 std::pair<lf::string_view, lf::string>{ lf::string_view("mouse_x"), lua_event_number(event.position.x) },
																		 std::pair<lf::string_view, lf::string>{ lf::string_view("mouse_y"), lua_event_number(event.position.y) },
																		 std::pair<lf::string_view, lf::string>{ lf::string_view("scroll_x"), lua_event_number(event.delta.x) },
																		 std::pair<lf::string_view, lf::string>{ lf::string_view("scroll_y"), lua_event_number(event.delta.y) },
																	 });
				event_script += script;
				pending_scripts.push_back({ "input-scroll", std::move(event_script) });
				return;
			}
			return;
		}

		if (event.type == INPUT_EVENT_POINTER_MOVE || event.type == INPUT_EVENT_CONTROL) {
			last_mouse_position = { event.position.x, event.position.y };
			has_mouse_position = true;
		}

		string attribute;
		string event_key;
		string event_action;
		u32 event_character = 0;

		if (event.type == INPUT_EVENT_TEXT) {
			attribute = "keydown";
			event_character = event.character;
		} else if (event.type == INPUT_EVENT_CONTROL && event.control.type == INPUT_CONTROL_KEY) {
			if (event.state == input_state::Pressed) {
				attribute = "keydown";
			} else if (event.state == input_state::Released) {
				attribute = "keyup";
			}
			event_key = key_name(static_cast<rt::input_key>(event.control.value));
		}

		if (attribute.empty()) {
			return;
		}

		Rml::Context* context = document->GetContext();
		Rml::Element* focused = context ? context->GetFocusElement() : nullptr;
		if (focused && !event_key.empty()) {
			string focused_script = focused->GetAttribute<Rml::String>(Rml::String(lf::format("{}-{}", attribute, event_key)), "");
			if (!focused_script.empty()) {
				string event_script = lua_event_table(attribute, {
																	 { "key", lua_event_string(event_key) },
																	 { "character", lua_event_number(static_cast<i32>(event_character)) },
																	 { "modifiers", lua_event_number(static_cast<i32>(event.modifiers.value)) },
																 });
				event_script += focused_script;
				pending_scripts.push_back({ "input", std::move(event_script) });
			}
		}

		Rml::ElementList keybinds = focused_keybinds(*document);
		for (Rml::Element* keybind : keybinds) {
			if (!keybind) {
				continue;
			}

			bool matched = false;
			event_action.clear();
			if (event.type == INPUT_EVENT_TEXT) {
				matched = keybind_matches_action_name(*keybind, text_key_name(event.character), event_action);
				if (!matched) {
					matched = keybind_matches_text(*keybind, event.character);
				}
				if (matched && event_key.empty()) {
					event_key = text_key_name(event.character);
				}
			} else {
				matched = keybind_matches_action(*keybind, static_cast<rt::input_key>(event.control.value), event_action);
				if (!matched) {
					matched = keybind_matches_key(*keybind, static_cast<rt::input_key>(event.control.value));
				}
			}
			if (!matched) {
				continue;
			}

			string script = keybind->GetAttribute<Rml::String>(attribute, "");
			if (script.empty()) {
				continue;
			}

			string event_script = lua_event_table(attribute, {
																 { "key", lua_event_string(event_key) },
																 { "character", lua_event_number(static_cast<i32>(event_character)) },
																 { "modifiers", lua_event_number(static_cast<i32>(event.modifiers.value)) },
															 });
			event_script += script;
			pending_scripts.push_back({ "input", std::move(event_script) });
		}
	}

	void Scene::process_input() {
		if (!document) {
			return;
		}
		Rml::Context* context = document->GetContext();
		if (!context) {
			return;
		}

		std::vector<rt::input_event> events = rt::Window::InputEvents(display);
		for (const rt::input_event& event : events) {
			switch (event.type) {
			case INPUT_EVENT_CONTROL:
				if (event.control.type == INPUT_CONTROL_BUTTON) {
					rt::input_button button = static_cast<rt::input_button>(event.control.value);
					if (event.state == input_state::Pressed) context->ProcessMouseButtonDown(rml_button(button), rml_modifiers(event.modifiers));
					if (event.state == input_state::Released) context->ProcessMouseButtonUp(rml_button(button), rml_modifiers(event.modifiers));
				} else if (event.control.type == INPUT_CONTROL_KEY) {
					rt::input_key key_code = static_cast<rt::input_key>(event.control.value);
					Rml::Input::KeyIdentifier key = rml_key(key_code);
					if (key != Rml::Input::KI_UNKNOWN) {
						if (event.state == input_state::Pressed) {
							if (key_code == rt::KEY_V && (event.modifiers.has(rt::INPUT_MODIFIER_CTRL) || event.modifiers.has(rt::INPUT_MODIFIER_SUPER))) {
								string clipboard_text = platform_clipboard_text();
								string filtered_text = filter_text_input(clipboard_text);
								if (filtered_text != clipboard_text) {
									for (char character : filtered_text)
										context->ProcessTextInput(static_cast<Rml::Character>(character));
									break;
								}
							}
							if (!(has_text_input_focus() && text_key_should_skip_control_event(key_code, event.modifiers))) {
								context->ProcessKeyDown(key, rml_modifiers(event.modifiers));
							}
							if ((key_code == rt::KEY_ENTER || key_code == rt::KEY_NUMPAD_ENTER) && accepts_text_input('\n')) {
								context->ProcessTextInput('\n');
							}
						} else if (event.state == input_state::Released) {
							if (!(has_text_input_focus() && text_key_should_skip_control_event(key_code, event.modifiers))) {
								context->ProcessKeyUp(key, rml_modifiers(event.modifiers));
							}
						}
					}
				}
				break;
			case INPUT_EVENT_POINTER_MOVE: {
				int mouse_x = static_cast<int>(event.position.x);
				int mouse_y = static_cast<int>(event.position.y);
				int modifiers = rml_modifiers(event.modifiers);
				context->ProcessMouseMove(mouse_x, mouse_y, modifiers);
			} break;
			case INPUT_EVENT_POINTER_ENTER:
				if (event.state == input_state::Up) {
					context->ProcessMouseLeave();
					ReleaseWindowDocumentEvents(*document);
					update_cursor(nullptr, false, true);
				}
				break;
			case INPUT_EVENT_SCROLL:
				context->ProcessMouseWheel(Rml::Vector2f{ -event.delta.x, -event.delta.y }, rml_modifiers(event.modifiers));
				break;
			case INPUT_EVENT_TEXT:
				if (accepts_text_input(event.character)) context->ProcessTextInput(static_cast<Rml::Character>(event.character));
				break;
			case INPUT_EVENT_FOCUS:
				if (event.state == input_state::Up) {
					context->ProcessMouseLeave();
					ReleaseWindowDocumentEvents(*document);
					update_cursor(nullptr, false, true);
				}
				break;
			case INPUT_EVENT_DROP:
				break;
			}
			input(event);
		}
		rt::Window::UpdateInput(display);
	}

	void Scene::resize_context() {
		if (!document) {
			return;
		}
		Rml::Context* context = document->GetContext();
		if (!context) {
			return;
		}
		dim2<u32> window_size = rt::Window::Size(display);
		Rml::Vector2i context_size = context->GetDimensions();
		if (context_size.x != static_cast<int>(window_size.width) || context_size.y != static_cast<int>(window_size.height)) {
			context->SetDimensions({ static_cast<int>(window_size.width), static_cast<int>(window_size.height) });
		}
	}

	void Scene::render(rt::view<rt::command_buffer> cmd) {
		if (!document) {
			return;
		}
		Rml::Context* context = document->GetContext();
		if (!context) {
			return;
		}
		resize_context();
		context->Update();
		dim2<u32> render_size = rt::Window::Size(display);
		rt::view<rt::framebuffer> framebuffer = rt::Window::CurrentFramebuffer(window_view());
		if (framebuffer) {
			rt::view<rt::texture_view> color = rt::Framebuffer::ColorView(framebuffer, 0);
			rt_extent_3d extent = color ? rt::TextureView::Extent(color) : rt_extent_3d{};
			if (extent.width && extent.height) {
				render_size = { extent.width, extent.height };
			}
		}
		rml_renderer().begin(cmd, render_size);
		context->Render();
		rml_renderer().end();
	}

	bool Scene::render_frame() {
		if (!rt::Window::Drawable(window_view())) {
			std::this_thread::yield();
			return !rt::Window::ShouldClose(window_view());
		}

		rt::handle<rt::queue> graphics_queue = rt::Queue::Query(rt::QueueCapability::Graphics);
		rt::view<rt::command_buffer> cmd = rt::Window::BeginFrame(window_view(), graphics_queue);
		if (!cmd) {
			return !rt::Window::ShouldClose(window_view());
		}
		{
			// Only RmlUi/DOM work needs the scene lock. Swapchain acquire and
			// present can block while a window is occluded or alt-tabbed and must
			// never prevent input or Lua updates from running.
			std::unique_lock lock(scene_mutex);
			render(cmd);
		}
		rt::Window::EndFrame(window_view());
		return !rt::Window::ShouldClose(window_view());
	}

	void Scene::fixed_update() {
		if (!document) {
			return;
		}
		for (const FixedUpdater& updater : fixed_updaters) {
			updater(*document);
		}
	}

	void Scene::set_rml(string_view id, string_view rml) {
		std::lock_guard lock(scene_mutex);
		if (Rml::Element* element = document->GetElementById(Rml::String(id))) {
			Rml::String next(rml);
			if (element->GetInnerRML() != next) {
				string focused_id;
				string focused_value;
				Rml::Context* context = document->GetContext();
				Rml::Element* focused = context ? context->GetFocusElement() : nullptr;
				if (focused && !focused->GetId().empty()) {
					focused_id = string(focused->GetId());
					if (auto* input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(focused)) {
						focused_value = string(input->GetValue());
					}
				}
				element->SetInnerRML(next);
				if (!focused_id.empty()) {
					if (Rml::Element* restored = document->GetElementById(Rml::String(focused_id))) {
						if (auto* input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(restored)) {
							input->SetValue(Rml::String(focused_value));
							restored->SetAttribute("value", Rml::String(focused_value));
						}
						restored->Focus(true);
					}
				}
			}
		}
	}

	void Scene::set_attribute(string_view id, string_view name, string_view value) {
		std::lock_guard lock(scene_mutex);
		if (Rml::Element* element = document->GetElementById(Rml::String(id))) {
			Rml::String attribute_name(name);
			Rml::String next(value);
			if (element->GetAttribute<Rml::String>(attribute_name, "") != next) {
				element->SetAttribute(attribute_name, next);
			}
		}
	}

	void Scene::set_attribute(string_view id, string_view name, f32 value) {
		std::lock_guard lock(scene_mutex);
		if (Rml::Element* element = document->GetElementById(Rml::String(id))) {
			element->SetAttribute(Rml::String(name), value);
		}
	}

	void Scene::queue_script(string script, string source_name) {
		std::lock_guard lock(scene_mutex);
		pending_scripts.push_back({ std::move(source_name), std::move(script) });
	}

	std::optional<Scene::LoadRequest> Scene::take_load_request() {
		std::optional<LoadRequest> request = std::move(pending_load_request);
		pending_load_request.reset();
		return request;
	}

	void Scene::request_load(string_view path, string_view args) {
		pending_load_request = LoadRequest{ string(path), string(args) };
	}

	bool Scene::has_text_input_focus() const {
		if (!document) {
			return false;
		}

		Rml::Context* context = document->GetContext();
		Rml::Element* focused = context ? context->GetFocusElement() : nullptr;
		return focused && rmlui_dynamic_cast<Rml::ElementFormControlInput*>(focused) != nullptr;
	}

	bool Scene::accepts_text_input(u32 character) const {
		if (!document) {
			return true;
		}

		Rml::Context* context = document->GetContext();
		Rml::Element* focused = context ? context->GetFocusElement() : nullptr;
		if (!focused || focused->GetAttribute<Rml::String>("input-filter", "") != "digits") {
			return true;
		}

		return character >= '0' && character <= '9';
	}

	string Scene::filter_text_input(string_view text) const {
		if (!document) {
			log::Error("[scene] filter_text_input called before a document was loaded");
			throw runtime_exception("filter_text_input called before a document was loaded");
		}

		Rml::Context* context = document->GetContext();
		Rml::Element* focused = context ? context->GetFocusElement() : nullptr;
		if (!focused || focused->GetAttribute<Rml::String>("input-filter", "") != "digits") {
			return string(text);
		}

		string filtered;
		filtered.reserve(text.size());
		for (char character : text) {
			if (character >= '0' && character <= '9') {
				filtered += character;
			}
		}
		return filtered;
	}

	void Scene::refresh_title() {
		title_text = document ? string(document->GetTitle()) : string();
	}

	string_view Scene::title() const {
		return title_text;
	}
} // namespace lf
