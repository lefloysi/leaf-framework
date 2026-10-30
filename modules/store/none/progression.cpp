#include "leaf/store/progression.hpp"

namespace lf::store {
	class StandaloneProgressionBackend final : public ProgressionBackend {
	public:
		bool supported() const override;
		report<i32> read_integer(string_view id) override;
		report<f32> read_float(string_view id) override;
		report<bool> read_achievement(string_view id) override;
		error set_integer(string_view id, i32 value) override;
		error set_float(string_view id, f32 value) override;
		error update_average(string_view id, f32 amount, f64 seconds) override;
		error unlock(string_view id) override;
		error commit() override;
		progression_commit_state status() const override;
		error last_error() const override;
	};

	bool StandaloneProgressionBackend::supported() const {
		return false;
	}
	report<i32> StandaloneProgressionBackend::read_integer(string_view) {
		return unexpected(last_error());
	}
	report<f32> StandaloneProgressionBackend::read_float(string_view) {
		return unexpected(last_error());
	}
	report<bool> StandaloneProgressionBackend::read_achievement(string_view) {
		return unexpected(last_error());
	}
	error StandaloneProgressionBackend::set_integer(string_view, i32) {
		return last_error();
	}
	error StandaloneProgressionBackend::set_float(string_view, f32) {
		return last_error();
	}
	error StandaloneProgressionBackend::update_average(string_view, f32, f64) {
		return last_error();
	}
	error StandaloneProgressionBackend::unlock(string_view) {
		return last_error();
	}
	error StandaloneProgressionBackend::commit() {
		return last_error();
	}
	progression_commit_state StandaloneProgressionBackend::status() const {
		return progression_commit_state::idle;
	}
	error StandaloneProgressionBackend::last_error() const {
		return error(generic_errc::invalid_state, "No storefront is configured. Local progression is saved independently.");
	}

	ProgressionBackend& store_progression_backend() {
		static StandaloneProgressionBackend backend;
		return backend;
	}
	void initialize_store_progression() {
	}
	void shutdown_store_progression() {
	}
} // namespace lf::store

