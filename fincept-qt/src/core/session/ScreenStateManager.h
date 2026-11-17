#pragma once
#include <QHash>
#include <QObject>
#include <QPointer>

// Forward declare to avoid including the full screen header here.
namespace fincept::screens {
class IStatefulScreen;
}

namespace fincept {

/// Central coordinator for screen UI state persistence.
///
/// Screens call notify_changed() whenever significant UI state changes
/// (tab switched, filter applied, scroll moved). This class debounces rapid
/// changes and async-writes to cache.db's screen_state table via TabSessionStore.
///
/// DockScreenRouter calls save_now() on screen hide and restore() after a
/// screen is first materialized.
///
/// All writes are non-blocking: they are dispatched via QtConcurrent::run()
/// so the UI thread is never stalled waiting on SQLite.
///
/// Ordering / crash safety (see ScreenStatePolicy.h):
///   * a screen restored or saved under a panel instance id is BOUND to it, and
///     its notify_changed() saves then go to that panel's own row (plus the
///     screen-type row, tagged with the writer) — so the row a restore reads
///     first is always the one every edit updates;
///   * every write carries a dispatch sequence number and a write is skipped if
///     a later one already landed on that row, so pool threads finishing out of
///     order can never put older state on top of newer;
///   * a restore compares the panel row with the screen-type row and uses the
///     newer copy (never writes anything itself);
///   * the debounce restarts on every change but never delays a save by more
///     than ~2.5 s, bounding what an abnormal exit can lose. Each row write is
///     a single SQLite statement (atomic).
class ScreenStateManager : public QObject {
    Q_OBJECT
  public:
    static ScreenStateManager& instance();

    /// Set the session ID written alongside each state record.
    /// Call this once at startup before any screens are shown.
    void set_session_id(const QString& id);
    QString session_id() const;

    /// Load persisted state from cache.db and call screen->restore_state().
    /// Called by DockScreenRouter immediately after a screen is materialized.
    /// No-op if cache.db is unavailable or no state is stored yet.
    void restore(screens::IStatefulScreen* screen);

    /// Immediately flush the screen's current state to cache.db.
    /// Called by DockScreenRouter on visibilityChanged(false).
    /// The write is dispatched asynchronously — this call returns immediately.
    void save_now(screens::IStatefulScreen* screen);

    /// Mark this screen as having pending state changes.
    /// Coalesces rapid calls; a single async write fires after a 500 ms quiet period.
    /// Safe to call from any connected signal (tab changed, filter changed, etc.).
    void notify_changed(screens::IStatefulScreen* screen);

    /// Direct save for screens that cannot implement IStatefulScreen due to
    /// conflicting save_state() signatures (e.g. IWorkspaceParticipant).
    /// Call from hideEvent() or on significant state changes.
    void save_direct(const QString& key, const QVariantMap& state, int version = 1);

    /// Direct load for screens that cannot implement IStatefulScreen.
    /// Returns empty map if nothing stored or version mismatch.
    QVariantMap load_direct(const QString& key, int expected_version = 1);

    // ── Phase 4b: instance-UUID-keyed save/restore overloads ─────────────────
    //
    // Identical semantics to the screen-key versions, but storage is keyed
    // by per-panel UUID so two open panels of the same screen type don't
    // overwrite each other's state. DockScreenRouter uses these for any
    // screen that has a PanelHandle (which, post-Phase 4, is every panel).
    //
    // The legacy screen-key API stays for non-panel callers
    // (IWorkspaceParticipant, etc.) and for the migration window where
    // some screens haven't been wired to their PanelHandle yet.

    /// Restore state into `screen` under the given instance UUID and bind the
    /// screen to that UUID (its later notify_changed() saves go to this row).
    /// With `fallback_to_screen_key`, the screen-type row is a candidate too:
    /// it is used when the panel has no row of its own yet (first open /
    /// migration) or when it is NEWER than the panel's row and was written by
    /// this panel or an older build — the newest saved state always wins
    /// (ScreenStatePolicy.h). Reads only; never writes.
    void restore_by_uuid(screens::IStatefulScreen* screen, const QString& instance_uuid,
                         bool fallback_to_screen_key = true);

    /// Save now (async) under the given instance UUID. The screen's
    /// state_key() is also written into the `screen_key` column for
    /// diagnostics; the actual lookup key is the UUID.
    void save_now_by_uuid(screens::IStatefulScreen* screen, const QString& instance_uuid);

    /// Notify changed under UUID. The debounce key is the UUID, so two
    /// open watchlists with rapid changes don't coalesce into one row.
    void notify_changed_by_uuid(screens::IStatefulScreen* screen, const QString& instance_uuid);

    /// Drop the UUID-keyed row. Called by DockScreenRouter when a panel
    /// closes permanently (vs. temporary dematerialisation).
    void erase_by_uuid(const QString& instance_uuid);

    // ── Synchronous saves for shutdown ───────────────────────────────────────
    //
    // save_now*/notify_changed* dispatch the SQLite write via QtConcurrent so
    // the UI thread never blocks. That's correct at runtime but WRONG on quit:
    // panels visible at close never receive visibilityChanged(false), and an
    // async write fired from closeEvent races process exit and is usually lost.
    // These variants write on the CALLING thread (blocking, no QtConcurrent) so
    // the row is on disk before the app exits. Use ONLY from shutdown paths.
    void save_now_sync(screens::IStatefulScreen* screen);
    void save_now_by_uuid_sync(screens::IStatefulScreen* screen, const QString& instance_uuid);

    /// Write every still-debounced change now, on the calling thread. Shutdown
    /// paths only (DockScreenRouter::flush_all_screen_states) — covers screens
    /// that notified a change but are not open dock panels.
    void flush_pending_sync();

  private:
    explicit ScreenStateManager(QObject* parent = nullptr);

    /// Flush all screens in pending_saves_ to cache.db.
    void flush_pending();

    /// (Re)start the debounce timer, capped so a stream of changes cannot
    /// postpone the save indefinitely.
    void arm_debounce();

    QHash<QString, screens::IStatefulScreen*> pending_saves_;      // key → screen ptr
    QHash<QString, screens::IStatefulScreen*> pending_uuid_saves_; // instance_uuid → screen ptr
    QTimer* debounce_timer_ = nullptr;
    QString session_id_;
};

} // namespace fincept
