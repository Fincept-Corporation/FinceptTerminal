// AngelOneBroker — Angel One Smart API v2 implementation
// Auth flow: API Key + MPIN + base32 TOTP secret → auto-generate 6-digit TOTP → login
// Tokens stored in BrokerCredentials:
//   access_token    = JWT for REST calls
//   refresh_token   = JWT refresh token
//   user_id         = client code / user ID
//   additional_data = JSON: { "feed_token": "...", "client_code": "..." }

#include "trading/brokers/angelone/AngelOneBroker.h"

#include "core/logging/Logger.h"
#include "trading/brokers/BrokerHttp.h"
#include "trading/instruments/InstrumentService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QRegularExpression>

namespace fincept::trading {

static constexpr const char* TAG_AO = "AngelOneBroker";
static const QString BASE_AO = "https://apiconnect.angelone.in";

// SmartAPI requires real-looking client-context headers on every request. The
// previous stub (127.0.0.1 / 00:00:00:00:00:00) is associated with intermittent
// AB1010 / AB1050 throttle/auth errors. Resolve once at first call and cache.
struct ClientContext {
    QString local_ip;
    QString public_ip;
    QString mac;
};

static const ClientContext& client_context() {
    static const ClientContext ctx = []() {
        ClientContext c;
        for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
            const auto flags = iface.flags();
            if (!flags.testFlag(QNetworkInterface::IsUp) || flags.testFlag(QNetworkInterface::IsLoopBack))
                continue;
            if (c.mac.isEmpty()) {
                const QString hw = iface.hardwareAddress();
                if (!hw.isEmpty() && hw != "00:00:00:00:00:00")
                    c.mac = hw;
            }
            if (c.local_ip.isEmpty()) {
                for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
                    const auto addr = entry.ip();
                    if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
                        c.local_ip = addr.toString();
                        break;
                    }
                }
            }
            if (!c.mac.isEmpty() && !c.local_ip.isEmpty())
                break;
        }
        if (c.local_ip.isEmpty())
            c.local_ip = "192.168.1.1"; // last-resort placeholder; SmartAPI accepts non-loopback addrs
        if (c.mac.isEmpty())
            c.mac = "00:1A:2B:3C:4D:5E";
        // Public IP discovery would require an external probe (api.ipify.org);
        // sending the LAN IP here is what most retail SDKs do and SmartAPI accepts it.
        c.public_ip = c.local_ip;
        return c;
    }();
    return ctx;
}

// ─────────────────────────────────────────────────────────────────────────────
// TOTP Generation (RFC 6238, SHA-1, 30-second window, 6 digits)
// Base32 decode → HMAC-SHA1 with Unix epoch / 30 as 8-byte big-endian counter
// ─────────────────────────────────────────────────────────────────────────────

static QByteArray base32_decode(const QString& input) {
    const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    QString clean = input.toUpper().remove('=').remove(' ');
    QByteArray out;
    int buffer = 0, bits = 0;
    for (QChar c : clean) {
        int val = alphabet.indexOf(c);
        if (val < 0)
            continue;
        buffer = (buffer << 5) | val;
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            out.append(static_cast<char>((buffer >> bits) & 0xFF));
        }
    }
    return out;
}

static QByteArray hmac_sha1(const QByteArray& key, const QByteArray& msg) {
    // HMAC-SHA1: ipad/opad per RFC 2104
    const int block = 64;
    QByteArray k = key;
    if (k.size() > block)
        k = QCryptographicHash::hash(k, QCryptographicHash::Sha1);
    k = k.leftJustified(block, '\0');

    QByteArray ipad(block, '\x36'), opad(block, '\x5c');
    for (int i = 0; i < block; ++i) {
        ipad[i] ^= k[i];
        opad[i] ^= k[i];
    }
    QByteArray inner = QCryptographicHash::hash(ipad + msg, QCryptographicHash::Sha1);
    return QCryptographicHash::hash(opad + inner, QCryptographicHash::Sha1);
}

