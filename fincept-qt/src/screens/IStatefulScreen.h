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
};

} // namespace fincept::screens
