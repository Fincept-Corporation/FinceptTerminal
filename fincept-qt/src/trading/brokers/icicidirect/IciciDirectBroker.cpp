#include "trading/brokers/icicidirect/IciciDirectBroker.h"

#include "core/logging/Logger.h"
#include "trading/brokers/BrokerHttp.h"
#include "trading/brokers/BrokerTokenUtil.h"
#include "trading/instruments/InstrumentService.h"

#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

#include <cmath>

namespace fincept::trading {

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

namespace {

// Breeze returns most numbers as JSON strings; tolerate both representations.
double jnum(const QJsonObject& o, const QString& key) {
    const QJsonValue v = o.value(key);
    if (v.isDouble())
        return v.toDouble();
    return v.toString().toDouble();
}

QString jstr(const QJsonObject& o, const QString& key) {
    const QJsonValue v = o.value(key);
    if (v.isString())
        return v.toString();
    if (v.isDouble())
        return QString::number(v.toDouble());
    return {};
}

// Breeze checksum timestamp: UTC ISO-8601 with a fixed ".000Z" millisecond part.
QString breeze_timestamp() {
    return QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddThh:mm:ss") + ".000Z";
}

// Map the UnifiedOrder enums onto Breeze wire values.
QString icici_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:
            return "market";
        case OrderType::Limit:
            return "limit";
        case OrderType::StopLoss:
        case OrderType::StopLossLimit:
            return "stoploss";
    }
    return "market";
}

// Breeze cash-segment product. Note: ICICI prohibits "marginplus"/"optionplus"
// via API; intraday equity uses plain "margin".
QString icici_equity_product(ProductType p) {
    switch (p) {
        case ProductType::Delivery:
            return "cash";
        case ProductType::Intraday:
        case ProductType::Margin:
            return "margin";
        case ProductType::MTF:
            return "mtf";
        default:
            return "cash";
    }
}

// "30-MAR-2026" -> "2026-03-30T06:00:00.000Z" (Breeze expiry/date format).
QString icici_iso_expiry(const QString& display) {
    if (display.isEmpty())
        return {};
    const QStringList p = display.split('-');
    if (p.size() != 3)
        return {};
    static const QMap<QString, QString> mon = {{"JAN", "01"}, {"FEB", "02"}, {"MAR", "03"}, {"APR", "04"},
                                               {"MAY", "05"}, {"JUN", "06"}, {"JUL", "07"}, {"AUG", "08"},
                                               {"SEP", "09"}, {"OCT", "10"}, {"NOV", "11"}, {"DEC", "12"}};
    const QString mm = mon.value(p[1].toUpper());
    if (mm.isEmpty())
        return {};
    const QString dd = p[0].rightJustified(2, '0');
    return QStringLiteral("%1-%2-%3T06:00:00.000Z").arg(p[2], mm, dd);
}

// "yyyy-MM-dd" (caller format) -> Breeze ISO datetime. `end_of_day` selects the
// session-close time; otherwise session-open. Passes through values that already
// look like ISO datetimes.
QString icici_iso_datetime(const QString& date, bool end_of_day) {
    const QDate d = QDate::fromString(date, "yyyy-MM-dd");
    if (!d.isValid())
        return date;
    return d.toString("yyyy-MM-dd") + (end_of_day ? "T15:30:00.000Z" : "T09:15:00.000Z");
}

// Map a generic resolution onto a Breeze historicalcharts interval.
QString icici_interval(const QString& resolution) {
    static const QMap<QString, QString> map = {
        {"1", "1minute"},   {"1m", "1minute"},   {"5", "5minute"},   {"5m", "5minute"},
        {"30", "30minute"}, {"30m", "30minute"}, {"D", "1day"},      {"1D", "1day"},
        {"day", "1day"},    {"1day", "1day"},
    };
    return map.value(resolution, QString());
}

// Split "NSE:RELIANCE" -> ("NSE","RELIANCE"); bare "RELIANCE" -> ("NSE","RELIANCE").
QPair<QString, QString> split_symbol(const QString& s) {
    if (s.contains(':'))
        return {s.section(':', 0, 0).toUpper(), s.section(':', 1)};
    return {QStringLiteral("NSE"), s};
}

} // namespace

