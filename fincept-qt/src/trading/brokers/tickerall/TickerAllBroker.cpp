#include "trading/brokers/tickerall/TickerAllBroker.h"

#include "trading/brokers/BrokerHttp.h"
#include "trading/brokers/BrokerModifyFields.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimeZone>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>

namespace fincept::trading {

// Named inner namespace, not an anonymous one: with unity builds ON for every
// release/CI preset (CLAUDE.md), ~20 .cpp files are concatenated into one TU and
// their anonymous namespaces merge — two files defining a file-scope `now_ts`
// would then be a hard redefinition error that only appears on CI.
namespace tickerall_detail {

inline int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// Parse a flexible date string (ISO / yyyy-MM-dd / yyyy-MM-dd HH:mm) into UTC.
// Returns an invalid QDateTime when none of the forms match.
inline QDateTime parse_dt(const QString& s) {
    if (s.isEmpty())
        return {};
    QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
    if (!dt.isValid()) {
        const QDate d = QDate::fromString(s, QStringLiteral("yyyy-MM-dd"));
        if (d.isValid())
            dt = QDateTime(d, QTime(0, 0), QTimeZone::utc());
    }
    if (!dt.isValid()) {
        dt = QDateTime::fromString(s, QStringLiteral("yyyy-MM-dd HH:mm"));
        if (dt.isValid())
            dt.setTimeZone(QTimeZone::utc());
    }
    return dt.isValid() ? dt.toUTC() : QDateTime();
}

// Ticket identity is a string in PendingOrder but a number in PlaceOrderResult,
// so read either shape rather than trusting one.
inline QString ticket_str(const QJsonValue& v) {
    if (v.isString())
        return v.toString();
    if (v.isDouble())
        return QString::number(static_cast<qint64>(v.toDouble()));
    return {};
}

} // namespace tickerall_detail

using tickerall_detail::now_ts;
using tickerall_detail::parse_dt;
using tickerall_detail::ticket_str;

// ── Auth headers ─────────────────────────────────────────────────────────────

QMap<QString, QString> TickerAllBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Authorization", QStringLiteral("Bearer %1").arg(creds.api_key)},
        {"Content-Type", "application/json"},
        {"Accept", "application/json"},
    };
}

// ── Error helpers ────────────────────────────────────────────────────────────

QString TickerAllBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    // The [TOKEN_EXPIRED] marker is the contract IBroker::validate_session
    // classifies on (BrokerInterface.h). Without it the account reads "Connected"
    // forever while every request 401s and the user is never prompted to reconnect.
    if (resp.status_code == 401)
        return QStringLiteral("[TOKEN_EXPIRED] TickerAll API key is invalid or expired. "
                              "Check your key at tickerall.com.");
    if (resp.status_code == 403)
        return QStringLiteral("TickerAll: plan limit reached or resource reserved. "
                              "Live accounts require a paid plan.");
    if (resp.status_code == 404)
        return QStringLiteral("TickerAll account or ticket not found. The MT5 session may have ended — reconnect.");
    if (resp.status_code == 400 || resp.status_code == 422)
        return QStringLiteral("TickerAll: invalid request. Check symbol, volume and price.");
    if (resp.status_code == 429)
        return QStringLiteral("TickerAll rate limit reached. Please wait and retry.");
    if (resp.status_code == 503)
        return QStringLiteral("TickerAll is momentarily unreachable. Safe to retry.");

    // TickerAll error envelope: {"error":{"code":…,"message":…}} or a flat message.
    if (!resp.json.isEmpty()) {
        const QJsonObject err = resp.json.value("error").toObject();
        const QString nested = err.value("message").toString();
        if (!nested.isEmpty())
            return nested;
        const QString msg = resp.json.value("message").toString();
        if (!msg.isEmpty())
            return msg;
        const QString flat = resp.json.value("error").toString();
        if (!flat.isEmpty())
            return flat;
    }
    if (!resp.error.isEmpty())
        return resp.error;
    return fallback;
}

// ── Mapping helpers ──────────────────────────────────────────────────────────

// TickerAll's order type union is 'market' | 'limit' | 'stop'. It has no
// stop-limit, so StopLossLimit degrades to a stop order triggered at the same
// price — the closest behaviour the venue actually offers.
QString TickerAllBroker::order_type_for(OrderType type) {
    switch (type) {
        case OrderType::Market:
            return QStringLiteral("market");
        case OrderType::Limit:
            return QStringLiteral("limit");
        case OrderType::StopLoss:
        case OrderType::StopLossLimit:
            return QStringLiteral("stop");
    }
    return QStringLiteral("market");
}