QString AngelOneBroker::generate_totp(const QString& base32_secret) {
    // If already a 6-digit numeric code, pass through (manual entry fallback)
    if (base32_secret.length() == 6 && base32_secret.toUInt() > 0)
        return base32_secret;

    QByteArray key = base32_decode(base32_secret);
    if (key.isEmpty()) {
        LOG_WARN(TAG_AO, "TOTP: invalid base32 secret");
        return {};
    }

    quint64 counter = static_cast<quint64>(QDateTime::currentSecsSinceEpoch()) / 30;
    QByteArray msg(8, '\0');
    for (int i = 7; i >= 0; --i) {
        msg[i] = static_cast<char>(counter & 0xFF);
        counter >>= 8;
    }

    QByteArray hash = hmac_sha1(key, msg);
    int offset = hash[hash.size() - 1] & 0x0F;
    quint32 code =
        ((static_cast<quint8>(hash[offset]) & 0x7F) << 24) | ((static_cast<quint8>(hash[offset + 1]) & 0xFF) << 16) |
        ((static_cast<quint8>(hash[offset + 2]) & 0xFF) << 8) | (static_cast<quint8>(hash[offset + 3]) & 0xFF);
    code %= 1000000;
    return QString("%1").arg(code, 6, 10, QChar('0'));
}

// ─────────────────────────────────────────────────────────────────────────────
// Symbol token lookup
// Delegates entirely to InstrumentService which downloads the master contract
// asynchronously in ws_init() via refresh("angelone", creds).
// ensure_token_cache() was removed — it caused deadlocks by doing a blocking
// 8MB HTTP download on QtConcurrent worker threads via BrokerHttp.
// ─────────────────────────────────────────────────────────────────────────────

QString AngelOneBroker::lookup_token(const QString& symbol, const QString& exchange) {
    // Use the shared InstrumentService cache (populated asynchronously by ws_init).
    // Never call ensure_token_cache() here — that does a blocking 8MB HTTP download
    // on whatever thread calls this (often a QtConcurrent worker), causing deadlock
    // or severe UI stalls on first use.
    auto svc_token = InstrumentService::instance().instrument_token(symbol, exchange, "angelone");
    if (svc_token.has_value() && svc_token.value() > 0)
        return QString::number(svc_token.value());

    // Try stripping -EQ/-BE suffix (AngelOne stores "RELIANCE-EQ" but callers pass "RELIANCE")
    const QString stripped = symbol.toUpper().remove(QRegularExpression("(-EQ|-BE)$"));
    if (stripped != symbol.toUpper()) {
        auto svc_token2 = InstrumentService::instance().instrument_token(stripped, exchange, "angelone");
        if (svc_token2.has_value() && svc_token2.value() > 0)
            return QString::number(svc_token2.value());
    }

    // InstrumentService cache not yet loaded — caller must wait for refresh_done signal
    LOG_WARN(TAG_AO, QString("Token not found for %1:%2 (instrument cache not loaded yet?)").arg(exchange, symbol));
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// Mapping helpers
// ─────────────────────────────────────────────────────────────────────────────

QString AngelOneBroker::ao_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:
            return "MARKET";
        case OrderType::Limit:
            return "LIMIT";
        case OrderType::StopLoss:
            return "STOPLOSS_MARKET";
        case OrderType::StopLossLimit:
            return "STOPLOSS_LIMIT";
    }
    return "MARKET";
}

QString AngelOneBroker::ao_variety(OrderType t, bool amo) {
    if (amo)
        return "AMO";
    if (t == OrderType::StopLoss || t == OrderType::StopLossLimit)
        return "STOPLOSS";
    return "NORMAL";
}

QString AngelOneBroker::ao_product(ProductType p) {
    switch (p) {
        case ProductType::Intraday:
            return "INTRADAY";
        case ProductType::Delivery:
            return "DELIVERY";
        case ProductType::Margin:
            return "CARRYFORWARD";
        case ProductType::CoverOrder:
            return "INTRADAY"; // not natively supported
        case ProductType::BracketOrder:
            return "INTRADAY";
    }
    return "INTRADAY";
}

QString AngelOneBroker::ao_transaction(OrderSide s) {
    return s == OrderSide::Buy ? "BUY" : "SELL";
}

// ─────────────────────────────────────────────────────────────────────────────
// Auth headers
// additional_data stores JSON: { "feed_token": "...", "client_code": "..." }
// ─────────────────────────────────────────────────────────────────────────────

