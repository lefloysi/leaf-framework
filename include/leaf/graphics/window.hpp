#ifndef LEAF_GRAPHICS_WINDOW_HPP
#define LEAF_GRAPHICS_WINDOW_HPP

<<<<<<< Updated upstream:include/leaf/graphics/window.hpp
=======
#include <leaf/core/array.hpp>
#include <leaf/core/math/dim.hpp>
#include <leaf/core/math/pos.hpp>
>>>>>>> Stashed changes:include/leaf/application/window.hpp
#include <leaf/core/string.hpp>
#include <leaf/core/vector.hpp>
#include <leaf/graphics/resource.hpp>
#include <leaf/math/dim.hpp>
#include <leaf/math/pos.hpp>
#include <leaf/resource/prototypes/cursor.hpp>

<<<<<<< Updated upstream:include/leaf/graphics/window.hpp
#include <vector>
=======
#include <mutex>
>>>>>>> Stashed changes:include/leaf/application/window.hpp

namespace lf {
	bool SetCursorPrototype(rt::view<rt::window> display, CursorPrototype::ID id);
}

namespace rt {

	struct window_t;
	struct PlatformCursor;

	template<>
	struct resource_traits<window> {
		using native_handle = window_t*;
		static void destroy(native_handle handle);
	};

	enum input_key : u08 {
		KEY_NULL = 0,

		KEY_BROWSER_BACK,
		KEY_BROWSER_FORWARD,
		KEY_BROWSER_REFRESH,
		KEY_VOLUME_MUTE,
		KEY_VOLUME_DOWN,
		KEY_VOLUME_UP,
		KEY_MEDIA_NEXT_TRACK,
		KEY_MEDIA_PREV_TRACK,
		KEY_MEDIA_STOP,
		KEY_MEDIA_PLAY_PAUSE,
		KEY_LAUNCH_MAIL,

		KEY_CANCEL,
		KEY_CONTEXT_MENU,
		KEY_EXECUTE,
		KEY_HELP,

		KEY_PAUSE,
		KEY_PRINT,

		KEY_SELECT,
		KEY_ESCAPE,
		KEY_HOME,
		KEY_START,

		KEY_PAGE_DOWN,
		KEY_PAGE_UP,

		KEY_DOWN_ARROW,
		KEY_LEFT_ARROW,
		KEY_RIGHT_ARROW,
		KEY_UP_ARROW,

		KEY_ALT_LEFT,
		KEY_ALT_RIGHT,
		KEY_CTRL_LEFT,
		KEY_CTRL_RIGHT,
		KEY_SHIFT_LEFT,
		KEY_SHIFT_RIGHT,
		KEY_SUPER_LEFT,
		KEY_SUPER_RIGHT,

		KEY_NUM_LOCK,
		KEY_SCROLL_LOCK,
		KEY_CAPS_LOCK,
		KEY_STANDBY,

		KEY_BACKSPACE,
		KEY_CLEAR,
		KEY_END,
		KEY_DELETE,
		KEY_INSERT,

		KEY_A,
		KEY_B,
		KEY_C,
		KEY_D,
		KEY_E,
		KEY_F,
		KEY_G,
		KEY_H,
		KEY_I,
		KEY_J,
		KEY_K,
		KEY_L,
		KEY_M,
		KEY_N,
		KEY_O,
		KEY_P,
		KEY_Q,
		KEY_R,
		KEY_S,
		KEY_T,
		KEY_U,
		KEY_V,
		KEY_W,
		KEY_X,
		KEY_Y,
		KEY_Z,
		KEY_0,
		KEY_1,
		KEY_2,
		KEY_3,
		KEY_4,
		KEY_5,
		KEY_6,
		KEY_7,
		KEY_8,
		KEY_9,

		KEY_NUMPAD_0,
		KEY_NUMPAD_1,
		KEY_NUMPAD_2,
		KEY_NUMPAD_3,
		KEY_NUMPAD_4,
		KEY_NUMPAD_5,
		KEY_NUMPAD_6,
		KEY_NUMPAD_7,
		KEY_NUMPAD_8,
		KEY_NUMPAD_9,
		KEY_NUMPAD_ADD,
		KEY_NUMPAD_DECIMAL,
		KEY_NUMPAD_DIVIDE,
		KEY_NUMPAD_ENTER,
		KEY_NUMPAD_MULTIPLY,
		KEY_NUMPAD_SUBTRACT,

		KEY_BACKQUOTE,
		KEY_BACKSLASH,
		KEY_BRACKET_LEFT,
		KEY_BRACKET_RIGHT,
		KEY_COMMA,
		KEY_ENTER,
		KEY_EQUAL,
		KEY_HASH,
		KEY_MINUS,
		KEY_PERIOD,
		KEY_QUOTE,
		KEY_SEMICOLON,
		KEY_SLASH,
		KEY_SPACE,
		KEY_TAB,

		KEY_F1,
		KEY_F2,
		KEY_F3,
		KEY_F4,
		KEY_F5,
		KEY_F6,
		KEY_F7,
		KEY_F8,
		KEY_F9,
		KEY_F10,
		KEY_F11,
		KEY_F12,
		KEY_F13,
		KEY_F14,
		KEY_F15,
		KEY_F16,
		KEY_F17,
		KEY_F18,
		KEY_F19,
		KEY_F20,
		KEY_F21,
		KEY_F22,
		KEY_F23,
		KEY_F24,

