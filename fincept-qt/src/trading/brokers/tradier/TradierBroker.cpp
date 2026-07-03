#include "trading/brokers/tradier/TradierBroker.h"

#include "trading/brokers/BrokerHttp.h"

#include <QDateTime>
#include <QTimeZone>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrlQuery>

#include <algorithm>

namespace fincept::trading {

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// ---------- Static helpers ----------

QString TradierBroker::base(const BrokerCredentials& creds) {
    // api_secret holds the environment field value
    QString env = creds.api_secret.trimmed().toLower();
    if (env == "sandbox" || env == "paper")
        return "https://sandbox.tradier.com";
    return "https://api.tradier.com";
}

const BrokerEnumMap<QString>& TradierBroker::tr_enum_map() {
    static const auto m = [] {
        BrokerEnumMap<QString> x;
        x.set(OrderType::Market, "market");
        x.set(OrderType::Limit, "limit");
        x.set(OrderType::StopLoss, "stop");
        x.set(OrderType::StopLossLimit, "stop_limit");
        // Duration: Delivery → gtc; everything else falls back to "day" at callsite.
        x.set(ProductType::Delivery, "gtc");
        return x;
    }();
    return m;
}

QJsonArray TradierBroker::normalize_array(const QJsonValue& val) {
    if (val.isArray())
        return val.toArray();
    if (val.isObject())
        return QJsonArray{val};
    return QJsonArray{};
}

bool TradierBroker::is_token_expired(const BrokerHttpResponse& resp) {
    return (resp.status_code == 401);
}

// OCC option symbols: ROOT (1-6 letters) + YYMMDD + C/P + 8-digit strike,
// e.g. AAPL240119C00150000. Their quotes are per-share while cost_basis (and
// thus avg_price) bakes the 100x contract multiplier, so P&L math must scale
// the quoted mark to match.
static bool is_occ_option(const QString& symbol) {
    static const QRegularExpression re(QStringLiteral("^[A-Z]{1,6}\\d{6}[CP]\\d{8}$"));
    return re.match(symbol).hasMatch();
}

QString TradierBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    if (is_token_expired(resp))
        return "[TOKEN_EXPIRED] Access token is invalid or expired";
    if (!resp.success)
        return resp.error.isEmpty() ? fallback : resp.error;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (doc.isObject()) {
        // {"errors": {"error": "..."}} or {"fault": {"faultstring": "..."}}
        QJsonObject obj = doc.object();
        QString err = obj.value("errors").toObject().value("error").toString();
        if (!err.isEmpty())
            return err;
        err = obj.value("fault").toObject().value("faultstring").toString();
        if (!err.isEmpty())
            return err;
    }
    return fallback;
}

// ---------- Auth headers ----------

QMap<QString, QString> TradierBroker::auth_headers(const BrokerCredentials& creds) const {
    return {{"Authorization", "Bearer " + creds.access_token}, {"Accept", "application/json"}};
}

// ---------- exchange_token ----------
// ApiKey = access token, Environment field = "sandbox" or "live"
// Validates via GET /v1/user/profile, extracts account_id → user_id

TokenExchangeResponse TradierBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                    const QString& /*auth_code*/) {
    if (api_key.trimmed().isEmpty())
        return {false, "", "", "", "Access token is required", ""};

    QString env_base = (api_secret.trimmed().toLower() == "sandbox" || api_secret.trimmed().toLower() == "paper")
                           ? "https://sandbox.tradier.com"
                           : "https://api.tradier.com";

    auto& http = BrokerHttp::instance();
    auto resp = http.get(env_base + "/v1/user/profile",
                         {{"Authorization", "Bearer " + api_key}, {"Accept", "application/json"}});

    if (!resp.success) {
        if (resp.status_code == 401)
            return {false, "", "", "", "[TOKEN_EXPIRED] Access token is invalid or expired", ""};
        return {false, "", "", "", "Profile fetch failed: " + resp.error, ""};
    }

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "", "", "Profile: invalid response", ""};

    QJsonObject profile = doc.object().value("profile").toObject();
    if (profile.isEmpty())
        return {false, "", "", "", "Profile: missing profile object", ""};

    // account may be object (single) or array (multiple) — take first
    QString account_id;
    QJsonValue acct_val = profile.value("account");
    if (acct_val.isArray()) {
        account_id = acct_val.toArray()[0].toObject().value("account_number").toString();
    } else {
        account_id = acct_val.toObject().value("account_number").toString();
    }

    if (account_id.isEmpty())
        return {false, "", "", "", "Profile: could not find account_number", ""};

    return {true, api_key, "", account_id, "", ""};
}

