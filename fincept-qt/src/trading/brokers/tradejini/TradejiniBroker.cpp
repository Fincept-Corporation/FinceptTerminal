#include "trading/brokers/tradejini/TradejiniBroker.h"

#include "trading/brokers/BrokerHttp.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrlQuery>

namespace fincept::trading {

static constexpr const char* BASE = "https://api.tradejini.com/v2";

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// Pick the first present key from an object and return it as a QJsonValue.
static QJsonValue tj_first(const QJsonObject& o, const QString& a, const QString& b) {
    return o.contains(a) ? o.value(a) : o.value(b);
}

// ---------- Static helpers ----------

QString TradejiniBroker::bearer(const BrokerCredentials& creds) {
    // Header format: "Bearer <api_key>:<access_token>".
    return QString("Bearer %1:%2").arg(creds.api_key, creds.access_token);
}

const BrokerEnumMap<QString>& TradejiniBroker::tj_enum_map() {
    static const auto m = [] {
        BrokerEnumMap<QString> x;
        x.set(OrderType::Market, "market");
        x.set(OrderType::Limit, "limit");
        x.set(OrderType::StopLoss, "stopmarket"); // SL-M
        x.set(OrderType::StopLossLimit, "stoplimit");
        x.set(ProductType::Intraday, "intraday");
        x.set(ProductType::Delivery, "delivery");
        x.set(ProductType::Margin, "normal");
        return x;
    }();
    return m;
}

QString TradejiniBroker::tj_status(const QString& s) {
    QString sl = s.toLower();
    if (sl == "complete" || sl == "completed" || sl == "executed" || sl == "filled" || sl == "traded")
        return "filled";
    if (sl == "open" || sl == "pending" || sl == "trigger pending" || sl == "modified")
        return "open";
    if (sl == "cancelled" || sl == "canceled")
        return "cancelled";
    if (sl == "rejected")
        return "rejected";
    return "open";
}

bool TradejiniBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 401 || resp.status_code == 403)
        return true;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return false;
    QJsonObject obj = doc.object();
    QString msg = obj.value("message").toString().toLower();
    if (msg.isEmpty() && obj.value("d").isObject())
        msg = obj.value("d").toObject().value("msg").toString().toLower();
    return msg.contains("unauthorized") || msg.contains("invalid token") || msg.contains("session") ||
           msg.contains("expired") || msg.contains("token");
}

QString TradejiniBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    if (is_token_expired(resp))
        return "[TOKEN_EXPIRED] Session expired, please re-login";
    if (!resp.success && resp.status_code == 0)
        return resp.error.isEmpty() ? fallback : resp.error;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        QString msg = obj.value("message").toString();
        if (msg.isEmpty() && obj.value("d").isObject())
            msg = obj.value("d").toObject().value("msg").toString();
        if (!msg.isEmpty())
            return msg;
    }
    return fallback;
}

// ---------- Auth headers ----------

QMap<QString, QString> TradejiniBroker::auth_headers(const BrokerCredentials& creds) const {
    return {{"Authorization", bearer(creds)}};
}

// ---------- exchange_token ----------
// ApiKey    = api_key (bearer prefix / app secret)
// ApiSecret = password
// AuthCode  = totp
// POST /api-gw/oauth/individual-token-v2  (Bearer <api_key>, form: password, twoFa, twoFaTyp=totp)

TokenExchangeResponse TradejiniBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                      const QString& auth_code) {
    if (api_key.isEmpty() || api_secret.isEmpty() || auth_code.isEmpty())
        return {false, "", "", "", "Tradejini login: api_key, password and TOTP are required", ""};

    QMap<QString, QString> params;
    params["password"] = api_secret;
    params["twoFa"] = auth_code.trimmed();
    params["twoFaTyp"] = "totp";

    auto& http = BrokerHttp::instance();
    auto resp = http.post_form(QString("%1/api-gw/oauth/individual-token-v2").arg(BASE), params,
                               {{"Authorization", QString("Bearer %1").arg(api_key)}});

    if (!resp.success && resp.status_code == 0)
        return {false, "", "", "", "Login failed: " + resp.error, ""};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "", "", "Login: invalid response", ""};

    QJsonObject obj = doc.object();
    QString token = obj.value("access_token").toString();
    if (token.isEmpty())
        return {false, "", "", "", obj.value("message").toString("Login failed: no access token"), ""};

    return {true, token, "", "", "", ""};
}

// ---------- place_order ----------
// POST /oms/place-order  (form-urlencoded)

