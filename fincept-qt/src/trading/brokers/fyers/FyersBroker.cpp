// Fyers Broker — Qt-adapted port

#include "trading/brokers/fyers/FyersBroker.h"

#include "core/logging/Logger.h"
#include "trading/brokers/BrokerHttp.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>

namespace fincept::trading {

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// ============================================================================
// Auth
// ============================================================================

QMap<QString, QString> FyersBroker::auth_headers(const BrokerCredentials& creds) const {
    return {{"Authorization", creds.api_key + ":" + creds.access_token},
            {"Content-Type", "application/json"},
            {"Accept", "application/json"},
            {"version", "3"}};
}

TokenExchangeResponse FyersBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                  const QString& auth_code) {
    QByteArray input = (api_key + ":" + api_secret).toUtf8();
    QString hash = QCryptographicHash::hash(input, QCryptographicHash::Sha256).toHex();

    QJsonObject payload{{"grant_type", "authorization_code"}, {"appIdHash", hash}, {"code", auth_code}};

    auto resp = BrokerHttp::instance().post_json(QString(base_url()) + "/api/v3/validate-authcode", payload);

    TokenExchangeResponse result;
    if (!resp.success) {
        result.error = resp.error;
        return result;
    }

    if (resp.json.value("s").toString() == "ok") {
        result.success = true;
        result.access_token = resp.json.value("access_token").toString();
        result.refresh_token = resp.json.value("refresh_token").toString();
        result.user_id = resp.json.value("user_id").toString();
    } else {
        result.error = resp.json.value("message").toString("Token exchange failed");
    }
    return result;
}

// ============================================================================
// Order Type Mappings
// ============================================================================

const BrokerEnumMap<int>& FyersBroker::fyers_int_map() {
    static const auto m = [] {
        BrokerEnumMap<int> x;
        x.set(OrderType::Limit, 1);
        x.set(OrderType::Market, 2);
        x.set(OrderType::StopLoss, 3);
        x.set(OrderType::StopLossLimit, 4);
        x.set(OrderSide::Buy, 1);
        x.set(OrderSide::Sell, -1);
        return x;
    }();
    return m;
}

const BrokerEnumMap<QString>& FyersBroker::fyers_str_map() {
    static const auto m = [] {
        BrokerEnumMap<QString> x;
        x.set(ProductType::Intraday, "INTRADAY");
        x.set(ProductType::Delivery, "CNC");
        x.set(ProductType::Margin, "MARGIN");
        x.set(ProductType::MTF, "MTF");
        x.set(ProductType::CoverOrder, "CO");
        x.set(ProductType::BracketOrder, "BO");
        return x;
    }();
    return m;
}

// ============================================================================
// Place Order
// ============================================================================

OrderPlaceResponse FyersBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    QJsonObject payload{{"symbol", order.symbol},
                        {"qty", static_cast<int>(order.quantity)},
                        {"type", fyers_int_map().order_type_or(order.order_type, 2)},
                        {"side", fyers_int_map().side_or(order.side, 1)},
                        {"productType", fyers_str_map().product_or(order.product_type, "INTRADAY")},
                        {"validity", order.validity},
                        {"offlineOrder", false}};

    if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit)
        payload["limitPrice"] = order.price;
    if (order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit)
        payload["stopPrice"] = order.stop_price;
    if (order.stop_loss > 0)
        payload["stopLoss"] = order.stop_loss;
    if (order.take_profit > 0)
        payload["takeProfit"] = order.take_profit;

    auto resp =
        BrokerHttp::instance().post_json(QString(base_url()) + "/api/v3/orders/sync", payload, auth_headers(creds));

    OrderPlaceResponse result;
    if (!resp.success) {
        result.error = resp.error;
        return result;
    }

    if (resp.json.value("s").toString() == "ok") {
        result.success = true;
        result.order_id = resp.json.value("id").toString();
    } else {
        result.error = resp.json.value("message").toString("Order placement failed");
    }
    return result;
}

// ============================================================================
// Modify / Cancel
// ============================================================================

ApiResponse<QJsonObject> FyersBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                   const QJsonObject& modifications) {
    QJsonObject payload = modifications;
    payload["id"] = order_id;
    auto resp =
        BrokerHttp::instance().patch_json(QString(base_url()) + "/api/v3/orders/sync", payload, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() == "ok")
        return {true, resp.json, "", ts};
    return {false, std::nullopt, resp.json.value("message").toString("Modify failed"), ts};
}

