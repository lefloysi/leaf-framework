#include "leaf/core/profiler.hpp"

#include "leaf/core/filesystem.hpp"
#include "leaf/core/format.hpp"
#include "leaf/core/logging.hpp"
#include "leaf/core/unordered_map.hpp"
#include "leaf/core/vector.hpp"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <mutex>

namespace lf {
	struct ProfileBucket {
			u64 calls = 0;
			f64 total_ms = 0.0;
			f64 max_ms = 0.0;
	};

	static std::atomic<bool> profiler_enabled = false;
	static std::mutex profiler_mutex;
	static unordered_map<string, ProfileBucket> profiler_buckets;

	void SetProfilerEnabled(bool enabled) {
		profiler_enabled.store(enabled, std::memory_order_relaxed);
	}

	bool ProfilerEnabled() {
		return profiler_enabled.load(std::memory_order_relaxed);
	}

	void ResetProfiler() {
		std::lock_guard lock(profiler_mutex);
		profiler_buckets.clear();
	}

	void RecordProfileSample(string_view name, f64 milliseconds) {
		if (!ProfilerEnabled()) {
			return;
		}

		std::lock_guard lock(profiler_mutex);
		ProfileBucket& bucket = profiler_buckets[string(name)];
		++bucket.calls;
		bucket.total_ms += milliseconds;
		bucket.max_ms = std::max(bucket.max_ms, milliseconds);
	}

	vector<ProfileSnapshotEntry> ProfileSnapshot() {
		vector<ProfileSnapshotEntry> snapshot;
		{
			std::lock_guard lock(profiler_mutex);
			snapshot.reserve(profiler_buckets.size());
			for (const auto& [name, bucket] : profiler_buckets) {
				snapshot.push_back({
					.name = name,
					.calls = bucket.calls,
					.total_ms = bucket.total_ms,
					.max_ms = bucket.max_ms,
				});
			}
		}

		std::sort(snapshot.begin(), snapshot.end(), [](const ProfileSnapshotEntry& lhs, const ProfileSnapshotEntry& rhs) {
			return lhs.total_ms > rhs.total_ms;
		});
		return snapshot;
	}

	void LogProfileSnapshot(string_view label) {
		vector<ProfileSnapshotEntry> snapshot = ProfileSnapshot();
		auto write_line = [&](string_view message) {
			log::Debug("{}", message);
		};

		write_line(lf::format("[{}] {} profile buckets", label, snapshot.size()));
		for (const ProfileSnapshotEntry& entry : snapshot) {
			const f64 average_ms = entry.calls == 0 ? 0.0 : entry.total_ms / static_cast<f64>(entry.calls);
			write_line(lf::format(
				"[{}] {} calls={} total={:.3f}ms avg={:.3f}ms max={:.3f}ms",
				label,
				entry.name,
				entry.calls,
				entry.total_ms,
				average_ms,
				entry.max_ms
			));
		}
	}

	ProfileScope::ProfileScope(string_view name) : name(name),
												   start(std::chrono::steady_clock::now()),
												   enabled(ProfilerEnabled()) {}

	ProfileScope::~ProfileScope() {
		if (!enabled) {
			return;
		}

		auto end = std::chrono::steady_clock::now();
		RecordProfileSample(name, std::chrono::duration<f64, std::milli>(end - start).count());
	}
} // namespace lf