OrderPlaceResponse TradejiniBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    const QString order_type = tj_enum_map().order_type_or(order.order_type, "market");

    QMap<QString, QString> params;
    params["symId"] = order.symbol; // broker symbol id, e.g. "RELIANCE-EQ" / token id
    params["qty"] = QString::number(static_cast<int>(order.quantity));
    params["side"] = (order.side == OrderSide::Buy) ? "buy" : "sell";
    params["type"] = order_type;
    params["product"] = tj_enum_map().product_or(order.product_type, "intraday");
    params["validity"] = (order.validity.compare("IOC", Qt::CaseInsensitive) == 0) ? "ioc" : "day";

    if (order_type == "limit" || order_type == "stoplimit")
        params["limitPrice"] = QString::number(order.price, 'f', 2);
    if (order_type == "stoplimit" || order_type == "stopmarket")
        params["trigPrice"] = QString::number(order.stop_price, 'f', 2);
    if (order_type == "market" || order_type == "stopmarket")
        params["mktProt"] = "2";

    auto& http = BrokerHttp::instance();
    auto resp = http.post_form(QString("%1/oms/place-order").arg(BASE), params, {{"Authorization", bearer(creds)}});

    if (!resp.success && resp.status_code == 0)
        return {false, "", checked_error(resp, "place_order failed")};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "place_order: invalid response"};

    QJsonObject obj = doc.object();
    if (obj.value("s").toString() != "ok")
        return {false, "", checked_error(resp, "place_order failed")};

    // orderId may be a string or a number — toVariant().toString() handles both.
    QString order_id = obj.value("d").toObject().value("orderId").toVariant().toString();
    if (order_id.isEmpty())
        return {false, "", "place_order: no orderId in response"};
    return {true, order_id, ""};
}

// ---------- modify_order ----------
// PUT /api/oms/modify-order  (form-urlencoded)

ApiResponse<QJsonObject> TradejiniBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                       const QJsonObject& mods) {
    int64_t ts = now_ts();

    const QString price_type = mods.value("orderType").toString("limit").toLower();
    QString tj_type = "limit";
    if (price_type.contains("market") && price_type.contains("stop"))
        tj_type = "stopmarket";
    else if (price_type.contains("stop") || price_type == "sl")
        tj_type = "stoplimit";
    else if (price_type.contains("market"))
        tj_type = "market";

    QMap<QString, QString> params;
    params["symId"] = mods.value("symbol").toString();
    params["orderId"] = order_id;
    params["qty"] = QString::number(mods.value("quantity").toInt(0));
    params["type"] = tj_type;
    params["validity"] = "day";
    params["side"] = mods.value("side").toString("buy").toLower();
    if (tj_type == "limit" || tj_type == "stoplimit")
        params["limitPrice"] = QString::number(mods.value("price").toDouble(0), 'f', 2);
    if (tj_type == "stoplimit" || tj_type == "stopmarket")
        params["trigPrice"] = QString::number(mods.value("triggerPrice").toDouble(0), 'f', 2);

    auto& http = BrokerHttp::instance();
    auto resp = http.put_form(QString("%1/api/oms/modify-order").arg(BASE), params, {{"Authorization", bearer(creds)}});

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "modify_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "modify_order: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("s").toString() != "ok")
        return {false, std::nullopt, checked_error(resp, "modify_order failed"), ts};

    return {true, obj.value("d").toObject(), "", ts};
}

// ---------- cancel_order ----------
// DELETE /api/oms/cancel-order?orderId=<id>

ApiResponse<QJsonObject> TradejiniBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    int64_t ts = now_ts();

    QUrlQuery q;
    q.addQueryItem("orderId", order_id);
    QString url = QString("%1/api/oms/cancel-order?%2").arg(BASE, q.toString(QUrl::FullyEncoded));

    auto& http = BrokerHttp::instance();
    auto resp = http.del(url, {{"Authorization", bearer(creds)}});

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "cancel_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "cancel_order: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("s").toString() != "ok")
        return {false, std::nullopt, checked_error(resp, "cancel_order failed"), ts};

    return {true, obj.value("d").toObject(), "", ts};
}

// ---------- get_orders ----------
// GET /api/oms/orders?symDetails=true

ApiResponse<QVector<BrokerOrderInfo>> TradejiniBroker::get_orders(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/api/oms/orders?symDetails=true").arg(BASE), {{"Authorization", bearer(creds)}});

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_orders failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_orders: invalid response", ts};

    QJsonObject obj = doc.object();
    QString s = obj.value("s").toString();
    if (s == "no-data")
        return {true, QVector<BrokerOrderInfo>{}, "", ts};
    if (s != "ok")
        return {false, std::nullopt, checked_error(resp, "get_orders failed"), ts};

    QVector<BrokerOrderInfo> orders;
    for (const QJsonValue& v : obj.value("d").toArray()) {
        QJsonObject o = v.toObject();
        QJsonObject sym = o.value("sym").toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("orderId").toVariant().toString();
        info.symbol = sym.value("trdSym").toString(sym.value("sym").toString());
        info.exchange = sym.value("exch").toString();
        info.quantity = o.value("qty").toVariant().toInt();
        info.filled_qty = o.value("fillQty").toVariant().toInt();
        info.price = o.value("limitPrice").toVariant().toDouble();
        info.stop_price = o.value("trigPrice").toVariant().toDouble();
        info.avg_price = o.value("avgPrice").toVariant().toDouble();
        info.status = tj_status(o.value("status").toString());
        info.side = o.value("side").toString().toLower();
        info.order_type = o.value("type").toString();
        info.product_type = o.value("product").toString();
        info.timestamp = o.value("orderTime").toString();
        orders.append(info);
    }

    return {true, orders, "", ts};
}

