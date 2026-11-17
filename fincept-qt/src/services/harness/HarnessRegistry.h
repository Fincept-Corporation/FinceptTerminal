#pragma once
// HarnessRegistry.h — the single source of truth about the agent harnesses
// installed on this computer (Claude Code, Codex, …): where each one is, which
// version, whether it is signed in, which models and efforts it offers, and
// the user's per-harness settings.
//
// Ownership & threading
//   * GUI-thread QObject singleton; construction is cheap (no I/O, no process).
//     initialize_deferred() — from CodingAgentService::initialize_deferred(),
//     i.e. main's deferred init — migrates the old settings, loads the cache
//     and starts detection, so detection no longer waits for the workbench.
//   * Path searches (PATH + every known install folder, npm shim reading) run
//     on a private one-thread pool. Version / sign-in / model probes are
//     QProcess instances driven by the GUI event loop with timeouts — never a
//     waitFor* outside shutdown — and at most kMaxConcurrentProbes run at once.
//   * Every auto-detected binary must identify itself through the adapter's
//     parse_version() before it is used.
//   * Flatpak: the CLIs are programs of the user's system. The path search is
//     answered by the host (HarnessHostDetect.h, on the same worker pool) and
//     every probe / run / CONNECT command starts there (HarnessLaunch →
//     process::host::wrap). Without the host-command permission nothing is
//     searched: HarnessStatus::host_blocked + detect_error say how to allow it.
//   * Cache: <AppPaths::cache()>/harness/registry.json — path, file size +
//     mtime, version, sign-in state (10 min) and model list (24 h, or until the
//     version changes). No credential material is ever cached; sign-in output
//     is parsed for its named fields by the adapter and dropped, never logged.
//
// Settings (AppConfig): harness/<id>/path_override, default_model,
// default_effort, enabled_for_terminal, env_policy, terminal_consent_vendor,
// terminal_consent_at; harness/default_id, harness/allow_full_access. The old coding_agents/claude_path and
// coding_agents/codex_path keys are moved to harness/<id>/path_override once.
//
// Registered adapters: Claude Code, Codex (verified), Grok (from --help),
// Cursor, GitHub Copilot, Gemini CLI, Qwen Code (documentation only). The last
// five carry descriptor().beta until a real transcript has been recorded.

#include "services/harness/HarnessTypes.h"

#include <QObject>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <memory>

namespace fincept::harness {

class IHarnessAdapter;

class HarnessRegistry : public QObject {
    Q_OBJECT
  public:
    static HarnessRegistry& instance();
    ~HarnessRegistry() override;

    static constexpr int kMaxConcurrentProbes = 2;
    static constexpr qint64 kAuthTtlMs = 10LL * 60 * 1000;
    static constexpr qint64 kModelsTtlMs = 24LL * 3600 * 1000;

    static constexpr const char* kEnvPolicyInherit = "inherit";
    static constexpr const char* kEnvPolicySubscriptionOnly = "subscription_only";

    /// Migrate settings, load the cache and detect every harness. Idempotent.
    void initialize_deferred();

    QStringList ids() const; // registration order
    const IHarnessAdapter* adapter(const QString& id) const;
    /// An adapter is registered for `id`.
    bool is_known(const QString& id) const;
    /// The adapter's display name; "unknown harness (<id>)" when none is
    /// registered (a stored run of a harness this build does not have).
    QString display_name(const QString& id) const;
    /// Installed harnesses in registration order.
    QStringList installed_ids() const;
    /// The harness a new run uses when the caller names none: the user's default
    /// when installed, else the first installed one, else the default, else the
    /// first registered.
    QString preferred_harness() const;
    HarnessStatus status(const QString& id) const;
    /// At least one detection pass for `id` finished (installed or not). True for unknown ids.
    bool detection_complete(const QString& id) const;
    /// Start detection for ids that never completed one and have none in flight.
    void ensure_detected(const QString& id = {});
    /// Re-run what `scope` names for one harness (empty id = all), now.
    /// A detection already in flight is re-run once it finishes.
    void refresh(const QString& id = {}, RefreshScope scope = RefreshScope::All);
    /// Re-check only what is stale — a sign-in state older than kAuthTtlMs, a
    /// model list older than kModelsTtlMs or from another version — for every
    /// installed harness, and detect the ones never detected. Cheap; called
    /// whenever a screen showing harness state becomes visible.
    void revalidate();

