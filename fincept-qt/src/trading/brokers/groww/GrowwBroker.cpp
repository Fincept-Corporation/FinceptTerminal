#include "trading/brokers/groww/GrowwBroker.h"

#include "trading/brokers/BrokerHttp.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

namespace fincept::trading {

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// ============================================================================
// Helpers
// ============================================================================

bool GrowwBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 401 || resp.status_code == 403)
        return true;
    // GA005 = insufficient authorization (token expired / invalid).
    // See Groww error taxonomy — GA001 invalid input, GA006 business reject (funds), etc.
    if (error_code(resp) == QLatin1String("GA005"))
        return true;
    return false;
}

QString GrowwBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    if (!resp.json.isEmpty()) {
        // Preferred Groww shape: { status: "FAILURE", error: { code: "GA00x", message: "..." } }
        QJsonValue err_v = resp.json["error"];
        if (err_v.isObject()) {
            QJsonObject e = err_v.toObject();
            QString msg = e["message"].toString();
            QString code = e["code"].toString();
            if (!msg.isEmpty() && !code.isEmpty())
                return QString("[%1] %2").arg(code, msg);
            if (!msg.isEmpty())
                return msg;
        }

        QString msg = resp.json["message"].toString();
        if (!msg.isEmpty())
            return msg;
        if (err_v.isString())
            return err_v.toString();
        QString status = resp.json["status"].toString();
        if (!status.isEmpty() && status != "SUCCESS")
            return QString("Groww error: status=%1").arg(status);
    }
    if (!resp.error.isEmpty())
        return resp.error;
    if (!resp.raw_body.isEmpty())
        return resp.raw_body.left(200);
    return fallback;
}

QString GrowwBroker::error_code(const BrokerHttpResponse& resp) {
    if (resp.json.isEmpty())
        return {};
    QJsonValue err_v = resp.json["error"];
    if (err_v.isObject())
        return err_v.toObject()["code"].toString();
    return {};
}

bool GrowwBroker::is_rate_limited(const BrokerHttpResponse& resp) {
    // Groww surfaces rate-limit breaches via HTTP 429 without a GA code in the body.
    return resp.status_code == 429;
}

// exchange as sent to Groww order endpoints (NSE/BSE for CASH; NSE/BSE for FNO)
QString GrowwBroker::groww_exchange(const QString& exchange) {
    if (exchange == "NFO")
        return "NSE";
    if (exchange == "BFO")
        return "BSE";
    return exchange; // NSE, BSE as-is
}

// segment: CASH for equities, FNO for derivatives
QString GrowwBroker::groww_segment(const QString& exchange) {
    if (exchange == "NFO" || exchange == "BFO")
        return "FNO";
    return "CASH";
}

const BrokerEnumMap<QString>& GrowwBroker::groww_enum_map() {
    static const auto m = [] {
        BrokerEnumMap<QString> x;
        x.set(OrderType::Market, "MARKET");
        x.set(OrderType::Limit, "LIMIT");
        x.set(OrderType::StopLoss, "STOP_LOSS_MARKET");      // SL-M: trigger only, market fill
        x.set(OrderType::StopLossLimit, "STOP_LOSS_LIMIT"); // SL:   trigger + limit price
        x.set(ProductType::Intraday, "MIS");
        x.set(ProductType::Delivery, "CNC");
        x.set(ProductType::Margin, "NRML");
        return x;
    }();
    return m;
}

// resolution string → interval_in_minutes for Groww historical API
int GrowwBroker::resolution_to_minutes(const QString& resolution) {
    return history_interval_minutes(resolution);
}

// Groww /v1/historical/candles accepts 1, 5, 10, 15, 30, 60 minute bars plus 1440 (1D)
// and 10080 (1W). Windows are capped per interval: ≤5m → 30d, 10–30m → 90d, ≥1h → 180d.
int GrowwBroker::history_interval_minutes(const QString& resolution) {
    const QString r = resolution.toLower();
    if (r == "1" || r == "1m" || r == "1min")
        return 1;
    if (r == "3" || r == "3m" || r == "3min")
        return 3;
    if (r == "5" || r == "5m" || r == "5min")
        return 5;
    if (r == "10" || r == "10m" || r == "10min")
        return 10;
    if (r == "15" || r == "15m" || r == "15min")
        return 15;
    if (r == "30" || r == "30m" || r == "30min")
        return 30;
    if (r == "60" || r == "1h" || r == "1hour")
        return 60;
    if (r == "240" || r == "4h" || r == "4hours")
        return 240;
    if (r == "d" || r == "1d" || r == "day" || r == "1day")
        return 1440;
    if (r == "w" || r == "1w" || r == "week" || r == "1week")
        return 10080;
    if (r == "1month")
        return 43200;
    return 1;
}

// Groww /v1/historical/candles takes a string token, NOT a number.
// Token enum per docs: 1min, 3min, 5min, 10min, 15min, 30min, 1hour, 4hours,
// 1day, 1week, 1month.
QString GrowwBroker::history_interval_token(const QString& resolution) {
    switch (history_interval_minutes(resolution)) {
        case 1:    return "1min";
        case 3:    return "3min";
        case 5:    return "5min";
        case 10:   return "10min";
        case 15:   return "15min";
        case 30:   return "30min";
        case 60:   return "1hour";
        case 240:  return "4hours";
        case 1440: return "1day";
        case 10080:return "1week";
        case 43200:return "1month";
        default:   return "1min";
    }
}