// ---------- place_order ----------
// POST form-encoded to /v1/accounts/{acct}/orders

OrderPlaceResponse TradierBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    QUrlQuery form;
    form.addQueryItem("class", "equity");
    form.addQueryItem("symbol", order.symbol);
    form.addQueryItem("side", (order.side == OrderSide::Buy) ? "buy" : "sell");
    form.addQueryItem("quantity", QString::number(static_cast<int>(order.quantity)));
    form.addQueryItem("type", tr_enum_map().order_type_or(order.order_type, "market"));
    form.addQueryItem("duration", tr_enum_map().product_or(order.product_type, "day"));
    if (order.price > 0)
        form.addQueryItem("price", QString::number(order.price, 'f', 2));
    if (order.stop_price > 0)
        form.addQueryItem("stop", QString::number(order.stop_price, 'f', 2));
    form.addQueryItem("tag", "fincept");

    QMap<QString, QString> hdrs = auth_headers(creds);
    hdrs["Content-Type"] = "application/x-www-form-urlencoded";

    auto& http = BrokerHttp::instance();
    auto resp = http.post_raw(base(creds) + "/v1/accounts/" + acct + "/orders",
                              form.toString(QUrl::FullyEncoded).toUtf8(), hdrs);

    if (!resp.success)
        return {false, "", checked_error(resp, "place_order failed")};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "place_order: invalid response"};

    QJsonObject obj = doc.object();
    QJsonObject order_obj = obj.value("order").toObject();
    if (order_obj.isEmpty() || order_obj.value("status").toString() != "ok")
        return {false, "",
                checked_error(resp, obj.value("errors").toObject().value("error").toString("place_order failed"))};

    QString order_id = QString::number(order_obj.value("id").toVariant().toLongLong());
    return {true, order_id, ""};
}

// ---------- modify_order ----------

ApiResponse<QJsonObject> TradierBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                     const QJsonObject& mods) {
    int64_t ts = now_ts();
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    QUrlQuery form;
    if (mods.contains("type"))
        form.addQueryItem("type", mods.value("type").toString());
    if (mods.contains("duration"))
        form.addQueryItem("duration", mods.value("duration").toString());
    if (mods.contains("price"))
        form.addQueryItem("price", QString::number(mods.value("price").toDouble(), 'f', 2));
    if (mods.contains("stop"))
        form.addQueryItem("stop", QString::number(mods.value("stop").toDouble(), 'f', 2));

    QMap<QString, QString> hdrs = auth_headers(creds);
    hdrs["Content-Type"] = "application/x-www-form-urlencoded";

    auto& http = BrokerHttp::instance();
    auto resp = http.put_raw(base(creds) + "/v1/accounts/" + acct + "/orders/" + order_id,
                             form.toString(QUrl::FullyEncoded).toUtf8(), hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "modify_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "modify_order: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("order").toObject().value("status").toString() != "ok")
        return {false, std::nullopt, checked_error(resp, "modify_order failed"), ts};

    return {true, obj, "", ts};
}

// ---------- cancel_order ----------

ApiResponse<QJsonObject> TradierBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    int64_t ts = now_ts();
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.del(base(creds) + "/v1/accounts/" + acct + "/orders/" + order_id, auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "cancel_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "cancel_order: invalid response", ts};

    return {true, doc.object(), "", ts};
}

// ---------- get_orders ----------