ApiResponse<QJsonObject> FyersBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    QJsonObject payload{{"id", order_id}};
    auto resp = BrokerHttp::instance().del(QString(base_url()) + "/api/v3/orders/sync", auth_headers(creds), payload);
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() == "ok")
        return {true, resp.json, "", ts};
    return {false, std::nullopt, resp.json.value("message").toString("Cancel failed"), ts};
}

// ============================================================================
// Orders / Trades / Positions / Holdings / Funds / Quotes / History
// ============================================================================

ApiResponse<QVector<BrokerOrderInfo>> FyersBroker::get_orders(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/api/v3/orders", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("Failed"), ts};

    QVector<BrokerOrderInfo> orders;
    auto arr = resp.json.value("orderBook").toArray();
    for (const auto& v : arr) {
        auto o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("id").toString();
        info.symbol = o.value("symbol").toString();
        info.exchange = o.value("exchange").toString();
        info.side = o.value("side").toInt() == 1 ? "buy" : "sell";
        // Fyers v3 order type enum: 1=Limit, 2=Market, 3=StopMarket, 4=StopLimit
        static const QMap<int, QString> type_map = {
            {1, "LIMIT"}, {2, "MARKET"}, {3, "STOP"}, {4, "STOPLIMIT"}};
        info.order_type = type_map.value(o.value("type").toInt(), QString::number(o.value("type").toInt()));
        info.product_type = o.value("productType").toString();
        info.quantity = o.value("qty").toDouble();
        info.price = o.value("limitPrice").toDouble();
        info.trigger_price = o.value("stopPrice").toDouble();
        info.filled_qty = o.value("filledQty").toDouble();
        info.avg_price = o.value("tradedPrice").toDouble();
        // Fyers v3 order status enum: 1=Cancelled, 2=Traded/Filled, 3=Reserved,
        // 4=Transit, 5=Rejected, 6=Pending/Open, 7=Expired
        static const QMap<int, QString> status_map = {
            {1, "cancelled"}, {2, "complete"}, {3, "reserved"},
            {4, "transit"},   {5, "rejected"}, {6, "open"},      {7, "expired"}};
        info.status = status_map.value(o.value("status").toInt(), QString::number(o.value("status").toInt()));
        info.message = o.value("message").toString();
        orders.append(info);
    }
    return {true, orders, "", ts};
}

ApiResponse<QJsonObject> FyersBroker::get_trade_book(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/api/v3/tradebook", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() == "ok")
        return {true, resp.json, "", ts};
    return {false, std::nullopt, resp.json.value("message").toString("Failed"), ts};
}

ApiResponse<QVector<BrokerPosition>> FyersBroker::get_positions(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/api/v3/positions", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("Failed"), ts};

    QVector<BrokerPosition> positions;
    auto arr = resp.json.value("netPositions").toArray();
    for (const auto& v : arr) {
        auto p = v.toObject();
        BrokerPosition pos;
        pos.symbol = p.value("symbol").toString();
        pos.exchange = p.value("exchange").toString();
        pos.product_type = p.value("productType").toString();
        pos.quantity = p.value("netQty").toDouble();
        pos.avg_price = p.value("netAvg").toDouble();
        pos.ltp = p.value("ltp").toDouble();
        pos.pnl = p.value("pl").toDouble();
        pos.side = pos.quantity >= 0 ? "buy" : "sell";
        positions.append(pos);
    }
    return {true, positions, "", ts};
}

ApiResponse<QVector<BrokerHolding>> FyersBroker::get_holdings(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/api/v3/holdings", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("Failed"), ts};

    QVector<BrokerHolding> holdings;
    auto arr = resp.json.value("holdings").toArray();
    for (const auto& v : arr) {
        auto h = v.toObject();
        BrokerHolding hold;
        hold.symbol = h.value("symbol").toString();
        hold.exchange = h.value("exchange").toString();
        hold.quantity = h.value("quantity").toDouble();
        hold.avg_price = h.value("costPrice").toDouble();
        hold.ltp = h.value("ltp").toDouble();
        hold.invested_value = hold.quantity * hold.avg_price;
        hold.current_value = hold.quantity * hold.ltp;
        hold.pnl = hold.current_value - hold.invested_value;
        hold.pnl_pct = hold.invested_value > 0 ? (hold.pnl / hold.invested_value) * 100 : 0;
        holdings.append(hold);
    }
    return {true, holdings, "", ts};
}