// ============================================================================
// Auth headers — Bearer access_token
// ============================================================================

QMap<QString, QString> GrowwBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Authorization", "Bearer " + creds.access_token},
        {"Content-Type", "application/json"},
        {"Accept", "application/json"},
        {"X-API-VERSION", "1.0"},
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
    body["key_type"] = "approval";
    body["checksum"] = checksum;
    body["timestamp"] = timestamp;

    QMap<QString, QString> headers = {
        {"Authorization", "Bearer " + api_key},
        {"Content-Type", "application/json"},
        {"Accept", "application/json"},
    };

    auto resp = BrokerHttp::instance().post_json("https://api.groww.in/v1/token/api/access", body, headers);

    if (!resp.success)
        return {false, "", "", "", checked_error(resp, "Network error"), ""};

    QString token = resp.json["token"].toString();
    if (token.isEmpty())
        return {false, "", "", "", checked_error(resp, "No token in response"), ""};

    return {true, token, "", "", "", ""};
}

// ============================================================================
// Phase 3: Order operations
// ============================================================================

OrderPlaceResponse GrowwBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    auto hdrs = auth_headers(creds);

    QJsonObject body;
    body["trading_symbol"] = order.symbol;
    body["quantity"] = order.quantity;
    body["validity"] = "DAY";
    body["exchange"] = groww_exchange(order.exchange);
    body["segment"] = groww_segment(order.exchange);
    body["product"] = groww_enum_map().product_or(order.product_type, "CNC");
    body["order_type"] = groww_enum_map().order_type_or(order.order_type, "MARKET");
    body["transaction_type"] = (order.side == OrderSide::Buy) ? "BUY" : "SELL";

    if (order.order_type != OrderType::Market)
        body["price"] = order.price;
    if (order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit)
        body["trigger_price"] = order.stop_price;

    auto resp = BrokerHttp::instance().post_json("https://api.groww.in/v1/order/create", body, hdrs);

    if (!resp.success)
        return {false, "", checked_error(resp, "Network error")};

    if (is_token_expired(resp))
        return {false, "", "[TOKEN_EXPIRED]"};

    QJsonObject payload = resp.json["payload"].toObject();
    // Groww returns `groww_order_id` (their canonical ID); fall back to
    // `order_reference_id` (exchange-side) then plain `order_id` for older deployments.
    QString order_id = payload["groww_order_id"].toString();
    if (order_id.isEmpty())
        order_id = payload["order_reference_id"].toString();
    if (order_id.isEmpty())
        order_id = payload["order_id"].toString();
    if (order_id.isEmpty())
        return {false, "", checked_error(resp, "No order_id in response")};

    return {true, order_id, ""};
}

ApiResponse<QJsonObject> GrowwBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                   const QJsonObject& mods) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    // Groww requires groww_order_id + order_type + segment in the body.
    // Caller may pass segment/exchange in the mods JSON; default to CASH when absent.
    QString segment = mods.value("segment").toString();
    if (segment.isEmpty()) {
        QString exchange = mods.value("exchange").toString();
        segment = groww_segment(exchange.isEmpty() ? QStringLiteral("NSE") : exchange);
    }

    QJsonObject body;
    body["groww_order_id"] = order_id;
    body["segment"] = segment;
    // Only forward order_type when caller passes it. Defaulting to "LIMIT"
    // would silently flip a MARKET order to LIMIT on a price-only modify.
    if (mods.contains("order_type"))
        body["order_type"] = mods["order_type"].toString();
    if (mods.contains("quantity"))
        body["quantity"] = mods["quantity"];
    if (mods.contains("price"))
        body["price"] = mods["price"];
    if (mods.contains("trigger_price"))
        body["trigger_price"] = mods["trigger_price"];

    auto resp = BrokerHttp::instance().post_json("https://api.groww.in/v1/order/modify", body, hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

    QString status = resp.json["status"].toString();
    if (status != "SUCCESS")
        return {false, std::nullopt, checked_error(resp, "Modify failed"), ts};

    return {true, resp.json, "", ts};
}

