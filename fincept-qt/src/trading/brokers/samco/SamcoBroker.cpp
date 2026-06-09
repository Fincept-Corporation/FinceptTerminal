#include "trading/brokers/samco/SamcoBroker.h"

#include "trading/brokers/BrokerHttp.h"
#include "trading/brokers/BrokerTokenUtil.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrlQuery>

namespace fincept::trading {

static constexpr const char* BASE = "https://tradeapi.samco.in";

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

static double samco_d(const QJsonValue& v) {
    if (v.isString())
        return v.toString().remove(',').toDouble();
    return v.toDouble();
}
static qint64 samco_i(const QJsonValue& v) {
    if (v.isString())
        return static_cast<qint64>(v.toString().remove(',').toDouble());
    return static_cast<qint64>(v.toDouble());
}
// Pick the first present key from an object (Samco aliases some fields).
static QJsonValue samco_first(const QJsonObject& o, const QString& a, const QString& b) {
    return o.contains(a) ? o.value(a) : o.value(b);
}

// ---------- Static helpers ----------

const BrokerEnumMap<QString>& SamcoBroker::sm_enum_map() {
    static const auto m = [] {
        BrokerEnumMap<QString> x;
        x.set(OrderType::Market, "MKT");
        x.set(OrderType::Limit, "L");
        x.set(OrderType::StopLoss, "SL-M");
        x.set(OrderType::StopLossLimit, "SL");
        x.set(ProductType::Intraday, "MIS");
        x.set(ProductType::Delivery, "CNC");
        x.set(ProductType::Margin, "NRML");
        return x;
    }();
    return m;
}

QString SamcoBroker::sm_status(const QString& s) {
    QString sl = s.toLower();
    if (sl == "complete" || sl == "completed" || sl == "executed" || sl == "filled")
        return "filled";
    if (sl == "open" || sl == "pending" || sl == "ordered" || sl == "trigger pending" ||
        sl == "after market order req received")
        return "open";
    if (sl == "cancelled" || sl == "canceled")
        return "cancelled";
    if (sl == "rejected")
        return "rejected";
    return "open";
}

bool SamcoBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 401 || resp.status_code == 403)
        return true;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return false;
    QJsonObject obj = doc.object();
    if (obj.value("status").toString() == "Success")
        return false;
    QString msg = obj.value("statusMessage").toString().toLower();
    return msg.contains("session") || msg.contains("token") || msg.contains("invalid") || msg.contains("expired") ||
           msg.contains("login");
}

QString SamcoBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    if (is_token_expired(resp))
        return "[TOKEN_EXPIRED] Session expired, please re-login";
    if (!resp.success && resp.status_code == 0)
        return resp.error.isEmpty() ? fallback : resp.error;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (doc.isObject()) {
        QString msg = doc.object().value("statusMessage").toString();
        if (!msg.isEmpty())
            return msg;
    }
    return fallback;
}

// ---------- Auth headers ----------

QMap<QString, QString> SamcoBroker::auth_headers(const BrokerCredentials& creds) const {
    return {{"x-session-token", creds.access_token}, {"Accept", "application/json"}};
}

// ---------- exchange_token ----------
// ApiKey=uid, ApiSecret=password, AuthCode=secret_api_key
//   POST /accessToken/token {uid, secretApiKey} → accessToken
//   POST /login {userId, password, accessToken} → sessionToken

