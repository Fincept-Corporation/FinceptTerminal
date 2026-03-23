// Indian Brokers — implementation for Zerodha, AngelOne, Upstox, Dhan, Kotak, Groww,
// AliceBlue, 5Paisa, IIFL, Motilal, Shoonya
// Each broker implements auth_headers + exchange_token + 10 trading API methods.

#include "core/logging/Logger.h"
#include "trading/brokers/AliceBlueBroker.h"
#include "trading/brokers/AngelOneBroker.h"
#include "trading/brokers/BrokerHttp.h"
#include "trading/brokers/DhanBroker.h"
#include "trading/brokers/FivePaisaBroker.h"
#include "trading/brokers/GrowwBroker.h"
#include "trading/brokers/IIFLBroker.h"
#include "trading/brokers/KotakBroker.h"
#include "trading/brokers/MotilalBroker.h"
#include "trading/brokers/ShoonyaBroker.h"
#include "trading/brokers/UpstoxBroker.h"
#include "trading/brokers/ZerodhaBroker.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>

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

QMap<QString, QString> ZerodhaBroker::auth_headers(const BrokerCredentials& creds) const {
    return {{"Authorization", "token " + creds.api_key + ":" + creds.access_token},
            {"X-Kite-Version", kite_api_version},
            {"Content-Type", "application/x-www-form-urlencoded"}};
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

TokenExchangeResponse ZerodhaBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                    const QString& request_token) {
    QByteArray input = (api_key + request_token + api_secret).toUtf8();
    QString checksum = QCryptographicHash::hash(input, QCryptographicHash::Sha256).toHex();

    QMap<QString, QString> params = {{"api_key", api_key}, {"request_token", request_token}, {"checksum", checksum}};
    auto resp = BrokerHttp::instance().post_form(QString(base_url()) + "/session/token", params);

    TokenExchangeResponse result;
    if (!resp.success) {
        result.error = resp.error;
        return result;
    }
    if (resp.json.value("status").toString() == "success") {
        auto data = resp.json.value("data").toObject();
        result.success = true;
        result.access_token = data.value("access_token").toString();
        result.user_id = data.value("user_id").toString();
    } else {
        result.error = resp.json.value("message").toString("Token exchange failed");
    }
    return result;
}

OrderPlaceResponse ZerodhaBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    QString variety = zerodha_variety(order.product_type);
    QMap<QString, QString> params = {{"tradingsymbol", order.symbol},
                                     {"exchange", order.exchange},
                                     {"transaction_type", zerodha_side(order.side)},
                                     {"order_type", zerodha_order_type(order.order_type)},
                                     {"quantity", QString::number(static_cast<int>(order.quantity))},
                                     {"product", zerodha_product(order.product_type)},
                                     {"validity", order.validity}};
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
        result.error = resp.json.value("message").toString("Order failed");
    }
    return result;
}

ApiResponse<QJsonObject> ZerodhaBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                     const QJsonObject& mods) {
    Q_UNUSED(creds);
    Q_UNUSED(order_id);
    Q_UNUSED(mods);
    return {false, std::nullopt, "Zerodha modify: use form PUT (TODO)", now_ts()};
}

ApiResponse<QJsonObject> ZerodhaBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    auto resp =
        BrokerHttp::instance().del(QString("%1/orders/regular/%2").arg(base_url(), order_id), auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    return {true, resp.json, "", ts};
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
        pos.side = pos.quantity >= 0 ? "buy" : "sell";
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
        hold.pnl = hold.current_value - hold.invested_value;
        hold.pnl_pct = hold.invested_value > 0 ? (hold.pnl / hold.invested_value) * 100 : 0;
        holdings.append(hold);
    }
    return {true, holdings, "", ts};
}
ApiResponse<BrokerFunds> ZerodhaBroker::get_funds(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/user/margins/equity", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    auto data = resp.json.value("data").toObject();
    BrokerFunds funds;
    funds.available_balance = data.value("available").toObject().value("live_balance").toDouble();
    funds.used_margin = data.value("utilised").toObject().value("debits").toDouble();
    funds.total_balance = funds.available_balance + funds.used_margin;
    funds.raw_data = resp.json;
    return {true, funds, "", ts};
}
ApiResponse<QVector<BrokerQuote>> ZerodhaBroker::get_quotes(const BrokerCredentials& creds,
                                                            const QVector<QString>& symbols) {
    QString syms;
    for (int i = 0; i < symbols.size(); ++i) {
        if (i > 0)
            syms += "&i=";
        syms += symbols[i];
    }
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/quote?i=" + syms, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    QVector<BrokerQuote> quotes;
    auto data = resp.json.value("data").toObject();
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        auto q_obj = it.value().toObject();
        BrokerQuote q;
        q.symbol = it.key();
        q.ltp = q_obj.value("last_price").toDouble();
        auto ohlc = q_obj.value("ohlc").toObject();
        q.open = ohlc.value("open").toDouble();
        q.high = ohlc.value("high").toDouble();
        q.low = ohlc.value("low").toDouble();
        q.close = ohlc.value("close").toDouble();
        q.volume = q_obj.value("volume").toDouble();
        q.change = q_obj.value("net_change").toDouble();
        quotes.append(q);
    }
    return {true, quotes, "", ts};
}
ApiResponse<QVector<BrokerCandle>> ZerodhaBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                              const QString& resolution, const QString& from_date,
                                                              const QString& to_date) {
    QString url = QString("%1/instruments/historical/%2/%3?from=%4&to=%5")
                      .arg(base_url(), symbol, resolution, from_date, to_date);
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
        candles.append({c[0].toVariant().toLongLong(), c[1].toDouble(), c[2].toDouble(), c[3].toDouble(),
                        c[4].toDouble(), c[5].toDouble()});
    }
    return {true, candles, "", ts};
}

