#include "leaf/application/window.hpp"

#include "leaf/core/exception.hpp"
#include "leaf/core/format.hpp"
#include "leaf/core/logging.hpp"
#include "leaf/graphics/command_buffer.hpp"
#include "leaf/graphics/queue.hpp"
#include "leaf/graphics/swapchain.hpp"
#include "leaf/graphics/timepoint.hpp"
#include "leaf/manager/asset.hpp"
#include "leaf/platform/platform.hpp"
#include "leaf/resource/database.hpp"

namespace lf {
	size_t Window::control_index(input_control control) {
		if (control.type == INPUT_CONTROL_KEY) {
			return control.value < KEY_ENUM_MAX ? control.value : control_count;
		}
		if (control.type == INPUT_CONTROL_BUTTON) {
			return control.value < BUTTON_ENUM_MAX ? KEY_ENUM_MAX + control.value : control_count;
		}
		return control_count;
	}

	Window::Window(string_view title, dim2<u32> size) : extent(size) {
		platform = create_platform_window({ title, size.width, size.height });
		if (!platform) {
			throw runtime_exception("failed to create platform window");
		}
		try {
			swapchain = rt::unique(rt::Swapchain::Create());
			bind_platform_window_swapchain(platform, swapchain);
			queue = rt::unique(rt::Queue::Create(rt::queue_capability::graphics));
			frame_command_buffer = rt::unique(rt::CommandBuffer::Create());
			platform_window_owner(platform, this);
		} catch (...) {
			swapchain.reset();
			destroy_platform_window(platform);
			platform = nullptr;
			throw;
		}
	}

	Window::~Window() {
		hide();
		discard_frame();
		rt::Timepoint::Wait(rt::Queue::Flush(queue));
		frame_command_buffer.reset();
		queue.reset();
		platform_window_cursor(platform, nullptr);
		platform_window_clear_owner(platform);
		swapchain.reset();
		destroy_platform_window(platform);
	}

	void Window::set_title(string_view title) {
		platform_window_title(platform, title);
	}
	void Window::show() {
		platform_window_show(platform);
	}

	void Window::hide() {
		platform_window_hide(platform);
	}
	void Window::set_size(dim2<u32> size) {
		if (fullscreen_enabled) {
			return;
		}
		extent = size;
		platform_window_size(platform, size);
	}
	void Window::set_fullscreen(bool enabled) {
		if (fullscreen_enabled == enabled) {
			return;
		}
		if (!enabled) {
			platform_window_fullscreen(platform, false, { static_cast<i32>(position.x), static_cast<i32>(position.y) }, extent);
			fullscreen_enabled = false;
			return;
		}
		const pos2<i32> actual_position = platform_window_position(platform);
		position = { static_cast<f32>(actual_position.x), static_cast<f32>(actual_position.y) };
		extent = platform_window_size(platform);
		platform_window_fullscreen(platform, true, { static_cast<i32>(position.x), static_cast<i32>(position.y) }, extent);
		fullscreen_enabled = enabled;
	}
	bool Window::fullscreen() const {
		return fullscreen_enabled;
	}
	void Window::set_vsync(bool enabled) {
		vsync_enabled = enabled;
		log::Warning("Swapchain vsync control is not supported by this Rutile version");
	}
	bool Window::drawable() const {
		return platform_window_drawable(platform);
	}

	bool Window::should_close() const {
		return platform_window_should_close(platform);
	}

	void Window::set_should_close(bool should_close) {
		platform_window_should_close(platform, should_close);
		if (should_close) { hide(); }
	}

	dim2<u32> Window::size() const {
		return platform_window_size(platform);
	}

