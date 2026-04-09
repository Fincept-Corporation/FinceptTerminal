#include "trading/brokers/tradier/TradierBroker.h"

#include "trading/brokers/BrokerHttp.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrlQuery>

namespace fincept::trading {

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// ---------- Static helpers ----------

QString TradierBroker::base(const BrokerCredentials& creds) {
    // api_secret holds the environment field value
    QString env = creds.api_secret.trimmed().toLower();
    if (env == "sandbox" || env == "paper")
        return "https://sandbox.tradier.com";
    return "https://api.tradier.com";
}

QString TradierBroker::tr_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:
            return "market";
        case OrderType::Limit:
            return "limit";
        case OrderType::StopLoss:
            return "stop";
        case OrderType::StopLossLimit:
            return "stop_limit";
        default:
            return "market";
    }
}

QString TradierBroker::tr_duration(ProductType p) {
    return (p == ProductType::Delivery) ? "gtc" : "day";
}

QJsonArray TradierBroker::normalize_array(const QJsonValue& val) {
    if (val.isArray())
        return val.toArray();
    if (val.isObject())
        return QJsonArray{val};
    return QJsonArray{};
}

bool TradierBroker::is_token_expired(const BrokerHttpResponse& resp) {
    return (resp.status_code == 401);
}

QString TradierBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    if (is_token_expired(resp))
        return "[TOKEN_EXPIRED] Access token is invalid or expired";
    if (!resp.success)
        return resp.error.isEmpty() ? fallback : resp.error;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (doc.isObject()) {
        // {"errors": {"error": "..."}} or {"fault": {"faultstring": "..."}}
        QJsonObject obj = doc.object();
        QString err = obj.value("errors").toObject().value("error").toString();
        if (!err.isEmpty())
            return err;
        err = obj.value("fault").toObject().value("faultstring").toString();
        if (!err.isEmpty())
            return err;
    }
    return fallback;
}

// ---------- Auth headers ----------

QMap<QString, QString> TradierBroker::auth_headers(const BrokerCredentials& creds) const {
    return {{"Authorization", "Bearer " + creds.access_token}, {"Accept", "application/json"}};
}

// ---------- exchange_token ----------
// ApiKey = access token, Environment field = "sandbox" or "live"
// Validates via GET /v1/user/profile, extracts account_id → user_id

TokenExchangeResponse TradierBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                    const QString& /*auth_code*/) {
    if (api_key.trimmed().isEmpty())
        return {false, "", "", "", "Access token is required"};

    QString env_base = (api_secret.trimmed().toLower() == "sandbox" || api_secret.trimmed().toLower() == "paper")
                           ? "https://sandbox.tradier.com"
                           : "https://api.tradier.com";

    auto& http = BrokerHttp::instance();
    auto resp = http.get(env_base + "/v1/user/profile",
                         {{"Authorization", "Bearer " + api_key}, {"Accept", "application/json"}});

    if (!resp.success) {
        if (resp.status_code == 401)
            return {false, "", "", "", "[TOKEN_EXPIRED] Access token is invalid or expired"};
        return {false, "", "", "", "Profile fetch failed: " + resp.error};
    }

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "", "", "Profile: invalid response"};

    QJsonObject profile = doc.object().value("profile").toObject();
    if (profile.isEmpty())
        return {false, "", "", "", "Profile: missing profile object"};

    // account may be object (single) or array (multiple) — take first
    QString account_id;
    QJsonValue acct_val = profile.value("account");
    if (acct_val.isArray()) {
        account_id = acct_val.toArray()[0].toObject().value("account_number").toString();
    } else {
        account_id = acct_val.toObject().value("account_number").toString();
    }

    if (account_id.isEmpty())
        return {false, "", "", "", "Profile: could not find account_number"};

    return {true, api_key, "", account_id, ""};
}

// ---------- place_order ----------
// POST form-encoded to /v1/accounts/{acct}/orders