QMap<QString, QString> AngelOneBroker::auth_headers(const BrokerCredentials& creds) const {
    const auto& ctx = client_context();
    return {
        {"Authorization", "Bearer " + creds.access_token},
        {"X-PrivateKey", creds.api_key},
        {"X-UserType", "USER"},
        {"X-SourceID", "WEB"},
        {"X-ClientLocalIP", ctx.local_ip},
        {"X-ClientPublicIP", ctx.public_ip},
        {"X-MACAddress", ctx.mac},
        {"Content-Type", "application/json"},
        {"Accept", "application/json"},
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// exchange_token — login with API key + MPIN + TOTP secret
// auth_code = base32 TOTP secret (auto-generates 6-digit code internally)
// ─────────────────────────────────────────────────────────────────────────────

TokenExchangeResponse AngelOneBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                     const QString& auth_code) {
    TokenExchangeResponse result;

    // auth_code is JSON: {"client_code":"R1234","totp_secret":"BASE32..."}
    // packed by EquityCredentials when ClientCode field is present
    QString client_code;
    QString totp_secret = auth_code;

    auto doc = QJsonDocument::fromJson(auth_code.toUtf8());
    if (doc.isObject()) {
        auto obj = doc.object();
        client_code = obj.value("client_code").toString();
        totp_secret = obj.value("totp_secret").toString();
    }
    if (client_code.isEmpty()) {
        result.error = "Client code is required. Please enter your Angel One login ID.";
        return result;
    }

    QString totp_code = generate_totp(totp_secret);
    if (totp_code.isEmpty()) {
        result.error = "Failed to generate TOTP. Check your TOTP secret.";
        return result;
    }
    LOG_INFO(TAG_AO, "Generated TOTP for login, client: " + client_code);

    // Login headers — X-PrivateKey = API key (from developer console)
    const auto& ctx = client_context();
    QMap<QString, QString> login_headers = {
        {"X-PrivateKey", api_key},
        {"X-UserType", "USER"},
        {"X-SourceID", "WEB"},
        {"X-ClientLocalIP", ctx.local_ip},
        {"X-ClientPublicIP", ctx.public_ip},
        {"X-MACAddress", ctx.mac},
        {"Content-Type", "application/json"},
        {"Accept", "application/json"},
    };

    QJsonObject payload{
        {"clientcode", client_code}, // login ID
        {"password", api_secret},    // MPIN
        {"totp", totp_code},         // auto-generated 6-digit code
    };

    auto resp = BrokerHttp::instance().post_json(BASE_AO + "/rest/auth/angelbroking/user/v1/loginByPassword", payload,
                                                 login_headers);

    if (!resp.success) {
        result.error = checked_error(resp, "Login failed");
        LOG_ERROR(TAG_AO, "Login failed: " + result.error);
        return result;
    }

    auto data = resp.json.value("data").toObject();
    if (!resp.json.value("status").toBool()) {
        result.error = checked_error(resp, "Login failed");
        LOG_ERROR(TAG_AO, "Login rejected: " + result.error);
        return result;
    }

    result.success = true;
    result.access_token = data.value("jwtToken").toString();
    result.refresh_token = data.value("refreshToken").toString();
    result.user_id = client_code; // use client_code as stable login ID

    // Pack feed_token, client_code, totp_secret into additional_data JSON
    QJsonObject extra{
        {"feed_token", data.value("feedToken").toString()},
        {"client_code", client_code},
        {"totp_secret", totp_secret}, // persist so UI can pre-fill & token refresh can re-login
    };
    result.additional_data = QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));

    LOG_INFO(TAG_AO, "Login success, user: " + result.user_id);
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Place Order
// ─────────────────────────────────────────────────────────────────────────────

OrderPlaceResponse AngelOneBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    QString token = lookup_token(order.symbol, order.exchange);

    QJsonObject payload{
        {"variety", ao_variety(order.order_type, order.amo)},
        {"tradingsymbol", order.symbol},
        {"symboltoken", token},
        {"transactiontype", ao_transaction(order.side)},
        {"exchange", order.exchange},
        {"ordertype", ao_order_type(order.order_type)},
        {"producttype", ao_product(order.product_type)},
        {"duration", order.validity.isEmpty() ? "DAY" : order.validity},
        {"price", QString::number(order.price, 'f', 2)},
        {"triggerprice", QString::number(order.stop_price, 'f', 2)},
        {"quantity", QString::number(static_cast<int>(order.quantity))},
        {"squareoff", "0"},
        {"stoploss", order.stop_loss > 0 ? QString::number(order.stop_loss, 'f', 2) : "0"},
        {"disclosedquantity", "0"},
    };

    auto resp = BrokerHttp::instance().post_json(BASE_AO + "/rest/secure/angelbroking/order/v1/placeOrder", payload,
                                                 auth_headers(creds));

    OrderPlaceResponse result;
    if (!resp.success || !resp.json.value("status").toBool()) {
        result.error = checked_error(resp, "Order placement failed");
        return result;
    }
    result.success = true;
    result.order_id = resp.json.value("data").toObject().value("orderid").toString();
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Modify Order
// ─────────────────────────────────────────────────────────────────────────────