ApiResponse<BrokerFunds> FyersBroker::get_funds(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/api/v3/funds", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("Failed"), ts};

    BrokerFunds funds;
    auto fl = resp.json.value("fund_limit").toArray();
    // Fyers v3 fund_limit ids (stable; titles vary by tier/segment):
    //   1=Total Balance, 2=Utilized Amount, 3=Realized P&L, 4=Limit at start of day,
    //   5=Receivables, 6=Fund Transfer, 7=Adhoc Limit, 8=Notional Cash,
    //   9=Available Balance, 10=Clear Balance
    for (const auto& v : fl) {
        auto item = v.toObject();
        const int id = item.value("id").toInt(-1);
        const double equity = item.value("equityAmount").toDouble();
        const double commodity = item.value("commodityAmount").toDouble();
        const double total = equity + commodity;
        switch (id) {
            case 1:
                funds.total_balance = total;
                break;
            case 2:
                funds.used_margin = total;
                break;
            case 9:
            case 10:
                funds.available_balance = total;
                break;
        }
    }
    funds.raw_data = resp.json;
    return {true, funds, "", ts};
}

ApiResponse<QVector<BrokerQuote>> FyersBroker::get_quotes(const BrokerCredentials& creds,
                                                          const QVector<QString>& symbols) {
    QString syms;
    for (int i = 0; i < symbols.size(); ++i) {
        if (i > 0)
            syms += ",";
        syms += symbols[i];
    }
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/data/quotes?symbols=" + syms, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("Failed"), ts};

    QVector<BrokerQuote> quotes;
    auto d_arr = resp.json.value("d").toArray();
    for (const auto& val : d_arr) {
        auto item = val.toObject();
        if (item.value("s").toString() != "ok")
            continue;
        auto v = item.value("v").toObject();
        BrokerQuote q;
        q.symbol = v.value("symbol").toString();
        q.ltp = v.value("lp").toDouble();
        q.open = v.value("open_price").toDouble();
        q.high = v.value("high_price").toDouble();
        q.low = v.value("low_price").toDouble();
        q.close = v.value("prev_close_price").toDouble();
        q.volume = v.value("volume").toDouble();
        q.change = v.value("ch").toDouble();
        q.change_pct = v.value("chp").toDouble();
        q.bid = v.value("bid").toDouble();
        q.ask = v.value("ask").toDouble();
        q.timestamp = v.value("tt").toVariant().toLongLong();
        quotes.append(q);
    }
    return {true, quotes, "", ts};
}

ApiResponse<QVector<BrokerCandle>> FyersBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                            const QString& resolution, const QString& from_date,
                                                            const QString& to_date) {
    QString url = QString("%1/data/history?symbol=%2&resolution=%3&date_format=1&range_from=%4&range_to=%5&cont_flag=1")
                      .arg(base_url(), symbol, resolution, from_date, to_date);

    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("Failed"), ts};

    QVector<BrokerCandle> candles;
    auto arr = resp.json.value("candles").toArray();
    for (const auto& v : arr) {
        auto c = v.toArray();
        if (c.size() < 6)
            continue;
        BrokerCandle candle;
        candle.timestamp = c[0].toVariant().toLongLong();
        candle.open = c[1].toDouble();
        candle.high = c[2].toDouble();
        candle.low = c[3].toDouble();
        candle.close = c[4].toDouble();
        candle.volume = c[5].toDouble();
        candles.append(candle);
    }
    return {true, candles, "", ts};
}

// ============================================================================
// Market Clock — GET /api/v3/marketStatus
// Response: {s:"ok", marketStatus:[{exchange, segment, status, msg}, ...]}
// We surface a coarse boolean is_open (true if ANY segment is open) and let
// callers inspect raw via subsequent queries if they need per-segment detail.
// ============================================================================

