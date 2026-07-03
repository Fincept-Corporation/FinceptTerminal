// ActionCenter.cpp — semi-auto order approval implementation (Phase 3 §2).
#include "trading/ActionCenter.h"

#include "core/logging/Logger.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/sqlite/Database.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/TradingEvents.h"
#include "trading/UnifiedTrading.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QUuid>

#include <cmath>

namespace fincept::trading {

namespace {
constexpr const char* kLog = "ActionCenter";

QString iso(const QDateTime& dt) { return dt.isValid() ? dt.toString(Qt::ISODate) : QString(); }
QDateTime from_iso(const QString& s) { return s.isEmpty() ? QDateTime() : QDateTime::fromString(s, Qt::ISODate); }
} // namespace

ActionCenter& ActionCenter::instance() {
    static ActionCenter s;
    return s;
}

const QSet<QString>& ActionCenter::immediate_ops() {
    // Risk-management + read-only ops always execute immediately, never queued.
    // Mirrors OpenAlgo order_router_service.py IMMEDIATE_EXECUTION_OPERATIONS.
    static const QSet<QString> ops = {
        "closeallpositions", "closeposition", "cancelorder", "cancelallorder",
        "modifyorder", "orderstatus", "orderbook", "tradebook", "positions",
        "holdings", "funds", "openposition", "modifygttorder", "cancelgttorder",
        "gttorderbook",
    };
    return ops;
}

// ── Route decision ─────────────────────────────────────────────────────────

bool ActionCenter::should_queue(const QString& account_id, const QString& order_type) const {
    if (immediate_ops().contains(order_type.toLower()))
        return false;
    return get_order_mode(account_id) == OrderMode::SemiAuto;
}

// ── Mode (persisted in settings) ───────────────────────────────────────────

void ActionCenter::set_order_mode(const QString& account_id, OrderMode mode) {
    const QString key = "action_center.mode." + account_id;
    SettingsRepository::instance().set(key, order_mode_str(mode), "action_center");
    LOG_INFO(kLog, QString("Order mode for %1 set to %2").arg(account_id, order_mode_str(mode)));
    emit stats_updated(account_id);
}

OrderMode ActionCenter::get_order_mode(const QString& account_id) const {
    const QString key = "action_center.mode." + account_id;
    auto r = SettingsRepository::instance().get(key, "auto");
    return parse_order_mode(r.is_ok() ? r.value() : QString("auto"));
}

// ── Serialization ──────────────────────────────────────────────────────────

QJsonObject ActionCenter::serialize_unified_order(const UnifiedOrder& order) {
    QJsonObject j;
    j["symbol"] = order.symbol;
    j["exchange"] = order.exchange;
    j["action"] = order.side == OrderSide::Buy ? "BUY" : "SELL";
    j["quantity"] = order.quantity;
    j["price"] = order.price;
    j["trigger_price"] = order.stop_price;
    j["stop_price"] = order.stop_price;
    // price_type in OpenAlgo terms (MARKET/LIMIT/SL/SL-M).
    QString pt;
    switch (order.order_type) {
        case OrderType::Market: pt = "MARKET"; break;
        case OrderType::Limit: pt = "LIMIT"; break;
        case OrderType::StopLoss: pt = "SL-M"; break;
        case OrderType::StopLossLimit: pt = "SL"; break;
    }
    j["pricetype"] = pt;
    j["product"] = QString::fromLatin1(product_type_str(order.product_type));
    j["validity"] = order.validity;
    j["stop_loss"] = order.stop_loss;
    j["take_profit"] = order.take_profit;
    j["amo"] = order.amo;
    j["instrument_token"] = order.instrument_token;
    return j;
}

QJsonObject ActionCenter::serialize_basket_order(const BasketOrderRequest& basket) {
    QJsonObject j;
    j["strategy"] = basket.strategy_name;
    QJsonArray orders;
    for (const auto& o : basket.orders)
        orders.append(serialize_unified_order(o));
    j["orders"] = orders;
    return j;
}

UnifiedOrder ActionCenter::deserialize_unified_order(const QJsonObject& json) {
    UnifiedOrder o;
    o.symbol = json.value("symbol").toString();
    o.exchange = json.value("exchange").toString();
    o.side = json.value("action").toString().toUpper() == "SELL" ? OrderSide::Sell : OrderSide::Buy;
    o.quantity = json.value("quantity").toDouble();
    o.price = json.value("price").toDouble();
    o.stop_price = json.contains("stop_price") ? json.value("stop_price").toDouble()
                                               : json.value("trigger_price").toDouble();

    const QString pt = json.value("pricetype").toString().toUpper();
    if (pt == "LIMIT")
        o.order_type = OrderType::Limit;
    else if (pt == "SL")
        o.order_type = OrderType::StopLossLimit;
    else if (pt == "SL-M")
        o.order_type = OrderType::StopLoss;
    else
        o.order_type = OrderType::Market;

    const QString prod = json.value("product").toString().toLower();
    if (prod == "delivery" || prod == "cnc")
        o.product_type = ProductType::Delivery;
    else if (prod == "margin" || prod == "nrml")
        o.product_type = ProductType::Margin;
    else if (prod == "cover_order")
        o.product_type = ProductType::CoverOrder;
    else if (prod == "bracket_order")
        o.product_type = ProductType::BracketOrder;
    else if (prod == "mtf")
        o.product_type = ProductType::MTF;
    else
        o.product_type = ProductType::Intraday;

    o.validity = json.value("validity").toString("DAY");
    o.stop_loss = json.value("stop_loss").toDouble();
    o.take_profit = json.value("take_profit").toDouble();
    o.amo = json.value("amo").toBool();
    o.instrument_token = json.value("instrument_token").toString();
    return o;
}

// ── Display fields ─────────────────────────────────────────────────────────

void ActionCenter::populate_display_fields(PendingOrder& po) {
    const QJsonObject& d = po.order_data;
    po.strategy = d.value("strategy").toString("N/A");

    auto qty_str = [](const QJsonValue& v) -> QString {
        if (v.isDouble()) {
            double q = v.toDouble();
            // show ints without trailing .0
            return q == std::floor(q) ? QString::number((qint64)q) : QString::number(q);
        }
        return v.toString();
    };

    if (po.order_type == "basketorder") {
        const QJsonArray orders = d.value("orders").toArray();
        po.symbol = QString("Basket (%1 orders)").arg(orders.size());
        po.exchange = orders.size() > 1 ? "Multiple"
                      : (orders.isEmpty() ? "" : orders.first().toObject().value("exchange").toString());
        po.action = orders.size() > 1 ? "Multiple"
                    : (orders.isEmpty() ? "" : orders.first().toObject().value("action").toString());
        qint64 total = 0;
        for (const auto& v : orders)
            total += (qint64)v.toObject().value("quantity").toDouble();
        po.quantity = QString::number(total);
        po.price_type = orders.size() > 1 ? "Multiple"
                        : (orders.isEmpty() ? "" : orders.first().toObject().value("pricetype").toString("MARKET"));
        return;
    }

    if (po.order_type == "splitorder") {
        po.symbol = d.value("symbol").toString();
        po.exchange = d.value("exchange").toString();
        po.action = d.value("action").toString();
        po.quantity = QString("%1 (split: %2)").arg(qty_str(d.value("quantity")),
                                                     qty_str(d.value("splitsize")));
        po.price_type = d.value("pricetype").toString("MARKET");
        return;
    }

    // placeorder / smartorder / placegttorder default.
    po.symbol = d.value("symbol").toString();
    po.exchange = d.value("exchange").toString();
    po.action = d.value("action").toString();
    po.quantity = qty_str(d.value("quantity"));
    po.price_type = d.value("pricetype").toString(d.value("price_type").toString("MARKET"));
}

// ── Persistence ────────────────────────────────────────────────────────────

QString ActionCenter::queue_order(const QString& account_id, const QString& order_type,
                                  const QJsonObject& order_data) {
    PendingOrder po;
    po.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    po.account_id = account_id;
    po.order_type = order_type;
    // Strip credential-like keys defensively.
    QJsonObject clean = order_data;
    clean.remove("apikey");
    clean.remove("api_key");
    clean.remove("api_secret");
    clean.remove("access_token");
    po.order_data = clean;
    po.status = "pending";
    po.created_at = QDateTime::currentDateTime();
    populate_display_fields(po);

    const QString json_str = QString::fromUtf8(QJsonDocument(po.order_data).toJson(QJsonDocument::Compact));
    auto r = Database::instance().execute(
        "INSERT INTO pending_orders (id, account_id, order_type, order_data, status, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?)",
        {po.id, po.account_id, po.order_type, json_str, po.status, iso(po.created_at)});
    if (r.is_err()) {
        LOG_ERROR(kLog, QString("queue_order failed: %1").arg(QString::fromStdString(r.error())));
        return {};
    }

    LOG_INFO(kLog, QString("Queued %1 order %2 for account %3").arg(order_type, po.id, account_id));
    emit pending_order_created(po);
    emit stats_updated(account_id);
    return po.id;
}

PendingOrder ActionCenter::map_row(QSqlQuery& q) {
    PendingOrder po;
    po.id = q.value("id").toString();
    po.account_id = q.value("account_id").toString();
    po.order_type = q.value("order_type").toString();
    po.order_data = QJsonDocument::fromJson(q.value("order_data").toString().toUtf8()).object();
    po.status = q.value("status").toString();
    po.created_at = from_iso(q.value("created_at").toString());
    po.approved_at = from_iso(q.value("approved_at").toString());
    po.rejected_at = from_iso(q.value("rejected_at").toString());
    po.rejection_reason = q.value("rejection_reason").toString();
    po.broker_order_id = q.value("broker_order_id").toString();
    populate_display_fields(po);
    return po;
}

std::optional<PendingOrder> ActionCenter::get_order(const QString& pending_id) const {
    auto r = Database::instance().execute("SELECT * FROM pending_orders WHERE id = ?", {pending_id});
    if (r.is_err())
        return std::nullopt;
    auto& q = r.value();
    if (!q.next())
        return std::nullopt;
    return map_row(q);
}

QVector<PendingOrder> ActionCenter::get_pending_orders(const QString& account_id) const {
    QVector<PendingOrder> out;
    QString sql = "SELECT * FROM pending_orders WHERE status = 'pending'";
    QVariantList params;
    if (!account_id.isEmpty()) {
        sql += " AND account_id = ?";
        params << account_id;
    }
    sql += " ORDER BY created_at DESC";
    auto r = Database::instance().execute(sql, params);
    if (r.is_err())
        return out;
    auto& q = r.value();
    while (q.next())
        out.append(map_row(q));
    return out;
}

QVector<PendingOrder> ActionCenter::get_all_orders(const QString& account_id, int limit) const {
    QVector<PendingOrder> out;
    QString sql = "SELECT * FROM pending_orders";
    QVariantList params;
    if (!account_id.isEmpty()) {
        sql += " WHERE account_id = ?";
        params << account_id;
    }
    sql += " ORDER BY created_at DESC LIMIT ?";
    params << limit;
    auto r = Database::instance().execute(sql, params);
    if (r.is_err())
        return out;
    auto& q = r.value();
    while (q.next())
        out.append(map_row(q));
    return out;
}

ActionCenter::Stats ActionCenter::get_stats(const QString& account_id) const {
    Stats s;
    // Stats are computed across ALL orders (matches OpenAlgo behaviour: tabs
    // always show correct totals regardless of the active status filter).
    const auto orders = get_all_orders(account_id, 100000);
    for (const auto& o : orders) {
        if (o.status == "pending")
            ++s.total_pending;
        else if (o.status == "approved")
            ++s.total_approved;
        else if (o.status == "rejected")
            ++s.total_rejected;

        const QString act = o.action.toUpper();
        if (act == "BUY")
            ++s.total_buy;
        else if (act == "SELL")
            ++s.total_sell;

        if (o.order_type == "placeorder")
            ++s.by_type_place;
        else if (o.order_type == "smartorder")
            ++s.by_type_smart;
        else if (o.order_type == "basketorder")
            ++s.by_type_basket;
    }
    return s;
}

// ── Execution on approval ──────────────────────────────────────────────────

QString ActionCenter::execute_pending(const PendingOrder& po, bool& ok, QString& err) {
    ok = false;
    if (po.order_type == "placeorder" || po.order_type == "smartorder") {
        // Smart collapses to a standard place via UnifiedTrading for now —
        // the serialized order carries the same fields. (place_smart_order
        // routing can be wired here later without changing the queue/approve
        // contract.)
        UnifiedOrder order = deserialize_unified_order(po.order_data);
        UnifiedOrderResponse resp = UnifiedTrading::instance().place_order(po.account_id, order);
        ok = resp.success;
        err = resp.message;
        return resp.order_id;
    }

    if (po.order_type == "placegttorder") {
        // A GTT is a RESTING trigger at the broker — it must NEVER collapse
        // into an immediate place_order (that fires the order right away).
        // Route to the broker's native GTT API; when the account has no GTT
        // path, fail loudly so the user is not misled into an instant fill.
        auto account = AccountManager::instance().get_account(po.account_id);
        if (account.account_id.isEmpty()) {
            err = "Account not found: " + po.account_id;
            return {};
        }
        if (account.trading_mode == "paper") {
            err = "GTT not supported for paper accounts";
            return {};
        }
        auto* broker = BrokerRegistry::instance().get(account.broker_id);
        if (!broker) {
            err = "Broker not found: " + account.broker_id;
            return {};
        }
        const UnifiedOrder base = deserialize_unified_order(po.order_data);
        if (base.stop_price <= 0.0) {
            err = "GTT order missing trigger_price";
            return {};
        }
        GttOrder gtt;
        gtt.symbol = base.symbol;
        gtt.exchange = base.exchange;
        gtt.type = GttOrderType::Single;
        gtt.last_price = po.order_data.value("last_price").toDouble();
        GttTrigger trig;
        trig.trigger_price = base.stop_price; // deserialize maps trigger_price/stop_price here
        trig.limit_price = base.price;
        trig.quantity = base.quantity;
        trig.side = base.side;
        // GTT legs are Market or Limit only: SL-M fires at market on trigger,
        // Limit / SL carry their limit price.
        trig.order_type = (base.order_type == OrderType::Market || base.order_type == OrderType::StopLoss)
                              ? OrderType::Market
                              : OrderType::Limit;
        trig.product = base.product_type;
        gtt.triggers.append(trig);

        const auto creds = AccountManager::instance().load_credentials(po.account_id);
        // Brokers without a gtt_place override return the base-class
        // "GTT not supported for this broker" — surfaced as a rejection.
        const GttPlaceResponse resp = broker->gtt_place(creds, gtt);
        ok = resp.success;
        err = resp.error;
        return resp.gtt_id;
    }

    if (po.order_type == "basketorder") {
        BasketOrderRequest basket;
        basket.strategy_name = po.order_data.value("strategy").toString();
        const QJsonArray arr = po.order_data.value("orders").toArray();
        for (const auto& v : arr)
            basket.orders.append(deserialize_unified_order(v.toObject()));
        // place_basket_orders is async (background thread + callback). For the
        // approval gate we fire it and report acceptance immediately; per-leg
        // results surface via the trading event bus / order book.
        UnifiedTrading::instance().place_basket_orders(po.account_id, basket,
            [](const BasketOrderResult&) {});
        ok = true;
        err.clear();
        return {}; // no single broker order id for a basket
    }

    if (po.order_type == "splitorder") {
        SplitOrderRequest req;
        req.base_order = deserialize_unified_order(po.order_data);
        req.split_size = po.order_data.value("splitsize").toInt(
            (int)po.order_data.value("split_size").toDouble());
        req.delay_between_ms = po.order_data.value("delay_between_ms").toInt(100);
        UnifiedTrading::instance().place_split_orders(po.account_id, req,
            [](const SplitOrderResult&) {});
        ok = true;
        err.clear();
        return {};
    }

    err = "Unknown order_type: " + po.order_type;
    return {};
}

void ActionCenter::approve_order(const QString& pending_id) {
    auto po_opt = get_order(pending_id);
    if (!po_opt) {
        LOG_WARN(kLog, "approve_order: pending id not found: " + pending_id);
        return;
    }
    PendingOrder po = *po_opt;
    if (po.status != "pending") {
        LOG_WARN(kLog, QString("approve_order: %1 is not pending (status=%2)").arg(pending_id, po.status));
        return;
    }

    bool ok = false;
    QString err;
    const QString broker_order_id = execute_pending(po, ok, err);

    const QDateTime now = QDateTime::currentDateTime();
    auto r = Database::instance().execute(
        "UPDATE pending_orders SET status = ?, approved_at = ?, broker_order_id = ?, "
        "rejection_reason = ? WHERE id = ?",
        {ok ? "approved" : "rejected", iso(now), broker_order_id,
         ok ? QString() : ("Execution failed: " + err), pending_id});
    if (r.is_err())
        LOG_ERROR(kLog, QString("approve_order update failed: %1").arg(QString::fromStdString(r.error())));

    if (ok) {
        LOG_INFO(kLog, QString("Approved %1 → broker order %2").arg(pending_id, broker_order_id));
        emit order_approved(pending_id, broker_order_id);
    } else {
        LOG_WARN(kLog, QString("Approval of %1 executed but broker rejected: %2").arg(pending_id, err));
        emit order_rejected(pending_id, "Execution failed: " + err);
    }
    emit stats_updated(po.account_id);
}

void ActionCenter::reject_order(const QString& pending_id, const QString& reason) {
    auto po_opt = get_order(pending_id);
    if (!po_opt) {
        LOG_WARN(kLog, "reject_order: pending id not found: " + pending_id);
        return;
    }
    PendingOrder po = *po_opt;
    if (po.status != "pending")
        return;

    const QDateTime now = QDateTime::currentDateTime();
    auto r = Database::instance().execute(
        "UPDATE pending_orders SET status = 'rejected', rejected_at = ?, rejection_reason = ? WHERE id = ?",
        {iso(now), reason, pending_id});
    if (r.is_err()) {
        LOG_ERROR(kLog, QString("reject_order update failed: %1").arg(QString::fromStdString(r.error())));
        return;
    }
    LOG_INFO(kLog, QString("Rejected %1: %2").arg(pending_id, reason));
    emit order_rejected(pending_id, reason);
    emit stats_updated(po.account_id);
}

void ActionCenter::approve_all_pending(const QString& account_id) {
    const auto pending = get_pending_orders(account_id);
    for (const auto& po : pending)
        approve_order(po.id);
    if (!pending.isEmpty())
        emit stats_updated(account_id);
}

void ActionCenter::reject_all_pending(const QString& account_id, const QString& reason) {
    const auto pending = get_pending_orders(account_id);
    for (const auto& po : pending)
        reject_order(po.id, reason);
    if (!pending.isEmpty())
        emit stats_updated(account_id);
}

} // namespace fincept::trading
