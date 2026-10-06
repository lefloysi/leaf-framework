#include <algorithm>
#include <leaf/core/exception.hpp>
#include <leaf/procedural/scratch.hpp>
#include <memory>
#include <new>
#include <vector>

namespace lf::procedural {
	namespace detail {
		struct Buffer {
			void* data = nullptr;
			usize capacity = 0;
			usize alignment = alignof(std::max_align_t);
			Buffer() = default;
			~Buffer();
			Buffer(const Buffer&) = delete;
			Buffer& operator=(const Buffer&) = delete;
			void* reserve(usize bytes, usize requested_alignment);
		};
		struct Scratch {
			const void* owner;
			bool leased = false;
			Buffer vertices;
			Buffer indices;
			std::vector<std::unique_ptr<Buffer>> auxiliary;
			usize next_auxiliary = 0;
		};
		struct Pool {
			ScratchStatistics statistics;
			std::vector<std::unique_ptr<Scratch>> slots;
		};
		thread_local Pool pool;

		Buffer::~Buffer() {
			if (data) {
				::operator delete(data, std::align_val_t{ alignment });
				pool.statistics.retained_bytes -= capacity;
			}
		}
		void* Buffer::reserve(usize bytes, usize requested_alignment) {
			if (!bytes) { return nullptr; }
			if (bytes <= capacity && requested_alignment <= alignment) { return data; }
			usize next_capacity = bytes;
			if (capacity <= static_cast<usize>(-1) / 2) { next_capacity = std::max(bytes, capacity * 2); }
			const usize next_alignment = std::max(requested_alignment, alignof(std::max_align_t));
			void* next = ::operator new(next_capacity, std::align_val_t{ next_alignment });
			if (data) { ::operator delete(data, std::align_val_t{ alignment }); }
			pool.statistics.retained_bytes += next_capacity - capacity;
			++pool.statistics.allocations;
			data = next;
			capacity = next_capacity;
			alignment = next_alignment;
			return data;
		}
		Scratch* acquire() {
			for (auto& slot : pool.slots) {
				if (!slot->leased) {
					slot->leased = true;
					return slot.get();
				}
			}
			auto slot = std::make_unique<Scratch>();
			slot->owner = &pool;
			slot->leased = true;
			Scratch* result = slot.get();
			pool.slots.push_back(std::move(slot));
			return result;
		}
		void check_thread(const Scratch* scratch) {
			if (scratch->owner != &pool) { throw runtime_exception{ "procedural scratch belongs to another thread" }; }
		}
		void release(Scratch* scratch) noexcept {
			if (scratch->owner != &pool) { std::terminate(); }
			scratch->leased = false;
		}
		void* reserve_vertices(Scratch* scratch, usize bytes, usize alignment) {
			check_thread(scratch);
			return scratch->vertices.reserve(bytes, alignment);
		}
		void* reserve_indices(Scratch* scratch, usize bytes, usize alignment) {
			check_thread(scratch);
			return scratch->indices.reserve(bytes, alignment);
		}
		void reset_auxiliary(Scratch* scratch) {
			check_thread(scratch);
			scratch->next_auxiliary = 0;
		}
		void* auxiliary(Scratch* scratch, usize bytes, usize alignment) {
			check_thread(scratch);
			if (!bytes) { return nullptr; }
			if (scratch->next_auxiliary == scratch->auxiliary.size()) {
				scratch->auxiliary.push_back(std::make_unique<Buffer>());
			}
			Buffer& buffer = *scratch->auxiliary[scratch->next_auxiliary];
			void* result = buffer.reserve(bytes, alignment);
			++scratch->next_auxiliary;
			return result;
		}
	} // namespace detail
	ScratchStatistics scratch_statistics() { return detail::pool.statistics; }
	void trim_scratch() {
		std::erase_if(detail::pool.slots, [](const auto& slot) { return !slot->leased; });
	}
} // namespace lf::procedural
