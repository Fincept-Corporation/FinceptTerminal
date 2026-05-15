#include "trading/brokers/iifl/IIFLBroker.h"

#include "trading/brokers/BrokerHttp.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

namespace fincept::trading {

static constexpr const char* INTERACTIVE_URL = "https://ttblaze.iifl.com/interactive";
static constexpr const char* MARKET_DATA_URL = "https://ttblaze.iifl.com/apimarketdata";

static int64_t now_ts() {
    return QDateTime::currentSecsSinceEpoch();
}

// ---------- Static helpers ----------

IIFLBroker::TokenParts IIFLBroker::unpack_token(const QString& packed) {
    QStringList parts = packed.split(":::");
    if (parts.size() >= 2)
        return {parts[0], parts[1]};
    return {packed, ""};
}

IIFLBroker::KeyParts IIFLBroker::unpack_key(const QString& packed) {
    QStringList parts = packed.split(":::");
    if (parts.size() >= 2)
        return {parts[0], parts[1]};
    return {packed, ""};
}

bool IIFLBroker::is_token_expired(const BrokerHttpResponse& resp) {
    if (resp.status_code == 401)
        return true;
    if (!resp.success)
        return false;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return false;
    QString type = doc.object().value("type").toString();
    return (type == "error" || type == "sessionerror");
}

QString IIFLBroker::checked_error(const BrokerHttpResponse& resp, const QString& fallback) {
    if (is_token_expired(resp))
        return "[TOKEN_EXPIRED] Session expired, please re-login";
    if (!resp.success)
        return resp.error.isEmpty() ? fallback : resp.error;
    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (doc.isObject()) {
        QString msg = doc.object().value("message").toString();
        if (!msg.isEmpty())
            return msg;
    }
    return fallback;
}

QString IIFLBroker::iifl_exchange(const QString& exchange) {
    if (exchange == "NSE")
        return "NSECM";
    if (exchange == "BSE")
        return "BSECM";
    if (exchange == "NFO")
        return "NSEFO";
    if (exchange == "BFO")
        return "BSEFO";
    if (exchange == "MCX")
        return "MCXFO";
    if (exchange == "CDS")
        return "NSECD";
    return "NSECM";
}

int IIFLBroker::iifl_exchange_id(const QString& exchange) {
    if (exchange == "NSE")
        return 1;
    if (exchange == "NFO")
        return 2;
    if (exchange == "CDS")
        return 3;
    if (exchange == "BSE")
        return 11;
    if (exchange == "BFO")
        return 12;
    if (exchange == "MCX")
        return 51;
    return 1;
}

const BrokerEnumMap<QString>& IIFLBroker::iifl_enum_map() {
    static const auto m = [] {
        BrokerEnumMap<QString> x;
        x.set(OrderType::Market, "MARKET");
        x.set(OrderType::Limit, "LIMIT");
        x.set(OrderType::StopLoss, "STOPMARKET");      // SL-M: trigger only, market fill
        x.set(OrderType::StopLossLimit, "STOPLIMIT"); // SL:   trigger + limit price
        x.set(ProductType::Intraday, "MIS");
        x.set(ProductType::Delivery, "CNC");
        x.set(ProductType::Margin, "NRML");
        return x;
    }();
    return m;
}

QString IIFLBroker::iifl_compression(const QString& resolution) {
    // Intraday compressions are seconds (per XTS doc); daily/weekly/monthly take
    // letter codes "D"/"W"/"M". Previously sending 1440/10080 for daily was
    // wrong — XTS rejects those minute values for daily bars.
    if (resolution == "1" || resolution == "1m")
        return "60";
    if (resolution == "5" || resolution == "5m")
        return "300";
    if (resolution == "15" || resolution == "15m")
        return "900";
    if (resolution == "30" || resolution == "30m")
        return "1800";
    if (resolution == "60" || resolution == "1h")
        return "3600";
    if (resolution == "D" || resolution == "1D" || resolution == "day")
        return "D";
    if (resolution == "W" || resolution == "1W" || resolution == "week")
        return "W";
    if (resolution == "M" || resolution == "1M" || resolution == "month")
        return "M";
    return "60";
}

QString IIFLBroker::iifl_time(const QString& date_str, bool end_of_day) {
    // date_str expected as "YYYY-MM-DD"
    QDate d = QDate::fromString(date_str, "yyyy-MM-dd");
    if (!d.isValid())
        return date_str;
    QString time_part = end_of_day ? "153000" : "091500";
    // Format: "Jan 01 2024 091500"
    return d.toString("MMM dd yyyy") + " " + time_part;
}

// ---------- Auth headers ----------

QMap<QString, QString> IIFLBroker::auth_headers(const BrokerCredentials& creds) const {
    TokenParts tok = unpack_token(creds.access_token);
    return {{"authorization", tok.trade}, {"Content-Type", "application/json"}, {"Accept", "application/json"}};
}

// ---------- exchange_token ----------

TokenExchangeResponse IIFLBroker::exchange_token(const QString& api_key, const QString& api_secret,
                                                 const QString& /*auth_code*/) {
    KeyParts trade_keys = unpack_key(api_key);
    KeyParts market_keys = unpack_key(api_secret);

    // Step 1: Interactive login (trade token)
    QJsonObject trade_body;
    trade_body["appKey"] = trade_keys.app_key;
    trade_body["secretKey"] = trade_keys.secret;
    trade_body["source"] = "WEBAPI";

    auto& http = BrokerHttp::instance();
    auto trade_resp = http.post_json(QString("%1/user/session").arg(INTERACTIVE_URL), trade_body,
                                     {{"Content-Type", "application/json"}, {"Accept", "application/json"}});
    if (!trade_resp.success)
        return {false, "", "", "", "Interactive login failed: " + trade_resp.error, ""};

    QJsonDocument trade_doc = QJsonDocument::fromJson(trade_resp.raw_body.toUtf8());
    if (!trade_doc.isObject())
        return {false, "", "", "", "Interactive login: invalid response", ""};

    QJsonObject trade_obj = trade_doc.object();
    if (trade_obj.value("type").toString() != "success")
        return {false, "", "", "", trade_obj.value("description").toString("Interactive login failed"), ""};

    QString trade_token = trade_obj.value("result").toObject().value("token").toString();
    if (trade_token.isEmpty())
        return {false, "", "", "", "Interactive login: no token in response", ""};

    // Step 2: Market data login (feed token)
    QJsonObject market_body;
    market_body["appKey"] = market_keys.app_key;
    market_body["secretKey"] = market_keys.secret;
    market_body["source"] = "WEBAPI";

    auto market_resp = http.post_json(QString("%1/auth/login").arg(MARKET_DATA_URL), market_body,
                                      {{"Content-Type", "application/json"}, {"Accept", "application/json"}});
    if (!market_resp.success)
        return {false, "", "", "", "Market data login failed: " + market_resp.error, ""};

    QJsonDocument market_doc = QJsonDocument::fromJson(market_resp.raw_body.toUtf8());
    if (!market_doc.isObject())
        return {false, "", "", "", "Market data login: invalid response", ""};

    QJsonObject market_obj = market_doc.object();
    if (market_obj.value("type").toString() != "success")
        return {false, "", "", "", market_obj.value("description").toString("Market data login failed"), ""};

    QJsonObject market_result = market_obj.value("result").toObject();
    QString feed_token = market_result.value("token").toString();
    QString user_id = market_result.value("userID").toString();

    if (feed_token.isEmpty())
        return {false, "", "", "", "Market data login: no token in response", ""};

    // Pack both tokens
    QString packed_token = trade_token + ":::" + feed_token;
    return {true, packed_token, user_id, "", "", ""};
}

// ---------- place_order ----------

OrderPlaceResponse IIFLBroker::place_order(const BrokerCredentials& creds, const UnifiedOrder& order) {
    TokenParts tok = unpack_token(creds.access_token);

    QJsonObject body;
    body["exchangeSegment"] = iifl_exchange(order.exchange);
    // exchangeInstrumentID must be a JSON number per XTS spec; passing as string
    // works for some endpoints but is silently rejected for derivatives.
    body["exchangeInstrumentID"] = order.instrument_token.toLongLong();
    body["productType"] = iifl_enum_map().product_or(order.product_type, "MIS");
    body["orderType"] = iifl_enum_map().order_type_or(order.order_type, "MARKET");
    body["orderSide"] = (order.side == OrderSide::Buy) ? "BUY" : "SELL";
    body["timeInForce"] = "DAY";
    // XTS spec: integer/double, not stringified.
    body["disclosedQuantity"] = 0;
    body["orderQuantity"] = static_cast<int>(order.quantity);
    body["limitPrice"] = order.price;
    body["stopPrice"] = order.stop_price;
    body["orderUniqueIdentifier"] = "fincept";

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(
        QString("%1/orders").arg(INTERACTIVE_URL), body,
        {{"authorization", tok.trade}, {"Content-Type", "application/json"}, {"Accept", "application/json"}});
    if (!resp.success)
        return {false, "", checked_error(resp, "place_order failed")};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, "", "place_order: invalid response"};