// Timeframe union: M1 | M5 | M15 | M30 | H1 | H4 | D1 | W1 | MN1.
QString TickerAllBroker::map_timeframe(const QString& resolution) {
    const QString r = resolution.trimmed();
    if (r == "1" || r.compare("1m", Qt::CaseInsensitive) == 0 || r.compare("M1", Qt::CaseSensitive) == 0)
        return QStringLiteral("M1");
    if (r == "5" || r.compare("5m", Qt::CaseInsensitive) == 0)
        return QStringLiteral("M5");
    if (r == "15" || r.compare("15m", Qt::CaseInsensitive) == 0)
        return QStringLiteral("M15");
    if (r == "30" || r.compare("30m", Qt::CaseInsensitive) == 0)
        return QStringLiteral("M30");
    if (r == "60" || r.compare("1h", Qt::CaseInsensitive) == 0)
        return QStringLiteral("H1");
    // 2h/3h/4h/8h all fold to H4 — the coarsest intraday bar TickerAll serves.
    if (r == "120" || r == "180" || r == "240" || r == "480" || r.compare("2h", Qt::CaseInsensitive) == 0 ||
        r.compare("3h", Qt::CaseInsensitive) == 0 || r.compare("4h", Qt::CaseInsensitive) == 0 ||
        r.compare("8h", Qt::CaseInsensitive) == 0)
        return QStringLiteral("H4");
    if (r == "D" || r.compare("1d", Qt::CaseInsensitive) == 0)
        return QStringLiteral("D1");
    if (r == "W" || r.compare("1w", Qt::CaseInsensitive) == 0)
        return QStringLiteral("W1");
    if (r == "M" || r.compare("1mn", Qt::CaseInsensitive) == 0 || r.compare("1M", Qt::CaseSensitive) == 0)
        return QStringLiteral("MN1");
    return QStringLiteral("H1");
}

// ── Account snapshot (funds + positions + live prices in one call) ───────────

ApiResponse<QJsonObject> TickerAllBroker::account_snapshot(const BrokerCredentials& creds, int max_age_s) {
    const QString account_id = creds.access_token;
    if (account_id.isEmpty())
        return {false, std::nullopt, "[TOKEN_EXPIRED] No TickerAll session. Connect the MT5 account first."};

    if (max_age_s > 0) {
        QMutexLocker lock(&snapshot_mutex_);
        const auto it = snapshot_cache_.constFind(account_id);
        if (it != snapshot_cache_.constEnd() && now_ts() - it->fetched_at < max_age_s)
            return {true, it->payload, ""};
    }

    const QString url = QStringLiteral("%1/v1/accounts/%2").arg(base_url(), account_id);
    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Failed to fetch account snapshot.")};

    // An account that is provisioned but not currently logged in answers
    // status:"offline" with no `account` block. Surface that as a real error
    // instead of reporting a zero balance as though it were the truth.
    const QString status = resp.json.value("status").toString();
    if (status == QLatin1String("offline"))
        return {false, std::nullopt,
                QStringLiteral("MT5 account is offline on TickerAll. Reconnect it to resume trading.")};

    {
        QMutexLocker lock(&snapshot_mutex_);
        snapshot_cache_[account_id] = Snapshot{resp.json, now_ts()};
    }
    return {true, resp.json, ""};
}

// ── Authentication (TickerAll session start) ─────────────────────────────────

