#include "iifl_broker.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <chrono>
#include <sstream>

namespace fincept::trading {

using namespace fincept::http;

// ============================================================================
// Helpers
// ============================================================================

static int64_t iifl_now_ts() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

// ============================================================================
// Auth headers — IIFL uses token directly in Authorization header
// ============================================================================

std::map<std::string, std::string> IIFLBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Authorization", creds.access_token},
        {"Content-Type",  "application/json"},
        {"Accept",        "application/json"}
    };
}

// ============================================================================
// Mapping helpers
// ============================================================================

const char* IIFLBroker::iifl_exchange_segment(const std::string& exchange) {
    if (exchange == "NSE")  return "NSECM";
    if (exchange == "BSE")  return "BSECM";
    if (exchange == "NFO")  return "NSEFO";
    if (exchange == "BFO")  return "BSEFO";
    if (exchange == "MCX")  return "MCXFO";
    if (exchange == "CDS")  return "NSECD";
    return "NSECM";
}

int IIFLBroker::iifl_segment_id(const std::string& exchange) {
    if (exchange == "NSE")  return 1;
    if (exchange == "NFO")  return 2;
    if (exchange == "CDS")  return 3;
    if (exchange == "BSE")  return 11;
    if (exchange == "BFO")  return 12;
    if (exchange == "MCX")  return 51;
    return 1;
}

const char* IIFLBroker::iifl_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "MARKET";
        case OrderType::Limit:         return "LIMIT";
        case OrderType::StopLoss:      return "STOPMARKET";
        case OrderType::StopLossLimit: return "STOPLIMIT";
        default:                       return "MARKET";
    }
}

const char* IIFLBroker::iifl_product_type(ProductType p) {
    switch (p) {
        case ProductType::Intraday:      return "MIS";
        case ProductType::Delivery:      return "CNC";
        case ProductType::Margin:        return "NRML";
        case ProductType::CoverOrder:    return "CO";
        case ProductType::BracketOrder:  return "BO";
        default:                         return "MIS";
    }
}

bool IIFLBroker::is_success(const json& body) {
    return body.value("type", "") == "success";
}

std::string IIFLBroker::extract_error(const json& body, const char* fallback) {
    if (body.contains("description") && body["description"].is_string())
        return body["description"].get<std::string>();
    if (body.contains("message") && body["message"].is_string())
        return body["message"].get<std::string>();
    return fallback;
}

// ============================================================================
// Token Exchange — appKey + secretKey → session token
// ============================================================================

