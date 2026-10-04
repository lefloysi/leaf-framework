#include "leaf/leaf.hpp"

#include "leaf/core/error.hpp"
#include "leaf/core/filesystem.hpp"
#include "leaf/core/logging.hpp"
#include "leaf/core/profiler.hpp"
#include "leaf/core/scope.hpp"
#include "leaf/core/span.hpp"
#include "leaf/core/string.hpp"
#include "leaf/application/rml.hpp"
#include "leaf/graphics/graphics.hpp"
#include "leaf/manager/asset.hpp"
#include "leaf/platform/platform.hpp"
#include "leaf/store/lifecycle.hpp"
#include "leaf/system/system.hpp"
#include <leaf/resource/registry.hpp>

#include <utility>

namespace lf {
	void Init(span<string_view> args, string_view application) {
		if (error result = init_system(args)) {
			throw runtime_exception(result.message);
		}
		scope_exit system_cleanup(exit_system);

		if (application.empty()) {
			application = "leaf-framework";
		}

		const auto directory = GetAppdataDir() / fs::native_path{ application };
		OverwriteAppdataDir(directory.string());
		fs::native_volume(directory, { fs::access_mode::read_write, fs::missing_action::create });

		if (error result = fs::init(GetInstallDir(), GetAppdataDir())) {
			throw runtime_exception(result.message);
		}
		scope_exit filesystem_cleanup(fs::exit);


		if (error result = init_store(args)) {
			throw runtime_exception(result.message);
		}
		scope_exit store_cleanup(exit_store);


		if (error result = asset::init(2)) {
			throw runtime_exception(result.message);
		}
		scope_exit assets_cleanup(asset::exit);

		PrototypeTypeRegistry::functions.clear();
		Register<PrototypeTypeRegistry>::install(PrototypeTypeRegistry::instance());

		if (error result = rt::init_graphics(args)) {
			throw runtime_exception(result.message);
		}
		scope_exit graphics_cleanup(rt::exit_graphics);

		if (error result = rt::init_graphics_extensions()) {
			throw runtime_exception(result.message);
		}

		if (error result = init_platform(args)) {
			throw runtime_exception(result.message);
		}
		scope_exit platform_cleanup(exit_platform);

		if (error result = init_rml(args)) {
			throw runtime_exception(result.message);
		}
		scope_exit rml_cleanup(exit_rml);


		rml_cleanup.release();
		platform_cleanup.release();
		graphics_cleanup.release();
		assets_cleanup.release();
		store_cleanup.release();
		filesystem_cleanup.release();
		system_cleanup.release();
	}

	void Run(const std::function<void()>& application) {
		run_platform(application);
	}

	bool Update() {
		LF_PROFILE_SCOPE("frame.update-store");
		update_store();
		return update_platform();
	}

	void Exit() {
		exit_rml();
		asset::exit();
		exit_platform();
		rt::exit_graphics();
		PrototypeTypeRegistry::functions.clear();
		exit_store();
		fs::exit();
		exit_system();
		log::Logger::instance().flush();
	}
} // namespace lf