TokenExchangeResponse SamcoBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                  const QString& auth_code) {
    if (api_key.isEmpty() || api_secret.isEmpty() || auth_code.isEmpty())
        return {false, "", "", "", "Samco login: user id, password and secret API key are required", ""};

    auto& http = BrokerHttp::instance();

    // Step 1: access token from the permanent secret API key.
    QJsonObject tok_payload;
    tok_payload["uid"] = api_key;
    tok_payload["secretApiKey"] = auth_code;
    auto tok_resp = http.post_json(QString("%1/accessToken/token").arg(BASE), tok_payload,
                                   {{"Content-Type", "application/json"}, {"Accept", "application/json"}});
    if (!tok_resp.success && tok_resp.status_code == 0)
        return {false, "", "", "", "Access token request failed: " + tok_resp.error, ""};

    QJsonDocument tok_doc = QJsonDocument::fromJson(tok_resp.raw_body.toUtf8());
    if (!tok_doc.isObject())
        return {false, "", "", "", "Access token: invalid response", ""};
    QJsonObject tok_obj = tok_doc.object();
    if (tok_obj.value("status").toString() != "Success")
        return {false, "", "", "", tok_obj.value("statusMessage").toString("Access token generation failed"), ""};
    QString access_token = tok_obj.value("accessToken").toString();
    if (access_token.isEmpty())
        return {false, "", "", "", "Access token: empty token in response", ""};

    // Step 2: login → sessionToken.
    QJsonObject login_payload;
    login_payload["userId"] = api_key;
    login_payload["password"] = api_secret;
    login_payload["accessToken"] = access_token;
    auto login_resp = http.post_json(QString("%1/login").arg(BASE), login_payload,
                                     {{"Content-Type", "application/json"}, {"Accept", "application/json"}});
    if (!login_resp.success && login_resp.status_code == 0)
        return {false, "", "", "", "Login failed: " + login_resp.error, ""};

    QJsonDocument login_doc = QJsonDocument::fromJson(login_resp.raw_body.toUtf8());
    if (!login_doc.isObject())
        return {false, "", "", "", "Login: invalid response", ""};
    QJsonObject login_obj = login_doc.object();
    if (login_obj.value("status").toString() != "Success")
        return {false, "", "", "", login_obj.value("statusMessage").toString("Login failed"), ""};
    QString session = login_obj.value("sessionToken").toString();
    if (session.isEmpty())
        return {false, "", "", "", "Login: no sessionToken in response", ""};

    // Samco's secretApiKey (auth_code) is permanent, and uid/password are stored
    // — persist the secret so the daily session can be silently re-minted. Token
    // lapses at the daily reset.
    QJsonObject extra_obj{{"secret_api_key", auth_code}};
    const QString extra =
        with_token_expiry(QString::fromUtf8(QJsonDocument(extra_obj).toJson(QJsonDocument::Compact)),
                          next_ist_flush_epoch(6, 0));
    return {true, session, "", api_key, extra, ""};
}

// Silent refresh = replay the two-step login using the stored uid/password and
// the persisted permanent secretApiKey.
TokenExchangeResponse SamcoBroker::refresh_session(const BrokerCredentials& creds) {
    const auto extra = QJsonDocument::fromJson(creds.additional_data.toUtf8()).object();
    const QString secret_api_key = extra.value("secret_api_key").toString();
    if (creds.api_key.isEmpty() || creds.api_secret.isEmpty() || secret_api_key.isEmpty())
        return {false, "", "", "", "", "Samco silent refresh requires stored user id, password and secret API key"};
    return exchange_token(creds.api_key, creds.api_secret, secret_api_key);
}

// ---------- place_order ----------
// POST /order/placeOrder  (JSON, x-session-token)

OrderPlaceResponse SamcoBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    const QString order_type = sm_enum_map().order_type_or(order.order_type, "MKT");

    QJsonObject payload;
    payload["symbolName"] = order.symbol;
    payload["exchange"] = order.exchange;
    payload["transactionType"] = (order.side == OrderSide::Buy) ? "BUY" : "SELL";
    payload["orderType"] = order_type;
    payload["quantity"] = QString::number(static_cast<int>(order.quantity));
    payload["disclosedQuantity"] = "0";
    payload["orderValidity"] = "DAY";
    payload["productType"] = sm_enum_map().product_or(order.product_type, "MIS");
    payload["afterMarketOrderFlag"] = order.amo ? "YES" : "NO";
    if (order_type == "L" || order_type == "SL")
        payload["price"] = QString::number(order.price, 'f', 2);
    if (order_type == "SL" || order_type == "SL-M")
        payload["triggerPrice"] = QString::number(order.stop_price, 'f', 2);

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(QString("%1/order/placeOrder").arg(BASE), payload, auth_headers(creds));

    if (!resp.success && resp.status_code == 0)
        return {false, "", checked_error(resp, "place_order failed")};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "place_order: invalid response"};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "Success")
        return {false, "", obj.value("statusMessage").toString("place_order failed")};

    return {true, obj.value("orderNumber").toString(), ""};
}

// ---------- modify_order ----------
// PUT /order/modifyOrder/{orderNumber}