TokenExchangeResponse TickerAllBroker::exchange_token(const QString& api_key, const QString& /*api_secret*/,
                                                      const QString& auth_code) {
    TokenExchangeResponse result;

    const auto payload = QJsonDocument::fromJson(auth_code.toUtf8()).object();
    const QString login = payload.value("login").toString();
    const QString password = payload.value("password").toString();
    const QString server = payload.value("server").toString();
    const QString account_type = payload.value("account_type").toString(QStringLiteral("demo"));

    if (api_key.isEmpty()) {
        result.error = "TickerAll API key is required.";
        return result;
    }
    if (login.isEmpty() || password.isEmpty() || server.isEmpty()) {
        result.error = "MT5 login, password, and server name are required.";
        return result;
    }

    const QMap<QString, QString> headers = {
        {"Authorization", QStringLiteral("Bearer %1").arg(api_key)},
        {"Content-Type", "application/json"},
        {"Accept", "application/json"},
    };

    QJsonObject body;
    body["broker"] = QStringLiteral("mt5");
    body["server"] = server;
    body["password"] = password;
    // `account` is number|string upstream. MT5 logins are numeric, so send a
    // number when it parses cleanly and fall back to the raw string otherwise.
    bool numeric = false;
    const qlonglong login_num = login.toLongLong(&numeric);
    if (numeric)
        body["account"] = login_num;
    else
        body["account"] = login;

    const QString url = QStringLiteral("%1/v1/sessions").arg(base_url());
    auto resp = BrokerHttp::instance().post_json(url, body, headers);
    if (!resp.success) {
        result.error = checked_error(resp, "Failed to start the TickerAll MT5 session.");
        return result;
    }

    const QString account_id = resp.json.value("accountId").toString();
    if (account_id.isEmpty()) {
        result.error = "TickerAll did not return an accountId.";
        return result;
    }

    // Verify the session actually reached the broker before we report success —
    // a started session that never logs in would otherwise look connected.
    const QString verify_url = QStringLiteral("%1/v1/accounts/%2").arg(base_url(), account_id);
    auto verify = BrokerHttp::instance().get(verify_url, headers);
    if (!verify.success) {
        result.error = checked_error(verify, "MT5 session started but verification failed.");
        return result;
    }
    if (verify.json.value("status").toString() == QLatin1String("offline")) {
        result.error = "MT5 session started but the broker connection is offline. Check server name and password.";
        return result;
    }

    // Trust the server's own demo/live classification over the user's dropdown.
    const bool is_demo = resp.json.value("isDemo").toBool(account_type == QLatin1String("demo"));

    result.success = true;
    result.access_token = account_id;
    result.user_id = login;
    result.additional_data = QString::fromUtf8(QJsonDocument(QJsonObject{
                                                                 {"server", server},
                                                                 {"platform", "mt5"},
                                                                 {"account_type", is_demo ? "demo" : "live"},
                                                             })
                                                   .toJson(QJsonDocument::Compact));
    return result;
}

// ── Order placement ──────────────────────────────────────────────────────────

OrderPlaceResponse TickerAllBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    OrderPlaceResponse result;

    const QString account_id = creds.access_token;
    if (account_id.isEmpty()) {
        result.error = "[TOKEN_EXPIRED] No TickerAll session. Connect the MT5 account first.";
        return result;
    }

    const QString url = QStringLiteral("%1/v1/accounts/%2/orders").arg(base_url(), account_id);

    QJsonObject body;
    body["type"] = order_type_for(order.order_type);
    body["symbol"] = order.symbol;
    body["side"] = (order.side == OrderSide::Buy) ? QStringLiteral("BUY") : QStringLiteral("SELL");
    body["volume"] = order.quantity;

    // `price` is the trigger level for limit/stop; a market order must not carry one.
    if (order.order_type != OrderType::Market && order.price > 0)
        body["price"] = order.price;
    if (order.stop_loss > 0)
        body["stopLoss"] = order.stop_loss;
    if (order.take_profit > 0)
        body["takeProfit"] = order.take_profit;

    auto headers = auth_headers(creds);
    // TickerAll deduplicates state-changing calls on Idempotency-Key. UnifiedOrder
    // mints client_order_id ONCE per order intent (UnifiedTrading::place_order), so
    // a resubmit after BrokerHttp's 8 s client-side timeout is rejected as a
    // duplicate instead of opening a second live position.
    if (!order.client_order_id.isEmpty())
        headers.insert(QStringLiteral("Idempotency-Key"), order.client_order_id);

    auto resp = BrokerHttp::instance().post_json(url, body, headers);
    if (!resp.success) {
        result.error = checked_error(resp, "Failed to place order.");
        return result;
    }

    const QString ticket = ticket_str(resp.json.value("ticket"));
    if (ticket.isEmpty()) {
        result.error = QStringLiteral("MT5 accepted the request but returned no ticket.");
        return result;
    }

    result.success = true;
    result.order_id = ticket;

    // A fill changes balance and positions; drop the snapshot so the next
    // portfolio read reflects it rather than serving a stale cached one.
    {
        QMutexLocker lock(&snapshot_mutex_);
        snapshot_cache_.remove(account_id);
    }
    return result;
}