ApiResponse<QJsonObject> GrowwBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    // Groww requires groww_order_id + segment. IBroker::cancel_order has no segment,
    // so try CASH first; if the order isn't found there, retry on FNO.
    auto do_cancel = [&](const QString& segment) {
        QJsonObject body;
        body["groww_order_id"] = order_id;
        body["segment"] = segment;
        return BrokerHttp::instance().post_json("https://api.groww.in/v1/order/cancel", body, hdrs);
    };

    auto resp = do_cancel(QStringLiteral("CASH"));
    // Retry on FNO if CASH cancel didn't succeed. BrokerHttp marks 4xx as
    // success=false, so the previous `success && !SUCCESS` form never
    // triggered the FNO retry; widen to "anything not a SUCCESS response".
    const bool cash_ok = resp.success && resp.json["status"].toString() == "SUCCESS";
    if (!cash_ok)
        resp = do_cancel(QStringLiteral("FNO"));

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
        if (s == "COMPLETED" || s == "EXECUTED" || s == "DELIVERY_AWAITED")
            return "filled";
        if (s == "OPEN" || s == "NEW" || s == "ACKED" || s == "APPROVED")
            return "open";
        if (s == "CANCELLED" || s == "CANCELLATION_REQUESTED")
            return "cancelled";
        if (s == "REJECTED" || s == "FAILED")
            return "rejected";
        if (s == "TRIGGER_PENDING" || s == "MODIFICATION_REQUESTED")
            return "pending";
        return "open";
    };

    auto fetch_segment = [&](const QString& segment) -> bool {
        constexpr int PAGE_SIZE = 25; // Groww API max per page
        int page = 0;
        while (true) {
            QString url = QString("https://api.groww.in/v1/order/list?segment=%1&page=%2&page_size=%3")
                              .arg(segment)
                              .arg(page)
                              .arg(PAGE_SIZE);
            auto resp = BrokerHttp::instance().get(url, hdrs);
            if (!resp.success)
                return false;
            if (is_token_expired(resp))
                return false;

            QJsonObject payload = resp.json["payload"].toObject();
            QJsonArray list = payload["order_list"].toArray();
            for (const auto& item : list) {
                QJsonObject o = item.toObject();
                BrokerOrderInfo info;
                // Groww canonical id is groww_order_id; fall back for legacy responses.
                info.order_id = o["groww_order_id"].toString();
                if (info.order_id.isEmpty())
                    info.order_id = o["order_id"].toString();
                info.symbol = o["trading_symbol"].toString();
                info.exchange = o["exchange"].toString();
                info.quantity = o["quantity"].toInt();
                info.filled_qty = o["filled_quantity"].toInt();
                info.price = o["price"].toDouble();
                info.trigger_price = o["trigger_price"].toDouble();
                // Doc-canonical status field is `order_status`; older endpoints used `status`.
                QString raw_status = o["order_status"].toString();
                if (raw_status.isEmpty())
                    raw_status = o["status"].toString();
                info.status = parse_status(raw_status);
                info.side = (o["transaction_type"].toString() == "BUY") ? "buy" : "sell";
                info.order_type = o["order_type"].toString();
                info.product_type = o["product"].toString();
                // Spec: created_at (ISO8601). Fallback: legacy order_time.
                info.timestamp = o["created_at"].toString();
                if (info.timestamp.isEmpty())
                    info.timestamp = o["order_time"].toString();
                orders.append(info);
            }
            if (list.size() < PAGE_SIZE)
                break; // last page
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

    // Groww has no global trade book — trades are fetched per order via
    // GET /v1/order/trades/{groww_order_id}?segment=...&page=0&page_size=50.
    // Aggregate trades across filled orders in both segments into a single payload.
    QJsonArray all_trades;

    auto fetch_segment_trades = [&](const QString& segment) {
        constexpr int PAGE_SIZE = 25;
        int page = 0;
        while (true) {
            QString list_url = QString("https://api.groww.in/v1/order/list?segment=%1&page=%2&page_size=%3")
                                   .arg(segment)
                                   .arg(page)
                                   .arg(PAGE_SIZE);
            auto list_resp = BrokerHttp::instance().get(list_url, hdrs);
            if (!list_resp.success || is_token_expired(list_resp))
                return;

            QJsonArray orders = list_resp.json["payload"].toObject()["order_list"].toArray();
            for (const auto& it : orders) {
                QJsonObject o = it.toObject();
                QString status = o["status"].toString();
                if (o["filled_quantity"].toInt() == 0 && status != "COMPLETED" && status != "EXECUTED")
                    continue;

                QString oid = o["groww_order_id"].toString();
                if (oid.isEmpty())
                    oid = o["order_id"].toString();
                if (oid.isEmpty())
                    continue;

                QString trades_url = QString("https://api.groww.in/v1/order/trades/%1?segment=%2&page=0&page_size=50")
                                         .arg(oid, segment);
                auto tr = BrokerHttp::instance().get(trades_url, hdrs);
                if (!tr.success || is_token_expired(tr))
                    continue;

                QJsonArray tlist = tr.json["payload"].toObject()["trade_list"].toArray();
                for (const auto& t : tlist)
                    all_trades.append(t);
            }
            if (orders.size() < PAGE_SIZE)
                break;
            ++page;
        }
    };

    fetch_segment_trades(QStringLiteral("CASH"));
    fetch_segment_trades(QStringLiteral("FNO"));

    QJsonObject result;
    QJsonObject payload;
    payload["trade_list"] = all_trades;
    result["status"] = "SUCCESS";
    result["payload"] = payload;
    return {true, result, "", ts};
}

ApiResponse<QVector<BrokerPosition>> GrowwBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);
    QVector<BrokerPosition> positions;

    auto fetch_segment = [&](const QString& segment) -> bool {
        QString url = QString("https://api.groww.in/v1/positions/user?segment=%1").arg(segment);
        auto resp = BrokerHttp::instance().get(url, hdrs);
        if (!resp.success)
            return false;
        if (is_token_expired(resp))
            return false;

        // Positions live under payload.positions per Groww docs.
        QJsonValue payload_v = resp.json["payload"];
        QJsonArray list = payload_v.isObject() ? payload_v.toObject()["positions"].toArray() : payload_v.toArray();
        for (const auto& item : list) {
            QJsonObject p = item.toObject();

            // Net open quantity = credit (buy) - debit (sell). Prefer the API's
            // explicit `quantity` if present; otherwise derive from credit/debit.
            int qty = 0;
            if (p.contains("quantity") && !p["quantity"].isNull()) {
                qty = p["quantity"].toInt();
            } else {
                int credit = p["credit_quantity"].toInt();
                int debit = p["debit_quantity"].toInt();
                qty = credit - debit;
            }
            if (qty == 0)
                continue;

            // avg_price preference: net_price → credit_price (for long) → debit_price (for short)
            double avg = p["net_price"].toDouble();
            if (avg == 0.0)
                avg = (qty > 0) ? p["credit_price"].toDouble() : p["debit_price"].toDouble();

            BrokerPosition pos;
            pos.symbol = p["trading_symbol"].toString();
            pos.exchange = p["exchange"].toString();
            pos.quantity = qty;
            pos.avg_price = avg;
            pos.ltp = 0.0; // Not returned by positions endpoint; query live-data/ltp separately if needed.
            pos.pnl = p["realised_pnl"].toDouble();
            pos.product_type = p["product"].toString();
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

    auto resp = BrokerHttp::instance().get("https://api.groww.in/v1/holdings/user", hdrs);

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
        holding.symbol = h["trading_symbol"].toString();
        // exchange is not in the Groww holdings response; NSE is the DP default for equity holdings.
        holding.exchange = h.contains("exchange") ? h["exchange"].toString() : QStringLiteral("NSE");
        holding.quantity = h["quantity"].toInt();
        holding.avg_price = h["average_price"].toDouble();
        holding.ltp = 0.0; // Not returned by holdings endpoint; query live-data/ltp separately if needed.
        holding.pnl = 0.0; // Derived value — compute in the service layer once LTP is fetched.
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
    // Spec shape: top-level has clear_cash, net_margin_used, adhoc_margin, payin_amount.
    // Collateral can live at top level as `collateral_available` (older deployments)
    // OR nested under `equity_margin_details.collateral_available` / `collateral`
    // (current docs). Try both.
    double clear_cash = payload["clear_cash"].toDouble();
    double used = payload["net_margin_used"].toDouble();
    double collateral = payload["collateral_available"].toDouble();
    if (collateral == 0.0) {
        const QJsonObject eq = payload["equity_margin_details"].toObject();
        collateral = eq["collateral_available"].toDouble();
        if (collateral == 0.0)
            collateral = eq["collateral"].toDouble();
    }

    BrokerFunds funds;
    funds.available_balance = clear_cash + collateral;
    funds.used_margin = used;
    funds.total_balance = funds.available_balance + funds.used_margin;
    funds.collateral = collateral;
    funds.raw_data = payload;

    return {true, funds, "", ts};
}

GrowwBroker::SymbolRef GrowwBroker::split_symbol(const QString& sym) {
    SymbolRef r;
    r.orig = sym;
    r.exchange = QStringLiteral("NSE");
    r.trading_symbol = sym;
    const int colon = sym.indexOf(':');
    if (colon != -1) {
        r.exchange = sym.left(colon);
        r.trading_symbol = sym.mid(colon + 1);
    }
    r.segment = groww_segment(r.exchange);
    r.exchange_symbol = groww_exchange(r.exchange) + "_" + r.trading_symbol;
    return r;
}

bool GrowwBroker::fetch_ltp_batch(const BrokerCredentials& creds, const QString& segment,
                                  const QVector<SymbolRef>& refs, QVector<BrokerQuote>& out) {
    if (refs.isEmpty())
        return true;
    auto hdrs = auth_headers(creds);

    // Groww accepts up to 50 symbols per call — chunk accordingly.
    constexpr int BATCH = 50;
    for (int start = 0; start < refs.size(); start += BATCH) {
        const int end = std::min<int>(start + BATCH, refs.size());

        QStringList syms;
        syms.reserve(end - start);
        for (int i = start; i < end; ++i)
            syms << refs[i].exchange_symbol;

        // exchange_symbols is a repeatable query param: ?exchange_symbols=NSE_A&exchange_symbols=NSE_B
        QUrl url(QStringLiteral("https://api.groww.in/v1/live-data/ltp"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("segment"), segment);
        for (const auto& s : syms)
            q.addQueryItem(QStringLiteral("exchange_symbols"), s);
        url.setQuery(q);

        auto resp = BrokerHttp::instance().get(url.toString(), hdrs);
        if (!resp.success)
            return false;
        if (is_token_expired(resp))
            return false;

        // Response shape: payload.ltp is a map { "NSE_RELIANCE": 2850.5, ... }
        QJsonObject payload = resp.json["payload"].toObject();
        QJsonObject ltp_map = payload["ltp"].toObject();
        if (ltp_map.isEmpty())
            ltp_map = payload; // some variants return the map directly

        for (int i = start; i < end; ++i) {
            const auto& ref = refs[i];
            if (!ltp_map.contains(ref.exchange_symbol))
                continue;
            double ltp = ltp_map.value(ref.exchange_symbol).toDouble();

            // upsert by orig symbol
            int idx = -1;
            for (int k = 0; k < out.size(); ++k) {
                if (out[k].symbol == ref.orig) {
                    idx = k;
                    break;
                }
            }
            if (idx == -1) {
                BrokerQuote bq;
                bq.symbol = ref.orig;
                bq.ltp = ltp;
                out.append(bq);
            } else {
                out[idx].ltp = ltp;
            }
        }
    }
    return true;
}

bool GrowwBroker::fetch_ohlc_batch(const BrokerCredentials& creds, const QString& segment,
                                   const QVector<SymbolRef>& refs, QVector<BrokerQuote>& out) {
    if (refs.isEmpty())
        return true;
    auto hdrs = auth_headers(creds);

    constexpr int BATCH = 50;
    for (int start = 0; start < refs.size(); start += BATCH) {
        const int end = std::min<int>(start + BATCH, refs.size());

        QUrl url(QStringLiteral("https://api.groww.in/v1/live-data/ohlc"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("segment"), segment);
        for (int i = start; i < end; ++i)
            q.addQueryItem(QStringLiteral("exchange_symbols"), refs[i].exchange_symbol);
        url.setQuery(q);

        auto resp = BrokerHttp::instance().get(url.toString(), hdrs);
        if (!resp.success)
            return false;
        if (is_token_expired(resp))
            return false;

        // Response shape: payload.ohlc is a map { "NSE_RELIANCE": {open,high,low,close,...}, ... }
        QJsonObject payload = resp.json["payload"].toObject();
        QJsonObject ohlc_map = payload["ohlc"].toObject();
        if (ohlc_map.isEmpty())
            ohlc_map = payload;

        for (int i = start; i < end; ++i) {
            const auto& ref = refs[i];
            if (!ohlc_map.contains(ref.exchange_symbol))
                continue;
            QJsonObject ohlc = ohlc_map.value(ref.exchange_symbol).toObject();

            int idx = -1;
            for (int k = 0; k < out.size(); ++k) {
                if (out[k].symbol == ref.orig) {
                    idx = k;
                    break;
                }
            }
            BrokerQuote* q_ptr = nullptr;
            if (idx == -1) {
                BrokerQuote bq;
                bq.symbol = ref.orig;
                out.append(bq);
                q_ptr = &out.last();
            } else {
                q_ptr = &out[idx];
            }
            q_ptr->open = ohlc["open"].toDouble();
            q_ptr->high = ohlc["high"].toDouble();
            q_ptr->low = ohlc["low"].toDouble();
            q_ptr->close = ohlc["close"].toDouble();
            if (ohlc.contains("volume"))
                q_ptr->volume = ohlc["volume"].toDouble();
        }
    }
    return true;
}

ApiResponse<QVector<BrokerQuote>> GrowwBroker::get_quotes(const BrokerCredentials& creds,
                                                          const QVector<QString>& symbols) {
    int64_t ts = now_ts();
    QVector<BrokerQuote> quotes;

    // Single-symbol path: the /live-data/quote endpoint returns the full snapshot
    // (ltp + ohlc + volume + bid/ask). Prefer it when only one symbol is requested.
    if (symbols.size() == 1) {
        auto hdrs = auth_headers(creds);
        const auto ref = split_symbol(symbols.first());
        QString url = QString("https://api.groww.in/v1/live-data/quote"
                              "?exchange=%1&segment=%2&trading_symbol=%3")
                          .arg(groww_exchange(ref.exchange), ref.segment, ref.trading_symbol);
        auto resp = BrokerHttp::instance().get(url, hdrs);
        if (!resp.success)
            return {false, std::nullopt, checked_error(resp, "Network error"), ts};
        if (is_token_expired(resp))
            return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};

        QJsonObject payload = resp.json["payload"].toObject();
        QJsonObject ohlc = payload["ohlc"].toObject();

        BrokerQuote q;
        q.symbol = ref.orig;
        q.ltp = payload["last_price"].toDouble();
        q.open = ohlc["open"].toDouble();
        q.high = ohlc["high"].toDouble();
        q.low = ohlc["low"].toDouble();
        q.close = ohlc["close"].toDouble();
        q.volume = payload["volume"].toDouble();
        quotes.append(q);
        return {true, quotes, "", ts};
    }

    // Batch path: group symbols by segment (CASH/FNO are separate buckets on Groww's
    // /live-data/ltp and /live-data/ohlc), then issue at most 2 RPCs per segment
    // (ltp + ohlc) per 50-symbol chunk. Drops N per-symbol calls to O(1) per 50.
    QMap<QString, QVector<SymbolRef>> by_segment;
    for (const QString& sym : symbols) {
        auto ref = split_symbol(sym);
        by_segment[ref.segment].append(ref);
    }

    for (auto it = by_segment.constBegin(); it != by_segment.constEnd(); ++it) {
        if (!fetch_ltp_batch(creds, it.key(), it.value(), quotes))
            continue; // best-effort — partial batch still returns what succeeded
        fetch_ohlc_batch(creds, it.key(), it.value(), quotes);
    }

    return {true, quotes, "", ts};
}

ApiResponse<QVector<BrokerCandle>> GrowwBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                            const QString& resolution, const QString& from_date,
                                                            const QString& to_date) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    QString exchange = "NSE";
    QString trading_symbol = symbol;
    int colon = symbol.indexOf(':');
    if (colon != -1) {
        exchange = symbol.left(colon);
        trading_symbol = symbol.mid(colon + 1);
    }

    QString segment = groww_segment(exchange);
    QString ex = groww_exchange(exchange);
    const QString interval_token = history_interval_token(resolution);

    // Groww /v1/historical/candles is the current endpoint (replaces /candle/range).
    // Expects datetime format "YYYY-MM-DD HH:MM:SS"; callers pass bare dates, so pin
    // to the NSE session window.
    //
    // groww_symbol is "EXCHANGE-TRADINGSYMBOL" (e.g. "NSE-WIPRO"); for options the
    // suffix is the option-contract code (e.g. "NSE-NIFTY-30Sep25-24650-CE"). The
    // hyphen prefix is required — passing the bare trading symbol returns 4xx.
    QString start_time = from_date + " 09:15:00";
    QString end_time = to_date + " 15:30:00";
    const QString groww_symbol = ex + "-" + trading_symbol;

    QUrl qurl(QStringLiteral("https://api.groww.in/v1/historical/candles"));
    QUrlQuery qq;
    qq.addQueryItem(QStringLiteral("exchange"), ex);
    qq.addQueryItem(QStringLiteral("segment"), segment);
    qq.addQueryItem(QStringLiteral("groww_symbol"), groww_symbol);
    qq.addQueryItem(QStringLiteral("start_time"), start_time);
    qq.addQueryItem(QStringLiteral("end_time"), end_time);
    qq.addQueryItem(QStringLiteral("candle_interval"), interval_token);
    qurl.setQuery(qq);
    const QString url = qurl.toString();

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
        if (c.size() < 6)
            continue;
        BrokerCandle candle;
        // [timestamp_ms, open, high, low, close, volume]
        candle.timestamp = static_cast<int64_t>(c[0].toDouble());
        candle.open = c[1].toDouble();
        candle.high = c[2].toDouble();
        candle.low = c[3].toDouble();
        candle.close = c[4].toDouble();
        candle.volume = c[5].toDouble();
        result.append(candle);
    }

    return {true, result, "", ts};
}