		KEY_ENUM_MAX,
	};

	enum input_button : u08 {
		BUTTON_NULL = 0x00,
		BUTTON_1 = 0x01,
		BUTTON_2 = 0x02,
		BUTTON_3 = 0x03,
		BUTTON_4 = 0x04,
		BUTTON_5 = 0x05,
		BUTTON_6 = 0x06,
		BUTTON_7 = 0x07,
		BUTTON_8 = 0x08,
		BUTTON_ENUM_MAX,
		BUTTON_LEFT = BUTTON_1,
		BUTTON_RIGHT = BUTTON_2,
		BUTTON_MIDDLE = BUTTON_3,
	};

	enum input_state : u08 {
		Up,
		Pressed,
		Down,
		Released,
	};

	enum input_modifier : u08 {
		INPUT_MODIFIER_CTRL = (1 << 0),
		INPUT_MODIFIER_SHIFT = (1 << 1),
		INPUT_MODIFIER_ALT = (1 << 2),
		INPUT_MODIFIER_SUPER = (1 << 3),
	};
	struct input_modifiers {
		u08 value = 0;

		void add(input_modifier modifier) {
			value |= modifier;
		}

		bool has(input_modifier modifier) const {
			return (value & modifier) != 0;
		}
	};

	enum input_event_type : u08 {
		INPUT_EVENT_CONTROL,
		INPUT_EVENT_POINTER_MOVE,
		INPUT_EVENT_POINTER_ENTER,
		INPUT_EVENT_SCROLL,
		INPUT_EVENT_TEXT,
		INPUT_EVENT_FOCUS,
		INPUT_EVENT_DROP,
	};

	enum input_control_type : u08 {
		INPUT_CONTROL_NONE,
		INPUT_CONTROL_KEY,
		INPUT_CONTROL_BUTTON,
	};

	struct input_control {
		input_control_type type = INPUT_CONTROL_NONE;
		u16 value = 0;
	};

	struct input_event {
		input_event_type type = INPUT_EVENT_CONTROL;
		input_control control;
		input_state state = input_state::Up;
		input_modifiers modifiers;
		pos2<f32> position;
		pos2<f32> delta;
		u32 character = 0;
		string text;
	};

	namespace Window {
		handle<window> Create();
		void Destroy(handle<window> window);
		void SetTitle(view<window> window, string_view title);
		void Show(view<window> window);
		void SetWidth(view<window> window, u32 width);
		void SetHeight(view<window> window, u32 height);
		void SetFullscreen(view<window> window, bool fullscreen);
		void SetVsync(view<window> window, bool enabled);
		void RequestFullscreen(view<window> window, bool fullscreen);
		bool FullscreenRequestPending(view<window> window);
		bool ApplyFullscreenRequest(view<window> window);
		bool Fullscreen(view<const window> window);
		void ApplyCursor(view<window> window, string_view name, PlatformCursor* cursor);
		bool Drawable(view<const window> window);
		bool ShouldClose(view<const window> window);
		void SetShouldClose(view<window> window, bool should_close);

		dim2<u32> Size(view<const window> window);
		std::vector<input_event> InputEvents(view<window> window);
		input_state InputState(view<const window> window, input_control control);
		void UpdateInput(view<window> window);
		bool MouseDown(view<const window> window, input_button button);
		bool MousePressed(view<const window> window, input_button button);
		bool MouseReleased(view<const window> window, input_button button);
		bool KeyDown(view<const window> window, input_key key);
		bool KeyPressed(view<const window> window, input_key key);
		bool KeyReleased(view<const window> window, input_key key);
		view<framebuffer> CurrentFramebuffer(view<window> window);
		view<const framebuffer> CurrentFramebuffer(view<const window> window);
		view<command_buffer> BeginFrame(view<window> window, view<queue> queue);
		void EndFrame(view<window> window);
	} // namespace Window

} // namespace rt

<<<<<<< Updated upstream:include/leaf/graphics/window.hpp
#endif /* LEAF_GRAPHICS_WINDOW_HPP */
=======
		void on_control(input_control control, bool down, input_modifiers modifiers);
		void on_text(u32 character);
		void on_cursor(pos2<f32> position);
		void on_cursor_enter(bool entered);
		void on_scroll(pos2<f32> delta);
		void on_focus(bool focused);
		void on_drop(string_view path);

	  private:
		static constexpr size_t control_count = KEY_ENUM_MAX + BUTTON_ENUM_MAX;
		static size_t control_index(input_control control);
		void discard_frame();

		PlatformWindow* platform = nullptr;
		rt::unique<rt::swapchain> swapchain;
		rt::unique<rt::queue> queue;
		rt::unique<rt::command_buffer> frame_command_buffer;

		rt::view<rt::framebuffer> frame_buffer;
		rt::timepoint frame_rendered;
		bool frame_submitted = false;

		dim2<u32> extent = { 1280, 720 };
		pos2<f32> position = { 100, 100 };
		pos2<f32> cursor_position{};
		CursorPrototype::ID current_cursor;

		array<input_state, control_count> controls{};
		vector<input_event> events;
		bool cursor_inside = false;
		bool fullscreen_enabled = false;
		bool vsync_enabled = false;
		input_modifiers modifiers{};

		mutable std::mutex input_mutex;
	};

} // namespace lf

#endif /* LEAF_APPLICATION_WINDOW_HPP */
>>>>>>> Stashed changes:include/leaf/application/window.hpp
