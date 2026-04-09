#include "trading/brokers/shoonya/ShoonyaBroker.h"

#include "trading/brokers/BrokerHttp.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

namespace fincept::trading {

static constexpr const char* BASE = "https://api.shoonya.com/NorenWClientTP";

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// ---------- Body builders ----------

QByteArray ShoonyaBroker::make_body(const QJsonObject& jdata, const QString& token) {
    QByteArray json = QJsonDocument(jdata).toJson(QJsonDocument::Compact);
    return "jData=" + QUrl::toPercentEncoding(json) + "&jKey=" + token.toUtf8();
}

QByteArray ShoonyaBroker::make_login_body(const QJsonObject& jdata) {
    QByteArray json = QJsonDocument(jdata).toJson(QJsonDocument::Compact);
    return "jData=" + QUrl::toPercentEncoding(json);
}

// ---------- Static helpers ----------

QString ShoonyaBroker::sh_product(ProductType p) {
    switch (p) {
        case ProductType::Intraday:
            return "I";
        case ProductType::Delivery:
            return "C";
        case ProductType::Margin:
            return "M";
        default:
            return "I";
    }
}

QString ShoonyaBroker::sh_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:
            return "MKT";
        case OrderType::Limit:
            return "LMT";
        case OrderType::StopLoss:
            return "SL-MKT"; // SL-M: trigger only, market fill
        case OrderType::StopLossLimit:
            return "SL-LMT"; // SL:   trigger + limit price
        default:
            return "MKT";
    }
}

QString ShoonyaBroker::sh_status(const QString& s) {
    QString sl = s.toUpper();
    if (sl == "COMPLETE")
        return "filled";
    if (sl == "OPEN" || sl == "TRIGGER PENDING")
        return "open";
    if (sl == "CANCELLED")
        return "cancelled";
    if (sl == "REJECTED")
        return "rejected";
    return "open";
}

bool ShoonyaBroker::is_token_expired(const BrokerHttpResponse& resp) {
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

QString ShoonyaBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
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
// Shoonya uses no Authorization header — token is in the form body as jKey

QMap<QString, QString> ShoonyaBroker::auth_headers(const BrokerCredentials& /*creds*/) const {
    return {{"Content-Type", "application/x-www-form-urlencoded"}};
}

// ---------- exchange_token ----------
// ApiKey    = uid
// ApiSecret = api_secret  (used to build appkey hash)
// AuthCode  = "vendor_code:::totp"

TokenExchangeResponse ShoonyaBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                    const QString& auth_code) {
    QStringList parts = auth_code.split(":::");
    QString vendor_code = (parts.size() >= 1) ? parts[0] : "";
    QString totp = (parts.size() >= 2) ? parts[1] : "";

    // pwd = SHA256(password) — api_secret is the password here
    QString pwd = QString(QCryptographicHash::hash(api_secret.toUtf8(), QCryptographicHash::Sha256).toHex());

    // appkey = SHA256(uid + "|" + api_secret)
    QString appkey =
        QString(QCryptographicHash::hash((api_key + "|" + api_secret).toUtf8(), QCryptographicHash::Sha256).toHex());

    QJsonObject jdata;
    jdata["uid"] = api_key;
    jdata["pwd"] = pwd;
    jdata["factor2"] = totp;
    jdata["vc"] = vendor_code;
    jdata["appkey"] = appkey;
    jdata["imei"] = "abc1234";
    jdata["apkversion"] = "1.0.0";
    jdata["source"] = "API";

    auto& http = BrokerHttp::instance();
    auto resp = http.post_raw(QString("%1/QuickAuth").arg(BASE), make_login_body(jdata),
                              {{"Content-Type", "application/x-www-form-urlencoded"}});

    if (!resp.success)
        return {false, "", "", "", "Login failed: " + resp.error};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "", "", "Login: invalid response"};

    QJsonObject obj = doc.object();
    if (obj.value("stat").toString() != "Ok")
        return {false, "", "", "", obj.value("emsg").toString("Login failed")};

    QString token = obj.value("susertoken").toString();
    QString uid = obj.value("uid").toString(api_key);

    if (token.isEmpty())
        return {false, "", "", "", "Login: no susertoken in response"};

    return {true, token, "", uid, ""};
}

