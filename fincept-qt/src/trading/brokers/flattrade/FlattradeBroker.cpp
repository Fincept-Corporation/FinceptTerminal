#include "trading/brokers/flattrade/FlattradeBroker.h"

#include "trading/brokers/BrokerHttp.h"
#include "trading/brokers/BrokerTokenUtil.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

namespace fincept::trading {

static constexpr const char* BASE = "https://piconnect.flattrade.in/PiConnectAPI";
static constexpr const char* AUTH_URL = "https://authapi.flattrade.in/trade/apitoken";

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// ---------- Body builder ----------

QByteArray FlattradeBroker::make_body(const QJsonObject& jdata, const QString& token) {
    QByteArray json = QJsonDocument(jdata).toJson(QJsonDocument::Compact);
    return "jData=" + QUrl::toPercentEncoding(json) + "&jKey=" + token.toUtf8();
}

QString FlattradeBroker::uid_from_creds(const BrokerCredentials& creds) {
    // ApiKey packed as "clientid:::apikey"; uid/actid is the client-id portion.
    // Fall back to user_id, then to the whole api_key if no ":::" separator.
    const QString packed = creds.api_key;
    if (packed.contains(":::"))
        return packed.section(":::", 0, 0);
    if (!creds.user_id.isEmpty())
        return creds.user_id;
    return packed;
}

// ---------- Static helpers ----------

const BrokerEnumMap<QString>& FlattradeBroker::ft_enum_map() {
    static const auto m = [] {
        BrokerEnumMap<QString> x;
        x.set(OrderType::Market, "MKT");
        x.set(OrderType::Limit, "LMT");
        x.set(OrderType::StopLoss, "SL-MKT");      // SL-M: trigger only, market fill
        x.set(OrderType::StopLossLimit, "SL-LMT"); // SL:   trigger + limit price
        x.set(ProductType::Intraday, "I");
        x.set(ProductType::Delivery, "C");
        x.set(ProductType::Margin, "M");
        return x;
    }();
    return m;
}

QString FlattradeBroker::ft_status(const QString& s) {
    QString sl = s.toUpper();
    if (sl == "COMPLETE")
        return "filled";
    if (sl == "OPEN" || sl == "TRIGGER PENDING" || sl == "TRIGGER_PENDING")
        return "open";
    if (sl == "CANCELLED" || sl == "CANCELED")
        return "cancelled";
    if (sl == "REJECTED")
        return "rejected";
    return "open";
}

bool FlattradeBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 401 || resp.status_code == 403)
        return true;
    if (!resp.success)
        return false;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return false;
    QJsonObject obj = doc.object();
    if (obj.value("stat").toString() != "Not_Ok")
        return false;
    QString emsg = obj.value("emsg").toString().toLower();
    return emsg.contains("session") || emsg.contains("invalid") || emsg.contains("token") || emsg.contains("login") ||
           emsg.contains("expired");
}

QString FlattradeBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    if (is_token_expired(resp))
        return "[TOKEN_EXPIRED] Session expired, please re-login";
    if (!resp.success)
        return resp.error.isEmpty() ? fallback : resp.error;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (doc.isObject()) {
        QString emsg = doc.object().value("emsg").toString();
        if (!emsg.isEmpty())
            return emsg;
    }
    return fallback;
}

// ---------- Auth headers ----------
// Flattrade uses no Authorization header — token is in the form body as jKey

QMap<QString, QString> FlattradeBroker::auth_headers(const BrokerCredentials& /*creds*/) const {
    return {{"Content-Type", "application/x-www-form-urlencoded"}};
}

// ---------- exchange_token ----------
// ApiKey    = "clientid:::apikey"
// ApiSecret = api_secret
// AuthCode  = request_code  (one-time code from Flattrade auth redirect)
// security_hash = SHA256(apikey + request_code + api_secret)

