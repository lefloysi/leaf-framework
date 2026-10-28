#include "leaf/store/lifecycle.hpp"

#include <leaf/core/logging.hpp>

namespace lf {
	bool restart_store_app(u32) {
		return false;
	}

	error init_store(span<string_view>) {
		log::Debug("[leaf] Starting store...");
		log::Info("[leaf] Store backend : none");
		return error::no_error;
	}

	string_view store_backend_name() {
		return "none";
	}

	void update_store() {
	}

	void exit_store() {}

} // namespace lf
