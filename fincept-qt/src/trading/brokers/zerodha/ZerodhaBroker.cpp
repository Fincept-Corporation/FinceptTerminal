#include "trading/brokers/zerodha/ZerodhaBroker.h"

#include "core/logging/Logger.h"
#include "trading/adapter/BrokerEnumMap.h"
#include "trading/brokers/BrokerHttp.h"
#include "trading/brokers/BrokerTokenUtil.h"
#include "trading/brokers/zerodha/ZerodhaAutoLogin.h"
#include "trading/instruments/InstrumentService.h"

#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::trading {

QString ZerodhaBroker::kite_login_url(const QString& api_key) {
    return QStringLiteral("https://kite.zerodha.com/connect/login?v=3&api_key=%1").arg(api_key);
}

TokenExchangeResponse ZerodhaBroker::login_with_totp(const QString& user_id, const QString& password,
                                                    const QString& api_key, const QString& api_secret,
                                                    const QString& totp_secret,
                                                    std::function<void(const QString&)> progress) {
    auto result = zerodha::run_auto_login(user_id, password, api_key, api_secret, totp_secret,
                                          std::move(progress));
    // Kite access tokens are flushed each morning (~06:00 IST). Record the
    // expiry hint; the live sweep re-validates and silent-relogins on expiry.
    if (result.token.success)
        result.token.additional_data =
            with_token_expiry(result.token.additional_data, next_ist_flush_epoch(6, 0));
    return result.token;
}

// Silent refresh = replay the stored TOTP auto-login (Kite has no usable refresh
// token for retail). Requires user_id + password + totp_secret in storage.
TokenExchangeResponse ZerodhaBroker::refresh_session(const BrokerCredentials& creds) {
    const auto extra = QJsonDocument::fromJson(creds.additional_data.toUtf8()).object();
    const QString password = extra.value("password").toString();
    const QString totp_secret = extra.value("totp_secret").toString();
    if (creds.user_id.isEmpty() || password.isEmpty() || totp_secret.isEmpty()) {
        return {false, "", "", "", "",
                "Zerodha silent refresh requires stored user id, password and TOTP secret"};
    }
    return login_with_totp(creds.user_id, password, creds.api_key, creds.api_secret, totp_secret, {});
}

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

QMap<QString, QString> ZerodhaBroker::auth_headers(const BrokerCredentials& creds) const {
    return {{"Authorization", "token " + creds.api_key + ":" + creds.access_token},
            {"X-Kite-Version", kite_api_version}};
}

const BrokerEnumMap<QString>& ZerodhaBroker::kite_enum_map() {
    static const auto m = [] {
        BrokerEnumMap<QString> x;
        // Order types
        x.set(OrderType::Market, "MARKET");
        x.set(OrderType::Limit, "LIMIT");
        x.set(OrderType::StopLoss, "SL-M");
        x.set(OrderType::StopLossLimit, "SL");
        // Sides
        x.set(OrderSide::Buy, "BUY");
        x.set(OrderSide::Sell, "SELL");
        // Products
        x.set(ProductType::Intraday, "MIS");
        x.set(ProductType::Delivery, "CNC");
        x.set(ProductType::Margin, "NRML");
        x.set(ProductType::MTF, "MTF"); // equities only
        // Cover/Bracket → variety (handled separately by zerodha_variety)
        return x;
    }();
    return m;
}
// Kite varieties: regular, amo, co, iceberg, auction.
// Bracket Order (bo) was retired by SEBI and is no longer accepted — orders
// with variety=bo come back rejected. Map BracketOrder → regular and rely on
// GTT for stop-loss/target legs instead.
const char* ZerodhaBroker::zerodha_variety(ProductType p) {
    switch (p) {
        case ProductType::CoverOrder:
            return "co";
        case ProductType::BracketOrder:
            return "regular"; // BO deprecated; caller should use GTT for OCO
        default:
            return "regular";
    }
}

// Kite Connect v3 supports: minute, 3minute, 5minute, 10minute, 15minute,
// 30minute, 60minute, day, week. 2hour / 3day are NOT accepted (older docs
// listed them; current API rejects them with 400 invalid_input).
QString ZerodhaBroker::zerodha_interval(const QString& resolution) {
    static const QMap<QString, QString> map = {
        {"1", "minute"},    {"1m", "minute"},    {"3", "3minute"},    {"3m", "3minute"},   {"5", "5minute"},
        {"5m", "5minute"},  {"10", "10minute"},  {"10m", "10minute"}, {"15", "15minute"},  {"15m", "15minute"},
        {"30", "30minute"}, {"30m", "30minute"}, {"60", "60minute"},  {"60m", "60minute"}, {"1h", "60minute"},
        {"D", "day"},       {"1D", "day"},       {"day", "day"},      {"W", "week"},       {"1W", "week"},
        {"week", "week"},
        // Aliases that Kite rejects — coerce to the nearest supported interval.
        {"2h", "60minute"}, {"2H", "60minute"},
        {"3d", "day"},      {"3D", "day"},
    };
    return map.value(resolution, QString());
}

