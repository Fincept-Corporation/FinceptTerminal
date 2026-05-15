#include "trading/brokers/motilal/MotilalBroker.h"

#include "trading/brokers/BrokerHttp.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>

namespace fincept::trading {

static constexpr const char* BASE = "https://openapi.motilaloswal.com";

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// ---------- Static helpers ----------

QString MotilalBroker::mo_exchange(const QString& exchange) {
    if (exchange == "NFO")
        return "NSEFO";
    if (exchange == "CDS")
        return "NSECD";
    if (exchange == "BFO")
        return "BSEFO";
    // NSE, BSE, MCX, NCDEX pass through unchanged
    return exchange.toUpper();
}

const BrokerEnumMap<QString>& MotilalBroker::mo_enum_map() {
    static const auto m = [] {
        BrokerEnumMap<QString> x;
        x.set(OrderType::Market, "MARKET");
        x.set(OrderType::Limit, "LIMIT");
        x.set(OrderType::StopLoss, "STOPLOSSMARKET"); // SL-M: market-with-trigger
        x.set(OrderType::StopLossLimit, "STOPLOSS"); // SL:    limit-with-trigger
        x.set(ProductType::Intraday, "VALUEPLUS");
        x.set(ProductType::Delivery, "DELIVERY");
        x.set(ProductType::Margin, "NORMAL");
        return x;
    }();
    return m;
}

QString MotilalBroker::mo_status(const QString& s) {
    QString sl = s.toLower();
    if (sl == "traded")
        return "filled";
    if (sl == "confirm" || sl == "sent" || sl == "open" || sl == "trigger pending")
        return "open";
    if (sl == "cancel")
        return "cancelled";
    if (sl == "rejected" || sl == "error")
        return "rejected";
    return "open";
}

bool MotilalBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 403 || resp.status_code == 401)
        return true;
    if (!resp.success)
        return false;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return false;
    QString status = doc.object().value("status").toString();
    if (status == "SUCCESS")
        return false;
    QString msg = doc.object().value("message").toString().toLower();
    return msg.contains("invalid") || msg.contains("session") || msg.contains("token") ||
           msg.contains("unauthorized") || msg.contains("login") || msg.contains("auth");
}

QString MotilalBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    if (is_token_expired(resp))
        return "[TOKEN_EXPIRED] Session expired, please re-login";
    if (!resp.success)
        return resp.error.isEmpty() ? fallback : resp.error;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (doc.isObject()) {
        QString msg = doc.object().value("message").toString();
        if (!msg.isEmpty())
            return msg;
    }
    return fallback;
}

// ---------- Auth headers ----------

QMap<QString, QString> MotilalBroker::auth_headers(const BrokerCredentials& creds) const {
    // Motilal's API documentation lists both `accesstoken` and `Authorization`
    // as mandatory on authenticated calls — current deployments accept either
    // alone, but sending both is the safe form per FAQ. ApiSecretKey is also
    // documented as mandatory; we don't have a separate API-secret slot in
    // our credential model (api_secret = login password), so use api_secret
    // here and let the user-facing UI document this packing.
    //
    // vendorinfo carries the trading account identifier (UCC). An empty
    // vendorinfo causes a class of permission errors on dealer-flagged accounts.
    return {
        {"Authorization", creds.access_token},
        {"accesstoken", creds.access_token}, // duplicate of Authorization per doc
        {"Content-Type", "application/json"},
        {"Accept", "application/json"},
        {"ApiKey", creds.api_key},
        {"ApiSecretKey", creds.api_secret},
        {"User-Agent", "MOSL/V.1.1.0"},
        {"ClientLocalIp", "127.0.0.1"},
        {"ClientPublicIp", "127.0.0.1"},
        {"MacAddress", "00:00:00:00:00:00"},
        {"SourceId", "WEB"},
        {"vendorinfo", creds.user_id.isEmpty() ? creds.api_key : creds.user_id},
        {"osname", "Windows 10"},
        {"osversion", "10.0"},
        {"devicemodel", "PC"},
        {"manufacturer", "Generic"},
        {"productname", "FinceptTerminal"},
        {"productversion", "4.0.3"},
        {"browsername", "Chrome"},
        {"browserversion", "120.0"},
    };
}

// ---------- exchange_token ----------
// ApiKey    = api_key
// ApiSecret = password
// AuthCode  = "dob:::totp"

