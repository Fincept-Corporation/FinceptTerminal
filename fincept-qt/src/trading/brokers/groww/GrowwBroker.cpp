#include "trading/brokers/groww/GrowwBroker.h"
#include "trading/brokers/BrokerHttp.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

namespace fincept::trading {

static int64_t now_ts() { return QDateTime::currentSecsSinceEpoch(); }

// ============================================================================
// Helpers
// ============================================================================

bool GrowwBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 401 || resp.status_code == 403)
        return true;
    if (!resp.json.isEmpty()) {
        QString status = resp.json["status"].toString();
        if (!status.isEmpty() && status != "SUCCESS")
            return false; // error but not necessarily token expiry — checked per-call
    }
    return false;
}

QString GrowwBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    if (!resp.json.isEmpty()) {
        QString msg = resp.json["message"].toString();
        if (!msg.isEmpty()) return msg;
        msg = resp.json["error"].toString();
        if (!msg.isEmpty()) return msg;
        QString status = resp.json["status"].toString();
        if (!status.isEmpty() && status != "SUCCESS")
            return QString("Groww error: status=%1").arg(status);
    }
    if (!resp.error.isEmpty()) return resp.error;
    if (!resp.raw_body.isEmpty()) return resp.raw_body.left(200);
    return fallback;
}

// exchange as sent to Groww order endpoints (NSE/BSE for CASH; NSE/BSE for FNO)
QString GrowwBroker::groww_exchange(const QString& exchange) {
    if (exchange == "NFO") return "NSE";
    if (exchange == "BFO") return "BSE";
    return exchange; // NSE, BSE as-is
}

// segment: CASH for equities, FNO for derivatives
QString GrowwBroker::groww_segment(const QString& exchange) {
    if (exchange == "NFO" || exchange == "BFO") return "FNO";
    return "CASH";
}

QString GrowwBroker::groww_product(ProductType p) {
    switch (p) {
        case ProductType::Intraday: return "MIS";
        case ProductType::Delivery: return "CNC";
        case ProductType::Margin:   return "NRML";
        default:                    return "CNC";
    }
}

QString GrowwBroker::groww_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "MARKET";
        case OrderType::Limit:         return "LIMIT";
        case OrderType::StopLoss:      return "STOP_LOSS_MARKET";   // SL-M: trigger only, market fill
        case OrderType::StopLossLimit: return "STOP_LOSS_LIMIT";    // SL:   trigger + limit price
        default:                       return "MARKET";
    }
}

// resolution string → interval_in_minutes for Groww historical API
int GrowwBroker::resolution_to_minutes(const QString& resolution) {
    if (resolution == "1"  || resolution == "1m")  return 1;
    if (resolution == "5"  || resolution == "5m")  return 5;
    if (resolution == "10" || resolution == "10m") return 10;
    if (resolution == "15" || resolution == "15m") return 15;
    if (resolution == "30" || resolution == "30m") return 30;
    if (resolution == "60" || resolution == "1h")  return 60;
    if (resolution == "D"  || resolution == "1D")  return 1440;
    if (resolution == "W"  || resolution == "1W")  return 10080;
    return 1;
}

// ============================================================================
// Auth headers — Bearer access_token
// ============================================================================

QMap<QString, QString> GrowwBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Authorization", "Bearer " + creds.access_token},
        {"Content-Type",  "application/json"},
        {"Accept",        "application/json"},
    };
}

// ============================================================================
// Phase 2: exchange_token
// Checksum = SHA256(api_secret + timestamp_string), hex-encoded
// POST https://api.groww.in/v1/token/api/access
// Headers: Authorization: Bearer {api_key}   (api_key used as bearer before auth)
// Body: { key_type: "approval", checksum: "...", timestamp: "1234567890" }
// Response: { "token": "eyJ..." }
// ============================================================================

