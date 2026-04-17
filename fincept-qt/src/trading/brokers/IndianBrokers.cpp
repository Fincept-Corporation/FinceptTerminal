// Indian Brokers — stub implementations for brokers not yet fully built out.
// Groww and Zerodha have full implementations in their own broker-specific directories;
// everyone else here gets auth_headers + placeholder trading-API stubs.

#include "core/logging/Logger.h"
#include "trading/brokers/AliceBlueBroker.h"
#include "trading/brokers/BrokerHttp.h"
#include "trading/brokers/DhanBroker.h"
#include "trading/brokers/FivePaisaBroker.h"
#include "trading/brokers/IIFLBroker.h"
#include "trading/brokers/KotakBroker.h"
#include "trading/brokers/MotilalBroker.h"
#include "trading/brokers/ShoonyaBroker.h"
#include "trading/brokers/UpstoxBroker.h"
#include "trading/brokers/ZerodhaBroker.h"
#include "trading/instruments/InstrumentService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>

namespace fincept::trading {

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// Helper: standard GET → parse JSON response with "status"/"s" check
static ApiResponse<QJsonObject> simple_get(const QString& url, const QMap<QString, QString>& headers,
                                           const QString& success_key = "status", const QString& success_val = "true") {
    auto resp = BrokerHttp::instance().get(url, headers);
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    return {true, resp.json, "", ts};
}

// ═══════════════════════════════════════════════════════════════════════════════
// ZERODHA — Uses form-urlencoded, SHA256 for token, "token api:access" auth
// ═══════════════════════════════════════════════════════════════════════════════

// auth_headers: only Authorization + X-Kite-Version.
// Content-Type is NOT set here — BrokerHttp::post_form sets it for form POSTs,
// and GET requests must not carry it (Zerodha rejects them).
QMap<QString, QString> ZerodhaBroker::auth_headers(const BrokerCredentials& creds) const {
    return {{"Authorization", "token " + creds.api_key + ":" + creds.access_token},
            {"X-Kite-Version", kite_api_version}};
}

const char* ZerodhaBroker::zerodha_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:
            return "MARKET";
        case OrderType::Limit:
            return "LIMIT";
        case OrderType::StopLoss:
            return "SL-M";
        case OrderType::StopLossLimit:
            return "SL";
    }
    return "MARKET";
}
const char* ZerodhaBroker::zerodha_product(ProductType p) {
    switch (p) {
        case ProductType::Intraday:
            return "MIS";
        case ProductType::Delivery:
            return "CNC";
        case ProductType::Margin:
            return "NRML";
        default:
            return "MIS";
    }
}
const char* ZerodhaBroker::zerodha_side(OrderSide s) {
    return s == OrderSide::Buy ? "BUY" : "SELL";
}
const char* ZerodhaBroker::zerodha_variety(ProductType p) {
    switch (p) {
        case ProductType::CoverOrder:
            return "co";
        case ProductType::BracketOrder:
            return "bo";
        default:
            return "regular";
    }
}

// Maps our resolution strings to Zerodha interval strings.
// Zerodha accepts: minute, 3minute, 5minute, 10minute, 15minute, 30minute,
//                  60minute, day, week, 2hour, 3day
QString ZerodhaBroker::zerodha_interval(const QString& resolution) {
    static const QMap<QString, QString> map = {
        {"1", "minute"},    {"1m", "minute"},    {"3", "3minute"},    {"3m", "3minute"},   {"5", "5minute"},
        {"5m", "5minute"},  {"10", "10minute"},  {"10m", "10minute"}, {"15", "15minute"},  {"15m", "15minute"},
        {"30", "30minute"}, {"30m", "30minute"}, {"60", "60minute"},  {"60m", "60minute"}, {"1h", "60minute"},
        {"D", "day"},       {"1D", "day"},       {"day", "day"},      {"W", "week"},       {"1W", "week"},
        {"week", "week"},   {"2h", "2hour"},     {"2H", "2hour"},     {"3d", "3day"},      {"3D", "3day"},
    };
    return map.value(resolution, "day");
}

