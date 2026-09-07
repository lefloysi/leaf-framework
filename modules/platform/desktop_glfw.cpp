#include "leaf/platform/platform.hpp"

#include "leaf/core/exception.hpp"
#include "leaf/core/logging.hpp"
#include "leaf/graphics/resource.hpp"
#include "leaf/application/window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <rt_glfw_swapchain.h>
#include <rt_swapchain.h>

#include <algorithm>
#include <array>
#include <vector>

static lf::PlatformWindow* from_glfw(GLFWwindow* wnd) { return reinterpret_cast<lf::PlatformWindow*>(wnd); }
static GLFWwindow* to_glfw(lf::PlatformWindow* wnd) { return reinterpret_cast<GLFWwindow*>(wnd); }
static lf::Window* owner(GLFWwindow* wnd) { return static_cast<lf::Window*>(glfwGetWindowUserPointer(wnd)); }

static lf::input_key input_key_from_glfw(int key) {
	if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
		return static_cast<lf::input_key>(lf::KEY_A + key - GLFW_KEY_A);
	}
	if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
		return static_cast<lf::input_key>(lf::KEY_0 + key - GLFW_KEY_0);
	}
	if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) {
		return static_cast<lf::input_key>(lf::KEY_NUMPAD_0 + key - GLFW_KEY_KP_0);
	}
	if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F24) {
		return static_cast<lf::input_key>(lf::KEY_F1 + key - GLFW_KEY_F1);
	}

	switch (key) {
	case GLFW_KEY_BACKSPACE: /******/ return lf::KEY_BACKSPACE;
	case GLFW_KEY_TAB: /************/ return lf::KEY_TAB;
	case GLFW_KEY_ENTER: /**********/ return lf::KEY_ENTER;
	case GLFW_KEY_ESCAPE: /*********/ return lf::KEY_ESCAPE;
	case GLFW_KEY_SPACE: /**********/ return lf::KEY_SPACE;
	case GLFW_KEY_DELETE: /*********/ return lf::KEY_DELETE;
	case GLFW_KEY_INSERT: /*********/ return lf::KEY_INSERT;
	case GLFW_KEY_HOME: /***********/ return lf::KEY_HOME;
	case GLFW_KEY_END: /************/ return lf::KEY_END;
	case GLFW_KEY_PAGE_UP: /********/ return lf::KEY_PAGE_UP;
	case GLFW_KEY_PAGE_DOWN: /******/ return lf::KEY_PAGE_DOWN;
	case GLFW_KEY_LEFT: /***********/ return lf::KEY_LEFT_ARROW;
	case GLFW_KEY_RIGHT: /**********/ return lf::KEY_RIGHT_ARROW;
	case GLFW_KEY_UP: /*************/ return lf::KEY_UP_ARROW;
	case GLFW_KEY_DOWN: /***********/ return lf::KEY_DOWN_ARROW;
	case GLFW_KEY_LEFT_ALT: /*******/ return lf::KEY_ALT_LEFT;
	case GLFW_KEY_RIGHT_ALT: /******/ return lf::KEY_ALT_RIGHT;
	case GLFW_KEY_LEFT_CONTROL: /***/ return lf::KEY_CTRL_LEFT;
	case GLFW_KEY_RIGHT_CONTROL: /**/ return lf::KEY_CTRL_RIGHT;
	case GLFW_KEY_LEFT_SHIFT: /*****/ return lf::KEY_SHIFT_LEFT;
	case GLFW_KEY_RIGHT_SHIFT: /****/ return lf::KEY_SHIFT_RIGHT;
	case GLFW_KEY_LEFT_SUPER: /*****/ return lf::KEY_SUPER_LEFT;
	case GLFW_KEY_RIGHT_SUPER: /****/ return lf::KEY_SUPER_RIGHT;
	case GLFW_KEY_NUM_LOCK: /*******/ return lf::KEY_NUM_LOCK;
	case GLFW_KEY_SCROLL_LOCK: /****/ return lf::KEY_SCROLL_LOCK;
	case GLFW_KEY_CAPS_LOCK: /******/ return lf::KEY_CAPS_LOCK;
	case GLFW_KEY_PAUSE: /**********/ return lf::KEY_PAUSE;
	case GLFW_KEY_PRINT_SCREEN: /***/ return lf::KEY_PRINT;
	case GLFW_KEY_KP_ADD: /*********/ return lf::KEY_NUMPAD_ADD;
	case GLFW_KEY_KP_DECIMAL: /*****/ return lf::KEY_NUMPAD_DECIMAL;
	case GLFW_KEY_KP_DIVIDE: /******/ return lf::KEY_NUMPAD_DIVIDE;
	case GLFW_KEY_KP_ENTER: /*******/ return lf::KEY_NUMPAD_ENTER;
	case GLFW_KEY_KP_MULTIPLY: /****/ return lf::KEY_NUMPAD_MULTIPLY;
	case GLFW_KEY_KP_SUBTRACT: /****/ return lf::KEY_NUMPAD_SUBTRACT;
	case GLFW_KEY_GRAVE_ACCENT: /***/ return lf::KEY_BACKQUOTE;
	case GLFW_KEY_BACKSLASH: /******/ return lf::KEY_BACKSLASH;
	case GLFW_KEY_LEFT_BRACKET: /***/ return lf::KEY_BRACKET_LEFT;
	case GLFW_KEY_RIGHT_BRACKET: /**/ return lf::KEY_BRACKET_RIGHT;
	case GLFW_KEY_COMMA: /**********/ return lf::KEY_COMMA;
	case GLFW_KEY_EQUAL: /**********/ return lf::KEY_EQUAL;
	case GLFW_KEY_MINUS: /**********/ return lf::KEY_MINUS;
	case GLFW_KEY_PERIOD: /*********/ return lf::KEY_PERIOD;
	case GLFW_KEY_APOSTROPHE: /*****/ return lf::KEY_QUOTE;
	case GLFW_KEY_SEMICOLON: /******/ return lf::KEY_SEMICOLON;
	case GLFW_KEY_SLASH: /**********/ return lf::KEY_SLASH;
	case GLFW_KEY_MENU: /***********/ return lf::KEY_CONTEXT_MENU;
	default: return lf::KEY_NULL;
	}
}

