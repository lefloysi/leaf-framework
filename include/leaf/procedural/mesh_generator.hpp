#pragma once
#include <cstdint>
#include <cstring>
#include <leaf/core/exception.hpp>
#include <leaf/core/span.hpp>
#include <leaf/procedural/scratch.hpp>
#include <limits>
#include <type_traits>

namespace lf {
	enum class Primitive { points,
						   lines,
						   triangles };

	template<typename Vertex, typename Index = u32>
	struct MeshView {
		span<const Vertex> vertices;
		span<const Index> indices;
		Primitive topology = Primitive::triangles;
	};
	template<typename Vertex>
	struct MeshView<Vertex, void> {
		span<const Vertex> vertices;
		Primitive topology = Primitive::triangles;
	};
	namespace procedural::detail {
		template<typename Index>
		struct IndexCursor {
			Index* index_cursor = nullptr;
		};
		template<>
		struct IndexCursor<void> {};

		template<typename T>
		usize byte_count(usize count);
		template<typename T>
		usize written(T* begin, T* cursor, usize capacity);
	} // namespace procedural::detail

	template<typename Vertex, typename Index = u32>
	class MeshGenerator : public procedural::detail::IndexCursor<Index> {
		static_assert(std::is_trivially_copyable_v<Vertex>);
		static_assert(std::is_void_v<Index> || std::is_same_v<Index, u08> || std::is_same_v<Index, u16> || std::is_same_v<Index, u32> || std::is_same_v<Index, u64>);

	  public:
		MeshGenerator();
		~MeshGenerator();
		MeshGenerator(const MeshGenerator&) = delete;
		MeshGenerator& operator=(const MeshGenerator&) = delete;
		MeshGenerator(MeshGenerator&&) = delete;
		MeshGenerator& operator=(MeshGenerator&&) = delete;

		// Starts a replacement batch. Invalidates the previous published spans.
		void reserve(usize vertices, usize indices = 0, Primitive topology = Primitive::triangles);
		void publish();
		template<typename T>
		span<T> scratch(usize count);

		Vertex current{};
		Vertex* vertex_cursor = nullptr;
		MeshView<Vertex, Index> mesh;

	  private:
		procedural::detail::Scratch* storage;
		Vertex* vertex_begin = nullptr;
		void* index_begin = nullptr;
		usize vertex_capacity = 0;
		usize index_capacity = 0;
		bool writing = false;
	};

	namespace procedural::detail {
		template<typename T>
		usize byte_count(usize count) {
			if (count > std::numeric_limits<usize>::max() / sizeof(T)) { throw runtime_exception{ "procedural allocation size overflow" }; }
			return count * sizeof(T);
		}
		template<typename T>
		usize written(T* begin, T* cursor, usize capacity) {
			const auto first = reinterpret_cast<std::uintptr_t>(begin);
			const auto last = reinterpret_cast<std::uintptr_t>(cursor);
			if (last < first || last - first > byte_count<T>(capacity) || (last - first) % sizeof(T)) {
				throw runtime_exception{ "procedural cursor outside reserved storage" };
			}
			return (last - first) / sizeof(T);
		}
	} // namespace procedural::detail

	template<typename Vertex, typename Index>
	MeshGenerator<Vertex, Index>::MeshGenerator() : storage{ procedural::detail::acquire() } {}
	template<typename Vertex, typename Index>
	MeshGenerator<Vertex, Index>::~MeshGenerator() { procedural::detail::release(storage); }

	template<typename Vertex, typename Index>
	void MeshGenerator<Vertex, Index>::reserve(usize vertices, usize indices, Primitive topology) {
		procedural::detail::check_thread(storage);
		if (topology != Primitive::points && topology != Primitive::lines && topology != Primitive::triangles) { throw runtime_exception{ "invalid procedural topology" }; }
		const usize vertex_bytes = procedural::detail::byte_count<Vertex>(vertices);
		usize index_bytes = 0;
		if constexpr (!std::is_void_v<Index>) {
			if (vertices && vertices - 1 > std::numeric_limits<Index>::max()) { throw runtime_exception{ "vertex reservation exceeds index width" }; }
			index_bytes = procedural::detail::byte_count<Index>(indices);
		} else {
			if (indices) { throw runtime_exception{ "non-indexed mesh cannot reserve indices" }; }
		}
		mesh = {};
		writing = false;
		vertex_cursor = nullptr;
		vertex_begin = nullptr;
		index_begin = nullptr;
		if constexpr (!std::is_void_v<Index>) { this->index_cursor = nullptr; }
		vertex_begin = static_cast<Vertex*>(procedural::detail::reserve_vertices(storage, vertex_bytes, alignof(Vertex)));
		if constexpr (!std::is_void_v<Index>) {
			index_begin = procedural::detail::reserve_indices(storage, index_bytes, alignof(Index));
			this->index_cursor = static_cast<Index*>(index_begin);
		}
		vertex_cursor = vertex_begin;
		vertex_capacity = vertices;
		index_capacity = indices;
		mesh.topology = topology;
		procedural::detail::reset_auxiliary(storage);
		writing = true;
	}
	template<typename Vertex, typename Index>
	void MeshGenerator<Vertex, Index>::publish() {
		procedural::detail::check_thread(storage);
		if (!writing) { throw runtime_exception{ "no procedural batch is being written" }; }
		const usize vertices = procedural::detail::written(vertex_begin, vertex_cursor, vertex_capacity);
		usize elements = vertices;
		if constexpr (!std::is_void_v<Index>) {
			elements = procedural::detail::written(static_cast<Index*>(index_begin), this->index_cursor, index_capacity);
			if (elements && !vertices) { throw runtime_exception{ "indices require vertices" }; }
		}
		const usize arity = mesh.topology == Primitive::triangles ? 3 : mesh.topology == Primitive::lines ? 2
																										  : 1;
		if (elements % arity) { throw runtime_exception{ "incomplete procedural primitive" }; }
		mesh.vertices = { vertex_begin, vertices };
		if constexpr (!std::is_void_v<Index>) { mesh.indices = { static_cast<const Index*>(index_begin), elements }; }
		writing = false;
	}
	template<typename Vertex, typename Index>
	template<typename T>
	span<T> MeshGenerator<Vertex, Index>::scratch(usize count) {
		static_assert(std::is_trivially_copyable_v<T>);
		procedural::detail::check_thread(storage);
		if (!writing) { throw runtime_exception{ "scratch allocation requires a reserved batch" }; }
		return { static_cast<T*>(procedural::detail::auxiliary(storage, procedural::detail::byte_count<T>(count), alignof(T))), count };
	}
} // namespace lf
