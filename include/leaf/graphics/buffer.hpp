#ifndef LEAF_GRAPHICS_BUFFER_HPP
#define LEAF_GRAPHICS_BUFFER_HPP

#include <leaf/graphics/resource.hpp>

namespace rt {
	namespace Buffer {
		handle<buffer> Create();
		void Destroy(handle<buffer> buffer);
		void Resize(view<buffer> buffer, memory_type memory_type, u64 size);
		void Read(view<buffer> buffer, rt_buffer_range range, u08* data, u64 data_size);
	} // namespace Buffer
} // namespace rt

#endif /* LEAF_GRAPHICS_BUFFER_HPP */
