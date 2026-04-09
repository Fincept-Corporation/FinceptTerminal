#include "trading/brokers/saxo/SaxoBankBroker.h"

#include "trading/brokers/BrokerHttp.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrlQuery>

namespace fincept::trading {

// Live and SIM token endpoints
static constexpr const char* TOKEN_URL_LIVE = "https://live.logonvalidation.net/token";
static constexpr const char* TOKEN_URL_SIM = "https://sim.logonvalidation.net/token";
static constexpr const char* BASE_LIVE = "https://gateway.saxobank.com/openapi";
static constexpr const char* BASE_SIM = "https://gateway.saxobank.com/sim/openapi";

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// ---------- Static helpers ----------

QString SaxoBankBroker::extract_uic(const QString& symbol) {
    // Format: "NYSE:AAPL:211" — Uic is 3rd part
    QStringList parts = symbol.split(":");
    if (parts.size() >= 3)
        return parts[2];
    return {};
}

QString SaxoBankBroker::saxo_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:
            return "Market";
        case OrderType::Limit:
            return "Limit";
        case OrderType::StopLoss:
            return "StopIfTraded";
        case OrderType::StopLossLimit:
            return "StopLimit";
        default:
            return "Market";
    }
}

QString SaxoBankBroker::saxo_duration(ProductType p) {
    return (p == ProductType::Delivery) ? "GoodTillCancel" : "DayOrder";
}

int SaxoBankBroker::saxo_horizon(const QString& resolution) {
    if (resolution == "1" || resolution == "1m")
        return 1;
    if (resolution == "5" || resolution == "5m")
        return 5;
    if (resolution == "10" || resolution == "10m")
        return 10;
    if (resolution == "15" || resolution == "15m")
        return 15;
    if (resolution == "30" || resolution == "30m")
        return 30;
    if (resolution == "60" || resolution == "1h")
        return 60;
    if (resolution == "240" || resolution == "4h")
        return 240;
    if (resolution == "D" || resolution == "1D")
        return 1440;
    if (resolution == "W")
        return 10080;
    if (resolution == "M")
        return 43200;
    return 1440; // default daily
}

bool SaxoBankBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 401)
        return true;
    if (!resp.success)
        return false;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return false;
    QString code = doc.object().value("ErrorCode").toString();
    return (code == "InvalidToken" || code == "TokenExpired");
}

QString SaxoBankBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    if (is_token_expired(resp))
        return "[TOKEN_EXPIRED] Access token expired, please re-authenticate";
    if (!resp.success)
        return resp.error.isEmpty() ? fallback : resp.error;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (doc.isObject()) {
        QString msg = doc.object().value("Message").toString();
        if (!msg.isEmpty())
            return msg;
        msg = doc.object().value("ErrorCode").toString();
        if (!msg.isEmpty())
            return msg;
    }
    return fallback;
}

// ---------- Auth headers ----------

QMap<QString, QString> SaxoBankBroker::auth_headers(const BrokerCredentials& creds) const {
    return {{"Authorization", "Bearer " + creds.access_token},
            {"Content-Type", "application/json"},
            {"Accept", "application/json"}};
}

// ---------- exchange_token ----------
// Saxo OAuth2 authorization code flow
// ApiKey    = App Key (client_id)
// ApiSecret = App Secret (client_secret)
// AuthCode  = authorization code from OAuth redirect
// Stores: access_token, refresh_token (in additional), account_key (as user_id)

