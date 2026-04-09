#include "trading/brokers/zerodha/ZerodhaBroker.h"

#include "core/logging/Logger.h"
#include "trading/brokers/BrokerHttp.h"
#include "trading/instruments/InstrumentService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>

namespace fincept::trading {

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

QMap<QString, QString> ZerodhaBroker::auth_headers(const BrokerCredentials& creds) const {
    return {{"Authorization", "token " + creds.api_key + ":" + creds.access_token},
            {"X-Kite-Version", kite_api_version}};
}

const char* ZerodhaBroker::zerodha_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:
            return "MARKET";
        case OrderType::Limit:
            return "LIMIT";
        case OrderType::StopLoss:
            return "SL-M";
        case OrderType::StopLossLimit:
            return "SL";
    }
    return "MARKET";
}
const char* ZerodhaBroker::zerodha_product(ProductType p) {
    switch (p) {
        case ProductType::Intraday:
            return "MIS";
        case ProductType::Delivery:
            return "CNC";
        case ProductType::Margin:
            return "NRML";
        default:
            return "MIS";
    }
}
const char* ZerodhaBroker::zerodha_side(OrderSide s) {
    return s == OrderSide::Buy ? "BUY" : "SELL";
}
const char* ZerodhaBroker::zerodha_variety(ProductType p) {
    switch (p) {
        case ProductType::CoverOrder:
            return "co";
        case ProductType::BracketOrder:
            return "bo";
        default:
            return "regular";
    }
}

QString ZerodhaBroker::zerodha_interval(const QString& resolution) {
    static const QMap<QString, QString> map = {
        {"1", "minute"},    {"1m", "minute"},    {"3", "3minute"},    {"3m", "3minute"},   {"5", "5minute"},
        {"5m", "5minute"},  {"10", "10minute"},  {"10m", "10minute"}, {"15", "15minute"},  {"15m", "15minute"},
        {"30", "30minute"}, {"30m", "30minute"}, {"60", "60minute"},  {"60m", "60minute"}, {"1h", "60minute"},
        {"D", "day"},       {"1D", "day"},       {"day", "day"},      {"W", "week"},       {"1W", "week"},
        {"week", "week"},   {"2h", "2hour"},     {"2H", "2hour"},     {"3d", "3day"},      {"3D", "3day"},
    };
    return map.value(resolution, "day");
}

bool ZerodhaBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 403)
        return true;
    // Zerodha also returns 200 with error_type "TokenException" in some cases
    if (resp.json.value("error_type").toString() == "TokenException")
        return true;
    return false;
}

// Returns the error message, prefixed "[TOKEN_EXPIRED] " when the token has expired.
// Covers both JSON-body errors (status != success) and network/HTTP errors (!resp.success).
QString ZerodhaBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    // Prefer message from JSON body; fall back to network-level error or caller's fallback
    QString msg = resp.json.value("message").toString(resp.error.isEmpty() ? fallback : resp.error);
    if (is_token_expired(resp))
        return "[TOKEN_EXPIRED] " + msg;
    return msg;
}

TokenExchangeResponse ZerodhaBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                    const QString& request_token) {
    QByteArray input = (api_key + request_token + api_secret).toUtf8();
    QString checksum = QCryptographicHash::hash(input, QCryptographicHash::Sha256).toHex();

    QMap<QString, QString> params = {{"api_key", api_key}, {"request_token", request_token}, {"checksum", checksum}};
    QMap<QString, QString> headers = {{"X-Kite-Version", kite_api_version}};
    auto resp = BrokerHttp::instance().post_form(QString(base_url()) + "/session/token", params, headers);

    TokenExchangeResponse result;
    if (!resp.success) {
        result.error = resp.error;
        return result;
    }
    if (resp.json.value("status").toString() == "success") {
        auto data = resp.json.value("data").toObject();
        result.success = true;
        result.access_token = data.value("access_token").toString();
        result.refresh_token = data.value("refresh_token").toString();
        result.user_id = data.value("user_id").toString();
    } else {
        result.error = resp.json.value("message").toString("Token exchange failed");
    }
    return result;
}

