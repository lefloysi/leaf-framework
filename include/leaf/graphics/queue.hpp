#ifndef LEAF_GRAPHICS_QUEUE_HPP
#define LEAF_GRAPHICS_QUEUE_HPP

#include <leaf/graphics/resource.hpp>

namespace rt {
	namespace Queue {
		handle<queue> Create(queue_capability capability);
		void Wait(view<queue> queue, timepoint timepoint);
		timepoint Submit(view<queue> queue, view<command_buffer> command_buffer);
		timepoint Flush(view<queue> queue);
	} // namespace Queue
} // namespace rt

#endif /* LEAF_GRAPHICS_QUEUE_HPP */
