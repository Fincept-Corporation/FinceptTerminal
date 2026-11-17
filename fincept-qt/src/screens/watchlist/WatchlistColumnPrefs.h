#pragma once
// WatchlistColumnPrefs — the Watchlist table's saved layout, per list.
//
// A layout is keyed by column ID (WatchlistColumns), never by position:
// visible columns in display order, their widths, the sort column, the
// grouping and its collapsed groups, the active view and whether the detail
// pane is open. Stored in SettingsRepository (category "watchlist"):
//
//   watchlist.layout.<list id>   the list's own layout
//   watchlist.layout.default     the last layout saved — new lists start from it
//   watchlist.table_columns      the pre-v2 index-keyed column prefs (read once,
//                                migrated, never modified or deleted)

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace fincept::screens {

struct WatchlistLayout {
    QStringList visible;        ///< column ids in display order ("symbol" always first)
    QHash<QString, int> widths; ///< column id -> width in px
    QString sort_id;            ///< empty = the list's manual order
    Qt::SortOrder sort_order = Qt::AscendingOrder;
    QString grouping = QStringLiteral("none");
    QStringList collapsed;      ///< group keys the user collapsed
    QString view = QStringLiteral("table"); ///< "table" | "map" | "correlation"
    bool detail_visible = true;
    QString map_size = QStringLiteral("cap");      ///< heatmap: "cap" | "equal"
    QString map_horizon = QStringLiteral("1d");    ///< heatmap colour: "1d" | "1w" | "1m" | "3m" | "ytd" | "1y"

    QJsonObject to_json() const;
    /// Unknown column ids are dropped; an empty / invalid object gives an empty layout.
    static WatchlistLayout from_json(const QJsonObject& o);
    bool is_valid() const { return !visible.isEmpty(); }
};

class WatchlistColumnPrefs {
  public:
    /// The default column set (registry defaults), manual order, no grouping.
    static WatchlistLayout builtin_default();
    /// Translate the pre-v2 index-keyed prefs ({"widths": {"0": 100}, "hidden": [5],
    /// "sort_col": 2, "sort_order": 1}) to an id-keyed layout. Columns the user
    /// had hidden stay hidden, visible ones stay visible with their widths; the
    /// new columns are added at their default places.
    static WatchlistLayout from_legacy(const QJsonObject& legacy);

    /// Layout for `list_id`: its own, else the last saved default, else the
    /// migrated pre-v2 prefs, else builtin_default(). Reads SettingsRepository
    /// (one small row; GUI thread is fine).
    static WatchlistLayout load(const QString& list_id);
    /// Save for `list_id` (and as the default for lists without their own).
    /// Asynchronous; debounce at the call site.
    static void save(const QString& list_id, const WatchlistLayout& layout);
    /// Drop `list_id`'s own layout (it follows the default again). Asynchronous.
    static void forget(const QString& list_id);

    static QString list_key(const QString& list_id);
    static constexpr const char* kDefaultKey = "watchlist.layout.default";
    static constexpr const char* kLegacyKey = "watchlist.table_columns";
    static constexpr const char* kCategory = "watchlist";
};

} // namespace fincept::screens
