// Global Brokers — Alpaca (US), IBKR (US), Tradier (US), SaxoBank (EU)

#include "trading/brokers/AlpacaBroker.h"
#include "trading/brokers/IBKRBroker.h"
#include "trading/brokers/TradierBroker.h"
#include "trading/brokers/SaxoBankBroker.h"
#include "trading/brokers/BrokerHttp.h"
#include "core/logging/Logger.h"
#include <QJsonArray>
#include <QDateTime>

namespace fincept::trading {

static int64_t now_ts() { return QDateTime::currentSecsSinceEpoch(); }

// ═══════════════════════════════════════════════════════════════════════════════
// ALPACA — US broker, API key/secret in headers (no OAuth)
// ═══════════════════════════════════════════════════════════════════════════════

QMap<QString, QString> AlpacaBroker::auth_headers(const BrokerCredentials& creds) const {
    return {{"APCA-API-KEY-ID", creds.api_key}, {"APCA-API-SECRET-KEY", creds.api_secret},
            {"Content-Type", "application/json"}, {"Accept", "application/json"}};
}

TokenExchangeResponse AlpacaBroker::exchange_token(const QString& api_key, const QString& api_secret, const QString&) {
    QMap<QString, QString> headers = {{"APCA-API-KEY-ID", api_key}, {"APCA-API-SECRET-KEY", api_secret}, {"Accept", "application/json"}};
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/v2/account", headers);
    TokenExchangeResponse result;
    if (!resp.success) { result.error = resp.error; return result; }
    result.success = true; result.access_token = api_secret; result.user_id = resp.json.value("id").toString();
    return result;
}

OrderPlaceResponse AlpacaBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    static auto alpaca_type = [](OrderType t) -> const char* {
        switch (t) { case OrderType::Market: return "market"; case OrderType::Limit: return "limit";
                     case OrderType::StopLoss: return "stop"; case OrderType::StopLossLimit: return "stop_limit"; } return "market";
    };
    QJsonObject payload{{"symbol", order.symbol}, {"qty", QString::number(order.quantity)},
        {"side", order.side == OrderSide::Buy ? "buy" : "sell"}, {"type", alpaca_type(order.order_type)}, {"time_in_force", "day"}};
    if (order.price > 0) payload["limit_price"] = QString::number(order.price, 'f', 2);
    if (order.stop_price > 0) payload["stop_price"] = QString::number(order.stop_price, 'f', 2);

    auto resp = BrokerHttp::instance().post_json(QString(base_url()) + "/v2/orders", payload, auth_headers(creds));
    OrderPlaceResponse result;
    if (!resp.success) { result.error = resp.error; return result; }
    result.success = true; result.order_id = resp.json.value("id").toString();
    return result;
}

ApiResponse<QJsonObject> AlpacaBroker::modify_order(const BrokerCredentials& creds, const QString& order_id, const QJsonObject& mods) {
    auto resp = BrokerHttp::instance().put_json(QString(base_url()) + "/v2/orders/" + order_id, mods, auth_headers(creds));
    int64_t ts = now_ts(); return resp.success ? ApiResponse<QJsonObject>{true, resp.json, "", ts} : ApiResponse<QJsonObject>{false, std::nullopt, resp.error, ts};
}

ApiResponse<QJsonObject> AlpacaBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    auto resp = BrokerHttp::instance().del(QString(base_url()) + "/v2/orders/" + order_id, auth_headers(creds));
    int64_t ts = now_ts(); return resp.success ? ApiResponse<QJsonObject>{true, resp.json, "", ts} : ApiResponse<QJsonObject>{false, std::nullopt, resp.error, ts};
}

