#pragma once

#include "leaf/core/error.hpp"
#include "leaf/core/progress.hpp"
#include "leaf/core/singleton.hpp"
#include "leaf/script/mod_info.hpp"

#include <mutex>
#include <future>
#include <thread>
#include <functional>
#include <optional>

namespace lf {
	struct Mod : Singleton<Mod> {
	  public:
		struct Source {
			enum class Kind {
				root,
				mod_directory,
			};
			fs::path path;
			bool privileged = false;
			Kind kind = Kind::root;
		};
		struct Discovery {
			vector<ModInfo> mods;
			vector<error> errors;
		};
		using MetadataFallback = std::function<report<std::optional<ModInfo>>(const fs::path&, bool)>;

		static Discovery discover(span<const Source> sources, const MetadataFallback& fallback = {});

		enum class enabled_preferences_access {
			read_only,
			synchronize,
		};
		struct LoadOptions {
			fs::path enabled_mods_path{ "/enabled_mods.yaml" };
			bool use_enabled_preferences = true;
			enabled_preferences_access preferences_access = enabled_preferences_access::synchronize;
		};

		// async operation
		static void load(span<const Source> sources);
		static void load(span<const Source> sources, LoadOptions options);
		static void unload();
		static const Progress& progress();
		static std::future<error> result();
		static void cancel();
		static span<ModInfo> loaded();

	  private:
		friend Singleton<Mod>;
		~Mod();

		vector<ModInfo> loaded_mods;
		vector<fs::mapping> mod_mappings;
		Progress loading_progress;
		std::future<error> loading_result;
		std::mutex operation_mutex;
		std::jthread loading_worker;
	};
} // namespace lf
