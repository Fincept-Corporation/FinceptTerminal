// KotakBroker — Kotak Neo API v2 implementation
// Auth: Two-step TOTP + MPIN flow
//   Step 1: POST /login/1.0/tradeApiLogin  {mobileNumber, ucc, totp}  → view_token + view_sid
//   Step 2: POST /login/1.0/tradeApiValidate {mpin}                   → trading_token + trading_sid + base_url
//
// Tokens stored in BrokerCredentials:
//   access_token = packed "trading_token:::trading_sid:::base_url:::access_token"
//   user_id      = UCC
//
// Credential fields:
//   ClientCode = UCC (client ID)
//   ApiKey     = "ACCESS_TOKEN|||+91MOBILE"  (portal access token + mobile, separator "|||")
//   ApiSecret  = MPIN
//   AuthCode   = base32 TOTP secret
//
// Key quirks:
//   - base_url is DYNAMIC — comes from MPIN validation response
//   - All order endpoints: form-encoded body jData={url-encoded JSON}; short field names
//   - Quotes endpoint: Authorization: access_token header (NOT Auth/Sid)
//   - Historical data NOT supported by Kotak Neo — returns empty
//   - Token field in master contract CSVs is pSymbol (numeric string)
//   - Token expiry: stat == "Not_Ok" with auth emsg, or HTTP 401

#include "trading/brokers/kotak/KotakBroker.h"

#include "core/logging/Logger.h"
#include "trading/brokers/BrokerHttp.h"
#include "trading/instruments/InstrumentService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace fincept::trading {

static constexpr const char* TAG = "KotakBroker";
static const QString LOGIN_BASE = "https://mis.kotaksecurities.com";

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// ── TOTP helpers (RFC 6238, same implementation as AngelOne) ──────────────────

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