    QJsonObject obj = doc.object();
    if (obj.value("type").toString() != "success")
        return {false, "", obj.value("description").toString("place_order failed")};

    QString order_id = obj.value("result").toObject().value("AppOrderID").toVariant().toString();
    return {true, order_id, ""};
}

// ---------- modify_order ----------

ApiResponse<QJsonObject> IIFLBroker::modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                  const QJsonObject& mods) {
    int64_t ts = now_ts();
    TokenParts tok = unpack_token(creds.access_token);

    QJsonObject body;
    body["appOrderID"] = order_id.toLongLong();
    body["modifiedProductType"] = mods.value("productType").toString("MIS");
    body["modifiedOrderType"] = mods.value("orderType").toString("MARKET");
    body["modifiedOrderQuantity"] = mods.value("quantity").toInt(0);
    body["modifiedDisclosedQuantity"] = 0;
    body["modifiedLimitPrice"] = mods.value("limitPrice").toDouble(0.0);
    body["modifiedStopPrice"] = mods.value("stopPrice").toDouble(0.0);
    body["modifiedTimeInForce"] = mods.value("timeInForce").toString("DAY");
    body["orderUniqueIdentifier"] = "fincept";

    auto& http = BrokerHttp::instance();
    auto resp = http.put_json(
        QString("%1/orders").arg(INTERACTIVE_URL), body,
        {{"authorization", tok.trade}, {"Content-Type", "application/json"}, {"Accept", "application/json"}});
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "modify_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "modify_order: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("type").toString() != "success")
        return {false, std::nullopt, obj.value("description").toString("modify_order failed"), ts};

    return {true, obj.value("result").toObject(), "", ts};
}