TokenExchangeResponse ZerodhaBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                    const QString& request_token) {
    QByteArray input = (api_key + request_token + api_secret).toUtf8();
    QString checksum = QCryptographicHash::hash(input, QCryptographicHash::Sha256).toHex();

    QMap<QString, QString> params = {{"api_key", api_key}, {"request_token", request_token}, {"checksum", checksum}};
    // X-Kite-Version is required even on the session endpoint
    QMap<QString, QString> headers = {{"X-Kite-Version", kite_api_version}};
    auto resp = BrokerHttp::instance().post_form(QString(base_url()) + "/session/token", params, headers);

    TokenExchangeResponse result;
    if (!resp.success) {
        result.error = resp.error;
        return result;
    }
    if (resp.json.value("status").toString() == "success") {
        auto data = resp.json.value("data").toObject();
        result.success = true;
        result.access_token = data.value("access_token").toString();
        result.refresh_token = data.value("refresh_token").toString();
        result.user_id = data.value("user_id").toString();
    } else {
        result.error = resp.json.value("message").toString("Token exchange failed");
    }
    return result;
}

OrderPlaceResponse ZerodhaBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    QString variety = zerodha_variety(order.product_type);
    QMap<QString, QString> params = {
        {"tradingsymbol", order.symbol},
        {"exchange", order.exchange},
        {"transaction_type", zerodha_side(order.side)},
        {"order_type", zerodha_order_type(order.order_type)},
        {"quantity", QString::number(static_cast<int>(order.quantity))},
        {"product", zerodha_product(order.product_type)},
        {"validity", order.validity.isEmpty() ? "DAY" : order.validity},
        {"disclosed_quantity", "0"},
        {"tag", "fincept"}, // tracking tag visible in Zerodha order history
    };
    if (order.price > 0)
        params["price"] = QString::number(order.price, 'f', 2);
    if (order.stop_price > 0)
        params["trigger_price"] = QString::number(order.stop_price, 'f', 2);

    auto resp =
        BrokerHttp::instance().post_form(QString("%1/orders/%2").arg(base_url(), variety), params, auth_headers(creds));

    OrderPlaceResponse result;
    if (!resp.success) {
        result.error = resp.error;
        return result;
    }
    if (resp.json.value("status").toString() == "success") {
        result.success = true;
        result.order_id = resp.json.value("data").toObject().value("order_id").toString();
    } else {
        result.error = resp.json.value("message").toString("Order placement failed");
    }
    return result;
}

ApiResponse<QJsonObject> ZerodhaBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                     const QJsonObject& mods) {
    // Zerodha modify uses form-urlencoded PUT to /orders/{variety}/{order_id}
    // Variety defaults to "regular"; caller can pass "variety" key in mods to override.
    QString variety = mods.value("variety").toString("regular");
    QMap<QString, QString> params;
    // Required fields — fall back to sensible defaults if not supplied
    if (mods.contains("order_type"))
        params["order_type"] = mods.value("order_type").toString();
    if (mods.contains("quantity"))
        params["quantity"] = mods.value("quantity").toString();
    if (mods.contains("price"))
        params["price"] = mods.value("price").toString();
    if (mods.contains("trigger_price"))
        params["trigger_price"] = mods.value("trigger_price").toString();
    if (mods.contains("disclosed_quantity"))
        params["disclosed_quantity"] = mods.value("disclosed_quantity").toString();
    params["validity"] = mods.value("validity").toString("DAY");

    auto resp = BrokerHttp::instance().put_form(QString("%1/orders/%2/%3").arg(base_url(), variety, order_id), params,
                                                auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("status").toString() == "success")
        return {true, resp.json, "", ts};
    return {false, std::nullopt, resp.json.value("message").toString("Modify failed"), ts};
}

