#include "leaf/graphics/swapchain.hpp"
#include "leaf/core/logging.hpp"

#include <chrono>

namespace rt::Swapchain {
	handle<swapchain> Create() {
		rt_swapchain swapchain = rtSwapchainCreate();
		detail::check_rutile_error("failed to create swapchain");
		return { swapchain };
	}

	void Resize(view<swapchain> swapchain, u32 width, u32 height) {
		auto start = std::chrono::steady_clock::now();
		rtSwapchainResize(swapchain, width, height);
		auto end = std::chrono::steady_clock::now();
		const auto elapsed = std::chrono::duration<double, std::milli>(end - start);
		lf::log::Info("[swapchain] resize {}x{}: {:.3f} ms", width, height, elapsed.count());
		detail::check_rutile_error("failed to resize swapchain");
	}

	rt_swapchain_acquire_result Acquire(view<swapchain> swapchain) {
		rt_swapchain_acquire_result result = rtSwapchainAcquire(swapchain);
		detail::check_rutile_error("failed to acquire swapchain framebuffer");
		return result;
	}

	void Present(view<swapchain> swapchain, timepoint rendered) {
		rtSwapchainPresent(swapchain, rendered);
		detail::check_rutile_error("failed to present swapchain");
	}
} // namespace rt::Swapchain
