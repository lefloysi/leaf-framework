#pragma once
#include <leaf/core/types.hpp>

namespace lf::procedural {
	struct ScratchStatistics {
		usize allocations = 0;
		usize retained_bytes = 0;
	};
	// Statistics and trimming apply to the calling thread; allocations count raw buffer growth.
	ScratchStatistics scratch_statistics();
	void trim_scratch();
	namespace detail {
		struct Scratch;
		Scratch* acquire();
		void release(Scratch* scratch) noexcept;
		void check_thread(const Scratch* scratch);
		void* reserve_vertices(Scratch* scratch, usize bytes, usize alignment);
		void* reserve_indices(Scratch* scratch, usize bytes, usize alignment);
		void reset_auxiliary(Scratch* scratch);
		void* auxiliary(Scratch* scratch, usize bytes, usize alignment);
	} // namespace detail
} // namespace lf::procedural