QString IciciDirectBroker::icici_login_url(const QString& api_key) {
    return QStringLiteral("https://api.icicidirect.com/apiuser/login?api_key=%1")
        .arg(QString::fromUtf8(QUrl::toPercentEncoding(api_key)));
}

QMap<QString, QString> IciciDirectBroker::breeze_headers(const BrokerCredentials& creds, const QByteArray& body) const {
    const QString ts = breeze_timestamp();
    const QByteArray to_sign = ts.toUtf8() + body + creds.api_secret.toUtf8();
    const QString checksum = QString::fromLatin1(QCryptographicHash::hash(to_sign, QCryptographicHash::Sha256).toHex());
    return {
        {"Content-Type", "application/json"},
        {"X-Checksum", "token " + checksum},
        {"X-Timestamp", ts},
        {"X-AppKey", creds.api_key},
        {"X-SessionToken", creds.access_token},
    };
}

// Base (unsigned) headers — used for the one-time /customerdetails call, which
// authenticates via the body rather than a checksum.
QMap<QString, QString> IciciDirectBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Content-Type", "application/json"},
        {"X-AppKey", creds.api_key},
        {"X-SessionToken", creds.access_token},
    };
}

BrokerHttpResponse IciciDirectBroker::breeze_request(const QString& method, const QString& endpoint,
                                                     const QJsonObject& payload, const BrokerCredentials& creds) {
    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    const QString url = QString(base_url()) + endpoint;
    return BrokerHttp::instance().send(method, url, body, "application/json", breeze_headers(creds, body));
}

bool IciciDirectBroker::is_success(const BrokerHttpResponse& resp) {
    if (!resp.success)
        return false;
    // Envelope: {"Success": <data>, "Status": 200, "Error": null}
    const QJsonValue err = resp.json.value("Error");
    if (err.isString() && !err.toString().isEmpty())
        return false;
    const int status = resp.json.value("Status").toInt(200);
    return status >= 200 && status < 300;
}

QString IciciDirectBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    const QString err = resp.json.value("Error").toString();
    if (!err.isEmpty())
        return err;
    if (!resp.error.isEmpty())
        return resp.error;
    return fallback;
}

TokenExchangeResponse IciciDirectBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                        const QString& auth_code) {
    Q_UNUSED(api_secret)
    TokenExchangeResponse result;

    // Trade the raw API Session for the base64 session token via /customerdetails.
    // This call carries the session + app key in the body and is NOT checksum-signed.
    QJsonObject body{{"SessionToken", auth_code}, {"AppKey", api_key}};
    const QByteArray body_bytes = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QMap<QString, QString> headers{{"Content-Type", "application/json"}};
    auto resp = BrokerHttp::instance().send("GET", QString(base_url()) + "/customerdetails", body_bytes,
                                            "application/json", headers);

    LOG_INFO("IciciDirect", QString("customerdetails HTTP %1 success=%2 body=%3")
                                .arg(resp.status_code)
                                .arg(resp.success)
                                .arg(resp.raw_body.left(300)));

    if (!resp.success) {
        result.error = resp.json.value("Error").toString(
            resp.error.isEmpty() ? QString("HTTP %1").arg(resp.status_code) : resp.error);
        return result;
    }

    const QJsonObject success = resp.json.value("Success").toObject();
    const QString session_b64 = success.value("session_token").toString();
    if (session_b64.isEmpty()) {
        result.error = resp.json.value("Error").toString("ICICI session token missing in customerdetails response");
        return result;
    }

    // session_token = base64("user_id:session_key"); decode for the user id and
    // reuse the base64 value verbatim as the X-SessionToken header.
    const QString decoded = QString::fromUtf8(QByteArray::fromBase64(session_b64.toUtf8()));
    const QString user_id = decoded.section(':', 0, 0);

    result.success = true;
    result.access_token = session_b64;
    result.user_id = user_id.isEmpty() ? success.value("idirect_userid").toString() : user_id;
    // ICICI sessions expire at the daily reset; re-auth needs a fresh web
    // SessionToken, so detect-only. Startup hint.
    result.additional_data = with_token_expiry(result.additional_data, next_ist_flush_epoch(6, 0));
    LOG_INFO("IciciDirect", QString("exchange_token ok: user_id=%1").arg(result.user_id));
    return result;
}

