#include "leaf/store/progression.hpp"

#include <steam/steam_api.h>

namespace lf::store {
	class SteamProgressionBackend final : public ProgressionBackend {
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
		void initialize();
		void shutdown();
		void stored(UserStatsStored_t* event);
		error check_ready() const;
		progression_commit_state state = progression_commit_state::idle;
		error failure;
		CCallbackManual<SteamProgressionBackend, UserStatsStored_t> stored_callback;
		bool initialized = false;
	};

	bool SteamProgressionBackend::supported() const {
		return initialized;
	}
	error SteamProgressionBackend::check_ready() const {
		if (!initialized || !SteamUserStats()) {
			return error(generic_errc::invalid_state, "Steam user statistics are unavailable.");
		}
		if (state == progression_commit_state::pending) {
			return error(generic_errc::invalid_state, "A Steam statistics upload is already pending.");
		}
		return error::no_error;
	}
	report<i32> SteamProgressionBackend::read_integer(string_view id) {
		if (auto err = check_ready(); err) {
			return unexpected(err);
		}
		i32 value = 0;
		if (!SteamUserStats()->GetStat(string(id).c_str(), &value)) {
			return unexpected(error(lf::format("Steam integer statistic '{}' is undefined or has a different type.", id)));
		}
		return value;
	}
	report<f32> SteamProgressionBackend::read_float(string_view id) {
		if (auto err = check_ready(); err) {
			return unexpected(err);
		}
		f32 value = 0;
		if (!SteamUserStats()->GetStat(string(id).c_str(), &value)) {
			return unexpected(error(lf::format("Steam floating-point statistic '{}' is undefined or has a different type.", id)));
		}
		return value;
	}
	report<bool> SteamProgressionBackend::read_achievement(string_view id) {
		if (auto err = check_ready(); err) {
			return unexpected(err);
		}
		bool value = false;
		if (!SteamUserStats()->GetAchievement(string(id).c_str(), &value)) {
			return unexpected(error(lf::format("Steam achievement '{}' is undefined.", id)));
		}
		return value;
	}
	error SteamProgressionBackend::set_integer(string_view id, i32 value) {
		IF_ERROR_RETURN_ERROR(check_ready());
		if (!SteamUserStats()->SetStat(string(id).c_str(), value)) {
			return error(lf::format("Steam rejected integer statistic '{}'.", id));
		}
		return error::no_error;
	}
	error SteamProgressionBackend::set_float(string_view id, f32 value) {
		IF_ERROR_RETURN_ERROR(check_ready());
		if (!SteamUserStats()->SetStat(string(id).c_str(), value)) {
			return error(lf::format("Steam rejected floating-point statistic '{}'.", id));
		}
		return error::no_error;
	}
	error SteamProgressionBackend::update_average(string_view id, f32 amount, f64 seconds) {
		IF_ERROR_RETURN_ERROR(check_ready());
		if (!SteamUserStats()->UpdateAvgRateStat(string(id).c_str(), amount, seconds)) {
			return error(lf::format("Steam rejected average-rate statistic '{}'.", id));
		}
		return error::no_error;
	}
	error SteamProgressionBackend::unlock(string_view id) {
		IF_ERROR_RETURN_ERROR(check_ready());
		if (!SteamUserStats()->SetAchievement(string(id).c_str())) {
			return error(lf::format("Steam rejected achievement '{}'.", id));
		}
		return error::no_error;
	}
	error SteamProgressionBackend::commit() {
		IF_ERROR_RETURN_ERROR(check_ready());
		if (failure.code == make_error_code(generic_errc::invalid_argument)) {
			// Steam reloads its cache after InvalidParam. Replaying this cache would silently lose data.
			return failure;
		}
		if (!SteamUserStats()->StoreStats()) {
			state = progression_commit_state::failed;
			failure = error("Steam could not start the statistics upload. Local changes remain pending.");
			return failure;
		}
		state = progression_commit_state::pending;
		failure = error::no_error;
		return error::no_error;
	}
	progression_commit_state SteamProgressionBackend::status() const {
		return state;
	}
	error SteamProgressionBackend::last_error() const {
		return failure;
	}
	void SteamProgressionBackend::stored(UserStatsStored_t* event) {
		if (state != progression_commit_state::pending || event->m_nGameID != CGameID(SteamUtils()->GetAppID()).ToUint64()) {
			return;
		}
		if (event->m_eResult == k_EResultOK) {
			state = progression_commit_state::succeeded;
			failure = error::no_error;
			return;
		}
		state = progression_commit_state::failed;
		failure = error(event->m_eResult == k_EResultInvalidParam ? generic_errc::invalid_argument : generic_errc::io_error,
			lf::format("Steam statistics upload failed (result {}). Local changes remain pending.", int(event->m_eResult)));
	}
	void SteamProgressionBackend::initialize() {
		stored_callback.Register(this, &SteamProgressionBackend::stored);
		state = progression_commit_state::idle;
		failure = error::no_error;
		initialized = true;
	}
	void SteamProgressionBackend::shutdown() {
		stored_callback.Unregister();
		initialized = false;
		state = progression_commit_state::idle;
	}
	ProgressionBackend& store_progression_backend() {
		static SteamProgressionBackend backend;
		return backend;
	}
	void initialize_store_progression() {
		static_cast<SteamProgressionBackend&>(store_progression_backend()).initialize();
	}
	void shutdown_store_progression() {
		static_cast<SteamProgressionBackend&>(store_progression_backend()).shutdown();
	}
} // namespace lf::store


