#pragma once

// Phase 0 stub — see fincept-qt/DATAHUB_ARCHITECTURE.md §3.4.
// Full definition lands in Phase 1 alongside DataHub.cpp.

namespace fincept::datahub {

/// Per-topic refresh + caching metadata.
/// Populated by `DataHub::set_policy()` or `set_policy_pattern()`.
struct TopicPolicy {
    /// Cached value is considered fresh for this many milliseconds.
    /// Scheduler will not trigger a refresh until it expires.
    int ttl_ms = 30'000;

    /// Lower bound on refresh frequency. The scheduler will not call
    /// `Producer::refresh()` on this topic more often than this,
    /// regardless of how many subscribers request it.
    int min_interval_ms = 5'000;

    /// If true, the scheduler never touches this topic — producers are
    /// expected to push updates as they arrive (e.g. WebSocket feeds).
    /// `ttl_ms` and `min_interval_ms` are ignored.
    bool push_only = false;

    /// Higher priority topics are dispatched first when the scheduler
    /// has to shed load under producer rate limits.
    int priority = 0;

    /// Opt-in: when true, the hub rehydrates the last cached value from
    /// `CacheManager` on app start and emits it to subscribers. Default
    /// false per the Phase 0 decision — we start cold.
    bool rehydrate_on_start = false;

    /// Backpressure guard for high-rate push_only feeds. When non-zero,
    /// the hub stores the latest payload for the topic but delays fan-out
    /// — each `publish()` within the window replaces the pending value,
    /// and only the most-recent value is delivered to subscribers when
    /// the window ends. `topic_updated`, pattern slots, and direct slots
    /// all fire exactly once per window. Added in Phase 4 for Kraken /
    /// HyperLiquid ticker feeds.
    int coalesce_within_ms = 0;
};

} // namespace fincept::datahub
