// DhanBroker — Dhan HQ API v2 implementation
// Auth: Client ID + Access Token (direct entry from Dhan web console, no OAuth code exchange)
// Tokens stored in BrokerCredentials:
//   api_key      = Dhan Client ID
//   access_token = JWT access token
//
// Key quirks:
//   - Exchange segments: NSE_EQ, BSE_EQ, NSE_FNO, BSE_FNO, NSE_CURRENCY, BSE_CURRENCY, MCX_COMM
//   - securityId (numeric token) required for orders, quotes, and history
//   - History has two separate endpoints: /v2/charts/historical (daily) vs /v2/charts/intraday
//   - Funds field has typo: "availabelBalance" (not "availableBalance")
//   - Token expiry: errorType == "Invalid_Authentication" in response body

#include "trading/brokers/dhan/DhanBroker.h"

#include "core/logging/Logger.h"
#include "trading/brokers/BrokerHttp.h"
#include "trading/instruments/InstrumentService.h"

#include <QDateTime>
#include <QJsonArray>

namespace fincept::trading {

static constexpr const char* TAG = "DhanBroker";
static const QString BASE = "https://api.dhan.co";

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// ── Exchange segment mapping ──────────────────────────────────────────────────
QString DhanBroker::dhan_exchange(const QString& exchange) {
    static const QMap<QString, QString> map = {
        {"NSE", "NSE_EQ"},   {"BSE", "BSE_EQ"},       {"NFO", "NSE_FNO"},
        {"BFO", "BSE_FNO"},  {"CDS", "NSE_CURRENCY"}, {"BCD", "BSE_CURRENCY"},
        {"MCX", "MCX_COMM"}, {"NSE_INDEX", "IDX_I"},  {"BSE_INDEX", "IDX_I"},
    };
    return map.value(exchange.toUpper(), "NSE_EQ");
}

// Reverse: Dhan segment → canonical exchange
static QString canonical_exchange(const QString& seg) {
    static const QMap<QString, QString> map = {
        {"NSE_EQ", "NSE"},       {"BSE_EQ", "BSE"},       {"NSE_FNO", "NFO"},  {"BSE_FNO", "BFO"},
        {"NSE_CURRENCY", "CDS"}, {"BSE_CURRENCY", "BCD"}, {"MCX_COMM", "MCX"}, {"IDX_I", "NSE_INDEX"},
    };
    return map.value(seg, seg);
}

// ── Product mapping ───────────────────────────────────────────────────────────
QString DhanBroker::dhan_product(ProductType p) {
    switch (p) {
        case ProductType::Intraday:
            return "INTRADAY";
        case ProductType::Delivery:
            return "CNC";
        case ProductType::Margin:
            return "MARGIN";
        default:
            return "INTRADAY";
    }
}

// ── Order type mapping ────────────────────────────────────────────────────────
QString DhanBroker::dhan_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:
            return "MARKET";
        case OrderType::Limit:
            return "LIMIT";
        case OrderType::StopLoss:
            return "STOP_LOSS"; // SL (trigger + limit price)
        case OrderType::StopLossLimit:
            return "STOP_LOSS_MARKET"; // SL-M (trigger only)
    }
    return "MARKET";
}

// ── Security ID lookup ────────────────────────────────────────────────────────
// Uses InstrumentService token (numeric) as Dhan securityId.
// Falls back to empty string — caller must handle missing token.
QString DhanBroker::lookup_security_id(const QString& symbol, const QString& exchange, const QString& broker_id) {
    if (!broker_id.isEmpty() && InstrumentService::instance().is_loaded(broker_id)) {
        auto tok = InstrumentService::instance().instrument_token(symbol, exchange, broker_id);
        if (tok.has_value() && tok.value() > 0)
            return QString::number(tok.value());
    }
    LOG_WARN(TAG, QString("Security ID not found for %1:%2 — instrument DB not loaded?").arg(exchange, symbol));
    return {};
}

