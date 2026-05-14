#include "trading/brokers/upstox/UpstoxBroker.h"

#include "core/logging/Logger.h"
#include "trading/brokers/BrokerHttp.h"
#include "trading/instruments/InstrumentService.h"

#include <QDateTime>
#include <QJsonArray>
#include <QUrl>

namespace fincept::trading {

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// ── Exchange segment mapping ──────────────────────────────────────────────────
// Upstox uses segment codes like NSE_EQ, NSE_FO, BSE_EQ, etc.
static QString upstox_exchange(const QString& exchange) {
    static const QMap<QString, QString> map = {
        {"NSE", "NSE_EQ"}, {"BSE", "BSE_EQ"}, {"NFO", "NSE_FO"},          {"BFO", "BSE_FO"},
        {"CDS", "NSE_CD"}, {"MCX", "MCX_FO"}, {"NSE_INDEX", "NSE_INDEX"}, {"BSE_INDEX", "BSE_INDEX"},
    };
    return map.value(exchange.toUpper(), "NSE_EQ");
}

// Reverse: Upstox segment → canonical exchange
static QString canonical_exchange(const QString& seg) {
    static const QMap<QString, QString> map = {
        {"NSE_EQ", "NSE"}, {"BSE_EQ", "BSE"}, {"NSE_FO", "NFO"},          {"BSE_FO", "BFO"},
        {"NSE_CD", "CDS"}, {"MCX_FO", "MCX"}, {"NSE_INDEX", "NSE_INDEX"}, {"BSE_INDEX", "BSE_INDEX"},
    };
    return map.value(seg, seg);
}

// Build instrument_key: "{segment}|{token}"
// Uses InstrumentService token if available; falls back to "{segment}|{SYMBOL}" (works for EQ quotes)
static QString instrument_key(const QString& symbol, const QString& exchange, const QString& broker_id) {
    const QString seg = upstox_exchange(exchange);
    if (!broker_id.isEmpty() && InstrumentService::instance().is_loaded(broker_id)) {
        auto opt = InstrumentService::instance().find(symbol, exchange, broker_id);
        if (opt) {
            return seg + "|" + QString::number(static_cast<qlonglong>(opt->instrument_token));
        }
    }
    // Fallback: Upstox accepts segment|SYMBOL for equities
    return seg + "|" + symbol.toUpper();
}

// ── Product / OrderType mapping ───────────────────────────────────────────────
static const char* upstox_product(ProductType p, const QString& exchange) {
    // Delivery on derivatives → D (NRML equivalent), Intraday → I
    switch (p) {
        case ProductType::Intraday:
            return "I";
        case ProductType::Delivery:
            return "D";
        case ProductType::Margin:
            return "D"; // MTF maps to Delivery segment
        default:
            return "I";
    }
    (void)exchange;
}

static const char* upstox_order_type(OrderType t) {
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

// Reverse product mapping from Upstox response → display string.
// Upstox order/position responses return `exchange` as canonical "NSE"/"NFO"/...
// (not segment codes), so this accepts both forms and also derives the segment
// from an instrument_token like "NSE_FO|12345" when caller passes one in.
static QString rev_product(const QString& exchange_or_seg, const QString& product) {
    if (product == "I")
        return "Intraday";
    const QString s = exchange_or_seg.toUpper();
    const bool is_deriv = s == "NSE_FO" || s == "BSE_FO" || s == "MCX_FO" || s == "NSE_CD" ||
                          s == "NFO" || s == "BFO" || s == "MCX" || s == "CDS";
    return is_deriv ? "NRML" : "Delivery";
}

// ── Token expiry detection ────────────────────────────────────────────────────
bool UpstoxBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 401 || resp.status_code == 403)
        return true;
    // Upstox error responses: {"status":"error","errors":[{"errorCode":"UDAPI100010",...}]}
    // UDAPI100010 / UDAPI100011 = invalid/expired token
    const auto errors = resp.json.value("errors").toArray();
    for (const auto& e : errors) {
        const QString code = e.toObject().value("errorCode").toString();
        if (code.startsWith("UDAPI1000") && (code == "UDAPI100010" || code == "UDAPI100011"))
            return true;
    }
    // Also check message field for "token" keyword as last resort
    const QString msg = resp.json.value("message").toString().toLower();
    if (msg.contains("invalid token") || msg.contains("access token expired"))
        return true;
    return false;
}