// ── Order modification (pending orders) ──────────────────────────────────────

ApiResponse<QJsonObject> TickerAllBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                       const QJsonObject& mods) {
    const QString account_id = creds.access_token;
    if (account_id.isEmpty())
        return {false, std::nullopt, "[TOKEN_EXPIRED] No TickerAll session. Connect the MT5 account first."};

    const QString url =
        QStringLiteral("%1/v1/accounts/%2/orders/%3").arg(base_url(), account_id, order_id);

    // Read through modify_fields so every caller spelling of a concept is
    // accepted — a missed spelling silently transmits 0 (BrokerModifyFields.h).
    QJsonObject body;
    if (modify_fields::has_any(mods, modify_fields::kPrice))
        body["price"] = modify_fields::number(mods, modify_fields::kPrice);
    if (modify_fields::has_any(mods, modify_fields::kTrigger))
        body["stopLoss"] = modify_fields::number(mods, modify_fields::kTrigger);
    if (mods.contains(QStringLiteral("take_profit")))
        body["takeProfit"] = mods.value(QStringLiteral("take_profit")).toVariant().toDouble();
    else if (mods.contains(QStringLiteral("takeProfit")))
        body["takeProfit"] = mods.value(QStringLiteral("takeProfit")).toVariant().toDouble();

    if (body.isEmpty())
        return {false, std::nullopt, "Nothing to modify — supply price, stop loss or take profit."};

    auto resp = BrokerHttp::instance().patch_json(url, body, auth_headers(creds));
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Failed to modify order.")};

    return {true, resp.json, ""};
}

// ── Order cancellation (pending orders) ──────────────────────────────────────

ApiResponse<QJsonObject> TickerAllBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    const QString account_id = creds.access_token;
    if (account_id.isEmpty())
        return {false, std::nullopt, "[TOKEN_EXPIRED] No TickerAll session. Connect the MT5 account first."};

    const QString url =
        QStringLiteral("%1/v1/accounts/%2/orders/%3").arg(base_url(), account_id, order_id);

    auto resp = BrokerHttp::instance().del(url, auth_headers(creds));
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Failed to cancel order.")};

    return {true, resp.json, ""};
}

// ── Pending orders ───────────────────────────────────────────────────────────

ApiResponse<QVector<BrokerOrderInfo>> TickerAllBroker::get_orders(const BrokerCredentials& creds) {
    const QString account_id = creds.access_token;
    if (account_id.isEmpty())
        return {false, std::nullopt, "[TOKEN_EXPIRED] No TickerAll session. Connect the MT5 account first."};

    // waitMs=0: this is a poller, so don't let the server block waiting for a
    // just-warmed connection's snapshot to settle.
    const QString url =
        QStringLiteral("%1/v1/accounts/%2/orders/pending?waitMs=0").arg(base_url(), account_id);

    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Failed to fetch pending orders.")};

    const auto arr = resp.json.value("orders").toArray();

    QVector<BrokerOrderInfo> orders;
    orders.reserve(arr.size());

    for (const auto& item : arr) {
        const auto o = item.toObject();
        BrokerOrderInfo info;
        info.order_id = ticket_str(o.value("ticket"));
        info.symbol = o.value("symbol").toString();
        info.exchange = QStringLiteral("FOREX");
        info.side = (o.value("side").toString().compare("BUY", Qt::CaseInsensitive) == 0) ? "buy" : "sell";

        const QString ot = o.value("orderType").toString().toUpper();
        if (ot == QLatin1String("LIMIT"))
            info.order_type = "limit";
        else if (ot == QLatin1String("STOP_LIMIT"))
            info.order_type = "stop_limit";
        else if (ot == QLatin1String("STOP"))
            info.order_type = "stop";
        else
            info.order_type = "market";

        info.product_type = QStringLiteral("MARGIN");
        info.quantity = o.value("volume").toDouble();
        // `price` is the trigger level; `limitPrice` (nullable) is the limit leg
        // of a stop-limit. Report the limit leg as price when present.
        const QJsonValue limit_px = o.value("limitPrice");
        info.price = (limit_px.isDouble()) ? limit_px.toDouble() : o.value("price").toDouble();
        info.trigger_price = o.value("price").toDouble();
        info.stop_price = o.value("stopLoss").toDouble();
        info.filled_qty = 0;
        info.avg_price = 0;
        // Everything this endpoint returns is by definition still working.
        info.status = "open";
        info.timestamp = o.value("setTime").toString();

        orders.append(info);
    }

    return {true, orders, ""};
}

