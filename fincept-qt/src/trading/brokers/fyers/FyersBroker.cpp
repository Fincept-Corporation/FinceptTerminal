// Fyers Broker — Qt-adapted port

#include "trading/brokers/fyers/FyersBroker.h"

#include "core/logging/Logger.h"
#include "trading/brokers/BrokerHttp.h"
#include "trading/brokers/BrokerTokenUtil.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>

namespace fincept::trading {

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// Fyers requires symbols in EXCHANGE:SYMBOL-SEGMENT format (e.g. NSE:RELIANCE-EQ).
// Plain symbols from the UI (e.g. "RELIANCE") are normalized to NSE equity by default.
static QString to_fyers_sym(const QString& sym) {
    if (sym.contains(':'))
        return sym;
    return QStringLiteral("NSE:") + sym + QStringLiteral("-EQ");
}

// Strip Fyers format back to plain symbol: NSE:RELIANCE-EQ → RELIANCE.
// F&O contracts come back with "-CE"/"-PE"/"-FUT"; strip those too so a position
// symbol matches the bare HSM symbol the WS feed publishes (e.g.
// "NIFTY09JUN26C22650"), keeping option positions marked-to-market live. We strip
// only known segment suffixes — NOT everything after the first '-', since names
// like "BAJAJ-AUTO-EQ" legitimately contain hyphens.
static QString from_fyers_sym(const QString& fyers_sym) {
    QString s = fyers_sym;
    const int colon = s.indexOf(':');
    if (colon >= 0)
        s = s.mid(colon + 1);
    static const QStringList kSuffixes = {QStringLiteral("-EQ"), QStringLiteral("-CE"),
                                          QStringLiteral("-PE"), QStringLiteral("-FUT")};
    for (const auto& suf : kSuffixes) {
        if (s.endsWith(suf)) {
            s.chop(int(suf.size()));
            break;
        }
    }
    return s;
}

// Extract exchange string from Fyers symbol or int code
static QString exchange_of(const QString& fyers_sym, int code = -1) {
    const int colon = fyers_sym.indexOf(':');
    if (colon >= 0)
        return fyers_sym.left(colon);
    switch (code) {
        case 10: case 11: case 12: return QStringLiteral("NSE");
        case 20: case 21: case 22: return QStringLiteral("BSE");
        case 30: return QStringLiteral("MCX");
        default: return {};
    }
}

// Tag Fyers auth errors so AccountDataStream can detect token expiry.
// Fyers returns HTTP 401/403 or JSON code -16 for invalid/expired tokens.
static QString fyers_check_auth(const BrokerHttpResponse& resp, const QString& msg) {
    if (resp.status_code == 401 || resp.status_code == 403)
        return QStringLiteral("[TOKEN_EXPIRED] ") + msg;
    const int code = resp.json.value("code").toInt(0);
    if (code == -16 || code == -17)
        return QStringLiteral("[TOKEN_EXPIRED] ") + msg;
    return msg;
}

// ============================================================================
// Auth
// ============================================================================

QString FyersBroker::fyers_login_url(const QString& client_id, const QString& redirect_uri) {
    QUrl url(QStringLiteral("https://api-t1.fyers.in/api/v3/generate-authcode"));
    QUrlQuery q;
    q.addQueryItem("client_id", client_id);
    q.addQueryItem("redirect_uri", redirect_uri);
    q.addQueryItem("response_type", "code");
    q.addQueryItem("state", QString::number(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(q);
    return url.toString();
}

QMap<QString, QString> FyersBroker::auth_headers(const BrokerCredentials& creds) const {
    return {{"Authorization", creds.api_key + ":" + creds.access_token},
            {"Content-Type", "application/json"},
            {"Accept", "application/json"},
            {"version", "3"}};
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
        result.refresh_token = resp.json.value("refresh_token").toString();
        result.user_id = resp.json.value("user_id").toString();
        // Fyers access tokens lapse at the daily login reset; the live sweep
        // re-validates and silent-refreshes (15-day refresh token) as needed.
        result.additional_data = with_token_expiry(result.additional_data, next_ist_flush_epoch(6, 0));
    } else {
        result.error = resp.json.value("message").toString("Token exchange failed");
    }
    return result;
}

// Silent refresh using the 15-day refresh token. Fyers additionally requires the
// account's trading PIN, which must be present in additional_data["pin"].
TokenExchangeResponse FyersBroker::refresh_session(const BrokerCredentials& creds) {
    TokenExchangeResponse result;
    const auto extra = QJsonDocument::fromJson(creds.additional_data.toUtf8()).object();
    const QString pin = extra.value("pin").toString();
    if (creds.refresh_token.isEmpty() || pin.isEmpty()) {
        result.error = "Fyers silent refresh requires a stored refresh token and trading PIN";
        return result; // no HTTP — let the caller mark the session expired
    }

    const QByteArray input = (creds.api_key + ":" + creds.api_secret).toUtf8();
    const QString hash = QCryptographicHash::hash(input, QCryptographicHash::Sha256).toHex();
    QJsonObject payload{
        {"grant_type", "refresh_token"},
        {"appIdHash", hash},
        {"refresh_token", creds.refresh_token},
        {"pin", pin},
    };
    auto resp = BrokerHttp::instance().post_json(QString(base_url()) + "/api/v3/validate-refresh-token", payload);
    if (!resp.success || resp.json.value("s").toString() != "ok") {
        result.error = fyers_check_auth(resp, resp.json.value("message").toString("Refresh failed"));
        return result;
    }

    result.success = true;
    result.access_token = resp.json.value("access_token").toString();
    result.refresh_token = creds.refresh_token; // unchanged — valid for 15 days
    result.user_id = creds.user_id;
    result.additional_data = with_token_expiry(creds.additional_data, next_ist_flush_epoch(6, 0));
    LOG_INFO("Fyers", "Silent token refresh OK");
    return result;
}

// ============================================================================
// Order Type Mappings
// ============================================================================

const BrokerEnumMap<int>& FyersBroker::fyers_int_map() {
    static const auto m = [] {
        BrokerEnumMap<int> x;
        x.set(OrderType::Limit, 1);
        x.set(OrderType::Market, 2);
        x.set(OrderType::StopLoss, 3);
        x.set(OrderType::StopLossLimit, 4);
        x.set(OrderSide::Buy, 1);
        x.set(OrderSide::Sell, -1);
        return x;
    }();
    return m;
}

const BrokerEnumMap<QString>& FyersBroker::fyers_str_map() {
    static const auto m = [] {
        BrokerEnumMap<QString> x;
        x.set(ProductType::Intraday, "INTRADAY");
        x.set(ProductType::Delivery, "CNC");
        x.set(ProductType::Margin, "MARGIN");
        x.set(ProductType::MTF, "MTF");
        x.set(ProductType::CoverOrder, "CO");
        x.set(ProductType::BracketOrder, "BO");
        return x;
    }();
    return m;
}

// ============================================================================
// Place Order
// ============================================================================

OrderPlaceResponse FyersBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    QJsonObject payload{{"symbol", to_fyers_sym(order.symbol)},
                        {"qty", static_cast<int>(order.quantity)},
                        {"type", fyers_int_map().order_type_or(order.order_type, 2)},
                        {"side", fyers_int_map().side_or(order.side, 1)},
                        {"productType", fyers_str_map().product_or(order.product_type, "INTRADAY")},
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

    auto resp =
        BrokerHttp::instance().post_json(QString(base_url()) + "/api/v3/orders/sync", payload, auth_headers(creds));

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
    auto resp =
        BrokerHttp::instance().patch_json(QString(base_url()) + "/api/v3/orders/sync", payload, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() == "ok")
        return {true, resp.json, "", ts};
    return {false, std::nullopt, resp.json.value("message").toString("Modify failed"), ts};
}

ApiResponse<QJsonObject> FyersBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    QJsonObject payload{{"id", order_id}};
    auto resp = BrokerHttp::instance().del(QString(base_url()) + "/api/v3/orders/sync", auth_headers(creds), payload);
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
        return {false, std::nullopt, fyers_check_auth(resp, resp.error), ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, fyers_check_auth(resp, resp.json.value("message").toString("Failed")), ts};

    QVector<BrokerOrderInfo> orders;
    auto arr = resp.json.value("orderBook").toArray();
    for (const auto& v : arr) {
        auto o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("id").toString();
        const QString raw_order_sym = o.value("symbol").toString();
        info.symbol = from_fyers_sym(raw_order_sym);
        info.exchange = exchange_of(raw_order_sym, o.value("exchange").toInt(-1));
        info.side = o.value("side").toInt() == 1 ? "buy" : "sell";
        // Fyers v3 order type enum: 1=Limit, 2=Market, 3=StopMarket, 4=StopLimit
        static const QMap<int, QString> type_map = {
            {1, "LIMIT"}, {2, "MARKET"}, {3, "STOP"}, {4, "STOPLIMIT"}};
        info.order_type = type_map.value(o.value("type").toInt(), QString::number(o.value("type").toInt()));
        info.product_type = o.value("productType").toString();
        info.quantity = o.value("qty").toDouble();
        info.price = o.value("limitPrice").toDouble();
        info.trigger_price = o.value("stopPrice").toDouble();
        info.filled_qty = o.value("filledQty").toDouble();
        info.avg_price = o.value("tradedPrice").toDouble();
        // Fyers v3 order status enum: 1=Cancelled, 2=Traded/Filled, 3=Reserved,
        // 4=Transit, 5=Rejected, 6=Pending/Open, 7=Expired
        static const QMap<int, QString> status_map = {
            {1, "cancelled"}, {2, "complete"}, {3, "reserved"},
            {4, "transit"},   {5, "rejected"}, {6, "open"},      {7, "expired"}};
        info.status = status_map.value(o.value("status").toInt(), QString::number(o.value("status").toInt()));
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
        return {false, std::nullopt, fyers_check_auth(resp, resp.error), ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, fyers_check_auth(resp, resp.json.value("message").toString("Failed")), ts};

    QVector<BrokerPosition> positions;
    auto arr = resp.json.value("netPositions").toArray();
    for (const auto& v : arr) {
        auto p = v.toObject();
        BrokerPosition pos;
        const QString raw_pos_sym = p.value("symbol").toString();
        pos.symbol = from_fyers_sym(raw_pos_sym);
        pos.exchange = exchange_of(raw_pos_sym, p.value("exchange").toInt(-1));
        {
            // [posdiag] one-shot per symbol — verify position symbol matches the
            // WS quote symbol so option positions mark-to-market live.
            static QSet<QString> s_seen_pos;
            if (!s_seen_pos.contains(raw_pos_sym)) {
                s_seen_pos.insert(raw_pos_sym);
                LOG_INFO("FyersBroker", QString("[posdiag] raw='%1' → norm='%2' exch='%3'")
                                            .arg(raw_pos_sym, pos.symbol, pos.exchange));
            }
        }
        pos.product_type = p.value("productType").toString();
        pos.quantity = p.value("netQty").toDouble();
        pos.avg_price = p.value("netAvg").toDouble();
        pos.ltp = p.value("ltp").toDouble();
        pos.pnl = p.value("pl").toDouble();
        pos.pnl_pct = (pos.avg_price > 0.0) ? ((pos.ltp - pos.avg_price) / pos.avg_price) * 100.0 : 0.0;
        pos.side = pos.quantity >= 0 ? "buy" : "sell";
        positions.append(pos);
    }
    return {true, positions, "", ts};
}

ApiResponse<QVector<BrokerHolding>> FyersBroker::get_holdings(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/api/v3/holdings", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, fyers_check_auth(resp, resp.error), ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, fyers_check_auth(resp, resp.json.value("message").toString("Failed")), ts};

    QVector<BrokerHolding> holdings;
    auto arr = resp.json.value("holdings").toArray();
    for (const auto& v : arr) {
        auto h = v.toObject();
        BrokerHolding hold;
        const QString raw_hold_sym = h.value("symbol").toString();
        hold.symbol = from_fyers_sym(raw_hold_sym);
        hold.exchange = exchange_of(raw_hold_sym, h.value("exchange").toInt(-1));
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
        return {false, std::nullopt, fyers_check_auth(resp, resp.error), ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, fyers_check_auth(resp, resp.json.value("message").toString("Failed")), ts};

    BrokerFunds funds;
    auto fl = resp.json.value("fund_limit").toArray();
    // Fyers v3 fund_limit ids (stable; titles vary by tier/segment):
    //   1=Total Balance, 2=Utilized Amount, 3=Realized P&L, 4=Limit at start of day,
    //   5=Receivables, 6=Fund Transfer, 7=Adhoc Limit, 8=Notional Cash,
    //   9=Available Balance, 10=Clear Balance
    for (const auto& v : fl) {
        auto item = v.toObject();
        const int id = item.value("id").toInt(-1);
        const double equity = item.value("equityAmount").toDouble();
        const double commodity = item.value("commodityAmount").toDouble();
        const double total = equity + commodity;
        switch (id) {
            case 1:
                funds.total_balance = total;
                break;
            case 2:
                funds.used_margin = total;
                break;
            case 9:
            case 10:
                funds.available_balance = total;
                break;
        }
    }
    funds.raw_data = resp.json;
    return {true, funds, "", ts};
}

ApiResponse<QVector<BrokerQuote>> FyersBroker::get_quotes(const BrokerCredentials& creds,
                                                          const QVector<QString>& symbols) {
    // Fyers limits quote requests — batch in chunks of 50 to avoid URL length issues
    static constexpr int kBatchSize = 50;
    int64_t ts = now_ts();
    LOG_INFO("FyersBroker", QString("get_quotes: %1 symbols requested").arg(symbols.size()));

    QVector<BrokerQuote> all_quotes;
    for (int batch_start = 0; batch_start < symbols.size(); batch_start += kBatchSize) {
        int batch_end = qMin(batch_start + kBatchSize, symbols.size());
        // Reverse map: Fyers wire symbol → original caller symbol. Callers
        // (OptionChainService, equity screens) pass symbols in their own format
        // (e.g. "NSE:BANKNIFTY..." for F&O, "RELIANCE" for equity). We must
        // return q.symbol in the same format so lookups match.
        QHash<QString, QString> fyers_to_orig;
        QString syms;
        for (int i = batch_start; i < batch_end; ++i) {
            if (i > batch_start)
                syms += ",";
            const QString fyers_sym = to_fyers_sym(symbols[i]);
            syms += fyers_sym;
            fyers_to_orig.insert(fyers_sym, symbols[i]);
        }
        auto resp = BrokerHttp::instance().get(
            QString(base_url()) + "/data/quotes?symbols=" + syms, auth_headers(creds));
        if (!resp.success)
            return {false, std::nullopt, fyers_check_auth(resp, resp.error), ts};
        if (resp.json.value("s").toString() != "ok")
            return {false, std::nullopt, fyers_check_auth(resp, resp.json.value("message").toString("Failed")), ts};

        auto d_arr = resp.json.value("d").toArray();
        for (const auto& val : d_arr) {
            auto item = val.toObject();
            if (item.value("s").toString() != "ok") {
                auto v = item.value("v").toObject();
                LOG_WARN("FyersBroker", QString("get_quotes: symbol '%1' error: %2")
                                            .arg(item.value("n").toString(), v.value("errmsg").toString()));
                continue;
            }
            auto v = item.value("v").toObject();
            BrokerQuote q;
            const QString raw_sym = v.value("symbol").toString();
            q.symbol = fyers_to_orig.value(raw_sym, from_fyers_sym(raw_sym));
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
            q.timestamp = v.value("tt").toVariant().toLongLong() * 1000;
            q.oi = v.value("oi").toVariant().toLongLong();
            q.oi_change_pct = v.value("oichp").toDouble();
            all_quotes.append(q);
        }
    } // end batch loop
    LOG_INFO("FyersBroker", QString("get_quotes: returning %1 quotes (requested %2)")
                                .arg(all_quotes.size()).arg(symbols.size()));
    return {true, all_quotes, "", ts};
}

ApiResponse<QVector<BrokerCandle>> FyersBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                            const QString& resolution, const QString& from_date,
                                                            const QString& to_date) {
    // Map chart timeframes to Fyers resolution codes
    static const QHash<QString, QString> tf_map = {
        {"1m", "1"}, {"5m", "5"}, {"15m", "15"}, {"30m", "30"},
        {"1h", "60"}, {"1d", "D"}, {"1w", "1W"},
    };
    const QString fyers_res = tf_map.value(resolution, resolution);

    // Fyers date_format=1 expects dates without time: "yyyy-MM-dd"
    const QString from = from_date.left(10);
    const QString to = to_date.left(10);

    int64_t ts = now_ts();

    // Fyers /data/history caps the date window per request. Daily/weekly/monthly
    // resolutions allow up to 366 days; all intraday minute resolutions allow up
    // to 100 days. Determine the cap from the resolved Fyers resolution code.
    static const QSet<QString> daily_res = {"D", "1W", "1M"};
    const int cap_days = daily_res.contains(fyers_res) ? 366 : 100;

    const QString fyers_sym = to_fyers_sym(symbol);

    // Fetch + parse a single window. Returns true on a successful response and
    // appends parsed candles to `out`; on failure sets `err` and returns false.
    auto fetch_window = [&](const QString& range_from, const QString& range_to,
                            QVector<BrokerCandle>& out, QString& err) -> bool {
        const QString url =
            QString("%1/data/history?symbol=%2&resolution=%3&date_format=1&range_from=%4&range_to=%5&cont_flag=1")
                .arg(base_url(), fyers_sym, fyers_res, range_from, range_to);

        auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
        if (!resp.success) {
            err = fyers_check_auth(resp, resp.error);
            return false;
        }
        if (resp.json.value("s").toString() != "ok") {
            err = fyers_check_auth(resp, resp.json.value("message").toString("Failed"));
            return false;
        }

        const auto arr = resp.json.value("candles").toArray();
        for (const auto& v : arr) {
            auto c = v.toArray();
            if (c.size() < 6)
                continue;
            BrokerCandle candle;
            // Fyers returns epoch in seconds; internal convention is milliseconds.
            candle.timestamp = c[0].toVariant().toLongLong() * 1000;
            candle.open = c[1].toDouble();
            candle.high = c[2].toDouble();
            candle.low = c[3].toDouble();
            candle.close = c[4].toDouble();
            candle.volume = c[5].toDouble();
            out.append(candle);
        }
        return true;
    };

    const QDate from_d = QDate::fromString(from, "yyyy-MM-dd");
    const QDate to_d = QDate::fromString(to, "yyyy-MM-dd");

    // If parsing fails or the range fits in one window, preserve the original
    // single-request behavior.
    if (!from_d.isValid() || !to_d.isValid() || from_d.daysTo(to_d) <= cap_days) {
        QVector<BrokerCandle> candles;
        QString err;
        if (!fetch_window(from, to, candles, err))
            return {false, std::nullopt, err, ts};
        return {true, candles, "", ts};
    }

    // Range exceeds the cap: chunk into consecutive <=cap-day sub-windows.
    QVector<BrokerCandle> candles;
    QDate win_from = from_d;
    constexpr int kMaxIterations = 60;
    int iterations = 0;
    bool first_window = true;
    while (win_from <= to_d && iterations < kMaxIterations) {
        ++iterations;
        QDate win_to = win_from.addDays(cap_days);
        if (win_to > to_d)
            win_to = to_d;

        QString err;
        if (!fetch_window(win_from.toString("yyyy-MM-dd"), win_to.toString("yyyy-MM-dd"), candles, err)) {
            // First-window failure surfaces the error as before. A later failure
            // after we already collected data returns the partial result.
            if (first_window)
                return {false, std::nullopt, err, ts};
            LOG_WARN("FyersBroker",
                     QString("get_history: window fetch failed after partial data (%1); returning %2 candles")
                         .arg(err).arg(candles.size()));
            break;
        }
        first_window = false;
        win_from = win_to.addDays(1);
    }

    // Sort ascending by timestamp and dedupe consecutive duplicate timestamps
    // (overlapping window boundaries / repeated bars).
    std::sort(candles.begin(), candles.end(),
              [](const BrokerCandle& a, const BrokerCandle& b) { return a.timestamp < b.timestamp; });
    candles.erase(std::unique(candles.begin(), candles.end(),
                              [](const BrokerCandle& a, const BrokerCandle& b) {
                                  return a.timestamp == b.timestamp;
                              }),
                  candles.end());

    return {true, candles, "", ts};
}

// ============================================================================
// Market Depth — GET /data/depth?symbol=...&ohlcv_flag=1
// Returns 5-level bid/ask depth. Mapped into BrokerQuote objects so
// AccountDataStream::fetch_orderbook() can aggregate them via its L2 path.
// Note: Fyers uses "bids" (plural) for buy side, "ask" (singular) for sell side.
// ============================================================================

ApiResponse<QVector<BrokerQuote>> FyersBroker::get_historical_quotes_single(
    const BrokerCredentials& creds, const QString& symbol,
    const QString& /*start*/, const QString& /*end*/, int /*limit*/) {

    const QString url = QString("%1/data/depth?symbol=%2&ohlcv_flag=1")
                            .arg(base_url(), to_fyers_sym(symbol));
    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, fyers_check_auth(resp, resp.error), ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, fyers_check_auth(resp, resp.json.value("message").toString("Failed")), ts};

    // d is a map keyed by symbol: { "NSE:SBIN-EQ": { bids: [...], ask: [...], ... } }
    const auto d = resp.json.value("d").toObject();
    if (d.isEmpty())
        return {false, std::nullopt, "Empty depth response", ts};

    const auto sym_data = d.begin()->toObject();
    const auto bids_arr = sym_data.value("bids").toArray();
    const auto asks_arr = sym_data.value("ask").toArray();

    const QString norm_sym = from_fyers_sym(d.begin().key());
    LOG_INFO("FyersBroker", QString("depth: %1 bids, %2 asks for %3")
                                .arg(bids_arr.size()).arg(asks_arr.size()).arg(norm_sym));

    QVector<BrokerQuote> quotes;
    for (const auto& v : bids_arr) {
        auto lvl = v.toObject();
        BrokerQuote q;
        q.symbol = norm_sym;
        q.bid = lvl.value("price").toDouble();
        q.bid_size = lvl.value("volume").toDouble();
        q.oi = lvl.value("ord").toInt();
        quotes.append(q);
    }
    for (const auto& v : asks_arr) {
        auto lvl = v.toObject();
        BrokerQuote q;
        q.symbol = norm_sym;
        q.ask = lvl.value("price").toDouble();
        q.ask_size = lvl.value("volume").toDouble();
        q.oi = lvl.value("ord").toInt();
        quotes.append(q);
    }
    return {true, quotes, "", ts};
}

// ============================================================================
// Market Clock — GET /api/v3/marketStatus
// Response: {s:"ok", marketStatus:[{exchange, segment, status, msg}, ...]}
// We surface a coarse boolean is_open (true if ANY segment is open) and let
// callers inspect raw via subsequent queries if they need per-segment detail.
// ============================================================================

ApiResponse<MarketClock> FyersBroker::get_clock(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/api/v3/marketStatus", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("marketStatus failed"), ts};
    MarketClock clk;
    clk.timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    const auto arr = resp.json.value("marketStatus").toArray();
    for (const auto& v : arr) {
        const QString status = v.toObject().value("status").toString().toUpper();
        if (status == "OPEN" || status == "MARKET_OPEN") {
            clk.is_open = true;
            break;
        }
    }
    return {true, clk, "", ts};
}

// ============================================================================
// GTT — /api/v3/gtt/orders[/sync]
// Fyers GTT body shape:
//   { side, productType, symbol, qty, type, limitPrice, stopPrice,
//     orderInfo: { leg1: { price, qty }, leg2: { ... } } }
// type values: 1=Single, 2=OCO (two-leg). Triggers and limits map to leg1/leg2
// for OCO; Single uses the top-level fields.
// ============================================================================

namespace {

// Local product-string mapping that doesn't depend on the private static.
static QString fyers_product_str(ProductType p) {
    switch (p) {
        case ProductType::Intraday:     return "INTRADAY";
        case ProductType::Delivery:     return "CNC";
        case ProductType::Margin:       return "MARGIN";
        case ProductType::MTF:          return "MTF";
        case ProductType::CoverOrder:   return "CO";
        case ProductType::BracketOrder: return "BO";
    }
    return "INTRADAY";
}

QJsonObject fyers_gtt_body(const GttOrder& g) {
    QJsonObject body;
    body["symbol"] = to_fyers_sym(g.symbol);
    body["productType"] = fyers_product_str(
        g.triggers.isEmpty() ? ProductType::Delivery : g.triggers.first().product);
    body["side"] = (g.triggers.isEmpty() || g.triggers.first().side == OrderSide::Buy) ? 1 : -1;
    body["type"] = (g.type == GttOrderType::OCO) ? 2 : 1;
    if (g.triggers.isEmpty())
        return body;
    body["qty"] = static_cast<int>(g.triggers.first().quantity);

    if (g.type == GttOrderType::OCO && g.triggers.size() > 1) {
        QJsonObject leg1{{"price", g.triggers[0].trigger_price},
                         {"qty", static_cast<int>(g.triggers[0].quantity)}};
        QJsonObject leg2{{"price", g.triggers[1].trigger_price},
                         {"qty", static_cast<int>(g.triggers[1].quantity)}};
        QJsonObject info;
        info["leg1"] = leg1;
        info["leg2"] = leg2;
        body["orderInfo"] = info;
    } else {
        body["stopPrice"] = g.triggers[0].trigger_price;
        body["limitPrice"] = g.triggers[0].limit_price;
    }
    return body;
}

GttOrder fyers_parse_gtt(const QJsonObject& o) {
    GttOrder g;
    g.gtt_id = o.value("id").toString();
    if (g.gtt_id.isEmpty())
        g.gtt_id = QString::number(o.value("id").toInt());
    const QString raw_gtt_sym = o.value("symbol").toString();
    g.symbol = from_fyers_sym(raw_gtt_sym);
    g.exchange = exchange_of(raw_gtt_sym);
    g.status = o.value("status").toString().toLower();
    g.created_at = o.value("createdAt").toString();
    g.updated_at = o.value("updatedAt").toString();
    g.type = o.value("type").toInt() == 2 ? GttOrderType::OCO : GttOrderType::Single;
    g.last_price = o.value("lastPrice").toDouble();

    const int side = o.value("side").toInt();
    const auto side_enum = side == 1 ? OrderSide::Buy : OrderSide::Sell;
    const auto info = o.value("orderInfo").toObject();
    if (g.type == GttOrderType::OCO && info.contains("leg1")) {
        const auto l1 = info.value("leg1").toObject();
        const auto l2 = info.value("leg2").toObject();
        GttTrigger t0;
        t0.trigger_price = l1.value("price").toDouble();
        t0.quantity = l1.value("qty").toDouble();
        t0.side = side_enum;
        g.triggers.append(t0);
        GttTrigger t1;
        t1.trigger_price = l2.value("price").toDouble();
        t1.quantity = l2.value("qty").toDouble();
        t1.side = side_enum;
        g.triggers.append(t1);
    } else {
        GttTrigger t0;
        t0.trigger_price = o.value("stopPrice").toDouble();
        t0.limit_price = o.value("limitPrice").toDouble();
        t0.quantity = o.value("qty").toDouble();
        t0.side = side_enum;
        g.triggers.append(t0);
    }
    return g;
}

} // anonymous namespace

GttPlaceResponse FyersBroker::gtt_place(const BrokerCredentials& creds, const GttOrder& order) {
    QJsonObject body = fyers_gtt_body(order);
    auto resp = BrokerHttp::instance().post_json(QString(base_url()) + "/api/v3/gtt/orders/sync", body,
                                                 auth_headers(creds));
    GttPlaceResponse out;
    if (!resp.success) {
        out.error = resp.error;
        return out;
    }
    if (resp.json.value("s").toString() != "ok") {
        out.error = resp.json.value("message").toString("GTT place failed");
        return out;
    }
    out.success = true;
    out.gtt_id = resp.json.value("id").toString();
    if (out.gtt_id.isEmpty())
        out.gtt_id = QString::number(resp.json.value("id").toInt());
    return out;
}

ApiResponse<GttOrder> FyersBroker::gtt_get(const BrokerCredentials& creds, const QString& gtt_id) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/api/v3/gtt/orders?id=" + gtt_id, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("GTT get failed"), ts};
    // Endpoint returns an array even for a single id; pick the matching entry.
    for (const auto& v : resp.json.value("orderBook").toArray()) {
        auto o = v.toObject();
        if (o.value("id").toString() == gtt_id || QString::number(o.value("id").toInt()) == gtt_id)
            return {true, fyers_parse_gtt(o), "", ts};
    }
    return {false, std::nullopt, "GTT id not found", ts};
}