TokenExchangeResponse GrowwBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                   const QString& /*auth_code*/) {
    QString timestamp = QString::number(QDateTime::currentSecsSinceEpoch());

    // SHA256(api_secret + timestamp) — hex digest
    QByteArray data = (api_secret + timestamp).toUtf8();
    QString checksum = QString(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());

    QJsonObject body;
    body["key_type"]  = "approval";
    body["checksum"]  = checksum;
    body["timestamp"] = timestamp;

    QMap<QString, QString> headers = {
        {"Authorization", "Bearer " + api_key},
        {"Content-Type",  "application/json"},
        {"Accept",        "application/json"},
    };

    auto resp = BrokerHttp::instance().post_json("https://api.groww.in/v1/token/api/access", body, headers);

    if (!resp.success)
        return {false, "", "", "", checked_error(resp, "Network error")};

    QString token = resp.json["token"].toString();
    if (token.isEmpty())
        return {false, "", "", "", checked_error(resp, "No token in response")};

    return {true, token, "", "", ""};
}

// ============================================================================
// Phase 3: Order operations
// ============================================================================

OrderPlaceResponse GrowwBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    auto hdrs = auth_headers(creds);

    QJsonObject body;
    body["trading_symbol"]   = order.symbol;
    body["quantity"]         = order.quantity;
    body["validity"]         = "DAY";
    body["exchange"]         = groww_exchange(order.exchange);
    body["segment"]          = groww_segment(order.exchange);
    body["product"]          = groww_product(order.product_type);
    body["order_type"]       = groww_order_type(order.order_type);
    body["transaction_type"] = (order.side == OrderSide::Buy) ? "BUY" : "SELL";

    if (order.order_type != OrderType::Market)
        body["price"] = order.price;
    if (order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit)
        body["trigger_price"] = order.stop_price;

    auto resp = BrokerHttp::instance().post_json(
        "https://api.groww.in/v1/order/create", body, hdrs);

    if (!resp.success)
        return {false, "", checked_error(resp, "Network error")};

    if (is_token_expired(resp))
        return {false, "", "[TOKEN_EXPIRED]"};

    QJsonObject payload = resp.json["payload"].toObject();
    QString order_id = payload["order_id"].toString();
    if (order_id.isEmpty())
        return {false, "", checked_error(resp, "No order_id in response")};

    return {true, order_id, ""};
}

ApiResponse<QJsonObject> GrowwBroker::modify_order(const BrokerCredentials& creds,
                                                    const QString& order_id,
                                                    const QJsonObject& mods) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    QJsonObject body;
    body["order_id"] = order_id;
    if (mods.contains("quantity"))      body["quantity"]      = mods["quantity"];
    if (mods.contains("price"))         body["price"]         = mods["price"];
    if (mods.contains("trigger_price")) body["trigger_price"] = mods["trigger_price"];
    if (mods.contains("order_type"))    body["order_type"]    = mods["order_type"];

    auto resp = BrokerHttp::instance().post_json(
        "https://api.groww.in/v1/order/modify", body, hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    QString status = resp.json["status"].toString();
    if (status != "SUCCESS")
        return {false, std::nullopt, checked_error(resp, "Modify failed"), ts};

    return {true, resp.json, "", ts};
}

ApiResponse<QJsonObject> GrowwBroker::cancel_order(const BrokerCredentials& creds,
                                                    const QString& order_id) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    QJsonObject body;
    body["order_id"] = order_id;

    auto resp = BrokerHttp::instance().post_json(
        "https://api.groww.in/v1/order/cancel", body, hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    QString status = resp.json["status"].toString();
    if (status != "SUCCESS")
        return {false, std::nullopt, checked_error(resp, "Cancel failed"), ts};

    return {true, resp.json, "", ts};
}

// ============================================================================
// Phase 4: Read operations
// ============================================================================