ApiResponse<QVector<BrokerOrderInfo>> AlpacaBroker::get_orders(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/v2/orders?status=all&limit=100", auth_headers(creds));
    int64_t ts = now_ts(); if (!resp.success) return {false, std::nullopt, resp.error, ts};
    QVector<BrokerOrderInfo> orders;
    // Alpaca returns array at root
    QJsonParseError err; auto doc = QJsonDocument::fromJson(resp.raw_body.toUtf8(), &err);
    if (doc.isArray()) { for (const auto& v : doc.array()) { auto o = v.toObject(); BrokerOrderInfo info;
        info.order_id = o.value("id").toString(); info.symbol = o.value("symbol").toString();
        info.side = o.value("side").toString(); info.order_type = o.value("type").toString();
        info.quantity = o.value("qty").toString().toDouble(); info.price = o.value("limit_price").toString().toDouble();
        info.filled_qty = o.value("filled_qty").toString().toDouble(); info.avg_price = o.value("filled_avg_price").toString().toDouble();
        info.status = o.value("status").toString(); info.timestamp = o.value("submitted_at").toString();
        orders.append(info);
    }}
    return {true, orders, "", ts};
}

ApiResponse<QJsonObject> AlpacaBroker::get_trade_book(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/v2/account/activities/FILL", auth_headers(creds));
    int64_t ts = now_ts(); return resp.success ? ApiResponse<QJsonObject>{true, resp.json, "", ts} : ApiResponse<QJsonObject>{false, std::nullopt, resp.error, ts};
}

ApiResponse<QVector<BrokerPosition>> AlpacaBroker::get_positions(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/v2/positions", auth_headers(creds));
    int64_t ts = now_ts(); if (!resp.success) return {false, std::nullopt, resp.error, ts};
    QVector<BrokerPosition> positions;
    QJsonParseError err; auto doc = QJsonDocument::fromJson(resp.raw_body.toUtf8(), &err);
    if (doc.isArray()) { for (const auto& v : doc.array()) { auto p = v.toObject(); BrokerPosition pos;
        pos.symbol = p.value("symbol").toString(); pos.quantity = p.value("qty").toString().toDouble();
        pos.avg_price = p.value("avg_entry_price").toString().toDouble();
        pos.ltp = p.value("current_price").toString().toDouble();
        pos.pnl = p.value("unrealized_pl").toString().toDouble();
        pos.side = p.value("side").toString(); positions.append(pos);
    }}
    return {true, positions, "", ts};
}

ApiResponse<QVector<BrokerHolding>> AlpacaBroker::get_holdings(const BrokerCredentials& creds) {
    // Alpaca: positions ARE holdings for stocks
    auto pos_result = get_positions(creds);
    if (!pos_result.success) return {false, std::nullopt, pos_result.error, pos_result.timestamp};
    QVector<BrokerHolding> holdings;
    for (const auto& p : *pos_result.data) { BrokerHolding h;
        h.symbol = p.symbol; h.quantity = std::abs(p.quantity); h.avg_price = p.avg_price;
        h.ltp = p.ltp; h.invested_value = h.quantity * h.avg_price; h.current_value = h.quantity * h.ltp;
        h.pnl = h.current_value - h.invested_value;
        h.pnl_pct = h.invested_value > 0 ? (h.pnl / h.invested_value) * 100 : 0;
        holdings.append(h);
    }
    return {true, holdings, "", pos_result.timestamp};
}

ApiResponse<BrokerFunds> AlpacaBroker::get_funds(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/v2/account", auth_headers(creds));
    int64_t ts = now_ts(); if (!resp.success) return {false, std::nullopt, resp.error, ts};
    BrokerFunds funds;
    funds.total_balance = resp.json.value("equity").toString().toDouble();
    funds.available_balance = resp.json.value("buying_power").toString().toDouble();
    funds.used_margin = funds.total_balance - funds.available_balance;
    funds.raw_data = resp.json;
    return {true, funds, "", ts};
}

ApiResponse<QVector<BrokerQuote>> AlpacaBroker::get_quotes(const BrokerCredentials& creds, const QVector<QString>& symbols) {
    Q_UNUSED(creds); Q_UNUSED(symbols); return {false, std::nullopt, "Use data API for quotes", now_ts()};
}

