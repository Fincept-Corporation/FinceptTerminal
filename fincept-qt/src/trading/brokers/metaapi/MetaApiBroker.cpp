#include "trading/brokers/metaapi/MetaApiBroker.h"
#include "trading/brokers/metaapi/MetaApiRateLimiter.h"

#include "trading/brokers/BrokerHttp.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>
#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace fincept::trading {

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// ── URL helpers ──────────────────────────────────────────────────────────────

QString MetaApiBroker::trading_url(const QString& region) {
    const QString r = region.isEmpty() ? QStringLiteral("new-york-server") : region;
    return QStringLiteral("https://mt-client-api-v1.%1.agiliumtrade.ai").arg(r);
}

QString MetaApiBroker::market_data_url(const QString& region) {
    const QString r = region.isEmpty() ? QStringLiteral("new-york-server") : region;
    return QStringLiteral("https://mt-market-data-client-api-v1.%1.agiliumtrade.ai").arg(r);
}

QString MetaApiBroker::region_from_creds(const BrokerCredentials& creds) {
    if (creds.additional_data.isEmpty())
        return QStringLiteral("new-york-server");
    const auto doc = QJsonDocument::fromJson(creds.additional_data.toUtf8());
    return doc.object().value("region").toString(QStringLiteral("new-york-server"));
}

// ── Auth headers ─────────────────────────────────────────────────────────────

QMap<QString, QString> MetaApiBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"auth-token", creds.api_key},
        {"Content-Type", "application/json"},
        {"Accept", "application/json"},
    };
}

// ── Error helpers ────────────────────────────────────────────────────────────

QString MetaApiBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    if (resp.status_code == 400)
        return QStringLiteral("MetaAPI: Invalid request. Check symbol and order parameters.");
    if (resp.status_code == 401)
        return QStringLiteral("MetaAPI token is invalid or expired. Get a new token from metaapi.cloud.");
    if (resp.status_code == 404)
        return QStringLiteral("MT4 account not found on MetaAPI. Please re-provision.");
    if (resp.status_code == 409)
        return QStringLiteral("MetaAPI: Account conflict. Try re-provisioning.");
    if (resp.status_code == 422)
        return QStringLiteral("MetaAPI: Validation error. Check order details.");
    if (resp.status_code == 429)
        return QStringLiteral("MetaAPI rate limit reached. Please wait and retry.");

    if (!resp.json.isEmpty()) {
        const QString msg = resp.json.value("message").toString();
        if (!msg.isEmpty())
            return msg;
        const QString err = resp.json.value("error").toString();
        if (!err.isEmpty())
            return err;
    }
    if (!resp.error.isEmpty())
        return resp.error;
    return fallback;
}

bool MetaApiBroker::is_deployment_error(const BrokerHttpResponse& resp) {
    if (resp.status_code == 409)
        return true;
    if (!resp.json.isEmpty()) {
        const QString err = resp.json.value("error").toString();
        if (err.contains("not deployed", Qt::CaseInsensitive) ||
            err.contains("not synchronized", Qt::CaseInsensitive))
            return true;
    }
    return false;
}

bool MetaApiBroker::try_redeploy(const BrokerCredentials& creds) {
    const QString url = provisioning_url() +
                        QStringLiteral("/users/current/accounts/%1/deploy").arg(creds.access_token);
    auto resp = BrokerHttp::instance().post_json(url, QJsonObject{}, auth_headers(creds));
    return resp.success || resp.status_code == 204;
}

// ── Order type mapping ───────────────────────────────────────────────────────