QString KotakBroker::generate_totp(const QString& base32_secret) {
    if (base32_secret.length() == 6 && base32_secret.toUInt() > 0)
        return base32_secret; // already a 6-digit code

    QByteArray key = base32_decode(base32_secret);
    if (key.isEmpty()) {
        LOG_WARN(TAG, "TOTP: invalid base32 secret");
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

// ── Token packing / unpacking ─────────────────────────────────────────────────
// Packed format: "trading_token:::trading_sid:::base_url:::access_token[:::server_id]"
// 4-part (legacy) format is still accepted; server_id will be empty in that case.

KotakBroker::TokenParts KotakBroker::unpack(const QString& packed) {
    const QStringList parts = packed.split(":::");
    if (parts.size() < 4)
        return {};
    TokenParts p;
    p.trading_token = parts[0];
    p.trading_sid = parts[1];
    p.base_url = parts[2];
    p.access_token = parts[3];
    if (parts.size() >= 5)
        p.server_id = parts[4];
    p.valid = true;
    return p;
}

// ── Exchange segment mapping ──────────────────────────────────────────────────
QString KotakBroker::kotak_exchange(const QString& exchange) {
    static const QMap<QString, QString> map = {
        {"NSE", "nse_cm"}, {"BSE", "bse_cm"}, {"NFO", "nse_fo"},
        {"BFO", "bse_fo"}, {"CDS", "cde_fo"}, {"MCX", "mcx_fo"},
    };
    return map.value(exchange.toUpper(), "nse_cm");
}

static QString canonical_exchange(const QString& seg) {
    static const QMap<QString, QString> map = {
        {"nse_cm", "NSE"}, {"bse_cm", "BSE"}, {"nse_fo", "NFO"},
        {"bse_fo", "BFO"}, {"cde_fo", "CDS"}, {"mcx_fo", "MCX"},
    };
    return map.value(seg, seg);
}

// Per Kotak Neo SDK order_type map:
//   "SL"   = Stop loss LIMIT  (trigger + limit price) → StopLossLimit
//   "SL-M" = Stop loss MARKET (trigger only)          → StopLoss
const BrokerEnumMap<QString>& KotakBroker::kotak_enum_map() {
    static const auto m = [] {
        BrokerEnumMap<QString> x;
        x.set(OrderType::Market, "MKT");
        x.set(OrderType::Limit, "L");
        x.set(OrderType::StopLoss, "SL-M");
        x.set(OrderType::StopLossLimit, "SL");
        x.set(ProductType::Intraday, "MIS");
        x.set(ProductType::Delivery, "CNC");
        x.set(ProductType::Margin, "NRML");
        return x;
    }();
    return m;
}

// ── pSymbol lookup ────────────────────────────────────────────────────────────
QString KotakBroker::lookup_psymbol(const QString& symbol, const QString& exchange, const QString& broker_id) {
    if (!broker_id.isEmpty() && InstrumentService::instance().is_loaded(broker_id)) {
        auto tok = InstrumentService::instance().instrument_token(symbol, exchange, broker_id);
        if (tok.has_value() && tok.value() > 0)
            return QString::number(static_cast<qlonglong>(tok.value()));
    }
    LOG_WARN(TAG, QString("pSymbol not found for %1:%2").arg(exchange, symbol));
    return {};
}

// ── Form-encoding helper ──────────────────────────────────────────────────────
// PROD /quick/* endpoints expect plain form-encoded fields, not a single
// `jData=<json>` wrapper (that was a v1-era convention; the current Kotak Neo
// SDK posts each field on its own). Flatten the JSON object into a string-map;
// BrokerHttp::post_form percent-encodes values.
QMap<QString, QString> KotakBroker::jobj_to_form(const QJsonObject& obj) {
    QMap<QString, QString> out;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const auto& v = it.value();
        switch (v.type()) {
            case QJsonValue::Bool:
                out.insert(it.key(), v.toBool() ? "true" : "false");
                break;
            case QJsonValue::Double: {
                // Preserve integer formatting where possible
                const double d = v.toDouble();
                if (d == static_cast<qint64>(d))
                    out.insert(it.key(), QString::number(static_cast<qint64>(d)));
                else
                    out.insert(it.key(), QString::number(d, 'f', 2));
                break;
            }
            case QJsonValue::String:
                out.insert(it.key(), v.toString());
                break;
            case QJsonValue::Null:
                out.insert(it.key(), QString());
                break;
            default:
                out.insert(it.key(), QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact)));
                break;
        }
    }
    return out;
}

// Append ?sId=<server_id> when known; required for multi-DC routing on PROD.
QString KotakBroker::with_sid(const QString& base_path, const TokenParts& p) {
    if (p.server_id.isEmpty())
        return p.base_url + base_path;
    const QChar sep = base_path.contains('?') ? QLatin1Char('&') : QLatin1Char('?');
    return p.base_url + base_path + sep + "sId=" + p.server_id;
}

// ── Token expiry detection ────────────────────────────────────────────────────
bool KotakBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 401 || resp.status_code == 403)
        return true;
    const QString emsg = resp.json.value("emsg").toString().toLower();
    if (emsg.contains("invalid session") || emsg.contains("session expired") || emsg.contains("unauthorized") ||
        emsg.contains("not logged in") || emsg.contains("token expired") || emsg.contains("please login"))
        return true;
    return false;
}

QString KotakBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    QString msg = resp.json.value("emsg").toString();
    if (msg.isEmpty())
        msg = resp.json.value("message").toString();
    if (msg.isEmpty())
        msg = resp.error.isEmpty() ? fallback : resp.error;
    if (is_token_expired(resp))
        return "[TOKEN_EXPIRED] " + msg;
    return msg;
}

// ── Auth headers ──────────────────────────────────────────────────────────────
// Standard API calls (orders, positions, etc.) use Auth/Sid/neo-fin-key
QMap<QString, QString> KotakBroker::auth_headers(const BrokerCredentials& creds) const {
    auto p = unpack(creds.access_token);
    if (!p.valid)
        return {};
    return {
        {"Auth", p.trading_token},
        {"Sid", p.trading_sid},
        {"neo-fin-key", "neotradeapi"},
        {"accept", "application/json"},
        {"Content-Type", "application/x-www-form-urlencoded"},
    };
}

