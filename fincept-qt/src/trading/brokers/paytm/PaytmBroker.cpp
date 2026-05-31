#include "trading/brokers/paytm/PaytmBroker.h"

#include "trading/brokers/BrokerHttp.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimeZone>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>

namespace fincept::trading {

static constexpr const char* BASE = "https://developer.paytmmoney.com";

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// Pick the first present key from an object (handles Paytm's snake_case vs
// camelCase field aliasing) and return it as a QJsonValue.
static QJsonValue pm_first(const QJsonObject& o, const QString& a, const QString& b) {
    return o.contains(a) ? o.value(a) : o.value(b);
}

// Map a unified exchange to the value Paytm's chart endpoint expects. The
// undocumented price-charts route is exercised here for equities only, so only
// the cash exchanges are accepted; anything else returns "" (caller errors).
static QString api_exchange_for_history(const QString& exchange) {
    const QString e = exchange.toUpper();
    if (e == "NSE")
        return "NSE";
    if (e == "BSE")
        return "BSE";
    return QString();
}

// Best-effort timestamp -> epoch-milliseconds for the undocumented chart payload,
// whose time field shape is unknown. Handles numeric seconds, numeric ms, and a
// few string date/datetime formats (interpreted as IST). Returns 0 on failure.
static int64_t pm_history_epoch_ms(const QJsonValue& v) {
    if (v.isDouble()) {
        const int64_t n = static_cast<int64_t>(v.toVariant().toLongLong());
        return (n > 1000000000000LL) ? n : n * 1000LL; // 13-digit ms vs 10-digit s
    }
    const QString s = v.toVariant().toString();
    if (s.isEmpty())
        return 0;
    bool ok = false;
    const qint64 n = s.toLongLong(&ok);
    if (ok)
        return (n > 1000000000000LL) ? n : n * 1000LL;
    const QTimeZone ist(19800); // +5:30
    for (const char* fmt : {"yyyy-MM-ddTHH:mm:ss", "yyyy-MM-dd HH:mm:ss", "yyyy-MM-dd"}) {
        QDateTime dt = QDateTime::fromString(s, fmt);
        if (dt.isValid()) {
            dt.setTimeZone(ist);
            return dt.toMSecsSinceEpoch();
        }
    }
    QDateTime iso = QDateTime::fromString(s, Qt::ISODate);
    return iso.isValid() ? iso.toMSecsSinceEpoch() : 0;
}

// ---------- Static helpers ----------

const BrokerEnumMap<QString>& PaytmBroker::pm_enum_map() {
    static const auto m = [] {
        BrokerEnumMap<QString> x;
        x.set(OrderType::Market, "MKT");
        x.set(OrderType::Limit, "LMT");
        x.set(OrderType::StopLoss, "SLM");
        x.set(OrderType::StopLossLimit, "SL");
        // Paytm product codes: C(NC), M(argin), I(ntraday)
        x.set(ProductType::Intraday, "I");
        x.set(ProductType::Delivery, "C");
        x.set(ProductType::Margin, "M");
        return x;
    }();
    return m;
}

QString PaytmBroker::pm_status(const QString& s) {
    QString sl = s.toLower();
    if (sl == "success" || sl == "successfull" || sl == "complete" || sl == "completed" || sl == "executed" ||
        sl == "traded")
        return "filled";
    if (sl == "open" || sl == "pending" || sl == "trigger pending" || sl == "modified")
        return "open";
    if (sl == "cancelled" || sl == "canceled")
        return "cancelled";
    if (sl == "rejected" || sl == "failure")
        return "rejected";
    return "open";
}

bool PaytmBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 401 || resp.status_code == 403)
        return true;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return false;
    QString msg = doc.object().value("message").toString().toLower();
    return msg.contains("unauthorized") || msg.contains("invalid token") || msg.contains("session") ||
           msg.contains("expired") || msg.contains("jwt");
}

QString PaytmBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    if (is_token_expired(resp))
        return "[TOKEN_EXPIRED] Session expired, please re-login";
    if (!resp.success && resp.status_code == 0)
        return resp.error.isEmpty() ? fallback : resp.error;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        QString msg = obj.value("message").toString();
        if (msg.isEmpty()) {
            // Paytm error array: {"errors":[{"message":"..."}]}
            QJsonArray errs = obj.value("errors").toArray();
            if (!errs.isEmpty())
                msg = errs[0].toObject().value("message").toString();
        }
        if (!msg.isEmpty())
            return msg;
    }
    return fallback;
}