QString MetaApiBroker::action_type_for(OrderSide side, OrderType type) {
    const bool buy = (side == OrderSide::Buy);
    switch (type) {
        case OrderType::Market:
            return buy ? QStringLiteral("ORDER_TYPE_BUY") : QStringLiteral("ORDER_TYPE_SELL");
        case OrderType::Limit:
            return buy ? QStringLiteral("ORDER_TYPE_BUY_LIMIT") : QStringLiteral("ORDER_TYPE_SELL_LIMIT");
        case OrderType::StopLoss:
            return buy ? QStringLiteral("ORDER_TYPE_BUY_STOP") : QStringLiteral("ORDER_TYPE_SELL_STOP");
        case OrderType::StopLossLimit:
            return buy ? QStringLiteral("ORDER_TYPE_BUY_STOP_LIMIT")
                       : QStringLiteral("ORDER_TYPE_SELL_STOP_LIMIT");
    }
    return buy ? QStringLiteral("ORDER_TYPE_BUY") : QStringLiteral("ORDER_TYPE_SELL");
}

// ── Timeframe mapping ────────────────────────────────────────────────────────

QString MetaApiBroker::map_timeframe(const QString& resolution) {
    if (resolution == "1" || resolution == "1m")
        return QStringLiteral("1m");
    if (resolution == "5" || resolution == "5m")
        return QStringLiteral("5m");
    if (resolution == "15" || resolution == "15m")
        return QStringLiteral("15m");
    if (resolution == "30" || resolution == "30m")
        return QStringLiteral("30m");
    if (resolution == "60" || resolution == "1h")
        return QStringLiteral("1h");
    if (resolution == "2h" || resolution == "120")
        return QStringLiteral("4h");
    if (resolution == "3h" || resolution == "180")
        return QStringLiteral("4h");
    if (resolution == "4h" || resolution == "240")
        return QStringLiteral("4h");
    if (resolution == "8h" || resolution == "480")
        return QStringLiteral("4h");
    if (resolution == "D" || resolution == "1D" || resolution == "1d")
        return QStringLiteral("1d");
    if (resolution == "W" || resolution == "1W" || resolution == "1w")
        return QStringLiteral("1w");
    if (resolution == "M" || resolution == "1M" || resolution == "1mn")
        return QStringLiteral("1mn");
    return QStringLiteral("1h");
}

// ── Authentication (MetaAPI account provisioning) ────────────────────────────

TokenExchangeResponse MetaApiBroker::exchange_token(const QString& api_key, const QString& /*api_secret*/,
                                                     const QString& auth_code) {
    TokenExchangeResponse result;

    const auto payload = QJsonDocument::fromJson(auth_code.toUtf8()).object();
    const QString mt4_login = payload.value("login").toString();
    const QString mt4_password = payload.value("password").toString();
    const QString server_name = payload.value("server").toString();
    const QString region = payload.value("region").toString(QStringLiteral("new-york-server"));

    if (mt4_login.isEmpty() || mt4_password.isEmpty() || server_name.isEmpty()) {
        result.error = "MT4 login, password, and server name are required.";
        return result;
    }

    const QMap<QString, QString> headers = {
        {"auth-token", api_key},
        {"Content-Type", "application/json"},
        {"Accept", "application/json"},
    };

    // Provision MetaAPI account
    QJsonObject body;
    body["login"] = mt4_login;
    body["password"] = mt4_password;
    body["name"] = QStringLiteral("fincept-mt4-%1").arg(mt4_login);
    body["server"] = server_name;
    body["platform"] = QStringLiteral("mt4");
    body["type"] = QStringLiteral("cloud-g2");
    body["region"] = region;
    body["magic"] = 0;

    const QString prov_url = provisioning_url() + QStringLiteral("/users/current/accounts");
    auto resp = BrokerHttp::instance().post_json(prov_url, body, headers);

    QString account_id;

    if (resp.success) {
        account_id = resp.json.value("id").toString();
        if (account_id.isEmpty())
            account_id = resp.json.value("_id").toString();
    } else if (resp.status_code == 409) {
        // Account may already exist — try to find it
        auto list_resp = BrokerHttp::instance().get(prov_url, headers);
        if (list_resp.success) {
            const auto doc = QJsonDocument::fromJson(list_resp.raw_body.toUtf8());
            const auto arr = doc.array();
            for (const auto& item : arr) {
                const auto obj = item.toObject();
                if (obj.value("login").toString() == mt4_login &&
                    obj.value("server").toString() == server_name) {
                    account_id = obj.value("_id").toString();
                    break;
                }
            }
        }
        if (account_id.isEmpty()) {
            result.error = checked_error(resp, "Failed to provision MT4 account.");
            return result;
        }
    } else {
        result.error = checked_error(resp, "Failed to provision MT4 account.");
        return result;
    }

    // Poll until deployed (max 60 seconds)
    const QString status_url =
        provisioning_url() + QStringLiteral("/users/current/accounts/%1").arg(account_id);
    for (int i = 0; i < 20; ++i) {
        auto status_resp = BrokerHttp::instance().get(status_url, headers);
        if (status_resp.success) {
            const QString state = status_resp.json.value("state").toString();
            const QString conn = status_resp.json.value("connectionStatus").toString();
            if (state == "DEPLOYED" &&
                (conn == "CONNECTED" || conn == "AUTHENTICATED" || conn.isEmpty())) {
                break;
            }
        }
        if (i == 19) {
            result.error = "MT4 account deployment timed out. Please try again.";
            return result;
        }
        QThread::msleep(3000);
    }

    // Verify connection by fetching account info
    const QString base = trading_url(region);
    const QString info_url =
        base + QStringLiteral("/users/current/accounts/%1/account-information").arg(account_id);
    auto info_resp = BrokerHttp::instance().get(info_url, headers);

    if (!info_resp.success) {
        result.error = checked_error(info_resp, "MT4 account deployed but connection verification failed.");
        return result;
    }

    result.success = true;
    result.access_token = account_id;
    result.user_id = mt4_login;
    result.additional_data = QString::fromUtf8(QJsonDocument(QJsonObject{
        {"region", region},
        {"server", server_name},
        {"platform", "mt4"},
    }).toJson(QJsonDocument::Compact));

    return result;
}

