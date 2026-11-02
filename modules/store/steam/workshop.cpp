#include "leaf/store/workshop.hpp"

#include <steam/steam_api.h>

#include <filesystem>

namespace lf::store {
	class SteamWorkshop {
	public:
		WorkshopUpload upload;
		WorkshopProgress progress;
		UGCUpdateHandle_t update_handle = k_UGCUpdateHandleInvalid;
		CCallResult<SteamWorkshop, CreateItemResult_t> create_call;
		CCallResult<SteamWorkshop, SubmitItemUpdateResult_t> submit_call;
		void fail(string message);
		void submit();
		void created(CreateItemResult_t* result, bool io_failure);
		void submitted(SubmitItemUpdateResult_t* result, bool io_failure);
	};

	SteamWorkshop& steam_workshop() {
		static SteamWorkshop instance;
		return instance;
	}

	void SteamWorkshop::fail(string message) {
		progress.state = workshop_upload_state::failed;
		progress.message = std::move(message);
		update_handle = k_UGCUpdateHandleInvalid;
	}

	bool workshop_supported() {
		return SteamUGC() && SteamUser() && SteamUser()->BLoggedOn() && SteamUtils();
	}

	vector<WorkshopItem> workshop_subscriptions() {
		vector<WorkshopItem> result;
		if (!SteamUGC()) {
			return result;
		}
		vector<PublishedFileId_t> ids(SteamUGC()->GetNumSubscribedItems());
		const u32 count = SteamUGC()->GetSubscribedItems(ids.data(), static_cast<u32>(ids.size()));
		for (u32 index = 0; index < count; ++index) {
			WorkshopItem item;
			item.id = ids[index];
			const u32 state = SteamUGC()->GetItemState(ids[index]);
			char folder[32768]{};
			uint64 size = 0;
			uint32 timestamp = 0;
			if ((state & k_EItemStateInstalled) && SteamUGC()->GetItemInstallInfo(ids[index], &size, folder, sizeof(folder), &timestamp)) {
				item.folder = folder;
				item.installed_timestamp = timestamp;
				item.state = workshop_item_state::installed;
			}
			if (state & k_EItemStateNeedsUpdate) {
				item.state = workshop_item_state::update_pending;
			}
			if (state & k_EItemStateDownloading) {
				item.state = workshop_item_state::downloading;
			}
			uint64 downloaded = 0;
			uint64 total = 0;
			SteamUGC()->GetItemDownloadInfo(ids[index], &downloaded, &total);
			item.bytes_downloaded = downloaded;
			item.bytes_total = total;
			result.push_back(std::move(item));
		}
		return result;
	}

	error workshop_refresh() {
		if (!workshop_supported()) {
			return error("Connect to Steam to refresh Workshop puzzle packs.");
		}
		for (const auto& item : workshop_subscriptions()) {
			if (item.state == workshop_item_state::download_pending || item.state == workshop_item_state::update_pending) {
				if (!SteamUGC()->DownloadItem(item.id, false)) {
					return error("Steam could not queue a subscribed puzzle pack for download.");
				}
			}
		}
		return error::no_error;
	}

	void SteamWorkshop::submit() {
		update_handle = SteamUGC()->StartItemUpdate(SteamUtils()->GetAppID(), progress.item_id);
		if (update_handle == k_UGCUpdateHandleInvalid) {
			fail("Steam could not prepare this Workshop update.");
			return;
		}
		ERemoteStoragePublishedFileVisibility visibility = k_ERemoteStoragePublishedFileVisibilityPrivate;
		switch (upload.visibility) {
		case workshop_visibility::friends_only: visibility = k_ERemoteStoragePublishedFileVisibilityFriendsOnly; break;
		case workshop_visibility::public_item: visibility = k_ERemoteStoragePublishedFileVisibilityPublic; break;
		case workshop_visibility::unlisted: visibility = k_ERemoteStoragePublishedFileVisibilityUnlisted; break;
		default: break;
		}
		vector<const char*> tags;
		for (const auto& tag : upload.tags) {
			tags.push_back(tag.c_str());
		}
		SteamParamStringArray_t tag_array{ tags.data(), static_cast<int>(tags.size()) };
		if (!SteamUGC()->SetItemTitle(update_handle, upload.title.c_str())
			|| !SteamUGC()->SetItemDescription(update_handle, upload.description.c_str())
			|| !SteamUGC()->SetItemVisibility(update_handle, visibility)
			|| !SteamUGC()->SetItemTags(update_handle, &tag_array)
			|| !SteamUGC()->SetItemContent(update_handle, upload.content_folder.c_str())
			|| (!upload.preview_file.empty() && !SteamUGC()->SetItemPreview(update_handle, upload.preview_file.c_str()))) {
			fail("Steam rejected the title, description, content folder, or preview image.");
			return;
		}
		const auto call = SteamUGC()->SubmitItemUpdate(update_handle, upload.change_note.c_str());
		if (call == k_uAPICallInvalid) {
			fail("Steam could not start this Workshop upload. Your local pack is saved.");
			return;
		}
		progress.state = workshop_upload_state::uploading;
		progress.message = "Uploading puzzle pack...";
		submit_call.Set(call, this, &SteamWorkshop::submitted);
	}