// ── Token expiry detection ────────────────────────────────────────────────────
bool DhanBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 401)
        return true;
    // Dhan sets errorType field for auth errors
    const QString error_type = resp.json.value("errorType").toString();
    if (error_type == "Invalid_Authentication" || error_type == "Unauthorized")
        return true;
    // Also check raw body for auth error string (some endpoints return differently)
    if (resp.raw_body.contains("Invalid_Authentication"))
        return true;
    return false;
}

QString DhanBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    // Prefer errorMessage, then message, then network error, then fallback
    QString msg = resp.json.value("errorMessage").toString();
    if (msg.isEmpty())
        msg = resp.json.value("message").toString();
    if (msg.isEmpty())
        msg = resp.error.isEmpty() ? fallback : resp.error;
    if (is_token_expired(resp))
        return "[TOKEN_EXPIRED] " + msg;
    return msg;
}

// ── Auth headers ──────────────────────────────────────────────────────────────
QMap<QString, QString> DhanBroker::auth_headers(const BrokerCredentials& creds) const {
    return {{"access-token", creds.access_token},
            {"client-id", creds.api_key},
            {"Content-Type", "application/json"},
            {"Accept", "application/json"}};
}

// ── Token Exchange ─────────────────────────────────────────────────────────────
// Dhan uses direct token entry — user copies access token from Dhan web console.
// api_key   = Client ID
// api_secret = Access Token (long JWT)
// auth_code  = unused
TokenExchangeResponse DhanBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                 const QString& /*auth_code*/) {
    if (api_key.trimmed().isEmpty()) {
        return {false, "", "", "", "", "Client ID is required"};
    }
    if (api_secret.trimmed().isEmpty() || api_secret.length() < 20) {
        return {false, "", "", "", "", "Access token appears invalid (too short)"};
    }

    // Validate by calling fundlimit — lightweight auth check
    QMap<QString, QString> hdrs = {{"access-token", api_secret},
                                   {"client-id", api_key},
                                   {"Content-Type", "application/json"},
                                   {"Accept", "application/json"}};
    auto resp = BrokerHttp::instance().get(BASE + "/v2/fundlimit", hdrs);

    if (is_token_expired(resp)) {
        return {false, "", "", "", "", "[TOKEN_EXPIRED] Access token is invalid or expired"};
    }
    // Accept any non-auth error (market closed, etc.) as token valid
    LOG_INFO(TAG, "Token exchange OK, client_id=" + api_key);
    return {true, api_secret, /*refresh*/ "", api_key, /*additional*/ "", ""};
}

// ── Place Order ───────────────────────────────────────────────────────────────
// POST /v2/orders
OrderPlaceResponse DhanBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    const QString sec_id = lookup_security_id(order.symbol, order.exchange, creds.broker_id);
    if (sec_id.isEmpty())
        return {false, "", "Security ID not found for " + order.exchange + ":" + order.symbol};

    QJsonObject body;
    body["dhanClientId"] = creds.api_key;
    body["transactionType"] = order.side == OrderSide::Buy ? "BUY" : "SELL";
    body["exchangeSegment"] = dhan_exchange(order.exchange);
    body["productType"] = dhan_product(order.product_type);
    body["orderType"] = dhan_order_type(order.order_type);
    body["validity"] = order.validity.isEmpty() ? "DAY" : order.validity;
    body["securityId"] = sec_id;
    body["quantity"] = static_cast<int>(order.quantity);
    body["price"] = order.price;
    body["triggerPrice"] = order.stop_price;
    body["disclosedQuantity"] = 0;
    if (order.amo)
        body["afterMarketOrder"] = true;

    auto resp = BrokerHttp::instance().post_json(BASE + "/v2/orders", body, auth_headers(creds));

    if (!resp.success || resp.json.value("errorType").toString().length() > 0) {
        const QString err = checked_error(resp, "place_order failed");
        LOG_ERROR(TAG, "place_order: " + err);
        return {false, "", err};
    }

    const QString order_id = resp.json.value("orderId").toString();
    LOG_INFO(TAG, "place_order OK: " + order_id);
    return {true, order_id, ""};
}