// ── Order placement ──────────────────────────────────────────────────────────

OrderPlaceResponse MetaApiBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    OrderPlaceResponse result;

    const QString account_id = creds.access_token;
    if (!MetaApiRateLimiter::instance().try_consume(account_id, metaapi_credits::kTrade)) {
        result.error = "MetaAPI rate limit (5000 credits/10s). Retry shortly.";
        return result;
    }

    const QString region = region_from_creds(creds);
    const QString url =
        trading_url(region) + QStringLiteral("/users/current/accounts/%1/trade").arg(account_id);

    QJsonObject body;
    body["actionType"] = action_type_for(order.side, order.order_type);
    body["symbol"] = order.symbol;
    body["volume"] = order.quantity;

    if (order.order_type == OrderType::Limit) {
        if (order.price > 0)
            body["openPrice"] = order.price;
    } else if (order.order_type == OrderType::StopLoss) {
        if (order.price > 0)
            body["openPrice"] = order.price;
    } else if (order.order_type == OrderType::StopLossLimit) {
        if (order.price > 0)
            body["openPrice"] = order.price;
        if (order.stop_price > 0)
            body["stopLimitPrice"] = order.stop_price;
    }

    if (order.stop_price > 0 && order.order_type != OrderType::StopLossLimit)
        body["stopLoss"] = order.stop_price;

    auto resp = BrokerHttp::instance().post_json(url, body, auth_headers(creds));

    if (is_deployment_error(resp) && try_redeploy(creds)) {
        QThread::msleep(2000);
        resp = BrokerHttp::instance().post_json(url, body, auth_headers(creds));
    }

    if (!resp.success) {
        result.error = checked_error(resp, "Failed to place order.");
        return result;
    }

    const QString str_code = resp.json.value("stringCode").toString();
    if (str_code == "TRADE_RETCODE_DONE" || str_code == "TRADE_RETCODE_PLACED" ||
        str_code == "TRADE_RETCODE_DONE_PARTIAL") {
        result.success = true;
        result.order_id = resp.json.value("orderId").toString();
        if (result.order_id.isEmpty())
            result.order_id = resp.json.value("positionId").toString();
    } else {
        result.error = resp.json.value("message").toString();
        if (result.error.isEmpty())
            result.error = QStringLiteral("MT4 rejected: %1").arg(str_code);
    }

    return result;
}

