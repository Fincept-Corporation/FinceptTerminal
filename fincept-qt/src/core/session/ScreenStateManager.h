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

private:
    explicit ScreenStateManager(QObject* parent = nullptr);

    /// Flush all screens in pending_saves_ to cache.db.
    void flush_pending();

    /// Perform the actual async SQLite write for one screen.
    static void write_async(const QString& key,
                            const QVariantMap& state,
                            int version,
                            const QString& session_id);

    QHash<QString, screens::IStatefulScreen*> pending_saves_; // key → screen ptr
    QTimer* debounce_timer_ = nullptr;
    QString session_id_;
};

} // namespace fincept