TokenExchangeResponse SaxoBankBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                     const QString& auth_code) {
    if (auth_code.trimmed().isEmpty())
        return {false, "", "", "", "Authorization code is required"};

    // Try live token endpoint first; fall back to SIM if it fails
    // In practice user should specify sim vs live via auth_code prefix "sim:::code"
    bool use_sim = auth_code.startsWith("sim:::");
    QString code = use_sim ? auth_code.mid(6) : auth_code;
    QString token_url = use_sim ? TOKEN_URL_SIM : TOKEN_URL_LIVE;

    QUrlQuery form;
    form.addQueryItem("grant_type", "authorization_code");
    form.addQueryItem("code", code);
    form.addQueryItem("redirect_uri", "http://localhost");
    form.addQueryItem("client_id", api_key);
    form.addQueryItem("client_secret", api_secret);

    auto& http = BrokerHttp::instance();
    auto resp = http.post_raw(token_url, form.toString(QUrl::FullyEncoded).toUtf8(),
                              {{"Content-Type", "application/x-www-form-urlencoded"}, {"Accept", "application/json"}});

    if (!resp.success)
        return {false, "", "", "", "Token exchange failed: " + resp.error};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "", "", "Token exchange: invalid response"};

    QJsonObject obj = doc.object();
    QString access_token = obj.value("access_token").toString();
    QString refresh_token = obj.value("refresh_token").toString();

    if (access_token.isEmpty())
        return {false, "", "", "", obj.value("error_description").toString("Token exchange failed")};

    // Fetch AccountKey from /port/v1/clients/me
    QString base = use_sim ? BASE_SIM : BASE_LIVE;
    auto profile_resp = http.get(base + "/port/v1/clients/me",
                                 {{"Authorization", "Bearer " + access_token}, {"Accept", "application/json"}});

    QString account_key;
    if (profile_resp.success) {
        QJsonDocument pdoc = QJsonDocument::fromJson(profile_resp.raw_body.toUtf8());
        if (pdoc.isObject()) {
            account_key = pdoc.object().value("DefaultAccountKey").toString();
            if (account_key.isEmpty())
                account_key = pdoc.object().value("ClientKey").toString();
        }
    }

    // Pack refresh_token into additional field
    return {true, access_token, refresh_token, account_key, ""};
}

// ---------- place_order ----------

OrderPlaceResponse SaxoBankBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    QString uic_str = order.instrument_token;
    if (uic_str.isEmpty())
        uic_str = extract_uic(order.symbol);
    if (uic_str.isEmpty())
        return {false, "", "Saxo place_order: Uic required (symbol format: EXCHANGE:SYMBOL:UIC)"};

    QString account_key = creds.user_id;

    QJsonObject order_obj;
    order_obj["AccountKey"] = account_key;
    order_obj["Uic"] = uic_str.toLongLong();
    order_obj["AssetType"] = "Stock";
    order_obj["BuySell"] = (order.side == OrderSide::Buy) ? "Buy" : "Sell";
    order_obj["Amount"] = order.quantity;
    order_obj["OrderType"] = saxo_order_type(order.order_type);
    order_obj["ManualOrder"] = false;

    if (order.order_type != OrderType::Market)
        order_obj["OrderPrice"] = order.price;
    if (order.stop_price > 0)
        order_obj["StopLimitPrice"] = order.stop_price;

    QJsonObject duration;
    duration["DurationType"] = saxo_duration(order.product_type);
    order_obj["OrderDuration"] = duration;

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(QString("%1/trade/v2/orders").arg(BASE_LIVE), order_obj, auth_headers(creds));

    if (!resp.success)
        return {false, "", checked_error(resp, "place_order failed")};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "place_order: invalid response"};

    QJsonObject obj = doc.object();
    QString order_id = obj.value("OrderId").toString();
    if (order_id.isEmpty())
        return {false, "", checked_error(resp, obj.value("Message").toString("place_order failed"))};

    return {true, order_id, ""};
}

// ---------- modify_order ----------