// ── Order modification ───────────────────────────────────────────────────────

ApiResponse<QJsonObject> MetaApiBroker::modify_order(const BrokerCredentials& creds,
                                                      const QString& order_id,
                                                      const QJsonObject& mods) {
    const QString account_id = creds.access_token;
    if (!MetaApiRateLimiter::instance().try_consume(account_id, metaapi_credits::kTrade))
        return {false, std::nullopt, "MetaAPI rate limit. Retry shortly."};

    const QString region = region_from_creds(creds);
    const QString url =
        trading_url(region) + QStringLiteral("/users/current/accounts/%1/trade").arg(account_id);

    QJsonObject body;
    body["actionType"] = QStringLiteral("ORDER_MODIFY");
    body["orderId"] = order_id;
    if (mods.contains("price"))
        body["openPrice"] = mods.value("price").toDouble();
    if (mods.contains("stop_loss"))
        body["stopLoss"] = mods.value("stop_loss").toDouble();
    if (mods.contains("take_profit"))
        body["takeProfit"] = mods.value("take_profit").toDouble();

    auto resp = BrokerHttp::instance().post_json(url, body, auth_headers(creds));

    if (is_deployment_error(resp) && try_redeploy(creds)) {
        QThread::msleep(2000);
        resp = BrokerHttp::instance().post_json(url, body, auth_headers(creds));
    }

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Failed to modify order.")};

    const QString str_code = resp.json.value("stringCode").toString();
    if (str_code == "TRADE_RETCODE_DONE")
        return {true, resp.json, ""};
    return {false, std::nullopt, resp.json.value("message").toString()};
}

// ── Order cancellation ───────────────────────────────────────────────────────

ApiResponse<QJsonObject> MetaApiBroker::cancel_order(const BrokerCredentials& creds,
                                                      const QString& order_id) {
    const QString account_id = creds.access_token;
    if (!MetaApiRateLimiter::instance().try_consume(account_id, metaapi_credits::kTrade))
        return {false, std::nullopt, "MetaAPI rate limit. Retry shortly."};

    const QString region = region_from_creds(creds);
    const QString url =
        trading_url(region) + QStringLiteral("/users/current/accounts/%1/trade").arg(account_id);

    QJsonObject body;
    body["actionType"] = QStringLiteral("ORDER_CANCEL");
    body["orderId"] = order_id;

    auto resp = BrokerHttp::instance().post_json(url, body, auth_headers(creds));

    if (is_deployment_error(resp) && try_redeploy(creds)) {
        QThread::msleep(2000);
        resp = BrokerHttp::instance().post_json(url, body, auth_headers(creds));
    }

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Failed to cancel order.")};

    const QString str_code = resp.json.value("stringCode").toString();
    if (str_code == "TRADE_RETCODE_DONE")
        return {true, resp.json, ""};

    return {false, std::nullopt, resp.json.value("message").toString()};
}

// ── Get pending orders ───────────────────────────────────────────────────────

