#pragma once

#include <leaf/core/error.hpp>
#include <leaf/core/vector.hpp>

namespace lf::store {
	enum class workshop_item_state {
		download_pending,
		downloading,
		installed,
		update_pending,
	};

	struct WorkshopItem {
		u64 id = 0;
		string folder;
		u32 installed_timestamp = 0;
		workshop_item_state state = workshop_item_state::download_pending;
		u64 bytes_downloaded = 0;
		u64 bytes_total = 0;
	};

	enum class workshop_visibility {
		private_item,
		friends_only,
		public_item,
		unlisted,
	};

	struct WorkshopUpload {
		u64 item_id = 0; // Zero creates a new item; nonzero updates the author's existing item.
		string title;
		string description;
		string content_folder; // Absolute path. Keep contents unchanged until the upload finishes.
		string preview_file; // Absolute image path; optional for updates.
		string change_note;
		vector<string> tags;
		workshop_visibility visibility = workshop_visibility::private_item;
	};

	enum class workshop_upload_state {
		idle,
		creating,
		uploading,
		succeeded,
		failed,
	};

	struct WorkshopProgress {
		workshop_upload_state state = workshop_upload_state::idle;
		u64 item_id = 0; // Persist immediately, including after a failed upload of a new item.
		u64 bytes_processed = 0;
		u64 bytes_total = 0;
		bool needs_legal_agreement = false;
		string message;
	};

	// Main-thread APIs. Completion callbacks are dispatched by update_store().
	bool workshop_supported();
	vector<WorkshopItem> workshop_subscriptions();
	error workshop_refresh();
	error workshop_publish(const WorkshopUpload& upload);
	WorkshopProgress workshop_progress();
	error workshop_open(u64 item_id = 0); // Zero opens the application's Workshop.
	error workshop_open_legal_agreement();
	void shutdown_store_workshop();
} // namespace lf::store