TokenExchangeResponse MotilalBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                    const QString& auth_code) {
    QStringList auth_parts = auth_code.split(":::");
    QString dob = (auth_parts.size() >= 1) ? auth_parts[0] : "";
    QString totp = (auth_parts.size() >= 2) ? auth_parts[1] : "";

    // SHA256(password + api_key)
    QString raw = api_secret + api_key;
    QString hashed_pw = QString(QCryptographicHash::hash(raw.toUtf8(), QCryptographicHash::Sha256).toHex());

    QJsonObject body;
    body["userid"] = api_key; // user ID = api_key for Motilal
    body["password"] = hashed_pw;
    body["2FA"] = dob;
    if (!totp.isEmpty())
        body["totp"] = totp;

    QMap<QString, QString> login_headers = {
        {"Content-Type", "application/json"},
        {"Accept", "application/json"},
        {"ApiKey", api_key},
        {"User-Agent", "MOSL/V.1.1.0"},
        {"ClientLocalIp", "127.0.0.1"},
        {"ClientPublicIp", "127.0.0.1"},
        {"MacAddress", "00:00:00:00:00:00"},
        {"SourceId", "WEB"},
        {"vendorinfo", api_key},
        {"osname", "Windows 10"},
        {"osversion", "10.0"},
        {"devicemodel", "PC"},
        {"manufacturer", "Generic"},
        {"productname", "FinceptTerminal"},
        {"productversion", "4.0.3"},
        {"browsername", "Chrome"},
        {"browserversion", "120.0"},
    };

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(QString("%1/rest/login/v3/authdirectapi").arg(BASE), body, login_headers);

    if (!resp.success)
        return {false, "", "", "", "Login failed: " + resp.error, ""};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "", "", "Login: invalid response", ""};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "SUCCESS")
        return {false, "", "", "", obj.value("message").toString("Login failed"), ""};

    QString token = obj.value("AuthToken").toString();
    if (token.isEmpty())
        return {false, "", "", "", "Login: no AuthToken in response", ""};

    return {true, token, "", api_key, "", ""};
}

// ---------- place_order ----------

OrderPlaceResponse MotilalBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    QJsonObject body;
    body["exchange"] = mo_exchange(order.exchange);
    body["symboltoken"] = order.instrument_token.toInt();
    body["buyorsell"] = (order.side == OrderSide::Buy) ? "BUY" : "SELL";
    body["ordertype"] = mo_enum_map().order_type_or(order.order_type, "MARKET");
    body["producttype"] = mo_enum_map().product_or(order.product_type, "DELIVERY");
    body["orderduration"] = "DAY";
    body["price"] = order.price;
    body["triggerprice"] = order.stop_price;
    // Motilal's place endpoint expects `quantityinlot` (lots, not pieces). Sending
    // `quantity` is silently ignored — orders go through with 0 quantity and reject.
    body["quantityinlot"] = static_cast<int>(order.quantity);
    body["disclosedquantity"] = 0;
    body["amoorder"] = order.amo ? "Y" : "N";
    body["algoid"] = "";
    body["goodtilldate"] = "";
    body["tag"] = "fincept";
    body["participantcode"] = "";
    body["clientcode"] = creds.user_id; // required for dealer accounts; harmless for investor

    auto& http = BrokerHttp::instance();
    // v2 is the current place-order endpoint per docs; v1 is deprecated.
    auto resp = http.post_json(QString("%1/rest/trans/v2/placeorder").arg(BASE), body, auth_headers(creds));

    if (!resp.success)
        return {false, "", checked_error(resp, "place_order failed")};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "place_order: invalid response"};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "SUCCESS")
        return {false, "", obj.value("message").toString("place_order failed")};

    QString order_id = obj.value("uniqueorderid").toString();
    return {true, order_id, ""};
}

// ---------- modify_order ----------

