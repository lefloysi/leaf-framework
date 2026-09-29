#pragma once

#include <leaf/graphics/command_buffer.hpp>

#include <rt_antialias.h>

namespace rt {
	class antialias {
	  public:
		antialias();
		~antialias();

		antialias(const antialias&) = delete;
		antialias& operator=(const antialias&) = delete;

		void begin(view<command_buffer> commands, dim2<u32> extent);
		void resolve(view<command_buffer> commands, view<framebuffer> destination);

	  private:
		rt_antialias value = nullptr;
	};
} // namespace rt
