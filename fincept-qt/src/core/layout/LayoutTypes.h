#pragma once
#include "core/identity/Uuid.h"

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QRect>
#include <QString>
#include <QStringList>

namespace fincept::layout {

// ── Schema versions ──────────────────────────────────────────────────────────
//
// Each layer carries its own version. Bumping a layer's version invalidates
// only that layer; the loader runs migrations layer-by-layer rather than
// discarding the whole workspace on any change.
//
// When you bump these, add a migration in Workspace::migrate_*_from(...).
inline constexpr int kPanelStateVersion = 1;
inline constexpr int kFrameLayoutVersion = 1;
inline constexpr int kWorkspaceVersion = 1;

// ── MonitorTopologyKey ───────────────────────────────────────────────────────
//
// Stable identifier for "the set of monitors currently connected, regardless
// of which order Windows enumerated them this boot." Two physically identical
// setups produce the same key; plugging or unplugging a monitor produces a
// different key. Used by WorkspaceVariant to pick the right layout for the
// current desk vs laptop topology.
//
// Concrete format: a sorted, comma-joined list of per-monitor stable ids. See
// MonitorTopology::current_key() for the producer.
struct MonitorTopologyKey {
    QString value;

    bool is_empty() const { return value.isEmpty(); }
    bool operator==(const MonitorTopologyKey& other) const { return value == other.value; }
    bool operator!=(const MonitorTopologyKey& other) const { return value != other.value; }

    QJsonValue to_json() const { return value; }
    static MonitorTopologyKey from_json(const QJsonValue& v) { return MonitorTopologyKey{v.toString()}; }
};

inline size_t qHash(const MonitorTopologyKey& k, size_t seed = 0) noexcept {
    return qHash(k.value, seed);
}

// ── PanelState ───────────────────────────────────────────────────────────────
//
// The serialised form of one panel inside a frame. The state_blob is what
// IStatefulScreen::save_state() returned, opaque to the layout layer — only
// the panel itself knows how to restore it.
//
// Phase 0 ships the type only; persistence is wired up in Phase 4 (UUID
// keying) and Phase 6 (the workspace pipeline).
struct PanelState {
    PanelInstanceId instance_id;     ///< Stable UUID owned by PanelHandle.
    QString type_id;                 ///< e.g. "watchlist", "crypto_trading".
    QString title;                   ///< User-renameable display title.
    QString link_group;              ///< Empty = unlinked. e.g. "red", "green".
    QByteArray state_blob;           ///< Opaque IStatefulScreen state (CBOR/JSON).
    int state_version = 0;           ///< IStatefulScreen::state_version() at save time.
    int schema_version = kPanelStateVersion;

    QJsonObject to_json() const;
    static PanelState from_json(const QJsonObject& obj);
};

// ── FrameLayout ──────────────────────────────────────────────────────────────
//
// One frame's worth of layout: which panels exist, the ADS dock state, the
// frame's chrome flags (focus mode, always-on-top), and the active tab.
//
// The dock_state blob is `CDockManager::saveState()` output. Treated opaquely
// by this layer; ADS owns the format. We bump frame_layout schema version
// only when we change *our* surrounding fields, not when ADS changes its
// internal format (that's handled by the existing kDockLayoutVersion).
struct FrameLayout {
    WindowId window_id;
    QString name;                              ///< Optional user label ("Risk", "Macro").
    QList<PanelState> panels;
    QByteArray dock_state;                     ///< ADS CDockManager::saveState().
    PanelInstanceId active_panel;              ///< Which tab/area is focused.
    bool focus_mode = false;
    bool always_on_top = false;
    int schema_version = kFrameLayoutVersion;

    QJsonObject to_json() const;
    static FrameLayout from_json(const QJsonObject& obj);
};

// ── WorkspaceVariant ─────────────────────────────────────────────────────────
//
// One topology-specific arrangement. A user with a 3-monitor desk + a laptop
// stores two variants of "Morning" — the matcher picks the right one at load
// time based on the currently-connected monitors.
//
// Frame geometries live here, not in FrameLayout, because two variants share
// the same frame *contents* but differ in *placement*.
struct WorkspaceVariant {
    MonitorTopologyKey topology;
    QHash<WindowId, QRect> frame_geometries;   ///< Position/size per frame.
    QHash<WindowId, QString> frame_screens;    ///< StableScreenId per frame.

    QJsonObject to_json() const;
    static WorkspaceVariant from_json(const QJsonObject& obj);
};

// ── Workspace ────────────────────────────────────────────────────────────────
//
// The top-level layout object. One workspace = N frames + their panels + a
// list of topology variants for placement.
//
// Workspace `kind`:
//   "auto"          — the live, continuously-saved current state.
//   "user"          — saved by the user, named, immutable until overwritten.
//   "builtin"       — first-run default templates (Equity, FX, Quant).
//   "crash_recovery"— a snapshot saved by the crash-recovery ring.
struct Workspace {
    LayoutId id;
    QString name;
    QString kind = "auto";
    QString description;
    QString author;                            ///< For shared/exported layouts.
    qint64 created_at_unix = 0;
    qint64 updated_at_unix = 0;
    QString thumbnail_path;                    ///< Relative to layouts/ dir.

    QList<FrameLayout> frames;
    QList<WorkspaceVariant> variants;          ///< One per saved monitor topology.

    /// Per-link-group last context. Restored on workspace load so AAPL stays
    /// red after a restart. Stored opaquely; LinkManager owns the schema.
    QHash<QString, QByteArray> link_state;

    int schema_version = kWorkspaceVersion;

    QJsonObject to_json() const;
    static Workspace from_json(const QJsonObject& obj);

    /// Find the variant whose topology matches `key` exactly. nullptr if none.
    /// The matcher (Phase 6) walks variants for nearest match when no exact
    /// hit; this method is the cheap exact-match fast path.
    const WorkspaceVariant* find_variant_exact(const MonitorTopologyKey& key) const;
};

} // namespace fincept::layout
