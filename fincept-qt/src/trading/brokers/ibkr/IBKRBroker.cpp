#include "trading/brokers/ibkr/IBKRBroker.h"
#include "trading/brokers/BrokerHttp.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>

namespace fincept::trading {

static int64_t now_ts() { return QDateTime::currentSecsSinceEpoch(); }

// ---------- Static helpers ----------

QString IBKRBroker::gateway_url(const BrokerCredentials& creds) {
    // access_token stores the gateway URL; fall back to default
    QString url = creds.access_token;
    if (url.isEmpty()) url = creds.api_secret;
    if (url.isEmpty()) url = "https://localhost:5000";
    // Strip trailing slash
    while (url.endsWith('/')) url.chop(1);
    return url;
}

QString IBKRBroker::ibkr_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "MKT";
        case OrderType::Limit:         return "LMT";
        case OrderType::StopLoss:      return "STP";
        case OrderType::StopLossLimit: return "STPLMT";
        default:                       return "MKT";
    }
}

QString IBKRBroker::ibkr_tif(ProductType p) {
    // Delivery = GTC, everything else = DAY
    return (p == ProductType::Delivery) ? "GTC" : "DAY";
}

IBKRBroker::HistoryParams IBKRBroker::ibkr_history_params(const QString& resolution,
                                                            const QString& from_date,
                                                            const QString& to_date) {
    // Bar size
    QString bar;
    if      (resolution == "1"  || resolution == "1m")  bar = "1min";
    else if (resolution == "5"  || resolution == "5m")  bar = "5min";
    else if (resolution == "15" || resolution == "15m") bar = "15min";
    else if (resolution == "30" || resolution == "30m") bar = "30min";
    else if (resolution == "60" || resolution == "1h")  bar = "1h";
    else if (resolution == "D"  || resolution == "1D")  bar = "1d";
    else if (resolution == "W")                          bar = "1w";
    else if (resolution == "M")                          bar = "1m";
    else                                                 bar = "1d";

    // Derive period from date range
    QString period = "1m"; // default 1 month
    QDate from = QDate::fromString(from_date, "yyyy-MM-dd");
    QDate to   = QDate::fromString(to_date,   "yyyy-MM-dd");
    if (from.isValid() && to.isValid()) {
        int days = from.daysTo(to);
        if      (days <= 1)   period = "1d";
        else if (days <= 7)   period = "1w";
        else if (days <= 31)  period = "1m";
        else if (days <= 92)  period = "3m";
        else if (days <= 183) period = "6m";
        else if (days <= 365) period = "1y";
        else if (days <= 730) period = "2y";
        else                  period = "5y";
    }

    return {bar, period};
}

bool IBKRBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 401 || resp.status_code == 403) return true;
    if (!resp.success) return false;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject()) return false;
    QJsonObject obj = doc.object();
    // Gateway returns {error: "not authenticated"} or {statusCode: 401}
    QString error = obj.value("error").toString().toLower();
    if (error.contains("not authenticated") || error.contains("unauthorized"))
        return true;
    if (obj.value("statusCode").toInt() == 401)
        return true;
    return false;
}

QString IBKRBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    if (is_token_expired(resp)) return "[TOKEN_EXPIRED] Gateway session not authenticated";
    if (!resp.success) return resp.error.isEmpty() ? fallback : resp.error;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (doc.isObject()) {
        QString msg = doc.object().value("error").toString();
        if (!msg.isEmpty()) return msg;
        msg = doc.object().value("message").toString();
        if (!msg.isEmpty()) return msg;
    }
    return fallback;
}

// ---------- Auth headers ----------
// No Authorization header — gateway uses local browser session cookie

QMap<QString, QString> IBKRBroker::auth_headers(const BrokerCredentials& /*creds*/) const {
    return {{"Content-Type", "application/json"},
            {"Accept",       "application/json"}};
}

// ---------- exchange_token ----------
// No OAuth flow — just validates the gateway is running and authenticated.
// ApiKey    = account ID
// ApiSecret = gateway URL (e.g. "https://localhost:5000")

TokenExchangeResponse IBKRBroker::exchange_token(const QString& api_key,
                                                   const QString& api_secret,
                                                   const QString& /*auth_code*/) {
    QString gw = api_secret.isEmpty() ? "https://localhost:5000" : api_secret;
    while (gw.endsWith('/')) gw.chop(1);

    auto& http = BrokerHttp::instance();
    auto resp = http.get(
        gw + "/v1/api/iserver/auth/status",
        {{"Content-Type", "application/json"}, {"Accept", "application/json"}}
    );

    if (!resp.success)
        return {false, "", "", "", "Gateway not reachable at " + gw + ": " + resp.error};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "", "", "Gateway: invalid auth status response"};

    QJsonObject obj = doc.object();
    bool authenticated = obj.value("authenticated").toBool();
    if (!authenticated) {
        return {false, "", "", "",
                "Gateway is running but not authenticated. "
                "Please log in via the gateway browser interface first."};
    }

    // Store gateway URL as "access_token" — it's the only credential we need at runtime
    return {true, gw, "", api_key, ""};
}

