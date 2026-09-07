#include "leaf/leaf.hpp"

#include "leaf/core/error.hpp"
#include "leaf/core/filesystem.hpp"
#include "leaf/core/logging.hpp"
#include "leaf/core/scope.hpp"
#include "leaf/manager/asset.hpp"
#include "leaf/platform/platform.hpp"
#include <leaf/resource/registry.hpp>
#include "leaf/core/span.hpp"
#include "leaf/core/string.hpp"
#include "leaf/store/lifecycle.hpp"
#include "leaf/system/system.hpp"

#include <utility>

namespace lf {
	error Init(span<string_view> args) {
		if (error result{ init_system(args) }) {
			return result;
		}

		scope_exit system_cleanup{ exit_system };

		if (error result{ fs::init(GetInstallDir(), GetAppdataDir()) }) {
			return result;
		}

		scope_exit filesystem_cleanup{ fs::exit };

		if (error result{ init_store(args) }) {
			return result;
		}

		scope_exit store_cleanup{ exit_store };

		if (error result{ asset::init(2) }) {
			return result;
		}

		scope_exit assets_cleanup{ asset::exit };
		PrototypeTypeRegistry::functions.clear();
		scope_exit registration_cleanup{ [] { PrototypeTypeRegistry::functions.clear(); } };

		if (error result{ Register<PrototypeTypeRegistry>::install(PrototypeTypeRegistry::instance()) }) {
			return result;
		}

		registration_cleanup.release();
		assets_cleanup.release();
		store_cleanup.release();
		filesystem_cleanup.release();
		system_cleanup.release();
		return {};
	}

	bool Update() {
		update_store();
		return update_platform();
	}

	void Exit() {
		PrototypeTypeRegistry::functions.clear();
		asset::exit();
		exit_store();
		fs::exit();
		exit_system();
		log::Logger::instance().flush();
	}
}