ApiResponse<QVector<GttOrder>> FyersBroker::gtt_list(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/api/v3/gtt/orders", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("GTT list failed"), ts};
    QVector<GttOrder> out;
    for (const auto& v : resp.json.value("orderBook").toArray())
        out.append(fyers_parse_gtt(v.toObject()));
    return {true, out, "", ts};
}

ApiResponse<GttOrder> FyersBroker::gtt_modify(const BrokerCredentials& creds, const QString& gtt_id,
                                              const GttOrder& updated) {
    QJsonObject body = fyers_gtt_body(updated);
    body["id"] = gtt_id;
    auto resp = BrokerHttp::instance().patch_json(QString(base_url()) + "/api/v3/gtt/orders/sync", body,
                                                  auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("GTT modify failed"), ts};
    // Re-fetch to get the modified state.
    return gtt_get(creds, gtt_id);
}

ApiResponse<QJsonObject> FyersBroker::gtt_cancel(const BrokerCredentials& creds, const QString& gtt_id) {
    QJsonObject body{{"id", gtt_id}};
    auto resp = BrokerHttp::instance().del(QString(base_url()) + "/api/v3/gtt/orders/sync", auth_headers(creds), body);
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("GTT cancel failed"), ts};
    return {true, resp.json, "", ts};
}

