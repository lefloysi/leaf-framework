#include "leaf/graphics/buffer.hpp"

namespace rt {
	handle<buffer> Buffer::Create() {
		rt_buffer buffer = rtBufferCreate();
		detail::check_rutile_error("failed to create buffer");
		return { buffer };
	}

	void Buffer::Destroy(handle<buffer> buffer) {
		rtBufferDestroy(buffer);
	}

	void Buffer::Resize(view<buffer> buffer, memory_type memory_type, u64 size) {
		rtBufferResize(buffer, static_cast<rt_memory_type>(memory_type), size);
		detail::check_rutile_error("failed to resize buffer");
	}

	void Buffer::Read(view<buffer> buffer, rt_buffer_range range, u08* data, u64 data_size) {
		rtBufferRead(buffer, range, data, data_size);
		detail::check_rutile_error("failed to read buffer");
	}
} // namespace rt