ApiResponse<QJsonObject> SaxoBankBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                      const QJsonObject& mods) {
    int64_t ts = now_ts();

    QJsonObject body;
    body["AccountKey"] = creds.user_id;
    body["OrderId"] = order_id;
    body["AssetType"] = "Stock";
    body["ManualOrder"] = false;

    if (mods.contains("quantity"))
        body["Amount"] = mods.value("quantity").toDouble();
    if (mods.contains("price"))
        body["OrderPrice"] = mods.value("price").toDouble();
    if (mods.contains("orderType"))
        body["OrderType"] = mods.value("orderType").toString();

    if (mods.contains("duration")) {
        QJsonObject dur;
        dur["DurationType"] = mods.value("duration").toString("DayOrder");
        body["OrderDuration"] = dur;
    }

    auto& http = BrokerHttp::instance();
    auto resp = http.patch_json(QString("%1/trade/v2/orders").arg(BASE_LIVE), body, auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "modify_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "modify_order: invalid response", ts};

    return {true, doc.object(), "", ts};
}

// ---------- cancel_order ----------

ApiResponse<QJsonObject> SaxoBankBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.del(QString("%1/trade/v2/orders/%2?AccountKey=%3").arg(BASE_LIVE, order_id, creds.user_id),
                         auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "cancel_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "cancel_order: invalid response", ts};

    return {true, doc.object(), "", ts};
}

// ---------- get_orders ----------

ApiResponse<QVector<BrokerOrderInfo>> SaxoBankBroker::get_orders(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp =
        http.get(QString("%1/port/v1/orders/me?FieldGroups=DisplayAndFormat").arg(BASE_LIVE), auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_orders failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_orders: invalid response", ts};

    QJsonArray arr = doc.object().value("Data").toArray();
    QVector<BrokerOrderInfo> orders;
    orders.reserve(arr.size());

    auto parse_status = [](const QString& s) -> QString {
        if (s == "FinalFill")
            return "filled";
        if (s == "Cancelled" || s == "Expired")
            return "cancelled";
        if (s == "Working" || s == "Placed")
            return "open";
        if (s == "PartiallyFilled")
            return "open";
        return "open";
    };

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("OrderId").toString();
        info.symbol = o.value("DisplayAndFormat").toObject().value("Symbol").toString();
        info.exchange = o.value("Exchange").toObject().value("ExchangeId").toString();
        info.quantity = o.value("Amount").toDouble();
        info.price = o.value("Price").toDouble();
        info.status = parse_status(o.value("Status").toString());
        info.side = (o.value("BuySell").toString() == "Buy") ? "buy" : "sell";
        info.order_type = o.value("OpenOrderType").toString();
        info.product_type = o.value("Duration").toObject().value("DurationType").toString();
        info.timestamp = o.value("OrderTime").toString();
        orders.append(info);
    }

    return {true, orders, "", ts};
}

// ---------- get_trade_book ----------

ApiResponse<QJsonObject> SaxoBankBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/port/v1/orders/me").arg(BASE_LIVE), auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_trade_book failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_trade_book: invalid response", ts};

    return {true, doc.object(), "", ts};
}

// ---------- get_positions ----------

ApiResponse<QVector<BrokerPosition>> SaxoBankBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(
        QString("%1/port/v1/positions/me?FieldGroups=DisplayAndFormat,PositionBase,PositionView").arg(BASE_LIVE),
        auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_positions failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_positions: invalid response", ts};

    QJsonArray arr = doc.object().value("Data").toArray();
    QVector<BrokerPosition> positions;

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        QJsonObject base = o.value("PositionBase").toObject();
        QJsonObject view = o.value("PositionView").toObject();
        QJsonObject disp = o.value("DisplayAndFormat").toObject();

        double qty = base.value("Amount").toDouble();
        if (qty == 0.0)
            continue;

        BrokerPosition pos;
        pos.symbol = disp.value("Symbol").toString();
        pos.exchange = base.value("AssetType").toString();
        pos.quantity = qty;
        pos.avg_price = base.value("OpenPrice").toDouble();
        pos.ltp = view.value("CurrentPrice").toDouble();
        pos.pnl = view.value("ProfitLossOnTrade").toDouble();
        pos.side = qty > 0 ? "LONG" : "SHORT";
        if (pos.avg_price > 0)
            pos.pnl_pct = pos.pnl / (pos.avg_price * qAbs(qty)) * 100.0;
        positions.append(pos);
    }

    return {true, positions, "", ts};
}

// ---------- get_holdings ----------