OrderPlaceResponse IciciDirectBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    OrderPlaceResponse result;

    QString stock_code = order.symbol;
    QString exchange_code = order.exchange.isEmpty() ? QStringLiteral("NSE") : order.exchange.toUpper();
    QString product = icici_equity_product(order.product_type);
    QString expiry_iso;
    QString right = QStringLiteral("others");
    QString strike_str;

    // Resolve the ICICI stock_code (and F&O decomposition) from the master.
    auto inst = InstrumentService::instance().find(order.symbol, exchange_code, "icicidirect");
    if (inst.has_value()) {
        stock_code = inst->brsymbol;
        if (!inst->brexchange.isEmpty())
            exchange_code = inst->brexchange.toUpper();
        switch (inst->instrument_type) {
            case InstrumentType::FUT:
                product = "futures";
                expiry_iso = icici_iso_expiry(inst->expiry);
                break;
            case InstrumentType::CE:
            case InstrumentType::PE:
                product = "options";
                expiry_iso = icici_iso_expiry(inst->expiry);
                right = (inst->instrument_type == InstrumentType::CE) ? "call" : "put";
                strike_str = QString::number(inst->strike, 'f', 2);
                break;
            default:
                break;
        }
    }

    QJsonObject payload{
        {"stock_code", stock_code},
        {"exchange_code", exchange_code},
        {"product", product},
        {"action", (order.side == OrderSide::Buy) ? "buy" : "sell"},
        {"order_type", icici_order_type(order.order_type)},
        {"quantity", QString::number(static_cast<qlonglong>(order.quantity))},
        {"validity", order.validity.isEmpty() ? QStringLiteral("day") : order.validity.toLower()},
        {"disclosed_quantity", "0"},
        {"price", order.price > 0 ? QString::number(order.price, 'f', 2) : QString()},
        {"stoploss", order.stop_price > 0 ? QString::number(order.stop_price, 'f', 2) : QString()},
        {"expiry_date", expiry_iso},
        {"right", right},
        {"strike_price", strike_str},
        {"user_remark", "fincept"},
    };

    auto resp = breeze_request("POST", "/order", payload, creds);
    if (!is_success(resp)) {
        result.error = checked_error(resp, "Order placement failed");
        return result;
    }
    result.success = true;
    result.order_id = resp.json.value("Success").toObject().value("order_id").toString();
    return result;
}

ApiResponse<QJsonObject> IciciDirectBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                         const QJsonObject& mods) {
    QJsonObject payload{
        {"order_id", order_id},
        {"exchange_code", mods.value("exchange_code").toString("NSE")},
    };
    for (const QString& k : {QStringLiteral("order_type"), QStringLiteral("quantity"), QStringLiteral("price"),
                             QStringLiteral("stoploss"), QStringLiteral("validity"),
                             QStringLiteral("disclosed_quantity"), QStringLiteral("validity_date")}) {
        if (mods.contains(k))
            payload[k] = mods.value(k);
    }
    auto resp = breeze_request("PUT", "/order", payload, creds);
    int64_t ts = now_ts();
    if (!is_success(resp))
        return {false, std::nullopt, checked_error(resp, "Modify failed"), ts};
    return {true, resp.json, "", ts};
}

ApiResponse<QJsonObject> IciciDirectBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    // Breeze cancel needs the exchange. Without it on this signature, default to
    // NSE but honour an override packed in creds.additional_data {"exchange":...}.
    QString exchange = "NSE";
    if (!creds.additional_data.isEmpty()) {
        const auto ad = QJsonDocument::fromJson(creds.additional_data.toUtf8()).object();
        const QString e = ad.value("exchange").toString();
        if (!e.isEmpty())
            exchange = e.toUpper();
    }
    QJsonObject payload{{"order_id", order_id}, {"exchange_code", exchange}};
    auto resp = breeze_request("DELETE", "/order", payload, creds);
    int64_t ts = now_ts();
    if (!is_success(resp))
        return {false, std::nullopt, checked_error(resp, "Cancel failed"), ts};
    return {true, resp.json, "", ts};
}

