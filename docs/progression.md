# Statistics and achievements

Load `stat` and `achievement` prototypes through the existing mod loader, then initialize an
`lf::Progression` for each player. Definitions and runtime state are separate.

```lua
data:extend({
    {
        type = "stat", name = "quests_completed", value_type = "integer",
        default_value = 0, min_value = 0, increment_only = true,
        store_id = "STAT_QUESTS_COMPLETED",
    },
    {
        type = "achievement", name = "quest_starter",
        local_name = "Getting started", local_description = "Complete five quests.",
        conditions = {
            { type = "stat_threshold", stat = "quests_completed", threshold = 5 },
        },
        store_id = "ACH_QUEST_STARTER",
    },
})
```

Omit `store_id` for local-only entries. Store IDs are independent of local IDs. Definitions must
already exist in the storefront; Leaf does not publish them.

Stat types are `integer`, `float`, and `average_rate`. Optional `min_value`, `max_value`, and
`increment_only` constrain updates. Store-backed integers must fit signed 32-bit values.

Use `set_stat` or `add_stat` for scalar values. Use `update_rate(id, amount, elapsed_seconds)` for
average rates. Their `window_seconds` defaults to 3600. The runtime trims the oldest sample interval
proportionally and exposes amount per second. Include zero-amount intervals for idle time.
Configure the corresponding Steam window consistently.

Check returned errors: mismatched types, overflow, nonfinite values, and violated constraints are
rejected. Updates evaluate conditions transactionally. `find_stat` and `find_achievement` return
runtime objects whose addresses survive updates and restores; reinitialization invalidates them.

Conditions are engine-defined, not Lua functions. `stat_threshold` is built in. Register additional
evaluators with `register_condition` before `initialize`. They receive a configuration dictionary
and read-only runtime collection, and return completion plus progress from zero to one.
Multiple conditions must all complete. Awards without conditions can be completed explicitly with
`complete_achievement`. Completed achievements stay complete.

In-game displays should use `Stat::value`, `Achievement::progress`, and `Achievement::completed`.

## Persistence and synchronization

`save` and `load` accept Leaf virtual filesystem paths. Use a writable mount. Saves replace files
atomically; invalid restores leave state intact. Snapshot records use string IDs, independent of
prototype ordering. Old entries whose IDs no longer exist are ignored.

`lf::store::StoreProgression::synchronize(player)` imports and submits store-backed state.
The standalone backend leaves local progression functional. Steam imports untouched values,
preserves unsynchronized local changes, and merges increment-only values upward. Continue pumping
the existing `update_store` for completion callbacks.

Uploads acknowledge revisions only after asynchronous success. Updates made during an upload remain
pending. Save after synchronization changes local state, including acknowledgments that remove
pending average-rate samples. Failed submissions retain local changes. Ordinary retries do not
apply staged rate samples twice. A Steam InvalidParam response requires correcting the store schema
and restarting the session; rejected data is not acknowledged.

Store statistics are not detailed game saves. Completed level identities and quest logs remain
the game's responsibility.