OrderPlaceResponse ZerodhaBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    QString variety = zerodha_variety(order.product_type);
    QMap<QString, QString> params = {
        {"tradingsymbol", order.symbol},
        {"exchange", order.exchange},
        {"transaction_type", zerodha_side(order.side)},
        {"order_type", zerodha_order_type(order.order_type)},
        {"quantity", QString::number(static_cast<int>(order.quantity))},
        {"product", zerodha_product(order.product_type)},
        {"validity", order.validity.isEmpty() ? "DAY" : order.validity},
        {"disclosed_quantity", "0"},
        {"tag", "fincept"},
    };
    if (order.price > 0)
        params["price"] = QString::number(order.price, 'f', 2);
    if (order.stop_price > 0)
        params["trigger_price"] = QString::number(order.stop_price, 'f', 2);

    auto resp =
        BrokerHttp::instance().post_form(QString("%1/orders/%2").arg(base_url(), variety), params, auth_headers(creds));

    OrderPlaceResponse result;
    if (!resp.success || resp.json.value("status").toString() != "success") {
        result.error = checked_error(resp, "Order placement failed");
        return result;
    }
    result.success = true;
    result.order_id = resp.json.value("data").toObject().value("order_id").toString();
    return result;
}

ApiResponse<QJsonObject> ZerodhaBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                     const QJsonObject& mods) {
    QString variety = mods.value("variety").toString("regular");
    QMap<QString, QString> params;
    if (mods.contains("order_type"))
        params["order_type"] = mods.value("order_type").toString();
    if (mods.contains("quantity"))
        params["quantity"] = mods.value("quantity").toString();
    if (mods.contains("price"))
        params["price"] = mods.value("price").toString();
    if (mods.contains("trigger_price"))
        params["trigger_price"] = mods.value("trigger_price").toString();
    if (mods.contains("disclosed_quantity"))
        params["disclosed_quantity"] = mods.value("disclosed_quantity").toString();
    params["validity"] = mods.value("validity").toString("DAY");

    auto resp = BrokerHttp::instance().put_form(QString("%1/orders/%2/%3").arg(base_url(), variety, order_id), params,
                                                auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "Modify failed"), ts};
    return {true, resp.json, "", ts};
}

ApiResponse<QJsonObject> ZerodhaBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    QString variety = "regular";
    if (!creds.additional_data.isEmpty()) {
        auto ad = QJsonDocument::fromJson(creds.additional_data.toUtf8()).object();
        auto v = ad.value("variety").toString();
        if (!v.isEmpty())
            variety = v;
    }
    auto resp =
        BrokerHttp::instance().del(QString("%1/orders/%2/%3").arg(base_url(), variety, order_id), auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "Cancel failed"), ts};
    return {true, resp.json, "", ts};
}

ApiResponse<QVector<BrokerOrderInfo>> ZerodhaBroker::get_orders(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/orders", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, resp.error), ts};
    QVector<BrokerOrderInfo> orders;
    auto arr = resp.json.value("data").toArray();
    for (const auto& v : arr) {
        auto o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("order_id").toString();
        info.symbol = o.value("tradingsymbol").toString();
        info.exchange = o.value("exchange").toString();
        info.side = o.value("transaction_type").toString();
        info.order_type = o.value("order_type").toString();
        info.product_type = o.value("product").toString();
        info.quantity = o.value("quantity").toDouble();
        info.price = o.value("price").toDouble();
        info.trigger_price = o.value("trigger_price").toDouble();
        info.filled_qty = o.value("filled_quantity").toDouble();
        info.avg_price = o.value("average_price").toDouble();
        info.status = o.value("status").toString();
        info.timestamp = o.value("order_timestamp").toString();
        info.message = o.value("status_message").toString();
        orders.append(info);
    }
    return {true, orders, "", ts};
}