// ---------- get_trade_book ----------
// GET /api/oms/trades?symDetails=true

ApiResponse<QJsonObject> TradejiniBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/api/oms/trades?symDetails=true").arg(BASE), {{"Authorization", bearer(creds)}});

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_trade_book failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_trade_book: invalid response", ts};

    QJsonObject obj = doc.object();
    QString s = obj.value("s").toString();
    if (s != "ok" && s != "no-data")
        return {false, std::nullopt, checked_error(resp, "get_trade_book failed"), ts};

    QJsonObject wrapper;
    wrapper["trades"] = obj.value("d").toArray();
    return {true, wrapper, "", ts};
}

// ---------- get_positions ----------
// GET /api/oms/positions?symDetails=true

ApiResponse<QVector<BrokerPosition>> TradejiniBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/api/oms/positions?symDetails=true").arg(BASE), {{"Authorization", bearer(creds)}});

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_positions failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_positions: invalid response", ts};

    QJsonObject obj = doc.object();
    QString s = obj.value("s").toString();
    if (s == "no-data")
        return {true, QVector<BrokerPosition>{}, "", ts};
    if (s != "ok")
        return {false, std::nullopt, checked_error(resp, "get_positions failed"), ts};

    QVector<BrokerPosition> positions;
    for (const QJsonValue& v : obj.value("d").toArray()) {
        QJsonObject o = v.toObject();
        int net_qty = o.value("netQty").toVariant().toInt();
        if (net_qty == 0)
            continue;
        QJsonObject sym = o.value("sym").toObject();
        BrokerPosition pos;
        pos.symbol = sym.value("trdSym").toString(sym.value("sym").toString());
        pos.exchange = sym.value("exch").toString();
        QString product = o.value("product").toString().toLower();
        pos.product_type = (product == "delivery") ? "CNC" : (product == "intraday" ? "MIS" : "NRML");
        pos.quantity = net_qty;
        pos.avg_price = o.value("netAvgPrice").toVariant().toDouble();
        pos.ltp = o.value("ltp").toVariant().toDouble();
        pos.pnl = o.value("realizedPnl").toVariant().toDouble() + o.value("unrealizedPnl").toVariant().toDouble();
        pos.side = net_qty > 0 ? "LONG" : "SHORT";
        positions.append(pos);
    }

    return {true, positions, "", ts};
}

// ---------- get_holdings ----------
// GET /api/oms/holdings?symDetails=true

ApiResponse<QVector<BrokerHolding>> TradejiniBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/api/oms/holdings?symDetails=true").arg(BASE), {{"Authorization", bearer(creds)}});

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_holdings: invalid response", ts};

    QJsonObject obj = doc.object();
    QString s = obj.value("s").toString();
    if (s == "no-data")
        return {true, QVector<BrokerHolding>{}, "", ts};
    if (s != "ok")
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};

    // Holdings payload may be an array under "d" or a "holdings" array inside "d".
    QJsonValue d = obj.value("d");
    QJsonArray arr;
    if (d.isArray())
        arr = d.toArray();
    else if (d.isObject())
        arr = d.toObject().value("holdings").toArray();

    QVector<BrokerHolding> holdings;
    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        QJsonObject sym = o.value("sym").toObject();
        BrokerHolding h;
        h.symbol = sym.value("trdSym").toString(sym.value("sym").toString(o.value("symbol").toString()));
        h.exchange = sym.value("exch").toString(o.value("exchange").toString());
        h.quantity = tj_first(o, "qty", "quantity").toVariant().toDouble();
        h.avg_price = o.value("avgPrice").toVariant().toDouble();
        h.ltp = o.value("ltp").toVariant().toDouble();
        h.pnl = o.value("pnl").toVariant().toDouble();
        h.pnl_pct = tj_first(o, "pnlPercent", "pnlpercent").toVariant().toDouble();
        h.invested_value = h.quantity * h.avg_price;
        h.current_value = h.quantity * h.ltp;
        holdings.append(h);
    }

    return {true, holdings, "", ts};
}

// ---------- get_funds ----------
// GET /api/oms/limits