// ── Token Exchange ─────────────────────────────────────────────────────────────
// api_key    = "ACCESS_TOKEN|||+91MOBILE"  (portal static token + mobile, split by "|||")
// api_secret = MPIN
// auth_code  = base32 TOTP secret
// UCC comes from the ClientCode credential field — not accessible here directly.
// Workaround: user is instructed to enter UCC in ClientCode field and the UI
// pre-fills api_key as "ACCESS_TOKEN|||+91MOBILE|||UCC" (3-part).
// Fallback: if only 2 parts, UCC is unknown and we skip it (some flows don't need it).
TokenExchangeResponse KotakBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                  const QString& auth_code) {
    // Parse api_key: "access_token|||+91mobile|||UCC"
    const QStringList parts = api_key.split("|||");
    const QString access_token_portal = parts.value(0).trimmed();
    QString mobile = parts.value(1).trimmed();
    const QString ucc = parts.value(2).trimmed();

    if (access_token_portal.isEmpty())
        return {false, "", "", "", "", "Access token is required (format: token|||+91mobile|||UCC)"};
    if (ucc.isEmpty())
        return {false, "", "", "", "", "UCC is required (format: token|||+91mobile|||UCC)"};
    if (api_secret.trimmed().isEmpty())
        return {false, "", "", "", "", "MPIN is required"};
    if (auth_code.trimmed().isEmpty())
        return {false, "", "", "", "", "TOTP secret is required"};

    // Normalise mobile to +91XXXXXXXXXX
    mobile.remove(' ');
    if (!mobile.isEmpty() && !mobile.startsWith('+')) {
        if (mobile.startsWith("91") && mobile.length() == 12)
            mobile = "+" + mobile;
        else
            mobile = "+91" + mobile;
    }

    const QString totp_code = generate_totp(auth_code.trimmed());
    if (totp_code.isEmpty())
        return {false, "", "", "", "", "Failed to generate TOTP — check TOTP secret"};

    auto& http = BrokerHttp::instance();

    // ── Step 1: TOTP login ────────────────────────────────────────────────────
    QJsonObject step1_body;
    step1_body["ucc"] = ucc;
    step1_body["totp"] = totp_code;
    if (!mobile.isEmpty())
        step1_body["mobileNumber"] = mobile;

    QMap<QString, QString> step1_hdrs = {
        {"Authorization", access_token_portal},
        {"neo-fin-key", "neotradeapi"},
        {"Content-Type", "application/json"},
    };

    auto r1 = http.post_json(LOGIN_BASE + "/login/1.0/tradeApiLogin", step1_body, step1_hdrs);
    if (!r1.success) {
        LOG_ERROR(TAG, "Step1 login HTTP error: " + r1.error);
        return {false, "", "", "", "", "TOTP login failed: " + r1.error};
    }

    const auto d1 = r1.json.value("data").toObject();
    if (d1.value("status").toString() != "success") {
        const QString err = r1.json.value("errMsg").toString(r1.json.value("message").toString("TOTP login failed"));
        LOG_ERROR(TAG, "Step1 login error: " + err);
        return {false, "", "", "", "", "TOTP login error: " + err};
    }

    const QString view_token = d1.value("token").toString();
    const QString view_sid = d1.value("sid").toString();
    LOG_INFO(TAG, "Kotak Step1 OK, sid=" + view_sid);

    // ── Step 2: MPIN validation ───────────────────────────────────────────────
    QJsonObject step2_body;
    step2_body["mpin"] = api_secret.trimmed();

    QMap<QString, QString> step2_hdrs = {
        {"Authorization", access_token_portal}, {"neo-fin-key", "neotradeapi"}, {"sid", view_sid}, {"Auth", view_token},
        {"Content-Type", "application/json"},
    };

    auto r2 = http.post_json(LOGIN_BASE + "/login/1.0/tradeApiValidate", step2_body, step2_hdrs);
    if (!r2.success) {
        LOG_ERROR(TAG, "Step2 MPIN HTTP error: " + r2.error);
        return {false, "", "", "", "", "MPIN validation failed: " + r2.error};
    }

    const auto d2 = r2.json.value("data").toObject();
    if (d2.value("status").toString() != "success") {
        const QString err =
            r2.json.value("errMsg").toString(r2.json.value("message").toString("MPIN validation failed"));
        LOG_ERROR(TAG, "Step2 MPIN error: " + err);
        return {false, "", "", "", "", "MPIN validation error: " + err};
    }

    const QString trading_token = d2.value("token").toString();
    const QString trading_sid = d2.value("sid").toString();
    const QString base_url = d2.value("baseUrl").toString();
    // hsServerId routes data-centre traffic; if absent fall back to dataCenter.
    QString server_id = d2.value("hsServerId").toString();
    if (server_id.isEmpty())
        server_id = d2.value("dataCenter").toString();

    if (base_url.isEmpty())
        LOG_WARN(TAG, "baseUrl missing from MPIN response — API calls may fail");
    if (server_id.isEmpty())
        LOG_WARN(TAG, "hsServerId/dataCenter missing from MPIN response — sId routing disabled");

    // Pack: trading_token:::trading_sid:::base_url:::access_token_portal:::server_id
    const QString packed = trading_token + ":::" + trading_sid + ":::" + base_url + ":::" + access_token_portal +
                           ":::" + server_id;

    LOG_INFO(TAG, "Kotak auth complete, ucc=" + ucc + " base_url=" + base_url + " sId=" + server_id);
    return {true, packed, /*refresh*/ "", ucc, /*additional*/ "", ""};
}