OrderPlaceResponse TradierBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    QUrlQuery form;
    form.addQueryItem("class", "equity");
    form.addQueryItem("symbol", order.symbol);
    form.addQueryItem("side", (order.side == OrderSide::Buy) ? "buy" : "sell");
    form.addQueryItem("quantity", QString::number(static_cast<int>(order.quantity)));
    form.addQueryItem("type", tr_order_type(order.order_type));
    form.addQueryItem("duration", tr_duration(order.product_type));
    if (order.price > 0)
        form.addQueryItem("price", QString::number(order.price, 'f', 2));
    if (order.stop_price > 0)
        form.addQueryItem("stop", QString::number(order.stop_price, 'f', 2));
    form.addQueryItem("tag", "fincept");

    QMap<QString, QString> hdrs = auth_headers(creds);
    hdrs["Content-Type"] = "application/x-www-form-urlencoded";

    auto& http = BrokerHttp::instance();
    auto resp = http.post_raw(base(creds) + "/v1/accounts/" + acct + "/orders",
                              form.toString(QUrl::FullyEncoded).toUtf8(), hdrs);

    if (!resp.success)
        return {false, "", checked_error(resp, "place_order failed")};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "place_order: invalid response"};

    QJsonObject obj = doc.object();
    QJsonObject order_obj = obj.value("order").toObject();
    if (order_obj.isEmpty() || order_obj.value("status").toString() != "ok")
        return {false, "",
                checked_error(resp, obj.value("errors").toObject().value("error").toString("place_order failed"))};

    QString order_id = QString::number(order_obj.value("id").toVariant().toLongLong());
    return {true, order_id, ""};
}

// ---------- modify_order ----------

ApiResponse<QJsonObject> TradierBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                     const QJsonObject& mods) {
    int64_t ts = now_ts();
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    QUrlQuery form;
    if (mods.contains("type"))
        form.addQueryItem("type", mods.value("type").toString());
    if (mods.contains("duration"))
        form.addQueryItem("duration", mods.value("duration").toString());
    if (mods.contains("price"))
        form.addQueryItem("price", QString::number(mods.value("price").toDouble(), 'f', 2));
    if (mods.contains("stop"))
        form.addQueryItem("stop", QString::number(mods.value("stop").toDouble(), 'f', 2));

    QMap<QString, QString> hdrs = auth_headers(creds);
    hdrs["Content-Type"] = "application/x-www-form-urlencoded";

    auto& http = BrokerHttp::instance();
    auto resp = http.put_raw(base(creds) + "/v1/accounts/" + acct + "/orders/" + order_id,
                             form.toString(QUrl::FullyEncoded).toUtf8(), hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "modify_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "modify_order: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("order").toObject().value("status").toString() != "ok")
        return {false, std::nullopt, checked_error(resp, "modify_order failed"), ts};

    return {true, obj, "", ts};
}

// ---------- cancel_order ----------

ApiResponse<QJsonObject> TradierBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    int64_t ts = now_ts();
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.del(base(creds) + "/v1/accounts/" + acct + "/orders/" + order_id, auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "cancel_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "cancel_order: invalid response", ts};

    return {true, doc.object(), "", ts};
}

// ---------- get_orders ----------

ApiResponse<QVector<BrokerOrderInfo>> TradierBroker::get_orders(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.get(base(creds) + "/v1/accounts/" + acct + "/orders?includeTags=true", auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_orders failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_orders: invalid response", ts};

    QJsonValue orders_val = doc.object().value("orders");
    // "null" string means no orders
    if (orders_val.isString())
        return {true, QVector<BrokerOrderInfo>{}, "", ts};

    QJsonArray arr = normalize_array(orders_val.toObject().value("order"));
    QVector<BrokerOrderInfo> orders;
    orders.reserve(arr.size());

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = QString::number(o.value("id").toVariant().toLongLong());
        info.symbol = o.value("symbol").toString();
        info.exchange = o.value("exch").toString();
        info.quantity = o.value("quantity").toDouble();
        info.filled_qty = o.value("exec_quantity").toDouble();
        info.price = o.value("price").toDouble();
        info.stop_price = o.value("stop_price").toDouble();
        info.avg_price = o.value("avg_fill_price").toDouble();

        QString st = o.value("status").toString();
        if (st == "filled")
            info.status = "filled";
        else if (st == "canceled" || st == "expired")
            info.status = "cancelled";
        else if (st == "rejected" || st == "error")
            info.status = "rejected";
        else
            info.status = "open";

        info.side = o.value("side").toString();
        info.order_type = o.value("type").toString();
        info.product_type = o.value("duration").toString();
        info.timestamp = o.value("create_date").toString();
        orders.append(info);
    }

    return {true, orders, "", ts};
}

// ---------- get_trade_book ----------

ApiResponse<QJsonObject> TradierBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.get(base(creds) + "/v1/accounts/" + acct + "/orders", auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_trade_book failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_trade_book: invalid response", ts};

    return {true, doc.object(), "", ts};
}

// ---------- get_positions ----------