// ---------- cancel_order ----------

ApiResponse<QJsonObject> IIFLBroker::cancel_order(const BrokerCredentials& creds, const QString& order_id) {
    int64_t ts = now_ts();
    TokenParts tok = unpack_token(creds.access_token);

    auto& http = BrokerHttp::instance();
    auto resp =
        http.del(QString("%1/orders?appOrderID=%2").arg(INTERACTIVE_URL, order_id),
                 {{"authorization", tok.trade}, {"Content-Type", "application/json"}, {"Accept", "application/json"}});
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "cancel_order failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "cancel_order: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("type").toString() != "success")
        return {false, std::nullopt, obj.value("description").toString("cancel_order failed"), ts};

    return {true, obj.value("result").toObject(), "", ts};
}

// ---------- get_orders ----------

ApiResponse<QVector<BrokerOrderInfo>> IIFLBroker::get_orders(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    TokenParts tok = unpack_token(creds.access_token);

    auto& http = BrokerHttp::instance();
    auto resp =
        http.get(QString("%1/orders").arg(INTERACTIVE_URL),
                 {{"authorization", tok.trade}, {"Content-Type", "application/json"}, {"Accept", "application/json"}});
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_orders failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_orders: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("type").toString() != "success")
        return {false, std::nullopt, obj.value("description").toString("get_orders failed"), ts};

    QJsonArray arr = obj.value("result").toArray();
    QVector<BrokerOrderInfo> orders;
    orders.reserve(arr.size());

    auto parse_status = [](const QString& s) -> QString {
        if (s == "Filled")
            return "filled";
        if (s == "Cancelled")
            return "cancelled";
        if (s == "Rejected")
            return "rejected";
        if (s == "New")
            return "open";
        if (s == "Trigger Pending")
            return "pending";
        if (s == "PartiallyFilled")
            return "open";
        return "open";
    };

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        BrokerOrderInfo info;
        info.order_id = o.value("AppOrderID").toVariant().toString();
        info.symbol = o.value("TradingSymbol").toString();
        info.exchange = o.value("ExchangeSegment").toString();
        info.quantity = o.value("OrderQuantity").toInt();
        info.filled_qty = o.value("OrderQuantity").toInt() - o.value("LeavesQuantity").toInt();
        info.price = o.value("LimitPrice").toDouble();
        info.stop_price = o.value("StopPrice").toDouble();
        info.status = parse_status(o.value("OrderStatus").toString());
        info.side = (o.value("OrderSide").toString() == "BUY") ? "buy" : "sell";
        info.product_type = o.value("ProductType").toString();
        info.order_type = o.value("OrderType").toString();
        info.exchange_order_id = o.value("ExchangeOrderID").toString();
        info.timestamp = o.value("OrderGeneratedDateTime").toString();
        orders.append(info);
    }

    return {true, orders, "", ts};
}