// ── Place Order ───────────────────────────────────────────────────────────────
// POST {base_url}/quick/order/rule/ms/place — form-encoded jData (all values strings)
// Response: stat=="Ok" → nOrdNo is order ID
OrderPlaceResponse KotakBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    auto p = unpack(creds.access_token);
    if (!p.valid)
        return {false, "", "[TOKEN_EXPIRED] Invalid or missing session token"};

    const QString psymbol = lookup_psymbol(order.symbol, order.exchange, creds.broker_id);
    if (psymbol.isEmpty())
        return {false, "", "pSymbol not found for " + order.exchange + ":" + order.symbol};

    QJsonObject jobj;
    jobj["am"] = order.amo ? "YES" : "NO";
    jobj["dq"] = "0";
    jobj["bc"] = "1";
    jobj["es"] = kotak_exchange(order.exchange);
    jobj["mp"] = "0";
    jobj["pc"] = kotak_enum_map().product_or(order.product_type, "MIS");
    jobj["pf"] = "N";
    jobj["pr"] = QString::number(order.price, 'f', 2);
    jobj["pt"] = kotak_enum_map().order_type_or(order.order_type, "MKT");
    jobj["qt"] = QString::number(static_cast<int>(order.quantity));
    jobj["rt"] = order.validity.isEmpty() ? "DAY" : order.validity;
    jobj["tp"] = QString::number(order.stop_price, 'f', 2);
    jobj["ts"] = psymbol;
    jobj["tt"] = order.side == OrderSide::Buy ? "B" : "S";
    jobj["os"] = "NEOTRADEAPI"; // required ORDER_SOURCE tag on PROD

    auto hdrs = auth_headers(creds);
    auto resp = BrokerHttp::instance().post_form(with_sid("/quick/order/rule/ms/place", p), jobj_to_form(jobj), hdrs);

    if (!resp.success || resp.json.value("stat").toString() != "Ok") {
        const QString err = checked_error(resp, "place_order failed");
        LOG_ERROR(TAG, "place_order: " + err);
        return {false, "", err};
    }

    const QString order_id = resp.json.value("nOrdNo").toString();
    LOG_INFO(TAG, "place_order OK: " + order_id);
    return {true, order_id, ""};
}