ApiResponse<QJsonObject> AngelOneBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                      const QJsonObject& mods) {
    int64_t ts = QDateTime::currentSecsSinceEpoch();
    QJsonObject payload = mods;
    payload["orderid"] = order_id;
    if (!payload.contains("variety"))
        payload["variety"] = "NORMAL";

    auto resp = BrokerHttp::instance().post_json(BASE_AO + "/rest/secure/angelbroking/order/v1/modifyOrder", payload,
                                                 auth_headers(creds));

    if (!resp.success || !resp.json.value("status").toBool())
        return {false, std::nullopt, checked_error(resp, "Modify failed"), ts};
    return {true, resp.json, "", ts};
}

// ─────────────────────────────────────────────────────────────────────────────
// Cancel Order
// ─────────────────────────────────────────────────────────────────────────────

ApiResponse<QJsonObject> AngelOneBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    int64_t ts = QDateTime::currentSecsSinceEpoch();
    QJsonObject payload{{"variety", "NORMAL"}, {"orderid", order_id}};

    auto resp = BrokerHttp::instance().post_json(BASE_AO + "/rest/secure/angelbroking/order/v1/cancelOrder", payload,
                                                 auth_headers(creds));

    if (!resp.success || !resp.json.value("status").toBool())
        return {false, std::nullopt, checked_error(resp, "Cancel failed"), ts};
    return {true, resp.json, "", ts};
}

// ─────────────────────────────────────────────────────────────────────────────
// Get Orders
// ─────────────────────────────────────────────────────────────────────────────

static QString map_ao_status(const QString& s) {
    QString ls = s.toLower();
    if (ls == "complete")
        return "filled";
    if (ls == "open" || ls == "open pending")
        return "open";
    if (ls == "cancelled")
        return "cancelled";
    if (ls == "rejected")
        return "rejected";
    if (ls == "trigger pending")
        return "trigger_pending";
    if (ls == "validation pending" || ls == "put order req received")
        return "pending";
    return ls;
}

ApiResponse<QVector<BrokerOrderInfo>> AngelOneBroker::get_orders(const BrokerCredentials& creds) {
    int64_t ts = QDateTime::currentSecsSinceEpoch();
    auto resp =
        BrokerHttp::instance().get(BASE_AO + "/rest/secure/angelbroking/order/v1/getOrderBook", auth_headers(creds));

    if (!resp.success || !resp.json.value("status").toBool())
        return {false, std::nullopt, checked_error(resp, "get_orders failed"), ts};

    QVector<BrokerOrderInfo> orders;
    auto arr = resp.json.value("data").toArray();
    for (const auto& v : arr) {
        auto o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("orderid").toString();
        info.symbol = o.value("tradingsymbol").toString();
        info.exchange = o.value("exchange").toString();
        info.side = o.value("transactiontype").toString().toLower();
        info.order_type = o.value("ordertype").toString();
        info.product_type = o.value("producttype").toString();
        info.quantity = o.value("quantity").toString().toDouble();
        info.price = o.value("price").toString().toDouble();
        info.trigger_price = o.value("triggerprice").toString().toDouble();
        info.filled_qty = o.value("filledshares").toString().toDouble();
        info.avg_price = o.value("averageprice").toString().toDouble();
        info.status = map_ao_status(o.value("status").toString());
        info.timestamp = o.value("updatetime").toString(); // OpenAlgo: updatetime not orderentryTime
        info.message = o.value("text").toString();
        orders.append(info);
    }
    return {true, orders, "", ts};
}

// ─────────────────────────────────────────────────────────────────────────────
// Get Trade Book
// ─────────────────────────────────────────────────────────────────────────────

ApiResponse<QJsonObject> AngelOneBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = QDateTime::currentSecsSinceEpoch();
    auto resp =
        BrokerHttp::instance().get(BASE_AO + "/rest/secure/angelbroking/order/v1/getTradeBook", auth_headers(creds));

    if (!resp.success || !resp.json.value("status").toBool())
        return {false, std::nullopt, checked_error(resp, "get_trade_book failed"), ts};
    return {true, resp.json, "", ts};
}

// ─────────────────────────────────────────────────────────────────────────────
// Get Positions
// ─────────────────────────────────────────────────────────────────────────────

