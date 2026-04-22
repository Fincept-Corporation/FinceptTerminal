#pragma once
#include "core/symbol/SymbolGroup.h"
#include "core/symbol/SymbolRef.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
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
/// Lifetime: process-global. State persists per workspace via to_json() /
/// from_json() — WorkspaceManager serialises the blob into the workspace
/// file so the user's group→symbol assignments come back when the
/// workspace is reloaded.
class SymbolContext : public QObject {
    Q_OBJECT
  public:
    static SymbolContext& instance();

    SymbolRef group_symbol(SymbolGroup g) const;
    bool has_group_symbol(SymbolGroup g) const;

    /// Set the active symbol for a group. Emits group_symbol_changed and
    /// active_symbol_changed. If `ref` matches the existing value the
    /// signals are suppressed (no-op writes don't trigger UI churn).
    void set_group_symbol(SymbolGroup g, const SymbolRef& ref, QObject* source = nullptr);

    /// The most recently touched symbol anywhere in the app (group or not).
    SymbolRef active() const { return active_; }

    /// Reset everything to empty. Called when a fresh workspace is loaded
    /// that has no saved group state.
    void clear();

    QJsonObject to_json() const;
    void from_json(const QJsonObject& o);

  signals:
    /// `source` is whatever QObject* the caller passed into set_group_symbol;
    /// may be nullptr. Subscribers that are also publishers should compare
    /// `source == this` and skip re-publishing.
    void group_symbol_changed(fincept::SymbolGroup g, fincept::SymbolRef ref, QObject* source);
    void active_symbol_changed(fincept::SymbolRef ref, QObject* source);

  private:
    SymbolContext();
    QHash<SymbolGroup, SymbolRef> groups_;
    SymbolRef active_;
};

} // namespace fincept