ApiResponse<QJsonObject> ZerodhaBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    // Variety must match what was used when placing. Caller encodes it in
    // additional_data JSON as "variety"; default to "regular".
    QString variety = "regular";
    if (!creds.additional_data.isEmpty()) {
        auto ad = QJsonDocument::fromJson(creds.additional_data.toUtf8()).object();
        auto v = ad.value("variety").toString();
        if (!v.isEmpty())
            variety = v;
    }
    auto resp =
        BrokerHttp::instance().del(QString("%1/orders/%2/%3").arg(base_url(), variety, order_id), auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("status").toString() == "success")
        return {true, resp.json, "", ts};
    return {false, std::nullopt, resp.json.value("message").toString("Cancel failed"), ts};
}

ApiResponse<QVector<BrokerOrderInfo>> ZerodhaBroker::get_orders(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/orders", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    QVector<BrokerOrderInfo> orders;
    auto arr = resp.json.value("data").toArray();
    for (const auto& v : arr) {
        auto o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("order_id").toString();
        info.symbol = o.value("tradingsymbol").toString();
        info.exchange = o.value("exchange").toString();
        info.side = o.value("transaction_type").toString();
        info.order_type = o.value("order_type").toString();
        info.product_type = o.value("product").toString();
        info.quantity = o.value("quantity").toDouble();
        info.price = o.value("price").toDouble();
        info.trigger_price = o.value("trigger_price").toDouble();
        info.filled_qty = o.value("filled_quantity").toDouble();
        info.avg_price = o.value("average_price").toDouble();
        info.status = o.value("status").toString();
        info.timestamp = o.value("order_timestamp").toString();
        info.message = o.value("status_message").toString();
        orders.append(info);
    }
    return {true, orders, "", ts};
}

ApiResponse<QJsonObject> ZerodhaBroker::get_trade_book(const BrokerCredentials& creds) {
    return simple_get(QString(base_url()) + "/trades", auth_headers(creds));
}

ApiResponse<QVector<BrokerPosition>> ZerodhaBroker::get_positions(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/portfolio/positions", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    QVector<BrokerPosition> positions;
    // Zerodha returns both "net" and "day" arrays; we use "net" for current positions
    auto net = resp.json.value("data").toObject().value("net").toArray();
    for (const auto& v : net) {
        auto p = v.toObject();
        BrokerPosition pos;
        pos.symbol = p.value("tradingsymbol").toString();
        pos.exchange = p.value("exchange").toString();
        pos.product_type = p.value("product").toString();
        pos.quantity = p.value("quantity").toDouble();
        pos.avg_price = p.value("average_price").toDouble();
        pos.ltp = p.value("last_price").toDouble();
        pos.pnl = p.value("pnl").toDouble();
        pos.day_pnl = p.value("m2m_pnl").toDouble(); // mark-to-market day P&L
        pos.side = pos.quantity >= 0 ? "BUY" : "SELL";
        // pnl_pct: avoid divide-by-zero on zero-cost positions
        double cost = std::abs(pos.avg_price * pos.quantity);
        pos.pnl_pct = cost > 0 ? (pos.pnl / cost) * 100.0 : 0.0;
        positions.append(pos);
    }
    return {true, positions, "", ts};
}

ApiResponse<QVector<BrokerHolding>> ZerodhaBroker::get_holdings(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/portfolio/holdings", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    QVector<BrokerHolding> holdings;
    auto arr = resp.json.value("data").toArray();
    for (const auto& v : arr) {
        auto h = v.toObject();
        BrokerHolding hold;
        hold.symbol = h.value("tradingsymbol").toString();
        hold.exchange = h.value("exchange").toString();
        hold.quantity = h.value("quantity").toDouble();
        hold.avg_price = h.value("average_price").toDouble();
        hold.ltp = h.value("last_price").toDouble();
        hold.invested_value = hold.quantity * hold.avg_price;
        hold.current_value = hold.quantity * hold.ltp;
        // Use Zerodha's pre-calculated pnl field directly — more accurate than
        // on-the-fly calculation which ignores corporate actions and adjustments.
        hold.pnl = h.value("pnl").toDouble();
        hold.pnl_pct = hold.avg_price > 0 ? ((hold.ltp - hold.avg_price) / hold.avg_price) * 100.0 : 0.0;
        holdings.append(hold);
    }
    return {true, holdings, "", ts};
}