// ============================================================================
// Multi-Order — POST /api/v3/multi-order/sync
// Body: array of order objects (same shape as single place_order). Up to 10 legs.
// ============================================================================

ApiResponse<QJsonObject> FyersBroker::place_multi_order(const BrokerCredentials& creds,
                                                       const QVector<UnifiedOrder>& orders) {
    int64_t ts = now_ts();
    if (orders.isEmpty())
        return {false, std::nullopt, "place_multi_order: empty order list", ts};
    if (orders.size() > 10)
        return {false, std::nullopt, "place_multi_order: max 10 legs per batch", ts};

    QJsonArray payload;
    for (const auto& order : orders) {
        QJsonObject leg{{"symbol", to_fyers_sym(order.symbol)},
                        {"qty", static_cast<int>(order.quantity)},
                        {"type", fyers_int_map().order_type_or(order.order_type, 2)},
                        {"side", fyers_int_map().side_or(order.side, 1)},
                        {"productType", fyers_str_map().product_or(order.product_type, "INTRADAY")},
                        {"validity", order.validity},
                        {"offlineOrder", order.amo}};
        if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit)
            leg["limitPrice"] = order.price;
        if (order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit)
            leg["stopPrice"] = order.stop_price;
        payload.append(leg);
    }
    auto resp = BrokerHttp::instance().post_json_array(
        QString(base_url()) + "/api/v3/multi-order/sync", payload, auth_headers(creds));
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, resp.json.value("message").toString("multi-order failed"), ts};
    return {true, resp.json, "", ts};
}