// ============================================================================
// Pre-trade margin calculator  — POST /v1/margins/detail/orders?segment=...
// ============================================================================

QJsonObject GrowwBroker::order_to_margin_row(const UnifiedOrder& order) {
    QJsonObject row;
    row["trading_symbol"] = order.symbol;
    row["quantity"] = order.quantity;
    row["exchange"] = groww_exchange(order.exchange);
    row["product"] = groww_enum_map().product_or(order.product_type, "CNC");
    row["order_type"] = groww_enum_map().order_type_or(order.order_type, "MARKET");
    row["transaction_type"] = (order.side == OrderSide::Buy) ? "BUY" : "SELL";
    // Margin API wants the price for LIMIT/SL-L orders; MARKET can be omitted.
    if (order.order_type != OrderType::Market)
        row["price"] = order.price;
    if (order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit)
        row["trigger_price"] = order.stop_price;
    return row;
}

ApiResponse<OrderMargin> GrowwBroker::get_order_margins(const BrokerCredentials& creds,
                                                        const UnifiedOrder& order) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    const QString segment = groww_segment(order.exchange);
    QJsonArray arr;
    arr.append(order_to_margin_row(order));

    QString url = QString("https://api.groww.in/v1/margins/detail/orders?segment=%1").arg(segment);
    auto resp = BrokerHttp::instance().post_json_array(url, arr, hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};
    if (resp.json["status"].toString() != "SUCCESS")
        return {false, std::nullopt, checked_error(resp, "Margin calc failed"), ts};

    // Response payload shape varies by segment; common fields:
    //   total_requirement, exposure_required, span_required, option_buy_premium,
    //   brokerage_and_charges, cash_cnc_margin_required, cash_mis_margin_required
    QJsonObject payload = resp.json["payload"].toObject();
    OrderMargin m;
    m.symbol = order.symbol;
    m.exchange = order.exchange;
    m.side = (order.side == OrderSide::Buy) ? "BUY" : "SELL";
    m.quantity = order.quantity;
    m.price = order.price;
    m.total = payload["total_requirement"].toDouble();
    m.var_margin = payload["span_required"].toDouble(); // closest analogue for F&O; 0 for cash
    m.elm = payload["exposure_required"].toDouble();
    m.additional = payload["option_buy_premium"].toDouble();
    m.cash = payload.contains("cash_cnc_margin_required")
                 ? payload["cash_cnc_margin_required"].toDouble()
                 : payload["cash_mis_margin_required"].toDouble();
    m.bo_margin = 0.0; // Groww has no bracket orders
    m.pnl = 0.0;
    if (m.total > 0 && order.price > 0 && order.quantity > 0)
        m.leverage = (order.price * order.quantity) / m.total;
    return {true, m, "", ts};
}

