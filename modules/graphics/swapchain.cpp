#include "leaf/graphics/swapchain.hpp"

namespace rt::Swapchain {
	handle<swapchain> Create() {
		rt_swapchain swapchain = rtSwapchainCreate();
		detail::check_rutile_error("failed to create swapchain");
		return { swapchain };
	}

	void Resize(view<swapchain> swapchain, u32 width, u32 height) {
		rtSwapchainResize(swapchain, width, height);
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