// ── Modify Order ──────────────────────────────────────────────────────────────
// POST {base_url}/quick/order/vr/modify — form-encoded jData
ApiResponse<QJsonObject> KotakBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                   const QJsonObject& mods) {
    int64_t ts = now_ts();
    auto p = unpack(creds.access_token);
    if (!p.valid)
        return {false, std::nullopt, "[TOKEN_EXPIRED] Invalid session", ts};

    const QString symbol = mods.value("symbol").toString();
    const QString exchange = mods.value("exchange").toString("NSE");
    const QString psymbol = lookup_psymbol(symbol, exchange, creds.broker_id);

    QJsonObject jobj;
    jobj["no"] = order_id;
    // `tk` is the numeric scrip token; `ts` is the trading symbol — they're
    // distinct in the current SDK. Previously both fields were psymbol.
    jobj["tk"] = psymbol;
    jobj["ts"] = symbol;
    jobj["dq"] = QString::number(mods.value("disclosed_quantity").toInt(0));
    jobj["es"] = kotak_exchange(exchange);
    jobj["mp"] = "0";
    jobj["dd"] = mods.value("goodtilldate").toString(""); // empty by default
    jobj["vd"] = mods.value("validity").toString("DAY");
    jobj["pc"] = mods.value("product").toString("MIS");
    jobj["pr"] = QString::number(mods.value("price").toDouble(0.0), 'f', 2);
    jobj["pt"] = mods.value("order_type").toString("L");
    jobj["qt"] = QString::number(mods.value("quantity").toInt(0));
    jobj["fq"] = QString::number(mods.value("filled_quantity").toInt(0));
    jobj["tp"] = QString::number(mods.value("trigger_price").toDouble(0.0), 'f', 2);
    jobj["tt"] = mods.value("side").toString("B");
    jobj["os"] = "NEOTRADEAPI";

    auto hdrs = auth_headers(creds);
    auto resp = BrokerHttp::instance().post_form(with_sid("/quick/order/vr/modify", p), jobj_to_form(jobj), hdrs);

    if (!resp.success || resp.json.value("stat").toString() != "Ok")
        return {false, std::nullopt, checked_error(resp, "modify_order failed"), ts};
    return {true, resp.json, "", ts};
}

// ── Cancel Order ──────────────────────────────────────────────────────────────
// POST {base_url}/quick/order/cancel — form-encoded jData={on, am}
ApiResponse<QJsonObject> KotakBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    int64_t ts = now_ts();
    auto p = unpack(creds.access_token);
    if (!p.valid)
        return {false, std::nullopt, "[TOKEN_EXPIRED] Invalid session", ts};

    QJsonObject jobj;
    jobj["on"] = order_id;
    jobj["am"] = "NO";
    jobj["os"] = "NEOTRADEAPI";

    auto hdrs = auth_headers(creds);
    auto resp = BrokerHttp::instance().post_form(with_sid("/quick/order/cancel", p), jobj_to_form(jobj), hdrs);

    if (!resp.success || resp.json.value("stat").toString() != "Ok")
        return {false, std::nullopt, checked_error(resp, "cancel_order failed"), ts};
    return {true, resp.json, "", ts};
}

