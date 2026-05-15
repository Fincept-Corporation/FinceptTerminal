#include "trading/brokers/fivepaisa/FivePaisaBroker.h"

#include "trading/brokers/BrokerHttp.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

namespace fincept::trading {

static const char* BASE_URL = "https://Openapi.5paisa.com";

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// ============================================================================
// Helpers
// ============================================================================

bool FivePaisaBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 401 || resp.status_code == 403)
        return true;
    if (!resp.json.isEmpty()) {
        QString desc = resp.json["head"].toObject()["statusDescription"].toString().toLower();
        if (desc.contains("session") || desc.contains("unauthori") || desc.contains("token"))
            return true;
    }
    return false;
}

QString FivePaisaBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    if (!resp.json.isEmpty()) {
        QString desc = resp.json["head"].toObject()["statusDescription"].toString();
        if (!desc.isEmpty() && desc.toLower() != "success")
            return desc;
        QString msg = resp.json["body"].toObject()["Message"].toString();
        if (!msg.isEmpty())
            return msg;
        msg = resp.json["message"].toString();
        if (!msg.isEmpty())
            return msg;
    }
    if (!resp.error.isEmpty())
        return resp.error;
    if (!resp.raw_body.isEmpty())
        return resp.raw_body.left(200);
    return fallback;
}

FivePaisaBroker::KeyParts FivePaisaBroker::unpack_key(const QString& packed) {
    QStringList parts = packed.split(":::");
    KeyParts kp;
    if (parts.size() >= 1)
        kp.app_key = parts[0].trimmed();
    if (parts.size() >= 2)
        kp.user_id = parts[1].trimmed();
    if (parts.size() >= 3)
        kp.client_id = parts[2].trimmed();
    return kp;
}

// Exchange code: N, B, M
QString FivePaisaBroker::fp_exchange(const QString& exchange) {
    if (exchange == "BSE" || exchange == "BFO" || exchange == "BCD")
        return "B";
    if (exchange == "MCX")
        return "M";
    return "N"; // NSE, NFO, CDS
}

// Exchange type: C (Cash), D (Derivatives), U (Currency)
QString FivePaisaBroker::fp_exchange_type(const QString& exchange) {
    if (exchange == "NFO" || exchange == "BFO" || exchange == "MCX")
        return "D";
    if (exchange == "CDS" || exchange == "BCD")
        return "U";
    return "C"; // NSE, BSE
}

// Resolution for historical GET endpoint
QString FivePaisaBroker::fp_interval(const QString& resolution) {
    if (resolution == "1" || resolution == "1m")
        return "1m";
    if (resolution == "5" || resolution == "5m")
        return "5m";
    if (resolution == "10" || resolution == "10m")
        return "10m";
    if (resolution == "15" || resolution == "15m")
        return "15m";
    if (resolution == "30" || resolution == "30m")
        return "30m";
    if (resolution == "60" || resolution == "1h")
        return "1h";
    return "1d"; // D, 1D, W
}

// Build the standard request envelope: { head: {key: app_key}, body: body_fields }
QJsonObject FivePaisaBroker::make_body(const QString& app_key, const QString& /*client_id*/,
                                       const QJsonObject& body_fields) {
    QJsonObject head;
    head["key"] = app_key;
    QJsonObject root;
    root["head"] = head;
    root["body"] = body_fields;
    return root;
}

// ============================================================================
// Auth headers
// ============================================================================
// 5Paisa-API-Uid is a vendor identifier required on every authenticated request;
// missing it returns generic 4xx errors. The literal value is shipped in the
// official py5paisa SDK and is currently the only accepted token.

QMap<QString, QString> FivePaisaBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Authorization", "Bearer " + creds.access_token},
        {"Content-Type", "application/json"},
        {"Accept", "application/json"},
        {"5Paisa-API-Uid", "ka7SFqAU6SC"},
    };
}

// ============================================================================
// Phase 2: exchange_token
// ApiKey    = "app_key:::user_id:::client_id"
// ApiSecret = App Secret (EncryKey)
// AuthCode  = "email:::pin:::totp"
//
// Step 1: POST /VendorsAPI/Service1.svc/TOTPLogin  → body.RequestToken
// Step 2: POST /VendorsAPI/Service1.svc/GetAccessToken → body.AccessToken
// ============================================================================

