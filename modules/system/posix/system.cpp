#include "leaf/system/system.hpp"
#include "leaf/system/socket.hpp"
#include "leaf/core/logging.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits.h>
#include <unistd.h>
#include <vector>

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
		(void)title;
		std::fputs(message.data(), stderr);
		std::fputc('\n', stderr);
	}

	error init_system(span<string_view> args) {
		install_crash_handler();
		log::Logger::instance().add_sink(make_unique<log::ConsoleSink>());
		const char* xdg_data_home = std::getenv("XDG_DATA_HOME");
		if (xdg_data_home && xdg_data_home[0]) {
			system_data.appdata_dir = xdg_data_home;
		} else if (const char* home = std::getenv("HOME"); home && home[0]) {
			system_data.appdata_dir = fs::native_path{ home } / ".local" / "share";
		} else {
			system_data.appdata_dir = ".";
		}
		if (const char* appdata_override = std::getenv("LEAF_APPDATA_DIR"); appdata_override && appdata_override[0]) {
			system_data.appdata_dir = appdata_override;
		}

		char executable_path[PATH_MAX] = {};
		ssize_t length = readlink("/proc/self/exe", executable_path, sizeof(executable_path) - 1);
		if (length > 0) {
			executable_path[length] = '\0';
			system_data.install_dir = fs::native_path{ executable_path }.parent_path();
		} else if (!args.empty()) {
			system_data.install_dir = std::filesystem::absolute(fs::native_path{ args[0] }).parent_path();
		} else {
			system_data.install_dir = std::filesystem::current_path();
		}
		system_data.current_dir = std::filesystem::current_path();
		return sys::init_udp_sockets();
	}

	void exit_system() {
		sys::exit_udp_sockets();
	}

	string_view system_backend_name() {
#if defined(__APPLE__)
		return "macOS";
#elif defined(__linux__)
		return "Linux";
#else
		return "POSIX";
#endif
	}

	ShutdownHandlerId AddShutdownHandler(ShutdownHandler handler, void* user_data) {
		const ShutdownHandlerId id = next_shutdown_handler_id++;
		shutdown_handlers.push_back(ShutdownHandlerEntry{
			.id = id,
			.handler = handler,
			.user_data = user_data,
		});
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