static lf::input_modifiers input_modifiers_from_glfw(int mods) {
	lf::input_modifiers modifiers;
	if (mods & GLFW_MOD_CONTROL) {
		modifiers.add(lf::INPUT_MODIFIER_CTRL);
	}
	if (mods & GLFW_MOD_SHIFT) {
		modifiers.add(lf::INPUT_MODIFIER_SHIFT);
	}
	if (mods & GLFW_MOD_ALT) {
		modifiers.add(lf::INPUT_MODIFIER_ALT);
	}
	if (mods & GLFW_MOD_SUPER) {
		modifiers.add(lf::INPUT_MODIFIER_SUPER);
	}
	return modifiers;
}

static void mouse_button_callback(GLFWwindow* wnd, int button, int action, int mods) {
	if (action == GLFW_PRESS || action == GLFW_RELEASE) {
		lf::Window* window = owner(wnd);
		if (!window) {
			return;
		}
		lf::input_button input_button = static_cast<lf::input_button>(button + 1);
		bool down = action == GLFW_PRESS;
		double x = 0.0;
		double y = 0.0;
		glfwGetCursorPos(wnd, &x, &y);
		window->on_cursor({ static_cast<f32>(x), static_cast<f32>(y) });
		window->on_control(
			{ lf::INPUT_CONTROL_BUTTON, static_cast<u16>(input_button) },
			down,
			input_modifiers_from_glfw(mods)
		);
	}
}

static void key_callback(GLFWwindow* wnd, int key, int, int action, int mods) {
	if (action == GLFW_PRESS || action == GLFW_RELEASE || action == GLFW_REPEAT) {
		lf::input_key input_key = input_key_from_glfw(key);
		if (input_key != lf::KEY_NULL) {
			lf::Window* window = owner(wnd);
			if (!window) {
				return;
			}
			bool down = action != GLFW_RELEASE;
			lf::input_modifiers modifiers = input_modifiers_from_glfw(mods);
			window->on_control({ lf::INPUT_CONTROL_KEY, static_cast<u16>(input_key) }, down, modifiers);
		}
	}
}

static void char_callback(GLFWwindow* wnd, unsigned int codepoint) {
	lf::Window* window = owner(wnd);
	if (!window) {
		return;
	}
	window->on_text(static_cast<u32>(codepoint));
}

static void cursor_position_callback(GLFWwindow* wnd, double x, double y) {
	lf::Window* window = owner(wnd);
	if (!window) {
		return;
	}
	window->on_cursor({
		static_cast<f32>(x),
		static_cast<f32>(y),
	});
}