bool ZerodhaBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 403)
        return true;
    // Zerodha also returns 200 with error_type "TokenException" in some cases
    if (resp.json.value("error_type").toString() == "TokenException")
        return true;
    return false;
}

// Returns the error message, prefixed "[TOKEN_EXPIRED] " when the token has expired.
// Covers both JSON-body errors (status != success) and network/HTTP errors (!resp.success).
QString ZerodhaBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    // Prefer message from JSON body; fall back to network-level error or caller's fallback
    QString msg = resp.json.value("message").toString(resp.error.isEmpty() ? fallback : resp.error);
    if (is_token_expired(resp))
        return "[TOKEN_EXPIRED] " + msg;
    return msg;
}

TokenExchangeResponse ZerodhaBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                    const QString& request_token) {
    QByteArray input = (api_key + request_token + api_secret).toUtf8();
    QString checksum = QCryptographicHash::hash(input, QCryptographicHash::Sha256).toHex();

    LOG_INFO("Zerodha", QString("exchange_token: api_key=%1 req_token_len=%2 secret_len=%3")
                            .arg(api_key).arg(request_token.length()).arg(api_secret.length()));

    QMap<QString, QString> params = {{"api_key", api_key}, {"request_token", request_token}, {"checksum", checksum}};
    QMap<QString, QString> headers = {{"X-Kite-Version", kite_api_version}};
    auto resp = BrokerHttp::instance().post_form(QString(base_url()) + "/session/token", params, headers);

    LOG_INFO("Zerodha", QString("exchange_token HTTP %1 success=%2 body=%3")
                            .arg(resp.status_code).arg(resp.success).arg(resp.raw_body.left(500)));

    TokenExchangeResponse result;
    if (!resp.success) {
        // Surface Kite's error_type + message when available.
        const QString kite_msg = resp.json.value("message").toString();
        const QString kite_type = resp.json.value("error_type").toString();
        if (!kite_msg.isEmpty())
            result.error = kite_type.isEmpty() ? kite_msg : QString("%1: %2").arg(kite_type, kite_msg);
        else
            result.error = resp.error.isEmpty() ? QString("HTTP %1").arg(resp.status_code) : resp.error;
        LOG_ERROR("Zerodha", QString("exchange_token failed: %1").arg(result.error));
        return result;
    }
    if (resp.json.value("status").toString() == "success") {
        auto data = resp.json.value("data").toObject();
        result.success = true;
        result.access_token = data.value("access_token").toString();
        result.refresh_token = data.value("refresh_token").toString();
        result.user_id = data.value("user_id").toString();
        result.additional_data = with_token_expiry(result.additional_data, next_ist_flush_epoch(6, 0));
        LOG_INFO("Zerodha", QString("exchange_token ok: user_id=%1").arg(result.user_id));
    } else {
        result.error = resp.json.value("message").toString("Token exchange failed");
        LOG_ERROR("Zerodha", QString("exchange_token status!=success: %1").arg(result.error));
    }
    return result;
}

OrderPlaceResponse ZerodhaBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    // Variety is derived from order flags, not ProductType. AMO orders take
    // variety="amo" regardless of MIS/CNC/NRML; CoverOrder takes "co".
    // Iceberg/Auction would need new UnifiedOrder flags — not wired yet.
    QString variety = zerodha_variety(order.product_type);
    if (order.amo)
        variety = "amo";
    QMap<QString, QString> params = {
        {"tradingsymbol", order.symbol},
        {"exchange", order.exchange},
        {"transaction_type", kite_enum_map().side_or(order.side, "BUY")},
        {"order_type", kite_enum_map().order_type_or(order.order_type, "MARKET")},
        {"quantity", QString::number(static_cast<int>(order.quantity))},
        {"product", kite_enum_map().product_or(order.product_type, "MIS")},
        {"validity", order.validity.isEmpty() ? "DAY" : order.validity},
        {"disclosed_quantity", "0"},
        {"tag", "fincept"},
    };
    if (order.price > 0)
        params["price"] = QString::number(order.price, 'f', 2);
    if (order.stop_price > 0)
        params["trigger_price"] = QString::number(order.stop_price, 'f', 2);

    auto resp =
        BrokerHttp::instance().post_form(QString("%1/orders/%2").arg(base_url(), variety), params, auth_headers(creds));

    OrderPlaceResponse result;
    if (!resp.success || resp.json.value("status").toString() != "success") {
        result.error = checked_error(resp, "Order placement failed");
        return result;
    }
    result.success = true;
    result.order_id = resp.json.value("data").toObject().value("order_id").toString();
    return result;
}