// ---------- Auth headers ----------

QMap<QString, QString> PaytmBroker::auth_headers(const BrokerCredentials& creds) const {
    return {{"x-jwt-token", creds.access_token},
            {"Content-Type", "application/json"},
            {"Accept", "application/json"}};
}

// ---------- exchange_token ----------
// ApiKey=api_key, ApiSecret=api_secret, AuthCode=request_token
//   POST /accounts/v2/gettoken {api_key, api_secret_key, request_token}
//   → {access_token, public_access_token}

TokenExchangeResponse PaytmBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                  const QString& auth_code) {
    if (api_key.isEmpty() || api_secret.isEmpty() || auth_code.isEmpty())
        return {false, "", "", "", "Paytm login: api_key, api_secret and request_token are required", ""};

    QJsonObject payload;
    payload["api_key"] = api_key;
    payload["api_secret_key"] = api_secret;
    payload["request_token"] = auth_code.trimmed();

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(QString("%1/accounts/v2/gettoken").arg(BASE), payload,
                               {{"Content-Type", "application/json"}});

    if (!resp.success && resp.status_code == 0)
        return {false, "", "", "", "Login failed: " + resp.error, ""};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "", "", "Login: invalid response", ""};

    QJsonObject obj = doc.object();
    QString access_token = obj.value("access_token").toString();
    if (access_token.isEmpty())
        return {false, "", "", "", checked_error(resp, "Login failed: no access token"), ""};

    // public_access_token is the WebSocket feed token — stash it in additional_data.
    QString feed_token = obj.value("public_access_token").toString(access_token);
    return {true, access_token, "", "", feed_token, ""};
}

// ---------- place_order ----------
// POST /orders/v1/place/regular  (JSON, x-jwt-token)

OrderPlaceResponse PaytmBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    // Paytm needs the numeric security_id; the instrument token rides in
    // UnifiedOrder::instrument_token when known, else fall back to symbol.
    const QString security_id = order.instrument_token.isEmpty() ? order.symbol : order.instrument_token;
    const QString segment = (order.exchange == "NSE" || order.exchange == "BSE") ? "E" : "D";
    QString api_exchange = order.exchange;
    if (api_exchange == "NFO")
        api_exchange = "NSE";
    else if (api_exchange == "BFO")
        api_exchange = "BSE";

    QJsonObject payload;
    payload["security_id"] = security_id;
    payload["exchange"] = api_exchange;
    payload["txn_type"] = (order.side == OrderSide::Buy) ? "B" : "S";
    payload["order_type"] = pm_enum_map().order_type_or(order.order_type, "MKT");
    payload["quantity"] = QString::number(static_cast<int>(order.quantity));
    payload["product"] = pm_enum_map().product_or(order.product_type, "I");
    payload["price"] = QString::number(order.price, 'f', 2);
    payload["trigger_price"] = QString::number(order.stop_price, 'f', 2);
    payload["validity"] = "DAY";
    payload["segment"] = segment;
    payload["source"] = "N"; // Android source code; OpenAlgo uses W/M/N/I/R/O
    payload["off_mkt_flag"] = order.amo ? "Y" : "N";

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(QString("%1/orders/v1/place/regular").arg(BASE), payload, auth_headers(creds));

    if (!resp.success && resp.status_code == 0)
        return {false, "", checked_error(resp, "place_order failed")};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "place_order: invalid response"};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "success")
        return {false, "", checked_error(resp, "place_order failed")};

    QJsonArray data = obj.value("data").toArray();
    QString order_id = data.isEmpty() ? "" : data[0].toObject().value("order_no").toVariant().toString();
    if (order_id.isEmpty())
        return {false, "", "place_order: no order_no in response"};
    return {true, order_id, ""};
}

// ---------- find_order: look up a full order record by id from the order book ----------
// Paytm modify/cancel require serial_no, group_id, and the original order fields.

QJsonObject PaytmBroker::find_order(const BrokerCredentials& creds, const QString& order_id) {
    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/orders/v1/user/orders").arg(BASE), auth_headers(creds));
    if (!resp.success && resp.status_code == 0)
        return {};
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {};
    for (const QJsonValue& v : doc.object().value("data").toArray()) {
        QJsonObject o = v.toObject();
        if (o.value("order_no").toVariant().toString() == order_id)
            return o;
    }
    return {};
}