ApiResponse<BrokerFunds> TradejiniBroker::get_funds(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/api/oms/limits").arg(BASE), {{"Authorization", bearer(creds)}});

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_funds: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("s").toString() != "ok")
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    QJsonObject m = obj.value("d").toObject();
    BrokerFunds funds;
    funds.available_balance = m.value("availMargin").toVariant().toDouble();
    funds.used_margin = m.value("marginUsed").toVariant().toDouble();
    funds.collateral = m.value("stockCollateral").toVariant().toDouble();
    funds.total_balance = funds.available_balance + funds.used_margin +
                          m.value("realizedPnl").toVariant().toDouble();
    funds.raw_data = m;

    return {true, funds, "", ts};
}

// ---------- get_quotes ----------
// Tradejini exposes real-time quotes only through its WebSocket SDK (NxtradStream),
// not a REST endpoint. Stream live quotes via the ws adapter; this REST method
// has no upstream equivalent.

ApiResponse<QVector<BrokerQuote>> TradejiniBroker::get_quotes(const BrokerCredentials& /*creds*/,
                                                              const QVector<QString>& /*symbols*/) {
    return {false, std::nullopt,
            "Tradejini quotes are WebSocket-only — use the streaming adapter (no REST quote endpoint)", now_ts()};
}

// ---------- get_history ----------
// GET /api/mkt-data/chart/interval-data?id=<symId>&interval=<min>&from=<sec>&to=<sec>

ApiResponse<QVector<BrokerCandle>> TradejiniBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                               const QString& resolution, const QString& from_date,
                                                               const QString& to_date) {
    int64_t ts = now_ts();

    // Symbol format: "EXCHANGE:SYMID" or just the broker symbol id.
    QStringList parts = symbol.split(":");
    QString sym_id = (parts.size() >= 2) ? parts[1] : symbol;

    QDate from = QDate::fromString(from_date, "yyyy-MM-dd");
    QDate to = QDate::fromString(to_date, "yyyy-MM-dd");
    if (!from.isValid())
        from = QDate::currentDate().addYears(-1);
    if (!to.isValid())
        to = QDate::currentDate();

    int64_t from_epoch = QDateTime(from, QTime(9, 15, 0)).toSecsSinceEpoch();
    int64_t to_epoch = QDateTime(to, QTime(23, 59, 59)).toSecsSinceEpoch();

    // Tradejini intervals are integer minutes; daily is unsupported via this endpoint.
    static const QMap<QString, QString> interval_map = {
        {"1", "1"}, {"5", "5"}, {"15", "15"}, {"30", "30"}, {"60", "60"},
    };
    QString interval = interval_map.value(resolution, "5");

    QUrlQuery q;
    q.addQueryItem("id", sym_id);
    q.addQueryItem("interval", interval);
    q.addQueryItem("from", QString::number(from_epoch));
    q.addQueryItem("to", QString::number(to_epoch));
    QString url = QString("%1/api/mkt-data/chart/interval-data?%2").arg(BASE, q.toString(QUrl::FullyEncoded));

    auto& http = BrokerHttp::instance();
    auto resp = http.get(url, {{"Authorization", bearer(creds)}});

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_history failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_history: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("s").toString() != "ok")
        return {false, std::nullopt, checked_error(resp, "get_history failed"), ts};

    QVector<BrokerCandle> candles;
    // Payload "d" is typically an array of candle rows; support both object-rows
    // and column arrays [t, o, h, l, c, v].
    for (const QJsonValue& v : obj.value("d").toArray()) {
        BrokerCandle c;
        if (v.isObject()) {
            QJsonObject o = v.toObject();
            QJsonValue tv = o.contains("timestamp") ? o.value("timestamp") : o.value("time");
            int64_t t = static_cast<int64_t>(tv.toVariant().toLongLong());
            c.timestamp = (t > 1000000000000LL) ? t : t * 1000LL;
            c.open = o.value("open").toVariant().toDouble();
            c.high = o.value("high").toVariant().toDouble();
            c.low = o.value("low").toVariant().toDouble();
            c.close = o.value("close").toVariant().toDouble();
            c.volume = o.value("volume").toVariant().toDouble();
        } else if (v.isArray()) {
            QJsonArray row = v.toArray();
            if (row.size() < 6)
                continue;
            int64_t t = row[0].toVariant().toLongLong();
            c.timestamp = (t > 1000000000000LL) ? t : t * 1000LL;
            c.open = row[1].toVariant().toDouble();
            c.high = row[2].toVariant().toDouble();
            c.low = row[3].toVariant().toDouble();
            c.close = row[4].toVariant().toDouble();
            c.volume = row[5].toVariant().toDouble();
        } else {
            continue;
        }
        candles.append(c);
    }

    return {true, candles, "", ts};
}

} // namespace fincept::trading