ApiResponse<MarketClock> FyersBroker::get_clock(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/api/v3/marketStatus", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("marketStatus failed"), ts};
    MarketClock clk;
    clk.timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    const auto arr = resp.json.value("marketStatus").toArray();
    for (const auto& v : arr) {
        const QString status = v.toObject().value("status").toString().toUpper();
        if (status == "OPEN" || status == "MARKET_OPEN") {
            clk.is_open = true;
            break;
        }
    }
    return {true, clk, "", ts};
}

// ============================================================================
// GTT — /api/v3/gtt/orders[/sync]
// Fyers GTT body shape:
//   { side, productType, symbol, qty, type, limitPrice, stopPrice,
//     orderInfo: { leg1: { price, qty }, leg2: { ... } } }
// type values: 1=Single, 2=OCO (two-leg). Triggers and limits map to leg1/leg2
// for OCO; Single uses the top-level fields.
// ============================================================================

namespace {

// Local product-string mapping that doesn't depend on the private static.
static QString fyers_product_str(ProductType p) {
    switch (p) {
        case ProductType::Intraday:     return "INTRADAY";
        case ProductType::Delivery:     return "CNC";
        case ProductType::Margin:       return "MARGIN";
        case ProductType::MTF:          return "MTF";
        case ProductType::CoverOrder:   return "CO";
        case ProductType::BracketOrder: return "BO";
    }
    return "INTRADAY";
}

QJsonObject fyers_gtt_body(const GttOrder& g) {
    QJsonObject body;
    body["symbol"] = g.symbol;
    body["productType"] = fyers_product_str(
        g.triggers.isEmpty() ? ProductType::Delivery : g.triggers.first().product);
    body["side"] = (g.triggers.isEmpty() || g.triggers.first().side == OrderSide::Buy) ? 1 : -1;
    body["type"] = (g.type == GttOrderType::OCO) ? 2 : 1;
    if (g.triggers.isEmpty())
        return body;
    body["qty"] = static_cast<int>(g.triggers.first().quantity);

    if (g.type == GttOrderType::OCO && g.triggers.size() > 1) {
        QJsonObject leg1{{"price", g.triggers[0].trigger_price},
                         {"qty", static_cast<int>(g.triggers[0].quantity)}};
        QJsonObject leg2{{"price", g.triggers[1].trigger_price},
                         {"qty", static_cast<int>(g.triggers[1].quantity)}};
        QJsonObject info;
        info["leg1"] = leg1;
        info["leg2"] = leg2;
        body["orderInfo"] = info;
    } else {
        body["stopPrice"] = g.triggers[0].trigger_price;
        body["limitPrice"] = g.triggers[0].limit_price;
    }
    return body;
}

GttOrder fyers_parse_gtt(const QJsonObject& o) {
    GttOrder g;
    g.gtt_id = o.value("id").toString();
    if (g.gtt_id.isEmpty())
        g.gtt_id = QString::number(o.value("id").toInt());
    g.symbol = o.value("symbol").toString();
    g.exchange = g.symbol.section(':', 0, 0);
    g.status = o.value("status").toString().toLower();
    g.created_at = o.value("createdAt").toString();
    g.updated_at = o.value("updatedAt").toString();
    g.type = o.value("type").toInt() == 2 ? GttOrderType::OCO : GttOrderType::Single;
    g.last_price = o.value("lastPrice").toDouble();

    const int side = o.value("side").toInt();
    const auto side_enum = side == 1 ? OrderSide::Buy : OrderSide::Sell;
    const auto info = o.value("orderInfo").toObject();
    if (g.type == GttOrderType::OCO && info.contains("leg1")) {
        const auto l1 = info.value("leg1").toObject();
        const auto l2 = info.value("leg2").toObject();
        GttTrigger t0;
        t0.trigger_price = l1.value("price").toDouble();
        t0.quantity = l1.value("qty").toDouble();
        t0.side = side_enum;
        g.triggers.append(t0);
        GttTrigger t1;
        t1.trigger_price = l2.value("price").toDouble();
        t1.quantity = l2.value("qty").toDouble();
        t1.side = side_enum;
        g.triggers.append(t1);
    } else {
        GttTrigger t0;
        t0.trigger_price = o.value("stopPrice").toDouble();
        t0.limit_price = o.value("limitPrice").toDouble();
        t0.quantity = o.value("qty").toDouble();
        t0.side = side_enum;
        g.triggers.append(t0);
    }
    return g;
}

} // anonymous namespace