ApiResponse<QJsonObject> MotilalBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                     const QJsonObject& mods) {
    int64_t ts = now_ts();

    // Only forward what the caller supplied — defaulting newprice/qty to 0 will
    // overwrite the original order with a zero-priced/zero-qty LIMIT.
    QJsonObject body;
    body["uniqueorderid"] = order_id;
    if (mods.contains("orderType"))
        body["newordertype"] = mods.value("orderType").toString();
    body["neworderduration"] = mods.value("orderDuration").toString("DAY");
    if (mods.contains("price"))
        body["newprice"] = mods.value("price").toDouble();
    if (mods.contains("triggerPrice"))
        body["newtriggerprice"] = mods.value("triggerPrice").toDouble();
    if (mods.contains("quantity"))
        body["newquantityinlot"] = mods.value("quantity").toInt();
    body["newdisclosedquantity"] = 0;
    body["newgoodtilldate"] = mods.value("goodtilldate").toString("");
    body["lastmodifiedtime"] = mods.value("lastmodifiedtime").toString("");
    body["qtytradedtoday"] = mods.value("qtytradedtoday").toInt(0);
    body["clientcode"] = creds.user_id;

    auto& http = BrokerHttp::instance();
    // v5 is the current modify endpoint; v2 is being phased out.
    auto resp = http.post_json(QString("%1/rest/trans/v5/modifyorder").arg(BASE), body, auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "modify_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "modify_order: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "SUCCESS")
        return {false, std::nullopt, obj.value("message").toString("modify_order failed"), ts};

    return {true, obj, "", ts};
}

// ---------- cancel_order ----------

ApiResponse<QJsonObject> MotilalBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    int64_t ts = now_ts();

    QJsonObject body;
    body["uniqueorderid"] = order_id;
    body["clientcode"] = creds.user_id;

    auto& http = BrokerHttp::instance();
    // v2 cancel endpoint; v1 is deprecated.
    auto resp = http.post_json(QString("%1/rest/trans/v2/cancelorder").arg(BASE), body, auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "cancel_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "cancel_order: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "SUCCESS")
        return {false, std::nullopt, obj.value("message").toString("cancel_order failed"), ts};

    return {true, obj, "", ts};
}

// ---------- get_orders ----------

ApiResponse<QVector<BrokerOrderInfo>> MotilalBroker::get_orders(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    // v5 is the current orderbook endpoint.
    auto resp = http.post_json(QString("%1/rest/book/v5/getorderbook").arg(BASE), QJsonObject{}, auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_orders failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_orders: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "SUCCESS")
        return {false, std::nullopt, obj.value("message").toString("get_orders failed"), ts};

    QJsonArray arr = obj.value("data").toArray();
    QVector<BrokerOrderInfo> orders;
    orders.reserve(arr.size());

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("uniqueorderid").toString();
        info.symbol = o.value("symbol").toString();
        info.exchange = o.value("exchange").toString();
        info.quantity = o.value("orderqty").toInt();
        info.price = o.value("price").toDouble();
        info.stop_price = o.value("triggerprice").toDouble();
        info.avg_price = o.value("averageprice").toDouble();
        info.filled_qty = o.value("qtytradedtoday").toInt();
        info.status = mo_status(o.value("orderstatus").toString());
        info.side = o.value("buyorsell").toString().toLower();
        info.order_type = o.value("ordertype").toString();
        info.product_type = o.value("producttype").toString();
        info.timestamp = o.value("lastmodifiedtime").toString();
        orders.append(info);
    }

    return {true, orders, "", ts};
}

// ---------- get_trade_book ----------

ApiResponse<QJsonObject> MotilalBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    // v4 is current; v1 is deprecated.
    auto resp = http.post_json(QString("%1/rest/book/v4/gettradebook").arg(BASE), QJsonObject{}, auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_trade_book failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_trade_book: invalid response", ts};

    return {true, doc.object(), "", ts};
}

// ---------- get_positions ----------

ApiResponse<QVector<BrokerPosition>> MotilalBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    // v4 is the current position endpoint.
    auto resp = http.post_json(QString("%1/rest/book/v4/getposition").arg(BASE), QJsonObject{}, auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_positions failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_positions: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "SUCCESS")
        return {false, std::nullopt, obj.value("message").toString("get_positions failed"), ts};

    QJsonArray arr = obj.value("data").toArray();
    QVector<BrokerPosition> positions;

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        // net qty = buyquantity - sellquantity
        int buy_qty = o.value("buyquantity").toInt();
        int sell_qty = o.value("sellquantity").toInt();
        int net_qty = buy_qty - sell_qty;
        if (net_qty == 0)
            continue;

        double buy_amt = o.value("buyamount").toDouble();
        double avg_px = (buy_qty > 0) ? (buy_amt / buy_qty) : 0.0;

        BrokerPosition pos;
        pos.symbol = o.value("symbol").toString();
        pos.exchange = o.value("exchange").toString();
        pos.product_type = o.value("productname").toString();
        pos.quantity = net_qty;
        pos.avg_price = avg_px;
        pos.ltp = o.value("LTP").toDouble();
        pos.pnl = o.value("marktomarket").toDouble() + o.value("bookedprofitloss").toDouble();
        pos.side = net_qty > 0 ? "LONG" : "SHORT";
        positions.append(pos);
    }

    return {true, positions, "", ts};
}

