#include "trading/brokers/motilal/MotilalBroker.h"
#include "trading/brokers/BrokerHttp.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QCryptographicHash>

namespace fincept::trading {

static constexpr const char* BASE = "https://openapi.motilaloswal.com";

static int64_t now_ts() { return QDateTime::currentSecsSinceEpoch(); }

// ---------- Static helpers ----------

QString MotilalBroker::mo_exchange(const QString& exchange) {
    if (exchange == "NFO") return "NSEFO";
    if (exchange == "CDS") return "NSECD";
    if (exchange == "BFO") return "BSEFO";
    // NSE, BSE, MCX, NCDEX pass through unchanged
    return exchange.toUpper();
}

QString MotilalBroker::mo_product(ProductType p) {
    switch (p) {
        case ProductType::Intraday: return "VALUEPLUS";
        case ProductType::Delivery: return "DELIVERY";
        case ProductType::Margin:   return "NORMAL";
        default:                    return "DELIVERY";
    }
}

QString MotilalBroker::mo_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "MARKET";
        case OrderType::Limit:         return "LIMIT";
        case OrderType::StopLoss:      return "STOPLOSS";
        case OrderType::StopLossLimit: return "STOPLOSS";
        default:                       return "MARKET";
    }
}

QString MotilalBroker::mo_status(const QString& s) {
    QString sl = s.toLower();
    if (sl == "traded")                             return "filled";
    if (sl == "confirm" || sl == "sent" || sl == "open" || sl == "trigger pending") return "open";
    if (sl == "cancel")                             return "cancelled";
    if (sl == "rejected" || sl == "error")          return "rejected";
    return "open";
}

bool MotilalBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 403 || resp.status_code == 401) return true;
    if (!resp.success) return false;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject()) return false;
    QString status = doc.object().value("status").toString();
    if (status == "SUCCESS") return false;
    QString msg = doc.object().value("message").toString().toLower();
    return msg.contains("invalid") || msg.contains("session") ||
           msg.contains("token")   || msg.contains("unauthorized") ||
           msg.contains("login")   || msg.contains("auth");
}

QString MotilalBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    if (is_token_expired(resp)) return "[TOKEN_EXPIRED] Session expired, please re-login";
    if (!resp.success) return resp.error.isEmpty() ? fallback : resp.error;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (doc.isObject()) {
        QString msg = doc.object().value("message").toString();
        if (!msg.isEmpty()) return msg;
    }
    return fallback;
}

// ---------- Auth headers ----------

QMap<QString, QString> MotilalBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Authorization",  creds.access_token},
        {"Content-Type",   "application/json"},
        {"Accept",         "application/json"},
        {"ApiKey",         creds.api_key},
        {"User-Agent",     "MOSL/V.1.1.0"},
        {"ClientLocalIp",  "127.0.0.1"},
        {"ClientPublicIp", "127.0.0.1"},
        {"MacAddress",     "00:00:00:00:00:00"},
        {"SourceId",       "WEB"},
        {"vendorinfo",     ""},
        {"osname",         "Windows 10"},
        {"osversion",      "10.0"},
        {"devicemodel",    "PC"},
        {"manufacturer",   "Generic"},
        {"productname",    "FinceptTerminal"},
        {"productversion", "4.0.0"},
        {"browsername",    "Chrome"},
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
    QString dob  = (auth_parts.size() >= 1) ? auth_parts[0] : "";
    QString totp = (auth_parts.size() >= 2) ? auth_parts[1] : "";

    // SHA256(password + api_key)
    QString raw = api_secret + api_key;
    QString hashed_pw = QString(QCryptographicHash::hash(
        raw.toUtf8(), QCryptographicHash::Sha256).toHex());

    QJsonObject body;
    body["userid"]   = api_key;   // user ID = api_key for Motilal
    body["password"] = hashed_pw;
    body["2FA"]      = dob;
    if (!totp.isEmpty())
        body["totp"] = totp;

    QMap<QString, QString> login_headers = {
        {"Content-Type",   "application/json"},
        {"Accept",         "application/json"},
        {"ApiKey",         api_key},
        {"User-Agent",     "MOSL/V.1.1.0"},
        {"ClientLocalIp",  "127.0.0.1"},
        {"ClientPublicIp", "127.0.0.1"},
        {"MacAddress",     "00:00:00:00:00:00"},
        {"SourceId",       "WEB"},
        {"vendorinfo",     api_key},
        {"osname",         "Windows 10"},
        {"osversion",      "10.0"},
        {"devicemodel",    "PC"},
        {"manufacturer",   "Generic"},
        {"productname",    "FinceptTerminal"},
        {"productversion", "4.0.0"},
        {"browsername",    "Chrome"},
        {"browserversion", "120.0"},
    };

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(
        QString("%1/rest/login/v3/authdirectapi").arg(BASE),
        body, login_headers
    );

    if (!resp.success)
        return {false, "", "", "", "Login failed: " + resp.error};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "", "", "Login: invalid response"};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "SUCCESS")
        return {false, "", "", "", obj.value("message").toString("Login failed")};

    QString token = obj.value("AuthToken").toString();
    if (token.isEmpty())
        return {false, "", "", "", "Login: no AuthToken in response"};

    return {true, token, "", api_key, ""};
}

