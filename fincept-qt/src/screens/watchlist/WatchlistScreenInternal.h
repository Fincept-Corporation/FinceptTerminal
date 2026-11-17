#pragma once
// Internal to the WatchlistScreen_*.cpp files: sidebar item roles and the
// state of one add / import / drop whose symbols are being looked up.

#include "services/equity/EquityResearchModels.h"
#include "services/equity/EquityResearchService.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::screens {

namespace wl_internal {

/// Sidebar QListWidgetItem roles.
inline constexpr int kListIdRole = Qt::UserRole + 1;
inline constexpr int kListColorRole = Qt::UserRole + 2;
inline constexpr int kListCountRole = Qt::UserRole + 3;
inline constexpr int kListDefaultRole = Qt::UserRole + 4;

/// Settings key of the display currency.
inline constexpr const char* kDisplayCcyKey = "watchlist.display_currency";

/// Lookups running at once for one batch (each is a Python call).
inline constexpr int kMaxParallelLookups = 4;

} // namespace wl_internal

/// One ADD press, import or drop. Every symbol is resolved first
/// (EquityResearchService::resolve_symbol: RELIANCE -> RELIANCE.NS, BRK.B ->
/// BRK-B, typos -> "not found" plus candidates); the outcomes are applied
/// together, in input order, once the last lookup has answered.
struct WatchlistAddBatch {
    enum class Source { Typed, Import };
    enum class Outcome { Pending, Found, NotFound, LookupFailed };
    struct Entry {
        QString typed;
        Outcome outcome = Outcome::Pending;
        QString symbol;   ///< resolved symbol (or `typed` when the lookup failed)
        QString name;
        QString exchange;
        QString message;  ///< not found: the script's explanation; lookup failed: why
        QVector<services::equity::SymbolCandidate> candidates;
        QString notes;    ///< import only
        QString group;    ///< import only
    };
    Source source = Source::Typed;
    QString wl_id; ///< the list the batch adds to (the user may switch meanwhile)
    QVector<Entry> entries;
    int pending = 0; ///< lookups not answered yet
    int next = 0;    ///< next entry to look up
    int running = 0; ///< lookups in flight
    QStringList duplicates; ///< already listed, as typed
    QStringList invalid;    ///< not ticker-shaped
    int file_duplicates = 0;
};

} // namespace fincept::screens
