#pragma once
// SearchSources — the process-wide parts of the search box:
//   * installs the built-in rows into CommandRegistry once (CommandSeed: screens, sub-tabs,
//     settings, docs, shell actions — never over a row a screen already registered),
//   * bridges ActionRegistry: every action appears as a row, kept in step on
//     register / replace / unregister (id "action:<id>"),
//   * SAVED providers over snapshots loaded off the GUI thread when the popup opens
//     (watchlists, portfolios, saved layouts, backtest runs, agent runs) and a query
//     provider for notes; DATA rows for every connector in data_connectors.json,
//   * the shared index (rebuilt when the registry changes) and the shared usage store
//     (recent picks, counts, pins — settings key "search.usage", saved debounced).
// GUI thread only.

#include "core/command/CommandIndex.h"
#include "core/command/CommandUsage.h"

#include <QObject>

namespace fincept::ui::search::sources {

/// Seed + bridge + providers, once per process (later calls: nothing).
void ensure_installed();
/// Refresh the saved-item snapshots (off-thread; cheap when fresh). Called when the popup opens.
void refresh_saved();

/// The index over every registry row; rebuilt here when the registry's revision moved.
const command::CommandIndex& index();

/// The user's picks (loaded on first use). Call usage_changed() after a change.
command::CommandUsage& usage();
void usage_changed();

/// Emits saved_changed() when a snapshot arrives (the box re-runs its query, rows stay in place).
class Notifier : public QObject {
    Q_OBJECT
  public:
    static Notifier& instance();
  signals:
    void saved_changed();
};

} // namespace fincept::ui::search::sources
