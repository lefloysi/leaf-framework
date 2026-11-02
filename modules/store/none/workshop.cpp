#include "leaf/store/workshop.hpp"

namespace lf::store {
	bool workshop_supported() {
		return false;
	}

	vector<WorkshopItem> workshop_subscriptions() {
		return {};
	}

	error workshop_refresh() {
		return error("Steam Workshop is available in the Steam version. Local puzzle packs work offline.");
	}

	error workshop_publish(const WorkshopUpload&) {
		return workshop_refresh();
	}

	WorkshopProgress workshop_progress() {
		return {};
	}

	error workshop_open(u64) {
		return workshop_refresh();
	}

	error workshop_open_legal_agreement() {
		return workshop_refresh();
	}

	void shutdown_store_workshop() {
	}
} // namespace lf::store