TokenExchangeResponse FivePaisaBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                      const QString& auth_code) {
    auto kp = unpack_key(api_key);
    if (kp.app_key.isEmpty())
        return {false, "", "", "", "ApiKey must be 'app_key:::user_id:::client_id'", ""};

    QStringList auth_parts = auth_code.split(":::");
    if (auth_parts.size() < 3)
        return {false, "", "", "", "AuthCode must be 'email:::pin:::totp'", ""};
    QString email = auth_parts[0].trimmed();
    QString pin = auth_parts[1].trimmed();
    QString totp = auth_parts[2].trimmed();

    QMap<QString, QString> headers = {
        {"Content-Type", "application/json"},
        {"Accept", "application/json"},
        {"5Paisa-API-Uid", "ka7SFqAU6SC"},
    };

    // Step 1: TOTP login
    {
        QJsonObject head;
        head["Key"] = kp.app_key;
        QJsonObject body;
        body["Email_ID"] = email;
        body["TOTP"] = totp;
        body["PIN"] = pin;
        QJsonObject req;
        req["head"] = head;
        req["body"] = body;

        auto resp =
            BrokerHttp::instance().post_json(QString(BASE_URL) + "/VendorsAPI/Service1.svc/TOTPLogin", req, headers);

        if (!resp.success)
            return {false, "", "", "", checked_error(resp, "TOTP login network error"), ""};

        // Per SDK, success is indicated by head.statusDescription=="Success" AND
        // body.Status==0; bypassing the status check buries auth errors in an
        // empty-RequestToken message.
        const QJsonObject body_obj = resp.json["body"].toObject();
        const int status_code = body_obj["Status"].toInt(-1);
        const QString req_token = body_obj["RequestToken"].toString();
        if (req_token.isEmpty() || (status_code != 0 && status_code != -1)) {
            const QString msg = body_obj["Message"].toString();
            return {false, "", "", "", msg.isEmpty() ? "TOTP login failed (no RequestToken)" : msg, ""};
        }

        // Step 2: Get access token
        QJsonObject head2;
        head2["Key"] = kp.app_key;
        QJsonObject body2;
        body2["RequestToken"] = req_token;
        body2["EncryKey"] = api_secret;
        body2["UserId"] = kp.user_id;
        QJsonObject req2;
        req2["head"] = head2;
        req2["body"] = body2;

        auto resp2 = BrokerHttp::instance().post_json(QString(BASE_URL) + "/VendorsAPI/Service1.svc/GetAccessToken",
                                                      req2, headers);

        if (!resp2.success)
            return {false, "", "", "", checked_error(resp2, "GetAccessToken network error"), ""};

        QString access_token = resp2.json["body"].toObject()["AccessToken"].toString();
        if (access_token.isEmpty()) {
            QString msg = resp2.json["body"].toObject()["Message"].toString();
            return {false, "", "", "", msg.isEmpty() ? "No AccessToken in response" : msg, ""};
        }

        return {true, access_token, kp.client_id, "", "", ""};
    }
}

// ============================================================================
// Phase 3: Order operations
// ============================================================================