ApiResponse<QVector<BrokerOrderInfo>> GrowwBroker::get_orders(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);
    QVector<BrokerOrderInfo> orders;

    auto parse_status = [](const QString& s) -> QString {
        if (s == "COMPLETED" || s == "EXECUTED" || s == "DELIVERY_AWAITED") return "filled";
        if (s == "OPEN" || s == "NEW" || s == "ACKED" || s == "APPROVED")   return "open";
        if (s == "CANCELLED" || s == "CANCELLATION_REQUESTED")              return "cancelled";
        if (s == "REJECTED" || s == "FAILED")                               return "rejected";
        if (s == "TRIGGER_PENDING" || s == "MODIFICATION_REQUESTED")        return "pending";
        return "open";
    };

    auto fetch_segment = [&](const QString& segment) -> bool {
        constexpr int PAGE_SIZE = 25; // Groww API max per page
        int page = 0;
        while (true) {
        QString url = QString("https://api.groww.in/v1/order/list?segment=%1&page=%2&page_size=%3")
                          .arg(segment).arg(page).arg(PAGE_SIZE);
        auto resp = BrokerHttp::instance().get(url, hdrs);
        if (!resp.success) return false;
        if (is_token_expired(resp)) return false;

        QJsonObject payload = resp.json["payload"].toObject();
        QJsonArray list = payload["order_list"].toArray();
        for (const auto& item : list) {
            QJsonObject o = item.toObject();
            BrokerOrderInfo info;
            info.order_id       = o["order_id"].toString();
            info.symbol         = o["trading_symbol"].toString();
            info.exchange       = o["exchange"].toString();
            info.quantity       = o["quantity"].toInt();
            info.filled_qty     = o["filled_quantity"].toInt();
            info.price          = o["price"].toDouble();
            info.trigger_price  = o["trigger_price"].toDouble();
            info.status         = parse_status(o["status"].toString());
            info.side           = (o["transaction_type"].toString() == "BUY") ? "buy" : "sell";
            info.order_type     = o["order_type"].toString();
            info.product_type   = o["product"].toString();
            info.timestamp      = o["order_time"].toString();
            orders.append(info);
        }
        if (list.size() < PAGE_SIZE) break; // last page
        ++page;
        } // while
        return true;
    };

    fetch_segment("CASH");
    fetch_segment("FNO");

    return {true, orders, "", ts};
}

ApiResponse<QJsonObject> GrowwBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    auto resp = BrokerHttp::instance().get("https://api.groww.in/v1/order/trade/list", hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    return {true, resp.json, "", ts};
}

ApiResponse<QVector<BrokerPosition>> GrowwBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);
    QVector<BrokerPosition> positions;

    auto fetch_segment = [&](const QString& segment) -> bool {
        QString url = QString("https://api.groww.in/v1/positions/user?segment=%1").arg(segment);
        auto resp = BrokerHttp::instance().get(url, hdrs);
        if (!resp.success) return false;
        if (is_token_expired(resp)) return false;

        QJsonArray list = resp.json["payload"].toArray();
        for (const auto& item : list) {
            QJsonObject p = item.toObject();
            int qty = p["quantity"].toInt();
            if (qty == 0) continue;

            BrokerPosition pos;
            pos.symbol        = p["trading_symbol"].toString();
            pos.exchange      = p["exchange"].toString();
            pos.quantity      = qty;
            pos.avg_price     = p["buy_price"].toDouble();
            pos.ltp           = p["ltp"].toDouble();
            pos.pnl           = p["pnl"].toDouble();
            pos.product_type  = p["product"].toString();
            positions.append(pos);
        }
        return true;
    };

    fetch_segment("CASH");
    fetch_segment("FNO");

    return {true, positions, "", ts};
}

ApiResponse<QVector<BrokerHolding>> GrowwBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    auto resp = BrokerHttp::instance().get("https://api.groww.in/v1/portfolio/holdings", hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    QJsonArray arr = resp.json["payload"].toObject()["holdings"].toArray();
    QVector<BrokerHolding> holdings;
    holdings.reserve(arr.size());

    for (const auto& item : arr) {
        QJsonObject h = item.toObject();
        BrokerHolding holding;
        holding.symbol         = h["trading_symbol"].toString();
        holding.exchange       = h["exchange"].toString();
        holding.quantity       = h["quantity"].toInt();
        holding.avg_price      = h["average_price"].toDouble();
        holding.ltp            = h["last_price"].toDouble();
        holding.pnl            = h["pnl"].toDouble();
        holdings.append(holding);
    }

    return {true, holdings, "", ts};
}

