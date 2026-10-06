# leaf-procedural

CPU-only mesh generation, linked with `leaf::procedural`. Public entry point:
`<leaf/procedural/mesh_generator.hpp>`. The module links only `leaf::core`;
it does not upload meshes or own render resources.

```cpp
struct Vertex {
    float x, y;
};

class TriangleGenerator : public lf::MeshGenerator<Vertex> {
public:
    void generate();
};

void TriangleGenerator::generate() {
    reserve(3, 3);
    current = {0, 0};
    std::memcpy(vertex_cursor++, &current, sizeof(current));
    current = {1, 0};
    std::memcpy(vertex_cursor++, &current, sizeof(current));
    current = {0, 1};
    std::memcpy(vertex_cursor++, &current, sizeof(current));
    *index_cursor++ = 0;
    *index_cursor++ = 1;
    *index_cursor++ = 2;
    publish();
}
```

Derived generators write batches directly. `current` is one ordinary vertex
object; emitted vertices occupy aligned raw storage and are copied with `memcpy`.
There are no primitive assembly calls. `Index` supports `u08`, `u16`, `u32`,
`u64`, or `void` for a non-indexed stream. Vertex types must be trivially copyable
and support initializing the current state. Default topology is triangle lists;
`lf::Primitive::points` and `lines` are also available.

`reserve(vertex_capacity, index_capacity, topology)` starts a replacement batch.
It checks byte-size overflow and whether the vertex bound fits the index type.
Reserve once, then write through the cursors without allocation or bounds checks.
`publish()` checks the final cursors and primitive count, exposing read-only
`mesh.vertices` and (for indexed output) `mesh.indices` spans. Fewer elements than
reserved may be published. Index values must reference vertices actually written;
publication does not scan indices. Cursor validation cannot undo an out-of-bounds
write: the writer must stay inside the reserved region.

`scratch<T>(count)` reserves an additional aligned auxiliary span during a batch.
Call it during reservation, before the write loop. Each request receives a
separate reusable buffer; another auxiliary request does not invalidate it.
Auxiliary storage, like vertex storage, is uninitialized: use appropriate raw
writes such as `memcpy`. Allocate the same request sequence on subsequent batches
to reuse its capacities.

Each generator has its own lease from a thread-local pool, including nested
generators and generators retaining completed output. Destruction returns the
lease without releasing capacity. Spans expire at the next reservation or
generator destruction. Generators cannot be copied or moved, and both generators
and their borrowed spans must remain on their originating thread. Method calls
on another thread throw; destruction on another thread terminates. Direct span
and cursor access cannot enforce the thread contract. Do not use generators from
thread-local destructors after the scratch pool has been destroyed.

For persistent output, explicitly copy the published spans into owned vectors
before reserving again or destroying the generator. Owned copies can then be
transferred to another thread. `lf::procedural::trim_scratch()` frees idle leases
on the calling thread without disturbing live generators.

`scratch_statistics()` reports raw-buffer allocation count and retained raw
bytes for the calling thread. Pool bookkeeping allocations are excluded from
these diagnostics; the benchmark independently counts C++ allocations including
bookkeeping.

## Validation

Build `leaf-procedural-tests`, then run CTest with the filter
`leaf::procedural::`. Tests cover copied state, alignment, all index widths,
partial publication, invalid bounds, overflow, growth, auxiliary storage, nested
leases, explicit copies and thread isolation.

Enable `LEAF_BUILD_BENCHMARKS=ON` to build `leaf-procedural-benchmark`.
It has no benchmark dependency. Run a Release build and capture stdout as CSV.
The executable verifies every output component outside the timed region and
consumes a checksum. A warmed generator allocation makes the run fail.

The four paths produce identical vertices and indices. Indexed batches reuse
vertices six times; non-indexed batches produce triangle lists. Paths are
compile-time specializations, so no path switch occurs inside the write loop.
Timing includes reservation, state calculation, writes and publication.
Deallocation and verification are excluded. The first sample is discarded;
small batches use 512 measured samples and large batches use eight.

Cold samples release payload storage between batches; warm samples retain it.
The unreserved-vector baseline deliberately starts with an empty allocation
each time. Generator cold samples also discard idle leases, though the pool's
slot-vector capacity can survive trimming. Thus cold measures allocation, not
fresh-process startup. Allocation counts cover replaceable C++ `new` calls on
the measured thread, including aligned allocations. Retained bytes cover
payload capacity, not allocator or pool bookkeeping overhead.

Rows with `workers=2` report each concurrent worker's throughput independently,
not aggregate throughput. Compare repeated runs on the same machine; these
measurements are baselines, not a universal speed guarantee.

The initial Windows x64 MSVC 19.44 Release (`/O2 /Ob2 /DNDEBUG`) run is saved in
`../../benchmarks/baselines/procedural-windows-msvc.csv`. Its 15 single-thread
warmed generator cases measured 92–110% of the matching raw-buffer throughput,
with zero warmed allocations, also in the two-worker cases. No Outposts renderer
path has been migrated; rendering changes remain separate from this library.