ApiResponse<QJsonObject> ZerodhaBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                     const QJsonObject& mods) {
    QString variety = mods.value("variety").toString("regular");
    QMap<QString, QString> params;
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
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "Modify failed"), ts};
    return {true, resp.json, "", ts};
}

ApiResponse<QJsonObject> ZerodhaBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
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
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "Cancel failed"), ts};
    return {true, resp.json, "", ts};
}

ApiResponse<QVector<BrokerOrderInfo>> ZerodhaBroker::get_orders(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/orders", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, resp.error), ts};
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
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/trades", auth_headers(creds));
    int64_t ts = now_ts();
    return resp.success ? ApiResponse<QJsonObject>{true, resp.json, "", ts}
                        : ApiResponse<QJsonObject>{false, std::nullopt, checked_error(resp, resp.error), ts};
}

ApiResponse<QVector<BrokerPosition>> ZerodhaBroker::get_positions(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/portfolio/positions", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, resp.error), ts};
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
        pos.day_pnl = p.value("m2m_pnl").toDouble();
        pos.side = pos.quantity >= 0 ? "BUY" : "SELL";
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
        return {false, std::nullopt, checked_error(resp, resp.error), ts};
    QVector<BrokerHolding> holdings;
    auto arr = resp.json.value("data").toArray();
    for (const auto& v : arr) {
        auto h = v.toObject();
        BrokerHolding hold;
        hold.symbol = h.value("tradingsymbol").toString();
        hold.exchange = h.value("exchange").toString();
        // Total holding = settled (T+2) demat `quantity` + `t1_quantity` (bought,
        // not yet settled). Using only `quantity` under-counts a stock bought in the
        // last 1–2 sessions and makes qty/invested/value disagree with the Kite app.
        hold.quantity = h.value("quantity").toDouble() + h.value("t1_quantity").toDouble();
        hold.avg_price = h.value("average_price").toDouble();
        hold.ltp = h.value("last_price").toDouble();
        hold.invested_value = hold.quantity * hold.avg_price;
        hold.current_value = hold.quantity * hold.ltp;
        hold.pnl = h.value("pnl").toDouble();
        hold.pnl_pct = hold.avg_price > 0 ? ((hold.ltp - hold.avg_price) / hold.avg_price) * 100.0 : 0.0;
        // [TEMP DEBUG] Dump the full raw Zerodha holding object so we can diagnose the
        // average_price mismatch between the Kite app and the terminal. Remove after diagnosis.
        LOG_INFO("Zerodha", QString("HOLDINGS_RAW %1: %2")
                                .arg(hold.symbol,
                                     QString::fromUtf8(QJsonDocument(h).toJson(QJsonDocument::Compact))));
        holdings.append(hold);
    }
    return {true, holdings, "", ts};
}

ApiResponse<BrokerFunds> ZerodhaBroker::get_funds(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/user/margins", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, resp.error), ts};
    auto data = resp.json.value("data").toObject();
    auto equity = data.value("equity").toObject();
    auto commod = data.value("commodity").toObject();
    BrokerFunds funds;
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
    QString query;
    for (const auto& sym : symbols) {
        if (!query.isEmpty())
            query += "&i=";
        query += sym.contains(':') ? sym : ("NSE:" + sym);
    }
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/quote?i=" + query, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, resp.error), ts};
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
        q.change_pct = q.close > 0 ? (q.change / q.close) * 100.0 : 0.0;
        auto depth = q_obj.value("depth").toObject();
        // Illiquid contracts (esp. F&O on far-month options) can return empty
        // buy/sell arrays. .first() on an empty QJsonArray is undefined; guard.
        const auto buy_arr = depth.value("buy").toArray();
        const auto sell_arr = depth.value("sell").toArray();
        auto buy0 = buy_arr.isEmpty() ? QJsonObject{} : buy_arr.first().toObject();
        auto sell0 = sell_arr.isEmpty() ? QJsonObject{} : sell_arr.first().toObject();
        q.bid = buy0.value("price").toDouble();
        q.bid_size = buy0.value("quantity").toDouble();
        q.ask = sell0.value("price").toDouble();
        q.ask_size = sell0.value("quantity").toDouble();
        // F&O fields — present on derivatives, zero elsewhere. Safe to read either way.
        q.oi = static_cast<qint64>(q_obj.value("oi").toDouble());
        const double oi_high = q_obj.value("oi_day_high").toDouble();
        const double oi_low = q_obj.value("oi_day_low").toDouble();
        if (oi_high > 0)
            q.oi_change_pct = ((static_cast<double>(q.oi) - oi_low) / oi_high) * 100.0;
        quotes.append(q);
    }
    return {true, quotes, "", ts};
}

