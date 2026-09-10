#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <leaf/core/vector.hpp>
#include <leaf/procedural/mesh_generator.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#ifdef _MSC_VER
#include <malloc.h>
#endif

namespace {
	thread_local bool count_allocations = false;
	thread_local std::size_t allocations = 0;
	std::atomic<std::uint64_t> consumed = 0;
	std::mutex output_mutex;
} // namespace

void* operator new(std::size_t bytes) {
	if (count_allocations) { ++allocations; }
	if (void* p = std::malloc(bytes ? bytes : 1)) { return p; }
	throw std::bad_alloc{};
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
void* operator new(std::size_t bytes, std::align_val_t alignment) {
	if (count_allocations) { ++allocations; }
	const auto a = static_cast<std::size_t>(alignment);
#ifdef _MSC_VER
	void* p = _aligned_malloc(bytes ? bytes : 1, a);
#else
	void* p = std::aligned_alloc(a, ((bytes ? bytes : 1) + a - 1) / a * a);
#endif
	if (p) { return p; }
	throw std::bad_alloc{};
}
void* operator new[](std::size_t n, std::align_val_t a) { return ::operator new(n, a); }
void operator delete(void* p, std::align_val_t) noexcept {
#ifdef _MSC_VER
	_aligned_free(p);
#else
	std::free(p);
#endif
}
void operator delete[](void* p, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete(void* p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete[](void* p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }

namespace {
	using Clock = std::chrono::steady_clock;
	template<std::size_t Words>
	struct Vertex {
		std::uint32_t words[Words];
	};
	struct Raw {
		void* data = nullptr;
		std::size_t capacity = 0;
		~Raw();
		void reserve(std::size_t bytes);
	};
	Raw::~Raw() { ::operator delete(data, std::align_val_t{ 64 }); }
	void Raw::reserve(std::size_t bytes) {
		if (bytes > capacity) {
			void* next = ::operator new(bytes, std::align_val_t{ 64 });
			::operator delete(data, std::align_val_t{ 64 });
			data = next;
			capacity = bytes;
		}
	}

	template<typename V, typename I, int path>
	void measure(std::size_t vertices, bool cold, unsigned worker_count) {
		lf::procedural::trim_scratch();
		using Index = std::conditional_t<std::is_void_v<I>, std::uint32_t, I>;
		constexpr bool indexed = !std::is_void_v<I>;
		const std::size_t indices = indexed ? vertices * 6 : 0;
		lf::vector<V> vertex_vector;
		lf::vector<Index> index_vector;
		Raw raw_vertices;
		Raw raw_indices;
		std::optional<lf::MeshGenerator<V, I>> generator;
		double seconds = 0;
		std::size_t total_allocations = 0;
		std::size_t retained = 0;
		const unsigned rounds = vertices <= 240 ? 512 : 8;
		for (unsigned round = 0; round <= rounds; ++round) {
			generator.reset();
			if (cold || path == 0) {
				lf::vector<V>{}.swap(vertex_vector);
				lf::vector<Index>{}.swap(index_vector);
			}
			if (cold) {
				lf::procedural::trim_scratch();
				::operator delete(raw_vertices.data, std::align_val_t{ 64 });
				raw_vertices.data = nullptr;
				raw_vertices.capacity = 0;
				::operator delete(raw_indices.data, std::align_val_t{ 64 });
				raw_indices.data = nullptr;
				raw_indices.capacity = 0;
			}
			allocations = 0;
			count_allocations = true;
			const auto start = Clock::now();
			V* vp = nullptr;
			Index* ip = nullptr;
			if constexpr (path <= 1) {
				vertex_vector.clear();
				index_vector.clear();
				if constexpr (path == 1) {
					vertex_vector.reserve(vertices);
					index_vector.reserve(indices);
				}
			} else if constexpr (path == 2) {
				raw_vertices.reserve(vertices * sizeof(V));
				raw_indices.reserve(indices * sizeof(Index));
				vp = static_cast<V*>(raw_vertices.data);
				ip = static_cast<Index*>(raw_indices.data);
			} else {
				generator.emplace();
				generator->reserve(vertices, indices);
				vp = generator->vertex_cursor;
				if constexpr (indexed) { ip = generator->index_cursor; }
			}
			V local{};
			V& current = path == 3 ? generator->current : local;
			for (std::size_t i = 0; i < vertices; ++i) {
				for (std::size_t w = 0; w < sizeof(V) / sizeof(std::uint32_t); ++w) {
					current.words[w] = static_cast<std::uint32_t>(i * 17 + w);
				}
				if constexpr (path <= 1) {
					vertex_vector.push_back(current);
				} else {
					std::memcpy(vp++, &current, sizeof(V));
				}
			}
			if constexpr (indexed) {
				for (std::size_t i = 0; i < indices; ++i) {
					const auto index = static_cast<Index>(i % vertices);
					if constexpr (path <= 1) {
						index_vector.push_back(index);
					} else {
						*ip++ = index;
					}
				}
			}
			if constexpr (path == 3) {
				generator->vertex_cursor = vp;
				if constexpr (indexed) { generator->index_cursor = ip; }
				generator->publish();
			}
			const auto end = Clock::now();
			count_allocations = false;
			if (round) {
				seconds += std::chrono::duration<double>{ end - start }.count();
				total_allocations += allocations;
			}
			const V* output = path <= 1 ? vertex_vector.data() : path == 2 ? static_cast<V*>(raw_vertices.data)
																		   : generator->mesh.vertices.data();
			const Index* index_output = path <= 1 ? index_vector.data() : static_cast<Index*>(raw_indices.data);
			if constexpr (indexed) {
				if constexpr (path == 3) { index_output = generator->mesh.indices.data(); }
			}
			std::uint64_t checksum = 0;
			for (std::size_t i = 0; i < vertices; ++i) {
				for (std::size_t w = 0; w < sizeof(V) / sizeof(std::uint32_t); ++w) {
					if (output[i].words[w] != static_cast<std::uint32_t>(i * 17 + w)) { throw std::runtime_error{ "vertex mismatch" }; }
					checksum += output[i].words[w];
				}
			}
			for (std::size_t i = 0; i < indices; ++i) {
				if (index_output[i] != static_cast<Index>(i % vertices)) { throw std::runtime_error{ "index mismatch" }; }
				checksum += index_output[i];
			}
			consumed.fetch_add(checksum, std::memory_order_relaxed);
			retained = path <= 1   ? vertex_vector.capacity() * sizeof(V) + index_vector.capacity() * sizeof(Index)
					   : path == 2 ? raw_vertices.capacity + raw_indices.capacity
								   : lf::procedural::scratch_statistics().retained_bytes;
		}
		if (!cold && path == 3 && total_allocations) { throw std::runtime_error{ "generator allocated after warm-up" }; }
		const char* names[] = { "vector", "reserved-vector", "raw", "generator" };
		std::lock_guard lock{ output_mutex };
		std::printf("%u,%zu,%zu,%zu,%s,%s,%.0f,%.0f,%.0f,%u,%zu,%zu\n", worker_count, sizeof(V), indexed ? sizeof(Index) : 0, vertices, names[path], cold ? "cold" : "warm", rounds * vertices / seconds, rounds * (indexed ? indices : vertices) / (3 * seconds), rounds * (vertices * sizeof(V) + indices * sizeof(Index)) / seconds, rounds, total_allocations, retained);
	}

	template<typename V, typename I>
	void scenario(std::size_t count, unsigned threads = 1) {
		for (bool cold : { true, false }) {
			measure<V, I, 0>(count, cold, threads);
			measure<V, I, 1>(count, cold, threads);
			measure<V, I, 2>(count, cold, threads);
			measure<V, I, 3>(count, cold, threads);
		}
	}
	template<std::size_t Words>
	void sizes() {
		scenario<Vertex<Words>, u08>(240);
		scenario<Vertex<Words>, u16>(65520);
		scenario<Vertex<Words>, u32>(262140);
		scenario<Vertex<Words>, u64>(262140);
		scenario<Vertex<Words>, void>(262140);
	}
} // namespace
int main() {
	try {
		std::puts("workers,vertex_bytes,index_bytes,vertices,path,allocation,vertices/s,triangles/s,bytes/s,batches,allocations,retained_bytes");
		sizes<4>();
		sizes<10>();
		sizes<16>();
		std::jthread a{ [] { scenario<Vertex<4>, u32>(65520, 2); } };
		std::jthread b{ [] { scenario<Vertex<4>, u32>(65520, 2); } };
		a.join();
		b.join();
		std::fprintf(stderr, "Verified checksum: %llu\n", static_cast<unsigned long long>(consumed.load()));
	} catch (const std::exception& error) {
		std::fprintf(stderr, "%s\n", error.what());
		return 1;
	}
}