// ---------- modify_order ----------
// POST /orders/v1/modify/regular  — must echo serial_no + group_id from the order book

ApiResponse<QJsonObject> PaytmBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                   const QJsonObject& mods) {
    int64_t ts = now_ts();

    QJsonObject src = find_order(creds, order_id);
    if (src.isEmpty())
        return {false, std::nullopt, "modify_order: order not found in order book", ts};

    const QString price_type = mods.value("orderType").toString();
    QJsonObject payload;
    payload["order_no"] = order_id;
    payload["exchange"] = src.value("exchange");
    payload["segment"] = src.value("segment");
    payload["security_id"] = src.value("security_id");
    payload["quantity"] = mods.contains("quantity") ? QString::number(mods.value("quantity").toInt())
                                                     : src.value("quantity").toVariant().toString();
    payload["price"] = mods.contains("price") ? QString::number(mods.value("price").toDouble(), 'f', 2)
                                              : src.value("price").toVariant().toString();
    payload["trigger_price"] = mods.contains("triggerPrice")
                                   ? QString::number(mods.value("triggerPrice").toDouble(), 'f', 2)
                                   : src.value("trigger_price").toVariant().toString();
    payload["validity"] = "DAY";
    payload["product"] = src.value("product");
    payload["order_type"] = price_type.isEmpty() ? src.value("order_type").toVariant().toString() : price_type;
    payload["txn_type"] = src.value("txn_type");
    payload["source"] = "N";
    payload["off_mkt_flag"] = src.value("off_mkt_flag").toVariant().toString().isEmpty()
                                  ? "N"
                                  : src.value("off_mkt_flag").toVariant().toString();
    payload["serial_no"] = src.value("serial_no");
    payload["group_id"] = src.value("group_id");

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(QString("%1/orders/v1/modify/regular").arg(BASE), payload, auth_headers(creds));

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "modify_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "modify_order: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "modify_order failed"), ts};

    return {true, obj, "", ts};
}

// ---------- cancel_order ----------
// POST /orders/v1/cancel/regular  — must echo the full order record fields

ApiResponse<QJsonObject> PaytmBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    int64_t ts = now_ts();

    QJsonObject src = find_order(creds, order_id);
    if (src.isEmpty())
        return {false, std::nullopt, "cancel_order: order not found in order book", ts};

    QJsonObject payload;
    payload["order_no"] = order_id;
    payload["source"] = "N";
    payload["txn_type"] = src.value("txn_type");
    payload["exchange"] = src.value("exchange");
    payload["segment"] = src.value("segment");
    payload["product"] = src.value("product");
    payload["security_id"] = src.value("security_id");
    payload["quantity"] = src.value("quantity");
    payload["validity"] = src.value("validity");
    payload["order_type"] = src.value("order_type");
    payload["price"] = src.value("price");
    payload["off_mkt_flag"] = src.value("off_mkt_flag");
    payload["mkt_type"] = src.value("mkt_type");
    payload["serial_no"] = src.value("serial_no");
    payload["group_id"] = src.value("group_id");

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(QString("%1/orders/v1/cancel/regular").arg(BASE), payload, auth_headers(creds));

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "cancel_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "cancel_order: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "cancel_order failed"), ts};

    return {true, obj, "", ts};
}

// ---------- get_orders ----------
// GET /orders/v1/user/orders

ApiResponse<QVector<BrokerOrderInfo>> PaytmBroker::get_orders(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/orders/v1/user/orders").arg(BASE), auth_headers(creds));

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_orders failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_orders: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "success") {
        // No orders is a non-error empty state for Paytm.
        if (obj.value("data").toArray().isEmpty())
            return {true, QVector<BrokerOrderInfo>{}, "", ts};
        return {false, std::nullopt, checked_error(resp, "get_orders failed"), ts};
    }

    QVector<BrokerOrderInfo> orders;
    for (const QJsonValue& v : obj.value("data").toArray()) {
        QJsonObject o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("order_no").toVariant().toString();
        info.symbol = o.value("display_name").toString(o.value("security_id").toVariant().toString());
        info.exchange = o.value("exchange").toString();
        info.quantity = o.value("quantity").toVariant().toInt();
        info.filled_qty = o.value("traded_qty").toVariant().toInt();
        info.price = o.value("price").toVariant().toDouble();
        info.stop_price = o.value("trigger_price").toVariant().toDouble();
        info.avg_price = o.value("avg_traded_price").toVariant().toDouble();
        info.status = pm_status(o.value("status").toString());
        info.side = (o.value("txn_type").toString() == "B") ? "buy" : "sell";
        info.order_type = o.value("order_type").toString();
        info.product_type = o.value("product").toString();
        info.timestamp = o.value("order_date_time").toString();
        orders.append(info);
    }

    return {true, orders, "", ts};
}