ApiResponse<QVector<BrokerCandle>> ZerodhaBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                              const QString& resolution, const QString& from_date,
                                                              const QString& to_date) {
    QString token;

    QString lookup_exchange = "NSE";
    QString lookup_symbol = symbol;
    if (symbol.contains(':')) {
        lookup_exchange = symbol.section(':', 0, 0);
        lookup_symbol = symbol.section(':', 1);
    }
    auto svc_token = InstrumentService::instance().instrument_token(lookup_symbol, lookup_exchange, "zerodha");
    if (svc_token.has_value())
        token = QString::number(svc_token.value());

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
    if (interval.isEmpty()) {
        return {false, std::nullopt,
                "Zerodha get_history: unsupported resolution '" + resolution + "'", now_ts()};
    }

    // Kite Connect caps the date range PER REQUEST by interval. Resolve the max
    // number of days allowed in a single request for the (already-resolved) interval.
    auto cap_days_for_interval = [](const QString& iv) -> int {
        static const QMap<QString, int> caps = {
            {"minute", 60},  {"2minute", 60},  {"3minute", 100}, {"4minute", 100}, {"5minute", 100},
            {"10minute", 100}, {"15minute", 200}, {"30minute", 200}, {"60minute", 400}, {"hour", 400},
            {"2hour", 400}, {"3hour", 400}, {"4hour", 400}, {"day", 2000}, {"week", 2000},
        };
        return caps.value(iv, 2000);
    };
    const int cap_days = cap_days_for_interval(interval);

    // Fetch + parse ONE date window. Returns {http_resp, parsed_candles}.
    // Reuses the existing URL build + response parse (percent-encoding/URL unchanged per request).
    auto fetch_window = [&](const QString& win_from, const QString& win_to) {
        QString url = QString("%1/instruments/historical/%2/%3?from=%4&to=%5&continuous=0&oi=1")
                          .arg(base_url(), token, interval, win_from, win_to);
        auto resp = BrokerHttp::instance().get(url, auth_headers(creds));

        QVector<BrokerCandle> win_candles;
        if (resp.success) {
            auto arr = resp.json.value("data").toObject().value("candles").toArray();
            for (const auto& v : arr) {
                auto c = v.toArray();
                if (c.size() < 6)
                    continue;
                // BrokerCandle.timestamp is MILLISECONDS since epoch (the contract the
                // chart and every ms-based broker use). Kite returns ISO-8601 with a
                // +05:30 offset, e.g. "2024-03-28T09:15:00+0530"; emit ms, not seconds —
                // seconds here landed candles in Jan 1970 and made the live bar roll a
                // new candle on every tick (chart "going every second").
                qint64 epoch_ms = QDateTime::fromString(c[0].toString(), Qt::ISODateWithMs).toMSecsSinceEpoch();
                if (epoch_ms == 0)
                    epoch_ms = QDateTime::fromString(c[0].toString(), Qt::ISODate).toMSecsSinceEpoch();
                BrokerCandle bc;
                bc.timestamp = epoch_ms;
                bc.open = c[1].toDouble();
                bc.high = c[2].toDouble();
                bc.low = c[3].toDouble();
                bc.close = c[4].toDouble();
                bc.volume = c[5].toDouble();
                // c[6] is Open Interest when oi=1 is requested (F&O only); 0 elsewhere.
                if (c.size() >= 7)
                    bc.oi = c[6].toDouble();
                win_candles.append(bc);
            }
        }
        return std::make_pair(resp, win_candles);
    };

    // The caller passes "yyyy-MM-dd". Parse to compute the span; on failure, fall back
    // to the single-request path (preserves existing behavior for unexpected formats).
    const QDate d_from = QDate::fromString(from_date, "yyyy-MM-dd");
    const QDate d_to = QDate::fromString(to_date, "yyyy-MM-dd");
    const bool dates_valid = d_from.isValid() && d_to.isValid() && d_from <= d_to;
    const qint64 span_days = dates_valid ? d_from.daysTo(d_to) : 0;

    // Single-window path: range fits in one request (or dates couldn't be parsed).
    // Behaviorally identical to the original implementation.
    if (!dates_valid || span_days <= cap_days) {
        auto [resp, candles] = fetch_window(from_date, to_date);
        int64_t ts = now_ts();
        if (!resp.success)
            return {false, std::nullopt, checked_error(resp, resp.error), ts};
        return {true, candles, "", ts};
    }

    // Multi-window path: slice [from,to] into consecutive <=cap_days sub-windows
    // (oldest -> newest) and accumulate candles.
    QVector<BrokerCandle> candles;
    constexpr int kMaxIterations = 60;
    int iterations = 0;
    QDate cursor = d_from;
    int64_t ts = now_ts();
    while (cursor <= d_to) {
        if (++iterations > kMaxIterations)
            break; // SAFETY: cap the loop; return what we have so far.

        QDate win_end = cursor.addDays(cap_days);
        if (win_end > d_to)
            win_end = d_to;

        auto [resp, win_candles] = fetch_window(cursor.toString("yyyy-MM-dd"), win_end.toString("yyyy-MM-dd"));
        ts = now_ts();
        if (!resp.success) {
            // First window fails -> propagate the error as today. Otherwise, partial success:
            // stop and return whatever we have already collected.
            if (candles.isEmpty())
                return {false, std::nullopt, checked_error(resp, resp.error), ts};
            break;
        }
        candles += win_candles;

        // Advance past the current window. Step to the day after win_end to avoid an
        // infinite loop and overlap; consecutive duplicate timestamps are deduped below.
        if (win_end == d_to)
            break;
        cursor = win_end.addDays(1);
    }

    // Sort ascending by timestamp, then dedupe consecutive duplicate timestamps.
    std::sort(candles.begin(), candles.end(),
              [](const BrokerCandle& a, const BrokerCandle& b) { return a.timestamp < b.timestamp; });
    candles.erase(std::unique(candles.begin(), candles.end(),
                              [](const BrokerCandle& a, const BrokerCandle& b) { return a.timestamp == b.timestamp; }),
                  candles.end());

    return {true, candles, "", ts};
}

