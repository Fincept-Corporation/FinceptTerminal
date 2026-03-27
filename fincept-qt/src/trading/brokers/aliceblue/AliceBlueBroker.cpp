#include "trading/brokers/aliceblue/AliceBlueBroker.h"
#include "trading/brokers/BrokerHttp.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace fincept::trading {

// Auth endpoint is different from the trading API base
static const char* AUTH_URL = "https://ant.aliceblueonline.com/open-api/od/v1/vendor/getUserDetails";
static const char* API_BASE  = "https://a3.aliceblueonline.com";

static int64_t now_ts() { return QDateTime::currentSecsSinceEpoch(); }

// ============================================================================
// Helpers
// ============================================================================

bool AliceBlueBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 401 || resp.status_code == 403)
        return true;
    if (!resp.json.isEmpty()) {
        QString status = resp.json["status"].toString();
        if (status == "Not_ok" || status == "Not_Ok" || status == "NOT_OK")
            return true;
        // emsg containing auth-related text
        QString emsg = resp.json["emsg"].toString().toLower();
        if (emsg.contains("session") || emsg.contains("token") || emsg.contains("unauthori"))
            return true;
    }
    return false;
}

QString AliceBlueBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    if (!resp.json.isEmpty()) {
        QString msg = resp.json["message"].toString();
        if (!msg.isEmpty()) return msg;
        msg = resp.json["emsg"].toString();
        if (!msg.isEmpty()) return msg;
        QString status = resp.json["status"].toString();
        if (!status.isEmpty() && status != "Ok")
            return QString("AliceBlue error: status=%1").arg(status);
    }
    if (!resp.error.isEmpty()) return resp.error;
    if (!resp.raw_body.isEmpty()) return resp.raw_body.left(200);
    return fallback;
}

QString AliceBlueBroker::ab_product(ProductType p) {
    switch (p) {
        case ProductType::Intraday: return "INTRADAY";
        case ProductType::Delivery: return "LONGTERM";
        case ProductType::Margin:   return "NRML";
        default:                    return "INTRADAY";
    }
}

QString AliceBlueBroker::ab_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "MARKET";
        case OrderType::Limit:         return "LIMIT";
        case OrderType::StopLoss:      return "SL";
        case OrderType::StopLossLimit: return "SLM";
        default:                       return "MARKET";
    }
}

// AliceBlue history API: "1" for intraday (all resolutions), "D" for daily
QString AliceBlueBroker::ab_resolution(const QString& resolution) {
    if (resolution == "D" || resolution == "1D" || resolution == "W" || resolution == "1W")
        return "D";
    return "1"; // 1m, 5m, 15m, 30m, 60m, 1h all use "1" (1-minute bars)
}

// ============================================================================
// Auth headers — Bearer session token only
// ============================================================================

QMap<QString, QString> AliceBlueBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Authorization", "Bearer " + creds.access_token},
        {"Content-Type",  "application/json"},
        {"Accept",        "application/json"},
    };
}

// ============================================================================
// Phase 2: exchange_token
// Checksum = SHA256(userId + authCode + apiSecret) — hex digest
// POST https://ant.aliceblueonline.com/open-api/od/v1/vendor/getUserDetails
// No auth header needed — checksum is the credential
// Response: { "stat": "Ok", "userSession": "...", "clientId": "..." }
// ============================================================================

TokenExchangeResponse AliceBlueBroker::exchange_token(const QString& api_key,
                                                       const QString& api_secret,
                                                       const QString& auth_code) {
    // api_key = userId, api_secret = apiSecret, auth_code = request/auth token
    QString checksum_input = api_key + auth_code + api_secret;
    QString checksum = QString(
        QCryptographicHash::hash(checksum_input.toUtf8(), QCryptographicHash::Sha256).toHex());

    QJsonObject body;
    body["checkSum"] = checksum;

    QMap<QString, QString> headers = {
        {"Content-Type", "application/json"},
        {"Accept",       "application/json"},
    };

    auto resp = BrokerHttp::instance().post_json(AUTH_URL, body, headers);

    if (!resp.success)
        return {false, "", "", "", checked_error(resp, "Network error")};

    QString stat = resp.json["stat"].toString();
    if (stat != "Ok")
        return {false, "", "", "", checked_error(resp, "Authentication failed")};

    QString session = resp.json["userSession"].toString();
    QString client_id = resp.json["clientId"].toString();

    if (session.isEmpty())
        return {false, "", "", "", "No userSession in response"};

    return {true, session, client_id, "", ""};
}

