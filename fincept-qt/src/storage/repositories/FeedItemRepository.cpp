#include "storage/repositories/FeedItemRepository.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>

namespace fincept {

using feeds::FeedItem;

FeedItemRepository& FeedItemRepository::instance() {
    static FeedItemRepository s;
    return s;
}

FeedItem FeedItemRepository::map_row(QSqlQuery& q) {
    FeedItem it;
    it.id = q.value(0).toString();
    it.title = q.value(1).toString();
    it.summary = q.value(2).toString();
    it.link = q.value(3).toString();
    it.source = q.value(4).toString();
    it.sort_ts = q.value(5).toLongLong();
    it.time = q.value(6).toString();
    const QJsonObject obj = QJsonDocument::fromJson(q.value(7).toString().toUtf8()).object();
    for (auto i = obj.begin(); i != obj.end(); ++i)
        it.fields.insert(i.key(), i.value().toString());
    return it;
}

Result<void> FeedItemRepository::upsert(const QString& feed_id, const QVector<FeedItem>& items,
                                        qint64 fetched_at) const {
    if (items.isEmpty())
        return Result<void>::ok();

    // One transaction for the whole batch. Without it every row is its own
    // implicit commit — a 50-item RSS refresh cost 50 fsync-bearing commits, on
    // EVERY poll of EVERY subscribed feed. Pattern copied from
    // InstrumentRepository::replace_all / NewsArticleRepository.
    auto tx = db().begin_transaction();
    if (tx.is_err())
        return tx;

    const QString sql = "INSERT INTO feed_items "
                        "(feed_id, item_id, title, summary, link, source, sort_ts, time, fields_json, fetched_at) "
                        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                        "ON CONFLICT(feed_id, item_id) DO NOTHING";

    QVector<QVariantList> rows;
    rows.reserve(items.size());
    for (const auto& it : items) {
        QJsonObject o;
        for (auto i = it.fields.begin(); i != it.fields.end(); ++i)
            o[i.key()] = i.value();
        const QString fj = QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
        rows.append({feed_id, it.id, it.title, it.summary, it.link, it.source, it.sort_ts, it.time, fj, fetched_at});
    }

    auto w = db().execute_many(sql, rows);
    if (w.is_err()) {
        db().rollback();
        LOG_ERROR("FeedItemRepo", QString("upsert failed: %1").arg(QString::fromStdString(w.error())));
        return w;
    }

    auto c = db().commit();
    if (c.is_err()) {
        db().rollback();
        return c;
    }
    return Result<void>::ok();
}

Result<QVector<FeedItem>> FeedItemRepository::recent(const QString& feed_id, int limit) const {
    return query_list("SELECT item_id, title, summary, link, source, sort_ts, time, fields_json "
                      "FROM feed_items WHERE feed_id = ? ORDER BY sort_ts DESC LIMIT ?",
                      {feed_id, limit}, map_row);
}

Result<QVector<FeedItem>> FeedItemRepository::history(const QString& feed_id, qint64 from_ts, qint64 to_ts,
                                                      int limit) const {
    return query_list("SELECT item_id, title, summary, link, source, sort_ts, time, fields_json "
                      "FROM feed_items WHERE feed_id = ? AND sort_ts BETWEEN ? AND ? "
                      "ORDER BY sort_ts DESC LIMIT ?",
                      {feed_id, from_ts, to_ts, limit}, map_row);
}

Result<void> FeedItemRepository::prune(const QString& feed_id, int keep) const {
    return exec_write("DELETE FROM feed_items WHERE feed_id = ? AND item_id NOT IN "
                      "(SELECT item_id FROM feed_items WHERE feed_id = ? ORDER BY sort_ts DESC LIMIT ?)",
                      {feed_id, feed_id, keep});
}

Result<void> FeedItemRepository::clear(const QString& feed_id) const {
    return exec_write("DELETE FROM feed_items WHERE feed_id = ?", {feed_id});
}

} // namespace fincept