// ── Trade book (closed round-trips) ──────────────────────────────────────────

ApiResponse<QJsonObject> TickerAllBroker::get_trade_book(const BrokerCredentials& creds) {
    const QString account_id = creds.access_token;
    if (account_id.isEmpty())
        return {false, std::nullopt, "[TOKEN_EXPIRED] No TickerAll session. Connect the MT5 account first."};

    const auto now = QDateTime::currentDateTimeUtc();
    const auto start = now.addDays(-30);

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("from"), start.toString(Qt::ISODate));
    q.addQueryItem(QStringLiteral("to"), now.toString(Qt::ISODate));
    q.addQueryItem(QStringLiteral("limit"), QStringLiteral("500"));
    q.addQueryItem(QStringLiteral("waitMs"), QStringLiteral("0"));

    const QString url = QStringLiteral("%1/v1/accounts/%2/history?%3").arg(base_url(), account_id, q.toString());

    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Failed to fetch trade history.")};

    return {true, resp.json, ""};
}

// ── Positions ────────────────────────────────────────────────────────────────

ApiResponse<QVector<BrokerPosition>> TickerAllBroker::get_positions(const BrokerCredentials& creds) {
    auto snap = account_snapshot(creds, /*max_age_s=*/2);
    if (!snap.success)
        return {false, std::nullopt, snap.error};

    const auto arr = snap.data->value("positions").toArray();

    QVector<BrokerPosition> positions;
    positions.reserve(arr.size());

    for (const auto& item : arr) {
        const auto p = item.toObject();
        BrokerPosition pos;
        pos.symbol = p.value("symbol").toString();
        pos.exchange = QStringLiteral("FOREX");
        pos.product_type = QStringLiteral("MARGIN");
        pos.side = (p.value("side").toString().compare("BUY", Qt::CaseInsensitive) == 0) ? "long" : "short";
        pos.quantity = p.value("volume").toDouble();
        pos.avg_price = p.value("entryPrice").toDouble();
        pos.ltp = p.value("currentPrice").toDouble();
        pos.pnl = p.value("profit").toDouble();

        if (pos.avg_price > 0 && pos.quantity != 0) {
            const double invested = std::abs(pos.avg_price * pos.quantity);
            pos.pnl_pct = (invested != 0) ? (pos.pnl / invested) * 100.0 : 0.0;
        }

        positions.append(pos);
    }

    return {true, positions, ""};
}

// ── Holdings (MT5 has no separate holdings concept — mirror positions) ───────

ApiResponse<QVector<BrokerHolding>> TickerAllBroker::get_holdings(const BrokerCredentials& creds) {
    auto pos_result = get_positions(creds);
    if (!pos_result.success)
        return {false, std::nullopt, pos_result.error};

    QVector<BrokerHolding> holdings;
    holdings.reserve(pos_result.data->size());

    for (const auto& p : *pos_result.data) {
        BrokerHolding h;
        h.symbol = p.symbol;
        h.exchange = p.exchange;
        h.quantity = p.quantity;
        h.avg_price = p.avg_price;
        h.ltp = p.ltp;
        h.pnl = p.pnl;
        h.pnl_pct = p.pnl_pct;
        // invested/current ignore contract (lot) size: quantity is lots (e.g. 0.10),
        // not units, so price*lots is off by contractSize. The position payload
        // carries no contract size, and `profit` from the API is authoritative for
        // P&L, so these two are display-only approximations — same caveat as
        // MetaApiBroker::get_holdings.
        h.invested_value = p.avg_price * p.quantity;
        h.current_value = p.ltp * p.quantity;
        holdings.append(h);
    }

    return {true, holdings, ""};
}

// ── Funds / account information ──────────────────────────────────────────────

