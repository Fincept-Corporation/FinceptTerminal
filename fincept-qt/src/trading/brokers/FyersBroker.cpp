// Fyers Broker — Qt-adapted port

#include "trading/brokers/FyersBroker.h"

#include "core/logging/Logger.h"
#include "trading/brokers/BrokerHttp.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>

namespace fincept::trading {

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// ============================================================================
// Auth
// ============================================================================

QMap<QString, QString> FyersBroker::auth_headers(const BrokerCredentials& creds) const {
    return {{"Authorization", creds.api_key + ":" + creds.access_token},
            {"Content-Type", "application/json"},
            {"Accept", "application/json"}};
}

TokenExchangeResponse FyersBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                  const QString& auth_code) {
    QByteArray input = (api_key + ":" + api_secret).toUtf8();
    QString hash = QCryptographicHash::hash(input, QCryptographicHash::Sha256).toHex();

    QJsonObject payload{{"grant_type", "authorization_code"}, {"appIdHash", hash}, {"code", auth_code}};

    auto resp = BrokerHttp::instance().post_json(QString(base_url()) + "/api/v3/validate-authcode", payload);

    TokenExchangeResponse result;
    if (!resp.success) {
        result.error = resp.error;
        return result;
    }

    if (resp.json.value("s").toString() == "ok") {
        result.success = true;
        result.access_token = resp.json.value("access_token").toString();
        result.user_id = resp.json.value("user_id").toString();
    } else {
        result.error = resp.json.value("message").toString("Token exchange failed");
    }
    return result;
}

// ============================================================================
// Order Type Mappings
// ============================================================================

int FyersBroker::fyers_order_type(OrderType t) {
    switch (t) {
        case OrderType::Limit:
            return 1;
        case OrderType::Market:
            return 2;
        case OrderType::StopLoss:
            return 3;
        case OrderType::StopLossLimit:
            return 4;
    }
    return 2;
}

int FyersBroker::fyers_side(OrderSide s) {
    return s == OrderSide::Buy ? 1 : -1;
}

QString FyersBroker::fyers_product(ProductType p) {
    switch (p) {
        case ProductType::Intraday:
            return "INTRADAY";
        case ProductType::Delivery:
            return "CNC";
        case ProductType::Margin:
            return "MARGIN";
        case ProductType::CoverOrder:
            return "CO";
        case ProductType::BracketOrder:
            return "BO";
    }
    return "INTRADAY";
}

// ============================================================================
// Place Order
// ============================================================================

OrderPlaceResponse FyersBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    QJsonObject payload{{"symbol", order.symbol},
                        {"qty", static_cast<int>(order.quantity)},
                        {"type", fyers_order_type(order.order_type)},
                        {"side", fyers_side(order.side)},
                        {"productType", fyers_product(order.product_type)},
                        {"validity", order.validity},
                        {"offlineOrder", false}};

    if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit)
        payload["limitPrice"] = order.price;
    if (order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit)
        payload["stopPrice"] = order.stop_price;
    if (order.stop_loss > 0)
        payload["stopLoss"] = order.stop_loss;
    if (order.take_profit > 0)
        payload["takeProfit"] = order.take_profit;

    auto resp = BrokerHttp::instance().post_json(QString(base_url()) + "/api/v3/orders", payload, auth_headers(creds));

    OrderPlaceResponse result;
    if (!resp.success) {
        result.error = resp.error;
        return result;
    }

    if (resp.json.value("s").toString() == "ok") {
        result.success = true;
        result.order_id = resp.json.value("id").toString();
    } else {
        result.error = resp.json.value("message").toString("Order placement failed");
    }
    return result;
}

// ============================================================================
// Modify / Cancel
// ============================================================================

ApiResponse<QJsonObject> FyersBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                   const QJsonObject& modifications) {
    QJsonObject payload = modifications;
    payload["id"] = order_id;
    auto resp = BrokerHttp::instance().put_json(QString(base_url()) + "/api/v3/orders", payload, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() == "ok")
        return {true, resp.json, "", ts};
    return {false, std::nullopt, resp.json.value("message").toString("Modify failed"), ts};
}

ApiResponse<QJsonObject> FyersBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    QJsonObject payload{{"id", order_id}};
    auto resp = BrokerHttp::instance().del(QString(base_url()) + "/api/v3/orders", auth_headers(creds), payload);
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() == "ok")
        return {true, resp.json, "", ts};
    return {false, std::nullopt, resp.json.value("message").toString("Cancel failed"), ts};
}

// ============================================================================
// Orders / Trades / Positions / Holdings / Funds / Quotes / History
// ============================================================================

ApiResponse<QVector<BrokerOrderInfo>> FyersBroker::get_orders(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/api/v3/orders", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("Failed"), ts};

    QVector<BrokerOrderInfo> orders;
    auto arr = resp.json.value("orderBook").toArray();
    for (const auto& v : arr) {
        auto o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("id").toString();
        info.symbol = o.value("symbol").toString();
        info.exchange = o.value("exchange").toString();
        info.side = o.value("side").toInt() == 1 ? "buy" : "sell";
        info.order_type = QString::number(o.value("type").toInt());
        info.product_type = o.value("productType").toString();
        info.quantity = o.value("qty").toDouble();
        info.price = o.value("limitPrice").toDouble();
        info.trigger_price = o.value("stopPrice").toDouble();
        info.filled_qty = o.value("filledQty").toDouble();
        info.avg_price = o.value("tradedPrice").toDouble();
        info.status = QString::number(o.value("status").toInt());
        info.message = o.value("message").toString();
        orders.append(info);
    }
    return {true, orders, "", ts};
}