OrderPlaceResponse FivePaisaBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    auto kp = unpack_key(creds.api_key);
    auto hdrs = auth_headers(creds);

    bool is_intraday = (order.product_type == ProductType::Intraday);

    QJsonObject body;
    // PlaceOrderRequest uses the long-form transaction string ("BUY"/"SELL");
    // the legacy "B"/"S" short form is reserved for the BO/CO BuySell field.
    body["OrderType"] = (order.side == OrderSide::Buy) ? "BUY" : "SELL";
    body["Exchange"] = fp_exchange(order.exchange);
    body["ExchangeType"] = fp_exchange_type(order.exchange);
    body["ScripCode"] = order.instrument_token.isEmpty() ? 0 : order.instrument_token.toInt();
    body["Price"] = (order.order_type == OrderType::Market) ? 0.0 : order.price;
    body["Qty"] = order.quantity;
    body["StopLossPrice"] = (order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit)
                                ? order.stop_price
                                : 0.0;
    body["DisQty"] = 0;
    body["IsIntraday"] = is_intraday;
    body["AHPlaced"] = order.amo ? "Y" : "N";
    // Per-order UUID-ish — 5Paisa rejects duplicates with the same RemoteOrderID.
    body["RemoteOrderID"] =
        "FCPT-" + QString::number(QDateTime::currentMSecsSinceEpoch()) + "-" + QString::number(QRandomGenerator::global()->bounded(10000));
    body["AppSource"] = 0; // required: 0 = open API
    body["IOCOrder"] = false;
    body["IsStopLossOrder"] = (order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit);
    body["iOrderValidity"] = 0; // 0 = DAY
    // ValidTillDate uses Microsoft JSON Date format (epoch ms wrapped).
    const qint64 tomorrow_ms = QDateTime::currentDateTime().addDays(1).toMSecsSinceEpoch();
    body["ValidTillDate"] = "/Date(" + QString::number(tomorrow_ms) + ")/";
    body["PublicIP"] = "0.0.0.0"; // server fills in the real IP; placeholder is accepted
    body["OrderRequesterCode"] = kp.client_id;

    QJsonObject req = make_body(kp.app_key, kp.client_id, body);

    auto resp = BrokerHttp::instance().post_json(QString(BASE_URL) + "/VendorsAPI/Service1.svc/V1/PlaceOrderRequest",
                                                 req, hdrs);

    if (!resp.success)
        return {false, "", checked_error(resp, "Network error")};
    if (is_token_expired(resp))
        return {false, "", "[TOKEN_EXPIRED]"};

    QString status = resp.json["head"].toObject()["statusDescription"].toString();
    if (status != "Success")
        return {false, "", checked_error(resp, "Place order failed")};

    // Response field casing varies across endpoints: PlaceOrder may return
    // BrokerOrderID (upper-D) while OrderBook returns BrokerOrderId (lower-d).
    // Try both. The value can be int or string.
    const QJsonObject body_obj = resp.json["body"].toObject();
    QJsonValue broker_id = body_obj["BrokerOrderID"];
    if (broker_id.isUndefined() || broker_id.isNull())
        broker_id = body_obj["BrokerOrderId"];
    QString order_id = broker_id.isString() ? broker_id.toString() : QString::number(broker_id.toInt());
    if (order_id.isEmpty() || order_id == "0")
        return {false, "", "No BrokerOrderID in response"};

    return {true, order_id, ""};
}