	std::vector<input_event> Window::input_events() {
		std::vector<input_event> result;
		std::lock_guard lock(input_mutex);
		result.swap(events);
		return result;
	}
	input_state Window::control_state(input_control control) const {
		const size_t index = control_index(control);
		if (index >= controls.size()) {
			return lf::input_state::Up;
		}
		std::lock_guard lock(input_mutex);
		return controls[index];
	}
	void Window::update_input() {
		std::lock_guard lock(input_mutex);
		for (size_t index = 0; index < controls.size(); ++index) {
			lf::input_state& state = controls[index];
			if (state == lf::input_state::Pressed) {
				state = lf::input_state::Down;
			} else if (state == lf::input_state::Released) {
				state = lf::input_state::Up;
			}
		}
	}
	bool Window::mouse_down(input_button button) const {
		const auto state = control_state({ INPUT_CONTROL_BUTTON, static_cast<u16>(button) });
		return state == input_state::Down || state == input_state::Pressed;
	}
	bool Window::mouse_pressed(input_button button) const { return control_state({ INPUT_CONTROL_BUTTON, static_cast<u16>(button) }) == input_state::Pressed; }
	bool Window::mouse_released(input_button button) const { return control_state({ INPUT_CONTROL_BUTTON, static_cast<u16>(button) }) == input_state::Released; }
	bool Window::key_down(input_key key) const {
		const auto state = control_state({ INPUT_CONTROL_KEY, static_cast<u16>(key) });
		return state == input_state::Down || state == input_state::Pressed;
	}
	bool Window::key_pressed(input_key key) const { return control_state({ INPUT_CONTROL_KEY, static_cast<u16>(key) }) == input_state::Pressed; }
	bool Window::key_released(input_key key) const { return control_state({ INPUT_CONTROL_KEY, static_cast<u16>(key) }) == input_state::Released; }
	rt::view<rt::framebuffer> Window::current_framebuffer() { return frame_buffer; }
	rt::view<const rt::framebuffer> Window::current_framebuffer() const { return frame_buffer; }

	void Window::discard_frame() {
		if (!frame_buffer) {
			return;
		}
		if (frame_submitted) {
			rt::Swapchain::Present(swapchain, frame_rendered);
		} else {
			auto replacement = rt::unique(rt::CommandBuffer::Create());
			frame_command_buffer = std::move(replacement);
			const rt::timepoint released{ rt::Queue::Flush(queue) };
			rt::Swapchain::Present(swapchain, released);
		}
		frame_buffer = {};
		frame_rendered = {};
		frame_submitted = false;
	}

	void Window::submit(rt::view<rt::command_buffer> commands) {
		rt::Timepoint::Wait(rt::Queue::Submit(queue, commands));
	}

	rt::view<rt::command_buffer> Window::begin_frame() {
		discard_frame();
		if (!drawable()) { return {}; }
		const dim2<u32> actual_framebuffer_size = platform_framebuffer_size(platform);
		if (framebuffer_extent.width != actual_framebuffer_size.width || framebuffer_extent.height != actual_framebuffer_size.height) {
			rt::Swapchain::Resize(swapchain, actual_framebuffer_size.width, actual_framebuffer_size.height);
			framebuffer_extent = actual_framebuffer_size;
		}
		const rt_swapchain_acquire_result acquired = rt::Swapchain::Acquire(swapchain);
		frame_buffer.value = acquired.framebuffer;
		if (!frame_buffer) {
			return {};
		}
		frame_rendered = {};
		frame_submitted = false;
		rt::Queue::Wait(queue, acquired.timepoint);
		rt::Cmd::Reset(frame_command_buffer);
		rt::Cmd::Begin(frame_command_buffer);
		asset::update(frame_command_buffer);

		return frame_command_buffer;
	}

	void Window::begin_rendering() {
		if (!frame_buffer) {
			return;
		}

		rt::Cmd::BeginRendering(frame_command_buffer, frame_buffer);
		rt::Cmd::ClearColor(frame_command_buffer, {}, 0.0f, 0.0f, 0.0f, 1.0f);
		rt::Cmd::Clear(frame_command_buffer, rt::clear_flag::color);

		const dim2<u32> framebuffer_size = platform_framebuffer_size(platform);
		rt::Cmd::SetViewport(frame_command_buffer, 0, 0, framebuffer_size.width, framebuffer_size.height, 0.0f, 1.0f);
		rt::Cmd::SetScissor(frame_command_buffer, 0, 0, framebuffer_size.width, framebuffer_size.height);
	}
	void Window::end_frame() {
		if (!frame_buffer) {
			return;
		}
		rt::Cmd::EndRendering(frame_command_buffer);
		rt::Cmd::End(frame_command_buffer);
		frame_rendered = rt::Queue::Submit(queue, frame_command_buffer);
		frame_submitted = true;
		asset::submitted(frame_rendered);
		rt::Swapchain::Present(swapchain, frame_rendered);
		frame_buffer = {};
		frame_rendered = {};
		frame_submitted = false;
	}

