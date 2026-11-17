#pragma once
// WatchlistColumns — the Watchlist table's column registry.
//
// Every column the table can show, with a STABLE string id. The model always
// exposes all of them (model column = registry index); which are visible, in
// what order and how wide is the user's per-list layout, saved by id
// (WatchlistColumnPrefs), so columns can be added or reordered here later
// without breaking anyone's saved layout.

#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::screens {

/// How a column's cells are rendered.
enum class WatchlistColKind {
    Symbol,    ///< ticker (+ group label on group rows)
    Text,      ///< plain text
    Price,     ///< instrument-formatted price, tick flash
    Change,    ///< signed price change with currency
    ChangePct, ///< signed percent, direction colour
    Pct,       ///< signed percent, direction colour, no flash
    HeatPct,   ///< performance percent on a diverging heat background
    Number,    ///< plain number (P/E, beta)
    Volume,    ///< compact volume
    RelVolume, ///< volume / average volume, data bar
    Money,     ///< compact money (market cap)
    RangeBar,  ///< low..high track with the price marker
    Spark,     ///< sparkline
    Date,      ///< calendar date
    State,     ///< market session state + venue local time
    Alert,     ///< alert indicator
};

/// What a column needs fetched (bit flags).
enum WatchlistColNeed : int {
    kWlNeedNone = 0,
    kWlNeedQuote = 1,
    kWlNeedInfo = 2,
    kWlNeedHistory = 4,
    kWlNeedSpark = 8,
    kWlNeedFx = 16,
};

struct WatchlistColumnDef {
    const char* id;       ///< stable key (saved layouts, CSV header ids)
    const char* header;   ///< header text (translated through WatchlistColumns context)
    const char* tip;      ///< header tooltip (translated)
    const char* category; ///< chooser sub-menu (translated)
    WatchlistColKind kind;
    int needs;            ///< WatchlistColNeed flags
    bool default_visible;
    int width_chars;      ///< default width in average character widths
};

namespace watchlist_columns {

/// The registry, in default display order. Index = model column.
const QVector<WatchlistColumnDef>& all();
int count();
/// Registry index of `id`, or -1.
int index_of(const QString& id);
QString id_at(int column);
const WatchlistColumnDef& def(int column);

QString header(int column);
QString tooltip(int column);
QString category(int column);
/// Chooser categories, in menu order (translated).
QStringList categories();

/// Ids shown by default, in display order.
QStringList default_visible_ids();

/// Ids of the old fixed 8-column table, by its column index (for migrating
/// index-keyed preferences): SYMBOL NAME PRICE CHANGE CHG% HIGH LOW VOLUME.
QStringList legacy_ids();

/// Id of the symbol column: always visible, never hidden by a layout.
inline constexpr const char* kSymbol = "symbol";

} // namespace watchlist_columns

} // namespace fincept::screens
