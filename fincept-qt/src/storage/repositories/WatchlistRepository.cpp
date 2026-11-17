#include "storage/repositories/WatchlistRepository.h"

#include "storage/sync/SyncOutbox.h"

#include <QSet>

#include <algorithm>

namespace fincept {

namespace {

// Column list of every stock SELECT. The v054 group column is read only when it
// exists, so a database the migration has not reached yet still loads its lists.
QString wlrepo_stock_columns(bool with_group) {
    return with_group ? QStringLiteral("id, watchlist_id, symbol, name, exchange, notes, sort_order, added_at, "
                                       "COALESCE(group_name, '')")
                      : QStringLiteral("id, watchlist_id, symbol, name, exchange, notes, sort_order, added_at, ''");
}

constexpr const char* kWlRepoAlertColumns = "id, symbol, watchlist_id, kind, threshold, ma_period, enabled, repeat, "
                                            "note, last_triggered_at, trigger_count, created_at";

} // namespace

WatchlistRepository& WatchlistRepository::instance() {
    static WatchlistRepository s;
    return s;
}

Watchlist WatchlistRepository::map_watchlist(QSqlQuery& q) {
    return {
        q.value(0).toString(), q.value(1).toString(), q.value(2).toString(), q.value(3).toString(),
        q.value(4).toInt(),    q.value(5).toBool(),   q.value(6).toString(), q.value(7).toString(),
    };
}

WatchlistStock WatchlistRepository::map_stock(QSqlQuery& q) {
    WatchlistStock s;
    s.id = q.value(0).toInt();
    s.watchlist_id = q.value(1).toString();
    s.symbol = q.value(2).toString();
    s.name = q.value(3).toString();
    s.exchange = q.value(4).toString();
    s.notes = q.value(5).toString();
    s.sort_order = q.value(6).toInt();
    s.added_at = q.value(7).toString();
    s.group_name = q.value(8).toString();
    return s;
}

WatchlistAlert WatchlistRepository::map_alert(QSqlQuery& q) {
    WatchlistAlert a;
    a.id = q.value(0).toLongLong();
    a.symbol = q.value(1).toString();
    a.watchlist_id = q.value(2).toString();
    a.kind = q.value(3).toString();
    a.threshold = q.value(4).toDouble();
    a.ma_period = q.value(5).toInt();
    a.enabled = q.value(6).toInt() != 0;
    a.repeat = q.value(7).toInt() != 0;
    a.note = q.value(8).toString();
    a.last_triggered_at = q.value(9).toString();
    a.trigger_count = q.value(10).toInt();
    a.created_at = q.value(11).toString();
    return a;
}

bool WatchlistRepository::has_groups_column() {
    int v = groups_column_.load();
    if (v < 0) {
        v = 0;
        auto r = db().execute(QStringLiteral("PRAGMA table_info(watchlist_stocks)"));
        if (r.is_ok()) {
            auto& q = r.value();
            while (q.next()) {
                if (q.value(1).toString() == QLatin1String("group_name")) {
                    v = 1;
                    break;
                }
            }
        }
        groups_column_.store(v);
    }
    return v == 1;
}

bool WatchlistRepository::has_alerts_table() {
    int v = alerts_table_.load();
    if (v < 0) {
        v = 0;
        auto r = db().execute(
            QStringLiteral("SELECT name FROM sqlite_master WHERE type = 'table' AND name = 'watchlist_alerts'"));
        if (r.is_ok() && r.value().next())
            v = 1;
        alerts_table_.store(v);
    }
    return v == 1;
}

// â”€â”€ Lists â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

Result<Watchlist> WatchlistRepository::create(const QString& name, const QString& color) {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    // New lists go to the end of the sidebar order.
    auto r = exec_write("INSERT INTO watchlists (id, name, color, sort_order) "
                        "VALUES (?, ?, ?, (SELECT COALESCE(MAX(sort_order), -1) + 1 FROM watchlists))",
                        {id, name, color});
    if (r.is_err())
        return Result<Watchlist>::err(r.error());
    SyncOutbox::record("watchlist", id, "create");
    return get(id);
}

Result<QVector<Watchlist>> WatchlistRepository::list_all() {
    return query_list("SELECT id, name, description, color, sort_order, is_default, created_at, updated_at "
                      "FROM watchlists ORDER BY sort_order, name",
                      {}, map_watchlist);
}

Result<Watchlist> WatchlistRepository::get(const QString& id) {
    return query_one("SELECT id, name, description, color, sort_order, is_default, created_at, updated_at "
                     "FROM watchlists WHERE id = ?",
                     {id}, map_watchlist);
}

Result<void> WatchlistRepository::update(const Watchlist& w) {
    auto r = exec_write("UPDATE watchlists SET name = ?, description = ?, color = ?, sort_order = ?, "
                        "is_default = ?, updated_at = datetime('now') WHERE id = ?",
                        {w.name, w.description, w.color, w.sort_order, w.is_default ? 1 : 0, w.id});
    if (r.is_ok())
        SyncOutbox::record("watchlist", w.id, "update");
    return r;
}

Result<void> WatchlistRepository::remove(const QString& id) {
    auto r = exec_write("DELETE FROM watchlists WHERE id = ?", {id});
    if (r.is_ok())
        SyncOutbox::record("watchlist", id, "delete");
    return r;
}

Result<void> WatchlistRepository::set_watchlist_order(const QStringList& ids_in_order) {
    QVector<QVariantList> rows;
    rows.reserve(ids_in_order.size());
    for (int i = 0; i < ids_in_order.size(); ++i)
        rows.append(QVariantList{i, ids_in_order.at(i)});
    auto& d = db();
    auto t = d.begin_transaction();
    if (t.is_err())
        return t;
    auto r = d.execute_many(QStringLiteral("UPDATE watchlists SET sort_order = ?, updated_at = datetime('now') "
                                           "WHERE id = ?"),
                            rows);
    if (r.is_err()) {
        d.rollback();
        return r;
    }
    auto c = d.commit();
    if (c.is_err())
        return c;
    for (const QString& id : ids_in_order)
        SyncOutbox::record("watchlist", id, "update");
    return Result<void>::ok();
}

Result<void> WatchlistRepository::set_default(const QString& id) {
    QStringList changed;
    if (auto lists = list_all(); lists.is_ok()) {
        for (const Watchlist& w : lists.value()) {
            if (w.is_default != (w.id == id))
                changed << w.id;
        }
    }
    auto r = exec_write("UPDATE watchlists SET is_default = CASE WHEN id = ? THEN 1 ELSE 0 END, "
                        "updated_at = CASE WHEN is_default != (id = ?) THEN datetime('now') ELSE updated_at END",
                        {id, id});
    if (r.is_ok()) {
        for (const QString& c : changed)
            SyncOutbox::record("watchlist", c, "update");
    }
    return r;
}

Result<QHash<QString, int>> WatchlistRepository::stock_counts() {
    auto r = db().execute(QStringLiteral("SELECT watchlist_id, COUNT(*) FROM watchlist_stocks GROUP BY watchlist_id"));
    if (r.is_err())
        return Result<QHash<QString, int>>::err(r.error());
    QHash<QString, int> out;
    auto& q = r.value();
    while (q.next())
        out.insert(q.value(0).toString(), q.value(1).toInt());
    return Result<QHash<QString, int>>::ok(std::move(out));
}

Result<Watchlist> WatchlistRepository::duplicate(const QString& source_id, const QString& new_name) {
    auto src = get(source_id);
    if (src.is_err())
        return Result<Watchlist>::err(src.error());
    auto stocks = get_stocks(source_id);
    if (stocks.is_err())
        return Result<Watchlist>::err(stocks.error());

    auto created = create(new_name, src.value().color);
    if (created.is_err())
        return created;
    Watchlist w = created.value();
    w.description = src.value().description;
    update(w);

    const bool groups = has_groups_column();
    QVector<QVariantList> rows;
    rows.reserve(stocks.value().size());
    int order = 0;
    for (const WatchlistStock& s : stocks.value()) {
        QVariantList row{w.id, s.symbol, s.name, s.exchange, s.notes, order++};
        if (groups)
            row.append(s.group_name);
        rows.append(row);
    }
    const QString sql = groups ? QStringLiteral("INSERT OR IGNORE INTO watchlist_stocks "
                                                "(watchlist_id, symbol, name, exchange, notes, sort_order, group_name) "
                                                "VALUES (?, ?, ?, ?, ?, ?, ?)")
                               : QStringLiteral("INSERT OR IGNORE INTO watchlist_stocks "
                                                "(watchlist_id, symbol, name, exchange, notes, sort_order) "
                                                "VALUES (?, ?, ?, ?, ?, ?)");
    auto& d = db();
    auto t = d.begin_transaction();
    if (t.is_err())
        return Result<Watchlist>::err(t.error());
    auto r = d.execute_many(sql, rows);
    if (r.is_err()) {
        d.rollback();
        remove(w.id); // no half-copied list left behind
        return Result<Watchlist>::err(r.error());
    }
    auto c = d.commit();
    if (c.is_err())
        return Result<Watchlist>::err(c.error());
    for (const WatchlistStock& s : stocks.value())
        SyncOutbox::record("watchlist", w.id, "stock_add", s.symbol.toUpper());
    return get(w.id);
}

Result<int> WatchlistRepository::merge_into(const QString& source_id, const QString& target_id) {
    if (source_id == target_id)
        return Result<int>::ok(0);
    auto src = get_stocks(source_id);
    if (src.is_err())
        return Result<int>::err(src.error());
    auto dst = get_stocks(target_id);
    if (dst.is_err())
        return Result<int>::err(dst.error());

    QSet<QString> have;
    int next_order = 0;
    for (const WatchlistStock& s : dst.value()) {
        have.insert(s.symbol.toUpper());
        next_order = std::max(next_order, s.sort_order + 1);
    }
    const bool groups = has_groups_column();
    QVector<QVariantList> rows;
    QStringList added;
    for (const WatchlistStock& s : src.value()) {
        const QString sym = s.symbol.toUpper();
        if (have.contains(sym))
            continue;
        have.insert(sym);
        QVariantList row{target_id, sym, s.name, s.exchange, s.notes, next_order++};
        if (groups)
            row.append(s.group_name);
        rows.append(row);
        added << sym;
    }
    if (rows.isEmpty())
        return Result<int>::ok(0);
    const QString sql = groups ? QStringLiteral("INSERT OR IGNORE INTO watchlist_stocks "
                                                "(watchlist_id, symbol, name, exchange, notes, sort_order, group_name) "
                                                "VALUES (?, ?, ?, ?, ?, ?, ?)")
                               : QStringLiteral("INSERT OR IGNORE INTO watchlist_stocks "
                                                "(watchlist_id, symbol, name, exchange, notes, sort_order) "
                                                "VALUES (?, ?, ?, ?, ?, ?)");
    auto& d = db();
    auto t = d.begin_transaction();
    if (t.is_err())
        return Result<int>::err(t.error());
    auto r = d.execute_many(sql, rows);
    if (r.is_err()) {
        d.rollback();
        return Result<int>::err(r.error());
    }
    auto c = d.commit();
    if (c.is_err())
        return Result<int>::err(c.error());
    for (const QString& sym : added)
        SyncOutbox::record("watchlist", target_id, "stock_add", sym);
    return Result<int>::ok(static_cast<int>(added.size()));
}

// â”€â”€ Stocks â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

Result<void> WatchlistRepository::add_stock(const QString& watchlist_id, const QString& symbol, const QString& name,
                                            const QString& exchange) {
    // Appended after the current last entry, so a newly added symbol keeps its
    // place at the end instead of landing in alphabetical order among the rest.
    auto r = exec_write("INSERT OR IGNORE INTO watchlist_stocks (watchlist_id, symbol, name, exchange, sort_order) "
                        "VALUES (?, ?, ?, ?, (SELECT COALESCE(MAX(sort_order), -1) + 1 FROM watchlist_stocks "
                        "WHERE watchlist_id = ?))",
                        {watchlist_id, symbol.toUpper(), name, exchange, watchlist_id});
    if (r.is_ok())
        SyncOutbox::record("watchlist", watchlist_id, "stock_add", symbol.toUpper());
    return r;
}

Result<void> WatchlistRepository::remove_stock(const QString& watchlist_id, const QString& symbol) {
    auto r = exec_write("DELETE FROM watchlist_stocks WHERE watchlist_id = ? AND symbol = ?",
                        {watchlist_id, symbol.toUpper()});
    if (r.is_ok())
        SyncOutbox::record("watchlist", watchlist_id, "stock_remove", symbol.toUpper());
    return r;
}

Result<QVector<WatchlistStock>> WatchlistRepository::get_stocks(const QString& watchlist_id) {
    auto r = db().execute(QStringLiteral("SELECT %1 FROM watchlist_stocks WHERE watchlist_id = ? "
                                         "ORDER BY sort_order, symbol")
                              .arg(wlrepo_stock_columns(has_groups_column())),
                          {watchlist_id});
    if (r.is_err())
        return Result<QVector<WatchlistStock>>::err(r.error());

    QVector<WatchlistStock> result;
    auto& q = r.value();
    while (q.next()) {
        result.append(map_stock(q));
    }
    return Result<QVector<WatchlistStock>>::ok(std::move(result));
}

Result<void> WatchlistRepository::set_stock_order(const QString& watchlist_id, const QStringList& symbols_in_order) {
    QVector<QVariantList> rows;
    rows.reserve(symbols_in_order.size());
    for (int i = 0; i < symbols_in_order.size(); ++i)
        rows.append(QVariantList{i, watchlist_id, symbols_in_order.at(i).toUpper()});
    auto& d = db();
    auto t = d.begin_transaction();
    if (t.is_err())
        return t;
    auto r = d.execute_many(QStringLiteral("UPDATE watchlist_stocks SET sort_order = ? WHERE watchlist_id = ? AND "
                                           "symbol = ?"),
                            rows);
    if (r.is_err()) {
        d.rollback();
        return r;
    }
    return d.commit();
}

Result<void> WatchlistRepository::set_stock_notes(const QString& watchlist_id, const QString& symbol,
                                                  const QString& notes) {
    return exec_write("UPDATE watchlist_stocks SET notes = ? WHERE watchlist_id = ? AND symbol = ?",
                      {notes, watchlist_id, symbol.toUpper()});
}

Result<void> WatchlistRepository::set_stock_group(const QString& watchlist_id, const QStringList& symbols,
                                                  const QString& group) {
    if (!has_groups_column())
        return Result<void>::err("The database has not been upgraded for watchlist groups yet (migration v054).");
    QVector<QVariantList> rows;
    rows.reserve(symbols.size());
    for (const QString& s : symbols)
        rows.append(QVariantList{group, watchlist_id, s.toUpper()});
    auto& d = db();
    auto t = d.begin_transaction();
    if (t.is_err())
        return t;
    auto r = d.execute_many(QStringLiteral("UPDATE watchlist_stocks SET group_name = ? WHERE watchlist_id = ? AND "
                                           "symbol = ?"),
                            rows);
    if (r.is_err()) {
        d.rollback();
        return r;
    }
    return d.commit();
}

Result<void> WatchlistRepository::set_stock_name_if_empty(const QString& watchlist_id, const QString& symbol,
                                                          const QString& name) {
    if (name.trimmed().isEmpty())
        return Result<void>::ok();
    return exec_write("UPDATE watchlist_stocks SET name = ? WHERE watchlist_id = ? AND symbol = ? AND "
                      "(name IS NULL OR name = '')",
                      {name.trimmed(), watchlist_id, symbol.toUpper()});
}

// â”€â”€ Alerts â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

Result<QVector<WatchlistAlert>> WatchlistRepository::list_alerts() {
    if (!has_alerts_table())
        return Result<QVector<WatchlistAlert>>::ok({});
    return query_list_as<WatchlistAlert>(
        QStringLiteral("SELECT %1 FROM watchlist_alerts ORDER BY symbol, id").arg(QLatin1String(kWlRepoAlertColumns)),
        {}, map_alert);
}

Result<qint64> WatchlistRepository::save_alert(const WatchlistAlert& a) {
    if (!has_alerts_table())
        return Result<qint64>::err("The database has not been upgraded for watchlist alerts yet (migration v054).");
    if (a.id == 0) {
        return exec_insert("INSERT INTO watchlist_alerts (symbol, watchlist_id, kind, threshold, ma_period, enabled, "
                           "repeat, note) VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
                           {a.symbol.toUpper(), a.watchlist_id, a.kind, a.threshold, a.ma_period, a.enabled ? 1 : 0,
                            a.repeat ? 1 : 0, a.note});
    }
    auto r = exec_write("UPDATE watchlist_alerts SET symbol = ?, kind = ?, threshold = ?, ma_period = ?, enabled = ?, "
                        "repeat = ?, note = ?, updated_at = datetime('now') WHERE id = ?",
                        {a.symbol.toUpper(), a.kind, a.threshold, a.ma_period, a.enabled ? 1 : 0, a.repeat ? 1 : 0,
                         a.note, a.id});
    if (r.is_err())
        return Result<qint64>::err(r.error());
    return Result<qint64>::ok(a.id);
}

Result<void> WatchlistRepository::remove_alert(qint64 id) {
    if (!has_alerts_table())
        return Result<void>::ok();
    return exec_write("DELETE FROM watchlist_alerts WHERE id = ?", {id});
}

Result<void> WatchlistRepository::mark_alert_triggered(qint64 id, const QString& when_iso_utc, bool disable) {
    if (!has_alerts_table())
        return Result<void>::ok();
    return exec_write("UPDATE watchlist_alerts SET last_triggered_at = ?, trigger_count = trigger_count + 1, "
                      "enabled = CASE WHEN ? THEN 0 ELSE enabled END, updated_at = datetime('now') WHERE id = ?",
                      {when_iso_utc, disable ? 1 : 0, id});
}

} // namespace fincept
