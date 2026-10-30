#pragma once

#include <leaf/core/error.hpp>
#include <leaf/core/vector.hpp>

namespace lf {
	class Progression;
}

namespace lf::store {
	enum class progression_commit_state {
		idle,
		pending,
		succeeded,
		failed,
	};

	// Values are staged in the store cache. Only a successful commit acknowledges persistence.
	class ProgressionBackend {
	public:
		virtual ~ProgressionBackend();
		virtual bool supported() const = 0;
		virtual report<i32> read_integer(string_view id) = 0;
		virtual report<f32> read_float(string_view id) = 0;
		virtual report<bool> read_achievement(string_view id) = 0;
		virtual error set_integer(string_view id, i32 value) = 0;
		virtual error set_float(string_view id, f32 value) = 0;
		virtual error update_average(string_view id, f32 amount, f64 elapsed_seconds) = 0;
		virtual error unlock(string_view id) = 0;
		virtual error commit() = 0;
		virtual progression_commit_state status() const = 0;
		virtual error last_error() const = 0;
	};

	ProgressionBackend& store_progression_backend();
	void initialize_store_progression();
	void shutdown_store_progression();

	struct ProgressionWrite {
		string id;
		u64 revision = 0;
		usize rate_samples = 0;
	};

	class StoreProgression {
	public:
		StoreProgression();
		explicit StoreProgression(ProgressionBackend& backend);
		error synchronize(Progression& progression);
		bool pending() const noexcept;
	private:
		ProgressionBackend* backend_;
		Progression* owner_ = nullptr;
		vector<string> imported_stats_;
		vector<string> imported_achievements_;
		vector<ProgressionWrite> stat_writes_;
		vector<ProgressionWrite> achievement_writes_;
		bool submitted_ = false;
	};
} // namespace lf::store