ApiResponse<QVector<BrokerPosition>> AngelOneBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts = QDateTime::currentSecsSinceEpoch();
    auto resp =
        BrokerHttp::instance().get(BASE_AO + "/rest/secure/angelbroking/order/v1/getPosition", auth_headers(creds));

    if (!resp.success || !resp.json.value("status").toBool())
        return {false, std::nullopt, checked_error(resp, "get_positions failed"), ts};

    QVector<BrokerPosition> positions;
    auto arr = resp.json.value("data").toArray();
    for (const auto& v : arr) {
        auto p = v.toObject();
        double qty = p.value("netqty").toString().toDouble();
        if (qty == 0)
            continue; // skip flat positions

        BrokerPosition pos;
        pos.symbol = p.value("tradingsymbol").toString();
        pos.exchange = p.value("exchange").toString();
        pos.product_type = p.value("producttype").toString();
        pos.quantity = qty;
        pos.avg_price = p.value("avgnetprice").toString().toDouble(); // OpenAlgo: avgnetprice
        pos.ltp = p.value("ltp").toString().toDouble();
        // unrealised = floating P&L on the open position; pnl/realised = booked.
        // day_pnl is the intraday mark-to-market, surfaced by SmartAPI as `unrealised`.
        const double unreal = p.value("unrealised").toString().toDouble();
        const double real = p.value("realised").toString().toDouble();
        pos.pnl = unreal + real;
        pos.day_pnl = unreal != 0.0 ? unreal : p.value("pnl").toString().toDouble();
        pos.side = qty >= 0 ? "buy" : "sell";
        if (pos.avg_price > 0)
            pos.pnl_pct = (pos.ltp - pos.avg_price) / pos.avg_price * 100.0 * (qty >= 0 ? 1 : -1);
        positions.append(pos);
    }
    return {true, positions, "", ts};
}

// ─────────────────────────────────────────────────────────────────────────────
// Get Holdings
// ─────────────────────────────────────────────────────────────────────────────

ApiResponse<QVector<BrokerHolding>> AngelOneBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = QDateTime::currentSecsSinceEpoch();
    auto resp = BrokerHttp::instance().get(BASE_AO + "/rest/secure/angelbroking/portfolio/v1/getAllHolding",
                                           auth_headers(creds));

    if (!resp.success || !resp.json.value("status").toBool())
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};

    QVector<BrokerHolding> holdings;
    auto arr = resp.json.value("data").toObject().value("holdings").toArray();
    for (const auto& v : arr) {
        auto h = v.toObject();
        BrokerHolding hold;
        hold.symbol = h.value("tradingsymbol").toString();
        hold.exchange = h.value("exchange").toString();
        hold.quantity = h.value("quantity").toString().toDouble();
        hold.avg_price = h.value("averageprice").toString().toDouble();
        hold.ltp = h.value("ltp").toString().toDouble();
        hold.invested_value = hold.quantity * hold.avg_price;
        hold.current_value = hold.quantity * hold.ltp;
        // Use API-provided pnl fields directly (more accurate than computed)
        hold.pnl = h.value("profitandloss").toString().toDouble();
        hold.pnl_pct = h.value("pnlpercentage").toString().toDouble();
        if (hold.pnl == 0 && hold.invested_value > 0) {
            hold.pnl = hold.current_value - hold.invested_value;
            hold.pnl_pct = hold.pnl / hold.invested_value * 100.0;
        }
        holdings.append(hold);
    }
    return {true, holdings, "", ts};
}

// ─────────────────────────────────────────────────────────────────────────────
// Get Funds (RMS)
// ─────────────────────────────────────────────────────────────────────────────

ApiResponse<BrokerFunds> AngelOneBroker::get_funds(const BrokerCredentials& creds) {
    int64_t ts = QDateTime::currentSecsSinceEpoch();
    auto resp = BrokerHttp::instance().get(BASE_AO + "/rest/secure/angelbroking/user/v1/getRMS", auth_headers(creds));

    if (!resp.success || !resp.json.value("status").toBool())
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    auto d = resp.json.value("data").toObject();
    BrokerFunds funds;
    // getRMS: `net` is the authoritative net cash balance; `availablecash` is
    // free cash before considering used margin; `utiliseddebits` and friends
    // are debit components, not additive to total. Compute used = sum of debits.
    funds.available_balance = d.value("availablecash").toString().toDouble();
    funds.used_margin = d.value("utiliseddebits").toString().toDouble() +
                        d.value("utilisedspan").toString().toDouble() +
                        d.value("utilisedoptionpremium").toString().toDouble() +
                        d.value("utilisedholdingsales").toString().toDouble() +
                        d.value("utilisedexposure").toString().toDouble();
    const double net = d.value("net").toString().toDouble();
    funds.total_balance = net > 0 ? net : (funds.available_balance + funds.used_margin);
    funds.collateral = d.value("collateral").toString().toDouble();
    funds.raw_data = d;
    return {true, funds, "", ts};
}

