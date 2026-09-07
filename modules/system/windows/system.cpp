#include "leaf/system/system.hpp"
#include "leaf/system/socket.hpp"
#include "leaf/core/logging.hpp"
#include <Shlobj.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <windows.h>

namespace lf {
	struct SystemData {
		fs::native_path appdata_dir;
		fs::native_path install_dir;
		fs::native_path current_dir;
	};

	struct ShutdownHandlerEntry {
		ShutdownHandlerId id = 0;
		ShutdownHandler handler = nullptr;
		void* user_data = nullptr;
	};

	static SystemData system_data;
	static std::vector<ShutdownHandlerEntry> shutdown_handlers;
	static ShutdownHandlerId next_shutdown_handler_id = 1;

	void RequestShutdown() {
		for (const ShutdownHandlerEntry& entry : shutdown_handlers) {
			if (entry.handler) {
				entry.handler(entry.user_data);
			}
		}
	}

	void ShowErrorBox(string_view title, string_view message) {
		if (const char* suppress_dialogs = std::getenv("LEAF_NO_ERROR_DIALOGS"); suppress_dialogs && suppress_dialogs[0]) {
			std::fprintf(stderr, "%.*s: %.*s\n", static_cast<int>(title.size()), title.data(), static_cast<int>(message.size()), message.data());
			std::fflush(stderr);
			return;
		}
		const string title_string(title);
		const string message_string(message);
		MessageBoxA(nullptr, message_string.c_str(), title_string.c_str(), MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
	}

	static BOOL WINAPI console_shutdown_handler(DWORD control_type) {
		switch (control_type) {
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			RequestShutdown();
			return TRUE;
		default:
			return FALSE;
		}
	}

	error init_system(span<string_view> args) {
		install_crash_handler();
		log::Logger::instance().add_sink(make_unique<log::ConsoleSink>());
		wchar_t appdata_dir[MAX_PATH]{};
		if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata_dir))) {
			system_data.appdata_dir = appdata_dir;
		} else {
			system_data.appdata_dir.clear();
		}
		if (const char* appdata_override = std::getenv("LEAF_APPDATA_DIR"); appdata_override && appdata_override[0]) {
			OverwriteAppdataDir(appdata_override);
		}

		wchar_t executable_path[MAX_PATH]{};
		DWORD length = GetModuleFileNameW(nullptr, executable_path, MAX_PATH);
		if (length == 0 || length >= MAX_PATH) {
			system_data.install_dir.clear();
			return error::unknown_error;
		}
		system_data.install_dir = fs::native_path{ executable_path }.parent_path();
		std::error_code current_error;
		system_data.current_dir = std::filesystem::current_path(current_error);
		if (current_error) { return error{ current_error, "reading current directory" }; }
		return sys::init_udp_sockets();
	}
	void exit_system() {
		sys::exit_udp_sockets();
	}

	string_view system_backend_name() {
		return "Windows";
	}

	ShutdownHandlerId AddShutdownHandler(ShutdownHandler handler, void* user_data) {
		const ShutdownHandlerId id = next_shutdown_handler_id++;
		shutdown_handlers.push_back(ShutdownHandlerEntry{
			.id = id,
			.handler = handler,
			.user_data = user_data,
		});
		SetConsoleCtrlHandler(console_shutdown_handler, TRUE);
		return id;
	}

	void RemoveShutdownHandler(ShutdownHandlerId id) {
		for (size_t index = 0; index < shutdown_handlers.size(); ++index) {
			if (shutdown_handlers[index].id != id) {
				continue;
			}
			shutdown_handlers.erase(shutdown_handlers.begin() + static_cast<std::ptrdiff_t>(index));
			break;
		}
		if (shutdown_handlers.empty()) {
			SetConsoleCtrlHandler(console_shutdown_handler, FALSE);
		}
	}

	const fs::native_path& GetAppdataDir() {
		return system_data.appdata_dir;
	}

	const fs::native_path& GetInstallDir() {
		return system_data.install_dir;
	}

	const fs::native_path& GetCurrentDir() {
		return system_data.current_dir;
	}

	void OverwriteAppdataDir(string_view new_path) {
		system_data.appdata_dir = new_path;
	}
} // namespace lf