// ============================================================================
// Margin Calculator — Zerodha /margins endpoints
// ============================================================================

// Serialise a UnifiedOrder into the JSON leg format Zerodha expects for margin APIs.
static QJsonObject order_to_margin_leg(const UnifiedOrder& o) {
    QJsonObject leg;
    leg["exchange"] = o.exchange;
    leg["tradingsymbol"] = o.symbol;
    leg["transaction_type"] = (o.side == OrderSide::Buy) ? "BUY" : "SELL";
    leg["variety"] = "regular";
    leg["product"] = (o.product_type == ProductType::Intraday) ? "MIS"
                     : (o.product_type == ProductType::Margin) ? "NRML"
                                                               : "CNC";
    leg["order_type"] = (o.order_type == OrderType::Market)          ? "MARKET"
                        : (o.order_type == OrderType::StopLoss)      ? "SL-M"
                        : (o.order_type == OrderType::StopLossLimit) ? "SL"
                                                                     : "LIMIT";
    leg["quantity"] = int(o.quantity);
    leg["price"] = o.price;
    leg["trigger_price"] = o.stop_price;
    return leg;
}

// Parse a single margin object from Zerodha's response.
static OrderMargin parse_order_margin(const QJsonObject& m) {
    OrderMargin r;
    r.symbol = m.value("tradingsymbol").toString();
    r.exchange = m.value("exchange").toString();
    r.side = m.value("transaction_type").toString();
    r.quantity = m.value("quantity").toDouble();
    r.price = m.value("price").toDouble();
    r.total = m.value("total").toDouble();
    r.var_margin = m.value("var").toDouble();
    r.elm = m.value("elm").toDouble();
    r.additional = m.value("additional").toDouble();
    r.bo_margin = m.value("bo").toDouble();
    r.cash = m.value("cash").toDouble();
    r.pnl = m.value("pnl").toDouble();
    r.leverage = m.value("leverage").toDouble();
    r.error = m.value("error").toString();
    return r;
}

ApiResponse<OrderMargin> ZerodhaBroker::get_order_margins(const BrokerCredentials& creds, const UnifiedOrder& order) {
    QJsonArray arr;
    arr.append(order_to_margin_leg(order));

    auto resp = BrokerHttp::instance().post_json(QString(base_url()) + "/margins/orders", QJsonObject{{"orders", arr}},
                                                 auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "Margin calculation failed"), ts};

    auto data = resp.json.value("data").toArray();
    if (data.isEmpty())
        return {false, std::nullopt, "Empty margin response", ts};
    return {true, parse_order_margin(data.first().toObject()), "", ts};
}