// ── Get Orders ────────────────────────────────────────────────────────────────
// GET {base_url}/quick/user/orders — response: {data: [...]}
// Note: numeric fields come as strings in Kotak responses
ApiResponse<QVector<BrokerOrderInfo>> KotakBroker::get_orders(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto p = unpack(creds.access_token);
    if (!p.valid)
        return {false, std::nullopt, "[TOKEN_EXPIRED] Invalid session", ts};

    auto hdrs = auth_headers(creds);
    hdrs["Content-Type"] = "application/json";
    auto resp = BrokerHttp::instance().get(with_sid("/quick/user/orders", p), hdrs);

    if (!resp.success || resp.json.value("stat").toString() == "Not_Ok")
        return {false, std::nullopt, checked_error(resp, "get_orders failed"), ts};

    const QJsonArray arr = resp.json.value("data").toArray();
    QVector<BrokerOrderInfo> orders;
    orders.reserve(arr.size());
    for (const auto& v : arr) {
        const auto o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("nOrdNo").toString();
        info.symbol = o.value("trdSym").toString();
        info.exchange = canonical_exchange(o.value("exSeg").toString());
        info.side = o.value("tt").toString() == "B" ? "buy" : "sell";
        info.order_type = o.value("pt").toString();
        info.product_type = o.value("prod").toString();
        info.quantity = o.value("qt").toString().toDouble();
        info.price = o.value("pr").toString().toDouble();
        info.trigger_price = o.value("tp").toString().toDouble();
        info.filled_qty = o.value("flBuyQty").toString().toDouble() + o.value("flSellQty").toString().toDouble();
        info.avg_price = o.value("avgPr").toString().toDouble();
        info.status = o.value("ordSt").toString().toLower();
        info.timestamp = o.value("ordDt").toString();
        info.message = o.value("rejRsn").toString();
        orders.append(info);
    }
    return {true, orders, "", ts};
}

// ── Get Trade Book ────────────────────────────────────────────────────────────
// GET {base_url}/quick/user/trades
ApiResponse<QJsonObject> KotakBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto p = unpack(creds.access_token);
    if (!p.valid)
        return {false, std::nullopt, "[TOKEN_EXPIRED] Invalid session", ts};

    auto hdrs = auth_headers(creds);
    hdrs["Content-Type"] = "application/json";
    auto resp = BrokerHttp::instance().get(with_sid("/quick/user/trades", p), hdrs);

    if (!resp.success || resp.json.value("stat").toString() == "Not_Ok")
        return {false, std::nullopt, checked_error(resp, "get_trade_book failed"), ts};
    return {true, resp.json, "", ts};
}

// ── Get Positions ─────────────────────────────────────────────────────────────
// GET {base_url}/quick/user/positions
// Fields: trdSym, exSeg, prod, flBuyQty, flSellQty, cfBuyQty, cfSellQty, buyAmt, sellAmt, ltp
ApiResponse<QVector<BrokerPosition>> KotakBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto p = unpack(creds.access_token);
    if (!p.valid)
        return {false, std::nullopt, "[TOKEN_EXPIRED] Invalid session", ts};

    auto hdrs = auth_headers(creds);
    hdrs["Content-Type"] = "application/json";
    auto resp = BrokerHttp::instance().get(with_sid("/quick/user/positions", p), hdrs);

    if (!resp.success || resp.json.value("stat").toString() == "Not_Ok")
        return {false, std::nullopt, checked_error(resp, "get_positions failed"), ts};

    const QJsonArray arr = resp.json.value("data").toArray();
    QVector<BrokerPosition> positions;
    positions.reserve(arr.size());
    for (const auto& v : arr) {
        const auto o = v.toObject();
        const double buy_qty = o.value("flBuyQty").toString().toDouble() + o.value("cfBuyQty").toString().toDouble();
        const double sell_qty = o.value("flSellQty").toString().toDouble() + o.value("cfSellQty").toString().toDouble();
        const double net_qty = buy_qty - sell_qty;
        if (net_qty == 0.0)
            continue;

        const double buy_amt = o.value("buyAmt").toString().toDouble();
        const double sell_amt = o.value("sellAmt").toString().toDouble();

        BrokerPosition pos;
        pos.symbol = o.value("trdSym").toString();
        pos.exchange = canonical_exchange(o.value("exSeg").toString());
        pos.product_type = o.value("prod").toString();
        pos.quantity = net_qty;
        pos.avg_price = buy_qty > 0 ? buy_amt / buy_qty : 0.0;
        pos.ltp = o.value("ltp").toString().toDouble();
        pos.pnl = (pos.ltp * net_qty) - (buy_amt - sell_amt);
        pos.side = net_qty > 0 ? "LONG" : "SHORT";
        if (pos.avg_price > 0)
            pos.pnl_pct = (pos.ltp - pos.avg_price) / pos.avg_price * 100.0;
        positions.append(pos);
    }
    return {true, positions, "", ts};
}