// ---------- get_trade_book ----------
// Paytm exposes orders only (per-order trade detail needs an order id); return
// the order list as the trade book wrapper.

ApiResponse<QJsonObject> PaytmBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/orders/v1/user/orders").arg(BASE), auth_headers(creds));

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_trade_book failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (doc.isObject())
        return {true, doc.object(), "", ts};
    return {false, std::nullopt, "get_trade_book: invalid response", ts};
}

// ---------- get_positions ----------
// GET /orders/v1/position

ApiResponse<QVector<BrokerPosition>> PaytmBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/orders/v1/position").arg(BASE), auth_headers(creds));

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_positions failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_positions: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "success") {
        if (obj.value("data").toArray().isEmpty())
            return {true, QVector<BrokerPosition>{}, "", ts};
        return {false, std::nullopt, checked_error(resp, "get_positions failed"), ts};
    }

    QVector<BrokerPosition> positions;
    for (const QJsonValue& v : obj.value("data").toArray()) {
        QJsonObject o = v.toObject();
        int net_qty = pm_first(o, "net_qty", "netQty").toVariant().toInt();
        if (net_qty == 0)
            continue;
        BrokerPosition pos;
        pos.symbol = o.value("display_name").toString(o.value("security_id").toVariant().toString());
        pos.exchange = o.value("exchange").toString();
        QString product = o.value("product").toString();
        pos.product_type = (product == "C") ? "CNC" : (product == "I" ? "MIS" : "NRML");
        pos.quantity = net_qty;
        pos.avg_price = pm_first(o, "buy_avg", "cost_price").toVariant().toDouble();
        pos.ltp = pm_first(o, "last_traded_price", "ltp").toVariant().toDouble();
        pos.pnl = o.value("realised_profit").toVariant().toDouble() + o.value("unrealised_profit").toVariant().toDouble();
        pos.side = net_qty > 0 ? "LONG" : "SHORT";
        positions.append(pos);
    }

    return {true, positions, "", ts};
}

// ---------- get_holdings ----------
// GET /holdings/v1/get-user-holdings-data

ApiResponse<QVector<BrokerHolding>> PaytmBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/holdings/v1/get-user-holdings-data").arg(BASE), auth_headers(creds));

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_holdings: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "success") {
        if (obj.value("data").toObject().value("results").toArray().isEmpty())
            return {true, QVector<BrokerHolding>{}, "", ts};
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};
    }

    QVector<BrokerHolding> holdings;
    for (const QJsonValue& v : obj.value("data").toObject().value("results").toArray()) {
        QJsonObject o = v.toObject();
        BrokerHolding h;
        h.symbol = o.value("display_name").toString(o.value("nse_symbol").toString(o.value("bse_symbol").toString()));
        h.exchange = o.value("exchange").toString("NSE");
        h.quantity = o.value("quantity").toVariant().toDouble();
        h.avg_price = o.value("cost_price").toVariant().toDouble();
        h.ltp = pm_first(o, "last_traded_price", "ltp").toVariant().toDouble();
        h.invested_value = h.quantity * h.avg_price;
        h.current_value = h.quantity * h.ltp;
        h.pnl = (h.ltp - h.avg_price) * h.quantity;
        if (h.avg_price > 0)
            h.pnl_pct = ((h.ltp - h.avg_price) / h.avg_price) * 100.0;
        holdings.append(h);
    }

    return {true, holdings, "", ts};
}

// ---------- get_funds ----------
// GET /accounts/v1/funds/summary?config=true