// ---------- place_order ----------

OrderPlaceResponse ShoonyaBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    QJsonObject jdata;
    jdata["uid"] = creds.user_id;
    jdata["actid"] = creds.user_id;
    jdata["trantype"] = (order.side == OrderSide::Buy) ? "B" : "S";
    jdata["prd"] = sh_product(order.product_type);
    jdata["exch"] = order.exchange;
    jdata["tsym"] = QString(QUrl::toPercentEncoding(order.symbol));
    jdata["qty"] = QString::number(static_cast<int>(order.quantity));
    jdata["dscqty"] = "0";
    jdata["prctyp"] = sh_order_type(order.order_type);
    jdata["prc"] = QString::number(order.price, 'f', 2);
    jdata["trgprc"] = QString::number(order.stop_price, 'f', 2);
    jdata["ret"] = "DAY";
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

ApiResponse<QJsonObject> ShoonyaBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                     const QJsonObject& mods) {
    int64_t ts = now_ts();

    QJsonObject jdata;
    jdata["uid"] = creds.user_id;
    jdata["actid"] = creds.user_id;
    jdata["norenordno"] = order_id;
    jdata["exch"] = mods.value("exchange").toString();
    jdata["tsym"] = QString(QUrl::toPercentEncoding(mods.value("symbol").toString()));
    jdata["qty"] = QString::number(mods.value("quantity").toInt(0));
    jdata["prctyp"] = mods.value("orderType").toString("LMT");
    jdata["prc"] = QString::number(mods.value("price").toDouble(0), 'f', 2);
    jdata["trgprc"] = QString::number(mods.value("triggerPrice").toDouble(0), 'f', 2);
    jdata["dscqty"] = "0";
    jdata["ret"] = "DAY";
    jdata["ordersource"] = "API";

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

ApiResponse<QJsonObject> ShoonyaBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    int64_t ts = now_ts();

    QJsonObject jdata;
    jdata["uid"] = creds.user_id;
    jdata["norenordno"] = order_id;
    jdata["ordersource"] = "API";

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

ApiResponse<QVector<BrokerOrderInfo>> ShoonyaBroker::get_orders(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    QJsonObject jdata;
    jdata["uid"] = creds.user_id;
    jdata["ordersource"] = "API";

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
        info.status = sh_status(o.value("status").toString());
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

ApiResponse<QJsonObject> ShoonyaBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    QJsonObject jdata;
    jdata["uid"] = creds.user_id;
    jdata["actid"] = creds.user_id;

    auto& http = BrokerHttp::instance();
    auto resp = http.post_raw(QString("%1/TradeBook").arg(BASE), make_body(jdata, creds.access_token),
                              {{"Content-Type", "application/x-www-form-urlencoded"}});

    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_trade_book failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (doc.isObject())
        return {true, doc.object(), "", ts};

    // Wrap array in object for consistency
    QJsonObject wrapper;
    wrapper["trades"] = doc.array();
    return {true, wrapper, "", ts};
}

// ---------- get_positions ----------

ApiResponse<QVector<BrokerPosition>> ShoonyaBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    QJsonObject jdata;
    jdata["uid"] = creds.user_id;
    jdata["actid"] = creds.user_id;

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
        pos.side = net_qty > 0 ? "LONG" : "SHORT";
        positions.append(pos);
    }

    return {true, positions, "", ts};
}

// ---------- get_holdings ----------

ApiResponse<QVector<BrokerHolding>> ShoonyaBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    QJsonObject jdata;
    jdata["uid"] = creds.user_id;
    jdata["actid"] = creds.user_id;
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

    for (const QJsonValue& v : doc.array()) {
        QJsonObject o = v.toObject();

        // Extract symbol from exch_tsym array
        QString symbol, exchange;
        QJsonArray exch_tsym = o.value("exch_tsym").toArray();
        if (!exch_tsym.isEmpty()) {
            QJsonObject et = exch_tsym[0].toObject();
            symbol = et.value("tsym").toString();
            exchange = et.value("exch").toString();
        }
        if (symbol.isEmpty())
            continue;

        // Total qty = btstqty + holdqty + brkcolqty + unplgdqty + benqty + max(npoadqty, dpqty) - usedqty
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
        h.ltp = 0.0; // not provided in holdings response
        h.pnl = 0.0;
        holdings.append(h);
    }

    return {true, holdings, "", ts};
}

