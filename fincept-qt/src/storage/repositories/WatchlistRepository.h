#pragma once
#include "storage/repositories/BaseRepository.h"

#include <QHash>
#include <QStringList>
#include <QUuid>

#include <atomic>

namespace fincept {

struct Watchlist {
    QString id;
    QString name;
    QString description;
    QString color;
    int sort_order = 0;
    bool is_default = false;
    QString created_at;
    QString updated_at;
};

struct WatchlistStock {
    int id = 0;
    QString watchlist_id;
    QString symbol;
    QString name;
    QString exchange;
    QString notes;
    int sort_order = 0;
    QString added_at;
    /// User-defined group (migration v054). Empty = ungrouped, and always empty
    /// on a database that has not been migrated yet.
    QString group_name{};
};

/// One watchlist alert (table watchlist_alerts, migration v054). Alerts follow
/// the symbol, not the list: `watchlist_id` is only the list it was made from.
struct WatchlistAlert {
    qint64 id = 0;
    QString symbol;
    QString watchlist_id;
    /// "price_above", "price_below", "change_up", "change_down", "volume_spike",
    /// "high_52w", "low_52w", "ma_cross_above", "ma_cross_below".
    QString kind;
    double threshold = 0; ///< price / percent / volume multiple, by kind
    int ma_period = 0;    ///< 50 or 200 for the moving-average kinds
    bool enabled = true;
    bool repeat = false; ///< re-arm after the condition clears (else disable once fired)
    QString note;
    QString last_triggered_at; ///< ISO-8601 UTC, empty = never
    int trigger_count = 0;
    QString created_at;
};

class WatchlistRepository : public BaseRepository<Watchlist> {
  public:
    static WatchlistRepository& instance();

    // Watchlist CRUD. `color` is stored user data (an identity colour), so the
    // default is a fixed value, the same as the column default.
    Result<Watchlist> create(const QString& name, const QString& color = "#FF6600");
    Result<QVector<Watchlist>> list_all();
    Result<Watchlist> get(const QString& id);
    Result<void> update(const Watchlist& w);
    Result<void> remove(const QString& id);

    /// Persist the sidebar order: sort_order = position in `ids_in_order`.
    Result<void> set_watchlist_order(const QStringList& ids_in_order);
    /// Make `id` the default list (and no other).
    Result<void> set_default(const QString& id);
    /// Number of symbols per list id.
    Result<QHash<QString, int>> stock_counts();
    /// New list `new_name` with a copy of every entry of `source_id` (name,
    /// exchange, notes, group, order). Returns the new list.
    Result<Watchlist> duplicate(const QString& source_id, const QString& new_name);
    /// Add every entry of `source_id` that `target_id` lacks (appended in the
    /// source's order, notes and group kept). Returns how many were added.
    Result<int> merge_into(const QString& source_id, const QString& target_id);

    // Stock items
    /// Adds `symbol` at the end of the list (sort_order = last + 1). A symbol
    /// already on the list is left untouched (its order, notes and group too).
    Result<void> add_stock(const QString& watchlist_id, const QString& symbol, const QString& name = {},
                           const QString& exchange = {});
    Result<void> remove_stock(const QString& watchlist_id, const QString& symbol);
    /// Entries in the list's stored order (sort_order, then symbol).
    Result<QVector<WatchlistStock>> get_stocks(const QString& watchlist_id);

    /// Persist a manual order: sort_order = position in `symbols_in_order`.
    /// Symbols not in the list are ignored; listed symbols missing from the
    /// vector keep their old value (they sort after the renumbered ones only if
    /// that value is larger).
    Result<void> set_stock_order(const QString& watchlist_id, const QStringList& symbols_in_order);
    Result<void> set_stock_notes(const QString& watchlist_id, const QString& symbol, const QString& notes);
    /// Set the custom group of several entries. Fails (and changes nothing)
    /// when the database lacks the v054 column.
    Result<void> set_stock_group(const QString& watchlist_id, const QStringList& symbols, const QString& group);
    /// Stores a name for an entry that has none yet (CSV import, resolver).
    Result<void> set_stock_name_if_empty(const QString& watchlist_id, const QString& symbol, const QString& name);

    // Alerts (empty / errors when the database lacks the v054 table)
    Result<QVector<WatchlistAlert>> list_alerts();
    /// Insert (id == 0) or update; returns the alert id.
    Result<qint64> save_alert(const WatchlistAlert& a);
    Result<void> remove_alert(qint64 id);
    /// Record a firing: last_triggered_at, trigger_count + 1, and enabled = 0 when `disable`.
    Result<void> mark_alert_triggered(qint64 id, const QString& when_iso_utc, bool disable);

    /// Whether the v054 schema (group column, alerts table) is present.
    bool has_groups_column();
    bool has_alerts_table();

  private:
    WatchlistRepository() = default;
    static Watchlist map_watchlist(QSqlQuery& q);
    static WatchlistStock map_stock(QSqlQuery& q);
    static WatchlistAlert map_alert(QSqlQuery& q);

    // -1 = not checked yet, 0 = absent, 1 = present (schema only grows at startup).
    std::atomic<int> groups_column_{-1};
    std::atomic<int> alerts_table_{-1};
};

} // namespace fincept