// ============================================================================
// Multi-Quote — POST /data-rest/v2/quotes/
// Body: {"symbols": "NSE:SBIN-EQ,NSE:TCS-EQ"} (comma-separated, max 50)
// Response: {s:"ok", d:[{s:"ok", n:"NSE:SBIN-EQ", v:{lp, open_price, ...}}, ...]}
// ============================================================================

ApiResponse<QVector<BrokerQuote>> FyersBroker::get_multi_quotes(
    const BrokerCredentials& creds,
    const QVector<QPair<QString, QString>>& symbols) {

    static constexpr int kBatchSize = 50;
    int64_t ts = now_ts();
    QVector<BrokerQuote> all_quotes;

    for (int batch_start = 0; batch_start < symbols.size(); batch_start += kBatchSize) {
        int batch_end = qMin(batch_start + kBatchSize, symbols.size());

        // Build comma-separated Fyers symbol list and reverse map.
        // If exchange is provided and symbol lacks ':', prepend exchange so
        // to_fyers_sym sees a fully qualified "EXCH:SYMBOL" and passes it through.
        // Plain symbols without exchange default to NSE:-EQ via to_fyers_sym.
        QHash<QString, QPair<QString, QString>> fyers_to_orig;
        QStringList sym_list;
        for (int i = batch_start; i < batch_end; ++i) {
            const auto& [sym, exch] = symbols[i];
            QString input = sym;
            if (!sym.contains(':') && !exch.isEmpty()) {
                // Fyers EQ symbols use -EQ suffix; F&O already have segment in name
                input = exch + ":" + sym;
                if (!sym.contains('-'))
                    input += "-EQ";
            }
            const QString fyers_sym = to_fyers_sym(input);
            sym_list.append(fyers_sym);
            fyers_to_orig.insert(fyers_sym, symbols[i]);
        }

        QJsonObject payload;
        payload["symbols"] = sym_list.join(",");

        auto resp = BrokerHttp::instance().post_json(
            QString(base_url()) + "/data-rest/v2/quotes/", payload, auth_headers(creds));

        if (!resp.success)
            return {false, std::nullopt, fyers_check_auth(resp, resp.error), ts};
        if (resp.json.value("s").toString() != "ok")
            return {false, std::nullopt, fyers_check_auth(resp, resp.json.value("message").toString("Failed")), ts};

        auto d_arr = resp.json.value("d").toArray();
        for (const auto& val : d_arr) {
            auto item = val.toObject();
            if (item.value("s").toString() != "ok")
                continue;
            auto v = item.value("v").toObject();
            BrokerQuote q;
            const QString raw_sym = v.value("symbol").toString();
            if (fyers_to_orig.contains(raw_sym)) {
                const auto& orig = fyers_to_orig.value(raw_sym);
                q.symbol = orig.first;
            } else {
                q.symbol = from_fyers_sym(raw_sym);
            }
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
            q.timestamp = v.value("tt").toVariant().toLongLong() * 1000;
            q.oi = v.value("oi").toVariant().toLongLong();
            q.oi_change_pct = v.value("oichp").toDouble();
            all_quotes.append(q);
        }
    }

    return {true, all_quotes, "", ts};
}