// ---------- get_holdings ----------

ApiResponse<QVector<BrokerHolding>> MotilalBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    // v3 dpholding endpoint.
    auto resp = http.post_json(QString("%1/rest/report/v3/getdpholding").arg(BASE), QJsonObject{}, auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_holdings: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "SUCCESS")
        return {false, std::nullopt, obj.value("message").toString("get_holdings failed"), ts};

    QJsonArray arr = obj.value("data").toArray();
    QVector<BrokerHolding> holdings;

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        BrokerHolding h;
        h.symbol = o.value("scripname").toString();
        // Exchange comes from the holdings row when present; fall back to NSE
        // (the dpholdings endpoint is primarily NSE-listed equities).
        h.exchange = o.value("exchange").toString();
        if (h.exchange.isEmpty())
            h.exchange = "NSE";
        h.quantity = o.value("dpquantity").toInt();
        h.avg_price = o.value("buyavgprice").toDouble();
        // No LTP in holdings response — leave as 0; caller can fetch quotes separately
        h.ltp = 0.0;
        h.pnl = 0.0;
        holdings.append(h);
    }

    return {true, holdings, "", ts};
}

// ---------- get_funds ----------
// Uses /rest/report/v1/getreportmargindetail — returns array of {srno, amount}
// srno 102 = available, srno 220 = collateral, srno 300 = utilised

ApiResponse<BrokerFunds> MotilalBroker::get_funds(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    // v3 margin detail endpoint.
    auto resp = http.post_json(QString("%1/rest/report/v3/getreportmargindetail").arg(BASE), QJsonObject{},
                               auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_funds: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "SUCCESS")
        return {false, std::nullopt, obj.value("message").toString("get_funds failed"), ts};

    QJsonArray arr = obj.value("data").toArray();

    double available = 0.0;
    double collateral = 0.0;
    double used = 0.0;
    bool got_avail = false;

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        int srno = o.value("srno").toInt();
        double amount = o.value("amount").toDouble();
        switch (srno) {
            case 102:
                available = amount;
                got_avail = true;
                break;
            case 201:
                if (!got_avail)
                    available = amount;
                break;
            case 220:
                collateral = amount;
                break;
            case 300:
                used = amount;
                break;
            case 301:
                if (used == 0.0)
                    used = amount;
                break;
            default:
                break;
        }
    }

    BrokerFunds funds;
    funds.available_balance = available;
    funds.used_margin = used;
    funds.total_balance = available + used;
    funds.collateral = collateral;

    return {true, funds, "", ts};
}

// ---------- get_quotes ----------
// POST /rest/report/v1/getltpdata per symbol
// Prices are in paisa — divide by 100

ApiResponse<QVector<BrokerQuote>> MotilalBroker::get_quotes(const BrokerCredentials& creds,
                                                            const QVector<QString>& symbols) {
    int64_t ts = now_ts();
    if (symbols.isEmpty())
        return {true, QVector<BrokerQuote>{}, "", ts};

    auto& http = BrokerHttp::instance();
    QVector<BrokerQuote> quotes;

    for (const QString& sym : symbols) {
        QString exch = "NSE", name = sym;
        if (sym.contains(':')) {
            exch = sym.section(':', 0, 0);
            name = sym.section(':', 1);
        }
        // symbol format: "NSE:RELIANCE:1660" — token in 3rd part if present
        QString token_str;
        if (name.contains(':')) {
            token_str = name.section(':', 1);
            name = name.section(':', 0, 0);
        }

        QJsonObject body;
        body["exchange"] = mo_exchange(exch);
        body["scripcode"] = token_str.isEmpty() ? 0 : token_str.toInt();

        // v3 ltpdata endpoint.
        auto resp = http.post_json(QString("%1/rest/report/v3/getltpdata").arg(BASE), body, auth_headers(creds));
        if (!resp.success)
            continue;

        QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
        if (!doc.isObject())
            continue;

        QJsonObject obj = doc.object();
        if (obj.value("status").toString() != "SUCCESS")
            continue;

        QJsonObject data = obj.value("data").toObject();
        // All prices in paisa — divide by 100
        BrokerQuote quote;
        quote.symbol = name;
        quote.ltp = data.value("ltp").toDouble() / 100.0;
        quote.open = data.value("open").toDouble() / 100.0;
        quote.high = data.value("high").toDouble() / 100.0;
        quote.low = data.value("low").toDouble() / 100.0;
        quote.close = data.value("close").toDouble() / 100.0;
        quote.volume = static_cast<int64_t>(data.value("volume").toDouble());
        if (quote.close > 0)
            quote.change_pct = (quote.ltp - quote.close) / quote.close * 100.0;
        quote.change = quote.ltp - quote.close;
        quote.timestamp = ts;
        quotes.append(quote);
    }

    return {true, quotes, "", ts};
}

