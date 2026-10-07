#include "leaf/platform/platform.hpp"

#include "leaf/application/window.hpp"
#include "leaf/core/exception.hpp"
#include "leaf/core/logging.hpp"
#include "leaf/core/profiler.hpp"
#include "leaf/core/singleton.hpp"
#include "leaf/graphics/resource.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <rt_glfw_swapchain.h>
#include <rt_swapchain.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <deque>
#include <exception>
#include <future>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace lf {
	struct PlatformWindow {
		GLFWwindow* native = nullptr;
		Window* owner = nullptr;
		mutable std::mutex state_mutex;
		dim2<u32> extent{};
		dim2<u32> framebuffer_extent{};
		pos2<i32> position{};
		bool visible = false;
		bool iconified = false;
		bool should_close = false;
	};

	struct PlatformCursor {
		GLFWcursor* native = nullptr;
	};

	class Platform : public Singleton<Platform> {
	  public:
		void initialize() {
			owner_thread = std::this_thread::get_id();
		}

		bool owns_current_thread() const {
			return std::this_thread::get_id() == owner_thread;
		}

		template <typename Function>
		auto invoke(Function&& function) -> std::invoke_result_t<Function> {
			using Result = std::invoke_result_t<Function>;
			if (owns_current_thread()) {
				if constexpr (std::is_void_v<Result>) {
					function();
					return;
				} else {
					return function();
				}
			}

			auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<Function>(function));
			auto result = task->get_future();
			LF_PROFILE_SCOPE("platform.request-wait");
			{
				std::lock_guard lock(request_mutex);
				requests.emplace_back([task] { (*task)(); });
			}
			glfwPostEmptyEvent();
			if constexpr (std::is_void_v<Result>) {
				result.get();
			} else {
				return result.get();
			}
		}

		void run(const std::function<void()>& application) {
			assert(owns_current_thread());
			std::exception_ptr exception;
			std::atomic<bool> running = true;
			std::thread worker = std::thread([&] {
				try {
					application();
				} catch (...) {
					exception = std::current_exception();
				}
				running.store(false, std::memory_order_release);
				glfwPostEmptyEvent();
			});

			while (running.load(std::memory_order_acquire)) {
				glfwWaitEvents();
				drain();
			}
			drain();
			worker.join();
			if (exception) {
				std::rethrow_exception(exception);
			}
		}

		bool update() {
			if (owns_current_thread()) {
				glfwWaitEvents();
				drain();
			}
			return true;
		}

	  private:
		friend Singleton<Platform>;

		void drain() {
			std::deque<std::function<void()>> pending;
			{
				std::lock_guard lock(request_mutex);
				pending.swap(requests);
			}
			for (const std::function<void()>& request : pending) {
				request();
			}
		}

		std::thread::id owner_thread;
		std::mutex request_mutex;
		std::deque<std::function<void()>> requests;
	};
} // namespace lf

namespace {
	lf::PlatformWindow* from_glfw(GLFWwindow* wnd) { return static_cast<lf::PlatformWindow*>(glfwGetWindowUserPointer(wnd)); }
	GLFWwindow* to_glfw(lf::PlatformWindow* wnd) { return wnd ? wnd->native : nullptr; }
	lf::Window* owner(GLFWwindow* wnd) {
		lf::PlatformWindow* platform_window = from_glfw(wnd);
		return platform_window ? platform_window->owner : nullptr;
	}

	void update_window_extent(GLFWwindow* wnd, int width, int height) {
		if (lf::PlatformWindow* window = from_glfw(wnd)) {
			std::lock_guard lock(window->state_mutex);
			window->extent = { static_cast<u32>(std::max(width, 0)), static_cast<u32>(std::max(height, 0)) };
		}
	}

	void update_framebuffer_extent(GLFWwindow* wnd, int width, int height) {
		if (lf::PlatformWindow* window = from_glfw(wnd)) {
			std::lock_guard lock(window->state_mutex);
			window->framebuffer_extent = { static_cast<u32>(std::max(width, 0)), static_cast<u32>(std::max(height, 0)) };
		}
	}

	void update_window_position(GLFWwindow* wnd, int x, int y) {
		if (lf::PlatformWindow* window = from_glfw(wnd)) {
			std::lock_guard lock(window->state_mutex);
			window->position = { x, y };
		}
	}
} // namespace

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

static void close_callback(GLFWwindow* wnd) {
	if (lf::PlatformWindow* window = from_glfw(wnd)) {
		std::lock_guard lock(window->state_mutex);
		window->should_close = true;
	}
	if (lf::Window* window = owner(wnd)) {
		window->set_should_close(true);
	} else {
		glfwHideWindow(wnd);
	}
}

static void window_extent_callback(GLFWwindow* wnd, int width, int height) {
	update_window_extent(wnd, width, height);
}