ApiResponse<QVector<BrokerOrderInfo>> TradierBroker::get_orders(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.get(base(creds) + "/v1/accounts/" + acct + "/orders?includeTags=true", auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_orders failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_orders: invalid response", ts};

    QJsonValue orders_val = doc.object().value("orders");
    // "null" string means no orders
    if (orders_val.isString())
        return {true, QVector<BrokerOrderInfo>{}, "", ts};

    QJsonArray arr = normalize_array(orders_val.toObject().value("order"));
    QVector<BrokerOrderInfo> orders;
    orders.reserve(arr.size());

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = QString::number(o.value("id").toVariant().toLongLong());
        info.symbol = o.value("symbol").toString();
        info.exchange = o.value("exch").toString();
        info.quantity = o.value("quantity").toDouble();
        info.filled_qty = o.value("exec_quantity").toDouble();
        info.price = o.value("price").toDouble();
        info.stop_price = o.value("stop_price").toDouble();
        info.avg_price = o.value("avg_fill_price").toDouble();

        QString st = o.value("status").toString();
        if (st == "filled")
            info.status = "filled";
        else if (st == "canceled" || st == "expired")
            info.status = "cancelled";
        else if (st == "rejected" || st == "error")
            info.status = "rejected";
        else
            info.status = "open";

        info.side = o.value("side").toString();
        info.order_type = o.value("type").toString();
        info.product_type = o.value("duration").toString();
        info.timestamp = o.value("create_date").toString();
        orders.append(info);
    }

    return {true, orders, "", ts};
}

// ---------- get_trade_book ----------

ApiResponse<QJsonObject> TradierBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.get(base(creds) + "/v1/accounts/" + acct + "/orders", auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_trade_book failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_trade_book: invalid response", ts};

    return {true, doc.object(), "", ts};
}

// ---------- get_positions ----------

ApiResponse<QVector<BrokerPosition>> TradierBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.get(base(creds) + "/v1/accounts/" + acct + "/positions", auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_positions failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_positions: invalid response", ts};

    QJsonValue pos_val = doc.object().value("positions");
    if (pos_val.isString())
        return {true, QVector<BrokerPosition>{}, "", ts};

    QJsonArray arr = normalize_array(pos_val.toObject().value("position"));
    QVector<BrokerPosition> positions;

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        double qty = o.value("quantity").toDouble();
        if (qty == 0.0)
            continue;

        BrokerPosition pos;
        pos.symbol = o.value("symbol").toString();
        pos.exchange = "US";
        pos.quantity = qty;
        // avg_price = cost_basis / qty. For options this reflects the 100× contract
        // multiplier (cost_basis is the full notional); we do NOT un-bake it here.
        pos.avg_price = (qty != 0) ? o.value("cost_basis").toDouble() / qty : 0.0;
        pos.side = qty > 0 ? "LONG" : "SHORT";
        // ltp/pnl/pnl_pct hydrated below — the positions endpoint carries no price.
        positions.append(pos);
    }

    // Hydrate LTP via the existing /v1/markets/quotes plumbing (read-only,
    // batched). Without this, views with no live-quote subscription showed $0
    // value/P&L for real positions. Best-effort: on failure rows keep ltp = 0.
    if (!positions.isEmpty()) {
        QVector<QString> syms;
        syms.reserve(positions.size());
        for (const auto& p : positions)
            syms.append(p.symbol);
        auto quotes = get_quotes(creds, syms);
        if (quotes.success && quotes.data.has_value()) {
            for (auto& pos : positions) {
                for (const auto& q : quotes.data.value()) {
                    if (q.symbol != pos.symbol || q.ltp <= 0.0)
                        continue;
                    pos.ltp = q.ltp;
                    // Scale option marks by the 100x contract multiplier to match
                    // the cost_basis-derived avg_price (see is_occ_option).
                    const double mark = is_occ_option(pos.symbol) ? q.ltp * 100.0 : q.ltp;
                    pos.pnl = (mark - pos.avg_price) * pos.quantity;
                    if (pos.avg_price > 0.0)
                        pos.pnl_pct = ((mark - pos.avg_price) / pos.avg_price) * 100.0;
                    break;
                }
            }
        }
    }

    return {true, positions, "", ts};
}

// ---------- get_holdings ----------
// Tradier has no separate holdings — map to positions (long only)