ApiResponse<BrokerFunds> PaytmBroker::get_funds(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/accounts/v1/funds/summary?config=true").arg(BASE), auth_headers(creds));

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_funds: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() == "error")
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    QJsonObject summary = obj.value("data").toObject().value("funds_summary").toObject();
    BrokerFunds funds;
    funds.available_balance = summary.value("available_cash").toVariant().toDouble();
    funds.used_margin = summary.value("utilised_amount").toVariant().toDouble();
    funds.collateral = summary.value("collaterals").toVariant().toDouble();
    funds.total_balance = funds.available_balance + funds.used_margin;
    funds.raw_data = summary;

    return {true, funds, "", ts};
}

// ---------- get_quotes ----------
// GET /data/v1/price/live?mode=FULL&pref=<EXCHANGE:TOKEN:SEGMENT>
// Symbol format: "EXCHANGE:TOKEN[:SEGMENT]"; SEGMENT defaults to EQUITY.

ApiResponse<QVector<BrokerQuote>> PaytmBroker::get_quotes(const BrokerCredentials& creds,
                                                          const QVector<QString>& symbols) {
    int64_t ts = now_ts();
    if (symbols.isEmpty())
        return {true, QVector<BrokerQuote>{}, "", ts};

    auto& http = BrokerHttp::instance();
    QVector<BrokerQuote> quotes;
    QStringList prefs;

    for (const QString& sym : symbols) {
        QStringList parts = sym.split(":");
        if (parts.size() < 2)
            continue; // Paytm requires exchange + numeric token
        QString exchange = parts[0];
        QString token = parts[1];
        QString segment = (parts.size() >= 3) ? parts[2] : "EQUITY";
        prefs.append(QString("%1:%2:%3").arg(exchange, token, segment));
    }
    if (prefs.isEmpty())
        return {false, std::nullopt,
                "Paytm quotes require EXCHANGE:TOKEN format (numeric security_id)", ts};

    QString pref_param = QUrl::toPercentEncoding(prefs.join(",")); // batch in one request
    QString url = QString("%1/data/v1/price/live?mode=FULL&pref=%2").arg(BASE, pref_param);
    auto resp = http.get(url, auth_headers(creds));

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_quotes failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_quotes: invalid response", ts};

    for (const QJsonValue& v : doc.object().value("data").toArray()) {
        QJsonObject o = v.toObject();
        QJsonObject ohlc = o.value("ohlc").toObject();
        BrokerQuote bq;
        bq.symbol = o.value("security_id").toVariant().toString();
        bq.ltp = o.value("last_price").toVariant().toDouble();
        bq.open = ohlc.value("open").toVariant().toDouble();
        bq.high = ohlc.value("high").toVariant().toDouble();
        bq.low = ohlc.value("low").toVariant().toDouble();
        bq.close = ohlc.value("close").toVariant().toDouble();
        bq.volume = o.value("volume_traded").toVariant().toDouble();
        bq.oi = static_cast<qint64>(o.value("oi").toVariant().toLongLong());
        if (bq.close > 0) {
            bq.change = bq.ltp - bq.close;
            bq.change_pct = (bq.ltp - bq.close) / bq.close * 100.0;
        }
        bq.timestamp = ts;
        quotes.append(bq);
    }

    return {true, quotes, "", ts};
}

// ---------- get_history ----------
// EXPERIMENTAL / UNDOCUMENTED endpoint.
//
// Paytm Money publishes no historical-data API in its docs, but the official
// pyPMClient SDK carries a disabled route under a "# historical_data endpoints"
// comment: GET /data/v1/price-charts/sym (pmClient/constants.py). Live probes
// confirm the route is real and JWT-gated — no token -> 400 PM_OPEN_API_400 (the
// same app-layer error the live-quote endpoint returns), bad token -> 401, and
// POST -> 405 (so the live verb is GET, not the POST in the stale SDK). The
// request/response contract is NOT officially documented; the query fields below
// mirror the SDK's commented signature. Parsing is deliberately defensive and the
// method fails soft (empty success) when the shape doesn't match, so charts
// degrade gracefully. Verify against a live token before relying on this.
//
// Symbol format: "EXCHANGE:SYMBOL" (equity). SYMBOL is the trading symbol, NOT
// the numeric security_id used by get_quotes — this endpoint keys on symbol.