// ---------- get_funds ----------
// available = cash + payin - marginused

ApiResponse<BrokerFunds> ShoonyaBroker::get_funds(const BrokerCredentials& creds) {
    int64_t ts = now_ts();

    QJsonObject jdata;
    jdata["uid"] = creds.user_id;
    jdata["actid"] = creds.user_id;

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

    double cash = obj.value("cash").toString().toDouble();
    double payin = obj.value("payin").toString().toDouble();
    double margin_used = obj.value("marginused").toString().toDouble();
    double collateral = obj.value("brkcollamt").toString().toDouble();

    BrokerFunds funds;
    funds.available_balance = cash + payin - margin_used;
    funds.used_margin = margin_used;
    funds.total_balance = cash + payin;
    funds.collateral = collateral;

    return {true, funds, "", ts};
}

// ---------- get_quotes ----------
// Symbol format: "NSE:RELIANCE:22" (exchange:name:token)
// GetQuotes takes numeric token, not symbol name

ApiResponse<QVector<BrokerQuote>> ShoonyaBroker::get_quotes(const BrokerCredentials& creds,
                                                            const QVector<QString>& symbols) {
    int64_t ts = now_ts();
    if (symbols.isEmpty())
        return {true, QVector<BrokerQuote>{}, "", ts};

    auto& http = BrokerHttp::instance();
    QVector<BrokerQuote> quotes;

    for (const QString& sym : symbols) {
        QStringList parts = sym.split(":");
        QString exchange = (parts.size() >= 1) ? parts[0] : "NSE";
        QString name = (parts.size() >= 2) ? parts[1] : sym;
        QString token = (parts.size() >= 3) ? parts[2] : "";

        if (token.isEmpty())
            continue; // Shoonya GetQuotes requires numeric token

        QJsonObject jdata;
        jdata["uid"] = creds.user_id;
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
        if (quote.close > 0)
            quote.change_pct = (quote.ltp - quote.close) / quote.close * 100.0;
        quote.change = quote.ltp - quote.close;
        quote.timestamp = ts;
        quotes.append(quote);
    }

    return {true, quotes, "", ts};
}

// ---------- get_history ----------
// Intraday: POST /TPSeries  — st/et epoch seconds, intrv in minutes, fields: into/inth/intl/intc/intv
// Daily:    POST /EODChartData — sym="NSE:RELIANCE", from/to epoch seconds, field: ssboe

ApiResponse<QVector<BrokerCandle>> ShoonyaBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                              const QString& resolution, const QString& from_date,
                                                              const QString& to_date) {
    int64_t ts = now_ts();

    // Parse symbol: "NSE:RELIANCE:22" or "NSE:RELIANCE"
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
        // EODChartData uses sym="EXCHANGE:SYMBOL" and fields from/to
        QJsonObject jdata;
        jdata["uid"] = creds.user_id;
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
            candles.append(c);
        }
    } else {
        // TPSeries — needs numeric token
        if (token.isEmpty())
            return {false, std::nullopt, "get_history: numeric token required for intraday (format NSE:SYMBOL:TOKEN)",
                    ts};

        // Map resolution to minutes
        static const QMap<QString, QString> interval_map = {
            {"1", "1"},   {"3", "3"},   {"5", "5"},     {"10", "10"},   {"15", "15"},
            {"30", "30"}, {"60", "60"}, {"120", "120"}, {"240", "240"},
        };
        QString intrv = interval_map.value(resolution, "5");

        QJsonObject jdata;
        jdata["uid"] = creds.user_id;
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
            candles.append(c);
        }
    }

    return {true, candles, "", ts};
}

} // namespace fincept::trading