// ── Get Holdings ──────────────────────────────────────────────────────────────
// GET {base_url}/portfolio/v1/holdings
ApiResponse<QVector<BrokerHolding>> KotakBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto p = unpack(creds.access_token);
    if (!p.valid)
        return {false, std::nullopt, "[TOKEN_EXPIRED] Invalid session", ts};

    auto hdrs = auth_headers(creds);
    hdrs["Content-Type"] = "application/json";
    auto resp = BrokerHttp::instance().get(with_sid("/portfolio/v1/holdings", p), hdrs);

    if (!resp.success || resp.json.value("stat").toString() == "Not_Ok")
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};

    const QJsonArray arr = resp.json.value("data").toArray();
    QVector<BrokerHolding> holdings;
    holdings.reserve(arr.size());
    for (const auto& v : arr) {
        const auto h = v.toObject();
        BrokerHolding holding;
        holding.symbol = h.value("trdSym").toString();
        holding.exchange = canonical_exchange(h.value("exSeg").toString());
        holding.quantity = h.value("qty").toString().toDouble();
        holding.avg_price = h.value("avgPr").toString().toDouble();
        holding.ltp = h.value("ltp").toString().toDouble();
        holding.invested_value = holding.quantity * holding.avg_price;
        holding.current_value = holding.quantity * holding.ltp;
        holding.pnl = holding.current_value - holding.invested_value;
        if (holding.invested_value > 0)
            holding.pnl_pct = holding.pnl / holding.invested_value * 100.0;
        holdings.append(holding);
    }
    return {true, holdings, "", ts};
}

// ── Get Funds ─────────────────────────────────────────────────────────────────
// POST {base_url}/quick/user/limits — form-encoded jData={"seg":"ALL","exch":"ALL","prod":"ALL"}
// Available = CollateralValue + RmsPayInAmt - RmsPayOutAmt + Collateral
ApiResponse<BrokerFunds> KotakBroker::get_funds(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    auto p = unpack(creds.access_token);
    if (!p.valid)
        return {false, std::nullopt, "[TOKEN_EXPIRED] Invalid session", ts};

    QJsonObject jobj;
    jobj["seg"] = "ALL";
    jobj["exch"] = "ALL";
    jobj["prod"] = "ALL";

    auto hdrs = auth_headers(creds);
    auto resp = BrokerHttp::instance().post_form(with_sid("/quick/user/limits", p), jobj_to_form(jobj), hdrs);

    if (!resp.success || resp.json.value("stat").toString() != "Ok")
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    // Limits payload is wrapped: {stat: "Ok", data: {CollateralValue, RmsPayInAmt, ...}}.
    // Reading from the root made every field 0 — the values are nested under `data`.
    const QJsonObject d = resp.json.value("data").toObject();
    const double collateral_value = d.value("CollateralValue").toString().toDouble();
    const double pay_in = d.value("RmsPayInAmt").toString().toDouble();
    const double pay_out = d.value("RmsPayOutAmt").toString().toDouble();
    const double collateral = d.value("Collateral").toString().toDouble();
    const double margin_used = d.value("MarginUsed").toString().toDouble();

    BrokerFunds funds;
    funds.available_balance = collateral_value + pay_in - pay_out + collateral;
    funds.used_margin = margin_used;
    funds.total_balance = funds.available_balance + funds.used_margin;
    funds.collateral = collateral;
    funds.raw_data = resp.json;
    return {true, funds, "", ts};
}