ApiResponse<BasketMargin> ZerodhaBroker::get_basket_margins(const BrokerCredentials& creds,
                                                            const QVector<UnifiedOrder>& orders) {
    QJsonArray arr;
    for (const auto& o : orders)
        arr.append(order_to_margin_leg(o));

    // mode=compact returns per-order breakdown + netting
    QJsonObject payload;
    payload["orders"] = arr;
    payload["mode"] = "compact";

    auto resp = BrokerHttp::instance().post_json(QString(base_url()) + "/margins/basket", payload, auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "Basket margin calculation failed"), ts};

    auto data = resp.json.value("data").toObject();
    BasketMargin bm;
    bm.initial_margin = data.value("initial").toDouble();
    bm.final_margin = data.value("final").toDouble();
    for (const auto& v : data.value("orders").toArray())
        bm.orders.append(parse_order_margin(v.toObject()));
    return {true, bm, "", ts};
}

// ============================================================================
// GTT (Good Till Triggered) — Zerodha /gtt/triggers endpoints
// ============================================================================

// Build the JSON body required by Zerodha's GTT create/modify API.
// Zerodha expects:
//   type         : "single" | "two-leg"
//   tradingsymbol: broker native symbol
//   exchange     : "NSE" / "BSE" / etc.
//   trigger_values: [price] for single, [stoploss_price, target_price] for two-leg
//   last_price   : LTP at time of GTT creation
//   orders       : array of order objects (one per leg)
static QJsonObject build_gtt_body(const GttOrder& g) {
    QString type = (g.type == GttOrderType::OCO) ? "two-leg" : "single";

    QJsonArray trigger_values;
    QJsonArray orders;
    for (const auto& t : g.triggers) {
        trigger_values.append(t.trigger_price);
        QJsonObject leg;
        leg["exchange"] = g.exchange;
        leg["tradingsymbol"] = g.symbol;
        leg["transaction_type"] = (t.side == OrderSide::Buy) ? "BUY" : "SELL";
        leg["quantity"] = int(t.quantity);
        leg["order_type"] = (t.order_type == OrderType::Market) ? "MARKET" : "LIMIT";
        leg["product"] = (t.product == ProductType::Intraday) ? "MIS" : "CNC";
        leg["price"] = t.limit_price;
        orders.append(leg);
    }

    // Kite GTT API requires the symbol/exchange/triggers/last_price fields
    // nested inside a `condition` object — they are NOT flat at the body root.
    // The previous flat shape was rejected with 400 invalid_input.
    QJsonObject condition;
    condition["exchange"] = g.exchange;
    condition["tradingsymbol"] = g.symbol;
    condition["trigger_values"] = trigger_values;
    condition["last_price"] = g.last_price;

    QJsonObject body;
    body["type"] = type;
    body["condition"] = condition;
    body["orders"] = orders;
    return body;
}

// Parse a single GTT object from Zerodha's JSON response.
static GttOrder parse_gtt(const QJsonObject& o) {
    GttOrder g;
    g.gtt_id = QString::number(o.value("id").toInt());
    // Symbol/exchange live under `condition`, not at the response root.
    const auto cond = o.value("condition").toObject();
    g.symbol = cond.value("tradingsymbol").toString();
    if (g.symbol.isEmpty())
        g.symbol = o.value("tradingsymbol").toString(); // back-compat fallback
    g.exchange = cond.value("exchange").toString();
    if (g.exchange.isEmpty())
        g.exchange = o.value("exchange").toString();
    g.status = o.value("status").toString();
    g.created_at = o.value("created_at").toString();
    g.updated_at = o.value("updated_at").toString();
    g.last_price = cond.value("last_price").toDouble();

    QString type = o.value("type").toString();
    g.type = (type == "two-leg") ? GttOrderType::OCO : GttOrderType::Single;

    auto tv = cond.value("trigger_values").toArray();
    auto ords = o.value("orders").toArray();
    for (int i = 0; i < ords.size(); ++i) {
        auto leg = ords[i].toObject();
        GttTrigger t;
        t.trigger_price = (i < tv.size()) ? tv[i].toDouble() : 0.0;
        t.limit_price = leg.value("price").toDouble();
        t.quantity = leg.value("quantity").toDouble();
        t.side = (leg.value("transaction_type").toString() == "BUY") ? OrderSide::Buy : OrderSide::Sell;
        t.order_type = (leg.value("order_type").toString() == "MARKET") ? OrderType::Market : OrderType::Limit;
        t.product = (leg.value("product").toString() == "MIS") ? ProductType::Intraday : ProductType::Delivery;
        g.triggers.append(t);
    }
    return g;
}