// ---------- place_order ----------

OrderPlaceResponse IBKRBroker::place_order(const BrokerCredentials& creds,
                                             const UnifiedOrder& order) {
    QString gw    = gateway_url(creds);
    QString acct  = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    // conid: symbol format "NASDAQ:AAPL:265598" — conid is the 3rd part
    QString conid = order.instrument_token;
    if (conid.isEmpty()) {
        QStringList parts = order.symbol.split(":");
        if (parts.size() >= 3) conid = parts[2];
    }
    if (conid.isEmpty())
        return {false, "", "IBKR place_order: conid required (symbol format: EXCHANGE:SYMBOL:CONID)"};

    QJsonObject order_obj;
    order_obj["conid"]       = conid.toLongLong();
    order_obj["orderType"]   = ibkr_order_type(order.order_type);
    order_obj["side"]        = (order.side == OrderSide::Buy) ? "BUY" : "SELL";
    order_obj["tif"]         = ibkr_tif(order.product_type);
    order_obj["quantity"]    = order.quantity;
    order_obj["price"]       = order.price;
    if (order.stop_price > 0)
        order_obj["auxPrice"] = order.stop_price;
    order_obj["acctId"]      = acct;

    QJsonObject body;
    body["orders"] = QJsonArray{order_obj};

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(
        gw + "/v1/api/iserver/account/" + acct + "/orders",
        body, auth_headers(creds)
    );

    if (!resp.success)
        return {false, "", checked_error(resp, "place_order failed")};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    // Response is an array: [{order_id, order_status, ...}]
    QJsonArray arr;
    if (doc.isArray())       arr = doc.array();
    else if (doc.isObject()) {
        // May need order confirmation — check for "id" in object
        QString err = doc.object().value("error").toString();
        if (!err.isEmpty()) return {false, "", err};
        arr = QJsonArray{doc.object()};
    }

    if (arr.isEmpty())
        return {false, "", "place_order: empty response"};

    QJsonObject result = arr[0].toObject();
    QString order_id = result.value("order_id").toString();
    if (order_id.isEmpty())
        order_id = QString::number(result.value("order_id").toVariant().toLongLong());

    return {true, order_id, ""};
}

// ---------- modify_order ----------

ApiResponse<QJsonObject> IBKRBroker::modify_order(const BrokerCredentials& creds,
                                                    const QString& order_id,
                                                    const QJsonObject& mods) {
    int64_t ts   = now_ts();
    QString gw   = gateway_url(creds);
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    QJsonObject body;
    if (mods.contains("quantity"))     body["quantity"]  = mods.value("quantity").toDouble();
    if (mods.contains("price"))        body["price"]     = mods.value("price").toDouble();
    if (mods.contains("auxPrice"))     body["auxPrice"]  = mods.value("auxPrice").toDouble();
    if (mods.contains("orderType"))    body["orderType"] = mods.value("orderType").toString();
    if (mods.contains("tif"))          body["tif"]       = mods.value("tif").toString();

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(
        gw + "/v1/api/iserver/account/" + acct + "/order/" + order_id,
        body, auth_headers(creds)
    );

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "modify_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "modify_order: invalid response", ts};

    QJsonObject obj = doc.object();
    if (!obj.value("error").toString().isEmpty())
        return {false, std::nullopt, obj.value("error").toString(), ts};

    return {true, obj, "", ts};
}

// ---------- cancel_order ----------

ApiResponse<QJsonObject> IBKRBroker::cancel_order(const BrokerCredentials& creds,
                                                    const QString& order_id) {
    int64_t ts   = now_ts();
    QString gw   = gateway_url(creds);
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.del(
        gw + "/v1/api/iserver/account/" + acct + "/order/" + order_id,
        auth_headers(creds)
    );

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "cancel_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "cancel_order: invalid response", ts};

    return {true, doc.object(), "", ts};
}

// ---------- get_orders ----------