// ── Get Quotes ────────────────────────────────────────────────────────────────
// GET {base_url}/script-details/1.0/quotes/neosymbol/{comma-sep queries}/all
// Header: Authorization: access_token (NOT Auth/Sid — quotes use a different auth)
// Query format: "nse_cm|pSymbol1,nse_cm|pSymbol2,..."
// Response: JSON array; each item has ltp, ohlc{open,high,low,close}, last_volume, open_int
ApiResponse<QVector<BrokerQuote>> KotakBroker::get_quotes(const BrokerCredentials& creds,
                                                          const QVector<QString>& symbols) {
    int64_t ts = now_ts();
    if (symbols.isEmpty())
        return {true, QVector<BrokerQuote>{}, "", ts};

    auto p = unpack(creds.access_token);
    if (!p.valid)
        return {false, std::nullopt, "[TOKEN_EXPIRED] Invalid session", ts};

    QStringList queries;
    QMap<QString, QString> query_to_symbol; // "nse_cm|pSymbol" → original symbol name

    for (const auto& sym : symbols) {
        QString exch = "NSE", name = sym;
        if (sym.contains(':')) {
            exch = sym.section(':', 0, 0);
            name = sym.section(':', 1);
        }
        const QString psymbol = lookup_psymbol(name, exch, creds.broker_id);
        if (psymbol.isEmpty())
            continue;
        const QString seg = kotak_exchange(exch);
        const QString query = seg + "|" + psymbol;
        queries.append(query);
        query_to_symbol[query] = name;
    }

    if (queries.isEmpty())
        return {false, std::nullopt, "No valid pSymbols found", ts};

    // URL-encode keeping | and , as-is (safe for path segment)
    const QString combined = queries.join(",");
    const QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(combined, /*keep=*/QByteArray("|,")));

    // Quotes use Authorization header, not Auth/Sid
    QMap<QString, QString> quote_hdrs = {
        {"Authorization", p.access_token},
        {"Content-Type", "application/json"},
        {"accept", "application/json"},
    };

    // Defensive: strip trailing slash from base_url so we don't produce "//script-details/..."
    QString base = p.base_url;
    while (base.endsWith('/'))
        base.chop(1);
    const QString url = base + "/script-details/1.0/quotes/neosymbol/" + encoded + "/all";
    auto resp = BrokerHttp::instance().get(url, quote_hdrs);

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_quotes failed"), ts};

    // Response is a JSON array (not wrapped in data object)
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(resp.raw_body.toUtf8(), &err);
    if (!doc.isArray())
        return {false, std::nullopt, "Unexpected quotes response format", ts};

    const QJsonArray arr = doc.array();
    QVector<BrokerQuote> quotes;
    quotes.reserve(arr.size());
    for (const auto& v : arr) {
        const auto q = v.toObject();
        BrokerQuote quote;
        // Match back to symbol: response has exchange + exchange_token fields
        const QString key = q.value("exchange").toString() + "|" + q.value("exchange_token").toString();
        quote.symbol = query_to_symbol.value(key, q.value("display_symbol").toString());

        const auto ohlc = q.value("ohlc").toObject();
        quote.ltp = q.value("ltp").toDouble();
        quote.open = ohlc.value("open").toDouble();
        quote.high = ohlc.value("high").toDouble();
        quote.low = ohlc.value("low").toDouble();
        quote.close = ohlc.value("close").toDouble();
        quote.volume = q.value("last_volume").toDouble();
        quote.change = quote.ltp - quote.close;
        if (quote.close > 0)
            quote.change_pct = quote.change / quote.close * 100.0;
        quote.timestamp = ts;
        quotes.append(quote);
    }
    return {true, quotes, "", ts};
}

// ── Get History ───────────────────────────────────────────────────────────────
// Kotak Neo does NOT support historical data
ApiResponse<QVector<BrokerCandle>> KotakBroker::get_history(const BrokerCredentials& /*creds*/,
                                                            const QString& /*symbol*/, const QString& /*resolution*/,
                                                            const QString& /*from_date*/, const QString& /*to_date*/) {
    int64_t ts = now_ts();
    LOG_WARN(TAG, "get_history called — Kotak Neo does not support historical data");
    return {true, QVector<BrokerCandle>{}, "", ts};
}

} // namespace fincept::trading