GttPlaceResponse ZerodhaBroker::gtt_place(const BrokerCredentials& creds, const GttOrder& order) {
    // ── Market-Price-Protection (MPP) ────────────────────────────────────────
    // Zerodha's GTT infrastructure sends a regular order when the trigger fires.
    // If the order_type is MARKET, the execution price is unbounded — a flash
    // crash or spike can fill at an extreme price far from the trigger level.
    //
    // MPP converts Market legs to Limit with a protective buffer:
    //   buffer = max(5% of LTP, 5 × tick_size)
    //   BUY  → limit_price = LTP + buffer   (ceiling)
    //   SELL → limit_price = LTP - buffer   (floor, min 0)
    //
    // This gives the order enough room to fill under normal volatility while
    // capping the worst-case slippage. If the symbol's tick_size is available
    // from InstrumentService we use it; otherwise we fall back to the NSE
    // default of 0.05.
    GttOrder mpp_order = order;
    bool applied_mpp = false;

    for (auto& trigger : mpp_order.triggers) {
        if (trigger.order_type != OrderType::Market)
            continue;

        // Fetch LTP via the quote endpoint
        double ltp = mpp_order.last_price;
        if (ltp <= 0.0) {
            // Try to get a live quote for the symbol
            QVector<QString> syms = {(mpp_order.exchange.isEmpty() ? "NSE" : mpp_order.exchange) + ":" + mpp_order.symbol};
            auto quote_resp = get_quotes(creds, syms);
            if (quote_resp.success && quote_resp.data.has_value() && !quote_resp.data->isEmpty())
                ltp = quote_resp.data->first().ltp;
        }
        if (ltp <= 0.0) {
            LOG_WARN("Zerodha", QString("GTT MPP: could not determine LTP for %1 — sending Market as-is")
                                    .arg(mpp_order.symbol));
            continue;
        }

        // Determine tick_size from InstrumentService (fall back to 0.05)
        double tick = 0.05;
        auto inst = InstrumentService::instance().find(mpp_order.symbol, mpp_order.exchange.isEmpty() ? "NSE" : mpp_order.exchange, "zerodha");
        if (inst.has_value() && inst->tick_size > 0)
            tick = inst->tick_size;

        // buffer = max(5% of LTP, 5 × tick_size)
        const double buffer = std::max(ltp * 0.05, 5.0 * tick);

        // Convert to Limit with protective price
        trigger.order_type = OrderType::Limit;
        if (trigger.side == OrderSide::Buy)
            trigger.limit_price = ltp + buffer;
        else
            trigger.limit_price = std::max(0.0, ltp - buffer);

        // Round to tick size
        if (tick > 0)
            trigger.limit_price = std::round(trigger.limit_price / tick) * tick;

        applied_mpp = true;
        LOG_INFO("Zerodha", QString("GTT MPP: %1 %2 Market→Limit @ %3 (LTP=%4, buffer=%5, tick=%6)")
                                .arg(mpp_order.symbol)
                                .arg(trigger.side == OrderSide::Buy ? "BUY" : "SELL")
                                .arg(trigger.limit_price, 0, 'f', 2)
                                .arg(ltp, 0, 'f', 2)
                                .arg(buffer, 0, 'f', 2)
                                .arg(tick, 0, 'f', 4));
    }

    if (applied_mpp)
        mpp_order.last_price = mpp_order.last_price > 0 ? mpp_order.last_price : 0;

    auto resp = BrokerHttp::instance().post_json(QString(base_url()) + "/gtt/triggers", build_gtt_body(mpp_order),
                                                 auth_headers(creds));
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, "", checked_error(resp, "GTT place failed")};
    QString id = QString::number(resp.json.value("data").toObject().value("trigger_id").toInt());
    return {true, id, ""};
}

ApiResponse<GttOrder> ZerodhaBroker::gtt_get(const BrokerCredentials& creds, const QString& gtt_id) {
    auto resp = BrokerHttp::instance().get(QString("%1/gtt/triggers/%2").arg(base_url(), gtt_id), auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "GTT get failed"), ts};
    return {true, parse_gtt(resp.json.value("data").toObject()), "", ts};
}

ApiResponse<QVector<GttOrder>> ZerodhaBroker::gtt_list(const BrokerCredentials& creds) {
    auto resp = BrokerHttp::instance().get(QString(base_url()) + "/gtt/triggers", auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "GTT list failed"), ts};
    QVector<GttOrder> result;
    for (const auto& v : resp.json.value("data").toArray())
        result.append(parse_gtt(v.toObject()));
    return {true, result, "", ts};
}

