#include "storage/repositories/OrderBasketRepository.h"

#include "core/logging/Logger.h"
#include "trading/ActionCenter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QUuid>

namespace fincept {

namespace {

QString legs_to_json(const QVector<trading::UnifiedOrder>& legs) {
    QJsonArray arr;
    for (const auto& leg : legs)
        arr.append(trading::ActionCenter::serialize_unified_order(leg));
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QVector<trading::UnifiedOrder> legs_from_json(const QString& json) {
    QVector<trading::UnifiedOrder> legs;
    const auto doc = QJsonDocument::fromJson(json.toUtf8());
    for (const auto& v : doc.array())
        legs.append(trading::ActionCenter::deserialize_unified_order(v.toObject()));
    return legs;
}

} // namespace

OrderBasketRepository& OrderBasketRepository::instance() {
    static OrderBasketRepository inst;
    return inst;
}

QVector<OrderBasket> OrderBasketRepository::list_all() const {
    QVector<OrderBasket> out;
    auto r = db().execute("SELECT id, name, legs FROM order_baskets ORDER BY name", {});
    if (r.is_err()) {
        LOG_ERROR("BasketRepo", QString("list_all failed: %1").arg(QString::fromStdString(r.error())));
        return out;
    }
    auto& q = r.value();
    while (q.next()) {
        OrderBasket b;
        b.id = q.value(0).toString();
        b.name = q.value(1).toString();
        b.legs = legs_from_json(q.value(2).toString());
        out.append(b);
    }
    return out;
}

OrderBasket OrderBasketRepository::save(OrderBasket basket) {
    if (basket.id.isEmpty())
        basket.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto r = db().execute("INSERT INTO order_baskets (id, name, legs) VALUES (?, ?, ?) "
                          "ON CONFLICT(id) DO UPDATE SET name = excluded.name, legs = excluded.legs, "
                          "updated_at = datetime('now')",
                          {basket.id, basket.name, legs_to_json(basket.legs)});
    if (r.is_err())
        LOG_ERROR("BasketRepo", QString("save failed: %1").arg(QString::fromStdString(r.error())));
    return basket;
}

void OrderBasketRepository::remove(const QString& id) {
    auto r = db().execute("DELETE FROM order_baskets WHERE id = ?", {id});
    if (r.is_err())
        LOG_ERROR("BasketRepo", QString("remove failed: %1").arg(QString::fromStdString(r.error())));
}

} // namespace fincept