// ---------- get_trade_book ----------

ApiResponse<QJsonObject> IIFLBroker::get_trade_book(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    TokenParts tok = unpack_token(creds.access_token);

    auto& http = BrokerHttp::instance();
    auto resp =
        http.get(QString("%1/orders/trades").arg(INTERACTIVE_URL),
                 {{"authorization", tok.trade}, {"Content-Type", "application/json"}, {"Accept", "application/json"}});
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_trade_book failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_trade_book: invalid response", ts};

    return {true, doc.object(), "", ts};
}

// ---------- get_positions ----------

ApiResponse<QVector<BrokerPosition>> IIFLBroker::get_positions(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    TokenParts tok = unpack_token(creds.access_token);

    auto& http = BrokerHttp::instance();
    auto resp =
        http.get(QString("%1/portfolio/positions?dayOrNet=NET").arg(INTERACTIVE_URL),
                 {{"authorization", tok.trade}, {"Content-Type", "application/json"}, {"Accept", "application/json"}});
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_positions failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_positions: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("type").toString() != "success")
        return {false, std::nullopt, obj.value("description").toString("get_positions failed"), ts};

    QJsonArray arr = obj.value("result").toObject().value("positionList").toArray();
    QVector<BrokerPosition> positions;

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        int qty = o.value("Quantity").toInt();
        if (qty == 0)
            continue;

        BrokerPosition pos;
        pos.symbol = o.value("TradingSymbol").toString();
        pos.exchange = o.value("ExchangeSegment").toString();
        pos.quantity = qty;
        pos.avg_price = o.value("AveragePrice").toDouble();
        pos.ltp = o.value("LastPrice").toDouble();
        pos.pnl = o.value("UnrealizedMTM").toDouble();
        pos.product_type = o.value("ProductType").toString();
        positions.append(pos);
    }

    return {true, positions, "", ts};
}

// ---------- get_holdings ----------

ApiResponse<QVector<BrokerHolding>> IIFLBroker::get_holdings(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    TokenParts tok = unpack_token(creds.access_token);

    auto& http = BrokerHttp::instance();
    auto resp =
        http.get(QString("%1/portfolio/holdings").arg(INTERACTIVE_URL),
                 {{"authorization", tok.trade}, {"Content-Type", "application/json"}, {"Accept", "application/json"}});
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_holdings failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_holdings: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("type").toString() != "success")
        return {false, std::nullopt, obj.value("description").toString("get_holdings failed"), ts};

    QJsonArray arr = obj.value("result").toObject().value("RMSHoldings").toArray();
    QVector<BrokerHolding> holdings;

    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        BrokerHolding h;
        // Holding rows carry tradingsymbol + both NSE and BSE exchange codes.
        // Prefer NSE when present, otherwise fall back to BSE.
        h.symbol = o.value("TradingSymbol").toString();
        if (h.symbol.isEmpty())
            h.symbol = o.value("ExchangeNSECode").toString();
        if (h.symbol.isEmpty())
            h.symbol = o.value("ExchangeBSECode").toString();
        h.exchange = !o.value("ExchangeNSECode").toString().isEmpty() ? "NSE" : "BSE";
        h.quantity = o.value("HoldingQuantity").toInt();
        h.avg_price = o.value("BuyPrice").toDouble();
        h.ltp = o.value("Price").toDouble();
        h.pnl = (h.ltp - h.avg_price) * h.quantity;
        holdings.append(h);
    }

    return {true, holdings, "", ts};
}