ApiResponse<QJsonObject> ZerodhaBroker::get_trade_book(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/trades", auth_headers(creds));
    int64_t ts = now_ts();
    return resp.success ? ApiResponse<QJsonObject>{true, resp.json, "", ts}
                        : ApiResponse<QJsonObject>{false, std::nullopt, checked_error(resp, resp.error), ts};
}

ApiResponse<QVector<BrokerPosition>> ZerodhaBroker::get_positions(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/portfolio/positions", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, resp.error), ts};
    QVector<BrokerPosition> positions;
    auto net = resp.json.value("data").toObject().value("net").toArray();
    for (const auto& v : net) {
        auto p = v.toObject();
        BrokerPosition pos;
        pos.symbol = p.value("tradingsymbol").toString();
        pos.exchange = p.value("exchange").toString();
        pos.product_type = p.value("product").toString();
        pos.quantity = p.value("quantity").toDouble();
        pos.avg_price = p.value("average_price").toDouble();
        pos.ltp = p.value("last_price").toDouble();
        pos.pnl = p.value("pnl").toDouble();
        pos.day_pnl = p.value("m2m_pnl").toDouble();
        pos.side = pos.quantity >= 0 ? "BUY" : "SELL";
        double cost = std::abs(pos.avg_price * pos.quantity);
        pos.pnl_pct = cost > 0 ? (pos.pnl / cost) * 100.0 : 0.0;
        positions.append(pos);
    }
    return {true, positions, "", ts};
}

ApiResponse<QVector<BrokerHolding>> ZerodhaBroker::get_holdings(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/portfolio/holdings", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, resp.error), ts};
    QVector<BrokerHolding> holdings;
    auto arr = resp.json.value("data").toArray();
    for (const auto& v : arr) {
        auto h = v.toObject();
        BrokerHolding hold;
        hold.symbol = h.value("tradingsymbol").toString();
        hold.exchange = h.value("exchange").toString();
        hold.quantity = h.value("quantity").toDouble();
        hold.avg_price = h.value("average_price").toDouble();
        hold.ltp = h.value("last_price").toDouble();
        hold.invested_value = hold.quantity * hold.avg_price;
        hold.current_value = hold.quantity * hold.ltp;
        hold.pnl = h.value("pnl").toDouble();
        hold.pnl_pct = hold.avg_price > 0 ? ((hold.ltp - hold.avg_price) / hold.avg_price) * 100.0 : 0.0;
        holdings.append(hold);
    }
    return {true, holdings, "", ts};
}

ApiResponse<BrokerFunds> ZerodhaBroker::get_funds(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/user/margins", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, resp.error), ts};
    auto data = resp.json.value("data").toObject();
    auto equity = data.value("equity").toObject();
    auto commod = data.value("commodity").toObject();
    BrokerFunds funds;
    funds.available_balance = equity.value("net").toDouble() + commod.value("net").toDouble();
    funds.used_margin = equity.value("utilised").toObject().value("debits").toDouble() +
                        commod.value("utilised").toObject().value("debits").toDouble();
    funds.collateral = equity.value("available").toObject().value("collateral").toDouble() +
                       commod.value("available").toObject().value("collateral").toDouble();
    funds.total_balance = funds.available_balance + funds.used_margin;
    funds.raw_data = resp.json;
    return {true, funds, "", ts};
}