// ============================================================================
// Market Depth — GET /data/depth?symbol=...&ohlcv_flag=1
// Returns 5-level bid/ask depth. Reuses the same endpoint as
// get_historical_quotes_single but maps into MarketDepth struct.
// ============================================================================

ApiResponse<MarketDepth> FyersBroker::get_market_depth(
    const BrokerCredentials& creds,
    const QString& symbol, const QString& exchange) {

    Q_UNUSED(exchange);
    const QString url = QString("%1/data/depth?symbol=%2&ohlcv_flag=1")
                            .arg(base_url(), to_fyers_sym(symbol));
    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
    int64_t ts = now_ts();

    if (!resp.success)
        return {false, std::nullopt, fyers_check_auth(resp, resp.error), ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt, fyers_check_auth(resp, resp.json.value("message").toString("Failed")), ts};

    // d is a map keyed by symbol: { "NSE:SBIN-EQ": { bids: [...], ask: [...], ... } }
    const auto d = resp.json.value("d").toObject();
    if (d.isEmpty())
        return {false, std::nullopt, "Empty depth response", ts};

    const auto sym_data = d.begin()->toObject();

    MarketDepth md;
    md.symbol = symbol;
    md.exchange = exchange.isEmpty() ? exchange_of(d.begin().key()) : exchange;
    md.ltp = sym_data.value("ltp").toDouble();
    md.volume = sym_data.value("volume").toDouble();
    md.oi = sym_data.value("oi").toDouble();

    const auto bids_arr = sym_data.value("bids").toArray();
    for (const auto& v : bids_arr) {
        auto lvl = v.toObject();
        DepthLevel dl;
        dl.price = lvl.value("price").toDouble();
        dl.quantity = lvl.value("volume").toInt();
        dl.orders = lvl.value("ord").toInt();
        md.bids.append(dl);
    }

    // Fyers uses "ask" (singular) for sell side
    const auto asks_arr = sym_data.value("ask").toArray();
    for (const auto& v : asks_arr) {
        auto lvl = v.toObject();
        DepthLevel dl;
        dl.price = lvl.value("price").toDouble();
        dl.quantity = lvl.value("volume").toInt();
        dl.orders = lvl.value("ord").toInt();
        md.asks.append(dl);
    }

    return {true, md, "", ts};
}

