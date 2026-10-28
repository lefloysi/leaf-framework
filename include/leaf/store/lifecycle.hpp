#pragma once

#include <leaf/core/error.hpp>
#include <leaf/core/span.hpp>
#include <leaf/core/string.hpp>

namespace lf {
	// Call before Init. A true result requests exiting this process so the store can relaunch it.
	bool restart_store_app(u32 app_id);
	error init_store(span<string_view> args);
	string_view store_backend_name();
	void update_store();
	void exit_store();
} // namespace lf