ApiResponse<QVector<BrokerOrderInfo>> IBKRBroker::get_orders(const BrokerCredentials& creds) {
    int64_t ts  = now_ts();
    QString gw  = gateway_url(creds);

    auto& http = BrokerHttp::instance();
    auto resp = http.get(gw + "/v1/api/iserver/account/orders", auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_orders failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_orders: invalid response", ts};

    // Response: {orders: [...]}
    QJsonArray arr = doc.object().value("orders").toArray();
    QVector<BrokerOrderInfo> orders;
    orders.reserve(arr.size());

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        BrokerOrderInfo info;
        info.order_id    = QString::number(o.value("orderId").toVariant().toLongLong());
        info.symbol      = o.value("ticker").toString();
        info.exchange    = o.value("exchange").toString();
        info.quantity    = o.value("totalSize").toDouble();
        info.filled_qty  = o.value("filledQuantity").toDouble();
        info.price       = o.value("price").toDouble();
        info.stop_price  = o.value("auxPrice").toDouble();
        info.avg_price   = o.value("avgPrice").toDouble();
        // Status: Submitted, PreSubmitted, Filled, Cancelled, Inactive
        QString st = o.value("status").toString().toLower();
        if      (st == "filled")    info.status = "filled";
        else if (st == "cancelled" || st == "inactive") info.status = "cancelled";
        else if (st == "submitted" || st == "presubmitted") info.status = "open";
        else    info.status = "open";
        info.side         = o.value("side").toString().toLower();
        info.order_type   = o.value("orderType").toString();
        info.product_type = o.value("timeInForce").toString();
        info.timestamp    = o.value("lastExecutionTime").toString();
        orders.append(info);
    }

    return {true, orders, "", ts};
}

// ---------- get_trade_book ----------

ApiResponse<QJsonObject> IBKRBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    QString gw = gateway_url(creds);

    auto& http = BrokerHttp::instance();
    auto resp = http.get(gw + "/v1/api/iserver/account/trades", auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_trade_book failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (doc.isObject()) return {true, doc.object(), "", ts};

    QJsonObject wrapper;
    wrapper["trades"] = doc.array();
    return {true, wrapper, "", ts};
}

// ---------- get_positions ----------

ApiResponse<QVector<BrokerPosition>> IBKRBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts   = now_ts();
    QString gw   = gateway_url(creds);
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.get(
        gw + "/v1/api/portfolio/" + acct + "/positions/0",
        auth_headers(creds)
    );

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_positions failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    QJsonArray arr = doc.isArray() ? doc.array()
                                   : doc.object().value("positions").toArray();

    QVector<BrokerPosition> positions;

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        double qty = o.value("position").toDouble();
        if (qty == 0.0) continue;

        BrokerPosition pos;
        pos.symbol    = o.value("ticker").toString();
        pos.exchange  = o.value("listingExchange").toString();
        pos.quantity  = qty;
        pos.avg_price = o.value("avgCost").toDouble();
        pos.ltp       = o.value("mktPrice").toDouble();
        pos.pnl       = o.value("unrealizedPnl").toDouble();
        pos.day_pnl   = o.value("realizedPnl").toDouble();
        pos.side      = qty > 0 ? "LONG" : "SHORT";
        if (pos.avg_price > 0)
            pos.pnl_pct = (pos.ltp - pos.avg_price) / pos.avg_price * 100.0;
        positions.append(pos);
    }

    return {true, positions, "", ts};
}

// ---------- get_holdings ----------
// IBKR has no separate holdings endpoint — same as positions

ApiResponse<QVector<BrokerHolding>> IBKRBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts   = now_ts();
    QString gw   = gateway_url(creds);
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.get(
        gw + "/v1/api/portfolio/" + acct + "/positions/0",
        auth_headers(creds)
    );

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    QJsonArray arr = doc.isArray() ? doc.array()
                                   : doc.object().value("positions").toArray();

    QVector<BrokerHolding> holdings;

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        double qty = o.value("position").toDouble();
        if (qty <= 0.0) continue; // holdings = long positions only

        BrokerHolding h;
        h.symbol          = o.value("ticker").toString();
        h.exchange         = o.value("listingExchange").toString();
        h.quantity         = qty;
        h.avg_price        = o.value("avgCost").toDouble();
        h.ltp              = o.value("mktPrice").toDouble();
        h.pnl              = o.value("unrealizedPnl").toDouble();
        h.invested_value   = qty * h.avg_price;
        h.current_value    = o.value("mktValue").toDouble();
        if (h.invested_value > 0)
            h.pnl_pct = h.pnl / h.invested_value * 100.0;
        holdings.append(h);
    }

    return {true, holdings, "", ts};
}

// ---------- get_funds ----------

ApiResponse<BrokerFunds> IBKRBroker::get_funds(const BrokerCredentials& creds) {
    int64_t ts   = now_ts();
    QString gw   = gateway_url(creds);
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.get(
        gw + "/v1/api/portfolio/" + acct + "/summary",
        auth_headers(creds)
    );

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_funds: invalid response", ts};

    QJsonObject obj = doc.object();
    // Each field is {"amount": x, "currency": "USD", ...}
    auto field_val = [&](const QString& key) -> double {
        return obj.value(key).toObject().value("amount").toDouble();
    };

    BrokerFunds funds;
    funds.available_balance = field_val("availablefunds");
    funds.used_margin       = field_val("initmarginreq");
    funds.total_balance     = field_val("netliquidation");
    funds.collateral        = field_val("excessliquidity");

    return {true, funds, "", ts};
}

