#pragma once
#include "core/layout/LayoutTypes.h"

#include <QString>

class QScreen;

namespace fincept {

/// Stable monitor identification + topology hashing.
///
/// `QScreen::name()` is what today's code persists when pinning a window to
/// a monitor (`SessionManager::save_screen_name`). It works most of the time
/// on Windows (`\\.\DISPLAY1` etc.) but breaks on:
///   - USB-C dock plug/unplug (display name shifts).
///   - Reboot enumeration changes (different boot order = different name).
///   - Multiple identical monitors (all named "DISPLAY3" on different machines).
///
/// `MonitorTopology` defines a composite, stable id derived from properties
/// the OS exposes consistently across hot-plug events:
///
///   StableScreenId = serialNumber  (Qt 6.0+, when present)
///                  | hash("manufacturer/model/widthxheight/x@y")
///
/// Hash collisions across truly-identical monitors are fine — they're
/// interchangeable for layout placement.
///
/// `MonitorTopologyKey` (defined in LayoutTypes.h) is the key for an entire
/// connected-monitor configuration: a sorted, comma-joined list of stable
/// ids. Two physically-identical desk setups produce the same topology key;
/// plugging/unplugging produces a different key. WorkspaceVariant matching
/// (Phase 6) keys off this.
class MonitorTopology {
  public:
    /// Stable id for a single screen. Never empty for a connected screen.
    /// Result is deterministic per physical monitor across boots / dock
    /// reorderings (best-effort — see class doc).
    static QString stable_id(const QScreen* screen);

    /// Topology key for the full set of screens currently connected. Returns
    /// empty key if QGuiApplication has no screens (test harnesses, headless).
    static layout::MonitorTopologyKey current_key();

    /// Find a connected screen whose stable id matches. nullptr if not present.
    static QScreen* find_screen_by_stable_id(const QString& stable_id);
};

} // namespace fincept