static void cursor_enter_callback(GLFWwindow* wnd, int entered) {
	if (lf::Window* window = owner(wnd)) {
		window->on_cursor_enter(entered == GLFW_TRUE);
	}
}

static void scroll_callback(GLFWwindow* wnd, double x, double y) {
	if (lf::Window* window = owner(wnd)) {
		window->on_scroll({
			static_cast<f32>(x),
			static_cast<f32>(y),
		});
	}
}

static void focus_callback(GLFWwindow* wnd, int focused) {
	if (lf::Window* window = owner(wnd)) {
		window->on_focus(focused == GLFW_TRUE);
	}
}

static void drop_callback(GLFWwindow* wnd, int count, const char** paths) {
	lf::Window* window = owner(wnd);
	if (!window) {
		return;
	}
	for (int i = 0; i < count; ++i) {
		window->on_drop(paths[i]);
	}
}

namespace lf {
	error init_platform(span<string_view> args) {
		log::Info("[leaf] Starting platform...");
		if (!glfwInit()) {
			// TODO
			// to_error whatever();
			return error::unknown_error;
		}
		return error::no_error;
	}
	void exit_platform() {
		glfwTerminate();
	}
	string_view platform_backend_name() {
		return "GLFW";
	}

	PlatformWindow* create_platform_window(const PlatformWindowCreateInfo& info) {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

		GLFWwindow* wnd = glfwCreateWindow(static_cast<i32>(info.width), static_cast<i32>(info.height), info.title.data(), nullptr, nullptr);
		if (!wnd) {
			// TODO
			// throw_glfw_error();
			throw lf::runtime_exception("failed to create GLFW window");
		}

		return from_glfw(wnd);
	}

	void destroy_platform_window(PlatformWindow* wnd) {
		glfwSetMouseButtonCallback(to_glfw(wnd), nullptr);
		glfwSetKeyCallback(to_glfw(wnd), nullptr);
		glfwSetCharCallback(to_glfw(wnd), nullptr);
		glfwSetCursorPosCallback(to_glfw(wnd), nullptr);
		glfwSetCursorEnterCallback(to_glfw(wnd), nullptr);
		glfwSetScrollCallback(to_glfw(wnd), nullptr);
		glfwSetWindowFocusCallback(to_glfw(wnd), nullptr);
		glfwSetDropCallback(to_glfw(wnd), nullptr);
		glfwSetWindowUserPointer(to_glfw(wnd), nullptr);
		glfwDestroyWindow(to_glfw(wnd));
	}

	void bind_platform_window_swapchain(PlatformWindow* wnd, rt::view<rt::swapchain> swapchain) {
		rtSwapchainBindGLFW(swapchain, to_glfw(wnd));
		rt::detail::check_rutile_error("failed to bind GLFW wnd to swapchain");
	}

	void platform_window_owner(PlatformWindow* wnd, Window* owner) {
		if (!wnd) {
			return;
		}
		glfwSetWindowUserPointer(to_glfw(wnd), owner);
		glfwSetMouseButtonCallback(to_glfw(wnd), mouse_button_callback);
		glfwSetKeyCallback(to_glfw(wnd), key_callback);
		glfwSetCharCallback(to_glfw(wnd), char_callback);
		glfwSetCursorPosCallback(to_glfw(wnd), cursor_position_callback);
		glfwSetCursorEnterCallback(to_glfw(wnd), cursor_enter_callback);
		glfwSetScrollCallback(to_glfw(wnd), scroll_callback);
		glfwSetWindowFocusCallback(to_glfw(wnd), focus_callback);
		glfwSetDropCallback(to_glfw(wnd), drop_callback);
	}

	void platform_window_clear_owner(PlatformWindow* wnd) {
		if (!wnd) {
			return;
		}
		glfwSetWindowUserPointer(to_glfw(wnd), nullptr);
	}

	void platform_window_title(PlatformWindow* wnd, string_view title) {
		glfwSetWindowTitle(to_glfw(wnd), title.data());
	}

	void platform_window_show(PlatformWindow* wnd) {
		glfwShowWindow(to_glfw(wnd));
	}

	void platform_window_size(PlatformWindow* wnd, dim2<u32> size) {
		glfwSetWindowSize(to_glfw(wnd), static_cast<i32>(size.width), static_cast<i32>(size.height));
	}