ApiResponse<QJsonObject> SamcoBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                   const QJsonObject& mods) {
    int64_t ts = now_ts();

    const QString price_type = mods.value("orderType").toString("L");
    QJsonObject payload;
    payload["orderType"] = price_type;
    payload["quantity"] = QString::number(mods.value("quantity").toInt(0));
    payload["orderValidity"] = "DAY";
    if (price_type == "L" || price_type == "SL")
        payload["price"] = QString::number(mods.value("price").toDouble(0), 'f', 2);
    if (price_type == "SL" || price_type == "SL-M")
        payload["triggerPrice"] = QString::number(mods.value("triggerPrice").toDouble(0), 'f', 2);

    auto& http = BrokerHttp::instance();
    auto resp = http.put_json(QString("%1/order/modifyOrder/%2").arg(BASE, order_id), payload, auth_headers(creds));

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "modify_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "modify_order: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "Success")
        return {false, std::nullopt, obj.value("statusMessage").toString("modify_order failed"), ts};

    return {true, obj, "", ts};
}

// ---------- cancel_order ----------
// DELETE /order/cancelOrder?orderNumber=<id>

ApiResponse<QJsonObject> SamcoBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    int64_t ts = now_ts();

    QUrlQuery q;
    q.addQueryItem("orderNumber", order_id);
    QString url = QString("%1/order/cancelOrder?%2").arg(BASE, q.toString(QUrl::FullyEncoded));

    auto& http = BrokerHttp::instance();
    auto resp = http.del(url, auth_headers(creds));

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "cancel_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "cancel_order: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "Success")
        return {false, std::nullopt, obj.value("statusMessage").toString("cancel_order failed"), ts};

    return {true, obj, "", ts};
}

// ---------- get_orders ----------
// GET /order/orderBook

ApiResponse<QVector<BrokerOrderInfo>> SamcoBroker::get_orders(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/order/orderBook").arg(BASE), auth_headers(creds));

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_orders failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_orders: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "Success") {
        QString msg = obj.value("statusMessage").toString().toLower();
        if (msg.contains("no data") || msg.contains("no order"))
            return {true, QVector<BrokerOrderInfo>{}, "", ts};
        return {false, std::nullopt, checked_error(resp, "get_orders failed"), ts};
    }

    QVector<BrokerOrderInfo> orders;
    for (const QJsonValue& v : obj.value("orderBookDetails").toArray()) {
        QJsonObject o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("orderNumber").toString();
        info.symbol = o.value("tradingSymbol").toString(o.value("symbol").toString());
        info.exchange = o.value("exchange").toString();
        info.quantity = samco_i(samco_first(o, "totalQuantity", "quantity"));
        info.filled_qty = samco_i(o.value("filledQuantity"));
        info.price = samco_d(samco_first(o, "orderPrice", "price"));
        info.stop_price = samco_d(o.value("triggerPrice"));
        info.avg_price = samco_d(o.value("averagePrice"));
        info.status = sm_status(o.value("orderStatus").toString());
        info.side = o.value("transactionType").toString().toLower();
        info.order_type = o.value("orderType").toString();
        info.product_type = o.value("productCode").toString();
        info.timestamp = o.value("orderTime").toString();
        info.message = o.value("statusMessage").toString();
        orders.append(info);
    }

    return {true, orders, "", ts};
}

// ---------- get_trade_book ----------
// GET /trade/tradeBook

ApiResponse<QJsonObject> SamcoBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/trade/tradeBook").arg(BASE), auth_headers(creds));

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_trade_book failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (doc.isObject())
        return {true, doc.object(), "", ts};
    return {false, std::nullopt, "get_trade_book: invalid response", ts};
}

// ---------- get_positions ----------
// GET /position/getPositions?positionType=DAY