GttPlaceResponse FyersBroker::gtt_place(const BrokerCredentials& creds, const GttOrder& order) {
    QJsonObject body = fyers_gtt_body(order);
    auto resp = BrokerHttp::instance().post_json(QString(base_url()) + "/api/v3/gtt/orders/sync", body,
                                                 auth_headers(creds));
    GttPlaceResponse out;
    if (!resp.success) {
        out.error = resp.error;
        return out;
    }
    if (resp.json.value("s").toString() != "ok") {
        out.error = resp.json.value("message").toString("GTT place failed");
        return out;
    }
    out.success = true;
    out.gtt_id = resp.json.value("id").toString();
    if (out.gtt_id.isEmpty())
        out.gtt_id = QString::number(resp.json.value("id").toInt());
    return out;
}

ApiResponse<GttOrder> FyersBroker::gtt_get(const BrokerCredentials& creds, const QString& gtt_id) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/api/v3/gtt/orders?id=" + gtt_id, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("GTT get failed"), ts};
    // Endpoint returns an array even for a single id; pick the matching entry.
    for (const auto& v : resp.json.value("orderBook").toArray()) {
        auto o = v.toObject();
        if (o.value("id").toString() == gtt_id || QString::number(o.value("id").toInt()) == gtt_id)
            return {true, fyers_parse_gtt(o), "", ts};
    }
    return {false, std::nullopt, "GTT id not found", ts};
}

ApiResponse<QVector<GttOrder>> FyersBroker::gtt_list(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/api/v3/gtt/orders", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("GTT list failed"), ts};
    QVector<GttOrder> out;
    for (const auto& v : resp.json.value("orderBook").toArray())
        out.append(fyers_parse_gtt(v.toObject()));
    return {true, out, "", ts};
}

ApiResponse<GttOrder> FyersBroker::gtt_modify(const BrokerCredentials& creds, const QString& gtt_id,
                                              const GttOrder& updated) {
    QJsonObject body = fyers_gtt_body(updated);
    body["id"] = gtt_id;
    auto resp = BrokerHttp::instance().patch_json(QString(base_url()) + "/api/v3/gtt/orders/sync", body,
                                                  auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("GTT modify failed"), ts};
    // Re-fetch to get the modified state.
    return gtt_get(creds, gtt_id);
}

ApiResponse<QJsonObject> FyersBroker::gtt_cancel(const BrokerCredentials& creds, const QString& gtt_id) {
    QJsonObject body{{"id", gtt_id}};
    auto resp = BrokerHttp::instance().del(QString(base_url()) + "/api/v3/gtt/orders/sync", auth_headers(creds), body);
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("GTT cancel failed"), ts};
    return {true, resp.json, "", ts};
}

// ============================================================================
// Multi-Order — POST /api/v3/multi-order/sync
// Body: array of order objects (same shape as single place_order). Up to 10 legs.
// ============================================================================

ApiResponse<QJsonObject> FyersBroker::place_multi_order(const BrokerCredentials& creds,
                                                       const QVector<UnifiedOrder>& orders) {
    int64_t ts = now_ts();
    if (orders.isEmpty())
        return {false, std::nullopt, "place_multi_order: empty order list", ts};
    if (orders.size() > 10)
        return {false, std::nullopt, "place_multi_order: max 10 legs per batch", ts};

    QJsonArray payload;
    for (const auto& order : orders) {
        QJsonObject leg{{"symbol", order.symbol},
                        {"qty", static_cast<int>(order.quantity)},
                        {"type", fyers_int_map().order_type_or(order.order_type, 2)},
                        {"side", fyers_int_map().side_or(order.side, 1)},
                        {"productType", fyers_str_map().product_or(order.product_type, "INTRADAY")},
                        {"validity", order.validity},
                        {"offlineOrder", order.amo}};
        if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit)
            leg["limitPrice"] = order.price;
        if (order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit)
            leg["stopPrice"] = order.stop_price;
        payload.append(leg);
    }
    auto resp = BrokerHttp::instance().post_json_array(
        QString(base_url()) + "/api/v3/multi-order/sync", payload, auth_headers(creds));
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("multi-order failed"), ts};
    return {true, resp.json, "", ts};
}

} // namespace fincept::trading
