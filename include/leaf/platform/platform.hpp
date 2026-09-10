#pragma once

#include "leaf/core/error.hpp"
#include "leaf/core/math/dim.hpp"
#include "leaf/core/math/pos.hpp"
#include "leaf/core/span.hpp"
#include "leaf/core/string.hpp"
#include "leaf/core/types.hpp"
#include "leaf/graphics/resource.hpp"

namespace lf {
	class Window;
	struct PlatformCursor;
	struct PlatformWindow;

	struct PlatformWindowCreateInfo {
		string_view title;
		u32 width = 1280;
		u32 height = 720;
	};

	error init_platform(span<string_view> args);
	void exit_platform();
	string_view platform_backend_name();

	PlatformWindow* create_platform_window(const PlatformWindowCreateInfo& info);
	void destroy_platform_window(PlatformWindow* window);
	void bind_platform_window_swapchain(PlatformWindow* window, rt::view<rt::swapchain> swapchain);
	void platform_window_owner(PlatformWindow* window, Window* owner);
	void platform_window_clear_owner(PlatformWindow* window);
	void platform_window_title(PlatformWindow* window, string_view title);
	void platform_window_show(PlatformWindow* window);
	void platform_window_hide(PlatformWindow* window);
	void platform_window_size(PlatformWindow* window, dim2<u32> size);
	dim2<u32> platform_window_size(PlatformWindow* window);
	dim2<u32> platform_framebuffer_size(PlatformWindow* window);
	bool platform_window_drawable(PlatformWindow* window);
	void platform_window_position(PlatformWindow* window, pos2<i32> position);
	pos2<i32> platform_window_position(PlatformWindow* window);
	void platform_window_fullscreen(PlatformWindow* window, bool fullscreen, pos2<i32> windowed_position, dim2<u32> windowed_size);
	bool platform_window_should_close(PlatformWindow* window);
	void platform_window_should_close(PlatformWindow* window, bool should_close);
	PlatformCursor* create_platform_cursor(const u08* rgba, u32 width, u32 height, u32 hotspot_x, u32 hotspot_y);
	void destroy_platform_cursor(PlatformCursor* cursor);
	void platform_window_cursor(PlatformWindow* window, PlatformCursor* cursor);
	void platform_clipboard_text(string_view text);
	string platform_clipboard_text();
	bool update_platform();
} // namespace lf