TokenExchangeResponse FlattradeBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                      const QString& auth_code) {
    // apikey portion of the packed credential (after ":::"); the uid is before it.
    QString apikey = api_key.contains(":::") ? api_key.section(":::", 1, 1) : api_key;
    QString uid = api_key.contains(":::") ? api_key.section(":::", 0, 0) : api_key;
    QString request_code = auth_code.trimmed();

    if (apikey.isEmpty() || api_secret.isEmpty() || request_code.isEmpty())
        return {false, "", "", "", "Flattrade login: api_key, api_secret and request_code are required", ""};

    QString security_hash = QString(
        QCryptographicHash::hash((apikey + request_code + api_secret).toUtf8(), QCryptographicHash::Sha256).toHex());

    QJsonObject payload;
    payload["api_key"] = apikey;
    payload["request_code"] = request_code;
    payload["api_secret"] = security_hash;

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(AUTH_URL, payload, {{"Content-Type", "application/json"}});

    if (!resp.success)
        return {false, "", "", "", "Login failed: " + resp.error, ""};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "", "", "Login: invalid response", ""};

    QJsonObject obj = doc.object();
    if (obj.value("stat").toString() != "Ok")
        return {false, "", "", "", obj.value("emsg").toString("Login failed"), ""};

    QString token = obj.value("token").toString();
    if (token.isEmpty())
        return {false, "", "", "", "Login: no token in response", ""};

    // Flattrade tokens lapse at the daily reset; re-auth needs a fresh web
    // request_code, so there is no silent refresh. Hint only.
    const QString extra = with_token_expiry({}, next_ist_flush_epoch(6, 0));
    return {true, token, "", uid, extra, ""};
}

// ---------- place_order ----------

OrderPlaceResponse FlattradeBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    const QString uid = uid_from_creds(creds);

    QJsonObject jdata;
    jdata["uid"] = uid;
    jdata["actid"] = uid;
    jdata["trantype"] = (order.side == OrderSide::Buy) ? "B" : "S";
    jdata["prd"] = ft_enum_map().product_or(order.product_type, "I");
    jdata["exch"] = order.exchange;
    // tsym goes in raw — make_body percent-encodes the JSON wrapper exactly once.
    jdata["tsym"] = order.symbol;
    jdata["qty"] = QString::number(static_cast<int>(order.quantity));
    jdata["dscqty"] = "0";
    jdata["prctyp"] = ft_enum_map().order_type_or(order.order_type, "MKT");
    jdata["prc"] = QString::number(order.price, 'f', 2);
    jdata["trgprc"] = QString::number(order.stop_price, 'f', 2);
    jdata["ret"] = order.validity.isEmpty() ? "DAY" : order.validity;
    jdata["mkt_protection"] = "0";
    jdata["remarks"] = "fincept";
    jdata["ordersource"] = "API";

    auto& http = BrokerHttp::instance();
    auto resp = http.post_raw(QString("%1/PlaceOrder").arg(BASE), make_body(jdata, creds.access_token),
                              {{"Content-Type", "application/x-www-form-urlencoded"}});

    if (!resp.success)
        return {false, "", checked_error(resp, "place_order failed")};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "place_order: invalid response"};

    QJsonObject obj = doc.object();
    if (obj.value("stat").toString() != "Ok")
        return {false, "", obj.value("emsg").toString("place_order failed")};

    return {true, obj.value("norenordno").toString(), ""};
}

// ---------- modify_order ----------

ApiResponse<QJsonObject> FlattradeBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                       const QJsonObject& mods) {
    int64_t ts = now_ts();
    const QString uid = uid_from_creds(creds);

    QJsonObject jdata;
    jdata["uid"] = uid;
    jdata["actid"] = uid;
    jdata["norenordno"] = order_id;
    jdata["exch"] = mods.value("exchange").toString();
    jdata["tsym"] = mods.value("symbol").toString();
    jdata["qty"] = QString::number(mods.value("quantity").toInt(0));
    jdata["prctyp"] = mods.value("orderType").toString("LMT");
    jdata["prc"] = QString::number(mods.value("price").toDouble(0), 'f', 2);
    jdata["dscqty"] = "0";
    jdata["ret"] = mods.value("validity").toString("DAY");
    // Only include trigger price for SL/SL-M — trgprc=0 on LMT causes a reject.
    const QString prctyp = jdata["prctyp"].toString();
    if (prctyp == "SL-LMT" || prctyp == "SL-MKT")
        jdata["trgprc"] = QString::number(mods.value("triggerPrice").toDouble(0), 'f', 2);

    auto& http = BrokerHttp::instance();
    auto resp = http.post_raw(QString("%1/ModifyOrder").arg(BASE), make_body(jdata, creds.access_token),
                              {{"Content-Type", "application/x-www-form-urlencoded"}});

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "modify_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "modify_order: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("stat").toString() != "Ok")
        return {false, std::nullopt, obj.value("emsg").toString("modify_order failed"), ts};

    return {true, obj, "", ts};
}

// ---------- cancel_order ----------

