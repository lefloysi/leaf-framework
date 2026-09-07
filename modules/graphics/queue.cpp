#include "leaf/graphics/queue.hpp"

namespace rt {
	handle<queue> Queue::Create(queue_capability capability) {
		rt_queue queue = rtQueueCreate(static_cast<rt_queue_capability>(capability));
		detail::check_rutile_error("failed to create queue");
		return { queue };
	}

	void Queue::Wait(view<queue> queue, timepoint timepoint) {
		rtQueueWait(queue, timepoint);
		detail::check_rutile_error("failed to wait for queue");
	}

	timepoint Queue::Submit(view<queue> queue, view<command_buffer> command_buffer) {
		rt::timepoint timepoint = rtQueueSubmit(queue, command_buffer);
		detail::check_rutile_error("failed to submit command buffer");
		return timepoint;
	}

	timepoint Queue::Flush(view<queue> queue) {
		rt::timepoint timepoint = rtQueueFlush(queue);
		detail::check_rutile_error("failed to flush queue");
		return timepoint;
	}
} // namespace rt