ApiResponse<QVector<BrokerQuote>> ZerodhaBroker::get_quotes(const BrokerCredentials& creds,
                                                            const QVector<QString>& symbols) {
    QString query;
    for (const auto& sym : symbols) {
        if (!query.isEmpty())
            query += "&i=";
        query += sym.contains(':') ? sym : ("NSE:" + sym);
    }
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/quote?i=" + query, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, resp.error), ts};
    QVector<BrokerQuote> quotes;
    auto data = resp.json.value("data").toObject();
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        auto q_obj = it.value().toObject();
        BrokerQuote q;
        q.symbol = it.key();
        q.ltp = q_obj.value("last_price").toDouble();
        auto ohlc = q_obj.value("ohlc").toObject();
        q.open = ohlc.value("open").toDouble();
        q.high = ohlc.value("high").toDouble();
        q.low = ohlc.value("low").toDouble();
        q.close = ohlc.value("close").toDouble();
        q.volume = q_obj.value("volume").toDouble();
        q.change = q_obj.value("net_change").toDouble();
        q.change_pct = q.close > 0 ? (q.change / q.close) * 100.0 : 0.0;
        auto depth = q_obj.value("depth").toObject();
        auto buy0 = depth.value("buy").toArray().first().toObject();
        auto sell0 = depth.value("sell").toArray().first().toObject();
        q.bid = buy0.value("price").toDouble();
        q.bid_size = buy0.value("quantity").toDouble();
        q.ask = sell0.value("price").toDouble();
        q.ask_size = sell0.value("quantity").toDouble();
        quotes.append(q);
    }
    return {true, quotes, "", ts};
}

ApiResponse<QVector<BrokerCandle>> ZerodhaBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                              const QString& resolution, const QString& from_date,
                                                              const QString& to_date) {
    QString token;

    QString lookup_exchange = "NSE";
    QString lookup_symbol = symbol;
    if (symbol.contains(':')) {
        lookup_exchange = symbol.section(':', 0, 0);
        lookup_symbol = symbol.section(':', 1);
    }
    auto svc_token = InstrumentService::instance().instrument_token(lookup_symbol, lookup_exchange, "zerodha");
    if (svc_token.has_value())
        token = QString::number(svc_token.value());

    if (token.isEmpty() && !creds.additional_data.isEmpty()) {
        auto ad = QJsonDocument::fromJson(creds.additional_data.toUtf8()).object();
        token = ad.value("instrument_token").toString();
    }

    if (token.isEmpty()) {
        return {false, std::nullopt,
                "Zerodha get_history: instrument_token not found for " + symbol +
                    ". Call InstrumentService::refresh() first.",
                now_ts()};
    }

    QString interval = zerodha_interval(resolution);
    QString url = QString("%1/instruments/historical/%2/%3?from=%4&to=%5&continuous=0&oi=1")
                      .arg(base_url(), token, interval, from_date, to_date);
    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, resp.error), ts};

    QVector<BrokerCandle> candles;
    auto arr = resp.json.value("data").toObject().value("candles").toArray();
    for (const auto& v : arr) {
        auto c = v.toArray();
        if (c.size() < 6)
            continue;
        qint64 epoch = QDateTime::fromString(c[0].toString(), Qt::ISODateWithMs).toSecsSinceEpoch();
        if (epoch == 0)
            epoch = QDateTime::fromString(c[0].toString(), Qt::ISODate).toSecsSinceEpoch();
        candles.append({epoch, c[1].toDouble(), c[2].toDouble(), c[3].toDouble(), c[4].toDouble(), c[5].toDouble()});
    }
    return {true, candles, "", ts};
}

// ============================================================================
// Margin Calculator — Zerodha /margins endpoints
// ============================================================================

// Serialise a UnifiedOrder into the JSON leg format Zerodha expects for margin APIs.
static QJsonObject order_to_margin_leg(const UnifiedOrder& o) {
    QJsonObject leg;
    leg["exchange"] = o.exchange;
    leg["tradingsymbol"] = o.symbol;
    leg["transaction_type"] = (o.side == OrderSide::Buy) ? "BUY" : "SELL";
    leg["variety"] = "regular";
    leg["product"] = (o.product_type == ProductType::Intraday) ? "MIS"
                     : (o.product_type == ProductType::Margin) ? "NRML"
                                                               : "CNC";
    leg["order_type"] = (o.order_type == OrderType::Market)          ? "MARKET"
                        : (o.order_type == OrderType::StopLoss)      ? "SL-M"
                        : (o.order_type == OrderType::StopLossLimit) ? "SL"
                                                                     : "LIMIT";
    leg["quantity"] = int(o.quantity);
    leg["price"] = o.price;
    leg["trigger_price"] = o.stop_price;
    return leg;
}