// ---------- get_funds ----------

ApiResponse<BrokerFunds> IIFLBroker::get_funds(const BrokerCredentials& creds) {
    int64_t ts = now_ts();
    TokenParts tok = unpack_token(creds.access_token);

    auto& http = BrokerHttp::instance();
    auto resp =
        http.get(QString("%1/user/balance").arg(INTERACTIVE_URL),
                 {{"authorization", tok.trade}, {"Content-Type", "application/json"}, {"Accept", "application/json"}});
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_funds failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_funds: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("type").toString() != "success")
        return {false, std::nullopt, obj.value("description").toString("get_funds failed"), ts};

    QJsonArray balance_list = obj.value("result").toObject().value("BalanceList").toArray();
    if (balance_list.isEmpty())
        return {false, std::nullopt, "get_funds: empty BalanceList", ts};

    QJsonObject limits = balance_list[0].toObject().value("limitObject").toObject().value("RMSSubLimits").toObject();

    double available = limits.value("netMarginAvailable").toDouble();
    double collateral = limits.value("collateral").toDouble();
    double used = limits.value("marginUtilized").toDouble();

    BrokerFunds funds;
    funds.available_balance = available;
    funds.used_margin = used;
    funds.total_balance = available + used;
    funds.collateral = collateral;

    return {true, funds, "", ts};
}

// ---------- get_quotes ----------

ApiResponse<QVector<BrokerQuote>> IIFLBroker::get_quotes(const BrokerCredentials& creds,
                                                         const QVector<QString>& symbols) {
    int64_t ts = now_ts();
    TokenParts tok = unpack_token(creds.access_token);

    // Symbols expected as "NSE:RELIANCE:2885" (exchange:symbol:token)
    QJsonArray instruments;
    QMap<QString, QString> token_to_symbol;

    for (const QString& sym : symbols) {
        QStringList parts = sym.split(":");
        if (parts.size() < 3)
            continue;
        QString exchange = parts[0];
        QString name = parts[1];
        QString inst_id = parts[2];

        QJsonObject inst;
        inst["exchangeSegment"] = iifl_exchange_id(exchange);
        inst["exchangeInstrumentID"] = inst_id.toLongLong();
        instruments.append(inst);
        token_to_symbol[inst_id] = name;
    }

    if (instruments.isEmpty())
        return {true, QVector<BrokerQuote>{}, "", ts};

    QJsonObject body;
    body["instruments"] = instruments;
    // 1501 = Touchline (lightweight LTP/OHLC); 1502 = MarketDepth (heavier, full book).
    // Touchline is the canonical "get_quotes" target and stays within standard
    // data-tier permissions.
    body["xtsMessageCode"] = 1501;
    body["publishFormat"] = "JSON";

    auto& http = BrokerHttp::instance();
    auto resp = http.post_json(
        QString("%1/instruments/quotes").arg(MARKET_DATA_URL), body,
        {{"authorization", tok.feed}, {"Content-Type", "application/json"}, {"Accept", "application/json"}});
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_quotes failed"), ts};

    QJsonDocument outer = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!outer.isObject())
        return {false, std::nullopt, "get_quotes: invalid response", ts};

    QJsonObject outer_obj = outer.object();
    if (outer_obj.value("type").toString() != "success")
        return {false, std::nullopt, outer_obj.value("description").toString("get_quotes failed"), ts};

    QJsonArray list_quotes = outer_obj.value("result").toObject().value("listQuotes").toArray();
    QVector<BrokerQuote> quotes;

    for (const QJsonValue& qv : list_quotes) {
        // listQuotes entries are JSON strings that need to be parsed again
        QString raw = qv.toString();
        QJsonDocument qd = QJsonDocument::fromJson(raw.toUtf8());
        if (!qd.isObject())
            continue;

        QJsonObject qo = qd.object();
        QString inst_id = QString::number(qo.value("ExchangeInstrumentID").toVariant().toLongLong());
        QJsonObject touchline = qo.value("Touchline").toObject();

        BrokerQuote quote;
        quote.symbol = token_to_symbol.value(inst_id, inst_id);
        quote.ltp = touchline.value("LastTradedPrice").toDouble();
        quote.open = touchline.value("Open").toDouble();
        quote.high = touchline.value("High").toDouble();
        quote.low = touchline.value("Low").toDouble();
        quote.close = touchline.value("Close").toDouble();
        quote.volume = static_cast<int64_t>(touchline.value("TotalTradedQuantity").toDouble());
        quotes.append(quote);
    }

    return {true, quotes, "", ts};
}