// ============================================================================
// Pre-trade margin calculator — POST /api/v3/multiorder/margin  (native)
// Mirrors OpenAlgo broker/fyers/api/margin_api.py + mapping/margin_data.py.
// Payload: {"data":[{symbol, qty, side, type, productType, limitPrice,
//                    stopLoss, stopPrice, takeProfit}]}
// Response: {"s":"ok","data":{margin_avail, margin_total, margin_new_order}}
// Fyers returns total margin only (no SPAN/Exposure breakdown).
// ============================================================================
ApiResponse<OrderMargin> FyersBroker::get_order_margins(const BrokerCredentials& creds, const UnifiedOrder& order) {
    int64_t ts = now_ts();

    QJsonObject leg{{"symbol", to_fyers_sym(order.symbol)},
                    {"qty", static_cast<int>(order.quantity)},
                    {"side", fyers_int_map().side_or(order.side, 1)},
                    {"type", fyers_int_map().order_type_or(order.order_type, 2)},
                    {"productType", fyers_str_map().product_or(order.product_type, "INTRADAY")},
                    {"limitPrice", order.price},
                    {"stopLoss", 0.0},
                    {"stopPrice", order.stop_price},
                    {"takeProfit", 0.0}};

    QJsonObject payload{{"data", QJsonArray{leg}}};

    auto resp = BrokerHttp::instance().post_json(QString(base_url()) + "/api/v3/multiorder/margin", payload,
                                                 auth_headers(creds));
    if (!resp.success)
        return {false, std::nullopt, fyers_check_auth(resp, "Network error"), ts};
    if (resp.json.value("s").toString() != "ok")
        return {false, std::nullopt,
                fyers_check_auth(resp, resp.json.value("message").toString("Margin calculation failed")), ts};

    const auto data = resp.json.value("data").toObject();

    OrderMargin m;
    m.symbol = order.symbol;
    m.exchange = order.exchange;
    m.side = order.side == OrderSide::Buy ? "BUY" : "SELL";
    m.quantity = order.quantity;
    m.price = order.price;
    // margin_new_order = total margin including existing positions (best total estimate).
    m.total = data.value("margin_new_order").toDouble();
    if (m.total <= 0.0)
        m.total = data.value("margin_total").toDouble();
    m.cash = m.total; // no SPAN/Exposure breakdown from Fyers

    const double notional = order.price * order.quantity;
    if (m.total > 0.0 && notional > 0.0)
        m.leverage = notional / m.total;
    return {true, m, "", ts};
}

} // namespace fincept::trading