ApiResponse<QVector<BrokerPosition>> TradierBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.get(base(creds) + "/v1/accounts/" + acct + "/positions", auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_positions failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_positions: invalid response", ts};

    QJsonValue pos_val = doc.object().value("positions");
    if (pos_val.isString())
        return {true, QVector<BrokerPosition>{}, "", ts};

    QJsonArray arr = normalize_array(pos_val.toObject().value("position"));
    QVector<BrokerPosition> positions;

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        double qty = o.value("quantity").toDouble();
        if (qty == 0.0)
            continue;

        BrokerPosition pos;
        pos.symbol = o.value("symbol").toString();
        pos.exchange = "US";
        pos.quantity = qty;
        pos.avg_price = (qty != 0) ? o.value("cost_basis").toDouble() / qty : 0.0;
        pos.side = qty > 0 ? "LONG" : "SHORT";
        positions.append(pos);
    }

    return {true, positions, "", ts};
}

// ---------- get_holdings ----------
// Tradier has no separate holdings — map to positions (long only)

ApiResponse<QVector<BrokerHolding>> TradierBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.get(base(creds) + "/v1/accounts/" + acct + "/positions", auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_holdings: invalid response", ts};

    QJsonValue pos_val = doc.object().value("positions");
    if (pos_val.isString())
        return {true, QVector<BrokerHolding>{}, "", ts};

    QJsonArray arr = normalize_array(pos_val.toObject().value("position"));
    QVector<BrokerHolding> holdings;

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        double qty = o.value("quantity").toDouble();
        if (qty <= 0.0)
            continue; // long positions only for holdings

        BrokerHolding h;
        h.symbol = o.value("symbol").toString();
        h.exchange = "US";
        h.quantity = qty;
        h.avg_price = (qty > 0) ? o.value("cost_basis").toDouble() / qty : 0.0;
        h.invested_value = o.value("cost_basis").toDouble();
        holdings.append(h);
    }

    return {true, holdings, "", ts};
}

// ---------- get_funds ----------