ApiResponse<BrokerFunds> TickerAllBroker::get_funds(const BrokerCredentials& creds) {
    auto snap = account_snapshot(creds, /*max_age_s=*/2);
    if (!snap.success)
        return {false, std::nullopt, snap.error};

    const QJsonObject acct = snap.data->value("account").toObject();
    if (acct.isEmpty())
        return {false, std::nullopt, "TickerAll returned no account block — the MT5 session may still be warming up."};

    BrokerFunds funds;
    funds.total_balance = acct.value("balance").toDouble();
    funds.available_balance = acct.value("freeMargin").toDouble();
    funds.used_margin = acct.value("margin").toDouble();
    funds.collateral = 0;

    funds.raw_data = acct;
    funds.raw_data["equity"] = acct.value("equity").toDouble();
    funds.raw_data["leverage"] = acct.value("leverage").toInt();
    // marginLevel is null when no margin is in use — normalise to 0.
    funds.raw_data["marginLevel"] = acct.value("marginLevel").toDouble(0.0);
    funds.raw_data["currency"] = acct.value("currency").toString();

    return {true, funds, ""};
}

// ── Quotes ───────────────────────────────────────────────────────────────────

ApiResponse<QVector<BrokerQuote>> TickerAllBroker::get_quotes(const BrokerCredentials& creds,
                                                              const QVector<QString>& symbols) {
    const QString account_id = creds.access_token;
    if (account_id.isEmpty())
        return {false, std::nullopt, "[TOKEN_EXPIRED] No TickerAll session. Connect the MT5 account first."};

    // TickerAll serves live ticks over WebSocket only — there is no REST quote
    // endpoint — but get_quotes() is a synchronous IBroker call. Two REST sources
    // cover it without a socket:
    //   1. the account snapshot, whose open positions carry a live currentPrice
    //      (free: one call, already cached, and exact for everything we hold);
    //   2. for anything not held, one M1 candle, whose `bid` and `spread` fields
    //      give a true bid/ask pair rather than a synthetic mid.
    // A future WS-backed tick cache (IBroker::ws_adapter_name) would supersede
    // step 2 and drop the per-symbol call entirely.
    QHash<QString, QJsonObject> live_positions;
    auto snap = account_snapshot(creds, /*max_age_s=*/2);
    if (snap.success) {
        for (const auto& item : snap.data->value("positions").toArray()) {
            const auto p = item.toObject();
            const double px = p.value("currentPrice").toDouble();
            if (px > 0)
                live_positions.insert(p.value("symbol").toString(), p);
        }
    }

    QVector<BrokerQuote> quotes;
    quotes.reserve(symbols.size());

    for (const auto& sym : symbols) {
        BrokerQuote q;
        q.symbol = sym;

        const auto held = live_positions.constFind(sym);
        if (held != live_positions.constEnd()) {
            const double px = held->value("currentPrice").toDouble();
            q.ltp = px;
            q.close = px;
            // A position quotes at its own close side; no bid/ask split is exposed.
            q.bid = px;
            q.ask = px;
            const auto dt = QDateTime::fromString(held->value("lastUpdate").toString(), Qt::ISODate);
            q.timestamp = dt.isValid() ? dt.toSecsSinceEpoch() : now_ts();
            quotes.append(q);
            continue;
        }

        QUrlQuery cq;
        cq.addQueryItem(QStringLiteral("symbol"), sym);
        cq.addQueryItem(QStringLiteral("count"), QStringLiteral("1"));
        cq.addQueryItem(QStringLiteral("timeframe"), QStringLiteral("M1"));
        const QString url =
            QStringLiteral("%1/v1/accounts/%2/candles?%3").arg(base_url(), account_id, cq.toString());

        auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
        if (!resp.success)
            continue; // one bad symbol must not fail the whole batch

        const auto arr = resp.json.value("candles").toArray();
        if (arr.isEmpty())
            continue;

        const auto c = arr.last().toObject();
        const double close = c.value("close").toDouble();
        // `bid` mirrors close; `spread` is (ask - bid) in price units, 0 when no
        // ask tick landed in the bar — in which case ask degrades to bid.
        const double bid = c.value("bid").toDouble(close);
        const double spread = c.value("spread").toDouble(0.0);

        q.ltp = close;
        q.open = c.value("open").toDouble();
        q.high = c.value("high").toDouble();
        q.low = c.value("low").toDouble();
        q.close = close;
        q.bid = bid;
        q.ask = (spread > 0.0) ? bid + spread : bid;
        q.volume = c.value("tickVolume").toDouble(0.0);
        if (q.open > 0) {
            q.change = close - q.open;
            q.change_pct = (q.change / q.open) * 100.0;
        }
        // Candle.timestamp is the bar OPEN time in Unix SECONDS; BrokerQuote's
        // contract is seconds too, so it passes through unscaled.
        q.timestamp = static_cast<int64_t>(c.value("timestamp").toDouble(static_cast<double>(now_ts())));

        quotes.append(q);
    }

    return {true, quotes, ""};
}