ApiResponse<QVector<BrokerHolding>> TradierBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.get(base(creds) + "/v1/accounts/" + acct + "/positions", auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_holdings: invalid response", ts};

    QJsonValue pos_val = doc.object().value("positions");
    if (pos_val.isString())
        return {true, QVector<BrokerHolding>{}, "", ts};

    QJsonArray arr = normalize_array(pos_val.toObject().value("position"));
    QVector<BrokerHolding> holdings;

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        double qty = o.value("quantity").toDouble();
        if (qty <= 0.0)
            continue; // long positions only for holdings

        BrokerHolding h;
        h.symbol = o.value("symbol").toString();
        h.exchange = "US";
        h.quantity = qty;
        // avg_price = cost_basis / qty. For options this reflects the 100× contract
        // multiplier (cost_basis is the full notional); we do NOT un-bake it here.
        h.avg_price = (qty > 0) ? o.value("cost_basis").toDouble() / qty : 0.0;
        // Prefer the API cost_basis; fall back to qty*avg_price when it's absent.
        const double cost_basis = o.value("cost_basis").toDouble();
        h.invested_value = (cost_basis != 0.0) ? cost_basis : h.quantity * h.avg_price;
        // ltp/current_value/pnl hydrated below — the endpoint carries no price.
        holdings.append(h);
    }

    // Hydrate LTP and derive value/P&L via the existing /v1/markets/quotes
    // plumbing (read-only, batched). Best-effort: on failure rows keep ltp = 0.
    if (!holdings.isEmpty()) {
        QVector<QString> syms;
        syms.reserve(holdings.size());
        for (const auto& h : holdings)
            syms.append(h.symbol);
        auto quotes = get_quotes(creds, syms);
        if (quotes.success && quotes.data.has_value()) {
            for (auto& h : holdings) {
                for (const auto& q : quotes.data.value()) {
                    if (q.symbol != h.symbol || q.ltp <= 0.0)
                        continue;
                    h.ltp = q.ltp;
                    // Scale option marks by the 100x contract multiplier so
                    // current_value is on the same basis as cost_basis.
                    const double mark = is_occ_option(h.symbol) ? q.ltp * 100.0 : q.ltp;
                    h.current_value = h.quantity * mark;
                    h.pnl = h.current_value - h.invested_value;
                    if (h.invested_value > 0.0)
                        h.pnl_pct = (h.pnl / h.invested_value) * 100.0;
                    break;
                }
            }
        }
    }

    return {true, holdings, "", ts};
}

// ---------- get_funds ----------

ApiResponse<BrokerFunds> TradierBroker::get_funds(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    QString acct = creds.user_id.isEmpty() ? creds.api_key : creds.user_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.get(base(creds) + "/v1/accounts/" + acct + "/balances", auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_funds: invalid response", ts};

    QJsonObject bal = doc.object().value("balances").toObject();

    // cash sub-object (present for cash accounts and margin accounts)
    QJsonObject cash_obj = bal.value("cash").toObject();
    QJsonObject margin_obj = bal.value("margin").toObject();

    double cash_available = cash_obj.value("cash_available").toDouble();
    if (cash_available == 0.0)
        cash_available = margin_obj.value("stock_buying_power").toDouble() / 2.0;

    BrokerFunds funds;
    funds.available_balance = cash_available;
    funds.total_balance = bal.value("total_equity").toDouble();
    funds.used_margin = bal.value("market_value").toDouble(); // cost basis of open positions
    funds.collateral = margin_obj.value("stock_buying_power").toDouble();

    return {true, funds, "", ts};
}

// ---------- get_quotes ----------
// GET /v1/markets/quotes?symbols=AAPL,MSFT

ApiResponse<QVector<BrokerQuote>> TradierBroker::get_quotes(const BrokerCredentials& creds,
                                                            const QVector<QString>& symbols) {
    int64_t ts = now_ts();
    if (symbols.isEmpty())
        return {true, QVector<BrokerQuote>{}, "", ts};

    // Strip exchange prefix if present (e.g. "NASDAQ:AAPL" → "AAPL")
    QStringList tickers;
    for (const QString& sym : symbols) {
        tickers.append(sym.contains(':') ? sym.section(':', 1) : sym);
    }

    auto& http = BrokerHttp::instance();
    auto resp = http.get(base(creds) + "/v1/markets/quotes?symbols=" + tickers.join(",") + "&greeks=false",
                         auth_headers(creds));

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_quotes failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_quotes: invalid response", ts};

    QJsonArray arr = normalize_array(doc.object().value("quotes").toObject().value("quote"));
    QVector<BrokerQuote> quotes;
    quotes.reserve(arr.size());

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        BrokerQuote q;
        q.symbol = o.value("symbol").toString();
        q.ltp = o.value("last").toDouble();
        q.open = o.value("open").toDouble();
        q.high = o.value("high").toDouble();
        q.low = o.value("low").toDouble();
        q.close = o.value("prevclose").toDouble(); // prevclose is reliable; close is null during hours
        q.volume = static_cast<int64_t>(o.value("volume").toDouble());
        q.change = o.value("change").toDouble();
        q.change_pct = o.value("change_percentage").toDouble();
        q.timestamp = ts;
        quotes.append(q);
    }

    return {true, quotes, "", ts};
}

