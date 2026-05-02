#pragma once

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

    /// If a producer's refresh() doesn't publish() (success) or
    /// publish_error() (failure) within this window, the hub clears the
    /// `in_flight` flag and logs a warning. Prevents a crashed / hung /
    /// silently-failing producer from pinning a topic forever.
    int refresh_timeout_ms = 30'000;

    /// If true, the scheduler never touches this topic — producers are
    /// expected to push updates as they arrive (e.g. WebSocket feeds).
    /// `ttl_ms`, `min_interval_ms`, and `refresh_timeout_ms` are ignored.
    bool push_only = false;

    /// Backpressure guard for high-rate push_only feeds. When non-zero,
    /// the hub stores the latest payload for the topic but delays fan-out
    /// — each `publish()` within the window replaces the pending value,
    /// and only the most-recent value is delivered to subscribers when
    /// the window ends. `topic_updated`, pattern slots, and direct slots
    /// all fire exactly once per window.
    int coalesce_within_ms = 0;

    /// If true, the hub drops the entire TopicState (value, counters,
    /// error history) when the last subscriber leaves. Use for topics
    /// where the set of live topics is unbounded (e.g. agent run outputs,
    /// per-session LLM streams) so the hub doesn't accumulate state for
    /// every unique topic key seen in a long session.
    ///
    /// Default is false — most topics benefit from keeping last-known-
    /// good values for `peek()` and for re-subscribe warm-start.
    bool drop_on_idle = false;

    /// Phase 8 / decision 9.2: when true, fan-out to a subscriber is
    /// suppressed if the WindowFrame the subscriber lives in reports
    /// `is_active_for_work() == false` (minimised, hidden, etc.). The
    /// cached value is still updated, so the panel sees the latest data
    /// the moment the user brings the window forward.
    ///
    /// Use sparingly — only for high-frequency streams whose UI cost is
    /// real (sparkline ticks, live quotes). Low-frequency families like
    /// portfolio, news, or wallet balance should leave this false so the
    /// user sees fresh data the moment they restore the window without a
    /// re-fetch round-trip.
    ///
    /// Default false: zero behaviour change for existing producers.
    bool pause_when_inactive = false;
};

} // namespace fincept::datahub