ApiResponse<QJsonObject> FlattradeBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    int64_t ts = now_ts();

    QJsonObject jdata;
    jdata["uid"] = uid_from_creds(creds);
    jdata["norenordno"] = order_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.post_raw(QString("%1/CancelOrder").arg(BASE), make_body(jdata, creds.access_token),
                              {{"Content-Type", "application/x-www-form-urlencoded"}});

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "cancel_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "cancel_order: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("stat").toString() != "Ok")
        return {false, std::nullopt, obj.value("emsg").toString("cancel_order failed"), ts};

    return {true, obj, "", ts};
}

// ---------- get_orders ----------

ApiResponse<QVector<BrokerOrderInfo>> FlattradeBroker::get_orders(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    QJsonObject jdata;
    jdata["uid"] = uid_from_creds(creds);

    auto& http = BrokerHttp::instance();
    auto resp = http.post_raw(QString("%1/OrderBook").arg(BASE), make_body(jdata, creds.access_token),
                              {{"Content-Type", "application/x-www-form-urlencoded"}});

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_orders failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());

    // On empty orders: returns {"stat":"Not_Ok","emsg":"No Data"} object, not array
    if (doc.isObject()) {
        QString emsg = doc.object().value("emsg").toString().toLower();
        if (emsg.contains("no data") || emsg.contains("no order") || emsg.contains("empty"))
            return {true, QVector<BrokerOrderInfo>{}, "", ts};
        return {false, std::nullopt, checked_error(resp, doc.object().value("emsg").toString("get_orders failed")), ts};
    }

    if (!doc.isArray())
        return {false, std::nullopt, "get_orders: invalid response", ts};

    QJsonArray arr = doc.array();
    QVector<BrokerOrderInfo> orders;
    orders.reserve(arr.size());

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("norenordno").toString();
        info.symbol = o.value("tsym").toString();
        info.exchange = o.value("exch").toString();
        info.quantity = o.value("qty").toString().toInt();
        info.filled_qty = o.value("fillshares").toString().toInt();
        info.price = o.value("prc").toString().toDouble();
        info.stop_price = o.value("trgprc").toString().toDouble();
        info.avg_price = o.value("avgprc").toString().toDouble();
        info.status = ft_status(o.value("status").toString());
        info.side = (o.value("trantype").toString() == "B") ? "buy" : "sell";
        info.order_type = o.value("prctyp").toString();
        info.product_type = o.value("prd").toString();
        info.timestamp = o.value("norentm").toString();
        info.message = o.value("rejreason").toString();
        orders.append(info);
    }

    return {true, orders, "", ts};
}

// ---------- get_trade_book ----------

ApiResponse<QJsonObject> FlattradeBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    const QString uid = uid_from_creds(creds);

    QJsonObject jdata;
    jdata["uid"] = uid;
    jdata["actid"] = uid;

    auto& http = BrokerHttp::instance();
    auto resp = http.post_raw(QString("%1/TradeBook").arg(BASE), make_body(jdata, creds.access_token),
                              {{"Content-Type", "application/x-www-form-urlencoded"}});

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_trade_book failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (doc.isObject())
        return {true, doc.object(), "", ts};

    QJsonObject wrapper;
    wrapper["trades"] = doc.array();
    return {true, wrapper, "", ts};
}

// ---------- get_positions ----------

ApiResponse<QVector<BrokerPosition>> FlattradeBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    const QString uid = uid_from_creds(creds);

    QJsonObject jdata;
    jdata["uid"] = uid;
    jdata["actid"] = uid;

    auto& http = BrokerHttp::instance();
    auto resp = http.post_raw(QString("%1/PositionBook").arg(BASE), make_body(jdata, creds.access_token),
                              {{"Content-Type", "application/x-www-form-urlencoded"}});

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_positions failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());

    if (doc.isObject()) {
        QString emsg = doc.object().value("emsg").toString().toLower();
        if (emsg.contains("no data") || emsg.contains("no position") || emsg.contains("empty"))
            return {true, QVector<BrokerPosition>{}, "", ts};
        return {false, std::nullopt, checked_error(resp, doc.object().value("emsg").toString("get_positions failed")),
                ts};
    }

    if (!doc.isArray())
        return {false, std::nullopt, "get_positions: invalid response", ts};

    QVector<BrokerPosition> positions;

    for (const QJsonValue& v : doc.array()) {
        QJsonObject o = v.toObject();
        int net_qty = o.value("netqty").toString().toInt();
        if (net_qty == 0)
            continue;

        BrokerPosition pos;
        pos.symbol = o.value("tsym").toString();
        pos.exchange = o.value("exch").toString();
        pos.product_type = o.value("prd").toString();
        pos.quantity = net_qty;
        pos.avg_price = o.value("netavgprc").toString().toDouble();
        pos.ltp = o.value("lp").toString().toDouble();
        pos.pnl = o.value("urmtom").toString().toDouble() + o.value("rpnl").toString().toDouble();
        pos.pnl_pct = (pos.avg_price > 0.0) ? ((pos.ltp - pos.avg_price) / pos.avg_price) * 100.0 : 0.0;
        pos.side = net_qty > 0 ? "LONG" : "SHORT";
        positions.append(pos);
    }

    return {true, positions, "", ts};
}