ApiResponse<QVector<BrokerCandle>> AlpacaBroker::get_history(const BrokerCredentials& creds, const QString& symbol, const QString& resolution, const QString& from_date, const QString& to_date) {
    Q_UNUSED(creds); Q_UNUSED(symbol); Q_UNUSED(resolution); Q_UNUSED(from_date); Q_UNUSED(to_date);
    return {false, std::nullopt, "Use data API for bars", now_ts()};
}

// ═══════════════════════════════════════════════════════════════════════════════
// IBKR, TRADIER, SAXOBANK — Bearer token pattern
// ═══════════════════════════════════════════════════════════════════════════════

#define IMPL_STUB_METHODS(CLASS) \
TokenExchangeResponse CLASS::exchange_token(const QString&, const QString&, const QString&) { return {false, "", "", "", "Use OAuth flow"}; } \
OrderPlaceResponse CLASS::place_order(const BrokerCredentials&, const UnifiedOrder&) { return {false, "", "TODO"}; } \
ApiResponse<QJsonObject> CLASS::modify_order(const BrokerCredentials&, const QString&, const QJsonObject&) { return {false, std::nullopt, "TODO", now_ts()}; } \
ApiResponse<QJsonObject> CLASS::cancel_order(const BrokerCredentials&, const QString&) { return {false, std::nullopt, "TODO", now_ts()}; } \
ApiResponse<QVector<BrokerOrderInfo>> CLASS::get_orders(const BrokerCredentials&) { return {false, std::nullopt, "TODO", now_ts()}; } \
ApiResponse<QJsonObject> CLASS::get_trade_book(const BrokerCredentials&) { return {false, std::nullopt, "TODO", now_ts()}; } \
ApiResponse<QVector<BrokerPosition>> CLASS::get_positions(const BrokerCredentials&) { return {false, std::nullopt, "TODO", now_ts()}; } \
ApiResponse<QVector<BrokerHolding>> CLASS::get_holdings(const BrokerCredentials&) { return {false, std::nullopt, "TODO", now_ts()}; } \
ApiResponse<BrokerFunds> CLASS::get_funds(const BrokerCredentials&) { return {false, std::nullopt, "TODO", now_ts()}; } \
ApiResponse<QVector<BrokerQuote>> CLASS::get_quotes(const BrokerCredentials&, const QVector<QString>&) { return {false, std::nullopt, "TODO", now_ts()}; } \
ApiResponse<QVector<BrokerCandle>> CLASS::get_history(const BrokerCredentials&, const QString&, const QString&, const QString&, const QString&) { return {false, std::nullopt, "TODO", now_ts()}; }

// ── IBKR ──
QMap<QString, QString> IBKRBroker::auth_headers(const BrokerCredentials& creds) const {
    QMap<QString, QString> h; h["Authorization"] = "Bearer " + creds.access_token; h["Content-Type"] = "application/json"; h["Accept"] = "application/json"; return h;
}
IMPL_STUB_METHODS(IBKRBroker)

// ── Tradier ──
QMap<QString, QString> TradierBroker::auth_headers(const BrokerCredentials& creds) const {
    QMap<QString, QString> h; h["Authorization"] = "Bearer " + creds.access_token; h["Accept"] = "application/json"; return h;
}
IMPL_STUB_METHODS(TradierBroker)

// ── SaxoBank ──
QMap<QString, QString> SaxoBankBroker::auth_headers(const BrokerCredentials& creds) const {
    QMap<QString, QString> h; h["Authorization"] = "Bearer " + creds.access_token; h["Content-Type"] = "application/json"; h["Accept"] = "application/json"; return h;
}
IMPL_STUB_METHODS(SaxoBankBroker)

#undef IMPL_STUB_METHODS

} // namespace fincept::trading