ApiResponse<BrokerFunds> ZerodhaBroker::get_funds(const BrokerCredentials& creds) {
    // Fetch all segments at once — NOT /user/margins/equity (that only returns equity)
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/user/margins", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    auto data = resp.json.value("data").toObject();
    auto equity = data.value("equity").toObject();
    auto commod = data.value("commodity").toObject();

    BrokerFunds funds;
    // "net" is the correct available margin field (not "live_balance")
    funds.available_balance = equity.value("net").toDouble() + commod.value("net").toDouble();
    funds.used_margin = equity.value("utilised").toObject().value("debits").toDouble() +
                        commod.value("utilised").toObject().value("debits").toDouble();
    funds.collateral = equity.value("available").toObject().value("collateral").toDouble() +
                       commod.value("available").toObject().value("collateral").toDouble();
    funds.total_balance = funds.available_balance + funds.used_margin;
    funds.raw_data = resp.json;
    return {true, funds, "", ts};
}

ApiResponse<QVector<BrokerQuote>> ZerodhaBroker::get_quotes(const BrokerCredentials& creds,
                                                            const QVector<QString>& symbols) {
    // Zerodha requires exchange-prefixed symbols: "NSE:RELIANCE", "BSE:INFY"
    // If the caller already passed "NSE:RELIANCE" we leave it; if bare "RELIANCE"
    // we prefix with the default exchange from profile.
    QString query;
    for (const auto& sym : symbols) {
        if (!query.isEmpty())
            query += "&i=";
        // If no colon present, prefix with NSE (most common default)
        query += sym.contains(':') ? sym : ("NSE:" + sym);
    }
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/quote?i=" + query, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    QVector<BrokerQuote> quotes;
    auto data = resp.json.value("data").toObject();
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        auto q_obj = it.value().toObject();
        BrokerQuote q;
        q.symbol = it.key(); // already in "NSE:RELIANCE" form
        q.ltp = q_obj.value("last_price").toDouble();
        auto ohlc = q_obj.value("ohlc").toObject();
        q.open = ohlc.value("open").toDouble();
        q.high = ohlc.value("high").toDouble();
        q.low = ohlc.value("low").toDouble();
        q.close = ohlc.value("close").toDouble();
        q.volume = q_obj.value("volume").toDouble();
        q.change = q_obj.value("net_change").toDouble();
        q.change_pct = q.close > 0 ? (q.change / q.close) * 100.0 : 0.0;
        // Best bid/ask from level-1 depth (first entry of buy/sell depth arrays)
        auto depth = q_obj.value("depth").toObject();
        auto buy0 = depth.value("buy").toArray().first().toObject();
        auto sell0 = depth.value("sell").toArray().first().toObject();
        q.bid = buy0.value("price").toDouble();
        q.bid_size = buy0.value("quantity").toDouble();
        q.ask = sell0.value("price").toDouble();
        q.ask_size = sell0.value("quantity").toDouble();
        quotes.append(q);
    }
    return {true, quotes, "", ts};
}