static void framebuffer_extent_callback(GLFWwindow* wnd, int width, int height) {
	update_framebuffer_extent(wnd, width, height);
	if (auto* window = owner(wnd)) { window->on_framebuffer_resize(); }
}

static void window_position_callback(GLFWwindow* wnd, int x, int y) {
	update_window_position(wnd, x, y);
}

static void window_iconify_callback(GLFWwindow* wnd, int iconified) {
	if (lf::PlatformWindow* window = from_glfw(wnd)) {
		std::lock_guard lock(window->state_mutex);
		window->iconified = iconified == GLFW_TRUE;
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
		Platform::instance().initialize();
		if (!glfwInit()) {
			return error::unknown_error;
		}
		return error::no_error;
	}

	void exit_platform() {
		Platform::instance().invoke([] { glfwTerminate(); });
	}

	void run_platform(const std::function<void()>& application) {
		Platform::instance().run(application);
	}

	string_view platform_backend_name() {
		return "GLFW";
	}

	PlatformWindow* create_platform_window(const PlatformWindowCreateInfo& info) {
		return Platform::instance().invoke([info] {
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

			auto* window = new PlatformWindow;
			window->native = glfwCreateWindow(static_cast<i32>(info.width), static_cast<i32>(info.height), info.title.data(), nullptr, nullptr);
			if (!window->native) {
				delete window;
				throw runtime_exception("failed to create GLFW window");
			}
			glfwSetWindowUserPointer(window->native, window);
			int width = 0;
			int height = 0;
			glfwGetWindowSize(window->native, &width, &height);
			window->extent = { static_cast<u32>(std::max(width, 0)), static_cast<u32>(std::max(height, 0)) };
			glfwGetFramebufferSize(window->native, &width, &height);
			window->framebuffer_extent = { static_cast<u32>(std::max(width, 0)), static_cast<u32>(std::max(height, 0)) };
			glfwGetWindowPos(window->native, &width, &height);
			window->position = { width, height };
			return window;
		});
	}

	void destroy_platform_window(PlatformWindow* wnd) {
		Platform::instance().invoke([wnd] {
			GLFWwindow* native = to_glfw(wnd);
			if (!native) {
				return;
			}
			glfwSetWindowCloseCallback(native, nullptr);
			glfwSetWindowSizeCallback(native, nullptr);
			glfwSetFramebufferSizeCallback(native, nullptr);
			glfwSetWindowPosCallback(native, nullptr);
			glfwSetWindowIconifyCallback(native, nullptr);
			glfwSetMouseButtonCallback(native, nullptr);
			glfwSetKeyCallback(native, nullptr);
			glfwSetCharCallback(native, nullptr);
			glfwSetCursorPosCallback(native, nullptr);
			glfwSetCursorEnterCallback(native, nullptr);
			glfwSetScrollCallback(native, nullptr);
			glfwSetWindowFocusCallback(native, nullptr);
			glfwSetDropCallback(native, nullptr);
			glfwSetWindowUserPointer(native, nullptr);
			glfwDestroyWindow(native);
			delete wnd;
		});
	}

	void bind_platform_window_swapchain(PlatformWindow* wnd, rt::view<rt::swapchain> swapchain) {
		Platform::instance().invoke([wnd, swapchain] {
			rtSwapchainBindGLFW(swapchain, to_glfw(wnd));
			rt::detail::check_rutile_error("failed to bind GLFW wnd to swapchain");
		});
	}

	void platform_window_owner(PlatformWindow* wnd, Window* owner) {
		Platform::instance().invoke([wnd, owner] {
			if (!wnd) {
				return;
			}
			wnd->owner = owner;
			GLFWwindow* native = to_glfw(wnd);
			glfwSetWindowCloseCallback(native, close_callback);
			glfwSetWindowSizeCallback(native, window_extent_callback);
			glfwSetFramebufferSizeCallback(native, framebuffer_extent_callback);
			glfwSetWindowPosCallback(native, window_position_callback);
			glfwSetWindowIconifyCallback(native, window_iconify_callback);
			glfwSetMouseButtonCallback(native, mouse_button_callback);
			glfwSetKeyCallback(native, key_callback);
			glfwSetCharCallback(native, char_callback);
			glfwSetCursorPosCallback(native, cursor_position_callback);
			glfwSetCursorEnterCallback(native, cursor_enter_callback);
			glfwSetScrollCallback(native, scroll_callback);
			glfwSetWindowFocusCallback(native, focus_callback);
			glfwSetDropCallback(native, drop_callback);
		});
	}

	void platform_window_clear_owner(PlatformWindow* wnd) {
		Platform::instance().invoke([wnd] {
			if (wnd) {
				wnd->owner = nullptr;
			}
		});
	}

	void platform_window_title(PlatformWindow* wnd, string_view title) {
		string owned_title = string(title);
		Platform::instance().invoke([wnd, title = std::move(owned_title)] { glfwSetWindowTitle(to_glfw(wnd), title.c_str()); });
	}

	void platform_window_show(PlatformWindow* wnd) {
		Platform::instance().invoke([wnd] {
			glfwShowWindow(to_glfw(wnd));
			std::lock_guard lock(wnd->state_mutex);
			wnd->visible = true;
		});
	}

	void platform_window_hide(PlatformWindow* wnd) {
		Platform::instance().invoke([wnd] {
			glfwHideWindow(to_glfw(wnd));
			std::lock_guard lock(wnd->state_mutex);
			wnd->visible = false;
		});
	}

	void platform_window_extent(PlatformWindow* wnd, dim2<u32> extent) {
		Platform::instance().invoke([wnd, extent] { glfwSetWindowSize(to_glfw(wnd), static_cast<i32>(extent.width), static_cast<i32>(extent.height)); });
	}

	dim2<u32> platform_window_extent(PlatformWindow* wnd) {
		std::lock_guard lock(wnd->state_mutex);
		return wnd->extent;
	}

	dim2<u32> platform_framebuffer_extent(PlatformWindow* wnd) {
		std::lock_guard lock(wnd->state_mutex);
		return wnd->framebuffer_extent;
	}

	bool platform_window_drawable(PlatformWindow* wnd) {
		std::lock_guard lock(wnd->state_mutex);
		return !wnd->should_close && wnd->visible && !wnd->iconified && wnd->framebuffer_extent.width > 0 && wnd->framebuffer_extent.height > 0;
	}

	void platform_window_position(PlatformWindow* wnd, pos2<i32> position) {
		Platform::instance().invoke([wnd, position] { glfwSetWindowPos(to_glfw(wnd), position.x, position.y); });
	}

	pos2<i32> platform_window_position(PlatformWindow* wnd) {
		std::lock_guard lock(wnd->state_mutex);
		return wnd->position;
	}

	void platform_window_fullscreen(PlatformWindow* wnd, bool fullscreen, pos2<i32> windowed_position, dim2<u32> windowed_extent) {
		Platform::instance().invoke([wnd, fullscreen, windowed_position, windowed_extent] {
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
			glfwSetWindowMonitor(window, nullptr, windowed_position.x, windowed_position.y, static_cast<int>(windowed_extent.width), static_cast<int>(windowed_extent.height), GLFW_DONT_CARE);
			glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
			glfwSetWindowAttrib(window, GLFW_RESIZABLE, GLFW_TRUE);
		});
	}

	bool platform_window_should_close(PlatformWindow* wnd) {
		std::lock_guard lock(wnd->state_mutex);
		return wnd->should_close;
	}

	void platform_window_should_close(PlatformWindow* wnd, bool should_close) {
		Platform::instance().invoke([wnd, should_close] {
			glfwSetWindowShouldClose(to_glfw(wnd), should_close ? GLFW_TRUE : GLFW_FALSE);
			std::lock_guard lock(wnd->state_mutex);
			wnd->should_close = should_close;
		});
	}

	PlatformCursor* create_platform_cursor(const u08* rgba, u32 width, u32 height, u32 hotspot_x, u32 hotspot_y) {
		return Platform::instance().invoke([rgba, width, height, hotspot_x, hotspot_y] {
			GLFWimage image;
			image.width = static_cast<int>(width);
			image.height = static_cast<int>(height);
			image.pixels = const_cast<unsigned char*>(rgba);
			auto* cursor = new PlatformCursor;
			cursor->native = glfwCreateCursor(&image, static_cast<int>(hotspot_x), static_cast<int>(hotspot_y));
			if (!cursor->native) {
				delete cursor;
				return static_cast<PlatformCursor*>(nullptr);
			}
			return cursor;
		});
	}

	void destroy_platform_cursor(PlatformCursor* cursor) {
		if (!cursor) { return; }
		Platform::instance().invoke([cursor] {
			glfwDestroyCursor(cursor->native);
			delete cursor;
		});
	}

	void platform_window_cursor(PlatformWindow* wnd, PlatformCursor* cursor) {
		Platform::instance().invoke([wnd, cursor] { glfwSetCursor(to_glfw(wnd), cursor ? cursor->native : nullptr); });
	}

	bool update_platform() {
		return Platform::instance().update();
	}
	void platform_clipboard_text(string_view text) {
		Platform::instance().invoke([text = string(text)] { glfwSetClipboardString(nullptr, text.c_str()); });
	}

	string platform_clipboard_text() {
		return Platform::instance().invoke([] {
			const char* text = glfwGetClipboardString(nullptr);
			return text ? string(text) : string();
		});
	}
} // namespace lf