ApiResponse<QVector<BrokerOrderInfo>> MetaApiBroker::get_orders(const BrokerCredentials& creds) {
    const QString account_id = creds.access_token;
    if (!MetaApiRateLimiter::instance().try_consume(account_id, metaapi_credits::kRead))
        return {false, std::nullopt, "MetaAPI rate limit. Retry shortly."};

    const QString region = region_from_creds(creds);
    const QString url =
        trading_url(region) + QStringLiteral("/users/current/accounts/%1/orders").arg(account_id);

    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));

    if (is_deployment_error(resp) && try_redeploy(creds)) {
        QThread::msleep(2000);
        resp = BrokerHttp::instance().get(url, auth_headers(creds));
    }

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Failed to fetch orders.")};

    const auto doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    const auto arr = doc.array();

    QVector<BrokerOrderInfo> orders;
    orders.reserve(arr.size());

    for (const auto& item : arr) {
        const auto o = item.toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("id").toString();
        info.symbol = o.value("symbol").toString();
        info.exchange = QStringLiteral("FOREX");

        const QString type_str = o.value("type").toString();
        info.side = type_str.contains("BUY") ? "buy" : "sell";

        if (type_str.contains("LIMIT"))
            info.order_type = "limit";
        else if (type_str.contains("STOP"))
            info.order_type = "stop";
        else
            info.order_type = "market";

        info.exchange_order_id = o.value("brokerComment").toString();
        info.product_type = QStringLiteral("MARGIN");
        info.quantity = o.value("currentVolume").toDouble(o.value("volume").toDouble());
        info.price = o.value("openPrice").toDouble();
        info.trigger_price = o.value("stopPrice").toDouble();
        info.stop_price = o.value("stopLoss").toDouble();
        info.filled_qty = 0;
        info.avg_price = 0;

        const QString state = o.value("state").toString().toLower();
        if (state == "closed" || state == "filled")
            info.status = "filled";
        else if (state == "canceled" || state == "cancelled" || state == "rejected" || state == "expired")
            info.status = "cancelled";
        else
            info.status = "open";

        info.timestamp = o.value("time").toString();

        orders.append(info);
    }

    return {true, orders, ""};
}

// ── Trade book (deal history) ────────────────────────────────────────────────

ApiResponse<QJsonObject> MetaApiBroker::get_trade_book(const BrokerCredentials& creds) {
    const QString account_id = creds.access_token;
    if (!MetaApiRateLimiter::instance().try_consume(account_id, metaapi_credits::kHistoryRange))
        return {false, std::nullopt, "MetaAPI rate limit. Retry shortly."};

    const QString region = region_from_creds(creds);
    const auto now = QDateTime::currentDateTimeUtc();
    const auto start = now.addDays(-30);

    const QString url = trading_url(region) +
                        QStringLiteral("/users/current/accounts/%1/history-deals/time/%2/%3")
                            .arg(account_id, start.toString(Qt::ISODate), now.toString(Qt::ISODate));

    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Failed to fetch trade history.")};
    return {true, resp.json, ""};
}

// ── Positions ────────────────────────────────────────────────────────────────

ApiResponse<QVector<BrokerPosition>> MetaApiBroker::get_positions(const BrokerCredentials& creds) {
    const QString account_id = creds.access_token;
    if (!MetaApiRateLimiter::instance().try_consume(account_id, metaapi_credits::kRead))
        return {false, std::nullopt, "MetaAPI rate limit. Retry shortly."};

    const QString region = region_from_creds(creds);
    const QString url =
        trading_url(region) + QStringLiteral("/users/current/accounts/%1/positions").arg(account_id);

    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));

    if (is_deployment_error(resp) && try_redeploy(creds)) {
        QThread::msleep(2000);
        resp = BrokerHttp::instance().get(url, auth_headers(creds));
    }

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Failed to fetch positions.")};

    const auto doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    const auto arr = doc.array();

    QVector<BrokerPosition> positions;
    positions.reserve(arr.size());

    for (const auto& item : arr) {
        const auto p = item.toObject();
        BrokerPosition pos;
        pos.symbol = p.value("symbol").toString();
        pos.exchange = QStringLiteral("FOREX");

        const QString type_str = p.value("type").toString();
        pos.side = type_str.contains("BUY") ? "long" : "short";

        pos.quantity = p.value("volume").toDouble();
        pos.avg_price = p.value("openPrice").toDouble();
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

// ── Holdings (same as positions for MT4) ─────────────────────────────────────

ApiResponse<QVector<BrokerHolding>> MetaApiBroker::get_holdings(const BrokerCredentials& creds) {
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
        // TODO: invested/current ignore contract (lot) size; pnl from API 'profit' is authoritative.
        // quantity is in lots (e.g. 0.10), not units, so price*lots is off by the
        // contract size. The MetaAPI position payload carries no contract/lot-size
        // field to scale by, so the arithmetic is left as-is.
        h.invested_value = p.avg_price * p.quantity;
        h.current_value = p.ltp * p.quantity;
        holdings.append(h);
    }

    return {true, holdings, ""};
}