ApiResponse<QVector<BrokerOrderInfo>> IciciDirectBroker::get_orders(const BrokerCredentials& creds) {
    // Breeze scopes the order book by exchange + date range. Query the common
    // equity/derivative exchanges for today and merge.
    const QString from = icici_iso_datetime(QDate::currentDate().toString("yyyy-MM-dd"), false);
    const QString to = icici_iso_datetime(QDate::currentDate().toString("yyyy-MM-dd"), true);

    QVector<BrokerOrderInfo> orders;
    QString last_error;
    bool any_ok = false;
    for (const QString& exch : {QStringLiteral("NSE"), QStringLiteral("NFO"), QStringLiteral("BSE")}) {
        QJsonObject payload{{"exchange_code", exch}, {"from_date", from}, {"to_date", to}};
        auto resp = breeze_request("GET", "/order", payload, creds);
        if (!is_success(resp)) {
            last_error = checked_error(resp, "Failed to fetch orders");
            continue;
        }
        any_ok = true;
        for (const auto& v : resp.json.value("Success").toArray()) {
            const auto o = v.toObject();
            BrokerOrderInfo info;
            info.order_id = jstr(o, "order_id");
            info.exchange = jstr(o, "exchange_code");
            info.symbol = jstr(o, "stock_code");
            info.side = jstr(o, "action");
            info.order_type = jstr(o, "order_type");
            info.product_type = jstr(o, "product_type");
            info.quantity = jnum(o, "quantity");
            info.price = jnum(o, "price");
            info.trigger_price = jnum(o, "stoploss");
            info.filled_qty = jnum(o, "quantity") - jnum(o, "pending_quantity");
            info.avg_price = jnum(o, "average_price");
            info.status = jstr(o, "status");
            info.timestamp = jstr(o, "order_datetime");
            orders.append(info);
        }
    }
    int64_t ts = now_ts();
    if (!any_ok)
        return {false, std::nullopt, last_error.isEmpty() ? "Failed to fetch orders" : last_error, ts};
    return {true, orders, "", ts};
}

ApiResponse<QJsonObject> IciciDirectBroker::get_trade_book(const BrokerCredentials& creds) {
    const QString from = icici_iso_datetime(QDate::currentDate().toString("yyyy-MM-dd"), false);
    const QString to = icici_iso_datetime(QDate::currentDate().toString("yyyy-MM-dd"), true);
    QJsonObject payload{{"exchange_code", "NSE"}, {"from_date", from}, {"to_date", to}};
    auto resp = breeze_request("GET", "/trades", payload, creds);
    int64_t ts = now_ts();
    if (!is_success(resp))
        return {false, std::nullopt, checked_error(resp, "Failed to fetch trades"), ts};
    return {true, resp.json, "", ts};
}

ApiResponse<QVector<BrokerPosition>> IciciDirectBroker::get_positions(const BrokerCredentials& creds) {
    auto resp = breeze_request("GET", "/portfoliopositions", QJsonObject{}, creds);
    int64_t ts = now_ts();
    if (!is_success(resp))
        return {false, std::nullopt, checked_error(resp, "Failed to fetch positions"), ts};

    QVector<BrokerPosition> out;
    for (const auto& v : resp.json.value("Success").toArray()) {
        const auto o = v.toObject();
        BrokerPosition p;
        const QString code = jstr(o, "stock_code");
        p.exchange = jstr(o, "exchange_code");
        auto norm = InstrumentService::instance().from_brsymbol(code, p.exchange, "icicidirect");
        p.symbol = norm.has_value() ? norm.value() : code;
        p.product_type = jstr(o, "product_type");
        p.quantity = jnum(o, "quantity");
        p.avg_price = jnum(o, "average_price");
        p.ltp = jnum(o, "ltp");
        p.pnl = jnum(o, "pnl");
        p.side = jstr(o, "action");
        const double cost = std::abs(p.avg_price * p.quantity);
        p.pnl_pct = cost > 0 ? (p.pnl / cost) * 100.0 : 0.0;
        out.append(p);
    }
    return {true, out, "", ts};
}