ApiResponse<BrokerFunds> TradierBroker::get_funds(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.get(base(creds) + "/v1/accounts/" + acct + "/balances", auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_funds: invalid response", ts};

    QJsonObject bal = doc.object().value("balances").toObject();

    // cash sub-object (present for cash accounts and margin accounts)
    QJsonObject cash_obj = bal.value("cash").toObject();
    QJsonObject margin_obj = bal.value("margin").toObject();

    double cash_available = cash_obj.value("cash_available").toDouble();
    if (cash_available == 0.0)
        cash_available = margin_obj.value("stock_buying_power").toDouble() / 2.0;

    BrokerFunds funds;
    funds.available_balance = cash_available;
    funds.total_balance = bal.value("total_equity").toDouble();
    funds.used_margin = bal.value("market_value").toDouble(); // cost basis of open positions
    funds.collateral = margin_obj.value("stock_buying_power").toDouble();

    return {true, funds, "", ts};
}

// ---------- get_quotes ----------
// GET /v1/markets/quotes?symbols=AAPL,MSFT

ApiResponse<QVector<BrokerQuote>> TradierBroker::get_quotes(const BrokerCredentials& creds,
                                                            const QVector<QString>& symbols) {
    int64_t ts = now_ts();
    if (symbols.isEmpty())
        return {true, QVector<BrokerQuote>{}, "", ts};

    // Strip exchange prefix if present (e.g. "NASDAQ:AAPL" → "AAPL")
    QStringList tickers;
    for (const QString& sym : symbols) {
        tickers.append(sym.contains(':') ? sym.section(':', 1) : sym);
    }

    auto& http = BrokerHttp::instance();
    auto resp = http.get(base(creds) + "/v1/markets/quotes?symbols=" + tickers.join(",") + "&greeks=false",
                         auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_quotes failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_quotes: invalid response", ts};

    QJsonArray arr = normalize_array(doc.object().value("quotes").toObject().value("quote"));
    QVector<BrokerQuote> quotes;
    quotes.reserve(arr.size());

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        BrokerQuote q;
        q.symbol = o.value("symbol").toString();
        q.ltp = o.value("last").toDouble();
        q.open = o.value("open").toDouble();
        q.high = o.value("high").toDouble();
        q.low = o.value("low").toDouble();
        q.close = o.value("prevclose").toDouble(); // prevclose is reliable; close is null during hours
        q.volume = static_cast<int64_t>(o.value("volume").toDouble());
        q.change = o.value("change").toDouble();
        q.change_pct = o.value("change_percentage").toDouble();
        q.timestamp = ts;
        quotes.append(q);
    }

    return {true, quotes, "", ts};
}

// ---------- get_history ----------
// Daily/weekly/monthly: GET /v1/markets/history
// Intraday:             GET /v1/markets/timesales

ApiResponse<QVector<BrokerCandle>> TradierBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                              const QString& resolution, const QString& from_date,
                                                              const QString& to_date) {
    int64_t ts = now_ts();

    // Strip exchange prefix
    QString ticker = symbol.contains(':') ? symbol.section(':', 1) : symbol;
    // Also strip conid if present (3-part format)
    if (ticker.contains(':'))
        ticker = ticker.section(':', 0, 0);

    auto& http = BrokerHttp::instance();
    QVector<BrokerCandle> candles;

    bool is_intraday = (resolution == "1" || resolution == "5" || resolution == "15" || resolution == "1m" ||
                        resolution == "5m" || resolution == "15m");

    if (is_intraday) {
        // timesales endpoint
        QString intrv;
        if (resolution == "1" || resolution == "1m")
            intrv = "1min";
        else if (resolution == "5" || resolution == "5m")
            intrv = "5min";
        else
            intrv = "15min";

        // timesales expects datetime format "YYYY-MM-DD HH:MM"
        QString start = from_date + " 09:30";
        QString end = to_date + " 16:00";

        QString url = base(creds) + "/v1/markets/timesales?symbol=" + ticker + "&interval=" + intrv +
                      "&start=" + QUrl::toPercentEncoding(start) + "&end=" + QUrl::toPercentEncoding(end);

        auto resp = http.get(url, auth_headers(creds));
        if (!resp.success)
            return {false, std::nullopt, checked_error(resp, "get_history failed"), ts};

        QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
        if (!doc.isObject())
            return {false, std::nullopt, "get_history: invalid response", ts};

        QJsonValue series_val = doc.object().value("series");
        if (series_val.isNull() || series_val.isString())
            return {true, candles, "", ts};

        QJsonArray arr = normalize_array(series_val.toObject().value("data"));
        candles.reserve(arr.size());

        for (const QJsonValue& v : arr) {
            QJsonObject o = v.toObject();
            BrokerCandle c;
            // timesales has both "time" (ISO string) and "timestamp" (epoch seconds)
            c.timestamp = static_cast<int64_t>(o.value("timestamp").toDouble()) * 1000LL;
            c.open = o.value("open").toDouble();
            c.high = o.value("high").toDouble();
            c.low = o.value("low").toDouble();
            c.close = o.value("close").toDouble();
            c.volume = static_cast<int64_t>(o.value("volume").toDouble());
            candles.append(c);
        }

    } else {
        // history endpoint — daily/weekly/monthly
        QString intrv;
        if (resolution == "W" || resolution == "1W")
            intrv = "weekly";
        else if (resolution == "M" || resolution == "1M")
            intrv = "monthly";
        else
            intrv = "daily";

        QString url = base(creds) + "/v1/markets/history?symbol=" + ticker + "&interval=" + intrv +
                      "&start=" + from_date + "&end=" + to_date;

        auto resp = http.get(url, auth_headers(creds));
        if (!resp.success)
            return {false, std::nullopt, checked_error(resp, "get_history failed"), ts};

        QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
        if (!doc.isObject())
            return {false, std::nullopt, "get_history: invalid response", ts};

        QJsonValue hist_val = doc.object().value("history");
        if (hist_val.isNull() || hist_val.isString())
            return {true, candles, "", ts};

        QJsonArray arr = normalize_array(hist_val.toObject().value("day"));
        candles.reserve(arr.size());

        for (const QJsonValue& v : arr) {
            QJsonObject o = v.toObject();
            BrokerCandle c;
            // date is "YYYY-MM-DD" string — convert to epoch ms
            QDate d = QDate::fromString(o.value("date").toString(), "yyyy-MM-dd");
            c.timestamp = d.isValid()
                              ? static_cast<int64_t>(QDateTime(d, QTime(0, 0, 0), Qt::UTC).toSecsSinceEpoch()) * 1000LL
                              : 0LL;
            c.open = o.value("open").toDouble();
            c.high = o.value("high").toDouble();
            c.low = o.value("low").toDouble();
            c.close = o.value("close").toDouble();
            c.volume = static_cast<int64_t>(o.value("volume").toDouble());
            candles.append(c);
        }
    }

    return {true, candles, "", ts};
}

} // namespace fincept::trading