// ── Modify Order ──────────────────────────────────────────────────────────────
// PUT /v2/orders/{orderId}
ApiResponse<QJsonObject> DhanBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                  const QJsonObject& mods) {
    int64_t ts = now_ts();

    QJsonObject body;
    body["dhanClientId"] = creds.api_key;
    body["orderId"] = order_id;
    body["orderType"] = mods.value("order_type").toString("LIMIT");
    body["legName"] = "ENTRY_LEG";
    body["quantity"] = mods.value("quantity").toInt(0);
    body["price"] = mods.value("price").toDouble(0);
    body["triggerPrice"] = mods.value("trigger_price").toDouble(0);
    body["disclosedQuantity"] = mods.value("disclosed_quantity").toInt(0);
    body["validity"] = "DAY";

    auto resp = BrokerHttp::instance().put_json(BASE + "/v2/orders/" + order_id, body, auth_headers(creds));
    if (!resp.success || resp.json.value("errorType").toString().length() > 0)
        return {false, std::nullopt, checked_error(resp, "modify_order failed"), ts};
    return {true, resp.json, "", ts};
}

// ── Cancel Order ──────────────────────────────────────────────────────────────
// DELETE /v2/orders/{orderId}
ApiResponse<QJsonObject> DhanBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    int64_t ts = now_ts();
    auto resp = BrokerHttp::instance().del(BASE + "/v2/orders/" + order_id, auth_headers(creds));
    if (!resp.success || resp.json.value("errorType").toString().length() > 0)
        return {false, std::nullopt, checked_error(resp, "cancel_order failed"), ts};
    return {true, resp.json, "", ts};
}

// ── Get Orders ────────────────────────────────────────────────────────────────
// GET /v2/orders — returns array directly (not wrapped in data object)
ApiResponse<QVector<BrokerOrderInfo>> DhanBroker::get_orders(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto resp = BrokerHttp::instance().get(BASE + "/v2/orders", auth_headers(creds));

    if (!resp.success || resp.json.value("errorType").toString().length() > 0)
        return {false, std::nullopt, checked_error(resp, "get_orders failed"), ts};

    // Response is a JSON array at root level — parse raw_body as array
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(resp.raw_body.toUtf8(), &err);
    QJsonArray arr;
    if (doc.isArray())
        arr = doc.array();
    else if (resp.json.contains("data"))
        arr = resp.json.value("data").toArray();

    QVector<BrokerOrderInfo> orders;
    orders.reserve(arr.size());
    for (const auto& v : arr) {
        auto o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("orderId").toString();
        info.symbol = o.value("tradingSymbol").toString();
        info.exchange = canonical_exchange(o.value("exchangeSegment").toString());
        info.side = o.value("transactionType").toString().toLower();
        info.order_type = o.value("orderType").toString();
        info.product_type = o.value("productType").toString();
        info.quantity = o.value("quantity").toDouble();
        info.price = o.value("price").toDouble();
        info.trigger_price = o.value("triggerPrice").toDouble();
        info.filled_qty = o.value("filledQty").toDouble();
        info.avg_price = o.value("averageTradedPrice").toDouble();
        info.status = o.value("orderStatus").toString().toLower();
        info.timestamp = o.value("updateTime").toString();
        info.message = o.value("omsErrorDescription").toString();
        orders.append(info);
    }
    return {true, orders, "", ts};
}