// ── Funds / Account info ─────────────────────────────────────────────────────

ApiResponse<BrokerFunds> MetaApiBroker::get_funds(const BrokerCredentials& creds) {
    const QString account_id = creds.access_token;
    if (!MetaApiRateLimiter::instance().try_consume(account_id, metaapi_credits::kRead))
        return {false, std::nullopt, "MetaAPI rate limit. Retry shortly."};

    const QString region = region_from_creds(creds);
    const QString url =
        trading_url(region) +
        QStringLiteral("/users/current/accounts/%1/account-information").arg(account_id);

    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));

    if (is_deployment_error(resp) && try_redeploy(creds)) {
        QThread::msleep(2000);
        resp = BrokerHttp::instance().get(url, auth_headers(creds));
    }

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "Failed to fetch account info.")};

    BrokerFunds funds;
    funds.total_balance = resp.json.value("balance").toDouble();
    funds.available_balance = resp.json.value("freeMargin").toDouble();
    funds.used_margin = resp.json.value("margin").toDouble();
    funds.collateral = 0;

    funds.raw_data = resp.json;
    funds.raw_data["equity"] = resp.json.value("equity").toDouble();
    funds.raw_data["leverage"] = resp.json.value("leverage").toInt();
    funds.raw_data["marginLevel"] = resp.json.value("marginLevel").toDouble();
    funds.raw_data["currency"] = resp.json.value("currency").toString();

    return {true, funds, ""};
}

// ── Quotes ───────────────────────────────────────────────────────────────────

ApiResponse<QVector<BrokerQuote>> MetaApiBroker::get_quotes(const BrokerCredentials& creds,
                                                             const QVector<QString>& symbols) {
    const QString account_id = creds.access_token;
    const QString region = region_from_creds(creds);
    const QString base = trading_url(region);

    QVector<BrokerQuote> quotes;
    quotes.reserve(symbols.size());

    for (const auto& sym : symbols) {
        if (!MetaApiRateLimiter::instance().try_consume(account_id, metaapi_credits::kRead))
            continue;

        const QString url =
            base +
            QStringLiteral("/users/current/accounts/%1/symbols/%2/current-price")
                .arg(account_id, sym);

        auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
        if (!resp.success)
            continue;

        BrokerQuote q;
        q.symbol = sym;
        q.bid = resp.json.value("bid").toDouble();
        q.ask = resp.json.value("ask").toDouble();
        q.ltp = (q.bid + q.ask) / 2.0;
        q.bid_size = 0;
        q.ask_size = 0;
        q.open = 0;
        q.high = 0;
        q.low = 0;
        q.close = q.ltp;
        q.volume = 0;
        q.change = 0;
        q.change_pct = 0;
        const auto quote_dt = QDateTime::fromString(resp.json.value("time").toString(), Qt::ISODate);
        q.timestamp = quote_dt.isValid() ? quote_dt.toSecsSinceEpoch() : now_ts();

        quotes.append(q);
    }

    return {true, quotes, ""};
}

// ── Historical candles ───────────────────────────────────────────────────────

