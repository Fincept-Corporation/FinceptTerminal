#pragma once
#include "core/layout/LayoutTypes.h"

#include <QObject>
#include <QTimer>

namespace fincept {

/// Process-singleton that listens for monitor topology changes — screens
/// added, removed, primary-changed — and emits a debounced
/// `topology_changed` signal once the OS settles.
///
/// Why debounce: laptop lid close/open and dock plug/unplug can produce
/// 3–5 raw screen events in rapid succession (Qt fires
/// `screenAdded` → `screenRemoved` → `primaryScreenChanged` separately).
/// Reacting to each one individually would re-arrange windows mid-flight.
/// 500 ms debounce per design-doc decision 5.1 / 5.7 lets the dust settle.
///
/// Phase 6 saved-layout work consumes `topology_changed` to ask
/// "does the active workspace have a saved variant for this new topology?
/// If yes, prompt the user to switch."
///
/// Threading: hub-style — connects to `QGuiApplication` signals on the GUI
/// thread; the debounce QTimer also lives on the GUI thread. No worker thread.
class MonitorWatcher : public QObject {
    Q_OBJECT
  public:
    static MonitorWatcher& instance();

    /// Currently-known topology key. Cached so consumers can read without
    /// re-walking the screen list. Updated immediately when the underlying
    /// QGuiApplication signal fires; the debounced public signal is what
    /// notifies subscribers, but the cached value is fresh either way.
    layout::MonitorTopologyKey current_topology() const { return current_topology_; }

  signals:
    /// Emitted after a 500 ms quiet period following the last raw screen
    /// event. The argument is the new topology key (current_topology()).
    /// Subscribers compare against the previously-seen key to decide
    /// whether to act — Qt's signal queue may deliver multiple in flight
    /// during transitions on some platforms.
    void topology_changed(const layout::MonitorTopologyKey& new_topology);

  private:
    MonitorWatcher();

    void on_raw_screen_event();
    void on_debounce_fired();

    layout::MonitorTopologyKey current_topology_;
    QTimer debounce_;
};

} // namespace fincept