ApiResponse<QVector<BrokerCandle>> ZerodhaBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                              const QString& resolution, const QString& from_date,
                                                              const QString& to_date) {
    // Zerodha historical data requires the numeric instrument_token, not the trading symbol.
    // Resolution order:
    //   1. InstrumentService in-memory cache  (fast, populated after refresh)
    //   2. additional_data JSON fallback       (manual override)
    //   3. Fail with a clear error message
    QString token;

    // 1 — InstrumentService lookup (symbol + exchange → token)
    // Symbol may arrive as "NSE:RELIANCE" or plain "RELIANCE". Split on ':'.
    QString lookup_exchange = "NSE";
    QString lookup_symbol = symbol;
    if (symbol.contains(':')) {
        lookup_exchange = symbol.section(':', 0, 0);
        lookup_symbol = symbol.section(':', 1);
    }
    auto svc_token = InstrumentService::instance().instrument_token(lookup_symbol, lookup_exchange, "zerodha");
    if (svc_token.has_value()) {
        token = QString::number(svc_token.value());
    }

    // 2 — Fallback: caller-supplied token in additional_data JSON
    if (token.isEmpty() && !creds.additional_data.isEmpty()) {
        auto ad = QJsonDocument::fromJson(creds.additional_data.toUtf8()).object();
        token = ad.value("instrument_token").toString();
    }

    if (token.isEmpty()) {
        return {false, std::nullopt,
                "Zerodha get_history: instrument_token not found for " + symbol +
                    ". Call InstrumentService::refresh() first.",
                now_ts()};
    }

    QString interval = zerodha_interval(resolution);
    // continuous=0: no continuous contract stitching; oi=1: include open interest column
    QString url = QString("%1/instruments/historical/%2/%3?from=%4&to=%5&continuous=0&oi=1")
                      .arg(base_url(), token, interval, from_date, to_date);
    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    QVector<BrokerCandle> candles;
    auto arr = resp.json.value("data").toObject().value("candles").toArray();
    for (const auto& v : arr) {
        auto c = v.toArray();
        if (c.size() < 6)
            continue;
        // c[0] = "2024-01-15T09:15:00+0530" — Zerodha always includes the IST offset
        // in the timestamp string, so Qt::ISODate parses it correctly to UTC epoch.
        // No manual IST adjustment needed — Qt handles the offset.
        qint64 epoch = QDateTime::fromString(c[0].toString(), Qt::ISODateWithMs).toSecsSinceEpoch();
        if (epoch == 0) {
            // Fallback: some responses omit milliseconds
            epoch = QDateTime::fromString(c[0].toString(), Qt::ISODate).toSecsSinceEpoch();
        }
        // c[6] = open interest (present when oi=1; safe to ignore if absent)
        candles.append({epoch, c[1].toDouble(), c[2].toDouble(), c[3].toDouble(), c[4].toDouble(), c[5].toDouble()});
    }
    return {true, candles, "", ts};
}

// ═══════════════════════════════════════════════════════════════════════════════
// Remaining Indian brokers — Bearer token pattern (simpler)
// ═══════════════════════════════════════════════════════════════════════════════

// Stub implementations for remaining Indian brokers — auth_headers is unique per broker,
// all other methods return "not implemented" and will be filled as needed.

#define IMPL_STUB_METHODS(CLASS)                                                                                       \
    TokenExchangeResponse CLASS::exchange_token(const QString&, const QString&, const QString&) {                      \
        return {false, "", "", "", "Use OAuth flow"};                                                                  \
    }                                                                                                                  \
    OrderPlaceResponse CLASS::place_order(const BrokerCredentials&, const UnifiedOrder&) {                             \
        return {false, "", QString("%1: TODO").arg(name())};                                                           \
    }                                                                                                                  \
    ApiResponse<QJsonObject> CLASS::modify_order(const BrokerCredentials&, const QString&, const QJsonObject&) {       \
        return {false, std::nullopt, "TODO", now_ts()};                                                                \
    }                                                                                                                  \
    ApiResponse<QJsonObject> CLASS::cancel_order(const BrokerCredentials&, const QString&) {                           \
        return {false, std::nullopt, "TODO", now_ts()};                                                                \
    }                                                                                                                  \
    ApiResponse<QVector<BrokerOrderInfo>> CLASS::get_orders(const BrokerCredentials&) {                                \
        return {false, std::nullopt, "TODO", now_ts()};                                                                \
    }                                                                                                                  \
    ApiResponse<QJsonObject> CLASS::get_trade_book(const BrokerCredentials&) {                                         \
        return {false, std::nullopt, "TODO", now_ts()};                                                                \
    }                                                                                                                  \
    ApiResponse<QVector<BrokerPosition>> CLASS::get_positions(const BrokerCredentials&) {                              \
        return {false, std::nullopt, "TODO", now_ts()};                                                                \
    }                                                                                                                  \
    ApiResponse<QVector<BrokerHolding>> CLASS::get_holdings(const BrokerCredentials&) {                                \
        return {false, std::nullopt, "TODO", now_ts()};                                                                \
    }                                                                                                                  \
    ApiResponse<BrokerFunds> CLASS::get_funds(const BrokerCredentials&) {                                              \
        return {false, std::nullopt, "TODO", now_ts()};                                                                \
    }                                                                                                                  \
    ApiResponse<QVector<BrokerQuote>> CLASS::get_quotes(const BrokerCredentials&, const QVector<QString>&) {           \
        return {false, std::nullopt, "TODO", now_ts()};                                                                \
    }                                                                                                                  \
    ApiResponse<QVector<BrokerCandle>> CLASS::get_history(const BrokerCredentials&, const QString&, const QString&,    \
                                                          const QString&, const QString&) {                            \
        return {false, std::nullopt, "TODO", now_ts()};                                                                \
    }