// ─────────────────────────────────────────────────────────────────────────────
// Get Quotes — batch LTP via /quote/ endpoint
// AngelOne quote API requires exchange + symboltoken, not just symbol name
// Modes: LTP=1, QUOTE=2, SNAP_QUOTE=3
// ─────────────────────────────────────────────────────────────────────────────

ApiResponse<QVector<BrokerQuote>> AngelOneBroker::get_quotes(const BrokerCredentials& creds,
                                                             const QVector<QString>& symbols) {
    int64_t ts = QDateTime::currentSecsSinceEpoch();

    // Group by exchange (default NSE)
    QMap<QString, QJsonArray> by_exchange;
    for (const auto& sym : symbols) {
        // sym may be "NSE:RELIANCE" or just "RELIANCE"
        QString exchange = "NSE", name = sym;
        if (sym.contains(':')) {
            exchange = sym.section(':', 0, 0);
            name = sym.section(':', 1);
        }
        QString token = lookup_token(name, exchange);
        if (token.isEmpty())
            continue;
        by_exchange[exchange].append(token);
    }

    if (by_exchange.isEmpty())
        return {false, std::nullopt, "No valid symbol tokens found", ts};

    // Build exchangeTokens as flat object: { "NSE": ["2885", "3045"], "BSE": [...] }
    // This matches the Angel One Smart API v2 spec
    QJsonObject exchange_tokens;
    for (auto it = by_exchange.constBegin(); it != by_exchange.constEnd(); ++it) {
        exchange_tokens[it.key()] = it.value();
    }

    QJsonObject payload{
        {"mode", "FULL"},
        {"exchangeTokens", exchange_tokens},
    };

    auto resp = BrokerHttp::instance().post_json(BASE_AO + "/rest/secure/angelbroking/market/v1/quote", payload,
                                                 auth_headers(creds));

    if (!resp.success || !resp.json.value("status").toBool())
        return {false, std::nullopt, checked_error(resp, "get_quotes failed"), ts};

    QVector<BrokerQuote> quotes;
    auto fetched = resp.json.value("data").toObject().value("fetched").toArray();
    for (const auto& v : fetched) {
        auto q = v.toObject();
        BrokerQuote quote;
        // Strip -EQ/-BE suffix so symbol matches watchlist entries (e.g. "RELIANCE-EQ" → "RELIANCE")
        {
            QString sym = q.value("tradingSymbol").toString();
            int dash = sym.lastIndexOf('-');
            if (dash > 0)
                sym = sym.left(dash);
            quote.symbol = sym;
        }
        // All numeric fields come as JSON numbers (not strings) in FULL quote mode
        quote.ltp = q.value("ltp").toDouble();
        quote.open = q.value("open").toDouble();
        quote.high = q.value("high").toDouble();
        quote.low = q.value("low").toDouble();
        quote.close = q.value("close").toDouble();
        quote.volume = q.value("tradeVolume").toDouble();
        // Use API-provided change fields directly
        quote.change = q.value("netChange").toDouble();
        quote.change_pct = q.value("percentChange").toDouble();
        // bid/ask from depth (may be 0 after market close)
        auto depth = q.value("depth").toObject();
        auto buy_arr = depth.value("buy").toArray();
        auto sell_arr = depth.value("sell").toArray();
        // Find first non-zero bid/ask
        for (const auto& b : buy_arr) {
            auto obj = b.toObject();
            double p = obj.value("price").toDouble();
            if (p > 0) {
                quote.bid = p;
                quote.bid_size = obj.value("quantity").toDouble();
                break;
            }
        }
        for (const auto& s : sell_arr) {
            auto obj = s.toObject();
            double p = obj.value("price").toDouble();
            if (p > 0) {
                quote.ask = p;
                quote.ask_size = obj.value("quantity").toDouble();
                break;
            }
        }
        quote.timestamp = ts;
        quotes.append(quote);
    }
    return {true, quotes, "", ts};
}

// ─────────────────────────────────────────────────────────────────────────────
// Get History (OHLCV candles)
// resolution: "1m","3m","5m","10m","15m","30m","1h","1d"
// ─────────────────────────────────────────────────────────────────────────────

static QString map_resolution(const QString& res) {
    if (res == "1m" || res == "1")
        return "ONE_MINUTE";
    if (res == "3m" || res == "3")
        return "THREE_MINUTE";
    if (res == "5m" || res == "5")
        return "FIVE_MINUTE";
    if (res == "10m" || res == "10")
        return "TEN_MINUTE";
    if (res == "15m" || res == "15")
        return "FIFTEEN_MINUTE";
    if (res == "30m" || res == "30")
        return "THIRTY_MINUTE";
    if (res == "1h" || res == "60")
        return "ONE_HOUR";
    if (res == "1d" || res == "D")
        return "ONE_DAY";
    if (res == "1w" || res == "W")
        return "ONE_DAY"; // AngelOne has no weekly; use daily
    return "ONE_DAY";
}

