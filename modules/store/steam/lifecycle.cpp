#include "leaf/store/lifecycle.hpp"

#include <leaf/core/format.hpp>
#include <leaf/core/logging.hpp>
#include <leaf/store/progression.hpp>

#include <steam/steam_api.h>

namespace lf {
	bool restart_store_app(u32 app_id) {
		return SteamAPI_RestartAppIfNecessary(app_id);
	}

	void steam_warning_message_hook(int severity, const char* message) {
		if (severity > 0) {
			log::Warning("{}", lf::format("steam[{}]: {}", severity, message));
		} else {
			log::Debug("{}", lf::format("steam[{}]: {}", severity, message));
		}
	}

	error init_store(span<string_view>) {
		SteamErrMsg message{};
		if (SteamAPI_InitEx(&message) != k_ESteamAPIInitResult_OK) {
			return error(lf::format("Failed to initialize Steamworks: {}", message));
		}

		SteamUtils()->SetWarningMessageHook(steam_warning_message_hook);
		store::initialize_store_progression();
		return error::no_error;
	}

	string_view store_backend_name() {
		return "steam";
	}

	void update_store() {
		SteamAPI_RunCallbacks();
	}

	void exit_store() {
		store::shutdown_store_progression();
		SteamAPI_Shutdown();
	}
} // namespace lf
