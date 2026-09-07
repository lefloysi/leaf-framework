#pragma once

#include <leaf/graphics/resource.hpp>

namespace rt::Swapchain {
	handle<swapchain> Create();
	void Resize(view<swapchain> swapchain, u32 width, u32 height);
	rt_swapchain_acquire_result Acquire(view<swapchain> swapchain);
	void Present(view<swapchain> swapchain, timepoint rendered);
} // namespace rt::Swapchain
