#pragma once

#include "leaf/core/error.hpp"
#include "leaf/core/progress.hpp"
#include "leaf/core/singleton.hpp"
#include "leaf/script/mod_info.hpp"
#include "leaf/manager/asset.hpp"

#include <mutex>
#include <future>
#include <thread>

namespace lf {
	struct Mod : Singleton<Mod> {
	  public:
		struct Source {
			fs::path path;
			bool privileged = false;
		};

		// async operation
		static void load(span<const Source> sources);
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
		unique_ptr<asset::group> images;
		Progress loading_progress;
		std::future<error> loading_result;
		std::mutex operation_mutex;
		std::jthread loading_worker;
	};
} // namespace lf