// ---------- get_history ----------

ApiResponse<QVector<BrokerCandle>> IIFLBroker::get_history(const BrokerCredentials& creds, const QString& symbol,
                                                           const QString& resolution, const QString& from_date,
                                                           const QString& to_date) {
    int64_t ts = now_ts();
    TokenParts tok = unpack_token(creds.access_token);

    // Symbol format: "NSE:RELIANCE:2885"
    QStringList parts = symbol.split(":");
    QString exchange = (parts.size() >= 1) ? parts[0] : "NSE";
    QString inst_id = (parts.size() >= 3) ? parts[2] : symbol;

    QString seg_str = iifl_exchange(exchange);
    QString compress = iifl_compression(resolution);
    QString start = iifl_time(from_date, false);
    QString end = iifl_time(to_date, true);

    QString url = QString("%1/instruments/ohlc?exchangeSegment=%2&exchangeInstrumentID=%3"
                          "&startTime=%4&endTime=%5&compressionValue=%6")
                      .arg(MARKET_DATA_URL, seg_str, inst_id, QString(QUrl::toPercentEncoding(start)),
                           QString(QUrl::toPercentEncoding(end)), compress);

    auto& http = BrokerHttp::instance();
    auto resp = http.get(
        url, {{"authorization", tok.feed}, {"Content-Type", "application/json"}, {"Accept", "application/json"}});
    if (!resp.success)
        return {false, std::nullopt, checked_error(resp, "get_history failed"), ts};

    QJsonDocument doc = QJsonDocument::fromJson(resp.raw_body.toUtf8());
    if (!doc.isObject())
        return {false, std::nullopt, "get_history: invalid response", ts};

    QJsonObject obj = doc.object();
    if (obj.value("type").toString() != "success")
        return {false, std::nullopt, obj.value("description").toString("get_history failed"), ts};

    // result.dataReponse is a pipe-delimited string: "ts|o|h|l|c|v,ts|o|h|l|c|v,..."
    // The doc field name has the historical typo "dataReponse" but some XTS
    // deployments now return the corrected "dataResponse" — try both.
    const QJsonObject result_obj = obj.value("result").toObject();
    QString data_response = result_obj.value("dataReponse").toString();
    if (data_response.isEmpty())
        data_response = result_obj.value("dataResponse").toString();
    if (data_response.isEmpty())
        return {true, QVector<BrokerCandle>{}, "", ts};

    QVector<BrokerCandle> candles;
    QStringList records = data_response.split(",", Qt::SkipEmptyParts);
    for (const QString& rec : records) {
        QStringList fields = rec.split("|");
        if (fields.size() < 6)
            continue;

        BrokerCandle candle;
        // timestamp field is epoch seconds from IIFL
        candle.timestamp = fields[0].toLongLong() * 1000LL; // convert to ms
        candle.open = fields[1].toDouble();
        candle.high = fields[2].toDouble();
        candle.low = fields[3].toDouble();
        candle.close = fields[4].toDouble();
        candle.volume = static_cast<int64_t>(fields[5].toDouble());
        candles.append(candle);
    }

    return {true, candles, "", ts};
}

} // namespace fincept::trading