// ---------- get_history ----------
// Motilal Oswal exposes EOD candles via /rest/report/v3/geteoddatabyexchangename.
// Request body: { clientcode, exchange, scripcode, fromdate, todate }.
// Response: { status: "SUCCESS", data: [{ date, open, high, low, close, volume }, ...] }
// Intraday is NOT supported on the public REST API — only daily.
//
// Symbol format expected: "NSE:RELIANCE:1660" (exchange:name:token). Token is
// required since the endpoint keys on scripcode.

ApiResponse<QVector<BrokerCandle>> MotilalBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                              const QString& resolution, const QString& from_date,
                                                              const QString& to_date) {
    int64_t ts = now_ts();

    // Only daily / weekly / monthly are supported.
    const QString r = resolution.toUpper();
    const bool is_daily = r == "D" || r == "1D" || r == "DAY" || r == "W" || r == "1W" || r == "WEEK" ||
                          r == "M" || r == "1M" || r == "MONTH";
    if (!is_daily)
        return {false, std::nullopt,
                "Motilal Oswal historical API supports only EOD bars (D/W/M). Intraday is unavailable.", ts};

    // Parse "EX:SYMBOL:TOKEN" — token is required.
    QStringList parts = symbol.split(':');
    QString exch = parts.size() >= 1 ? parts[0] : "NSE";
    QString name = parts.size() >= 2 ? parts[1] : symbol;
    QString token = parts.size() >= 3 ? parts[2] : QString();
    if (token.isEmpty())
        return {false, std::nullopt, "MO history requires instrument token (format EXCHANGE:SYMBOL:TOKEN)", ts};

    QJsonObject body;
    body["clientcode"] = creds.user_id;
    body["exchange"] = mo_exchange(exch);
    body["scripcode"] = token.toInt();
    body["fromdate"] = from_date;
    body["todate"] = to_date;

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(QString("%1/rest/report/v3/geteoddatabyexchangename").arg(BASE), body,
                               auth_headers(creds));
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_history failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_history: invalid response", ts};
    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "SUCCESS")
        return {false, std::nullopt, obj.value("message").toString("get_history failed"), ts};

    QVector<BrokerCandle> candles;
    const QJsonArray rows = obj.value("data").toArray();
    candles.reserve(rows.size());
    for (const auto& v : rows) {
        const QJsonObject o = v.toObject();
        BrokerCandle c;
        const QString date_str = o.value("date").toString();
        // MO returns "yyyy-MM-dd" or "dd-MMM-yyyy" depending on the deployment.
        QDate d = QDate::fromString(date_str, "yyyy-MM-dd");
        if (!d.isValid())
            d = QDate::fromString(date_str, "dd-MMM-yyyy");
        c.timestamp = d.isValid() ? QDateTime(d, QTime(15, 30)).toSecsSinceEpoch() : 0;
        c.open = o.value("open").toDouble();
        c.high = o.value("high").toDouble();
        c.low = o.value("low").toDouble();
        c.close = o.value("close").toDouble();
        c.volume = o.value("volume").toDouble();
        candles.append(c);
    }
    return {true, candles, "", ts};
}

// ---------- get_master_contract ----------
// Per-exchange scrip dump: GET /getscripmastercsv?name={exchange}
// Returns a CSV stream with columns: scripcode, name, exchange, symbol, expiry, ...
// We surface the raw body to the caller (typically InstrumentService) for parsing.
ApiResponse<QJsonObject> MotilalBroker::get_master_contract(const BrokerCredentials& creds, const QString& exchange) {
    int64_t ts = now_ts();
    const QString url = QString("%1/getscripmastercsv?name=%2").arg(BASE, mo_exchange(exchange));
    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_master_contract failed"), ts};
    QJsonObject out;
    out["exchange"] = mo_exchange(exchange);
    out["csv"] = resp.raw_body;
    return {true, out, "", ts};
}

} // namespace fincept::trading