QString UpstoxBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    // Prefer first error from errors array, then message, then network error, then fallback
    QString msg;
    const auto errors = resp.json.value("errors").toArray();
    if (!errors.isEmpty())
        msg = errors.first().toObject().value("message").toString();
    if (msg.isEmpty())
        msg = resp.json.value("message").toString();
    if (msg.isEmpty())
        msg = resp.error.isEmpty() ? fallback : resp.error;

    if (is_token_expired(resp))
        return "[TOKEN_EXPIRED] " + msg;
    return msg;
}

// ── History interval mapping ──────────────────────────────────────────────────
// Returns {unit, interval_str, max_days_per_chunk}
struct UpstoxInterval {
    QString unit;
    QString interval;
    int max_days;
};
static UpstoxInterval upstox_interval(const QString& resolution) {
    static const QMap<QString, UpstoxInterval> map = {
        {"1", {"minutes", "1", 30}},    {"1m", {"minutes", "1", 30}},   {"2", {"minutes", "2", 30}},
        {"2m", {"minutes", "2", 30}},   {"3", {"minutes", "3", 30}},    {"3m", {"minutes", "3", 30}},
        {"5", {"minutes", "5", 30}},    {"5m", {"minutes", "5", 30}},   {"10", {"minutes", "10", 90}},
        {"10m", {"minutes", "10", 90}}, {"15", {"minutes", "15", 90}},  {"15m", {"minutes", "15", 90}},
        {"30", {"minutes", "30", 90}},  {"30m", {"minutes", "30", 90}}, {"60", {"minutes", "60", 90}},
        {"60m", {"minutes", "60", 90}}, {"1h", {"minutes", "60", 90}},  {"2h", {"hours", "2", 90}},
        {"4h", {"hours", "4", 90}},     {"D", {"days", "1", 3650}},     {"1D", {"days", "1", 3650}},
        {"W", {"weeks", "1", 7300}},    {"M", {"months", "1", 7300}},
    };
    return map.value(resolution, {"days", "1", 3650});
}

// ── Auth headers ──────────────────────────────────────────────────────────────
QMap<QString, QString> UpstoxBroker::auth_headers(const BrokerCredentials& creds) const {
    return {{"Authorization", "Bearer " + creds.access_token},
            {"Content-Type", "application/json"},
            {"Accept", "application/json"}};
}

// ── Token Exchange ─────────────────────────────────────────────────────────────
// POST https://api.upstox.com/v2/login/authorization/token
// Form-encoded: code, client_id, client_secret, redirect_uri, grant_type
TokenExchangeResponse UpstoxBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                   const QString& auth_code) {
    // Upstox requires a registered redirect_uri — use the loopback standard
    const QString redirect_uri = "http://127.0.0.1";

    QMap<QString, QString> form;
    form["code"] = auth_code;
    form["client_id"] = api_key;
    form["client_secret"] = api_secret;
    form["redirect_uri"] = redirect_uri;
    form["grant_type"] = "authorization_code";

    auto& http = BrokerHttp::instance();
    auto resp = http.post_form("https://api.upstox.com/v2/login/authorization/token", form, {});

    if (!resp.success) {
        LOG_ERROR("Upstox", "exchange_token HTTP error: " + resp.error);
        return {false, "", "", "", "", resp.error};
    }

    // Success: {"access_token":"...","extended_token":"...","user_id":"..."}
    if (resp.json.value("status").toString() == "success" || resp.json.contains("access_token")) {
        const QString access = resp.json.value("access_token").toString();
        const QString user = resp.json.value("user_id").toString();
        if (access.isEmpty()) {
            const QString err = checked_error(resp, "Empty access_token in response");
            return {false, "", "", "", "", err};
        }
        LOG_INFO("Upstox", "Token exchange OK, user=" + user);
        return {true, access, /*refresh*/ "", user, /*additional*/ "", ""};
    }

    const QString err = checked_error(resp, "Token exchange failed");
    LOG_ERROR("Upstox", "exchange_token failed: " + err);
    return {false, "", "", "", "", err};
}

// ── Place Order ───────────────────────────────────────────────────────────────
// POST /v2/order/place
OrderPlaceResponse UpstoxBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    const QString ikey = instrument_key(order.symbol, order.exchange, creds.broker_id);

    QJsonObject body;
    body["instrument_token"] = ikey;
    body["transaction_type"] = order.side == OrderSide::Buy ? "BUY" : "SELL";
    body["quantity"] = static_cast<int>(order.quantity);
    body["product"] = upstox_product(order.product_type, order.exchange);
    body["order_type"] = upstox_order_type(order.order_type);
    body["validity"] = "DAY";
    body["price"] = order.price;
    body["trigger_price"] = order.stop_price;
    body["disclosed_quantity"] = 0;
    body["is_amo"] = order.amo;
    body["tag"] = "fincept"; // visible in order history for reconciliation

    auto& http = BrokerHttp::instance();
    // v2 order mutations live on the HFT host; api.upstox.com returns 404 for /order/*
    auto resp = http.post_json("https://api-hft.upstox.com/v2/order/place", body, auth_headers(creds));

    if (!resp.success || resp.json.value("status").toString() != "success") {
        const QString err = checked_error(resp, "place_order failed");
        LOG_ERROR("Upstox", "place_order: " + err);
        return {false, "", err};
    }

    const QString order_id = resp.json.value("data").toObject().value("order_id").toString();
    LOG_INFO("Upstox", "place_order OK: " + order_id);
    return {true, order_id, ""};
}