ApiResponse<QVector<BrokerCandle>> MetaApiBroker::get_history(const BrokerCredentials& creds,
                                                               const QString& symbol,
                                                               const QString& resolution,
                                                               const QString& from_date,
                                                               const QString& to_date) {
    const QString account_id = creds.access_token;
    if (!MetaApiRateLimiter::instance().try_consume(account_id, metaapi_credits::kRead))
        return {false, std::nullopt, "MetaAPI rate limit. Retry shortly."};

    const QString region = region_from_creds(creds);
    const QString tf = map_timeframe(resolution);

    // Parse a flexible date string (ISO / yyyy-MM-dd / yyyy-MM-dd HH:mm) into a
    // UTC QDateTime. Returns an invalid QDateTime if none of the forms match.
    const auto parse_dt = [](const QString& s) -> QDateTime {
        if (s.isEmpty())
            return {};
        QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
        if (!dt.isValid()) {
            const QDate d = QDate::fromString(s, "yyyy-MM-dd");
            if (d.isValid()) dt = QDateTime(d, QTime(0, 0), QTimeZone::utc());
        }
        if (!dt.isValid()) {
            dt = QDateTime::fromString(s, "yyyy-MM-dd HH:mm");
            if (dt.isValid()) dt.setTimeZone(QTimeZone::utc());
        }
        return dt.isValid() ? dt.toUTC() : QDateTime();
    };

    // MetaAPI loads candles BACKWARD from startTime (the latest time of the
    // window). There is no endTime parameter, so map to_date -> startTime.
    QDateTime start_dt = parse_dt(to_date);
    if (!start_dt.isValid())
        start_dt = QDateTime::currentDateTimeUtc();

    // Lower bound for the requested window. When invalid we keep today's
    // single-request behaviour (no paging, no lower-bound trimming).
    const QDateTime from_dt = parse_dt(from_date);
    const bool from_valid = from_dt.isValid();

    constexpr int kLimit = 1000;

    // One backward fetch ending at `startTime` (the latest time of the page).
    // Appends parsed candles to `out`, sets `page_count` to the number of
    // candles returned, and records the millisecond-precision time of the
    // OLDEST candle in the page. Returns false only on HTTP failure.
    QVector<BrokerCandle> candles;
    const auto fetch_page = [&](const QDateTime& startTime,
                                int& page_count,
                                QDateTime& oldest_dt) -> bool {
        const QString start_str = startTime.toUTC().toString(Qt::ISODateWithMs);
        const QString url =
            market_data_url(region) +
            QStringLiteral(
                "/users/current/accounts/%1/historical-market-data/symbols/%2/timeframes/%3/candles"
                "?startTime=%4&limit=%5")
                .arg(account_id, symbol, tf, start_str)
                .arg(kLimit);

        auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
        if (!resp.success) {
            page_count = 0;
            oldest_dt = QDateTime();
            return false;
        }

        const auto doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
        const auto arr = doc.array();
        page_count = arr.size();
        oldest_dt = QDateTime();

        for (const auto& item : arr) {
            const auto c = item.toObject();
            BrokerCandle candle;

            const QString time_str = c.value("time").toString();
            const auto dt = QDateTime::fromString(time_str, Qt::ISODate);
            candle.timestamp = dt.isValid() ? dt.toMSecsSinceEpoch() : 0; // BrokerCandle contract = ms

            candle.open = c.value("open").toDouble();
            candle.high = c.value("high").toDouble();
            candle.low = c.value("low").toDouble();
            candle.close = c.value("close").toDouble();
            candle.volume = c.value("tickVolume").toDouble(c.value("volume").toDouble());

            // Track oldest candle (ms precision) to seed the next page's
            // startTime. The API may return the page in any order, so compare.
            if (dt.isValid() && (!oldest_dt.isValid() || dt < oldest_dt))
                oldest_dt = dt.toUTC();

            candles.append(candle);
        }
        return true;
    };

    // --- Page 1 (preserves single-request behaviour for short ranges) -------
    int page_count = 0;
    QDateTime oldest;
    {
        const QString start_str = start_dt.toUTC().toString(Qt::ISODateWithMs);
        const QString url =
            market_data_url(region) +
            QStringLiteral(
                "/users/current/accounts/%1/historical-market-data/symbols/%2/timeframes/%3/candles"
                "?startTime=%4&limit=%5")
                .arg(account_id, symbol, tf, start_str)
                .arg(kLimit);

        auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
        if (!resp.success)
            return {false, std::nullopt, checked_error(resp, "Failed to fetch candles.")};

        const auto doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
        const auto arr = doc.array();
        page_count = arr.size();

        for (const auto& item : arr) {
            const auto c = item.toObject();
            BrokerCandle candle;

            const QString time_str = c.value("time").toString();
            const auto dt = QDateTime::fromString(time_str, Qt::ISODate);
            candle.timestamp = dt.isValid() ? dt.toMSecsSinceEpoch() : 0; // BrokerCandle contract = ms

            candle.open = c.value("open").toDouble();
            candle.high = c.value("high").toDouble();
            candle.low = c.value("low").toDouble();
            candle.close = c.value("close").toDouble();
            candle.volume = c.value("tickVolume").toDouble(c.value("volume").toDouble());

            if (dt.isValid() && (!oldest.isValid() || dt < oldest))
                oldest = dt.toUTC();

            candles.append(candle);
        }
    }

    // --- Backward paging ----------------------------------------------------
    // Only page when from_date is valid (otherwise: single request as before),
    // the page was full (== limit), and the oldest candle is still newer than
    // the requested lower bound.
    if (from_valid) {
        constexpr int kMaxIterations = 30;
        int iterations = 0;
        while (page_count == kLimit && oldest.isValid() && oldest > from_dt &&
               iterations < kMaxIterations) {
            ++iterations;

            // Next page ends 1 ms before the oldest candle we already have.
            const QDateTime next_start = oldest.addMSecs(-1);
            const QDateTime prev_oldest = oldest;

            int next_count = 0;
            QDateTime page_oldest;
            if (!fetch_page(next_start, next_count, page_oldest))
                break;  // later-page failure: stop, return what we collected

            page_count = next_count;
            if (next_count == 0 || !page_oldest.isValid())
                break;  // empty / unparseable page: stop
            if (!(page_oldest < prev_oldest))
                break;  // no backward progress: avoid infinite loop
            oldest = page_oldest;
        }
    }

    // --- Merge: sort ascending by time, dedupe identical timestamps ---------
    std::sort(candles.begin(), candles.end(),
              [](const BrokerCandle& a, const BrokerCandle& b) {
                  return a.timestamp < b.timestamp;
              });
    candles.erase(std::unique(candles.begin(), candles.end(),
                              [](const BrokerCandle& a, const BrokerCandle& b) {
                                  return a.timestamp == b.timestamp;
                              }),
                  candles.end());

    // Drop candles older than the requested lower bound (only when valid).
    if (from_valid) {
        const int64_t from_ts = from_dt.toSecsSinceEpoch();
        candles.erase(std::remove_if(candles.begin(), candles.end(),
                                     [from_ts](const BrokerCandle& c) {
                                         return c.timestamp < from_ts;
                                     }),
                      candles.end());
    }

    return {true, candles, ""};
}