ApiResponse<QVector<BrokerHolding>> SaxoBankBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp =
        http.get(QString("%1/port/v1/netpositions/me?FieldGroups=DisplayAndFormat,NetPositionBase,NetPositionView")
                     .arg(BASE_LIVE),
                 auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_holdings: invalid response", ts};

    QJsonArray arr = doc.object().value("Data").toArray();
    QVector<BrokerHolding> holdings;

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        QJsonObject base = o.value("NetPositionBase").toObject();
        QJsonObject view = o.value("NetPositionView").toObject();
        QJsonObject disp = o.value("DisplayAndFormat").toObject();

        double qty = base.value("Amount").toDouble();
        if (qty <= 0.0)
            continue; // holdings = long positions only

        BrokerHolding h;
        h.symbol = disp.value("Symbol").toString();
        h.exchange = base.value("AssetType").toString();
        h.quantity = qty;
        h.avg_price = view.value("AverageOpenPrice").toDouble();
        h.ltp = view.value("CurrentPrice").toDouble();
        h.pnl = view.value("ProfitLossOnTrade").toDouble();
        h.invested_value = h.avg_price * qty;
        h.current_value = h.ltp * qty;
        if (h.invested_value > 0)
            h.pnl_pct = h.pnl / h.invested_value * 100.0;
        holdings.append(h);
    }

    return {true, holdings, "", ts};
}

// ---------- get_funds ----------

ApiResponse<BrokerFunds> SaxoBankBroker::get_funds(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/port/v1/balances/me").arg(BASE_LIVE), auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_funds: invalid response", ts};

    QJsonObject obj = doc.object();

    BrokerFunds funds;
    funds.total_balance = obj.value("TotalValue").toDouble();
    funds.available_balance = obj.value("MarginAvailableForTrading").toDouble();
    funds.used_margin = qAbs(obj.value("MarginUsedByCurrentPositions").toDouble());
    funds.collateral = obj.value("CollateralCreditValue").toObject().value("Line").toDouble();

    return {true, funds, "", ts};
}

// ---------- get_quotes ----------
// Symbol format: "NYSE:AAPL:211" — Uic is 3rd part
// Uses /trade/v1/infoprices/list for multiple symbols (same AssetType per call)

ApiResponse<QVector<BrokerQuote>> SaxoBankBroker::get_quotes(const BrokerCredentials& creds,
                                                             const QVector<QString>& symbols) {
    int64_t ts = now_ts();
    if (symbols.isEmpty())
        return {true, QVector<BrokerQuote>{}, "", ts};

    // Collect Uics and reverse map
    QStringList uics;
    QMap<QString, QString> uic_to_name;

    for (const QString& sym : symbols) {
        QString uic = extract_uic(sym);
        if (uic.isEmpty())
            continue;
        QString name = sym.contains(':') ? sym.section(':', 1, 1) : sym;
        uics.append(uic);
        uic_to_name[uic] = name;
    }

    if (uics.isEmpty())
        return {false, std::nullopt, "get_quotes: Uic required (symbol format EXCHANGE:SYMBOL:UIC)", ts};

    QString url = QString("%1/trade/v1/infoprices/list?Uics=%2&AssetType=Stock"
                          "&FieldGroups=DisplayAndFormat,PriceInfo,PriceInfoDetails,Quote")
                      .arg(BASE_LIVE, uics.join(","));

    auto& http = BrokerHttp::instance();
    auto resp = http.get(url, auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_quotes failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_quotes: invalid response", ts};

    QJsonArray arr = doc.object().value("Data").toArray();
    QVector<BrokerQuote> quotes;
    quotes.reserve(arr.size());

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        QString uic_key = QString::number(o.value("Uic").toVariant().toLongLong());
        QJsonObject quote_obj = o.value("Quote").toObject();
        QJsonObject price_info = o.value("PriceInfo").toObject();
        QJsonObject details = o.value("PriceInfoDetails").toObject();

        BrokerQuote q;
        q.symbol = uic_to_name.value(uic_key, o.value("DisplayAndFormat").toObject().value("Symbol").toString());
        // Use Mid price for LTP; fall back to LastTraded
        q.ltp = quote_obj.value("Mid").toDouble();
        if (q.ltp == 0.0)
            q.ltp = details.value("LastTraded").toDouble();
        q.open = details.value("Open").toDouble();
        q.high = price_info.value("High").toDouble();
        q.low = price_info.value("Low").toDouble();
        q.close = details.value("LastClose").toDouble();
        q.volume = static_cast<int64_t>(details.value("Volume").toDouble());
        q.change = price_info.value("NetChange").toDouble();
        q.change_pct = price_info.value("PercentChange").toDouble();
        q.timestamp = ts;
        quotes.append(q);
    }

    return {true, quotes, "", ts};
}

