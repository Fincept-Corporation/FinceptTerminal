#pragma once
#include "core/symbol/SymbolGroup.h"
#include "core/symbol/SymbolRef.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPair>
#include <QString>

namespace fincept {

/// Singleton service holding the "active security" for each symbol group
/// (Bloomberg Launchpad equivalent). Panels published via IGroupLinked
/// listen to group_symbol_changed and update themselves in lockstep.
///
/// The `source` parameter on set_group_symbol() carries the QObject that
/// originated the change; subscribers can compare against `this` to
/// suppress their own re-publish and avoid feedback loops.
///
/// Lifetime: process-global. State persists per layout via to_json() /
/// from_json() — WorkspaceShell stores the blob in
/// `Workspace.link_state["symbol_context"]` so the user's group→symbol
/// assignments come back when the layout is reloaded.
///
/// Phase 7 / decision 3.1 — slot primitive. Every (group, slot) pair holds
/// its own SymbolRef. v1 ships with the single canonical slot
/// `kPrimarySlot` ("primary"); the API is slot-aware so adding additional
/// slots ("comparison", "currency") later is mechanical for both producers
/// and consumers. All legacy (slot-less) APIs route through `kPrimarySlot`
/// transparently — existing IGroupLinked panels need no changes.
class SymbolContext : public QObject {
    Q_OBJECT
  public:
    static SymbolContext& instance();

    /// The default slot name. Single slot in v1; the constant exists so
    /// future named slots can land without grepping for the literal.
    /// Stored as `QLatin1String` so it implicitly converts to QString
    /// without needing `QStringLiteral` (which only accepts string
    /// literals, not `const char*` variables) at every call site.
    static inline const QLatin1String kPrimarySlot{"primary"};

    /// Convenience: legacy, slot-less, primary-slot accessors. Identical
    /// behaviour to the slot-aware overloads with `slot=kPrimarySlot`.
    SymbolRef group_symbol(SymbolGroup g) const;
    bool has_group_symbol(SymbolGroup g) const;
    void set_group_symbol(SymbolGroup g, const SymbolRef& ref, QObject* source = nullptr);

    /// Slot-aware accessors. v1 producers may continue to publish on
    /// the primary slot via the legacy overloads above; producers that
    /// want to push a comparison/currency context use these.
    SymbolRef group_slot_symbol(SymbolGroup g, const QString& slot) const;
    bool has_group_slot_symbol(SymbolGroup g, const QString& slot) const;
    void set_group_slot_symbol(SymbolGroup g, const QString& slot, const SymbolRef& ref,
                               QObject* source = nullptr);

    /// The most recently touched symbol anywhere in the app (group or not).
    SymbolRef active() const { return active_; }

    /// Reset everything to empty. Called when a fresh workspace is loaded
    /// that has no saved group state.
    void clear();

    QJsonObject to_json() const;
    void from_json(const QJsonObject& o);

  signals:
    /// Legacy primary-slot signal. `source` is whatever QObject* the
    /// caller passed into set_group_symbol; may be nullptr. Subscribers
    /// that are also publishers should compare `source == this` and skip
    /// re-publishing. Emitted iff the changed slot is `kPrimarySlot`.
    void group_symbol_changed(fincept::SymbolGroup g, fincept::SymbolRef ref, QObject* source);

    /// Slot-aware signal — fires for every slot change including primary.
    /// Subscribers that want all slots subscribe here; subscribers that
    /// only care about primary can keep using the legacy signal.
    void group_slot_symbol_changed(fincept::SymbolGroup g, QString slot,
                                   fincept::SymbolRef ref, QObject* source);

    void active_symbol_changed(fincept::SymbolRef ref, QObject* source);

  private:
    SymbolContext();

    // Storage keyed by (group, slot). The slot string is owned by the
    // hash key so we don't keep a parallel slot-list table. Default
    // slot is `kPrimarySlot` for legacy callers.
    QHash<QPair<SymbolGroup, QString>, SymbolRef> slot_symbols_;
    SymbolRef active_;
};

} // namespace fincept
