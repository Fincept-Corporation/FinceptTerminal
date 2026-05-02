#pragma once
#include "core/identity/Uuid.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

namespace fincept {

class PanelHandle;
class WindowFrame;

/// Process-singleton registry of every `PanelHandle` in the shell.
///
/// Indexed by:
///   - instance UUID (primary key, every lookup goes through this)
///   - type id (e.g. "watchlist") — find_by_type
///   - frame id (the WindowFrame the panel currently lives in) — find_by_frame
///   - link group ("red", "green", ...) — find_by_group
///
/// Indices are maintained on register / unregister / set_frame_id /
/// set_link_group / set_type_id. The handles emit signals on each mutation;
/// the registry connects to them at registration time and updates the
/// per-attribute index lists.
///
/// Phase 4 ships the registry. Consumers landing later:
///   - Phase 5 (tear-off): re-parents handles between frames.
///   - Phase 7 (linking): walks find_by_group on every LinkManager publish.
///   - Phase 8 (lifecycle): drives Suspended ↔ Active state transitions.
///
/// Threading: UI-thread only. Same constraint as WindowRegistry.
class PanelRegistry : public QObject {
    Q_OBJECT
  public:
    static PanelRegistry& instance();

    /// Take ownership of a handle (the registry parents it). Returns the
    /// pointer back for caller convenience. Idempotent on `instance_id` —
    /// repeat registration replaces the existing entry and emits
    /// `panel_replaced(id)`.
    PanelHandle* register_panel(PanelHandle* handle);

    /// Detach + delete the handle. Emits `panel_removed` before destroying.
    /// No-op if the id isn't registered.
    void unregister_panel(PanelInstanceId id);

    /// Lookup. Returns nullptr if not registered. Pointer remains valid
    /// until unregister_panel(id) for that id.
    PanelHandle* find(PanelInstanceId id) const;

    /// All registered handles in insertion order.
    QList<PanelHandle*> all_panels() const;

    /// Filtered views. Each returns a fresh QList of currently-matching
    /// handles, sorted by insertion order.
    QList<PanelHandle*> find_by_type(const QString& type_id) const;
    QList<PanelHandle*> find_by_frame(WindowId frame_id) const;
    QList<PanelHandle*> find_by_group(const QString& link_group) const;

    int size() const;

  signals:
    void panel_added(PanelInstanceId id);
    void panel_replaced(PanelInstanceId id);
    void panel_removed(PanelInstanceId id);

    /// Coarse signal fired whenever any indexed attribute changes
    /// (frame, group, type). Subscribers that care about specific
    /// attribute changes connect directly to PanelHandle's per-attribute
    /// signals; this is for bulk consumers (status bar, palette).
    void panel_metadata_changed(PanelInstanceId id);

  private:
    PanelRegistry() = default;

    /// Connect a handle's per-attribute signals so the registry's indices
    /// stay coherent. Called from register_panel.
    void wire_handle_signals(PanelHandle* handle);

    /// Primary store: instance_id → handle. Owns lifetime via QObject
    /// parenting (registry is the parent).
    QHash<PanelInstanceId, PanelHandle*> by_id_;

    /// Insertion order, for stable iteration in all_panels().
    QList<PanelInstanceId> insertion_order_;
};

} // namespace fincept