// ── Get Trade Book ────────────────────────────────────────────────────────────
// GET /v2/trades
ApiResponse<QJsonObject> DhanBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto resp = BrokerHttp::instance().get(BASE + "/v2/trades", auth_headers(creds));
    if (!resp.success || resp.json.value("errorType").toString().length() > 0)
        return {false, std::nullopt, checked_error(resp, "get_trade_book failed"), ts};
    return {true, resp.json, "", ts};
}

// ── Get Positions ─────────────────────────────────────────────────────────────
// GET /v2/positions — returns array directly
ApiResponse<QVector<BrokerPosition>> DhanBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto resp = BrokerHttp::instance().get(BASE + "/v2/positions", auth_headers(creds));

    if (!resp.success || resp.json.value("errorType").toString().length() > 0)
        return {false, std::nullopt, checked_error(resp, "get_positions failed"), ts};

    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(resp.raw_body.toUtf8(), &err);
    QJsonArray arr;
    if (doc.isArray())
        arr = doc.array();

    QVector<BrokerPosition> positions;
    positions.reserve(arr.size());
    for (const auto& v : arr) {
        auto p = v.toObject();
        double qty = p.value("netQty").toDouble();
        if (qty == 0.0)
            continue;

        BrokerPosition pos;
        pos.symbol = p.value("tradingSymbol").toString();
        pos.exchange = canonical_exchange(p.value("exchangeSegment").toString());
        pos.product_type = p.value("productType").toString();
        pos.quantity = qty;
        pos.avg_price = p.value("avgCostPrice").toDouble();
        pos.ltp = p.value("lastTradedPrice").toDouble();
        pos.pnl = p.value("unrealizedProfit").toDouble();
        pos.day_pnl = p.value("realizedProfit").toDouble();
        pos.side = qty > 0 ? "LONG" : "SHORT";
        if (pos.avg_price > 0)
            pos.pnl_pct = (pos.ltp - pos.avg_price) / pos.avg_price * 100.0;
        positions.append(pos);
    }
    return {true, positions, "", ts};
}

// ── Get Holdings ──────────────────────────────────────────────────────────────
// GET /v2/holdings — returns array directly
ApiResponse<QVector<BrokerHolding>> DhanBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto resp = BrokerHttp::instance().get(BASE + "/v2/holdings", auth_headers(creds));

    if (!resp.success || resp.json.value("errorType").toString().length() > 0)
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};

    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(resp.raw_body.toUtf8(), &err);
    QJsonArray arr;
    if (doc.isArray())
        arr = doc.array();

    QVector<BrokerHolding> holdings;
    holdings.reserve(arr.size());
    for (const auto& v : arr) {
        auto h = v.toObject();
        BrokerHolding holding;
        holding.symbol = h.value("tradingSymbol").toString();
        holding.exchange = h.value("exchange").toString();
        holding.quantity = h.value("totalQty").toDouble();
        holding.avg_price = h.value("avgCostPrice").toDouble();
        holding.ltp = h.value("lastTradedPrice").toDouble();
        holding.invested_value = holding.quantity * holding.avg_price;
        holding.current_value = holding.quantity * holding.ltp;
        holding.pnl = holding.current_value - holding.invested_value;
        if (holding.invested_value > 0)
            holding.pnl_pct = holding.pnl / holding.invested_value * 100.0;
        holdings.append(holding);
    }
    return {true, holdings, "", ts};
}

// ── Get Funds ─────────────────────────────────────────────────────────────────
// GET /v2/fundlimit
// NOTE: Dhan API has a typo — field is "availabelBalance" (not "availableBalance")
ApiResponse<BrokerFunds> DhanBroker::get_funds(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto resp = BrokerHttp::instance().get(BASE + "/v2/fundlimit", auth_headers(creds));

    if (!resp.success || resp.json.value("errorType").toString().length() > 0)
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    BrokerFunds funds;
    funds.available_balance = resp.json.value("availabelBalance").toDouble(); // intentional typo
    funds.used_margin = resp.json.value("utilizedAmount").toDouble();
    funds.total_balance = funds.available_balance + funds.used_margin;
    funds.collateral = resp.json.value("collateralAmount").toDouble();
    funds.raw_data = resp.json;
    return {true, funds, "", ts};
}