// Parse a single margin object from Zerodha's response.
static OrderMargin parse_order_margin(const QJsonObject& m) {
    OrderMargin r;
    r.symbol = m.value("tradingsymbol").toString();
    r.exchange = m.value("exchange").toString();
    r.side = m.value("transaction_type").toString();
    r.quantity = m.value("quantity").toDouble();
    r.price = m.value("price").toDouble();
    r.total = m.value("total").toDouble();
    r.var_margin = m.value("var").toDouble();
    r.elm = m.value("elm").toDouble();
    r.additional = m.value("additional").toDouble();
    r.bo_margin = m.value("bo").toDouble();
    r.cash = m.value("cash").toDouble();
    r.pnl = m.value("pnl").toDouble();
    r.leverage = m.value("leverage").toDouble();
    r.error = m.value("error").toString();
    return r;
}

ApiResponse<OrderMargin> ZerodhaBroker::get_order_margins(const BrokerCredentials& creds, const UnifiedOrder& order) {
    QJsonArray arr;
    arr.append(order_to_margin_leg(order));

    auto resp = BrokerHttp::instance().post_json(QString(base_url()) + "/margins/orders", QJsonObject{{"orders", arr}},
                                                 auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "Margin calculation failed"), ts};

    auto data = resp.json.value("data").toArray();
    if (data.isEmpty())
        return {false, std::nullopt, "Empty margin response", ts};
    return {true, parse_order_margin(data.first().toObject()), "", ts};
}

ApiResponse<BasketMargin> ZerodhaBroker::get_basket_margins(const BrokerCredentials& creds,
                                                            const QVector<UnifiedOrder>& orders) {
    QJsonArray arr;
    for (const auto& o : orders)
        arr.append(order_to_margin_leg(o));

    // mode=compact returns per-order breakdown + netting
    QJsonObject payload;
    payload["orders"] = arr;
    payload["mode"] = "compact";

    auto resp = BrokerHttp::instance().post_json(QString(base_url()) + "/margins/basket", payload, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "Basket margin calculation failed"), ts};

    auto data = resp.json.value("data").toObject();
    BasketMargin bm;
    bm.initial_margin = data.value("initial").toDouble();
    bm.final_margin = data.value("final").toDouble();
    for (const auto& v : data.value("orders").toArray())
        bm.orders.append(parse_order_margin(v.toObject()));
    return {true, bm, "", ts};
}

// ============================================================================
// GTT (Good Till Triggered) — Zerodha /gtt/triggers endpoints
// ============================================================================

// Build the JSON body required by Zerodha's GTT create/modify API.
// Zerodha expects:
//   type         : "single" | "two-leg"
//   tradingsymbol: broker native symbol
//   exchange     : "NSE" / "BSE" / etc.
//   trigger_values: [price] for single, [stoploss_price, target_price] for two-leg
//   last_price   : LTP at time of GTT creation
//   orders       : array of order objects (one per leg)
static QJsonObject build_gtt_body(const GttOrder& g) {
    QString type = (g.type == GttOrderType::OCO) ? "two-leg" : "single";

    QJsonArray trigger_values;
    QJsonArray orders;
    for (const auto& t : g.triggers) {
        trigger_values.append(t.trigger_price);
        QJsonObject leg;
        leg["exchange"] = g.exchange;
        leg["tradingsymbol"] = g.symbol;
        leg["transaction_type"] = (t.side == OrderSide::Buy) ? "BUY" : "SELL";
        leg["quantity"] = int(t.quantity);
        leg["order_type"] = (t.order_type == OrderType::Market) ? "MARKET" : "LIMIT";
        leg["product"] = (t.product == ProductType::Intraday) ? "MIS" : "CNC";
        leg["price"] = t.limit_price;
        orders.append(leg);
    }

    QJsonObject body;
    body["type"] = type;
    body["tradingsymbol"] = g.symbol;
    body["exchange"] = g.exchange;
    body["trigger_values"] = trigger_values;
    body["last_price"] = g.last_price;
    body["orders"] = orders;
    return body;
}