ApiResponse<BrokerFunds> GrowwBroker::get_funds(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    auto resp = BrokerHttp::instance().get("https://api.groww.in/v1/margins/detail/user", hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    QJsonObject payload = resp.json["payload"].toObject();
    double clear_cash    = payload["clear_cash"].toDouble();
    double collateral    = payload["collateral_available"].toDouble();
    double used          = payload["net_margin_used"].toDouble();

    BrokerFunds funds;
    funds.available_balance = clear_cash + collateral;
    funds.used_margin       = used;
    funds.total_balance     = funds.available_balance + funds.used_margin;
    funds.collateral        = collateral;

    return {true, funds, "", ts};
}

ApiResponse<QVector<BrokerQuote>> GrowwBroker::get_quotes(const BrokerCredentials& creds,
                                                           const QVector<QString>& symbols) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);
    QVector<BrokerQuote> quotes;

    for (const QString& sym : symbols) {
        // symbols may be "NSE:RELIANCE" or just "RELIANCE" — split if colon present
        QString exchange = "NSE";
        QString trading_symbol = sym;
        int colon = sym.indexOf(':');
        if (colon != -1) {
            exchange       = sym.left(colon);
            trading_symbol = sym.mid(colon + 1);
        }

        QString segment = groww_segment(exchange);
        QString ex      = groww_exchange(exchange);

        QString url = QString("https://api.groww.in/v1/live-data/quote"
                              "?exchange=%1&segment=%2&trading_symbol=%3")
                          .arg(ex, segment, trading_symbol);

        auto resp = BrokerHttp::instance().get(url, hdrs);
        if (!resp.success) continue;
        if (is_token_expired(resp)) break;

        QJsonObject payload = resp.json["payload"].toObject();
        QJsonObject ohlc    = payload["ohlc"].toObject();

        BrokerQuote q;
        q.symbol     = sym;
        q.ltp        = payload["last_price"].toDouble();
        q.open       = ohlc["open"].toDouble();
        q.high       = ohlc["high"].toDouble();
        q.low        = ohlc["low"].toDouble();
        q.close      = ohlc["close"].toDouble();
        q.volume     = payload["volume"].toDouble();
        quotes.append(q);
    }

    return {true, quotes, "", ts};
}

ApiResponse<QVector<BrokerCandle>> GrowwBroker::get_history(const BrokerCredentials& creds,
                                                             const QString& symbol,
                                                             const QString& resolution,
                                                             const QString& from_date,
                                                             const QString& to_date) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    QString exchange = "NSE";
    QString trading_symbol = symbol;
    int colon = symbol.indexOf(':');
    if (colon != -1) {
        exchange       = symbol.left(colon);
        trading_symbol = symbol.mid(colon + 1);
    }

    QString segment  = groww_segment(exchange);
    QString ex       = groww_exchange(exchange);
    int     interval = resolution_to_minutes(resolution);

    // Groww expects datetime format "YYYY-MM-DD HH:MM:SS"
    // from_date/to_date arrive as "YYYY-MM-DD" — append times
    QString start_time = from_date + " 09:15:00";
    QString end_time   = to_date   + " 15:30:00";

    QString url = QString("https://api.groww.in/v1/historical/candle/range"
                          "?exchange=%1&segment=%2&trading_symbol=%3"
                          "&start_time=%4&end_time=%5&interval_in_minutes=%6")
                      .arg(ex, segment, trading_symbol,
                           start_time, end_time, QString::number(interval));

    auto resp = BrokerHttp::instance().get(url, hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    QJsonArray candles = resp.json["payload"].toObject()["candles"].toArray();
    QVector<BrokerCandle> result;
    result.reserve(candles.size());

    for (const auto& item : candles) {
        QJsonArray c = item.toArray();
        if (c.size() < 6) continue;
        BrokerCandle candle;
        // [timestamp_ms, open, high, low, close, volume]
        candle.timestamp = static_cast<int64_t>(c[0].toDouble());
        candle.open      = c[1].toDouble();
        candle.high      = c[2].toDouble();
        candle.low       = c[3].toDouble();
        candle.close     = c[4].toDouble();
        candle.volume    = c[5].toDouble();
        result.append(candle);
    }

    return {true, result, "", ts};
}

} // namespace fincept::trading