// ── Symbol list (cached) ─────────────────────────────────────────────────────

QStringList MetaApiBroker::fetch_symbols(const BrokerCredentials& creds) {
    const QString account_id = creds.access_token;

    {
        QMutexLocker lock(&symbol_mutex_);
        if (symbol_cache_.contains(account_id)) {
            const int64_t cached_at = symbol_cache_time_.value(account_id, 0);
            if (now_ts() - cached_at < 3600)
                return symbol_cache_.value(account_id);
        }
    }

    if (!MetaApiRateLimiter::instance().try_consume(account_id, metaapi_credits::kSymbolList))
        return {};

    const QString region = region_from_creds(creds);
    const QString url =
        trading_url(region) + QStringLiteral("/users/current/accounts/%1/symbols").arg(account_id);

    auto resp = BrokerHttp::instance().get(url, auth_headers(creds));
    if (!resp.success)
        return {};

    const auto doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    const auto arr = doc.array();

    QStringList symbols;
    symbols.reserve(arr.size());
    for (const auto& item : arr)
        symbols.append(item.toString());

    {
        QMutexLocker lock(&symbol_mutex_);
        symbol_cache_[account_id] = symbols;
        symbol_cache_time_[account_id] = now_ts();
    }

    return symbols;
}

} // namespace fincept::trading