ApiResponse<QJsonObject> FivePaisaBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                       const QJsonObject& mods) {
    int64_t ts = now_ts();
    auto kp = unpack_key(creds.api_key);
    auto hdrs = auth_headers(creds);

    // 5paisa modify uses ExchOrderID — treat passed order_id as ExchOrderID
    QJsonObject body;
    body["ExchOrderID"] = order_id;
    body["Price"] = mods.value("price").toDouble(0.0);
    body["Qty"] = mods.value("quantity").toInt(0);
    body["StopLossPrice"] = mods.value("trigger_price").toDouble(0.0);
    body["DisQty"] = 0;

    QJsonObject req = make_body(kp.app_key, kp.client_id, body);

    auto resp = BrokerHttp::instance().post_json(QString(BASE_URL) + "/VendorsAPI/Service1.svc/V1/ModifyOrderRequest",
                                                 req, hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    // status "0" = success per 5paisa docs
    QString head_status = resp.json["head"].toObject()["status"].toString();
    QString head_desc = resp.json["head"].toObject()["statusDescription"].toString();
    if (head_status != "0" && head_desc != "Success")
        return {false, std::nullopt, checked_error(resp, "Modify failed"), ts};

    return {true, resp.json, "", ts};
}

ApiResponse<QJsonObject> FivePaisaBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    int64_t ts = now_ts();
    auto kp = unpack_key(creds.api_key);
    auto hdrs = auth_headers(creds);

    QJsonObject body;
    body["ExchOrderID"] = order_id;

    QJsonObject head;
    head["key"] = kp.app_key;
    QJsonObject req;
    req["head"] = head;
    req["body"] = body;

    auto resp = BrokerHttp::instance().post_json(QString(BASE_URL) + "/VendorsAPI/Service1.svc/V1/CancelOrderRequest",
                                                 req, hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    QString status = resp.json["head"].toObject()["statusDescription"].toString();
    if (status != "Success")
        return {false, std::nullopt, checked_error(resp, "Cancel failed"), ts};

    return {true, resp.json, "", ts};
}

// ============================================================================
// Phase 4: Read operations
// ============================================================================

ApiResponse<QVector<BrokerOrderInfo>> FivePaisaBroker::get_orders(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto kp = unpack_key(creds.api_key);
    auto hdrs = auth_headers(creds);

    QJsonObject body;
    body["ClientCode"] = kp.client_id;
    QJsonObject req = make_body(kp.app_key, kp.client_id, body);

    auto resp =
        BrokerHttp::instance().post_json(QString(BASE_URL) + "/VendorsAPI/Service1.svc/V3/OrderBook", req, hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    QJsonArray detail = resp.json["body"].toObject()["OrderBookDetail"].toArray();
    QVector<BrokerOrderInfo> orders;

    auto parse_status = [](const QString& s) -> QString {
        if (s == "Fully Executed")
            return "filled";
        if (s == "Pending")
            return "open";
        if (s == "Modified")
            return "open";
        if (s == "Cancelled")
            return "cancelled";
        if (s == "Rejected")
            return "rejected";
        return s.toLower();
    };

    for (const auto& item : detail) {
        QJsonObject o = item.toObject();
        BrokerOrderInfo info;
        info.order_id = QString::number(o["BrokerOrderId"].toInt());
        info.symbol = o["ScripName"].toString();
        info.exchange = o["Exch"].toString();
        info.quantity = o["Qty"].toInt();
        info.filled_qty = o["TradedQty"].toInt();
        info.price = o["Rate"].toDouble();
        info.trigger_price = o["TriggerRate"].toDouble();
        info.status = parse_status(o["OrderStatus"].toString());
        info.side = (o["BuySell"].toString() == "B") ? "buy" : "sell";
        info.order_type = (o["AtMarket"].toString() == "Y") ? "MARKET" : "LIMIT";
        info.product_type = o["DelvIntra"].toString();
        info.timestamp = o["ExchOrderTime"].toString();
        orders.append(info);
    }

    return {true, orders, "", ts};
}

ApiResponse<QJsonObject> FivePaisaBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto kp = unpack_key(creds.api_key);
    auto hdrs = auth_headers(creds);

    QJsonObject body;
    body["ClientCode"] = kp.client_id;
    QJsonObject req = make_body(kp.app_key, kp.client_id, body);

    auto resp =
        BrokerHttp::instance().post_json(QString(BASE_URL) + "/VendorsAPI/Service1.svc/V1/TradeBook", req, hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    return {true, resp.json, "", ts};
}

ApiResponse<QVector<BrokerPosition>> FivePaisaBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto kp = unpack_key(creds.api_key);
    auto hdrs = auth_headers(creds);

    QJsonObject body;
    body["ClientCode"] = kp.client_id;
    QJsonObject req = make_body(kp.app_key, kp.client_id, body);

    auto resp = BrokerHttp::instance().post_json(QString(BASE_URL) + "/VendorsAPI/Service1.svc/V2/NetPositionNetWise",
                                                 req, hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    QJsonArray detail = resp.json["body"].toObject()["NetPositionDetail"].toArray();
    QVector<BrokerPosition> positions;

    for (const auto& item : detail) {
        QJsonObject p = item.toObject();
        int net_qty = p["NetQty"].toInt();
        if (net_qty == 0)
            continue;

        QString exch = p["Exch"].toString();
        QString exch_type = p["ExchType"].toString();
        QString exchange;
        if (exch == "N" && exch_type == "C")
            exchange = "NSE";
        else if (exch == "B" && exch_type == "C")
            exchange = "BSE";
        else if (exch == "N" && exch_type == "D")
            exchange = "NFO";
        else if (exch == "B" && exch_type == "D")
            exchange = "BFO";
        else if (exch == "M" && exch_type == "D")
            exchange = "MCX";
        else if (exch == "N" && exch_type == "U")
            exchange = "CDS";
        else
            exchange = exch;

        QString order_for = p["OrderFor"].toString();
        QString product;
        if (order_for == "I")
            product = "MIS";
        else if (exchange == "NSE" || exchange == "BSE")
            product = "CNC";
        else
            product = "NRML";

        BrokerPosition pos;
        pos.symbol = p["ScripName"].toString();
        pos.exchange = exchange;
        pos.quantity = net_qty;
        pos.avg_price = p["AvgRate"].toDouble();
        pos.ltp = p["LTP"].toDouble();
        pos.pnl = p["MTOM"].toDouble();
        pos.product_type = product;
        positions.append(pos);
    }

    return {true, positions, "", ts};
}

ApiResponse<QVector<BrokerHolding>> FivePaisaBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto kp = unpack_key(creds.api_key);
    auto hdrs = auth_headers(creds);

    QJsonObject body;
    body["ClientCode"] = kp.client_id;
    QJsonObject req = make_body(kp.app_key, kp.client_id, body);

    auto resp = BrokerHttp::instance().post_json(QString(BASE_URL) + "/VendorsAPI/Service1.svc/V3/Holding", req, hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    QJsonArray data = resp.json["body"].toObject()["Data"].toArray();
    QVector<BrokerHolding> holdings;
    holdings.reserve(data.size());

    for (const auto& item : data) {
        QJsonObject h = item.toObject();
        double avg = h["AvgRate"].toDouble();
        double ltp = h["CurrentPrice"].toDouble();
        int qty = h["Qty"].toInt();

        BrokerHolding holding;
        holding.symbol = h["ScripName"].toString();
        holding.exchange = (h["Exch"].toString() == "B") ? "BSE" : "NSE";
        holding.quantity = qty;
        holding.avg_price = avg;
        holding.ltp = ltp;
        holding.pnl = (ltp - avg) * qty;
        holdings.append(holding);
    }

    return {true, holdings, "", ts};
}

ApiResponse<BrokerFunds> FivePaisaBroker::get_funds(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto kp = unpack_key(creds.api_key);
    auto hdrs = auth_headers(creds);

    QJsonObject body;
    body["ClientCode"] = kp.client_id;
    QJsonObject req = make_body(kp.app_key, kp.client_id, body);

    auto resp = BrokerHttp::instance().post_json(QString(BASE_URL) + "/VendorsAPI/Service1.svc/V4/Margin", req, hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    QJsonArray equity = resp.json["body"].toObject()["EquityMargin"].toArray();
    if (equity.isEmpty())
        return {false, std::nullopt, "Empty EquityMargin in response", ts};

    QJsonObject m = equity[0].toObject();
    double available = m["NetAvailableMargin"].toDouble();
    double collateral = m["TotalCollateralValue"].toDouble();
    double utilized = m["MarginUtilized"].toDouble();

    BrokerFunds funds;
    funds.available_balance = available + collateral;
    funds.used_margin = utilized;
    funds.total_balance = funds.available_balance + funds.used_margin;
    funds.collateral = collateral;

    return {true, funds, "", ts};
}

ApiResponse<QVector<BrokerQuote>> FivePaisaBroker::get_quotes(const BrokerCredentials& creds,
                                                              const QVector<QString>& symbols) {
    int64_t ts = now_ts();
    auto kp = unpack_key(creds.api_key);
    auto hdrs = auth_headers(creds);

    // symbol format: "NSE:RELIANCE:3045" or "NSE:RELIANCE"
    QJsonArray data_arr;
    QVector<QString> sym_keys;
    for (const QString& sym : symbols) {
        QStringList parts = sym.split(':');
        QString exchange = parts.size() >= 1 ? parts[0] : "NSE";
        QString scrip = parts.size() >= 2 ? parts[1] : sym;
        QString token = parts.size() >= 3 ? parts[2] : "0";

        QJsonObject entry;
        // MarketFeed family expects Exch/ExchType (short keys), not Exchange/ExchangeType.
        entry["Exch"] = fp_exchange(exchange);
        entry["ExchType"] = fp_exchange_type(exchange);
        entry["ScripCode"] = token.toInt();
        entry["Symbol"] = (token == "0") ? scrip : "";
        data_arr.append(entry);
        sym_keys.append(sym);
    }

    QJsonObject body;
    body["ClientCode"] = kp.client_id;
    body["Count"] = static_cast<int>(data_arr.size());
    body["MarketFeedData"] = data_arr;
    QJsonObject req = make_body(kp.app_key, kp.client_id, body);

    // V1/MarketFeed is the canonical quotes endpoint; MarketSnapshot is the
    // older alias and rejects the MarketFeedData/Exch/ExchType field shape.
    auto resp =
        BrokerHttp::instance().post_json(QString(BASE_URL) + "/VendorsAPI/Service1.svc/V1/MarketFeed", req, hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    QJsonArray result = resp.json["body"].toObject()["Data"].toArray();
    QVector<BrokerQuote> quotes;
    quotes.reserve(result.size());

    for (int i = 0; i < result.size(); ++i) {
        QJsonObject q = result[i].toObject();
        BrokerQuote quote;
        quote.symbol = (i < sym_keys.size()) ? sym_keys[i] : QString();
        quote.ltp = q["LastTradedPrice"].toDouble();
        quote.open = q["Open"].toDouble();
        quote.high = q["High"].toDouble();
        quote.low = q["Low"].toDouble();
        quote.close = q["PClose"].toDouble();
        quote.volume = q["Volume"].toDouble();
        quotes.append(quote);
    }

    return {true, quotes, "", ts};
}

ApiResponse<QVector<BrokerCandle>> FivePaisaBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                                const QString& resolution, const QString& from_date,
                                                                const QString& to_date) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    // Accept "EXCHANGE:SYMBOL:TOKEN"
    QStringList parts = symbol.split(':');
    QString exchange = parts.size() >= 1 ? parts[0] : "NSE";
    QString token = parts.size() >= 3 ? parts[2] : "";
    if (token.isEmpty())
        return {false, std::nullopt, "5Paisa history requires instrument token (EXCHANGE:SYMBOL:TOKEN)", ts};

    QString exch = fp_exchange(exchange);
    QString exch_type = fp_exchange_type(exchange);
    QString interval = fp_interval(resolution);

    // GET /V2/historical/{Exch}/{ExchType}/{token}/{interval}?from=YYYY-MM-DD&end=YYYY-MM-DD
    QString url = QString("%1/V2/historical/%2/%3/%4/%5?from=%6&end=%7")
                      .arg(BASE_URL, exch, exch_type, token, interval, from_date, to_date);

    auto resp = BrokerHttp::instance().get(url, hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    if (resp.json["status"].toString() != "success")
        return {false, std::nullopt, checked_error(resp, "No historical data"), ts};

    QJsonArray candles = resp.json["data"].toObject()["candles"].toArray();
    QVector<BrokerCandle> result;
    result.reserve(candles.size());

    for (const auto& item : candles) {
        QJsonArray c = item.toArray();
        if (c.size() < 6)
            continue;

        // [timestamp_str, open, high, low, close, volume]
        // timestamp: "YYYY-MM-DDTHH:MM:SS" (UTC)
        QString time_str = c[0].toString();
        QDateTime dt = QDateTime::fromString(time_str, Qt::ISODate);
        if (!dt.isValid())
            dt = QDateTime::fromString(time_str, "yyyy-MM-ddTHH:mm:ss");

        double open = c[1].toDouble();
        double high = c[2].toDouble();
        double low = c[3].toDouble();
        double close = c[4].toDouble();
        int vol = c[5].toInt();

        // Skip zero-volume / all-zero candles (holidays)
        if (vol == 0 || (open == 0 && high == 0 && low == 0 && close == 0))
            continue;

        BrokerCandle candle;
        candle.timestamp = dt.toMSecsSinceEpoch();
        candle.open = open;
        candle.high = high;
        candle.low = low;
        candle.close = close;
        candle.volume = vol;
        result.append(candle);
    }

    return {true, result, "", ts};
}

} // namespace fincept::trading