ApiResponse<BasketMargin> GrowwBroker::get_basket_margins(const BrokerCredentials& creds,
                                                          const QVector<UnifiedOrder>& orders) {
    int64_t ts = now_ts();
    if (orders.isEmpty())
        return {true, BasketMargin{}, "", ts};

    auto hdrs = auth_headers(creds);
    // Groww scopes margin calls by segment (query param). Group legs by segment and
    // issue one RPC per bucket; aggregate totals client-side.
    QMap<QString, QVector<UnifiedOrder>> by_segment;
    for (const auto& o : orders)
        by_segment[groww_segment(o.exchange)].append(o);

    BasketMargin result;
    for (auto it = by_segment.constBegin(); it != by_segment.constEnd(); ++it) {
        QJsonArray arr;
        for (const auto& o : it.value())
            arr.append(order_to_margin_row(o));

        QString url = QString("https://api.groww.in/v1/margins/detail/orders?segment=%1").arg(it.key());
        QJsonDocument doc(arr);
        auto resp = BrokerHttp::instance().post_raw(url, doc.toJson(QJsonDocument::Compact), hdrs);

        if (!resp.success)
            return {false, std::nullopt, checked_error(resp, "Network error"), ts};
        if (is_token_expired(resp))
            return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};
        if (resp.json["status"].toString() != "SUCCESS")
            return {false, std::nullopt, checked_error(resp, "Basket margin failed"), ts};

        QJsonObject payload = resp.json["payload"].toObject();
        const double total = payload["total_requirement"].toDouble();
        // Groww returns the netted total for the whole segment basket in one shot.
        result.initial_margin += total;
        result.final_margin += total; // no pre/post netting split in Groww response

        // Per-order rows aren't broken out by Groww; emit a best-effort synthetic row.
        for (const auto& o : it.value()) {
            OrderMargin m;
            m.symbol = o.symbol;
            m.exchange = o.exchange;
            m.side = (o.side == OrderSide::Buy) ? "BUY" : "SELL";
            m.quantity = o.quantity;
            m.price = o.price;
            m.total = 0.0; // unknown per-leg — only the segment total is returned
            result.orders.append(m);
        }
    }
    return {true, result, "", ts};
}