// ── Get Quotes ────────────────────────────────────────────────────────────────
// POST /v2/marketfeed/ltp
// Body: {"NSE_EQ": [securityId1, securityId2], "BSE_EQ": [...]}
ApiResponse<QVector<BrokerQuote>> DhanBroker::get_quotes(const BrokerCredentials& creds,
                                                         const QVector<QString>& symbols) {
    int64_t ts = now_ts();
    if (symbols.isEmpty()) {
        return {true, QVector<BrokerQuote>{}, "", ts};
    }

    // Group securityIds by exchange segment
    QMap<QString, QJsonArray> by_segment;
    // Keep a reverse map: segment+securityId → original symbol
    QMap<QString, QString> id_to_symbol;

    for (const auto& sym : symbols) {
        QString exch = "NSE", name = sym;
        if (sym.contains(':')) {
            exch = sym.section(':', 0, 0);
            name = sym.section(':', 1);
        }
        const QString seg = dhan_exchange(exch);
        const QString sec_id = lookup_security_id(name, exch, creds.broker_id);
        if (sec_id.isEmpty())
            continue;
        by_segment[seg].append(sec_id);
        id_to_symbol[seg + ":" + sec_id] = name;
    }

    if (by_segment.isEmpty())
        return {false, std::nullopt, "No valid security IDs found", ts};

    // Build request body
    QJsonObject body;
    for (auto it = by_segment.constBegin(); it != by_segment.constEnd(); ++it)
        body[it.key()] = it.value();

    auto resp = BrokerHttp::instance().post_json(BASE + "/v2/marketfeed/ltp", body, auth_headers(creds));
    if (!resp.success || resp.json.value("errorType").toString().length() > 0)
        return {false, std::nullopt, checked_error(resp, "get_quotes failed"), ts};

    // Response: {"data": {"NSE_EQ": {"12345": {"last_price": 500.0, ...}}}}
    QVector<BrokerQuote> quotes;
    const auto data = resp.json.value("data").toObject();
    for (auto seg_it = data.constBegin(); seg_it != data.constEnd(); ++seg_it) {
        const QString seg = seg_it.key();
        const auto tokens = seg_it.value().toObject();
        for (auto tok_it = tokens.constBegin(); tok_it != tokens.constEnd(); ++tok_it) {
            const auto q = tok_it.value().toObject();
            BrokerQuote quote;
            quote.symbol = id_to_symbol.value(seg + ":" + tok_it.key(), tok_it.key());
            quote.ltp = q.value("last_price").toDouble();
            quote.open = q.value("open").toDouble();
            quote.high = q.value("high").toDouble();
            quote.low = q.value("low").toDouble();
            quote.close = q.value("close").toDouble();
            quote.volume = q.value("volume").toDouble();
            if (quote.close > 0)
                quote.change_pct = (quote.ltp - quote.close) / quote.close * 100.0;
            quote.change = quote.ltp - quote.close;
            quote.timestamp = ts;
            quotes.append(quote);
        }
    }
    return {true, quotes, "", ts};
}