// ---------- get_history ----------
// GET /openapi/chart/v1/charts?Uic=211&AssetType=Stock&Horizon=1440&Count=365&Mode=From&Time=...

ApiResponse<QVector<BrokerCandle>> SaxoBankBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                               const QString& resolution, const QString& from_date,
                                                               const QString& to_date) {
    int64_t ts = now_ts();

    QString uic = extract_uic(symbol);
    if (uic.isEmpty())
        return {false, std::nullopt, "get_history: Uic required (symbol format EXCHANGE:SYMBOL:UIC)", ts};

    int horizon = saxo_horizon(resolution);

    // Build time range: Saxo uses Mode=UpTo with Time=end for historical data
    // Count derived from date range
    QDate from = QDate::fromString(from_date, "yyyy-MM-dd");
    QDate to = QDate::fromString(to_date, "yyyy-MM-dd");
    if (!from.isValid())
        from = QDate::currentDate().addYears(-1);
    if (!to.isValid())
        to = QDate::currentDate();

    int days = from.daysTo(to);
    int count = 500; // default
    if (horizon >= 1440) {
        count = days + 1;
    } else if (horizon >= 60) {
        count = qMin(days * 8, 500); // ~8 bars/day for 1h
    } else {
        count = qMin(days * (390 / qMax(horizon, 1)), 1200);
    }
    count = qMax(1, qMin(count, 1200)); // Saxo max is typically 1200

    QString time_str = QDateTime(to, QTime(23, 59, 59), Qt::UTC).toString(Qt::ISODate);

    QString url = QString("%1/chart/v1/charts?Uic=%2&AssetType=Stock&Horizon=%3&Count=%4"
                          "&Mode=UpTo&Time=%5")
                      .arg(BASE_LIVE, uic, QString::number(horizon), QString::number(count),
                           QString(QUrl::toPercentEncoding(time_str)));

    auto& http = BrokerHttp::instance();
    auto resp = http.get(url, auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_history failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_history: invalid response", ts};

    QJsonArray arr = doc.object().value("Data").toArray();
    QVector<BrokerCandle> candles;
    candles.reserve(arr.size());

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        // Time is ISO8601 string: "2024-01-15T00:00:00.000000Z"
        QDateTime dt = QDateTime::fromString(o.value("Time").toString(), Qt::ISODate);

        BrokerCandle c;
        c.timestamp = dt.isValid() ? dt.toMSecsSinceEpoch() : 0LL;
        // Stocks use Open/High/Low/Close/Volume directly
        c.open = o.value("Open").toDouble();
        c.high = o.value("High").toDouble();
        c.low = o.value("Low").toDouble();
        c.close = o.value("Close").toDouble();
        c.volume = static_cast<int64_t>(o.value("Volume").toDouble());

        // Fallback for FX (ask/bid split) — use mid
        if (c.open == 0.0) {
            c.open = (o.value("OpenAsk").toDouble() + o.value("OpenBid").toDouble()) / 2.0;
            c.high = (o.value("HighAsk").toDouble() + o.value("HighBid").toDouble()) / 2.0;
            c.low = (o.value("LowAsk").toDouble() + o.value("LowBid").toDouble()) / 2.0;
            c.close = (o.value("CloseAsk").toDouble() + o.value("CloseBid").toDouble()) / 2.0;
        }
        candles.append(c);
    }

    return {true, candles, "", ts};
}

} // namespace fincept::trading