ApiResponse<QJsonObject> FyersBroker::get_trade_book(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/api/v3/tradebook", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() == "ok")
        return {true, resp.json, "", ts};
    return {false, std::nullopt, resp.json.value("message").toString("Failed"), ts};
}

ApiResponse<QVector<BrokerPosition>> FyersBroker::get_positions(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/api/v3/positions", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("Failed"), ts};

    QVector<BrokerPosition> positions;
    auto arr = resp.json.value("netPositions").toArray();
    for (const auto& v : arr) {
        auto p = v.toObject();
        BrokerPosition pos;
        pos.symbol = p.value("symbol").toString();
        pos.exchange = p.value("exchange").toString();
        pos.product_type = p.value("productType").toString();
        pos.quantity = p.value("netQty").toDouble();
        pos.avg_price = p.value("netAvg").toDouble();
        pos.ltp = p.value("ltp").toDouble();
        pos.pnl = p.value("pl").toDouble();
        pos.side = pos.quantity >= 0 ? "buy" : "sell";
        positions.append(pos);
    }
    return {true, positions, "", ts};
}

ApiResponse<QVector<BrokerHolding>> FyersBroker::get_holdings(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/api/v3/holdings", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("Failed"), ts};

    QVector<BrokerHolding> holdings;
    auto arr = resp.json.value("holdings").toArray();
    for (const auto& v : arr) {
        auto h = v.toObject();
        BrokerHolding hold;
        hold.symbol = h.value("symbol").toString();
        hold.exchange = h.value("exchange").toString();
        hold.quantity = h.value("quantity").toDouble();
        hold.avg_price = h.value("costPrice").toDouble();
        hold.ltp = h.value("ltp").toDouble();
        hold.invested_value = hold.quantity * hold.avg_price;
        hold.current_value = hold.quantity * hold.ltp;
        hold.pnl = hold.current_value - hold.invested_value;
        hold.pnl_pct = hold.invested_value > 0 ? (hold.pnl / hold.invested_value) * 100 : 0;
        holdings.append(hold);
    }
    return {true, holdings, "", ts};
}

ApiResponse<BrokerFunds> FyersBroker::get_funds(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/api/v3/funds", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("Failed"), ts};

    BrokerFunds funds;
    auto fl = resp.json.value("fund_limit").toArray();
    for (const auto& v : fl) {
        auto item = v.toObject();
        QString title = item.value("title").toString();
        double val = item.value("equityAmount").toDouble();
        if (title == "Total Balance")
            funds.total_balance = val;
        else if (title == "Available Balance")
            funds.available_balance = val;
        else if (title == "Used Margin")
            funds.used_margin = val;
    }
    funds.raw_data = resp.json;
    return {true, funds, "", ts};
}

ApiResponse<QVector<BrokerQuote>> FyersBroker::get_quotes(const BrokerCredentials& creds,
                                                          const QVector<QString>& symbols) {
    QString syms;
    for (int i = 0; i < symbols.size(); ++i) {
        if (i > 0)
            syms += ",";
        syms += symbols[i];
    }
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/data/quotes?symbols=" + syms, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("Failed"), ts};

    QVector<BrokerQuote> quotes;
    auto d_arr = resp.json.value("d").toArray();
    for (const auto& val : d_arr) {
        auto item = val.toObject();
        if (item.value("s").toString() != "ok")
            continue;
        auto v = item.value("v").toObject();
        BrokerQuote q;
        q.symbol = v.value("symbol").toString();
        q.ltp = v.value("lp").toDouble();
        q.open = v.value("open_price").toDouble();
        q.high = v.value("high_price").toDouble();
        q.low = v.value("low_price").toDouble();
        q.close = v.value("prev_close_price").toDouble();
        q.volume = v.value("volume").toDouble();
        q.change = v.value("ch").toDouble();
        q.change_pct = v.value("chp").toDouble();
        q.bid = v.value("bid").toDouble();
        q.ask = v.value("ask").toDouble();
        q.timestamp = v.value("tt").toVariant().toLongLong();
        quotes.append(q);
    }
    return {true, quotes, "", ts};
}

ApiResponse<QVector<BrokerCandle>> FyersBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                            const QString& resolution, const QString& from_date,
                                                            const QString& to_date) {
    QString url = QString("%1/data/history?symbol=%2&resolution=%3&date_format=1&range_from=%4&range_to=%5&cont_flag=1")
                      .arg(base_url(), symbol, resolution, from_date, to_date);

    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("Failed"), ts};

    QVector<BrokerCandle> candles;
    auto arr = resp.json.value("candles").toArray();
    for (const auto& v : arr) {
        auto c = v.toArray();
        if (c.size() < 6)
            continue;
        BrokerCandle candle;
        candle.timestamp = c[0].toVariant().toLongLong();
        candle.open = c[1].toDouble();
        candle.high = c[2].toDouble();
        candle.low = c[3].toDouble();
        candle.close = c[4].toDouble();
        candle.volume = c[5].toDouble();
        candles.append(candle);
    }
    return {true, candles, "", ts};
}

} // namespace fincept::trading