// Parse a single GTT object from Zerodha's JSON response.
static GttOrder parse_gtt(const QJsonObject& o) {
    GttOrder g;
    g.gtt_id = QString::number(o.value("id").toInt());
    g.symbol = o.value("tradingsymbol").toString();
    g.exchange = o.value("exchange").toString();
    g.status = o.value("status").toString();
    g.created_at = o.value("created_at").toString();
    g.updated_at = o.value("updated_at").toString();
    g.last_price = o.value("condition").toObject().value("last_price").toDouble();

    QString type = o.value("type").toString();
    g.type = (type == "two-leg") ? GttOrderType::OCO : GttOrderType::Single;

    auto tv = o.value("condition").toObject().value("trigger_values").toArray();
    auto ords = o.value("orders").toArray();
    for (int i = 0; i < ords.size(); ++i) {
        auto leg = ords[i].toObject();
        GttTrigger t;
        t.trigger_price = (i < tv.size()) ? tv[i].toDouble() : 0.0;
        t.limit_price = leg.value("price").toDouble();
        t.quantity = leg.value("quantity").toDouble();
        t.side = (leg.value("transaction_type").toString() == "BUY") ? OrderSide::Buy : OrderSide::Sell;
        t.order_type = (leg.value("order_type").toString() == "MARKET") ? OrderType::Market : OrderType::Limit;
        t.product = (leg.value("product").toString() == "MIS") ? ProductType::Intraday : ProductType::Delivery;
        g.triggers.append(t);
    }
    return g;
}

GttPlaceResponse ZerodhaBroker::gtt_place(const BrokerCredentials& creds, const GttOrder& order) {
    auto resp = BrokerHttp::instance().post_json(QString(base_url()) + "/gtt/triggers", build_gtt_body(order),
                                                 auth_headers(creds));
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, "", checked_error(resp, "GTT place failed")};
    QString id = QString::number(resp.json.value("data").toObject().value("trigger_id").toInt());
    return {true, id, ""};
}

ApiResponse<GttOrder> ZerodhaBroker::gtt_get(const BrokerCredentials& creds, const QString& gtt_id) {
    auto resp = BrokerHttp::instance().get(QString("%1/gtt/triggers/%2").arg(base_url(), gtt_id), auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "GTT get failed"), ts};
    return {true, parse_gtt(resp.json.value("data").toObject()), "", ts};
}

ApiResponse<QVector<GttOrder>> ZerodhaBroker::gtt_list(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/gtt/triggers", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "GTT list failed"), ts};
    QVector<GttOrder> result;
    for (const auto& v : resp.json.value("data").toArray())
        result.append(parse_gtt(v.toObject()));
    return {true, result, "", ts};
}

ApiResponse<GttOrder> ZerodhaBroker::gtt_modify(const BrokerCredentials& creds, const QString& gtt_id,
                                                const GttOrder& updated) {
    auto resp = BrokerHttp::instance().put_json(QString("%1/gtt/triggers/%2").arg(base_url(), gtt_id),
                                                build_gtt_body(updated), auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "GTT modify failed"), ts};
    return {true, parse_gtt(resp.json.value("data").toObject()), "", ts};
}

ApiResponse<QJsonObject> ZerodhaBroker::gtt_cancel(const BrokerCredentials& creds, const QString& gtt_id) {
    auto resp = BrokerHttp::instance().del(QString("%1/gtt/triggers/%2").arg(base_url(), gtt_id), auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "GTT cancel failed"), ts};
    return {true, resp.json, "", ts};
}

} // namespace fincept::trading