// ---------- get_quotes ----------
// Symbol format: "NASDAQ:AAPL:265598" — conid is 3rd part (required)
// Fields: 31=last, 70=high, 71=low, 7295=open, 7296=prevClose, 87=volume

ApiResponse<QVector<BrokerQuote>> IBKRBroker::get_quotes(const BrokerCredentials& creds,
                                                           const QVector<QString>& symbols) {
    int64_t ts = now_ts();
    if (symbols.isEmpty())
        return {true, QVector<BrokerQuote>{}, "", ts};

    QString gw = gateway_url(creds);

    // Build conid list and reverse map conid→symbol name
    QStringList conids;
    QMap<QString, QString> conid_to_name;

    for (const QString& sym : symbols) {
        QStringList parts = sym.split(":");
        if (parts.size() < 3) continue;
        QString conid = parts[2];
        QString name  = parts[1];
        conids.append(conid);
        conid_to_name[conid] = name;
    }

    if (conids.isEmpty())
        return {false, std::nullopt, "get_quotes: conid required (format EXCHANGE:SYMBOL:CONID)", ts};

    // First call — snapshot may need two requests to populate
    QString url = gw + "/v1/api/iserver/marketdata/snapshot?conids="
                + conids.join(",") + "&fields=31,70,71,7295,7296,87";

    auto& http = BrokerHttp::instance();
    auto resp = http.get(url, auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_quotes failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isArray())
        return {false, std::nullopt, "get_quotes: invalid response", ts};

    QVector<BrokerQuote> quotes;

    for (const QJsonValue& v : doc.array()) {
        QJsonObject o = v.toObject();
        QString conid = QString::number(o.value("conid").toVariant().toLongLong());

        BrokerQuote quote;
        quote.symbol = conid_to_name.value(conid, conid);
        // Field codes returned as string keys "31", "70" etc.
        quote.ltp    = o.value("31").toString().toDouble();
        quote.high   = o.value("70").toString().toDouble();
        quote.low    = o.value("71").toString().toDouble();
        quote.open   = o.value("7295").toString().toDouble();
        quote.close  = o.value("7296").toString().toDouble();
        quote.volume = o.value("87").toString().toLongLong();
        if (quote.close > 0)
            quote.change_pct = (quote.ltp - quote.close) / quote.close * 100.0;
        quote.change    = quote.ltp - quote.close;
        quote.timestamp = ts;
        quotes.append(quote);
    }

    return {true, quotes, "", ts};
}

// ---------- get_history ----------

ApiResponse<QVector<BrokerCandle>> IBKRBroker::get_history(const BrokerCredentials& creds,
                                                             const QString& symbol,
                                                             const QString& resolution,
                                                             const QString& from_date,
                                                             const QString& to_date) {
    int64_t ts = now_ts();
    QString gw = gateway_url(creds);

    // Extract conid from symbol "NASDAQ:AAPL:265598"
    QStringList parts = symbol.split(":");
    QString conid = (parts.size() >= 3) ? parts[2] : "";
    if (conid.isEmpty())
        return {false, std::nullopt, "get_history: conid required (format EXCHANGE:SYMBOL:CONID)", ts};

    auto params = ibkr_history_params(resolution, from_date, to_date);

    QString url = gw + "/v1/api/iserver/marketdata/history?conid=" + conid
                + "&period=" + params.period
                + "&bar=" + params.bar
                + "&outsideRth=false";

    auto& http = BrokerHttp::instance();
    auto resp = http.get(url, auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_history failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_history: invalid response", ts};

    QJsonObject obj = doc.object();
    if (!obj.value("error").toString().isEmpty())
        return {false, std::nullopt, obj.value("error").toString(), ts};

    // Response: {data: [{t, o, h, l, c, v}, ...]}
    QJsonArray arr = obj.value("data").toArray();
    QVector<BrokerCandle> candles;
    candles.reserve(arr.size());

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        BrokerCandle c;
        // t is epoch milliseconds
        c.timestamp = static_cast<int64_t>(o.value("t").toDouble());
        c.open      = o.value("o").toDouble();
        c.high      = o.value("h").toDouble();
        c.low       = o.value("l").toDouble();
        c.close     = o.value("c").toDouble();
        c.volume    = static_cast<int64_t>(o.value("v").toDouble());
        candles.append(c);
    }

    return {true, candles, "", ts};
}

} // namespace fincept::trading