// ============================================================================
// Phase 3: Order operations
// ============================================================================

OrderPlaceResponse AliceBlueBroker::place_order(const BrokerCredentials& creds,
                                                 const UnifiedOrder& order) {
    auto hdrs = auth_headers(creds);

    // instrumentId must be the numeric exchange token (string)
    // InstrumentService lookup would go here in production; use "0" as fallback
    QString instrument_id = order.instrument_token.isEmpty() ? "0" : order.instrument_token;

    QJsonObject item;
    item["exchange"]                 = order.exchange;
    item["instrumentId"]             = instrument_id;
    item["transactionType"]          = (order.side == OrderSide::Buy) ? "BUY" : "SELL";
    item["quantity"]                 = order.quantity;
    item["product"]                  = ab_product(order.product_type);
    item["orderComplexity"]          = "REGULAR";
    item["orderType"]                = ab_order_type(order.order_type);
    item["validity"]                 = "DAY";
    item["price"]                    = (order.order_type == OrderType::Market)
                                           ? "0" : QString::number(order.price, 'f', 2);
    item["slTriggerPrice"]           = (order.order_type == OrderType::StopLoss ||
                                        order.order_type == OrderType::StopLossLimit)
                                           ? QString::number(order.stop_price, 'f', 2) : "0";
    item["slLegPrice"]               = "";
    item["targetLegPrice"]           = "";
    item["disclosedQuantity"]        = "";
    item["marketProtectionPercent"]  = "";
    item["deviceId"]                 = "";
    item["trailingSlAmount"]         = "";
    item["apiOrderSource"]           = "";
    item["algoId"]                   = "";
    item["orderTag"]                 = "fincept";

    // API expects an array of one item
    QJsonArray payload;
    payload.append(item);

    auto resp = BrokerHttp::instance().post_json_array(
        QString(API_BASE) + "/open-api/od/v1/orders/placeorder",
        payload, hdrs);

    if (!resp.success)
        return {false, "", checked_error(resp, "Network error")};
    if (is_token_expired(resp))
        return {false, "", "[TOKEN_EXPIRED]"};

    QString status = resp.json["status"].toString();
    if (status != "Ok")
        return {false, "", checked_error(resp, "Place order failed")};

    QJsonArray results = resp.json["result"].toArray();
    if (results.isEmpty())
        return {false, "", "No result in response"};

    QJsonObject result = results[0].toObject();
    // Check for per-result error (AliceBlue may return top-level Ok but result error)
    QString result_status = result["status"].toString();
    if (!result_status.isEmpty() && result_status != "Ok" && result["brokerOrderId"].toString().isEmpty())
        return {false, "", result["message"].toString()};

    QString order_id = result["brokerOrderId"].toString();
    if (order_id.isEmpty())
        return {false, "", "No brokerOrderId in response"};

    return {true, order_id, ""};
}