// ═══════════════════════════════════════════════════════════════════════════════
// ANGELONE — Bearer token + custom headers, instrument token resolution
// ═══════════════════════════════════════════════════════════════════════════════

QMap<QString, QString> AngelOneBroker::auth_headers(const BrokerCredentials& creds) const {
    return {{"Authorization", "Bearer " + creds.access_token},
            {"Content-Type", "application/json"},
            {"Accept", "application/json"},
            {"X-UserType", "USER"},
            {"X-SourceID", "WEB"},
            {"X-ClientLocalIP", "127.0.0.1"},
            {"X-ClientPublicIP", "127.0.0.1"},
            {"X-MACAddress", "00:00:00:00:00:00"},
            {"X-PrivateKey", creds.api_key}};
}
TokenExchangeResponse AngelOneBroker::exchange_token(const QString& api_key, const QString&, const QString& auth_code) {
    QJsonObject payload{{"clientcode", api_key}, {"password", auth_code}};
    auto resp = BrokerHttp::instance().post_json(
        QString(base_url()) + "/rest/auth/angelbroking/user/v1/loginByPassword", payload);
    TokenExchangeResponse result;
    if (!resp.success) {
        result.error = resp.error;
        return result;
    }
    auto data = resp.json.value("data").toObject();
    if (!data.isEmpty()) {
        result.success = true;
        result.access_token = data.value("jwtToken").toString();
        result.user_id = api_key;
        result.additional_data = data.value("feedToken").toString();
    } else {
        result.error = resp.json.value("message").toString("Login failed");
    }
    return result;
}
OrderPlaceResponse AngelOneBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    QJsonObject payload{{"variety", "NORMAL"},
                        {"tradingsymbol", order.symbol},
                        {"symboltoken", ""},
                        {"transactiontype", order.side == OrderSide::Buy ? "BUY" : "SELL"},
                        {"exchange", order.exchange.isEmpty() ? "NSE" : order.exchange},
                        {"ordertype", order.order_type == OrderType::Market ? "MARKET" : "LIMIT"},
                        {"producttype", order.product_type == ProductType::Intraday ? "INTRADAY" : "DELIVERY"},
                        {"duration", "DAY"},
                        {"quantity", QString::number(static_cast<int>(order.quantity))}};
    if (order.price > 0)
        payload["price"] = QString::number(order.price, 'f', 2);
    auto resp = BrokerHttp::instance().post_json(QString(base_url()) + "/rest/secure/angelbroking/order/v1/placeOrder",
                                                 payload, auth_headers(creds));
    OrderPlaceResponse result;
    if (!resp.success) {
        result.error = resp.error;
        return result;
    }
    auto data = resp.json.value("data").toObject();
    if (!data.isEmpty()) {
        result.success = true;
        result.order_id = data.value("orderid").toString();
    } else {
        result.error = resp.json.value("message").toString("Order failed");
    }
    return result;
}
ApiResponse<QJsonObject> AngelOneBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                      const QJsonObject& mods) {
    Q_UNUSED(creds);
    Q_UNUSED(order_id);
    Q_UNUSED(mods);
    return {false, std::nullopt, "Not yet implemented", now_ts()};
}
ApiResponse<QJsonObject> AngelOneBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    QJsonObject payload{{"variety", "NORMAL"}, {"orderid", order_id}};
    auto resp = BrokerHttp::instance().post_json(QString(base_url()) + "/rest/secure/angelbroking/order/v1/cancelOrder",
                                                 payload, auth_headers(creds));
    int64_t ts = now_ts();
    return resp.success ? ApiResponse<QJsonObject>{true, resp.json, "", ts}
                        : ApiResponse<QJsonObject>{false, std::nullopt, resp.error, ts};
}
ApiResponse<QVector<BrokerOrderInfo>> AngelOneBroker::get_orders(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/rest/secure/angelbroking/order/v1/getOrderBook",
                                           auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    QVector<BrokerOrderInfo> orders;
    auto arr = resp.json.value("data").toArray();
    for (const auto& v : arr) {
        auto o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("orderid").toString();
        info.symbol = o.value("tradingsymbol").toString();
        info.exchange = o.value("exchange").toString();
        info.side = o.value("transactiontype").toString();
        info.order_type = o.value("ordertype").toString();
        info.quantity = o.value("quantity").toDouble();
        info.price = o.value("price").toDouble();
        info.filled_qty = o.value("filledshares").toDouble();
        info.avg_price = o.value("averageprice").toDouble();
        info.status = o.value("orderstatus").toString();
        orders.append(info);
    }
    return {true, orders, "", ts};
}
ApiResponse<QJsonObject> AngelOneBroker::get_trade_book(const BrokerCredentials& creds) {
    return simple_get(QString(base_url()) + "/rest/secure/angelbroking/order/v1/getTradeBook", auth_headers(creds));
}
ApiResponse<QVector<BrokerPosition>> AngelOneBroker::get_positions(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/rest/secure/angelbroking/order/v1/getPosition",
                                           auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    QVector<BrokerPosition> positions;
    auto arr = resp.json.value("data").toArray();
    for (const auto& v : arr) {
        auto p = v.toObject();
        BrokerPosition pos;
        pos.symbol = p.value("tradingsymbol").toString();
        pos.exchange = p.value("exchange").toString();
        pos.quantity = p.value("netqty").toDouble();
        pos.avg_price = p.value("netprice").toDouble();
        pos.ltp = p.value("ltp").toDouble();
        pos.pnl = p.value("pnl").toDouble();
        pos.side = pos.quantity >= 0 ? "buy" : "sell";
        positions.append(pos);
    }
    return {true, positions, "", ts};
}
ApiResponse<QVector<BrokerHolding>> AngelOneBroker::get_holdings(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/rest/secure/angelbroking/portfolio/v1/getHolding",
                                           auth_headers(creds));
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
        hold.avg_price = h.value("averageprice").toDouble();
        hold.ltp = h.value("ltp").toDouble();
        hold.invested_value = hold.quantity * hold.avg_price;
        hold.current_value = hold.quantity * hold.ltp;
        hold.pnl = hold.current_value - hold.invested_value;
        hold.pnl_pct = hold.invested_value > 0 ? (hold.pnl / hold.invested_value) * 100 : 0;
        holdings.append(hold);
    }
    return {true, holdings, "", ts};
}
ApiResponse<BrokerFunds> AngelOneBroker::get_funds(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/rest/secure/angelbroking/user/v1/getRMS",
                                           auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    auto data = resp.json.value("data").toObject();
    BrokerFunds funds;
    funds.available_balance = data.value("net").toDouble();
    funds.used_margin = data.value("utiliseddebits").toDouble();
    funds.total_balance = funds.available_balance + funds.used_margin;
    funds.raw_data = resp.json;
    return {true, funds, "", ts};
}
ApiResponse<QVector<BrokerQuote>> AngelOneBroker::get_quotes(const BrokerCredentials& creds,
                                                             const QVector<QString>& symbols) {
    Q_UNUSED(creds);
    Q_UNUSED(symbols);
    return {false, std::nullopt, "Use LTP API", now_ts()};
}
ApiResponse<QVector<BrokerCandle>> AngelOneBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                               const QString& resolution, const QString& from_date,
                                                               const QString& to_date) {
    Q_UNUSED(creds);
    Q_UNUSED(symbol);
    Q_UNUSED(resolution);
    Q_UNUSED(from_date);
    Q_UNUSED(to_date);
    return {false, std::nullopt, "Use candle API", now_ts()};
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

// ── Groww ──
QMap<QString, QString> GrowwBroker::auth_headers(const BrokerCredentials& creds) const {
    QMap<QString, QString> h;
    h["Authorization"] = "Bearer " + creds.access_token;
    h["Content-Type"] = "application/json";
    h["Accept"] = "application/json";
    return h;
}
IMPL_STUB_METHODS(GrowwBroker)

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