ApiResponse<QVector<BrokerCandle>> AngelOneBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                               const QString& resolution, const QString& from_date,
                                                               const QString& to_date) {
    int64_t ts = QDateTime::currentSecsSinceEpoch();

    // symbol may be "NSE:RELIANCE" or just "RELIANCE"
    QString exchange = "NSE", name = symbol;
    if (symbol.contains(':')) {
        exchange = symbol.section(':', 0, 0);
        name = symbol.section(':', 1);
    }

    QString token = lookup_token(name, exchange);
    if (token.isEmpty())
        return {false, std::nullopt, "Symbol token not found for: " + symbol, ts};

    QJsonObject payload{
        {"exchange", exchange},  {"symboltoken", token}, {"interval", map_resolution(resolution)},
        {"fromdate", from_date}, // "YYYY-MM-DD HH:MM"
        {"todate", to_date},
    };

    auto resp = BrokerHttp::instance().post_json(BASE_AO + "/rest/secure/angelbroking/historical/v1/getCandleData",
                                                 payload, auth_headers(creds));

    if (!resp.success || !resp.json.value("status").toBool())
        return {false, std::nullopt, checked_error(resp, "get_history failed"), ts};

    // data is array of [timestamp, open, high, low, close, volume]
    QVector<BrokerCandle> candles;
    auto arr = resp.json.value("data").toArray();
    for (const auto& v : arr) {
        auto row = v.toArray();
        if (row.size() < 6)
            continue;
        BrokerCandle c;
        // AngelOne returns ISO 8601: "2025-03-03T00:00:00+05:30"
        QString dt_str = row[0].toString();
        QDateTime dt = QDateTime::fromString(dt_str, Qt::ISODate);
        if (!dt.isValid())
            dt = QDateTime::fromString(dt_str, "yyyy-MM-dd HH:mm");
        if (!dt.isValid())
            dt = QDateTime::fromString(dt_str, "yyyy-MM-ddTHH:mm:ss");
        c.timestamp = dt.toMSecsSinceEpoch();
        // AngelOne returns OHLCV as numbers (not strings)
        c.open = row[1].toDouble();
        c.high = row[2].toDouble();
        c.low = row[3].toDouble();
        c.close = row[4].toDouble();
        c.volume = row[5].toDouble();
        candles.append(c);
    }
    return {true, candles, "", ts};
}

// ============================================================================
// Margin Calculator
// Endpoint: POST /rest/secure/angelbroking/margin/v1/batch
// ============================================================================

// ─────────────────────────────────────────────────────────────────────────────
// Token expiry detection + unified error helper
// Angel One returns status:false with message "Invalid Token" / "Access Token is expired"
// or HTTP 401 on expired sessions.
// ─────────────────────────────────────────────────────────────────────────────

bool AngelOneBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 401)
        return true;
    const QString msg = resp.json.value("message").toString().toLower();
    if (msg.contains("invalid token") || msg.contains("access token is expired") || msg.contains("token expired") ||
        msg.contains("session expired"))
        return true;
    // errorCode AB1010 = invalid/expired token per Angel One Smart API docs
    const QString code = resp.json.value("errorCode").toString();
    if (code == "AB1010" || code == "AB1011")
        return true;
    return false;
}

QString AngelOneBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    QString msg = resp.json.value("message").toString();
    if (msg.isEmpty())
        msg = resp.error.isEmpty() ? fallback : resp.error;
    if (is_token_expired(resp))
        return "[TOKEN_EXPIRED] " + msg;
    return msg;
}

static double read_num_any(const QJsonObject& o, const QStringList& keys) {
    for (const auto& k : keys) {
        if (!o.contains(k))
            continue;
        const auto v = o.value(k);
        if (v.isDouble())
            return v.toDouble();
        if (v.isString())
            return v.toString().toDouble();
    }
    return 0.0;
}