// ---------- get_holdings ----------

ApiResponse<QVector<BrokerHolding>> FlattradeBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    const QString uid = uid_from_creds(creds);

    QJsonObject jdata;
    jdata["uid"] = uid;
    jdata["actid"] = uid;
    jdata["prd"] = "C";

    auto& http = BrokerHttp::instance();
    auto resp = http.post_raw(QString("%1/Holdings").arg(BASE), make_body(jdata, creds.access_token),
                              {{"Content-Type", "application/x-www-form-urlencoded"}});

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());

    if (doc.isObject()) {
        QString emsg = doc.object().value("emsg").toString().toLower();
        if (emsg.contains("no data") || emsg.contains("no holding") || emsg.contains("empty"))
            return {true, QVector<BrokerHolding>{}, "", ts};
        return {false, std::nullopt, checked_error(resp, doc.object().value("emsg").toString("get_holdings failed")),
                ts};
    }

    if (!doc.isArray())
        return {false, std::nullopt, "get_holdings: invalid response", ts};

    QVector<BrokerHolding> holdings;
    QVector<QString> quote_syms; // "EXCH:TSYM:TOKEN" for LTP hydration below

    for (const QJsonValue& v : doc.array()) {
        QJsonObject o = v.toObject();

        QString symbol, exchange, token;
        QJsonArray exch_tsym = o.value("exch_tsym").toArray();
        if (!exch_tsym.isEmpty()) {
            QJsonObject et = exch_tsym[0].toObject();
            symbol = et.value("tsym").toString();
            exchange = et.value("exch").toString();
            token = et.value("token").toString();
        }
        if (symbol.isEmpty())
            continue;

        int btstqty = o.value("btstqty").toString().toInt();
        int holdqty = o.value("holdqty").toString().toInt();
        int brkcolqty = o.value("brkcolqty").toString().toInt();
        int unplgdqty = o.value("unplgdqty").toString().toInt();
        int benqty = o.value("benqty").toString().toInt();
        int npoadqty = o.value("npoadqty").toString().toInt();
        int dpqty = o.value("dpqty").toString().toInt();
        int usedqty = o.value("usedqty").toString().toInt();
        int total_qty = btstqty + holdqty + brkcolqty + unplgdqty + benqty + qMax(npoadqty, dpqty) - usedqty;

        BrokerHolding h;
        h.symbol = symbol;
        h.exchange = exchange;
        h.quantity = total_qty;
        h.avg_price = o.value("upldprc").toString().toDouble();
        // Holdings response carries no LTP field — hydrated below via GetQuotes.
        h.ltp = 0.0;
        h.invested_value = total_qty * h.avg_price;
        h.current_value = 0.0;
        h.pnl = 0.0;
        h.pnl_pct = 0.0;
        if (!token.isEmpty())
            quote_syms.append(exchange + ":" + symbol + ":" + token);
        holdings.append(h);
    }

    // Hydrate LTP and derive value/P&L via the existing GetQuotes plumbing (the
    // exch_tsym rows carry the numeric token it needs). Previously left 0, so
    // holdings views without a live-quote subscription showed ₹0 for real
    // positions. Best-effort: on failure rows keep ltp = 0.
    if (!quote_syms.isEmpty()) {
        auto quotes = get_quotes(creds, quote_syms);
        if (quotes.success && quotes.data.has_value()) {
            for (auto& h : holdings) {
                for (const auto& q : quotes.data.value()) {
                    if (q.symbol != h.symbol || q.ltp <= 0.0)
                        continue;
                    h.ltp = q.ltp;
                    h.current_value = h.quantity * q.ltp;
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

ApiResponse<BrokerFunds> FlattradeBroker::get_funds(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    const QString uid = uid_from_creds(creds);

    QJsonObject jdata;
    jdata["uid"] = uid;
    jdata["actid"] = uid;

    auto& http = BrokerHttp::instance();
    auto resp = http.post_raw(QString("%1/Limits").arg(BASE), make_body(jdata, creds.access_token),
                              {{"Content-Type", "application/x-www-form-urlencoded"}});

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_funds: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("stat").toString() != "Ok")
        return {false, std::nullopt, checked_error(resp, obj.value("emsg").toString("get_funds failed")), ts};

    const double cash = obj.value("cash").toString().toDouble();
    const double payin = obj.value("payin").toString().toDouble();
    const double payout = obj.value("payout").toString().toDouble();
    const double daycash = obj.value("daycash").toString().toDouble();
    const double rpnl = obj.value("rpnl").toString().toDouble();
    const double margin_used = obj.value("marginused").toString().toDouble();
    const double collateral = obj.value("brkcollamt").toString().toDouble();

    BrokerFunds funds;
    funds.available_balance = cash + payin - payout + daycash - margin_used;
    funds.used_margin = margin_used;
    funds.total_balance = cash + payin - payout + daycash + rpnl;
    funds.collateral = collateral;
    funds.raw_data = obj;

    return {true, funds, "", ts};
}

// ---------- get_quotes ----------
// Symbol format: "NSE:RELIANCE:22" (exchange:name:token). GetQuotes takes the numeric token.

ApiResponse<QVector<BrokerQuote>> FlattradeBroker::get_quotes(const BrokerCredentials& creds,
                                                              const QVector<QString>& symbols) {
    int64_t ts = now_ts();
    if (symbols.isEmpty())
        return {true, QVector<BrokerQuote>{}, "", ts};

    const QString uid = uid_from_creds(creds);
    auto& http = BrokerHttp::instance();
    QVector<BrokerQuote> quotes;

    for (const QString& sym : symbols) {
        QStringList parts = sym.split(":");
        QString exchange = (parts.size() >= 1) ? parts[0] : "NSE";
        QString name = (parts.size() >= 2) ? parts[1] : sym;
        QString token = (parts.size() >= 3) ? parts[2] : "";

        if (token.isEmpty())
            continue; // GetQuotes requires numeric token

        QJsonObject jdata;
        jdata["uid"] = uid;
        jdata["exch"] = exchange;
        jdata["token"] = token;

        auto resp = http.post_raw(QString("%1/GetQuotes").arg(BASE), make_body(jdata, creds.access_token),
                                  {{"Content-Type", "application/x-www-form-urlencoded"}});
        if (!resp.success)
            continue;

        QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
        if (!doc.isObject())
            continue;

        QJsonObject o = doc.object();
        if (o.value("stat").toString() != "Ok")
            continue;

        BrokerQuote quote;
        quote.symbol = name;
        quote.ltp = o.value("lp").toString().toDouble();
        quote.open = o.value("o").toString().toDouble();
        quote.high = o.value("h").toString().toDouble();
        quote.low = o.value("l").toString().toDouble();
        quote.close = o.value("c").toString().toDouble();
        quote.volume = o.value("v").toString().toLongLong();
        quote.bid = o.value("bp1").toString().toDouble();
        quote.ask = o.value("sp1").toString().toDouble();
        quote.oi = static_cast<qint64>(o.value("oi").toString().toLongLong());
        if (quote.close > 0) {
            quote.change = quote.ltp - quote.close;
            quote.change_pct = (quote.ltp - quote.close) / quote.close * 100.0;
        } else {
            quote.change = 0.0;
            quote.change_pct = 0.0;
        }
        quote.timestamp = ts;
        quotes.append(quote);
    }

    return {true, quotes, "", ts};
}

// ---------- get_history ----------
// Intraday: POST /TPSeries  — st/et epoch seconds, intrv in minutes
// Daily:    POST /EODChartData — sym="NSE:RELIANCE", from/to epoch seconds

ApiResponse<QVector<BrokerCandle>> FlattradeBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                                const QString& resolution, const QString& from_date,
                                                                const QString& to_date) {
    int64_t ts = now_ts();
    const QString uid = uid_from_creds(creds);

    QStringList parts = symbol.split(":");
    QString exchange = (parts.size() >= 1) ? parts[0] : "NSE";
    QString name = (parts.size() >= 2) ? parts[1] : symbol;
    QString token = (parts.size() >= 3) ? parts[2] : "";

    QDate from = QDate::fromString(from_date, "yyyy-MM-dd");
    QDate to = QDate::fromString(to_date, "yyyy-MM-dd");
    if (!from.isValid())
        from = QDate::currentDate().addYears(-1);
    if (!to.isValid())
        to = QDate::currentDate();

    int64_t from_epoch = QDateTime(from, QTime(9, 15, 0)).toSecsSinceEpoch();
    int64_t to_epoch = QDateTime(to, QTime(15, 30, 0)).toSecsSinceEpoch();

    bool is_daily = (resolution == "D" || resolution == "1D" || resolution == "W" || resolution == "M");
    auto& http = BrokerHttp::instance();
    QVector<BrokerCandle> candles;

    if (is_daily) {
        QJsonObject jdata;
        jdata["uid"] = uid;
        jdata["sym"] = exchange + ":" + name;
        jdata["from"] = QString::number(from_epoch);
        jdata["to"] = QString::number(to_epoch);

        auto resp = http.post_raw(QString("%1/EODChartData").arg(BASE), make_body(jdata, creds.access_token),
                                  {{"Content-Type", "application/x-www-form-urlencoded"}});
        if (!resp.success)
            return {false, std::nullopt, checked_error(resp, "get_history failed"), ts};

        QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
        if (doc.isObject())
            return {false, std::nullopt, doc.object().value("emsg").toString("get_history failed"), ts};
        if (!doc.isArray())
            return {false, std::nullopt, "get_history: invalid response", ts};

        for (const QJsonValue& v : doc.array()) {
            QJsonObject o = v.toObject();
            BrokerCandle c;
            c.timestamp = static_cast<int64_t>(o.value("ssboe").toDouble()) * 1000LL;
            c.open = o.value("into").toString().toDouble();
            c.high = o.value("inth").toString().toDouble();
            c.low = o.value("intl").toString().toDouble();
            c.close = o.value("intc").toString().toDouble();
            c.volume = o.value("intv").toString().toLongLong();
            c.oi = o.value("oi").toString().toDouble();
            candles.append(c);
        }
    } else {
        if (token.isEmpty())
            return {false, std::nullopt, "get_history: numeric token required for intraday (format NSE:SYMBOL:TOKEN)",
                    ts};

        static const QMap<QString, QString> interval_map = {
            {"1", "1"},   {"3", "3"},   {"5", "5"},     {"10", "10"},   {"15", "15"},
            {"30", "30"}, {"60", "60"}, {"120", "120"}, {"240", "240"},
        };
        QString intrv = interval_map.value(resolution, "5");

        QJsonObject jdata;
        jdata["uid"] = uid;
        jdata["exch"] = exchange;
        jdata["token"] = token;
        jdata["st"] = QString::number(from_epoch);
        jdata["et"] = QString::number(to_epoch);
        jdata["intrv"] = intrv;

        auto resp = http.post_raw(QString("%1/TPSeries").arg(BASE), make_body(jdata, creds.access_token),
                                  {{"Content-Type", "application/x-www-form-urlencoded"}});
        if (!resp.success)
            return {false, std::nullopt, checked_error(resp, "get_history failed"), ts};

        QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
        if (doc.isObject())
            return {false, std::nullopt, doc.object().value("emsg").toString("get_history failed"), ts};
        if (!doc.isArray())
            return {false, std::nullopt, "get_history: invalid response", ts};

        for (const QJsonValue& v : doc.array()) {
            QJsonObject o = v.toObject();
            // Timestamp format: "DD-MM-YYYY HH:MM:SS"
            QString time_str = o.value("time").toString();
            QDateTime dt = QDateTime::fromString(time_str, "dd-MM-yyyy HH:mm:ss");
            BrokerCandle c;
            c.timestamp = dt.isValid() ? dt.toSecsSinceEpoch() * 1000LL : 0LL;
            c.open = o.value("into").toString().toDouble();
            c.high = o.value("inth").toString().toDouble();
            c.low = o.value("intl").toString().toDouble();
            c.close = o.value("intc").toString().toDouble();
            c.volume = o.value("intv").toString().toLongLong();
            c.oi = o.value("oi").toString().toDouble();
            candles.append(c);
        }
    }

    return {true, candles, "", ts};
}

} // namespace fincept::trading
