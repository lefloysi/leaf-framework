#include "leaf/leaf.hpp"

#include "leaf/application/rml_backend.hpp"
#include "leaf/core/error.hpp"
#include "leaf/core/filesystem.hpp"
#include "leaf/core/logging.hpp"
#include "leaf/core/span.hpp"
#include "leaf/core/string.hpp"
#include "leaf/graphics/graphics.hpp"
#include "leaf/graphics/window.hpp"
#include "leaf/platform/platform.hpp"
#include "leaf/store/lifecycle.hpp"
#include "leaf/system/system.hpp"

namespace lf {
	error Init(span<string_view> args) {
		error err;

		log::Info("[leaf] initializing leaf-framework...");

		err = init_system(args);
		if (err) {
			goto system_exit;
		}

		err = rt::init_graphics(args, false);
		if (err) {
			goto graphics_exit;
		}

		err = init_store(args);
		if (err) {
			goto store_exit;
		}

		err = init_platform(args);
		if (err) {
			goto platform_exit;
		}

<<<<<<< Updated upstream
		err = rt::init_graphics_extensions(false);
		if (err) {
			goto platform_exit;
=======
		scope_exit assets_cleanup{ asset::exit };
		PrototypeTypeRegistry::functions.clear();
		scope_exit registration_cleanup{ [] { PrototypeTypeRegistry::functions.clear(); } };

		if (error result{ Register<PrototypeTypeRegistry, error()>::install() }) {
			return result;
>>>>>>> Stashed changes
		}

		err = init_rml(args);
		if (err) {
			goto platform_exit;
		}

		return err;
	platform_exit:
		exit_platform();
	store_exit:
		exit_store();
		rt::exit_graphics();
	graphics_exit:
	system_exit:
		exit_system();
		log::Error("{}", err.message);
		return err;
	}

	error InitHeadless(span<string_view> args) {
		error err;

		err = init_system(args);
		if (err) {
			goto system_exit;
		}

		err = rt::init_graphics(args, true);
		if (err) {
			goto graphics_exit;
		}

		err = init_store(args);
		if (err) {
			goto store_exit;
		}

		return err;
	store_exit:
		exit_store();
	graphics_exit:
		rt::exit_graphics();
	system_exit:
		exit_system();
		log::Error("{}", err.message);
		return err;
	}

	bool Update() {
		update_store();
		return true;
	}

	void Exit() {
		log::Debug("[leaf] Shutting down!");

		exit_rml();
		exit_platform();

		exit_store();
		rt::exit_graphics();
		exit_system();
	}

	void ExitHeadless() {
		log::Debug("[leaf] Shutting down!");

		exit_store();
		rt::exit_graphics();
		exit_system();
	}
} // namespace lf