// ── Modify Order ──────────────────────────────────────────────────────────────
// PUT /v2/order/modify
ApiResponse<QJsonObject> UpstoxBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                    const QJsonObject& mods) {
    // Only forward fields the caller actually supplied. Per v3 docs, missing
    // optional fields inherit the original order's values — sending 0/empty
    // would overwrite price/qty/trigger to zero.
    QJsonObject body;
    body["order_id"] = order_id;
    if (mods.contains("quantity"))
        body["quantity"] = mods.value("quantity").toInt();
    if (mods.contains("price"))
        body["price"] = mods.value("price").toDouble();
    if (mods.contains("order_type"))
        body["order_type"] = mods.value("order_type").toString();
    if (mods.contains("trigger_price"))
        body["trigger_price"] = mods.value("trigger_price").toDouble();
    if (mods.contains("disclosed_quantity"))
        body["disclosed_quantity"] = mods.value("disclosed_quantity").toInt();
    body["validity"] = mods.value("validity").toString("DAY");

    auto& http = BrokerHttp::instance();
    // HFT host for order mutations
    auto resp = http.put_json("https://api-hft.upstox.com/v2/order/modify", body, auth_headers(creds));

    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "modify_order failed"), ts};
    return {true, resp.json.value("data").toObject(), "", ts};
}

// ── Cancel Order ──────────────────────────────────────────────────────────────
// DELETE /v2/order/cancel?order_id={id}
ApiResponse<QJsonObject> UpstoxBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    auto& http = BrokerHttp::instance();
    auto resp = http.del("https://api-hft.upstox.com/v2/order/cancel?order_id=" + order_id, auth_headers(creds));

    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "cancel_order failed"), ts};
    return {true, resp.json.value("data").toObject(), "", ts};
}

// ── Get Orders ────────────────────────────────────────────────────────────────
// GET /v2/order/retrieve-all
ApiResponse<QVector<BrokerOrderInfo>> UpstoxBroker::get_orders(const BrokerCredentials& creds) {
    auto& http = BrokerHttp::instance();
    auto resp = http.get("https://api.upstox.com/v2/order/retrieve-all", auth_headers(creds));

    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "get_orders failed"), ts};

    QVector<BrokerOrderInfo> orders;
    const auto arr = resp.json.value("data").toArray();
    orders.reserve(arr.size());
    for (const auto& v : arr) {
        const auto o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("order_id").toString();
        info.symbol = o.value("tradingsymbol").toString();
        info.exchange = canonical_exchange(o.value("exchange").toString());
        info.side = o.value("transaction_type").toString();
        info.order_type = o.value("order_type").toString();
        info.product_type = rev_product(o.value("exchange").toString(), o.value("product").toString());
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

// ── Get Trade Book ────────────────────────────────────────────────────────────
// GET /v2/order/trades/get-trades-for-day
ApiResponse<QJsonObject> UpstoxBroker::get_trade_book(const BrokerCredentials& creds) {
    auto& http = BrokerHttp::instance();
    auto resp = http.get("https://api.upstox.com/v2/order/trades/get-trades-for-day", auth_headers(creds));

    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "get_trade_book failed"), ts};
    return {true, resp.json, "", ts};
}