// ── Upstox ──
QMap<QString, QString> UpstoxBroker::auth_headers(const BrokerCredentials& creds) const {
    QMap<QString, QString> h;
    h["Authorization"] = "Bearer " + creds.access_token;
    h["Content-Type"] = "application/json";
    h["Accept"] = "application/json";
    return h;
}
IMPL_STUB_METHODS(UpstoxBroker)

// ── Dhan ──
QMap<QString, QString> DhanBroker::auth_headers(const BrokerCredentials& creds) const {
    QMap<QString, QString> h;
    h["access-token"] = creds.access_token;
    h["Content-Type"] = "application/json";
    h["Accept"] = "application/json";
    return h;
}
IMPL_STUB_METHODS(DhanBroker)

// ── Kotak ──
QMap<QString, QString> KotakBroker::auth_headers(const BrokerCredentials& creds) const {
    QMap<QString, QString> h;
    h["Authorization"] = "Bearer " + creds.access_token;
    h["Content-Type"] = "application/json";
    h["Accept"] = "application/json";
    return h;
}
IMPL_STUB_METHODS(KotakBroker)

// Groww is implemented in src/trading/brokers/groww/GrowwBroker.cpp (full, non-stub).

// ── AliceBlue ──
QMap<QString, QString> AliceBlueBroker::auth_headers(const BrokerCredentials& creds) const {
    QMap<QString, QString> h;
    h["Authorization"] = "Bearer " + creds.api_secret + " " + creds.access_token;
    h["Content-Type"] = "application/json";
    h["Accept"] = "application/json";
    return h;
}
IMPL_STUB_METHODS(AliceBlueBroker)

// ── 5Paisa ──
QMap<QString, QString> FivePaisaBroker::auth_headers(const BrokerCredentials& creds) const {
    QMap<QString, QString> h;
    h["Authorization"] = "bearer " + creds.access_token;
    h["Content-Type"] = "application/json";
    return h;
}
IMPL_STUB_METHODS(FivePaisaBroker)

// ── IIFL ──
QMap<QString, QString> IIFLBroker::auth_headers(const BrokerCredentials& creds) const {
    QMap<QString, QString> h;
    h["Authorization"] = creds.access_token;
    h["Content-Type"] = "application/json";
    h["Accept"] = "application/json";
    return h;
}
IMPL_STUB_METHODS(IIFLBroker)

// ── Motilal ──
QMap<QString, QString> MotilalBroker::auth_headers(const BrokerCredentials& creds) const {
    QMap<QString, QString> h;
    h["Authorization"] = creds.access_token;
    h["Content-Type"] = "application/json";
    h["Accept"] = "application/json";
    h["ApiKey"] = creds.api_key;
    return h;
}
IMPL_STUB_METHODS(MotilalBroker)

// ── Shoonya ──
QMap<QString, QString> ShoonyaBroker::auth_headers(const BrokerCredentials&) const {
    QMap<QString, QString> h;
    h["Content-Type"] = "application/x-www-form-urlencoded";
    return h;
}
IMPL_STUB_METHODS(ShoonyaBroker)

#undef IMPL_STUB_METHODS

} // namespace fincept::trading