	void SteamWorkshop::created(CreateItemResult_t* result, bool io_failure) {
		if (io_failure || !result || result->m_eResult != k_EResultOK) {
			fail(lf::format("Steam could not create the Workshop item (result {}).", result ? int(result->m_eResult) : -1));
			return;
		}
		progress.item_id = result->m_nPublishedFileId;
		progress.needs_legal_agreement = result->m_bUserNeedsToAcceptWorkshopLegalAgreement;
		submit();
	}

	void SteamWorkshop::submitted(SubmitItemUpdateResult_t* result, bool io_failure) {
		if (result) {
			progress.needs_legal_agreement = progress.needs_legal_agreement || result->m_bUserNeedsToAcceptWorkshopLegalAgreement;
		}
		if (io_failure || !result || result->m_eResult != k_EResultOK) {
			fail(lf::format("Workshop upload failed (result {}). Your local pack is saved; retry to update this item.", result ? int(result->m_eResult) : -1));
			return;
		}
		progress.state = workshop_upload_state::succeeded;
		progress.message = progress.needs_legal_agreement ? "Uploaded. Accept the Steam Workshop agreement to make this item visible." : "Puzzle pack uploaded.";
		update_handle = k_UGCUpdateHandleInvalid;
	}

	error workshop_publish(const WorkshopUpload& upload) {
		if (!workshop_supported()) {
			return error("Connect to Steam to upload a puzzle pack. Your local pack is saved.");
		}
		auto& backend = steam_workshop();
		if (backend.progress.state == workshop_upload_state::creating || backend.progress.state == workshop_upload_state::uploading) {
			return error("A Workshop upload is already in progress.");
		}
		if (upload.title.empty() || upload.title.size() >= k_cchPublishedDocumentTitleMax || upload.description.size() >= k_cchPublishedDocumentDescriptionMax) {
			return error("Enter a title under 129 bytes and a description under 8000 bytes.");
		}
		std::error_code ec;
		const auto content = std::filesystem::u8path(upload.content_folder.c_str());
		if (!content.is_absolute() || !std::filesystem::is_directory(content, ec)) {
			return error("Choose an existing absolute content folder for this Workshop pack.");
		}
		if (!upload.preview_file.empty()) {
			const auto preview = std::filesystem::u8path(upload.preview_file.c_str());
			if (!preview.is_absolute() || !std::filesystem::is_regular_file(preview, ec)) {
				return error("The Workshop preview image could not be found.");
			}
		}
		backend.upload = upload;
		backend.progress = {};
		backend.progress.item_id = upload.item_id;
		if (upload.item_id) {
			backend.submit();
			if (backend.progress.state == workshop_upload_state::failed) {
				return error(backend.progress.message);
			}
			return error::no_error;
		}
		const auto call = SteamUGC()->CreateItem(SteamUtils()->GetAppID(), k_EWorkshopFileTypeCommunity);
		if (call == k_uAPICallInvalid) {
			backend.fail("Steam could not start creating a Workshop item.");
			return error(backend.progress.message);
		}
		backend.progress.state = workshop_upload_state::creating;
		backend.progress.message = "Creating private Workshop item...";
		backend.create_call.Set(call, &backend, &SteamWorkshop::created);
		return error::no_error;
	}

	WorkshopProgress workshop_progress() {
		auto& backend = steam_workshop();
		if (backend.progress.state == workshop_upload_state::uploading && SteamUGC()) {
			uint64 processed = 0;
			uint64 total = 0;
			SteamUGC()->GetItemUpdateProgress(backend.update_handle, &processed, &total);
			backend.progress.bytes_processed = processed;
			backend.progress.bytes_total = total;
		}
		return backend.progress;
	}

	error workshop_open(u64 item_id) {
		if (!workshop_supported() || !SteamFriends() || !SteamUtils()->IsOverlayEnabled()) {
			return error("Enable the Steam overlay to open the Workshop from the game.");
		}
		const auto url = item_id ? lf::format("https://steamcommunity.com/sharedfiles/filedetails/?id={}", item_id)
			: lf::format("https://steamcommunity.com/app/{}/workshop/", SteamUtils()->GetAppID());
		SteamFriends()->ActivateGameOverlayToWebPage(url.c_str());
		return error::no_error;
	}

	error workshop_open_legal_agreement() {
		if (!workshop_supported() || !SteamFriends() || !SteamUtils()->IsOverlayEnabled()) {
			return error("Enable the Steam overlay to open the Workshop agreement.");
		}
		SteamFriends()->ActivateGameOverlayToWebPage("https://steamcommunity.com/sharedfiles/workshoplegalagreement");
		return error::no_error;
	}

	void shutdown_store_workshop() {
		auto& backend = steam_workshop();
		backend.create_call.Cancel();
		backend.submit_call.Cancel();
		backend.progress = {};
		backend.update_handle = k_UGCUpdateHandleInvalid;
	}
} // namespace lf::store
