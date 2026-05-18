#pragma once
#include <QVariantMap>

namespace fincept::screens {

/// Opt-in interface for screens that want to persist lightweight UI state
/// (active tab index, last searched symbol, selected region, etc.) across
/// sessions. Screens implementing this are called by DockScreenRouter after
/// construction (restore_state) and before destruction/hide (save_state).
///
/// Usage:
///   class MyScreen : public QWidget, public IStatefulScreen { ... };
///
/// The state map uses plain string keys and QVariant values — keep it small.
/// Do NOT store large data, credentials, or full model state here.
class IStatefulScreen {
  public:
    virtual ~IStatefulScreen() = default;

    /// Called by DockScreenRouter after the screen is fully constructed and
    /// shown for the first time. Restore UI state from the provided map.
    virtual void restore_state(const QVariantMap& state) = 0;

    /// Called by DockScreenRouter when the screen is hidden or closed.
    /// Return a map of lightweight UI state to persist.
    virtual QVariantMap save_state() const = 0;

    /// Unique key used for storage — must match the screen's dock id.
    virtual QString state_key() const = 0;

    /// State schema version. Increment this whenever save_state()'s key set
    /// changes in an incompatible way. Stored state with a different version
    /// is discarded and the screen starts with a clean slate rather than
    /// crashing or misinterpreting stale keys.
    virtual int state_version() const { return 1; }

    /// Synchronously flush any debounced/in-flight state changes. The
    /// default `notify_changed()` path is debounced (500ms) so rapid edits
    /// don't hammer SQLite, but Phase 5 tear-off and Phase 6 layout-switch
    /// need a synchronous "everything-on-disk-now" guarantee before
    /// reparenting a panel across frames or serialising a workspace.
    ///
    /// Default implementation: no-op. Screens that maintain their own
    /// in-memory edit buffers (e.g. notes editor, code editor) should
    /// override to push the buffer through `save_state()` immediately.
    /// `ScreenStateManager::save_now*()` does the SQLite write part —
    /// this hook covers the *screen-side* "flush in-memory edits to the
    /// state map" part that happens before save_state() is called.
    virtual void flush_pending_state() {}
};

} // namespace fincept::screens