	void Window::on_control(input_control control, bool down, input_modifiers next_modifiers) {
		const size_t index = control_index(control);
		if (index >= controls.size()) {
			return;
		}
		std::lock_guard lock(input_mutex);
		modifiers = next_modifiers;
		lf::input_state& state = controls[index];
		if (down && (state == lf::input_state::Up || state == lf::input_state::Released)) {
			state = lf::input_state::Pressed;
			events.push_back({ .type = INPUT_EVENT_CONTROL, .control = control, .state = state, .modifiers = modifiers, .position = cursor_position });
		} else if (!down && (state == lf::input_state::Down || state == lf::input_state::Pressed)) {
			state = lf::input_state::Released;
			events.push_back({ .type = INPUT_EVENT_CONTROL, .control = control, .state = state, .modifiers = modifiers, .position = cursor_position });
		}
	}
	void Window::on_text(u32 character) {
		std::lock_guard lock{ input_mutex };
		events.push_back({ .type = INPUT_EVENT_TEXT, .modifiers = modifiers, .position = cursor_position, .character = character });
	}

	void Window::on_cursor(pos2<f32> position) {
		std::lock_guard lock{ input_mutex };
		cursor_position = position;
		events.push_back({ .type = INPUT_EVENT_CURSOR_MOVE, .modifiers = modifiers, .position = cursor_position });
	}
	void Window::on_cursor_enter(bool entered) {
		std::lock_guard lock(input_mutex);
		cursor_inside = entered;
		events.push_back({ .type = INPUT_EVENT_CURSOR_ENTER, .state = entered ? lf::input_state::Down : lf::input_state::Up, .modifiers = modifiers, .position = cursor_position });
		if (entered) {
			return;
		}
		for (size_t index = KEY_ENUM_MAX; index < controls.size(); ++index) {
			lf::input_state& state = controls[index];
			if (state != lf::input_state::Down && state != lf::input_state::Pressed) {
				continue;
			}
			state = lf::input_state::Released;
			events.push_back({ .type = INPUT_EVENT_CONTROL, .control = { INPUT_CONTROL_BUTTON, static_cast<u16>(index - KEY_ENUM_MAX) }, .state = state, .modifiers = modifiers, .position = cursor_position });
		}
	}
	void Window::on_scroll(pos2<f32> delta) {
		std::lock_guard lock(input_mutex);
		events.push_back({ .type = INPUT_EVENT_SCROLL, .modifiers = modifiers, .position = cursor_position, .delta = delta });
	}
	void Window::on_focus(bool focused) {
		std::lock_guard lock(input_mutex);
		events.push_back({ .type = INPUT_EVENT_FOCUS, .state = focused ? lf::input_state::Down : lf::input_state::Up, .modifiers = modifiers, .position = cursor_position });
		if (focused) {
			return;
		}
		for (size_t index = 0; index < controls.size(); ++index) {
			lf::input_state& state = controls[index];
			if (state != lf::input_state::Down && state != lf::input_state::Pressed) {
				continue;
			}
			state = lf::input_state::Released;
			events.push_back({ .type = INPUT_EVENT_CONTROL, .control = index < KEY_ENUM_MAX ? input_control{ INPUT_CONTROL_KEY, static_cast<u16>(index) } : input_control{ INPUT_CONTROL_BUTTON, static_cast<u16>(index - KEY_ENUM_MAX) }, .state = state, .modifiers = modifiers, .position = cursor_position });
		}
		modifiers = {};
	}
	void Window::on_drop(string_view path) {
		std::lock_guard lock(input_mutex);
		events.push_back({ .type = INPUT_EVENT_DROP, .modifiers = modifiers, .position = cursor_position, .path = string(path) });
	}

	bool Window::set_cursor(CursorPrototype::ID id) {
		if (current_cursor == id) {
			return true;
		}
		if (!id) {
			platform_window_cursor(platform, nullptr);
			current_cursor = CursorPrototype::ID{};
			return true;
		}
		const CursorPrototype& cursor = Database<CursorPrototype>::get(id);
		if (!cursor.handle) {
			log::Warning("{}", lf::format("[cursor] cursor prototype '{}' not loaded", Database<CursorPrototype>::name(id)));
			return false;
		}
		platform_window_cursor(platform, cursor.handle);
		current_cursor = id;
		return true;
	}
} // namespace lf