ApiResponse<QJsonObject> AliceBlueBroker::modify_order(const BrokerCredentials& creds,
                                                        const QString& order_id,
                                                        const QJsonObject& mods) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    QJsonObject body;
    body["brokerOrderId"]     = order_id;
    body["quantity"]          = mods.value("quantity").toInt(0);
    body["orderType"]         = mods.contains("order_type")
                                    ? mods["order_type"].toString() : "LIMIT";
    body["slTriggerPrice"]    = mods.contains("trigger_price")
                                    ? QString::number(mods["trigger_price"].toDouble(), 'f', 2) : "0";
    body["price"]             = mods.contains("price")
                                    ? QString::number(mods["price"].toDouble(), 'f', 2) : "0";
    body["slLegPrice"]        = "";
    body["trailingSlAmount"]  = "";
    body["targetLegPrice"]    = "";
    body["validity"]          = "DAY";
    body["disclosedQuantity"] = "0";
    body["marketProtection"]  = "";
    body["deviceId"]          = "";

    auto resp = BrokerHttp::instance().post_json(
        QString(API_BASE) + "/open-api/od/v1/orders/modify", body, hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    if (resp.json["status"].toString() != "Ok")
        return {false, std::nullopt, checked_error(resp, "Modify failed"), ts};

    return {true, resp.json, "", ts};
}

ApiResponse<QJsonObject> AliceBlueBroker::cancel_order(const BrokerCredentials& creds,
                                                        const QString& order_id) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    QJsonObject body;
    body["brokerOrderId"] = order_id;

    auto resp = BrokerHttp::instance().post_json(
        QString(API_BASE) + "/open-api/od/v1/orders/cancel", body, hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    if (resp.json["status"].toString() != "Ok")
        return {false, std::nullopt, checked_error(resp, "Cancel failed"), ts};

    return {true, resp.json, "", ts};
}

// ============================================================================
// Phase 4: Read operations
// ============================================================================