ApiResponse<QVector<BrokerHolding>> IciciDirectBroker::get_holdings(const BrokerCredentials& creds) {
    QJsonObject payload{{"exchange_code", "NSE"}, {"from_date", ""}, {"to_date", ""},
                        {"stock_code", ""},       {"portfolio_type", ""}};
    auto resp = breeze_request("GET", "/portfolioholdings", payload, creds);
    int64_t ts = now_ts();
    if (!is_success(resp))
        return {false, std::nullopt, checked_error(resp, "Failed to fetch holdings"), ts};

    QVector<BrokerHolding> out;
    for (const auto& v : resp.json.value("Success").toArray()) {
        const auto o = v.toObject();
        BrokerHolding h;
        const QString code = jstr(o, "stock_code");
        h.exchange = jstr(o, "exchange_code");
        auto norm = InstrumentService::instance().from_brsymbol(code, h.exchange, "icicidirect");
        h.symbol = norm.has_value() ? norm.value() : code;
        h.quantity = jnum(o, "quantity");
        h.avg_price = jnum(o, "average_price");
        h.ltp = jnum(o, "current_market_price");
        h.invested_value = h.quantity * h.avg_price;
        h.current_value = h.quantity * h.ltp;
        h.pnl = h.current_value - h.invested_value;
        h.pnl_pct = h.avg_price > 0 ? ((h.ltp - h.avg_price) / h.avg_price) * 100.0 : 0.0;
        out.append(h);
    }
    return {true, out, "", ts};
}

ApiResponse<BrokerFunds> IciciDirectBroker::get_funds(const BrokerCredentials& creds) {
    auto resp = breeze_request("GET", "/funds", QJsonObject{}, creds);
    int64_t ts = now_ts();
    if (!is_success(resp))
        return {false, std::nullopt, checked_error(resp, "Failed to fetch funds"), ts};

    const QJsonObject s = resp.json.value("Success").toObject();
    BrokerFunds funds;
    funds.total_balance = jnum(s, "total_bank_balance");
    funds.available_balance = jnum(s, "unallocated_balance");
    funds.used_margin = jnum(s, "block_by_trade_balance");
    funds.raw_data = resp.json;
    return {true, funds, "", ts};
}

ApiResponse<QVector<BrokerQuote>> IciciDirectBroker::get_quotes(const BrokerCredentials& creds,
                                                                const QVector<QString>& symbols) {
    // Breeze quotes one instrument per call; loop and collect.
    QVector<BrokerQuote> quotes;
    QString last_error;
    bool any_ok = false;

    for (const QString& raw : symbols) {
        auto [exch, sym] = split_symbol(raw);
        QString stock_code = sym;
        QString product = "cash";
        QString expiry_iso;
        QString right;
        QString strike_str;

        auto inst = InstrumentService::instance().find(sym, exch, "icicidirect");
        if (inst.has_value()) {
            stock_code = inst->brsymbol;
            if (!inst->brexchange.isEmpty())
                exch = inst->brexchange.toUpper();
            if (inst->instrument_type == InstrumentType::FUT) {
                product = "futures";
                expiry_iso = icici_iso_expiry(inst->expiry);
            } else if (inst->instrument_type == InstrumentType::CE || inst->instrument_type == InstrumentType::PE) {
                product = "options";
                expiry_iso = icici_iso_expiry(inst->expiry);
                right = (inst->instrument_type == InstrumentType::CE) ? "call" : "put";
                strike_str = QString::number(inst->strike, 'f', 2);
            }
        }

        QJsonObject payload{
            {"stock_code", stock_code}, {"exchange_code", exch},     {"product_type", product},
            {"expiry_date", expiry_iso}, {"right", right},           {"strike_price", strike_str},
        };
        auto resp = breeze_request("GET", "/quotes", payload, creds);
        if (!is_success(resp)) {
            last_error = checked_error(resp, "Failed to fetch quote");
            continue;
        }
        any_ok = true;
        const QJsonArray arr = resp.json.value("Success").toArray();
        if (arr.isEmpty())
            continue;
        const QJsonObject o = arr.first().toObject();
        BrokerQuote q;
        q.symbol = raw;
        q.ltp = jnum(o, "ltp");
        q.open = jnum(o, "open");
        q.high = jnum(o, "high");
        q.low = jnum(o, "low");
        q.close = jnum(o, "previous_close");
        q.volume = jnum(o, "total_quantity_traded");
        q.bid = jnum(o, "best_bid_price");
        q.bid_size = jnum(o, "best_bid_quantity");
        q.ask = jnum(o, "best_offer_price");
        q.ask_size = jnum(o, "best_offer_quantity");
        q.change_pct = jnum(o, "ltp_percent_change");
        q.change = q.close > 0 ? q.ltp - q.close : 0.0;
        q.timestamp = now_ts();
        quotes.append(q);
    }

    int64_t ts = now_ts();
    if (!any_ok && !symbols.isEmpty())
        return {false, std::nullopt, last_error.isEmpty() ? "Failed to fetch quotes" : last_error, ts};
    return {true, quotes, "", ts};
}