// ---------- place_order ----------

OrderPlaceResponse MotilalBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    QJsonObject body;
    body["exchange"]          = mo_exchange(order.exchange);
    body["symboltoken"]       = order.instrument_token.toInt();
    body["buyorsell"]         = (order.side == OrderSide::Buy) ? "BUY" : "SELL";
    body["ordertype"]         = mo_order_type(order.order_type);
    body["producttype"]       = mo_product(order.product_type);
    body["orderduration"]     = "DAY";
    body["price"]             = order.price;
    body["triggerprice"]      = order.stop_price;
    body["quantity"]          = static_cast<int>(order.quantity);
    body["disclosedquantity"] = 0;
    body["amoorder"]          = order.amo ? "Y" : "N";
    body["algoid"]            = "";
    body["goodtilldate"]      = "";
    body["tag"]               = "";
    body["participantcode"]   = "";
    body["clientcode"]        = "";

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(
        QString("%1/rest/trans/v1/placeorder").arg(BASE),
        body, auth_headers(creds)
    );

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

ApiResponse<QJsonObject> MotilalBroker::modify_order(const BrokerCredentials& creds,
                                                       const QString& order_id,
                                                       const QJsonObject& mods) {
    int64_t ts = now_ts();

    QJsonObject body;
    body["uniqueorderid"]        = order_id;
    body["newordertype"]         = mods.value("orderType").toString("LIMIT");
    body["neworderduration"]     = "DAY";
    body["newprice"]             = mods.value("price").toDouble(0);
    body["newtriggerprice"]      = mods.value("triggerPrice").toDouble(0);
    body["newquantityinlot"]     = mods.value("quantity").toInt(0);
    body["newdisclosedquantity"] = 0;
    body["newgoodtilldate"]      = "";
    body["lastmodifiedtime"]     = mods.value("lastmodifiedtime").toString("");
    body["qtytradedtoday"]       = mods.value("qtytradedtoday").toInt(0);
    body["clientcode"]           = "";

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(
        QString("%1/rest/trans/v2/modifyorder").arg(BASE),
        body, auth_headers(creds)
    );

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

ApiResponse<QJsonObject> MotilalBroker::cancel_order(const BrokerCredentials& creds,
                                                       const QString& order_id) {
    int64_t ts = now_ts();

    QJsonObject body;
    body["uniqueorderid"] = order_id;
    body["clientcode"]    = "";

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(
        QString("%1/rest/trans/v1/cancelorder").arg(BASE),
        body, auth_headers(creds)
    );

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
    auto resp = http.post_json(
        QString("%1/rest/book/v2/getorderbook").arg(BASE),
        QJsonObject{}, auth_headers(creds)
    );

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
        info.order_id    = o.value("uniqueorderid").toString();
        info.symbol      = o.value("symbol").toString();
        info.exchange    = o.value("exchange").toString();
        info.quantity    = o.value("orderqty").toInt();
        info.price       = o.value("price").toDouble();
        info.stop_price  = o.value("triggerprice").toDouble();
        info.avg_price   = o.value("averageprice").toDouble();
        info.filled_qty  = o.value("qtytradedtoday").toInt();
        info.status      = mo_status(o.value("orderstatus").toString());
        info.side        = o.value("buyorsell").toString().toLower();
        info.order_type  = o.value("ordertype").toString();
        info.product_type= o.value("producttype").toString();
        info.timestamp   = o.value("lastmodifiedtime").toString();
        orders.append(info);
    }

    return {true, orders, "", ts};
}

// ---------- get_trade_book ----------