	dim2<u32> platform_window_size(PlatformWindow* wnd) {
		int width = 0;
		int height = 0;
		glfwGetWindowSize(to_glfw(wnd), &width, &height);
		return { static_cast<u32>(width), static_cast<u32>(height) };
	}

	dim2<u32> platform_framebuffer_size(PlatformWindow* wnd) {
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(to_glfw(wnd), &width, &height);
		return { static_cast<u32>(width), static_cast<u32>(height) };
	}

	bool platform_window_drawable(PlatformWindow* wnd) {
		GLFWwindow* glfw_window = to_glfw(wnd);
		if (glfwGetWindowAttrib(glfw_window, GLFW_ICONIFIED) == GLFW_TRUE) {
			return false;
		}

		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(glfw_window, &width, &height);
		return width > 0 && height > 0;
	}

	void platform_window_position(PlatformWindow* wnd, pos2<i32> position) {
		glfwSetWindowPos(to_glfw(wnd), position.x, position.y);
	}

	pos2<i32> platform_window_position(PlatformWindow* wnd) {
		int x = 0;
		int y = 0;
		glfwGetWindowPos(to_glfw(wnd), &x, &y);
		return { static_cast<i32>(x), static_cast<i32>(y) };
	}

	void platform_window_fullscreen(PlatformWindow* wnd, bool fullscreen, pos2<i32> windowed_position, dim2<u32> windowed_size) {
		GLFWwindow* window = to_glfw(wnd);
		if (fullscreen) {
			GLFWmonitor* monitor = glfwGetPrimaryMonitor();
			if (!monitor) {
				return;
			}
			const GLFWvidmode* mode = glfwGetVideoMode(monitor);
			if (!mode) {
				return;
			}
			int monitor_x = 0;
			int monitor_y = 0;
			glfwGetMonitorPos(monitor, &monitor_x, &monitor_y);
			glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
			glfwSetWindowAttrib(window, GLFW_RESIZABLE, GLFW_FALSE);
			glfwSetWindowAttrib(window, GLFW_FLOATING, GLFW_FALSE);
			glfwSetWindowMonitor(window, nullptr, monitor_x, monitor_y, mode->width, mode->height, GLFW_DONT_CARE);
			glfwFocusWindow(window);
			return;
		}

		glfwSetWindowAttrib(window, GLFW_FLOATING, GLFW_FALSE);
		glfwSetWindowMonitor(
			window,
			nullptr,
			windowed_position.x,
			windowed_position.y,
			static_cast<int>(windowed_size.width),
			static_cast<int>(windowed_size.height),
			GLFW_DONT_CARE
		);
		glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
		glfwSetWindowAttrib(window, GLFW_RESIZABLE, GLFW_TRUE);
	}

	bool platform_window_should_close(PlatformWindow* wnd) {
		return glfwWindowShouldClose(to_glfw(wnd));
	}

	void platform_window_should_close(PlatformWindow* wnd, bool should_close) {
		glfwSetWindowShouldClose(to_glfw(wnd), should_close ? GLFW_TRUE : GLFW_FALSE);
	}

	PlatformCursor* create_platform_cursor(const u08* rgba, u32 width, u32 height, u32 hotspot_x, u32 hotspot_y) {
		GLFWimage image;
		image.width = static_cast<int>(width);
		image.height = static_cast<int>(height);
		image.pixels = const_cast<unsigned char*>(rgba);
		return reinterpret_cast<PlatformCursor*>(glfwCreateCursor(&image, static_cast<int>(hotspot_x), static_cast<int>(hotspot_y)));
	}

	void destroy_platform_cursor(PlatformCursor* cursor) {
		if (cursor) {
			glfwDestroyCursor(reinterpret_cast<GLFWcursor*>(cursor));
		}
	}

	void platform_window_cursor(PlatformWindow* wnd, PlatformCursor* cursor) {
		glfwSetCursor(to_glfw(wnd), reinterpret_cast<GLFWcursor*>(cursor));
	}

	bool update_platform() {
		glfwWaitEventsTimeout(1.0 / 120.0);
		return true;
	}
	void platform_clipboard_text(string_view text) {
		glfwSetClipboardString(nullptr, text.data());
	}

	string platform_clipboard_text() {
		const char* text = glfwGetClipboardString(nullptr);
		return text ? string(text) : string();
	}
} // namespace lf