ApiResponse<QVector<BrokerPosition>> SamcoBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/position/getPositions?positionType=DAY").arg(BASE), auth_headers(creds));

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_positions failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_positions: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "Success") {
        QString msg = obj.value("statusMessage").toString().toLower();
        if (msg.contains("no data") || msg.contains("no position"))
            return {true, QVector<BrokerPosition>{}, "", ts};
        return {false, std::nullopt, checked_error(resp, "get_positions failed"), ts};
    }

    QVector<BrokerPosition> positions;
    for (const QJsonValue& v : obj.value("positionDetails").toArray()) {
        QJsonObject o = v.toObject();
        int net_qty = static_cast<int>(samco_i(o.value("netQuantity")));
        if (net_qty == 0)
            continue;
        // Samco reports a positive netQuantity; transactionType carries the sign.
        if (o.value("transactionType").toString().toUpper() == "SELL" && net_qty > 0)
            net_qty = -net_qty;

        BrokerPosition pos;
        pos.symbol = o.value("tradingSymbol").toString();
        pos.exchange = o.value("exchange").toString();
        pos.product_type = o.value("productCode").toString();
        pos.quantity = net_qty;
        pos.avg_price = samco_d(o.value("averagePrice"));
        pos.ltp = samco_d(o.value("lastTradedPrice"));
        pos.pnl = samco_d(o.value("realizedGainAndLoss")) + samco_d(o.value("mtmGainLoss"));
        pos.pnl_pct = (pos.avg_price > 0.0) ? ((pos.ltp - pos.avg_price) / pos.avg_price) * 100.0 : 0.0;
        pos.side = net_qty > 0 ? "LONG" : "SHORT";
        positions.append(pos);
    }

    return {true, positions, "", ts};
}

// ---------- get_holdings ----------
// GET /holding/getHoldings

ApiResponse<QVector<BrokerHolding>> SamcoBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/holding/getHoldings").arg(BASE), auth_headers(creds));

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_holdings: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "Success") {
        QString msg = obj.value("statusMessage").toString().toLower();
        if (msg.contains("no data") || msg.contains("no holding"))
            return {true, QVector<BrokerHolding>{}, "", ts};
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};
    }

    QVector<BrokerHolding> holdings;
    for (const QJsonValue& v : obj.value("holdingDetails").toArray()) {
        QJsonObject o = v.toObject();
        BrokerHolding h;
        h.symbol = o.value("tradingSymbol").toString(o.value("symbol").toString());
        h.exchange = o.value("exchange").toString();
        h.quantity = samco_d(samco_first(o, "holdingsQuantity", "quantity"));
        h.avg_price = samco_d(o.value("averagePrice"));
        h.ltp = samco_d(o.value("lastTradedPrice"));
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
// GET /limit/getLimits

ApiResponse<BrokerFunds> SamcoBroker::get_funds(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.get(QString("%1/limit/getLimits").arg(BASE), auth_headers(creds));

    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_funds: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "Success")
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    QJsonObject eq = obj.value("equityLimit").toObject();
    BrokerFunds funds;
    funds.available_balance = samco_d(eq.value("netAvailableMargin"));
    funds.used_margin = samco_d(eq.value("marginUsed"));
    funds.collateral = samco_d(eq.value("collateralMarginAgainstShares"));
    funds.total_balance = funds.available_balance + funds.used_margin;
    funds.raw_data = obj;

    return {true, funds, "", ts};
}

// ---------- get_quotes ----------
// GET /quote/getQuote?symbolName=<sym>[&exchange=<exch>]
// Symbol format: "EXCHANGE:SYMBOL" or just the broker symbol.