    // ── Settings ─────────────────────────────────────────────────────────────
    QString path_override(const QString& id) const;
    void set_path_override(const QString& id, const QString& path); // re-detects that harness
    QString default_model(const QString& id) const;
    void set_default_model(const QString& id, const QString& model);
    QString default_effort(const QString& id) const;
    void set_default_effort(const QString& id, const QString& effort);
    bool enabled_for_terminal(const QString& id) const;
    /// Switching ON needs the user's terminal-AI consent for this harness
    /// (owner decision: a one-time dialog naming the vendor that receives the
    /// data). Without it nothing is stored and terminal_consent_requested(id)
    /// is emitted — the UI shows the dialog and calls grant_terminal_consent().
    void set_enabled_for_terminal(const QString& id, bool on);
    /// The user confirmed (once) that terminal AI may send data to this
    /// harness's vendor. Kept when "Use for terminal AI" is switched off again.
    bool terminal_consent(const QString& id) const;
    /// Record the consent (vendor + time) and, when `enable`, switch "Use for
    /// terminal AI" on. GUI thread.
    void grant_terminal_consent(const QString& id, bool enable = true);
    /// Forget the consent and switch terminal AI off for this harness.
    void revoke_terminal_consent(const QString& id);
    /// kEnvPolicyInherit (default) | kEnvPolicySubscriptionOnly (strip the harness's API-key variables).
    QString env_policy(const QString& id) const;
    void set_env_policy(const QString& id, const QString& policy);
    QString default_harness() const;
    void set_default_harness(const QString& id);
    /// Full-access runs (no sandbox, no approvals) may be chosen at all. Off by
    /// default; the UI asks for confirmation before turning it on.
    bool allow_full_access() const;
    void set_allow_full_access(bool on);
    /// An API-key variable of the harness reaches its children (present in the
    /// environment and not stripped by the env policy). Names only.
    bool api_key_reaches_child(const QString& id) const;

    /// Efforts for `model` (the CLI's list when known, else the adapter's).
    QStringList efforts(const QString& id, const QString& model) const;
    /// A run failed for `kind`: Auth re-checks the sign-in, RateLimit sets a
    /// cooldown (until `retry_at_ms`, else 5 min), NotInstalled re-detects.
    void report_failure(const QString& id, FailureKind kind, qint64 retry_at_ms = 0);
    /// A run of `id` succeeded: clears the last failure and any cooldown, and
    /// re-checks a cached "signed out" that the success contradicts.
    void report_success(const QString& id);

    /// THE environment for any child of harness `id` (runs, probes, CONNECT
    /// commands): HarnessLaunch::child_environment() with the adapter's
    /// env_set / env_unset from `plan` and the user's env policy applied.
    QProcessEnvironment child_environment(const QString& id, const Installation& inst,
                                          const LaunchPlan* plan = nullptr, const QString& mcp_token = {}) const;

    /// RunSpec::user_mcp_server_names for a run of `id`: the adapter's names
    /// from the user's own config file. Flatpak: that file is on the host — the
    /// names read there at the last detection / revalidate(), and a fresh read
    /// is started for the next run (never a wait here). GUI thread.
    QStringList user_mcp_server_names(const QString& id);

  signals:
    void status_changed(const QString& id);
    void defaults_changed();
    /// "Use for terminal AI" was switched on for a harness without consent:
    /// ask the user (the dialog names the vendor), then grant_terminal_consent().
    void terminal_consent_requested(const QString& id);

  private:
    HarnessRegistry();
    Q_DISABLE_COPY(HarnessRegistry)

    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::harness