// ── Historical candles ───────────────────────────────────────────────────────

ApiResponse<QVector<BrokerCandle>> TickerAllBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                                const QString& resolution, const QString& from_date,
                                                                const QString& to_date) {
    const QString account_id = creds.access_token;
    if (account_id.isEmpty())
        return {false, std::nullopt, "[TOKEN_EXPIRED] No TickerAll session. Connect the MT5 account first."};

    const QString tf = map_timeframe(resolution);

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("symbol"), symbol);
    q.addQueryItem(QStringLiteral("timeframe"), tf);

    // The endpoint takes exactly one of: hours, count, or a from+to pair. Prefer
    // the explicit range when both bounds parse; otherwise fall back to a 24 h
    // look-back so a caller that passes nothing still gets a usable series.
    const QDateTime from_dt = parse_dt(from_date);
    const QDateTime to_dt = parse_dt(to_date);
    const bool ranged = from_dt.isValid() && to_dt.isValid() && from_dt < to_dt;

    if (ranged) {
        q.addQueryItem(QStringLiteral("from"), from_dt.toString(Qt::ISODate));
        q.addQueryItem(QStringLiteral("to"), to_dt.toString(Qt::ISODate));
    } else if (from_dt.isValid()) {
        const qint64 hours = std::max<qint64>(1, from_dt.secsTo(QDateTime::currentDateTimeUtc()) / 3600);
        q.addQueryItem(QStringLiteral("hours"), QString::number(hours));
    } else {
        q.addQueryItem(QStringLiteral("hours"), QStringLiteral("24"));
    }

    const QString url = QStringLiteral("%1/v1/accounts/%2/candles?%3").arg(base_url(), account_id, q.toString());

    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Failed to fetch candles.")};

    const auto arr = resp.json.value("candles").toArray();

    QVector<BrokerCandle> candles;
    candles.reserve(arr.size());

    for (const auto& item : arr) {
        const auto c = item.toObject();
        BrokerCandle candle;
        // Candle.timestamp is the bar OPEN time in Unix SECONDS; BrokerCandle's
        // contract is MILLISECONDS (see AlpacaBroker/SaxoBankBroker), so scale.
        candle.timestamp = static_cast<int64_t>(c.value("timestamp").toDouble()) * 1000LL;
        candle.open = c.value("open").toDouble();
        candle.high = c.value("high").toDouble();
        candle.low = c.value("low").toDouble();
        candle.close = c.value("close").toDouble();
        candle.volume = c.value("tickVolume").toDouble(0.0);
        candles.append(candle);
    }

    // The API documents "oldest first", but sort anyway: chart code assumes
    // ascending order and a silently reversed series draws a mirrored chart.
    std::sort(candles.begin(), candles.end(),
              [](const BrokerCandle& a, const BrokerCandle& b) { return a.timestamp < b.timestamp; });
    candles.erase(std::unique(candles.begin(), candles.end(),
                              [](const BrokerCandle& a, const BrokerCandle& b) { return a.timestamp == b.timestamp; }),
                  candles.end());

    return {true, candles, ""};
}

// ── Symbol list (cached for an hour) ─────────────────────────────────────────

QStringList TickerAllBroker::fetch_symbols(const BrokerCredentials& creds) {
    const QString account_id = creds.access_token;
    if (account_id.isEmpty())
        return {};

    {
        QMutexLocker lock(&symbol_mutex_);
        if (symbol_cache_.contains(account_id)) {
            const int64_t cached_at = symbol_cache_time_.value(account_id, 0);
            if (now_ts() - cached_at < 3600)
                return symbol_cache_.value(account_id);
        }
    }

    const QString url = QStringLiteral("%1/v1/accounts/%2/symbols").arg(base_url(), account_id);
    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
    if (!resp.success)
        return {};

    QStringList symbols;
    for (const auto& item : resp.json.value("symbols").toArray())
        symbols.append(item.toString());

    {
        QMutexLocker lock(&symbol_mutex_);
        symbol_cache_[account_id] = symbols;
        symbol_cache_time_[account_id] = now_ts();
    }

    return symbols;
}

} // namespace fincept::trading
