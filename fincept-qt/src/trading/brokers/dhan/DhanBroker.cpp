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
#include "trading/adapter/BrokerEnumMap.h"
#include "trading/brokers/BrokerHttp.h"
#include "trading/brokers/BrokerTokenUtil.h"
#include "trading/instruments/InstrumentService.h"

#include <QDateTime>
#include <QJsonArray>
#include <QSet>

#include <algorithm>

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

// Per DhanHQ v2 spec:
//   STOP_LOSS         = SL   = stop-with-LIMIT (needs price + triggerPrice)
//   STOP_LOSS_MARKET  = SL-M = stop-with-MARKET (needs triggerPrice only)
const BrokerEnumMap<QString>& DhanBroker::dhan_enum_map() {
    static const auto m = [] {
        BrokerEnumMap<QString> x;
        x.set(OrderType::Market, "MARKET");
        x.set(OrderType::Limit, "LIMIT");
        x.set(OrderType::StopLoss, "STOP_LOSS_MARKET");
        x.set(OrderType::StopLossLimit, "STOP_LOSS");
        x.set(ProductType::Intraday, "INTRADAY");
        x.set(ProductType::Delivery, "CNC");
        x.set(ProductType::Margin, "MARGIN");
        return x;
    }();
    return m;
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
    const QString error_type = resp.json.value("errorType").toString();
    const QString error_code = resp.json.value("errorCode").toString();
    const QString msg = resp.json.value("errorMessage").toString();

    // Rate-limiting / throttling is TRANSIENT — it must never be treated as an
    // expired token (doing so disconnects a perfectly valid session). DhanHQ v2
    // signals this with DH-904 ("Too many requests... try throttling") or 805
    // (data-API connection/rate limit). Some gateways ride these on an HTTP 401,
    // so this check comes first and short-circuits the 401 path below.
    const bool rate_limited = error_code.contains("904") || error_code.contains("805") ||
                              error_type.compare("Rate_Limit", Qt::CaseInsensitive) == 0 ||
                              msg.contains("rate limit", Qt::CaseInsensitive) ||
                              msg.contains("Too many requests", Qt::CaseInsensitive);
    if (rate_limited) {
        LOG_WARN(TAG, QString("Dhan rate-limited (status=%1 code=%2 type=%3) — NOT treating as token expiry; "
                              "throttle and retry. msg=%4")
                          .arg(resp.status_code)
                          .arg(error_code, error_type, msg));
        return false;
    }

    // Definitive auth-token failures per the DhanHQ v2 error annexure:
    //   DH-901 invalid/expired, DH-807 expired, DH-809 invalid, DH-810 bad client id.
    const bool auth_err = resp.status_code == 401 || error_type == "Invalid_Authentication" ||
                          error_type == "Unauthorized" || error_code == "DH-901" || error_code == "DH-807" ||
                          error_code == "DH-809" || error_code == "DH-810" ||
                          resp.raw_body.contains("Invalid_Authentication");

    if (auth_err) {
        // Log the exact response that triggers a disconnect so a *misclassified*
        // transient error is diagnosable from the log rather than guessed at.
        LOG_WARN(TAG, QString("Dhan classified TOKEN_EXPIRED → will disconnect. status=%1 type=%2 code=%3 msg=%4 "
                              "body=%5")
                          .arg(resp.status_code)
                          .arg(error_type, error_code, msg, resp.raw_body.left(400)));
    }
    return auth_err;
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

// Authoritative session check. DhanHQ v2 recommends GET /v2/profile, which
// returns the account's dhanClientId + tokenValidity for a live token. This is
// lighter and less prone to spurious errors than repeatedly polling fundlimit,
// and it logs the raw response so any disconnect is fully diagnosable.
SessionCheck DhanBroker::validate_session(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(BASE + "/v2/profile", auth_headers(creds));
    LOG_INFO(TAG, QString("validate_session GET /v2/profile → status=%1 success=%2 body=%3")
                      .arg(resp.status_code)
                      .arg(resp.success)
                      .arg(resp.raw_body.left(300)));

    // A 200 carrying the client id means the token is live. Parse the real
    // tokenValidity ("DD/MM/YYYY HH:mm", IST) so the persisted expiry hint is
    // updated to the broker's actual value instead of our coarse 24h estimate.
    if (resp.success && (resp.json.contains("dhanClientId") || resp.json.contains("tokenValidity"))) {
        qint64 exp = 0;
        const QString tv = resp.json.value("tokenValidity").toString();
        if (!tv.isEmpty()) {
            QDateTime dt = QDateTime::fromString(tv, "dd/MM/yyyy HH:mm");
            if (dt.isValid()) {
                dt.setTimeZone(ist_zone());
                exp = dt.toSecsSinceEpoch();
            }
        }
        return {SessionCheck::Status::Valid, exp, QString()};
    }

    // Definitive auth failure → expired. Anything else (rate limit, 5xx,
    // network) is inconclusive — the caller must NOT disconnect on it.
    if (is_token_expired(resp))
        return {SessionCheck::Status::Expired, 0,
                QStringLiteral("[TOKEN_EXPIRED] ") + checked_error(resp, "token invalid/expired")};

    LOG_WARN(TAG, QString("validate_session inconclusive (status=%1) — leaving connection state unchanged")
                      .arg(resp.status_code));
    return {SessionCheck::Status::Inconclusive, 0, checked_error(resp, "profile check failed")};
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
    // Accept any non-auth error (market closed, etc.) as token valid.
    // Dhan v2 access tokens are valid for 24h from generation (rolling window).
    // The live validation sweep is the authoritative guard; this expiry is only
    // a startup hint so a token isn't shown green long after it has lapsed.
    const QString extra = with_token_expiry({}, rolling_expiry_epoch(24));
    LOG_INFO(TAG, "Token exchange OK, client_id=" + api_key);
    return {true, api_secret, /*refresh*/ "", api_key, /*additional*/ extra, ""};
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
    body["productType"] = dhan_enum_map().product_or(order.product_type, "INTRADAY");
    body["orderType"] = dhan_enum_map().order_type_or(order.order_type, "MARKET");
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

// ── LTP hydration ─────────────────────────────────────────────────────────────
// Positions/holdings responses carry no price field. Batch-fetch last prices via
// POST /v2/marketfeed/ltp (same endpoint as get_quotes) using the securityIds the
// rows themselves carry, keyed "<segment>:<securityId>". Best-effort: on any
// failure the map stays empty and rows keep ltp = 0 (never fabricate prices).
static QMap<QString, double> fetch_ltp_by_security_id(const QMap<QString, QString>& headers,
                                                      const QMap<QString, QJsonArray>& ids_by_segment) {
    QMap<QString, double> ltp_map;
    if (ids_by_segment.isEmpty())
        return ltp_map;

    QJsonObject body;
    for (auto it = ids_by_segment.constBegin(); it != ids_by_segment.constEnd(); ++it)
        body[it.key()] = it.value();

    auto resp = BrokerHttp::instance().post_json(BASE + "/v2/marketfeed/ltp", body, headers);
    if (!resp.success || resp.json.value("errorType").toString().length() > 0)
        return ltp_map;

    // Response: {"data": {"NSE_EQ": {"12345": {"last_price": 500.0}}}}
    const auto data = resp.json.value("data").toObject();
    for (auto seg_it = data.constBegin(); seg_it != data.constEnd(); ++seg_it) {
        const auto tokens = seg_it.value().toObject();
        for (auto tok_it = tokens.constBegin(); tok_it != tokens.constEnd(); ++tok_it)
            ltp_map[seg_it.key() + ":" + tok_it.key()] = tok_it.value().toObject().value("last_price").toDouble();
    }
    return ltp_map;
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
    QVector<QString> ltp_keys; // parallel to positions: "<segment>:<securityId>"
    QMap<QString, QJsonArray> ids_by_segment;
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
        // Dhan v2 positions response uses buyAvg/sellAvg (not avgCostPrice).
        // Pick the side that has volume — for net-long use buyAvg, for net-short sellAvg.
        const double buy_avg = p.value("buyAvg").toDouble();
        const double sell_avg = p.value("sellAvg").toDouble();
        pos.avg_price = qty > 0 ? buy_avg : sell_avg;
        if (pos.avg_price == 0.0) // older deployments do return avgCostPrice
            pos.avg_price = p.value("avgCostPrice").toDouble();
        // Positions don't carry an LTP field — hydrated below via /v2/marketfeed/ltp
        // using the row's own securityId + exchangeSegment.
        pos.ltp = 0.0;
        pos.pnl = p.value("unrealizedProfit").toDouble();
        pos.day_pnl = p.value("realizedProfit").toDouble();
        pos.side = qty > 0 ? "LONG" : "SHORT";
        pos.pnl_pct = 0.0;
        const QString seg = p.value("exchangeSegment").toString();
        const QString sec_id = p.value("securityId").toVariant().toString();
        ltp_keys.append(seg + ":" + sec_id);
        if (!seg.isEmpty() && !sec_id.isEmpty())
            ids_by_segment[seg].append(sec_id);
        positions.append(pos);
    }

    // Hydrate live LTP so MTM/pnl_pct aren't silently 0 (best-effort, read-only).
    const auto ltp_map = fetch_ltp_by_security_id(auth_headers(creds), ids_by_segment);
    for (int i = 0; i < positions.size(); ++i) {
        const double ltp = ltp_map.value(ltp_keys.at(i), 0.0);
        if (ltp <= 0.0)
            continue;
        auto& pos = positions[i];
        pos.ltp = ltp;
        if (pos.avg_price > 0.0)
            pos.pnl_pct = ((pos.ltp - pos.avg_price) / pos.avg_price) * 100.0;
        if (pos.pnl == 0.0) // keep broker-reported MTM when present
            pos.pnl = (pos.ltp - pos.avg_price) * pos.quantity;
    }
    return {true, positions, "", ts};
}

// ── Get Holdings ──────────────────────────────────────────────────────────────
// GET /v2/holdings — returns array directly
ApiResponse<QVector<BrokerHolding>> DhanBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto resp = BrokerHttp::instance().get(BASE + "/v2/holdings", auth_headers(creds));

    // Dhan reports "account has no holdings" as an ERROR envelope ("No holdings
    // available") rather than an empty array — surface it as a successful empty
    // result so a connected account isn't flagged as a failed fetch.
    if (!is_token_expired(resp) && resp.raw_body.contains("No holdings available", Qt::CaseInsensitive))
        return {true, QVector<BrokerHolding>{}, "", ts};

    if (!resp.success || resp.json.value("errorType").toString().length() > 0)
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};

    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(resp.raw_body.toUtf8(), &err);
    QJsonArray arr;
    if (doc.isArray())
        arr = doc.array();

    QVector<BrokerHolding> holdings;
    holdings.reserve(arr.size());
    QVector<QString> ltp_keys; // parallel to holdings: "<segment>:<securityId>"
    QMap<QString, QJsonArray> ids_by_segment;
    for (const auto& v : arr) {
        auto h = v.toObject();
        BrokerHolding holding;
        holding.symbol = h.value("tradingSymbol").toString();
        holding.exchange = h.value("exchange").toString();
        holding.quantity = h.value("totalQty").toDouble();
        holding.avg_price = h.value("avgCostPrice").toDouble();
        // /v2/holdings does NOT return an LTP field — hydrated below via
        // /v2/marketfeed/ltp using the row's own securityId.
        holding.ltp = 0.0;
        holding.invested_value = holding.quantity * holding.avg_price;
        holding.current_value = 0.0;
        holding.pnl = 0.0;
        holding.pnl_pct = 0.0;
        // Holdings carry a plain exchange ("NSE"/"BSE"/"ALL") — quote on the
        // matching equity segment (dhan_exchange defaults unknowns to NSE_EQ).
        const QString seg = dhan_exchange(holding.exchange);
        const QString sec_id = h.value("securityId").toVariant().toString();
        ltp_keys.append(seg + ":" + sec_id);
        if (!sec_id.isEmpty())
            ids_by_segment[seg].append(sec_id);
        holdings.append(holding);
    }

    // Hydrate live LTP and derive value/P&L — previously left 0, so holdings views
    // without a live-quote subscription showed ₹0 value/P&L for real positions.
    const auto ltp_map = fetch_ltp_by_security_id(auth_headers(creds), ids_by_segment);
    for (int i = 0; i < holdings.size(); ++i) {
        const double ltp = ltp_map.value(ltp_keys.at(i), 0.0);
        if (ltp <= 0.0)
            continue;
        auto& holding = holdings[i];
        holding.ltp = ltp;
        holding.current_value = holding.quantity * ltp;
        holding.pnl = holding.current_value - holding.invested_value;
        if (holding.invested_value > 0.0)
            holding.pnl_pct = (holding.pnl / holding.invested_value) * 100.0;
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
            // Dhan nests OHLC under a sub-object: {"last_price":..., "ohlc":{open,high,low,close}}.
            // Reading at the top level (the old code) returned 0 for OHLC every time.
            const auto ohlc = q.value("ohlc").toObject();
            quote.open = ohlc.value("open").toDouble(q.value("open").toDouble());
            quote.high = ohlc.value("high").toDouble(q.value("high").toDouble());
            quote.low = ohlc.value("low").toDouble(q.value("low").toDouble());
            quote.close = ohlc.value("close").toDouble(q.value("close").toDouble());
            quote.volume = q.value("volume").toDouble();
            if (quote.close > 0) {
                quote.change = quote.ltp - quote.close;
                quote.change_pct = (quote.ltp - quote.close) / quote.close * 100.0;
            }
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
    const QString res_lc = resolution.toLower();
    const bool is_daily = (res_lc == "d" || res_lc == "1d" || res_lc == "w" || res_lc == "m");
    static const QMap<QString, QString> intraday_map = {
        {"1", "1"},   {"1m", "1"},   {"5", "5"},   {"5m", "5"},   {"15", "15"}, {"15m", "15"},
        {"25", "25"}, {"25m", "25"}, {"60", "60"}, {"60m", "60"}, {"1h", "60"},
    };
    const QString interval_str = intraday_map.value(resolution, "");
    if (!is_daily && interval_str.isEmpty()) {
        // Unsupported intraday interval (Dhan supports only 1/5/15/25/60) — error out
        // rather than silently returning daily candles for a different timeframe.
        return {false, std::nullopt, "Unsupported resolution for Dhan history: " + resolution, ts};
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
        // Map exchange segment → Dhan `instrument` enum. EQUITY is correct only
        // for cash; F&O / currency / commodity each take a distinct value.
        // Spec values: EQUITY, INDEX, FUTIDX, FUTSTK, OPTIDX, OPTSTK,
        //              FUTCUR, OPTCUR, FUTCOM, OPTFUT.
        QString instrument = "EQUITY";
        if (seg == "NSE_FNO" || seg == "BSE_FNO") {
            // Index derivatives use OPTIDX/FUTIDX; stock derivatives use OPTSTK/FUTSTK.
            // Detect index underlyings by their well-known root symbols.
            static const QSet<QString> kIdxRoots = {"NIFTY",      "BANKNIFTY", "FINNIFTY", "MIDCPNIFTY",
                                                    "NIFTYNXT50", "SENSEX",    "BANKEX",   "SENSEX50"};
            const bool isIdx =
                std::any_of(kIdxRoots.begin(), kIdxRoots.end(),
                            [&](const QString& r) { return name.startsWith(r); });
            // Heuristic: option contract names end in CE/PE; futures contain FUT.
            if (name.endsWith("CE") || name.endsWith("PE"))
                instrument = isIdx ? "OPTIDX" : "OPTSTK";
            else if (name.contains("FUT"))
                instrument = isIdx ? "FUTIDX" : "FUTSTK";
        } else if (seg == "IDX_I") {
            instrument = "INDEX";
        } else if (seg == "MCX_COMM") {
            instrument = name.contains("FUT") ? "FUTCOM" : "OPTFUT";
        } else if (seg == "NSE_CURRENCY" || seg == "BSE_CURRENCY") {
            instrument = name.contains("FUT") ? "FUTCUR" : "OPTCUR";
        }
        body["instrument"] = instrument;
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
            // Dhan returns epoch SECONDS; BrokerCandle.timestamp is milliseconds
            // (EquityChartPanel divides by 1000 and rolls the live bar against
            // currentMSecsSinceEpoch). Without ×1000 candles land in Jan 1970 and
            // the forming bar rolls on every tick ("chart goes every second").
            c.timestamp = static_cast<int64_t>(timestamps[i].toDouble()) * 1000;
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

// ── Multi-Quote ──────────────────────────────────────────────────────────────
// POST /v2/marketfeed/ltp — {NSE_EQ: [securityId1, securityId2...]}
// Groups symbols by exchange segment, max 1000.
// Response: {"data": {"NSE_EQ": {"12345": {"last_price": 500.0, ohlc: {...}}}}}
ApiResponse<QVector<BrokerQuote>> DhanBroker::get_multi_quotes(
    const BrokerCredentials& creds,
    const QVector<QPair<QString, QString>>& symbols) {

    int64_t ts = now_ts();
    if (symbols.isEmpty())
        return {true, QVector<BrokerQuote>{}, "", ts};

    // Group securityIds by exchange segment, keep reverse map
    QMap<QString, QJsonArray> by_segment;
    QMap<QString, QPair<QString, QString>> id_to_orig; // "seg:secId" → (symbol, exchange)

    for (const auto& [sym, exch] : symbols) {
        const QString exchange = exch.isEmpty() ? "NSE" : exch;
        const QString seg = dhan_exchange(exchange);
        const QString sec_id = lookup_security_id(sym, exchange, creds.broker_id);
        if (sec_id.isEmpty())
            continue;
        by_segment[seg].append(sec_id);
        id_to_orig[seg + ":" + sec_id] = {sym, exchange};
    }

    if (by_segment.isEmpty())
        return {false, std::nullopt, "No valid security IDs found", ts};

    // Build request body
    QJsonObject body;
    for (auto it = by_segment.constBegin(); it != by_segment.constEnd(); ++it)
        body[it.key()] = it.value();

    auto resp = BrokerHttp::instance().post_json(BASE + "/v2/marketfeed/ltp", body, auth_headers(creds));
    if (!resp.success || resp.json.value("errorType").toString().length() > 0)
        return {false, std::nullopt, checked_error(resp, "get_multi_quotes failed"), ts};

    // Parse response
    QVector<BrokerQuote> quotes;
    const auto data = resp.json.value("data").toObject();
    for (auto seg_it = data.constBegin(); seg_it != data.constEnd(); ++seg_it) {
        const QString seg = seg_it.key();
        const auto tokens = seg_it.value().toObject();
        for (auto tok_it = tokens.constBegin(); tok_it != tokens.constEnd(); ++tok_it) {
            const auto q = tok_it.value().toObject();
            BrokerQuote quote;
            const QString lookup_key = seg + ":" + tok_it.key();
            if (id_to_orig.contains(lookup_key))
                quote.symbol = id_to_orig.value(lookup_key).first;
            else
                quote.symbol = tok_it.key();
            quote.ltp = q.value("last_price").toDouble();
            const auto ohlc = q.value("ohlc").toObject();
            quote.open = ohlc.value("open").toDouble(q.value("open").toDouble());
            quote.high = ohlc.value("high").toDouble(q.value("high").toDouble());
            quote.low = ohlc.value("low").toDouble(q.value("low").toDouble());
            quote.close = ohlc.value("close").toDouble(q.value("close").toDouble());
            quote.volume = q.value("volume").toDouble();
            if (quote.close > 0) {
                quote.change = quote.ltp - quote.close;
                quote.change_pct = (quote.ltp - quote.close) / quote.close * 100.0;
            }
            quote.timestamp = ts;
            quotes.append(quote);
        }
    }
    return {true, quotes, "", ts};
}

// ── Market Depth ─────────────────────────────────────────────────────────────
// POST /v2/marketfeed/ohlc — {NSE_EQ: [securityId]}
// Returns OHLC + depth data for the requested security.
// Response: {"data": {"NSE_EQ": {"12345": {last_price, ohlc:{...}, depth:{buy:[...], sell:[...]}}}}}
ApiResponse<MarketDepth> DhanBroker::get_market_depth(
    const BrokerCredentials& creds,
    const QString& symbol, const QString& exchange) {

    int64_t ts = now_ts();
    const QString exch = exchange.isEmpty() ? "NSE" : exchange;
    const QString seg = dhan_exchange(exch);
    const QString sec_id = lookup_security_id(symbol, exch, creds.broker_id);
    if (sec_id.isEmpty())
        return {false, std::nullopt, "Security ID not found for " + exch + ":" + symbol, ts};

    QJsonObject body;
    QJsonArray ids;
    ids.append(sec_id);
    body[seg] = ids;

    auto resp = BrokerHttp::instance().post_json(BASE + "/v2/marketfeed/ohlc", body, auth_headers(creds));
    if (!resp.success || resp.json.value("errorType").toString().length() > 0)
        return {false, std::nullopt, checked_error(resp, "get_market_depth failed"), ts};

    // Navigate: data → segment → securityId
    const auto data = resp.json.value("data").toObject();
    const auto seg_obj = data.value(seg).toObject();
    const auto entry = seg_obj.value(sec_id).toObject();
    if (entry.isEmpty())
        return {false, std::nullopt, "No data returned for " + seg + ":" + sec_id, ts};

    MarketDepth md;
    md.symbol = symbol;
    md.exchange = exch;
    md.ltp = entry.value("last_price").toDouble();
    md.volume = entry.value("volume").toDouble();
    md.oi = entry.value("oi").toDouble();

    const auto depth = entry.value("depth").toObject();

    const auto buy_arr = depth.value("buy").toArray();
    for (const auto& level : buy_arr) {
        auto lv = level.toObject();
        DepthLevel dl;
        dl.price = lv.value("price").toDouble();
        dl.quantity = lv.value("quantity").toInt();
        dl.orders = lv.value("orders").toInt();
        md.bids.append(dl);
    }

    const auto sell_arr = depth.value("sell").toArray();
    for (const auto& level : sell_arr) {
        auto lv = level.toObject();
        DepthLevel dl;
        dl.price = lv.value("price").toDouble();
        dl.quantity = lv.value("quantity").toInt();
        dl.orders = lv.value("orders").toInt();
        md.asks.append(dl);
    }

    return {true, md, "", ts};
}

// ============================================================================
// Pre-trade margin calculator — POST /v2/margincalculator  (native, single order)
// Mirrors OpenAlgo broker/dhan/api/margin_api.py + mapping/margin_data.py.
// Payload: {dhanClientId, exchangeSegment, transactionType, quantity,
//           productType, securityId, price, triggerPrice?}
//   productType for margin: CNC→CNC, NRML→MARGIN, MIS→INTRADAY (matches dhan_enum_map).
// Response: {totalMargin, spanMargin, exposureMargin, variableMargin, availableBalance,
//            leverage, brokerage, ...}
// ============================================================================
ApiResponse<OrderMargin> DhanBroker::get_order_margins(const BrokerCredentials& creds, const UnifiedOrder& order) {
    int64_t ts = now_ts();

    const QString sec_id = lookup_security_id(order.symbol, order.exchange, creds.broker_id);
    if (sec_id.isEmpty())
        return {false, std::nullopt, "Security ID not found for " + order.exchange + ":" + order.symbol, ts};

    QJsonObject body;
    body["dhanClientId"] = creds.api_key;
    body["exchangeSegment"] = dhan_exchange(order.exchange);
    body["transactionType"] = order.side == OrderSide::Buy ? "BUY" : "SELL";
    body["quantity"] = static_cast<int>(order.quantity);
    body["productType"] = dhan_enum_map().product_or(order.product_type, "INTRADAY");
    body["securityId"] = sec_id;
    body["price"] = order.price;
    if (order.stop_price > 0)
        body["triggerPrice"] = order.stop_price;

    auto resp = BrokerHttp::instance().post_json(BASE + "/v2/margincalculator", body, auth_headers(creds));
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};
    if (resp.json.value("errorType").toString().length() > 0 || resp.json.value("status").toString() == "failed")
        return {false, std::nullopt, checked_error(resp, "Margin calculation failed"), ts};

    const auto& j = resp.json;
    OrderMargin m;
    m.symbol = order.symbol;
    m.exchange = order.exchange;
    m.side = order.side == OrderSide::Buy ? "BUY" : "SELL";
    m.quantity = order.quantity;
    m.price = order.price;
    m.total = j.value("totalMargin").toDouble();
    m.var_margin = j.value("spanMargin").toDouble();      // SPAN
    m.elm = j.value("exposureMargin").toDouble();         // Exposure
    m.additional = j.value("variableMargin").toDouble();  // VAR / variable margin
    m.cash = j.value("availableBalance").toDouble();
    m.leverage = j.value("leverage").toDouble();
    if (m.leverage <= 0.0) {
        const double notional = order.price * order.quantity;
        if (m.total > 0.0 && notional > 0.0)
            m.leverage = notional / m.total;
    }
    return {true, m, "", ts};
}

} // namespace fincept::trading