ApiResponse<QVector<BrokerCandle>> IciciDirectBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                                  const QString& resolution, const QString& from_date,
                                                                  const QString& to_date) {
    const QString interval = icici_interval(resolution);
    if (interval.isEmpty())
        return {false, std::nullopt, "ICICI get_history: unsupported resolution '" + resolution + "'", now_ts()};

    auto [exch, sym] = split_symbol(symbol);
    QString stock_code = sym;
    QString product = "cash";
    QString expiry_iso;
    QString right;
    QString strike_str;

    auto inst = InstrumentService::instance().find(sym, exch, "icicidirect");
    if (inst.has_value()) {
        stock_code = inst->brsymbol;
        if (!inst->brexchange.isEmpty())
            exch = inst->brexchange.toUpper();
        if (inst->instrument_type == InstrumentType::FUT) {
            product = "futures";
            expiry_iso = icici_iso_expiry(inst->expiry);
        } else if (inst->instrument_type == InstrumentType::CE || inst->instrument_type == InstrumentType::PE) {
            product = "options";
            expiry_iso = icici_iso_expiry(inst->expiry);
            right = (inst->instrument_type == InstrumentType::CE) ? "call" : "put";
            strike_str = QString::number(inst->strike, 'f', 2);
        }
    }

    QJsonObject payload{
        {"interval", interval},
        {"from_date", icici_iso_datetime(from_date, false)},
        {"to_date", icici_iso_datetime(to_date, true)},
        {"stock_code", stock_code},
        {"exchange_code", exch},
        {"product_type", product},
        {"expiry_date", expiry_iso},
        {"right", right},
        {"strike_price", strike_str},
    };
    auto resp = breeze_request("GET", "/historicalcharts", payload, creds);
    int64_t ts = now_ts();
    if (!is_success(resp))
        return {false, std::nullopt, checked_error(resp, "Failed to fetch history"), ts};

    QVector<BrokerCandle> candles;
    for (const auto& v : resp.json.value("Success").toArray()) {
        const auto o = v.toObject();
        BrokerCandle c;
        const QString dt = jstr(o, "datetime");
        QDateTime when = QDateTime::fromString(dt, Qt::ISODate);
        if (!when.isValid())
            when = QDateTime::fromString(dt, "yyyy-MM-dd hh:mm:ss");
        c.timestamp = when.isValid() ? when.toSecsSinceEpoch() : 0;
        c.open = jnum(o, "open");
        c.high = jnum(o, "high");
        c.low = jnum(o, "low");
        c.close = jnum(o, "close");
        c.volume = jnum(o, "volume");
        c.oi = jnum(o, "open_interest");
        candles.append(c);
    }
    return {true, candles, "", ts};
}

} // namespace fincept::trading