// ---------- get_history ----------
// Daily/weekly/monthly: GET /v1/markets/history
// Intraday:             GET /v1/markets/timesales

ApiResponse<QVector<BrokerCandle>> TradierBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                              const QString& resolution, const QString& from_date,
                                                              const QString& to_date) {
    int64_t ts = now_ts();

    // Strip exchange prefix
    QString ticker = symbol.contains(':') ? symbol.section(':', 1) : symbol;
    // Also strip conid if present (3-part format)
    if (ticker.contains(':'))
        ticker = ticker.section(':', 0, 0);

    auto& http = BrokerHttp::instance();
    QVector<BrokerCandle> candles;

    bool is_intraday = (resolution == "1" || resolution == "5" || resolution == "15" || resolution == "1m" ||
                        resolution == "5m" || resolution == "15m");

    // Reject unsupported intraday resolutions — Tradier only offers 1min/5min/15min bars
    if (resolution == "30" || resolution == "60" || resolution == "240" || resolution == "30m" ||
        resolution == "60m" || resolution == "240m" || resolution == "1h" || resolution == "4h") {
        return {false, std::nullopt,
                "Tradier does not support " + resolution +
                    "-minute bars; supported intraday intervals are 1min/5min/15min only",
                ts};
    }

    if (is_intraday) {
        // timesales endpoint
        QString intrv;
        if (resolution == "1" || resolution == "1m")
            intrv = "1min";
        else if (resolution == "5" || resolution == "5m")
            intrv = "5min";
        else
            intrv = "15min";

        // Per-request day cap: Tradier's timesales window is bounded, so long
        // ranges are split into consecutive sub-windows.
        //   1min        -> <= 20 days per request
        //   5min/15min  -> <= 40 days per request
        const int cap_days = (intrv == "1min") ? 20 : 40;

        // One-window fetch + parse. Returns success flag, error message, and the
        // parsed candles for [winStart, winEnd] (dates as yyyy-MM-dd). The
        // datetime format and ms timestamp conversion match the original code.
        struct WindowResult {
            bool success = false;
            QString error;
            QVector<BrokerCandle> candles;
        };
        auto fetch_window = [&](const QString& winStart, const QString& winEnd) -> WindowResult {
            WindowResult wr;

            // timesales expects datetime format "YYYY-MM-DD HH:MM"
            QString start = winStart + " 09:30";
            QString end = winEnd + " 16:00";

            QString url = base(creds) + "/v1/markets/timesales?symbol=" + ticker + "&interval=" + intrv +
                          "&start=" + QUrl::toPercentEncoding(start) + "&end=" + QUrl::toPercentEncoding(end);

            auto resp = http.get(url, auth_headers(creds));
            if (!resp.success) {
                wr.error = checked_error(resp, "get_history failed");
                return wr;
            }

            QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
            if (!doc.isObject()) {
                wr.error = "get_history: invalid response";
                return wr;
            }

            QJsonValue series_val = doc.object().value("series");
            if (series_val.isNull() || series_val.isString()) {
                // No data for this window — treat as a successful empty result.
                wr.success = true;
                return wr;
            }

            QJsonArray arr = normalize_array(series_val.toObject().value("data"));
            wr.candles.reserve(arr.size());

            for (const QJsonValue& v : arr) {
                QJsonObject o = v.toObject();
                BrokerCandle c;
                // timesales has both "time" (ISO string) and "timestamp" (epoch seconds)
                c.timestamp = static_cast<int64_t>(o.value("timestamp").toDouble()) * 1000LL;
                c.open = o.value("open").toDouble();
                c.high = o.value("high").toDouble();
                c.low = o.value("low").toDouble();
                c.close = o.value("close").toDouble();
                c.volume = static_cast<int64_t>(o.value("volume").toDouble());
                wr.candles.append(c);
            }

            wr.success = true;
            return wr;
        };

        QDate startDate = QDate::fromString(from_date, "yyyy-MM-dd");
        QDate endDate = QDate::fromString(to_date, "yyyy-MM-dd");

        // If either date is unparseable, fall back to a single request over the
        // raw range (preserves original behavior for unexpected inputs).
        bool fits_one_window =
            !startDate.isValid() || !endDate.isValid() || startDate.daysTo(endDate) < cap_days;

        if (fits_one_window) {
            // Single request — identical to the original single-window path.
            WindowResult wr = fetch_window(from_date, to_date);
            if (!wr.success)
                return {false, std::nullopt, wr.error, ts};
            candles = std::move(wr.candles);
        } else {
            // Loop consecutive <= cap_days sub-windows. Each window spans cap_days
            // inclusive of both endpoints, i.e. [w, w + (cap_days - 1)].
            const int kMaxIterations = 60; // safety bound
            QDate winStart = startDate;
            int iterations = 0;
            bool got_data = false;

            while (winStart <= endDate && iterations < kMaxIterations) {
                QDate winEnd = winStart.addDays(cap_days - 1);
                if (winEnd > endDate)
                    winEnd = endDate;

                WindowResult wr = fetch_window(winStart.toString("yyyy-MM-dd"), winEnd.toString("yyyy-MM-dd"));
                if (!wr.success) {
                    // First-window failure → propagate error as the original code did.
                    // Later failure (after collecting data) → break and return partial.
                    if (!got_data)
                        return {false, std::nullopt, wr.error, ts};
                    break;
                }

                if (!wr.candles.isEmpty()) {
                    got_data = true;
                    candles.append(wr.candles);
                }

                winStart = winEnd.addDays(1);
                ++iterations;
            }

            // Sort ascending by timestamp and dedupe (windows are adjacent but a
            // shared boundary bar could appear in two requests).
            std::sort(candles.begin(), candles.end(),
                      [](const BrokerCandle& a, const BrokerCandle& b) { return a.timestamp < b.timestamp; });
            candles.erase(std::unique(candles.begin(), candles.end(),
                                      [](const BrokerCandle& a, const BrokerCandle& b) {
                                          return a.timestamp == b.timestamp;
                                      }),
                          candles.end());
        }

    } else {
        // history endpoint — daily/weekly/monthly
        QString intrv;
        if (resolution == "W" || resolution == "1W")
            intrv = "weekly";
        else if (resolution == "M" || resolution == "1M")
            intrv = "monthly";
        else if (resolution == "D" || resolution == "1D" || resolution == "1d" || resolution.isEmpty())
            intrv = "daily";
        else
            return {false, std::nullopt, "get_history: unsupported resolution '" + resolution + "'", ts};

        QString url = base(creds) + "/v1/markets/history?symbol=" + ticker + "&interval=" + intrv +
                      "&start=" + from_date + "&end=" + to_date;

        auto resp = http.get(url, auth_headers(creds));
        if (!resp.success)
            return {false, std::nullopt, checked_error(resp, "get_history failed"), ts};

        QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
        if (!doc.isObject())
            return {false, std::nullopt, "get_history: invalid response", ts};

        QJsonValue hist_val = doc.object().value("history");
        if (hist_val.isNull() || hist_val.isString())
            return {true, candles, "", ts};

        QJsonArray arr = normalize_array(hist_val.toObject().value("day"));
        candles.reserve(arr.size());

        for (const QJsonValue& v : arr) {
            QJsonObject o = v.toObject();
            BrokerCandle c;
            // date is "YYYY-MM-DD" string — convert to epoch ms
            QDate d = QDate::fromString(o.value("date").toString(), "yyyy-MM-dd");
            c.timestamp = d.isValid()
                              ? static_cast<int64_t>(QDateTime(d, QTime(0, 0, 0), QTimeZone::UTC).toSecsSinceEpoch()) * 1000LL
                              : 0LL;
            c.open = o.value("open").toDouble();
            c.high = o.value("high").toDouble();
            c.low = o.value("low").toDouble();
            c.close = o.value("close").toDouble();
            c.volume = static_cast<int64_t>(o.value("volume").toDouble());
            candles.append(c);
        }
    }

    return {true, candles, "", ts};
}

// ============================================================================
// Pre-trade margin calculator — fallback estimator.
// Tradier exposes no per-order margin endpoint, so we use the shared heuristic
// estimator (BrokerInterface.h::estimate_order_margin).
// ============================================================================
ApiResponse<OrderMargin> TradierBroker::get_order_margins(const BrokerCredentials& /*creds*/,
                                                          const UnifiedOrder& order) {
    return {true, estimate_order_margin(order), "", now_ts()};
}

} // namespace fincept::trading