// ── Get Positions ─────────────────────────────────────────────────────────────
// GET /v2/portfolio/short-term-positions
ApiResponse<QVector<BrokerPosition>> UpstoxBroker::get_positions(const BrokerCredentials& creds) {
    auto& http = BrokerHttp::instance();
    auto resp = http.get("https://api.upstox.com/v2/portfolio/short-term-positions", auth_headers(creds));

    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "get_positions failed"), ts};

    QVector<BrokerPosition> positions;
    const auto arr = resp.json.value("data").toArray();
    positions.reserve(arr.size());
    for (const auto& v : arr) {
        const auto p = v.toObject();
        const double qty = p.value("quantity").toDouble();
        if (qty == 0.0)
            continue; // skip flat positions

        BrokerPosition pos;
        pos.symbol = p.value("tradingsymbol").toString();
        pos.exchange = canonical_exchange(p.value("exchange").toString());
        pos.product_type = rev_product(p.value("exchange").toString(), p.value("product").toString());
        pos.quantity = qty;
        pos.avg_price = p.value("average_price").toDouble();
        pos.ltp = p.value("last_price").toDouble();
        pos.pnl = p.value("pnl").toDouble();
        pos.day_pnl = p.value("day_pnl").toDouble();
        pos.side = qty > 0 ? "LONG" : "SHORT";
        if (pos.avg_price > 0)
            pos.pnl_pct = (pos.ltp - pos.avg_price) / pos.avg_price * 100.0;
        positions.append(pos);
    }
    return {true, positions, "", ts};
}

// ── Get Holdings ──────────────────────────────────────────────────────────────
// GET /v2/portfolio/long-term-holdings
ApiResponse<QVector<BrokerHolding>> UpstoxBroker::get_holdings(const BrokerCredentials& creds) {
    auto& http = BrokerHttp::instance();
    auto resp = http.get("https://api.upstox.com/v2/portfolio/long-term-holdings", auth_headers(creds));

    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};

    QVector<BrokerHolding> holdings;
    const auto arr = resp.json.value("data").toArray();
    holdings.reserve(arr.size());
    for (const auto& v : arr) {
        const auto h = v.toObject();
        BrokerHolding holding;
        holding.symbol = h.value("tradingsymbol").toString();
        holding.exchange = canonical_exchange(h.value("exchange").toString());
        holding.quantity = h.value("quantity").toDouble();
        holding.avg_price = h.value("average_price").toDouble();
        holding.ltp = h.value("last_price").toDouble();
        holding.pnl = h.value("pnl").toDouble();
        holding.invested_value = holding.quantity * holding.avg_price;
        holding.current_value = holding.quantity * holding.ltp;
        if (holding.avg_price > 0)
            holding.pnl_pct = (holding.ltp - holding.avg_price) / holding.avg_price * 100.0;
        holdings.append(holding);
    }
    return {true, holdings, "", ts};
}

// ── Get Funds ─────────────────────────────────────────────────────────────────
// GET /v2/user/get-funds-and-margin
ApiResponse<BrokerFunds> UpstoxBroker::get_funds(const BrokerCredentials& creds) {
    auto& http = BrokerHttp::instance();
    auto resp = http.get("https://api.upstox.com/v2/user/get-funds-and-margin", auth_headers(creds));

    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    // data has "equity" and "commodity" sub-objects
    const auto data = resp.json.value("data").toObject();
    const auto equity = data.value("equity").toObject();
    const auto commod = data.value("commodity").toObject();

    BrokerFunds funds;
    funds.available_balance = equity.value("available_margin").toDouble() + commod.value("available_margin").toDouble();
    funds.used_margin = equity.value("used_margin").toDouble() + commod.value("used_margin").toDouble();
    funds.total_balance = funds.available_balance + funds.used_margin;
    funds.raw_data = data;
    return {true, funds, "", ts};
}

// ── Get Quotes ────────────────────────────────────────────────────────────────
// GET /v3/market-quote/ohlc?instrument_key=KEY1,KEY2,...&interval=1d
ApiResponse<QVector<BrokerQuote>> UpstoxBroker::get_quotes(const BrokerCredentials& creds,
                                                           const QVector<QString>& symbols) {
    if (symbols.isEmpty()) {
        int64_t ts = now_ts();
        return {true, QVector<BrokerQuote>{}, "", ts};
    }

    // Build comma-separated instrument_key list
    QStringList keys;
    keys.reserve(symbols.size());
    for (const auto& sym : symbols) {
        // Support "EXCHANGE:SYMBOL" format
        QString exch = "NSE", s = sym;
        if (sym.contains(':')) {
            exch = sym.section(':', 0, 0);
            s = sym.section(':', 1);
        }
        keys.append(instrument_key(s, exch, creds.broker_id));
    }

    const QString url = "https://api.upstox.com/v3/market-quote/ohlc?instrument_key=" + keys.join(",") + "&interval=1d";

    auto& http = BrokerHttp::instance();
    auto resp = http.get(url, auth_headers(creds));

    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "get_quotes failed"), ts};

    QVector<BrokerQuote> quotes;
    const auto data = resp.json.value("data").toObject();
    for (auto it = data.begin(); it != data.end(); ++it) {
        const auto entry = it.value().toObject();
        const auto ohlc = entry.value("ohlc").toObject();

        BrokerQuote q;
        // Key may arrive as "NSE_EQ|12345" or "NSE_EQ:NHPC" depending on
        // endpoint version — fall back through both separators.
        const QString key = it.key();
        q.symbol = entry.value("symbol").toString();
        if (q.symbol.isEmpty()) {
            if (key.contains('|'))
                q.symbol = key.section('|', 1);
            else if (key.contains(':'))
                q.symbol = key.section(':', 1);
            else
                q.symbol = key;
        }
        q.ltp = entry.value("last_price").toDouble();
        q.open = ohlc.value("open").toDouble();
        q.high = ohlc.value("high").toDouble();
        q.low = ohlc.value("low").toDouble();
        q.close = ohlc.value("close").toDouble();
        q.volume = entry.value("volume").toDouble();
        if (q.close > 0)
            q.change_pct = (q.ltp - q.close) / q.close * 100.0;
        q.change = q.ltp - q.close;
        q.timestamp = now_ts();
        quotes.append(q);
    }
    return {true, quotes, "", ts};
}

