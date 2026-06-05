#include "storage/repositories/FeedSubscriptionRepository.h"

#include <QJsonDocument>
#include <QSqlQuery>

namespace fincept {

using feeds::FeedSubscription;

FeedSubscriptionRepository& FeedSubscriptionRepository::instance() {
    static FeedSubscriptionRepository s;
    return s;
}

FeedSubscription FeedSubscriptionRepository::map_row(QSqlQuery& q) {
    FeedSubscription s;
    s.id = q.value(0).toString();
    s.name = q.value(1).toString();
    s.url = q.value(2).toString();
    s.refresh_sec = q.value(3).toInt();
    s.parse_mode = feeds::parse_mode_from(q.value(4).toString());
    s.format = feeds::feed_format_from(q.value(5).toString());
    s.mapping = feeds::FieldMapping::from_json(QJsonDocument::fromJson(q.value(6).toString().toUtf8()).object());
    s.display_mode = feeds::display_mode_from(q.value(7).toString());
    s.headers = feeds::headers_from_json(QJsonDocument::fromJson(q.value(8).toString().toUtf8()).object());
    s.enabled = q.value(9).toBool();
    s.sort_order = q.value(10).toInt();
    s.persist = q.value(11).toBool();
    return s;
}

Result<QVector<FeedSubscription>> FeedSubscriptionRepository::list_all() const {
    return query_list("SELECT id, name, url, refresh_sec, parse_mode, format, field_map, "
                      "display_mode, headers, enabled, sort_order, persist "
                      "FROM feed_subscriptions ORDER BY sort_order ASC, name ASC",
                      {}, map_row);
}

Result<void> FeedSubscriptionRepository::upsert(const FeedSubscription& s) const {
    const QString fm = QString::fromUtf8(QJsonDocument(s.mapping.to_json()).toJson(QJsonDocument::Compact));
    const QString hd =
        QString::fromUtf8(QJsonDocument(feeds::headers_to_json(s.headers)).toJson(QJsonDocument::Compact));
    return exec_write(
        "INSERT INTO feed_subscriptions "
        "(id, name, url, refresh_sec, parse_mode, format, field_map, display_mode, headers, enabled, sort_order, "
        "persist, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now')) "
        "ON CONFLICT(id) DO UPDATE SET "
        "  name=excluded.name, url=excluded.url, refresh_sec=excluded.refresh_sec, "
        "  parse_mode=excluded.parse_mode, format=excluded.format, field_map=excluded.field_map, "
        "  display_mode=excluded.display_mode, headers=excluded.headers, enabled=excluded.enabled, "
        "  sort_order=excluded.sort_order, persist=excluded.persist, updated_at=datetime('now')",
        {s.id, s.name, s.url, s.refresh_sec, feeds::to_token(s.parse_mode), feeds::to_token(s.format), fm,
         feeds::to_token(s.display_mode), hd, s.enabled ? 1 : 0, s.sort_order, s.persist ? 1 : 0});
}

Result<void> FeedSubscriptionRepository::remove(const QString& id) const {
    return exec_write("DELETE FROM feed_subscriptions WHERE id = ?", {id});
}

Result<void> FeedSubscriptionRepository::set_enabled(const QString& id, bool enabled) const {
    return exec_write("UPDATE feed_subscriptions SET enabled = ?, updated_at = datetime('now') WHERE id = ?",
                      {enabled ? 1 : 0, id});
}

} // namespace fincept