// ── Get History ───────────────────────────────────────────────────────────────
// Daily:    POST /v2/charts/historical  — {securityId, exchangeSegment, instrument, fromDate, toDate}
// Intraday: POST /v2/charts/intraday   — same + interval ("1","5","15","25","60")
// Response: {open:[], high:[], low:[], close:[], volume:[], timestamp:[]} (parallel arrays, epoch s UTC)
// Intraday max chunk: 90 days
ApiResponse<QVector<BrokerCandle>> DhanBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                           const QString& resolution, const QString& from_date,
                                                           const QString& to_date) {
    int64_t ts = now_ts();

    QString exch = "NSE", name = symbol;
    if (symbol.contains(':')) {
        exch = symbol.section(':', 0, 0);
        name = symbol.section(':', 1);
    }

    const QString sec_id = lookup_security_id(name, exch, creds.broker_id);
    if (sec_id.isEmpty())
        return {false, std::nullopt, "Security ID not found for: " + symbol, ts};

    const QString seg = dhan_exchange(exch);

    // Determine daily vs intraday + interval string
    bool is_daily = (resolution == "D" || resolution == "1D" || resolution == "W" || resolution == "M");
    static const QMap<QString, QString> intraday_map = {
        {"1", "1"},   {"1m", "1"},   {"5", "5"},   {"5m", "5"},   {"15", "15"}, {"15m", "15"},
        {"25", "25"}, {"25m", "25"}, {"60", "60"}, {"60m", "60"}, {"1h", "60"},
    };
    const QString interval_str = intraday_map.value(resolution, "");
    if (!is_daily && interval_str.isEmpty()) {
        // Unsupported resolution — fall back to daily
        is_daily = true;
    }

    QDate from = QDate::fromString(from_date, "yyyy-MM-dd");
    QDate to = QDate::fromString(to_date, "yyyy-MM-dd");
    if (!from.isValid())
        from = QDate::currentDate().addYears(-1);
    if (!to.isValid())
        to = QDate::currentDate();

    auto& http = BrokerHttp::instance();
    QVector<BrokerCandle> all_candles;

    // Intraday: chunk into 90-day windows; daily: single request
    const int chunk_days = is_daily ? 3650 : 90;
    QDate chunk_start = from;

    while (chunk_start <= to) {
        QDate chunk_end = chunk_start.addDays(chunk_days - 1);
        if (chunk_end > to)
            chunk_end = to;

        QJsonObject body;
        body["securityId"] = sec_id;
        body["exchangeSegment"] = seg;
        body["instrument"] = "EQUITY";
        body["fromDate"] = chunk_start.toString("yyyy-MM-dd");
        body["toDate"] = chunk_end.toString("yyyy-MM-dd");

        QString endpoint;
        if (is_daily) {
            endpoint = BASE + "/v2/charts/historical";
        } else {
            body["interval"] = interval_str;
            endpoint = BASE + "/v2/charts/intraday";
        }

        auto resp = http.post_json(endpoint, body, auth_headers(creds));
        if (!resp.success || resp.json.value("errorType").toString().length() > 0) {
            if (all_candles.isEmpty())
                return {false, std::nullopt, checked_error(resp, "get_history failed"), ts};
            break;
        }

        // Parse parallel arrays
        const auto opens = resp.json.value("open").toArray();
        const auto highs = resp.json.value("high").toArray();
        const auto lows = resp.json.value("low").toArray();
        const auto closes = resp.json.value("close").toArray();
        const auto volumes = resp.json.value("volume").toArray();
        const auto timestamps = resp.json.value("timestamp").toArray();

        const int count = qMin(timestamps.size(), closes.size());
        for (int i = 0; i < count; ++i) {
            BrokerCandle c;
            c.timestamp = static_cast<int64_t>(timestamps[i].toDouble());
            c.open = opens.size() > i ? opens[i].toDouble() : 0.0;
            c.high = highs.size() > i ? highs[i].toDouble() : 0.0;
            c.low = lows.size() > i ? lows[i].toDouble() : 0.0;
            c.close = closes[i].toDouble();
            c.volume = volumes.size() > i ? volumes[i].toDouble() : 0.0;
            all_candles.append(c);
        }

        chunk_start = chunk_end.addDays(1);
    }

    // Sort ascending by timestamp
    std::sort(all_candles.begin(), all_candles.end(),
              [](const BrokerCandle& a, const BrokerCandle& b) { return a.timestamp < b.timestamp; });

    return {true, all_candles, "", ts};
}

} // namespace fincept::trading