ApiResponse<QVector<BrokerQuote>> SamcoBroker::get_quotes(const BrokerCredentials& creds,
                                                          const QVector<QString>& symbols) {
    int64_t ts = now_ts();
    if (symbols.isEmpty())
        return {true, QVector<BrokerQuote>{}, "", ts};

    auto& http = BrokerHttp::instance();
    QVector<BrokerQuote> quotes;

    for (const QString& sym : symbols) {
        QStringList parts = sym.split(":");
        QString exchange = (parts.size() >= 2) ? parts[0] : "NSE";
        QString name = (parts.size() >= 2) ? parts[1] : sym;

        QUrlQuery q;
        q.addQueryItem("symbolName", name);
        if (!exchange.isEmpty() && exchange != "NSE")
            q.addQueryItem("exchange", exchange);
        QString url = QString("%1/quote/getQuote?%2").arg(BASE, q.toString(QUrl::FullyEncoded));

        auto resp = http.get(url, auth_headers(creds));
        if (!resp.success && resp.status_code == 0)
            continue;

        QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
        if (!doc.isObject())
            continue;
        QJsonObject obj = doc.object();
        if (obj.value("status").toString() != "Success")
            continue;

        QJsonObject quote = obj.value("quoteDetails").toObject();
        BrokerQuote bq;
        bq.symbol = name;
        bq.ltp = samco_d(quote.value("lastTradedPrice"));
        bq.open = samco_d(quote.value("openValue"));
        bq.high = samco_d(quote.value("highValue"));
        bq.low = samco_d(quote.value("lowValue"));
        bq.close = samco_d(quote.value("previousClose"));
        bq.volume = samco_d(quote.value("totalTradedVolume"));
        bq.oi = samco_i(quote.value("openInterest"));
        QJsonArray bids = quote.value("bestBids").toArray();
        QJsonArray asks = quote.value("bestAsks").toArray();
        if (!bids.isEmpty())
            bq.bid = samco_d(bids[0].toObject().value("price"));
        if (!asks.isEmpty())
            bq.ask = samco_d(asks[0].toObject().value("price"));
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
// Daily:    GET /history/candleData?symbolName=<sym>&fromDate=<d>&toDate=<d>[&exchange=]
// Intraday: GET /intraday/candleData?symbolName=<sym>&fromDate=<d %H:%M:%S>&toDate=<...>&interval=<min>

ApiResponse<QVector<BrokerCandle>> SamcoBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                            const QString& resolution, const QString& from_date,
                                                            const QString& to_date) {
    int64_t ts = now_ts();

    QStringList parts = symbol.split(":");
    QString exchange = (parts.size() >= 2) ? parts[0] : "NSE";
    QString name = (parts.size() >= 2) ? parts[1] : symbol;

    QDate from = QDate::fromString(from_date, "yyyy-MM-dd");
    QDate to = QDate::fromString(to_date, "yyyy-MM-dd");
    if (!from.isValid())
        from = QDate::currentDate().addYears(-1);
    if (!to.isValid())
        to = QDate::currentDate();

    bool is_daily = (resolution == "D" || resolution == "1D" || resolution == "W" || resolution == "M");
    auto& http = BrokerHttp::instance();
    QString url, data_key;

    if (is_daily) {
        QUrlQuery q;
        q.addQueryItem("symbolName", name);
        q.addQueryItem("fromDate", from.toString("yyyy-MM-dd"));
        q.addQueryItem("toDate", to.toString("yyyy-MM-dd"));
        if (!exchange.isEmpty() && exchange != "NSE")
            q.addQueryItem("exchange", exchange);
        url = QString("%1/history/candleData?%2").arg(BASE, q.toString(QUrl::FullyEncoded));
        data_key = "historicalCandleData";
    } else {
        static const QMap<QString, QString> interval_map = {
            {"1", "1"}, {"5", "5"}, {"10", "10"}, {"15", "15"}, {"30", "30"}, {"60", "60"},
        };
        QString interval = interval_map.value(resolution, "5");
        QUrlQuery q;
        q.addQueryItem("symbolName", name);
        q.addQueryItem("fromDate", from.toString("yyyy-MM-dd") + " 00:00:00");
        q.addQueryItem("toDate", to.toString("yyyy-MM-dd") + " 23:59:59");
        q.addQueryItem("interval", interval);
        if (!exchange.isEmpty() && exchange != "NSE")
            q.addQueryItem("exchange", exchange);
        url = QString("%1/intraday/candleData?%2").arg(BASE, q.toString(QUrl::FullyEncoded));
        data_key = "intradayCandleData";
    }

    auto resp = http.get(url, auth_headers(creds));
    if (!resp.success && resp.status_code == 0)
        return {false, std::nullopt, checked_error(resp, "get_history failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_history: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "Success")
        return {false, std::nullopt, checked_error(resp, "get_history failed"), ts};

    QVector<BrokerCandle> candles;
    for (const QJsonValue& v : obj.value(data_key).toArray()) {
        QJsonObject o = v.toObject();
        BrokerCandle c;
        // Daily: "date" (yyyy-MM-dd). Intraday: "dateTime" (yyyy-MM-dd HH:mm:ss).
        QString date_str = o.value("dateTime").toString(o.value("date").toString());
        QDateTime dt = QDateTime::fromString(date_str, "yyyy-MM-dd HH:mm:ss");
        if (!dt.isValid())
            dt = QDateTime(QDate::fromString(date_str.left(10), "yyyy-MM-dd"), QTime(0, 0, 0));
        c.timestamp = dt.isValid() ? dt.toSecsSinceEpoch() * 1000LL : 0LL;
        c.open = samco_d(o.value("open"));
        c.high = samco_d(o.value("high"));
        c.low = samco_d(o.value("low"));
        c.close = samco_d(o.value("close"));
        c.volume = samco_d(o.value("volume"));
        candles.append(c);
    }

    return {true, candles, "", ts};
}

} // namespace fincept::trading