ApiResponse<QJsonObject> MotilalBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(
        QString("%1/rest/book/v1/gettradebook").arg(BASE),
        QJsonObject{}, auth_headers(creds)
    );

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
    auto resp = http.post_json(
        QString("%1/rest/book/v1/getposition").arg(BASE),
        QJsonObject{}, auth_headers(creds)
    );

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
        int buy_qty  = o.value("buyquantity").toInt();
        int sell_qty = o.value("sellquantity").toInt();
        int net_qty  = buy_qty - sell_qty;
        if (net_qty == 0) continue;

        double buy_amt  = o.value("buyamount").toDouble();
        double sell_amt = o.value("sellamount").toDouble();
        double avg_px   = (buy_qty > 0) ? (buy_amt / buy_qty) : 0.0;

        BrokerPosition pos;
        pos.symbol       = o.value("symbol").toString();
        pos.exchange     = o.value("exchange").toString();
        pos.product_type = o.value("productname").toString();
        pos.quantity     = net_qty;
        pos.avg_price    = avg_px;
        pos.ltp          = o.value("LTP").toDouble();
        pos.pnl          = o.value("marktomarket").toDouble()
                         + o.value("bookedprofitloss").toDouble();
        pos.side         = net_qty > 0 ? "LONG" : "SHORT";
        positions.append(pos);
    }

    return {true, positions, "", ts};
}

// ---------- get_holdings ----------

ApiResponse<QVector<BrokerHolding>> MotilalBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(
        QString("%1/rest/report/v1/getdpholding").arg(BASE),
        QJsonObject{}, auth_headers(creds)
    );

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
        h.symbol    = o.value("scripname").toString();
        h.exchange  = "NSE";
        h.quantity  = o.value("dpquantity").toInt();
        h.avg_price = o.value("buyavgprice").toDouble();
        // No LTP in holdings response — leave as 0; caller can fetch quotes separately
        h.ltp       = 0.0;
        h.pnl       = 0.0;
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
    auto resp = http.post_json(
        QString("%1/rest/report/v1/getreportmargindetail").arg(BASE),
        QJsonObject{}, auth_headers(creds)
    );

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_funds: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != "SUCCESS")
        return {false, std::nullopt, obj.value("message").toString("get_funds failed"), ts};

    QJsonArray arr = obj.value("data").toArray();

    double available  = 0.0;
    double collateral = 0.0;
    double used       = 0.0;
    bool   got_avail  = false;

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        int    srno   = o.value("srno").toInt();
        double amount = o.value("amount").toDouble();
        switch (srno) {
            case 102: available  = amount; got_avail = true; break;
            case 201: if (!got_avail) available = amount;    break;
            case 220: collateral = amount;                   break;
            case 300: used       = amount;                   break;
            case 301: if (used == 0.0) used = amount;        break;
            default:  break;
        }
    }

    BrokerFunds funds;
    funds.available_balance = available;
    funds.used_margin       = used;
    funds.total_balance     = available + used;
    funds.collateral        = collateral;

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
            name      = name.section(':', 0, 0);
        }

        QJsonObject body;
        body["exchange"]  = mo_exchange(exch);
        body["scripcode"] = token_str.isEmpty() ? 0 : token_str.toInt();

        auto resp = http.post_json(
            QString("%1/rest/report/v1/getltpdata").arg(BASE),
            body, auth_headers(creds)
        );
        if (!resp.success) continue;

        QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
        if (!doc.isObject()) continue;

        QJsonObject obj = doc.object();
        if (obj.value("status").toString() != "SUCCESS") continue;

        QJsonObject data = obj.value("data").toObject();
        // All prices in paisa — divide by 100
        BrokerQuote quote;
        quote.symbol = name;
        quote.ltp    = data.value("ltp").toDouble()    / 100.0;
        quote.open   = data.value("open").toDouble()   / 100.0;
        quote.high   = data.value("high").toDouble()   / 100.0;
        quote.low    = data.value("low").toDouble()    / 100.0;
        quote.close  = data.value("close").toDouble()  / 100.0;
        quote.volume = static_cast<int64_t>(data.value("volume").toDouble());
        if (quote.close > 0)
            quote.change_pct = (quote.ltp - quote.close) / quote.close * 100.0;
        quote.change    = quote.ltp - quote.close;
        quote.timestamp = ts;
        quotes.append(quote);
    }

    return {true, quotes, "", ts};
}

// ---------- get_history ----------
// Motilal Oswal does not provide historical candle data via REST API.

ApiResponse<QVector<BrokerCandle>> MotilalBroker::get_history(const BrokerCredentials& /*creds*/,
                                                                const QString& /*symbol*/,
                                                                const QString& /*resolution*/,
                                                                const QString& /*from_date*/,
                                                                const QString& /*to_date*/) {
    int64_t ts = now_ts();
    return {false, std::nullopt, "Historical data not supported by Motilal Oswal API", ts};
}

} // namespace fincept::trading
