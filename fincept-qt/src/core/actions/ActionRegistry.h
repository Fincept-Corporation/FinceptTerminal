#pragma once
#include "core/actions/ActionDef.h"
#include "core/result/Result.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

namespace fincept {

/// Central registry of all actions exposed by the application.
///
/// Designed to be the single source of truth feeding:
///   - Hotkey binding (KeyConfigManager — Phase 4 Track B).
///   - Command bar / palette parser (Phase 9).
///   - Help / cheat-sheet view (Phase 9).
///   - Telemetry log (Phase 10).
///   - Macros + replay (post-v1).
///
/// Phase 0 ships the **skeleton only**: register_action / find / match /
/// invoke. The registry is empty at boot; KeyConfigManager keeps doing what
/// it does today. Phase 4 lands `builtin_actions.cpp` which registers the
/// ~30 actions currently hard-coded in WindowFrame and switches the hotkey
/// layer to read from here.
///
/// Threading: registry is touched only from the UI thread. Internal storage
/// is plain QHash; no locking. Touching from another thread is a bug.
class ActionRegistry : public QObject {
    Q_OBJECT
  public:
    static ActionRegistry& instance();

    /// Register a new action. Idempotent on the action `id` — registering
    /// the same id twice replaces the previous entry and emits
    /// `action_replaced(id)` so consumers can re-bind. Returns false if the
    /// id is empty (no-op).
    bool register_action(ActionDef def);

    /// Remove an action by id. Returns true if anything was removed.
    /// Emits `action_unregistered(id)` so hotkey layer + palette refresh.
    bool unregister_action(const QString& id);

    /// Look up by exact id. Returns nullptr if unknown. Pointer is invalid
    /// after the next register_action() with the same id (replacement) or
    /// unregister_action(id) — callers should not store it across event
    /// loop ticks.
    const ActionDef* find(const QString& id) const;

    /// Whether an id is currently registered.
    bool contains(const QString& id) const { return find(id) != nullptr; }

    /// Return all action ids in insertion order. Stable enough for tests
    /// and for help/cheat-sheet rendering.
    QStringList all_ids() const;

    /// Count of registered actions. Cheap.
    int size() const;

    /// Match actions whose id, display, or any alias starts with `prefix`
    /// (case-insensitive). Used by the command bar / palette autocomplete.
    /// Up to `max_results` results, ordered by simple relevance:
    ///   1. exact id match
    ///   2. id starts-with
    ///   3. display starts-with
    ///   4. any alias starts-with
    ///
    /// Phase 0 ships the API — Phase 9 swaps the body for a real fuzzy
    /// matcher over `SuggestionIndex`. Existing call sites won't change.
    QList<const ActionDef*> match(const QString& prefix, int max_results = 25) const;

    /// Invoke an action by id with the given context. Runs the predicate
    /// first; if it fails, returns an error Result without calling the
    /// handler. Empty handler is treated as success (no-op placeholder).
    Result<void> invoke(const QString& id, const CommandContext& ctx) const;

  signals:
    void action_registered(const QString& id);
    void action_replaced(const QString& id);
    void action_unregistered(const QString& id);

  private:
    ActionRegistry() = default;

    QHash<QString, ActionDef> by_id_;
    QStringList insertion_order_;
};

} // namespace fincept