TokenExchangeResponse IIFLBroker::exchange_token(const std::string& api_key,
                                                    const std::string& api_secret,
                                                    const std::string& /*auth_code*/) {
    auto& client = HttpClient::instance();

    json payload = {
        {"appKey",    api_key},
        {"secretKey", api_secret}
    };

    Headers headers = {
        {"Content-Type", "application/json"}
    };

    auto resp = client.post_json(std::string(base_url()) + "/interactive/user/session",
                                  payload, headers);

    if (!resp.success) {
        return {false, "", "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();
    if (body.is_null()) {
        return {false, "", "", "Invalid JSON response"};
    }

    if (is_success(body) && body.contains("result") && body["result"].is_object()) {
        const auto& result = body["result"];
        std::string token = result.value("token", "");
        std::string user_id = result.value("userID", "");
        if (!token.empty()) {
            return {true, token, user_id, ""};
        }
    }

    return {false, "", "", extract_error(body, "Authentication failed")};
}

// ============================================================================
// Orders
// ============================================================================

OrderPlaceResponse IIFLBroker::place_order(const BrokerCredentials& creds,
                                              const UnifiedOrder& order) {
    auto& client = HttpClient::instance();

    json payload = {
        {"exchangeSegment",      iifl_exchange_segment(order.exchange)},
        {"exchangeInstrumentID", order.symbol},
        {"productType",          iifl_product_type(order.product_type)},
        {"orderType",            iifl_order_type(order.order_type)},
        {"orderSide",            order.side == OrderSide::Buy ? "BUY" : "SELL"},
        {"orderQuantity",        static_cast<int>(order.quantity)},
        {"limitPrice",           order.price},
        {"stopPrice",            order.stop_price},
        {"timeInForce",          "DAY"},
        {"disclosedQuantity",    0}
    };

    auto resp = client.post_json(std::string(base_url()) + "/interactive/orders",
                                  payload, auth_headers(creds));

    if (!resp.success) {
        return {false, "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();
    if (is_success(body) && body.contains("result") && body["result"].is_object()) {
        const auto& result = body["result"];
        std::string oid;
        if (result.contains("AppOrderID")) {
            // AppOrderID may be numeric
            if (result["AppOrderID"].is_string())
                oid = result["AppOrderID"].get<std::string>();
            else
                oid = std::to_string(result["AppOrderID"].get<int64_t>());
        }
        return {true, oid, ""};
    }

    return {false, "", extract_error(body, "Order placement failed")};
}

ApiResponse<json> IIFLBroker::modify_order(const BrokerCredentials& creds,
                                              const std::string& order_id,
                                              const json& modifications) {
    const int64_t ts = iifl_now_ts();
    auto& client = HttpClient::instance();

    json payload = {{"appOrderID", order_id}};

    if (modifications.contains("quantity"))
        payload["orderQuantity"] = modifications["quantity"];
    if (modifications.contains("price"))
        payload["limitPrice"] = modifications["price"];
    if (modifications.contains("trigger_price"))
        payload["stopPrice"] = modifications["trigger_price"];
    if (modifications.contains("order_type"))
        payload["orderType"] = modifications["order_type"];

    auto resp = client.put_json(std::string(base_url()) + "/interactive/orders",
                                 payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (is_success(body)) {
        return {true, body, "", ts};
    }
    return {false, std::nullopt, extract_error(body, "Order modification failed"), ts};
}

ApiResponse<json> IIFLBroker::cancel_order(const BrokerCredentials& creds,
                                              const std::string& order_id) {
    const int64_t ts = iifl_now_ts();
    auto& client = HttpClient::instance();

    const std::string url = std::string(base_url()) +
        "/interactive/orders?appOrderID=" + order_id;

    auto resp = client.del(url, "", auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (is_success(body)) {
        return {true, body, "", ts};
    }
    return {false, std::nullopt, extract_error(body, "Order cancellation failed"), ts};
}

ApiResponse<std::vector<BrokerOrderInfo>> IIFLBroker::get_orders(const BrokerCredentials& creds) {
    const int64_t ts = iifl_now_ts();
    auto& client = HttpClient::instance();

    auto resp = client.get(std::string(base_url()) + "/interactive/orders",
                            auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerOrderInfo> orders;

    if (is_success(body) && body.contains("result") && body["result"].is_array()) {
        for (const auto& o : body["result"]) {
            BrokerOrderInfo info;
            if (o.contains("AppOrderID")) {
                if (o["AppOrderID"].is_string())
                    info.order_id = o["AppOrderID"].get<std::string>();
                else
                    info.order_id = std::to_string(o["AppOrderID"].get<int64_t>());
            }
            info.symbol       = o.value("TradingSymbol", o.value("exchangeInstrumentID", ""));
            info.exchange     = o.value("ExchangeSegment", "");
            info.side         = o.value("OrderSide", "");
            info.order_type   = o.value("OrderType", "");
            info.product_type = o.value("ProductType", "");
            info.quantity     = o.value("OrderQuantity", 0.0);
            info.filled_qty   = o.value("OrderQuantityFilled", 0.0);
            info.price        = o.value("OrderPrice", 0.0);
            info.avg_price    = o.value("OrderAverageTradedPrice", 0.0);
            info.status       = o.value("OrderStatus", "");
            info.timestamp    = o.value("ExchangeTransactTime", "");
            orders.push_back(std::move(info));
        }
    }

    return {true, std::move(orders), "", ts};
}

ApiResponse<json> IIFLBroker::get_trade_book(const BrokerCredentials& creds) {
    const int64_t ts = iifl_now_ts();
    auto& client = HttpClient::instance();

    auto resp = client.get(std::string(base_url()) + "/interactive/orders/trades",
                            auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    return {true, resp.json_body(), "", ts};
}

// ============================================================================
// Portfolio
// ============================================================================

ApiResponse<std::vector<BrokerPosition>> IIFLBroker::get_positions(const BrokerCredentials& creds) {
    const int64_t ts = iifl_now_ts();
    auto& client = HttpClient::instance();

    auto resp = client.get(std::string(base_url()) + "/interactive/portfolio/positions?dayOrNet=NetWise",
                            auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerPosition> positions;

    if (is_success(body) && body.contains("result") && body["result"].is_object()) {
        const auto& result = body["result"];
        if (result.contains("positionList") && result["positionList"].is_array()) {
            for (const auto& p : result["positionList"]) {
                BrokerPosition pos;
                pos.symbol       = p.value("TradingSymbol", "");
                pos.exchange     = p.value("ExchangeSegment", "");
                pos.product_type = p.value("ProductType", "");
                pos.quantity     = p.value("Quantity", 0.0);
                pos.avg_price    = p.value("AveragePrice", 0.0);
                pos.ltp          = p.value("ltp", 0.0);
                pos.pnl          = p.value("RealizedMTM", 0.0);
                pos.day_pnl      = p.value("UnrealizedMTM", 0.0);
                positions.push_back(std::move(pos));
            }
        }
    }

    return {true, std::move(positions), "", ts};
}

ApiResponse<std::vector<BrokerHolding>> IIFLBroker::get_holdings(const BrokerCredentials& creds) {
    const int64_t ts = iifl_now_ts();
    auto& client = HttpClient::instance();

    auto resp = client.get(std::string(base_url()) + "/interactive/portfolio/holdings",
                            auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerHolding> holdings;

    if (is_success(body) && body.contains("result") && body["result"].is_object()) {
        const auto& result = body["result"];
        if (result.contains("holdingsList") && result["holdingsList"].is_array()) {
            for (const auto& h : result["holdingsList"]) {
                BrokerHolding hld;
                hld.symbol     = h.value("TradingSymbol", "");
                hld.exchange   = h.value("ExchangeSegment", "NSE");
                hld.quantity   = h.value("Quantity", 0.0);
                hld.avg_price  = h.value("AveragePrice", 0.0);
                hld.ltp        = h.value("LastTradedPrice", 0.0);
                hld.pnl        = h.value("RealizedPL", 0.0);
                holdings.push_back(std::move(hld));
            }
        }
    }

    return {true, std::move(holdings), "", ts};
}

ApiResponse<BrokerFunds> IIFLBroker::get_funds(const BrokerCredentials& creds) {
    const int64_t ts = iifl_now_ts();
    auto& client = HttpClient::instance();

    auto resp = client.get(std::string(base_url()) + "/interactive/user/balance",
                            auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    BrokerFunds funds;

    if (is_success(body) && body.contains("result") && body["result"].is_object()) {
        const auto& result = body["result"];
        // IIFL funds: BalanceList[0].limitObject.RMSSubLimits.*
        if (result.contains("BalanceList") && result["BalanceList"].is_array() &&
            !result["BalanceList"].empty()) {
            const auto& bl = result["BalanceList"][0];
            if (bl.contains("limitObject") && bl["limitObject"].is_object()) {
                const auto& lo = bl["limitObject"];
                if (lo.contains("RMSSubLimits") && lo["RMSSubLimits"].is_object()) {
                    const auto& rms = lo["RMSSubLimits"];
                    funds.available_balance = rms.value("netMarginAvailable", 0.0);
                    funds.collateral       = rms.value("collateral", 0.0);
                    funds.used_margin      = rms.value("marginUtilized", 0.0);
                }
            }
        }
        funds.raw_data = body;
    }

    return {true, std::move(funds), "", ts};
}

// ============================================================================
// Market Data
// ============================================================================

ApiResponse<std::vector<BrokerQuote>> IIFLBroker::get_quotes(const BrokerCredentials& creds,
                                                                const std::vector<std::string>& symbols) {
    const int64_t ts = iifl_now_ts();
    auto& client = HttpClient::instance();

    // Market data uses a different base URL
    const std::string md_base = "https://ttblaze.iifl.com/apimarketdata";

    // Build instruments array: [{"exchangeSegment":1,"exchangeInstrumentID":"..."}]
    json instruments = json::array();
    for (const auto& sym : symbols) {
        std::string exchange = "NSE";
        std::string instrument_id = sym;
        const auto colon = sym.find(':');
        if (colon != std::string::npos) {
            exchange = sym.substr(0, colon);
            instrument_id = sym.substr(colon + 1);
        }

        instruments.push_back({
            {"exchangeSegment", iifl_segment_id(exchange)},
            {"exchangeInstrumentID", instrument_id}
        });
    }

    json payload = {
        {"instruments",    instruments},
        {"xtsMessageCode", 1502},
        {"publishFormat",  "JSON"}
    };

    // Market data auth uses same token
    auto resp = client.post_json(md_base + "/instruments/quotes",
                                  payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerQuote> quotes;

    if (is_success(body) && body.contains("result") && body["result"].is_object()) {
        const auto& result = body["result"];
        if (result.contains("listQuotes") && result["listQuotes"].is_array()) {
            for (const auto& item : result["listQuotes"]) {
                // listQuotes items may be JSON strings that need parsing
                json q_data;
                if (item.is_string()) {
                    q_data = json::parse(item.get<std::string>(), nullptr, false);
                    if (q_data.is_discarded()) continue;
                } else {
                    q_data = item;
                }

                BrokerQuote q;
                q.symbol = q_data.value("TradingSymbol", "");

                // Touchline data
                if (q_data.contains("Touchline") && q_data["Touchline"].is_object()) {
                    const auto& tl = q_data["Touchline"];
                    q.ltp    = tl.value("LastTradedPrice", 0.0);
                    q.high   = tl.value("High", 0.0);
                    q.low    = tl.value("Low", 0.0);
                    q.open   = tl.value("Open", 0.0);
                    q.close  = tl.value("Close", 0.0);
                    q.volume = tl.value("TotalTradedQuantity", 0.0);

                    if (tl.contains("BidInfo") && tl["BidInfo"].is_object())
                        q.bid = tl["BidInfo"].value("Price", 0.0);
                    if (tl.contains("AskInfo") && tl["AskInfo"].is_object())
                        q.ask = tl["AskInfo"].value("Price", 0.0);
                }

                quotes.push_back(std::move(q));
            }
        }
    }

    return {true, std::move(quotes), "", ts};
}

ApiResponse<std::vector<BrokerCandle>> IIFLBroker::get_history(const BrokerCredentials& creds,
                                                                  const std::string& symbol,
                                                                  const std::string& resolution,
                                                                  const std::string& from_date,
                                                                  const std::string& to_date) {
    const int64_t ts = iifl_now_ts();
    auto& client = HttpClient::instance();

    const std::string md_base = "https://ttblaze.iifl.com/apimarketdata";

    // Parse symbol format "EXCHANGE:INSTRUMENT_ID"
    std::string exchange = "NSE";
    std::string instrument_id = symbol;
    const auto colon = symbol.find(':');
    if (colon != std::string::npos) {
        exchange = symbol.substr(0, colon);
        instrument_id = symbol.substr(colon + 1);
    }

    // Map resolution to IIFL compression values
    std::string compression = "1D";  // default daily
    if (resolution == "1m" || resolution == "1")    compression = "1";
    else if (resolution == "5m" || resolution == "5")    compression = "5";
    else if (resolution == "10m" || resolution == "10")  compression = "10";
    else if (resolution == "15m" || resolution == "15")  compression = "15";
    else if (resolution == "30m" || resolution == "30")  compression = "30";
    else if (resolution == "1h" || resolution == "60")   compression = "60";
    else if (resolution == "D" || resolution == "day" || resolution == "1d") compression = "1D";

    const std::string url = md_base + "/instruments/ohlc"
        "?exchangeSegment=" + std::to_string(iifl_segment_id(exchange)) +
        "&exchangeInstrumentID=" + instrument_id +
        "&startTime=" + from_date +
        "&endTime=" + to_date +
        "&compressionValue=" + compression;

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerCandle> candles;

    if (is_success(body) && body.contains("result") && body["result"].is_object()) {
        const auto& result = body["result"];
        if (result.contains("dataReponse") && result["dataReponse"].is_string()) {
            // IIFL returns pipe-delimited CSV: rows separated by commas,
            // fields by pipes: timestamp|open|high|low|close|volume
            const std::string csv = result["dataReponse"].get<std::string>();
            std::istringstream rows_stream(csv);
            std::string row;
            while (std::getline(rows_stream, row, ',')) {
                if (row.empty()) continue;
                // Parse pipe-delimited fields
                std::istringstream fields_stream(row);
                std::string field;
                std::vector<std::string> fields;
                while (std::getline(fields_stream, field, '|')) {
                    fields.push_back(field);
                }
                if (fields.size() >= 6) {
                    BrokerCandle candle;
                    try {
                        candle.timestamp = std::stoll(fields[0]);
                        candle.open      = std::stod(fields[1]);
                        candle.high      = std::stod(fields[2]);
                        candle.low       = std::stod(fields[3]);
                        candle.close     = std::stod(fields[4]);
                        candle.volume    = std::stod(fields[5]);
                        candles.push_back(candle);
                    } catch (...) {
                        // Skip malformed rows
                    }
                }
            }
        }
    }

    return {true, std::move(candles), "", ts};
}

} // namespace fincept::trading