// ============================================================================
// User profile — GET /v1/user/detail
// Groww exposes only: vendor_user_id, ucc, nse_enabled, bse_enabled,
// ddpi_enabled, active_segments. No name / email over the trade API.
// ============================================================================

ApiResponse<QJsonObject> GrowwBroker::get_profile(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    auto resp = BrokerHttp::instance().get("https://api.groww.in/v1/user/detail", hdrs);
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};
    if (resp.json["status"].toString() != "SUCCESS")
        return {false, std::nullopt, checked_error(resp, "Profile fetch failed"), ts};

    return {true, resp.json["payload"].toObject(), "", ts};
}

// ============================================================================
// Smart Orders (GTT / OCO) — /v1/order-advance/*
// Groww has no iceberg/slice — slicing must be done client-side. These hooks
// expose GTT + OCO through the standard IBroker::gtt_* interface.
// ============================================================================

// Build the body for Groww Smart Orders. The current spec uses:
//   transaction_type / quantity / net_position_quantity at the top level,
//   product_type (not "product"), duration (DAY/IOC),
//   For OCO: nested `target` and `stop_loss` objects each with `trigger_price`
//   and optional `price` (limit). Previously the impl used `order`/`secondary_order`
//   with a `product` key which Smart Orders rejects.
QJsonObject GrowwBroker::gtt_to_advance_body(const GttOrder& g, bool is_create) {
    QJsonObject body;
    if (is_create)
        body["reference_id"] = g.gtt_id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : g.gtt_id;
    body["smart_order_type"] = (g.type == GttOrderType::OCO) ? "OCO" : "GTT";
    body["segment"] = groww_segment(g.exchange);
    body["exchange"] = groww_exchange(g.exchange);
    body["trading_symbol"] = g.symbol;

    if (g.triggers.isEmpty())
        return body;

    const GttTrigger& t0 = g.triggers.first();
    body["transaction_type"] = (t0.side == OrderSide::Buy) ? "BUY" : "SELL";
    body["net_position_quantity"] = t0.quantity;
    body["quantity"] = t0.quantity;
    body["product_type"] = groww_enum_map().product_or(t0.product, "CNC");
    body["order_type"] = groww_enum_map().order_type_or(t0.order_type, "MARKET");
    body["duration"] = "DAY";

    if (g.type == GttOrderType::OCO && g.triggers.size() > 1) {
        const GttTrigger& t1 = g.triggers.at(1);
        // Convention: triggers[0] = target leg (higher price for long),
        // triggers[1] = stop-loss leg. Callers should order them this way;
        // we don't second-guess the prices.
        QJsonObject target;
        target["trigger_price"] = t0.trigger_price;
        if (t0.limit_price > 0)
            target["price"] = t0.limit_price;
        body["target"] = target;

        QJsonObject stop;
        stop["trigger_price"] = t1.trigger_price;
        if (t1.limit_price > 0)
            stop["price"] = t1.limit_price;
        body["stop_loss"] = stop;
    } else {
        // Single GTT: flat trigger_price + optional price (limit).
        body["trigger_price"] = t0.trigger_price;
        if (t0.limit_price > 0)
            body["price"] = t0.limit_price;
    }
    return body;
}