// ── Get History ───────────────────────────────────────────────────────────────
// GET /v3/historical-candle/{instrument_key}/{unit}/{interval}/{to_date}/{from_date}
ApiResponse<QVector<BrokerCandle>> UpstoxBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                             const QString& resolution, const QString& from_date,
                                                             const QString& to_date) {
    const QString ikey = instrument_key(symbol, "NSE", creds.broker_id);
    const auto iv = upstox_interval(resolution);

    // Chunk the date range if needed (Upstox has per-interval max window limits)
    QDate from = QDate::fromString(from_date, "yyyy-MM-dd");
    QDate to = QDate::fromString(to_date, "yyyy-MM-dd");
    if (!from.isValid())
        from = QDate::currentDate().addYears(-1);
    if (!to.isValid())
        to = QDate::currentDate();

    auto& http = BrokerHttp::instance();
    QVector<BrokerCandle> all_candles;

    QDate chunk_end = to;
    while (chunk_end >= from) {
        QDate chunk_start = chunk_end.addDays(-iv.max_days + 1);
        if (chunk_start < from)
            chunk_start = from;

        const QString url = QString("https://api.upstox.com/v3/historical-candle/%1/%2/%3/%4/%5")
                                .arg(QString(QUrl::toPercentEncoding(ikey)))
                                .arg(iv.unit)
                                .arg(iv.interval)
                                .arg(chunk_end.toString("yyyy-MM-dd"))
                                .arg(chunk_start.toString("yyyy-MM-dd"));

        auto resp = http.get(url, auth_headers(creds));
        if (!resp.success || resp.json.value("status").toString() != "success") {
            if (all_candles.isEmpty()) {
                int64_t ts = now_ts();
                return {false, std::nullopt, checked_error(resp, "get_history failed"), ts};
            }
            break; // partial result is still useful
        }

        // data.candles: [[timestamp, open, high, low, close, volume, oi], ...]
        const auto candles = resp.json.value("data").toObject().value("candles").toArray();
        for (const auto& cv : candles) {
            const auto c = cv.toArray();
            if (c.size() < 6)
                continue;
            BrokerCandle candle;
            // v3 returns ISO8601 string; some endpoints/snapshots return epoch ms.
            // Try string first, fall back to numeric.
            const QString ts_str = c[0].toString();
            if (!ts_str.isEmpty()) {
                candle.timestamp = QDateTime::fromString(ts_str, Qt::ISODate).toSecsSinceEpoch();
                if (candle.timestamp == 0)
                    candle.timestamp = QDateTime::fromString(ts_str, Qt::ISODateWithMs).toSecsSinceEpoch();
            }
            if (candle.timestamp == 0) {
                const qint64 ms = c[0].toVariant().toLongLong();
                candle.timestamp = ms > 1'000'000'000'000LL ? ms / 1000 : ms;
            }
            candle.open = c[1].toDouble();
            candle.high = c[2].toDouble();
            candle.low = c[3].toDouble();
            candle.close = c[4].toDouble();
            candle.volume = c[5].toDouble();
            all_candles.append(candle);
        }

        chunk_end = chunk_start.addDays(-1);
    }

    // Sort ascending by timestamp (chunks fetched newest-first)
    std::sort(all_candles.begin(), all_candles.end(),
              [](const BrokerCandle& a, const BrokerCandle& b) { return a.timestamp < b.timestamp; });

    int64_t ts = now_ts();
    return {true, all_candles, "", ts};
}

} // namespace fincept::trading
