# Store Workshop integration

Include `leaf/store/workshop.hpp` and link the selected storefront implementation. The Steam
backend uses SteamUGC; the standalone backend returns explicit unavailability errors.

Call these APIs on the main thread after store initialization. `lf::update_store()` dispatches
the asynchronous Steam callbacks; the framework's normal update loop already calls it.

- `workshop_supported()` reports online Steam availability for upload and overlay actions.
- `workshop_subscriptions()` lists subscribed item IDs, installed folders and download state.
  Cached installed content may remain available offline. Treat content as untrusted application
  data and validate it before use.
- `workshop_refresh()` requests downloads of subscribed missing or outdated items.
- `workshop_open()` opens the app's Workshop in the Steam overlay. Pass an item ID for its page.
- `workshop_publish(upload)` creates an item when `item_id` is zero, or updates an existing item.
  Metadata, tags, visibility, an absolute content folder and an optional preview are supported.
  Keep upload files unchanged until completion. New uploads default to private visibility.
- `workshop_progress()` reports creating/uploading/success/failure, byte progress, the item ID and
  whether the Workshop legal agreement is required. Persist a newly created item ID immediately,
  including when uploading its content subsequently fails. Retry using that ID to avoid duplicates.
- `workshop_open_legal_agreement()` opens Steam's agreement page.

Only one upload is active at a time. The caller presents errors and confirms publishing with the
player; calling the API sends content to Steam. Store shutdown cancels outstanding callbacks before
Steam is shut down. Workshop setup, permissions and content policies remain Steamworks settings.
Primary preview uploads require the app's Steam Cloud file and storage quotas to be configured;
see [SetItemPreview](https://partner.steamgames.com/doc/api/ISteamUGC#SetItemPreview).

The backend does not parse downloaded files, merge game content, or determine achievement policy.
Those decisions belong to the application. Subscription discovery and browsing do not publish data.