GttOrder GrowwBroker::parse_advance_to_gtt(const QJsonObject& payload) {
    GttOrder g;
    g.gtt_id = payload["smart_order_id"].toString();
    if (g.gtt_id.isEmpty())
        g.gtt_id = payload["reference_id"].toString();
    g.symbol = payload["trading_symbol"].toString();
    g.exchange = payload.contains("exchange") ? payload["exchange"].toString() : QStringLiteral("NSE");
    g.type = (payload["smart_order_type"].toString() == "OCO") ? GttOrderType::OCO : GttOrderType::Single;
    g.status = payload["status"].toString().toLower();
    g.created_at = payload["created_at"].toString();
    g.updated_at = payload["updated_at"].toString();

    const QString tx = payload["transaction_type"].toString();
    const auto side = (tx == "BUY") ? OrderSide::Buy : OrderSide::Sell;
    const double qty = payload["quantity"].toDouble(payload["net_position_quantity"].toDouble());

    if (g.type == GttOrderType::OCO) {
        // OCO: target + stop_loss nested objects.
        const QJsonObject target = payload["target"].toObject();
        const QJsonObject stop = payload["stop_loss"].toObject();
        GttTrigger t0;
        t0.trigger_price = target["trigger_price"].toDouble();
        t0.limit_price = target["price"].toDouble();
        t0.quantity = qty;
        t0.side = side;
        g.triggers.append(t0);
        GttTrigger t1;
        t1.trigger_price = stop["trigger_price"].toDouble();
        t1.limit_price = stop["price"].toDouble();
        t1.quantity = qty;
        t1.side = side;
        g.triggers.append(t1);
    } else {
        GttTrigger t0;
        t0.trigger_price = payload["trigger_price"].toDouble();
        t0.limit_price = payload["price"].toDouble();
        t0.quantity = qty;
        t0.side = side;
        g.triggers.append(t0);
    }
    return g;
}