ApiResponse<QVector<BrokerOrderInfo>> AliceBlueBroker::get_orders(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    auto resp = BrokerHttp::instance().get(
        QString(API_BASE) + "/open-api/od/v1/orders/book", hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    // "No orders" returns status Ok with empty result or error message — treat as empty
    if (resp.json["status"].toString() != "Ok") {
        QString msg = resp.json["message"].toString();
        if (msg.contains("Failed to retrieve") || msg.contains("No orders", Qt::CaseInsensitive))
            return {true, QVector<BrokerOrderInfo>{}, "", ts};
        return {false, std::nullopt, checked_error(resp, "Fetch orders failed"), ts};
    }

    QJsonArray results = resp.json["result"].toArray();
    QVector<BrokerOrderInfo> orders;
    orders.reserve(results.size());

    auto parse_status = [](const QString& s) -> QString {
        QString ls = s.toLower();
        if (ls == "complete")  return "filled";
        if (ls == "open")      return "open";
        if (ls == "cancelled") return "cancelled";
        if (ls == "rejected")  return "rejected";
        return ls;
    };

    for (const auto& item : results) {
        QJsonObject o = item.toObject();
        BrokerOrderInfo info;
        info.order_id      = o["brokerOrderId"].toString();
        info.symbol        = o["formattedInstrumentName"].toString().isEmpty()
                                 ? o["tradingSymbol"].toString()
                                 : o["formattedInstrumentName"].toString();
        info.exchange      = o["exchange"].toString();
        info.quantity      = o["quantity"].toInt();
        info.filled_qty    = o["filledQuantity"].toInt();
        info.price         = o["price"].toDouble();
        info.trigger_price = o["slTriggerPrice"].toDouble();
        info.status        = parse_status(o["orderStatus"].toString());
        info.side          = (o["transactionType"].toString() == "BUY") ? "buy" : "sell";
        info.order_type    = o["orderType"].toString();
        info.product_type  = o["product"].toString();
        info.timestamp     = o["orderTime"].toString();
        orders.append(info);
    }

    return {true, orders, "", ts};
}

ApiResponse<QJsonObject> AliceBlueBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    auto resp = BrokerHttp::instance().get(
        QString(API_BASE) + "/open-api/od/v1/orders/trades", hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    return {true, resp.json, "", ts};
}

ApiResponse<QVector<BrokerPosition>> AliceBlueBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    auto resp = BrokerHttp::instance().get(
        QString(API_BASE) + "/open-api/od/v1/positions", hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    if (resp.json["status"].toString() != "Ok") {
        QString msg = resp.json["message"].toString();
        if (msg.contains("No position") || msg.contains("not found", Qt::CaseInsensitive) ||
            msg.contains("Failed to retrieve"))
            return {true, QVector<BrokerPosition>{}, "", ts};
        return {false, std::nullopt, checked_error(resp, "Fetch positions failed"), ts};
    }

    QJsonArray results = resp.json["result"].toArray();
    QVector<BrokerPosition> positions;

    for (const auto& item : results) {
        QJsonObject p = item.toObject();
        int net_qty = p["netQuantity"].toInt();
        if (net_qty == 0) continue;

        BrokerPosition pos;
        pos.symbol       = p["tradingSymbol"].toString().isEmpty()
                               ? p["formattedInstrumentName"].toString()
                               : p["tradingSymbol"].toString();
        pos.exchange     = p["exchange"].toString();
        pos.quantity     = net_qty;
        pos.avg_price    = p["dayBuyPrice"].toDouble() > 0
                               ? p["dayBuyPrice"].toDouble()
                               : p["netAveragePrice"].toDouble();
        pos.ltp          = p["ltp"].toDouble();
        pos.pnl          = p["unrealisedPnl"].toDouble();
        pos.product_type = p["product"].toString();
        positions.append(pos);
    }

    return {true, positions, "", ts};
}

ApiResponse<QVector<BrokerHolding>> AliceBlueBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    auto resp = BrokerHttp::instance().get(
        QString(API_BASE) + "/open-api/od/v1/holdings/CNC", hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    if (resp.json["status"].toString() != "Ok") {
        QString msg = resp.json["message"].toString();
        if (msg.contains("No holding") || msg.contains("not found", Qt::CaseInsensitive) ||
            msg.contains("Failed to retrieve"))
            return {true, QVector<BrokerHolding>{}, "", ts};
        return {false, std::nullopt, checked_error(resp, "Fetch holdings failed"), ts};
    }

    QJsonArray results = resp.json["result"].toArray();
    QVector<BrokerHolding> holdings;
    holdings.reserve(results.size());

    for (const auto& item : results) {
        QJsonObject h = item.toObject();

        QString nse_sym = h["nseTradingSymbol"].toString();
        QString bse_sym = h["bseTradingSymbol"].toString();
        QString symbol  = nse_sym.isEmpty() ? bse_sym : nse_sym;
        if (symbol.isEmpty()) continue;

        QString exchange = nse_sym.isEmpty() ? "BSE" : "NSE";

        int qty = h["dpQuantity"].toInt();
        if (qty == 0) qty = h["totalQuantity"].toInt();

        double avg_price = h["averageTradedPrice"].toDouble();
        if (avg_price == 0.0) avg_price = h["investedPrice"].toDouble();

        double ltp = h["ltp"].toDouble();

        BrokerHolding holding;
        holding.symbol    = symbol;
        holding.exchange  = exchange;
        holding.quantity  = qty;
        holding.avg_price = avg_price;
        holding.ltp       = ltp;
        holding.pnl       = (ltp - avg_price) * qty;
        holdings.append(holding);
    }

    return {true, holdings, "", ts};
}

ApiResponse<BrokerFunds> AliceBlueBroker::get_funds(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    auto resp = BrokerHttp::instance().get(
        QString(API_BASE) + "/open-api/od/v1/limits/", hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    if (resp.json["status"].toString() != "Ok")
        return {false, std::nullopt, checked_error(resp, "Fetch funds failed"), ts};

    QJsonArray results = resp.json["result"].toArray();
    if (results.isEmpty())
        return {false, std::nullopt, "Empty limits response", ts};

    QJsonObject item = results[0].toObject();
    double trading_limit  = item["tradingLimit"].toDouble();
    double collateral     = item["collateralMargin"].toDouble();
    double utilized       = item["utilizedMargin"].toDouble();

    BrokerFunds funds;
    funds.available_balance = trading_limit + collateral;
    funds.used_margin       = utilized;
    funds.total_balance     = funds.available_balance + funds.used_margin;
    funds.collateral        = collateral;

    return {true, funds, "", ts};
}

ApiResponse<QVector<BrokerQuote>> AliceBlueBroker::get_quotes(const BrokerCredentials& creds,
                                                               const QVector<QString>& symbols) {
    // AliceBlue V2 REST API does not have a dedicated quote endpoint.
    // Quotes require WebSocket subscription — not available in REST broker model.
    // Return empty set; the UI should show last known prices from holdings/positions.
    int64_t ts = now_ts();
    (void)creds; (void)symbols;
    return {true, QVector<BrokerQuote>{}, "", ts};
}

ApiResponse<QVector<BrokerCandle>> AliceBlueBroker::get_history(const BrokerCredentials& creds,
                                                                  const QString& symbol,
                                                                  const QString& resolution,
                                                                  const QString& from_date,
                                                                  const QString& to_date) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    // BSE historical data not supported by AliceBlue
    QString exchange = "NSE";
    QString trading_symbol = symbol;
    int colon = symbol.indexOf(':');
    if (colon != -1) {
        exchange       = symbol.left(colon);
        trading_symbol = symbol.mid(colon + 1);
    }
    if (exchange == "BSE" || exchange == "BCD")
        return {false, std::nullopt, "AliceBlue does not support BSE historical data", ts};

    // instrument_token should be passed in from the caller via symbol "NSE:RELIANCE:3045"
    // For now accept "EXCHANGE:SYMBOL:TOKEN" format if colon count == 2
    QString instrument_token;
    QStringList parts = symbol.split(':');
    if (parts.size() == 3) {
        exchange          = parts[0];
        trading_symbol    = parts[1];
        instrument_token  = parts[2];
    }
    if (instrument_token.isEmpty())
        return {false, std::nullopt, "AliceBlue requires instrument token for historical data", ts};

    // Convert YYYY-MM-DD to Unix milliseconds (IST 09:15 start, 23:59 end)
    auto to_epoch_ms = [](const QString& date_str, bool is_end) -> QString {
        QDateTime dt = QDateTime::fromString(date_str, "yyyy-MM-dd");
        if (!dt.isValid()) return "0";
        if (is_end)
            dt.setTime(QTime(23, 59, 59));
        else
            dt.setTime(QTime(9, 15, 0));
        // IST = UTC+5:30 → subtract 5h30m to get UTC, then ms
        qint64 epoch_ms = dt.toMSecsSinceEpoch() - (5 * 3600 + 30 * 60) * 1000LL;
        return QString::number(epoch_ms);
    };

    QString res      = ab_resolution(resolution);
    QString from_ms  = to_epoch_ms(from_date, false);
    QString to_ms    = to_epoch_ms(to_date, true);

    QJsonObject body;
    body["token"]      = instrument_token;
    body["exchange"]   = exchange;
    body["from"]       = from_ms;
    body["to"]         = to_ms;
    body["resolution"] = res;

    auto resp = BrokerHttp::instance().post_json(
        QString(API_BASE) + "/open-api/od/ChartAPIService/api/chart/history", body, hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    QString stat = resp.json["stat"].toString().toLower();
    if (stat == "not_ok" || stat == "not ok" || !resp.json.contains("result"))
        return {false, std::nullopt, checked_error(resp, "No historical data"), ts};

    QJsonArray candles = resp.json["result"].toArray();
    QVector<BrokerCandle> result;
    result.reserve(candles.size());

    for (const auto& item : candles) {
        QJsonObject c = item.toObject();
        BrokerCandle candle;
        // Response fields: time (YYYY-MM-DD HH:MM:SS), open, high, low, close, volume
        candle.timestamp = QDateTime::fromString(c["time"].toString(), "yyyy-MM-dd HH:mm:ss")
                               .toMSecsSinceEpoch();
        candle.open      = c["open"].toDouble();
        candle.high      = c["high"].toDouble();
        candle.low       = c["low"].toDouble();
        candle.close     = c["close"].toDouble();
        candle.volume    = c["volume"].toDouble();
        result.append(candle);
    }

    return {true, result, "", ts};
}

} // namespace fincept::trading