ApiResponse<GttOrder> ZerodhaBroker::gtt_modify(const BrokerCredentials& creds, const QString& gtt_id,
                                                const GttOrder& updated) {
    auto resp = BrokerHttp::instance().put_json(QString("%1/gtt/triggers/%2").arg(base_url(), gtt_id),
                                                build_gtt_body(updated), auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "GTT modify failed"), ts};
    return {true, parse_gtt(resp.json.value("data").toObject()), "", ts};
}

ApiResponse<QJsonObject> ZerodhaBroker::gtt_cancel(const BrokerCredentials& creds, const QString& gtt_id) {
    auto resp = BrokerHttp::instance().del(QString("%1/gtt/triggers/%2").arg(base_url(), gtt_id), auth_headers(creds));
    int64_t ts = now_ts();
    if (!resp.success || resp.json.value("status").toString() != "success")
        return {false, std::nullopt, checked_error(resp, "GTT cancel failed"), ts};
    return {true, resp.json, "", ts};
}

// ============================================================================
// Multi-Quote & Market Depth — Zerodha /quote endpoint
// ============================================================================

ApiResponse<QVector<BrokerQuote>> ZerodhaBroker::get_multi_quotes(
    const BrokerCredentials& creds,
    const QVector<QPair<QString, QString>>& symbols) {

    QVector<BrokerQuote> all_quotes;
    constexpr int kBatchSize = 500;

    for (int start = 0; start < symbols.size(); start += kBatchSize) {
        int end = std::min<int>(start + kBatchSize, static_cast<int>(symbols.size()));

        // Build query string: ?i=EXCHANGE:SYMBOL&i=...
        QString query;
        for (int i = start; i < end; ++i) {
            const auto& [sym, exch] = symbols[i];
            // Try InstrumentService for broker-specific symbol; fall back to raw
            auto br_sym = InstrumentService::instance().to_brsymbol(sym, exch, "zerodha");
            QString key = (exch.isEmpty() ? "NSE" : exch) + ":" + (br_sym.has_value() ? br_sym.value() : sym);
            if (!query.isEmpty())
                query += "&i=";
            query += key;
        }

        auto resp = BrokerHttp::instance().get(
            QString(base_url()) + "/quote?i=" + query, auth_headers(creds));
        int64_t ts = now_ts();

        if (!resp.success)
            return {false, std::nullopt, checked_error(resp, resp.error), ts};

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
            q.change_pct = q.close > 0 ? (q.change / q.close) * 100.0 : 0.0;

            auto depth = q_obj.value("depth").toObject();
            const auto buy_arr = depth.value("buy").toArray();
            const auto sell_arr = depth.value("sell").toArray();
            auto buy0 = buy_arr.isEmpty() ? QJsonObject{} : buy_arr.first().toObject();
            auto sell0 = sell_arr.isEmpty() ? QJsonObject{} : sell_arr.first().toObject();
            q.bid = buy0.value("price").toDouble();
            q.bid_size = buy0.value("quantity").toDouble();
            q.ask = sell0.value("price").toDouble();
            q.ask_size = sell0.value("quantity").toDouble();

            q.oi = static_cast<qint64>(q_obj.value("oi").toDouble());
            const double oi_high = q_obj.value("oi_day_high").toDouble();
            const double oi_low = q_obj.value("oi_day_low").toDouble();
            if (oi_high > 0)
                q.oi_change_pct = ((static_cast<double>(q.oi) - oi_low) / oi_high) * 100.0;

            all_quotes.append(q);
        }
    }

    return {true, all_quotes, "", now_ts()};
}

ApiResponse<MarketDepth> ZerodhaBroker::get_market_depth(
    const BrokerCredentials& creds,
    const QString& symbol, const QString& exchange) {

    // Resolve broker-specific symbol
    auto br_sym = InstrumentService::instance().to_brsymbol(symbol, exchange, "zerodha");
    QString exch = exchange.isEmpty() ? "NSE" : exchange;
    QString key = exch + ":" + (br_sym.has_value() ? br_sym.value() : symbol);

    auto resp = BrokerHttp::instance().get(
        QString(base_url()) + "/quote?i=" + key, auth_headers(creds));
    int64_t ts = now_ts();

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, resp.error), ts};

    auto data = resp.json.value("data").toObject();
    auto q_obj = data.value(key).toObject();
    if (q_obj.isEmpty())
        return {false, std::nullopt, "No data returned for " + key, ts};

    MarketDepth md;
    md.symbol = symbol;
    md.exchange = exch;
    md.ltp = q_obj.value("last_price").toDouble();
    md.volume = q_obj.value("volume").toDouble();
    md.oi = q_obj.value("oi").toDouble();

    auto depth = q_obj.value("depth").toObject();

    // Parse 5-level bid/ask depth
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

} // namespace fincept::trading