ApiResponse<QVector<BrokerCandle>> PaytmBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                           const QString& resolution, const QString& from_date,
                                                           const QString& to_date) {
    int64_t ts = now_ts();

    QString exchange = "NSE";
    QString name = symbol;
    if (symbol.contains(':')) {
        exchange = symbol.section(':', 0, 0);
        name = symbol.section(':', 1, 1);
    }
    if (api_exchange_for_history(exchange).isEmpty())
        return {false, std::nullopt, "Paytm history: unsupported exchange " + exchange, ts};
    if (name.isEmpty())
        return {false, std::nullopt, "Paytm history: empty symbol", ts};

    // Map the unified resolution to Paytm's chart interval token. The SDK sample
    // shows "MINUTE"; daily/weekly/monthly tokens are inferred and may need
    // adjustment once the live contract is confirmed.
    const QString r = resolution.toUpper();
    QString interval;
    if (r == "D" || r == "1D" || r == "DAY")
        interval = "DAY";
    else if (r == "W" || r == "1W" || r == "WEEK")
        interval = "WEEK";
    else if (r == "M" || r == "1M" || r == "MONTH")
        interval = "MONTH";
    else
        interval = "MINUTE"; // 1/5/15/30/60-minute all map here (count is implicit)

    QUrlQuery q;
    q.addQueryItem("symbol", name);
    q.addQueryItem("exchange", api_exchange_for_history(exchange));
    q.addQueryItem("instType", "EQUITY");
    q.addQueryItem("interval", interval);
    q.addQueryItem("fromDate", from_date);
    q.addQueryItem("toDate", to_date);
    q.addQueryItem("cont", "false");

    const QString url = QString("%1/data/v1/price-charts/sym?%2").arg(BASE, q.toString(QUrl::FullyEncoded));
    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));

    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED] Session expired, please re-login", ts};
    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_history failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());

    // Locate the candle array across the shapes this undocumented endpoint might
    // return: a bare array, {data:[...]}, or {data:{candles|data:[...]}}.
    QJsonArray rows;
    if (doc.isArray()) {
        rows = doc.array();
    } else if (doc.isObject()) {
        const QJsonObject root = doc.object();
        const QJsonValue d = root.value("data");
        if (d.isArray())
            rows = d.toArray();
        else if (d.isObject()) {
            const QJsonObject dobj = d.toObject();
            rows = dobj.value("candles").toArray();
            if (rows.isEmpty())
                rows = dobj.value("data").toArray();
        }
        if (rows.isEmpty())
            rows = root.value("candles").toArray();
    }

    QVector<BrokerCandle> candles;
    candles.reserve(rows.size());
    for (const QJsonValue& v : rows) {
        BrokerCandle c;
        if (v.isArray()) {
            // [time, open, high, low, close, volume]
            const QJsonArray row = v.toArray();
            if (row.size() < 6)
                continue;
            c.timestamp = pm_history_epoch_ms(row[0]);
            c.open = row[1].toVariant().toDouble();
            c.high = row[2].toVariant().toDouble();
            c.low = row[3].toVariant().toDouble();
            c.close = row[4].toVariant().toDouble();
            c.volume = row[5].toVariant().toDouble();
        } else if (v.isObject()) {
            const QJsonObject o = v.toObject();
            QJsonValue tv = o.contains("time") ? o.value("time")
                            : o.contains("timestamp") ? o.value("timestamp")
                            : o.contains("date") ? o.value("date") : o.value("dateTime");
            c.timestamp = pm_history_epoch_ms(tv);
            c.open = pm_first(o, "open", "o").toVariant().toDouble();
            c.high = pm_first(o, "high", "h").toVariant().toDouble();
            c.low = pm_first(o, "low", "l").toVariant().toDouble();
            c.close = pm_first(o, "close", "c").toVariant().toDouble();
            c.volume = pm_first(o, "volume", "v").toVariant().toDouble();
        } else {
            continue;
        }
        candles.append(c);
    }

    std::sort(candles.begin(), candles.end(),
              [](const BrokerCandle& a, const BrokerCandle& b) { return a.timestamp < b.timestamp; });

    // Fail soft: if the undocumented contract didn't yield candles, return
    // empty-success with a hint rather than erroring, so charts show "no data".
    if (candles.isEmpty())
        return {true, candles,
                "Paytm history returned no parseable candles (undocumented endpoint — verify contract)", ts};
    return {true, candles, "", ts};
}

} // namespace fincept::trading