GttPlaceResponse GrowwBroker::gtt_place(const BrokerCredentials& creds, const GttOrder& order) {
    auto hdrs = auth_headers(creds);
    QJsonObject body = gtt_to_advance_body(order, /*is_create=*/true);

    auto resp = BrokerHttp::instance().post_json("https://api.groww.in/v1/order-advance/create", body, hdrs);
    if (!resp.success)
        return {false, "", checked_error(resp, "Network error")};
    if (is_token_expired(resp))
        return {false, "", "[TOKEN_EXPIRED]"};
    if (resp.json["status"].toString() != "SUCCESS")
        return {false, "", checked_error(resp, "GTT create failed")};

    QJsonObject payload = resp.json["payload"].toObject();
    QString id = payload["smart_order_id"].toString();
    if (id.isEmpty())
        id = payload["reference_id"].toString();
    return {true, id, ""};
}

ApiResponse<GttOrder> GrowwBroker::gtt_get(const BrokerCredentials& creds, const QString& gtt_id) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    // Groww needs segment + smart_order_type in the path. Caller only passed the id,
    // so try both (CASH/GTT, CASH/OCO, FNO/GTT, FNO/OCO). Stop at the first match.
    const QStringList segs = {"CASH", "FNO"};
    const QStringList types = {"GTT", "OCO"};
    for (const auto& seg : segs) {
        for (const auto& type : types) {
            QString url = QString("https://api.groww.in/v1/order-advance/status/%1/%2/internal/%3")
                              .arg(seg, type, gtt_id);
            auto resp = BrokerHttp::instance().get(url, hdrs);
            if (!resp.success || is_token_expired(resp))
                continue;
            if (resp.json["status"].toString() != "SUCCESS")
                continue;
            QJsonObject payload = resp.json["payload"].toObject();
            if (payload.isEmpty())
                continue;
            return {true, parse_advance_to_gtt(payload), "", ts};
        }
    }
    return {false, std::nullopt, "GTT not found", ts};
}

ApiResponse<QVector<GttOrder>> GrowwBroker::gtt_list(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    QVector<GttOrder> out;
    const QStringList segs = {"CASH", "FNO"};
    const QStringList types = {"GTT", "OCO"};
    for (const auto& seg : segs) {
        for (const auto& type : types) {
            QString url = QString("https://api.groww.in/v1/order-advance/list?segment=%1"
                                  "&smart_order_type=%2&page=0&page_size=100")
                              .arg(seg, type);
            auto resp = BrokerHttp::instance().get(url, hdrs);
            if (!resp.success || is_token_expired(resp))
                continue;
            if (resp.json["status"].toString() != "SUCCESS")
                continue;
            QJsonArray list = resp.json["payload"].toObject()["smart_order_list"].toArray();
            for (const auto& v : list) {
                QJsonObject o = v.toObject();
                if (!o.isEmpty())
                    out.append(parse_advance_to_gtt(o));
            }
        }
    }
    return {true, out, "", ts};
}

ApiResponse<GttOrder> GrowwBroker::gtt_modify(const BrokerCredentials& creds, const QString& gtt_id,
                                               const GttOrder& updated) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);
    QJsonObject body = gtt_to_advance_body(updated, /*is_create=*/false);

    QString url = QString("https://api.groww.in/v1/order-advance/modify/%1").arg(gtt_id);
    auto resp = BrokerHttp::instance().put_json(url, body, hdrs);
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Network error"), ts};
    if (is_token_expired(resp))
        return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};
    if (resp.json["status"].toString() != "SUCCESS")
        return {false, std::nullopt, checked_error(resp, "GTT modify failed"), ts};

    // Server echoes the updated smart order; re-parse so callers see the canonical state.
    return {true, parse_advance_to_gtt(resp.json["payload"].toObject()), "", ts};
}

ApiResponse<QJsonObject> GrowwBroker::gtt_cancel(const BrokerCredentials& creds, const QString& gtt_id) {
    int64_t ts = now_ts();
    auto hdrs = auth_headers(creds);

    // Cancel endpoint needs segment + type in the path. Walk the 4-way grid as in gtt_get.
    const QStringList segs = {"CASH", "FNO"};
    const QStringList types = {"GTT", "OCO"};
    BrokerHttpResponse last;
    for (const auto& seg : segs) {
        for (const auto& type : types) {
            QString url = QString("https://api.groww.in/v1/order-advance/cancel/%1/%2/%3").arg(seg, type, gtt_id);
            last = BrokerHttp::instance().post_json(url, QJsonObject{}, hdrs);
            if (!last.success)
                continue;
            if (is_token_expired(last))
                return {false, std::nullopt, "[TOKEN_EXPIRED]", ts};
            if (last.json["status"].toString() == "SUCCESS")
                return {true, last.json, "", ts};
        }
    }
    return {false, std::nullopt, checked_error(last, "GTT cancel failed"), ts};
}

} // namespace fincept::trading