static OrderMargin parse_margin_for_order(const UnifiedOrder& o, const QJsonObject& data) {
    const auto comps = data.value("marginComponents").toObject();

    OrderMargin r;
    r.symbol = o.symbol;
    r.exchange = o.exchange;
    r.side = (o.side == OrderSide::Buy) ? "BUY" : "SELL";
    r.quantity = o.quantity;
    r.price = o.price;

    r.total = read_num_any(data, {"totalMarginRequired", "total", "margin"});
    r.var_margin = read_num_any(comps, {"spanMargin", "varMargin", "var"});
    r.elm = read_num_any(comps, {"elmMargin", "exposureMargin", "elm"});
    r.additional = read_num_any(comps, {"additionalMargin", "additional"});
    r.bo_margin = read_num_any(comps, {"boMargin", "bo"});
    r.cash = read_num_any(comps, {"cash"});
    r.pnl = read_num_any(comps, {"pnl"});

    const double notional = o.price * o.quantity;
    if (notional > 0.0 && r.total > 0.0)
        r.leverage = r.total / notional;
    return r;
}

ApiResponse<OrderMargin> AngelOneBroker::get_order_margins(const BrokerCredentials& creds, const UnifiedOrder& order) {
    const int64_t ts = QDateTime::currentSecsSinceEpoch();

    QString token = lookup_token(order.symbol, order.exchange);
    if (token.isEmpty())
        return {false, std::nullopt, "Symbol token not found for margin: " + order.exchange + ":" + order.symbol, ts};

    QJsonObject leg{
        {"exchange", order.exchange},
        {"qty", int(order.quantity)},
        {"price", order.price},
        {"productType", ao_product(order.product_type)},
        {"token", token},
        {"tradeType", ao_transaction(order.side)},
        {"orderType", ao_order_type(order.order_type)},
    };

    QJsonObject payload{{"positions", QJsonArray{leg}}};
    auto resp = BrokerHttp::instance().post_json(BASE_AO + "/rest/secure/angelbroking/margin/v1/batch", payload,
                                                 auth_headers(creds));

    if (!resp.success || !resp.json.value("status").toBool())
        return {false, std::nullopt, checked_error(resp, "Margin calculation failed"), ts};

    auto data = resp.json.value("data").toObject();
    return {true, parse_margin_for_order(order, data), "", ts};
}

ApiResponse<BasketMargin> AngelOneBroker::get_basket_margins(const BrokerCredentials& creds,
                                                             const QVector<UnifiedOrder>& orders) {
    const int64_t ts = QDateTime::currentSecsSinceEpoch();
    if (orders.isEmpty())
        return {false, std::nullopt, "No orders provided for basket margin calculation", ts};

    QJsonArray legs;
    for (const auto& o : orders) {
        QString token = lookup_token(o.symbol, o.exchange);
        if (token.isEmpty())
            return {false, std::nullopt, "Symbol token not found for margin: " + o.exchange + ":" + o.symbol, ts};

        legs.append(QJsonObject{
            {"exchange", o.exchange},
            {"qty", int(o.quantity)},
            {"price", o.price},
            {"productType", ao_product(o.product_type)},
            {"token", token},
            {"tradeType", ao_transaction(o.side)},
            {"orderType", ao_order_type(o.order_type)},
        });
    }

    QJsonObject payload{{"positions", legs}};
    auto resp = BrokerHttp::instance().post_json(BASE_AO + "/rest/secure/angelbroking/margin/v1/batch", payload,
                                                 auth_headers(creds));

    if (!resp.success || !resp.json.value("status").toBool())
        return {false, std::nullopt, checked_error(resp, "Basket margin calculation failed"), ts};

    auto data = resp.json.value("data").toObject();

    BasketMargin bm;
    bm.initial_margin = read_num_any(data, {"totalMarginRequired", "total", "margin"});
    bm.final_margin = bm.initial_margin;

    // Angel's batch endpoint returns aggregate components; mirror those into per-order
    // entries so the UI still has one row per requested leg.
    bm.orders.reserve(orders.size());
    for (const auto& o : orders) {
        OrderMargin m = parse_margin_for_order(o, data);
        if (orders.size() > 1) {
            // Keep aggregate only at basket level for multi-order requests.
            m.total = 0.0;
            m.var_margin = 0.0;
            m.elm = 0.0;
            m.additional = 0.0;
            m.bo_margin = 0.0;
            m.cash = 0.0;
            m.pnl = 0.0;
            m.leverage = 0.0;
        }
        bm.orders.append(std::move(m));
    }

    return {true, bm, "", ts};
}

// ─────────────────────────────────────────────────────────────────────────────
// lookup_token_int — public helper used by AngelOneWebSocket subscriptions
// ─────────────────────────────────────────────────────────────────────────────

quint32 AngelOneBroker::lookup_token_int(const QString& symbol, const QString& exchange) {
    QString tok = lookup_token(symbol, exchange);
    if (tok.isEmpty())
        return 0;
    bool ok = false;
    quint32 v = tok.toUInt(&ok);
    return ok ? v : 0;
}

} // namespace fincept::trading
