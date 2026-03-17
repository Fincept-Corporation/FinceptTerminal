#include "aliceblue_broker.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace fincept::trading {

using namespace fincept::http;

// ============================================================================
// Helpers
// ============================================================================

static int64_t aliceblue_now_ts() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

// ============================================================================
// Auth headers — AliceBlue uses "Bearer {api_secret} {session_id}"
// ============================================================================

std::map<std::string, std::string> AliceBlueBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Authorization", "Bearer " + creds.api_secret + " " + creds.access_token},
        {"Content-Type",  "application/json"},
        {"Accept",        "application/json"}
    };
}

// ============================================================================
// SHA256 checksum generation
// ============================================================================

std::string AliceBlueBroker::generate_checksum(const std::string& user_id,
                                                 const std::string& api_secret,
                                                 const std::string& enc_key) {
    const std::string input = user_id + api_secret + enc_key;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);

    std::ostringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
    return ss.str();
}

// ============================================================================
// Mapping helpers
// ============================================================================

const char* AliceBlueBroker::ab_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "MKT";
        case OrderType::Limit:         return "L";
        case OrderType::StopLoss:      return "SL-M";
        case OrderType::StopLossLimit: return "SL";
        default:                       return "MKT";
    }
}

double AliceBlueBroker::parse_double(const json& val) {
    if (val.is_number()) return val.get<double>();
    if (val.is_string()) {
        try { return std::stod(val.get<std::string>()); }
        catch (...) { return 0.0; }
    }
    return 0.0;
}

int64_t AliceBlueBroker::parse_int(const json& val) {
    if (val.is_number_integer()) return val.get<int64_t>();
    if (val.is_string()) {
        try { return std::stoll(val.get<std::string>()); }
        catch (...) { return 0; }
    }
    return 0;
}

// ============================================================================
// Token Exchange — SHA256 checksum session
// ============================================================================

TokenExchangeResponse AliceBlueBroker::exchange_token(const std::string& api_key,
                                                        const std::string& api_secret,
                                                        const std::string& auth_code) {
    // api_key = user_id, api_secret = api_secret, auth_code = enc_key
    auto& client = HttpClient::instance();

    const std::string checksum = generate_checksum(api_key, api_secret, auth_code);

    json payload = {
        {"userId",   api_key},
        {"userData", checksum},
        {"source",   "WEB"}
    };

    Headers headers = {
        {"Content-Type", "application/json"},
        {"Accept",       "application/json"}
    };

    auto resp = client.post_json(std::string(base_url()) + "/customer/getUserSID",
                                  payload, headers);

    if (!resp.success) {
        return {false, "", "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();
    if (body.is_null()) {
        return {false, "", "", "Invalid JSON response"};
    }

    // Check for sessionID
    if (body.contains("sessionID") && body["sessionID"].is_string()) {
        return {true, body["sessionID"].get<std::string>(), api_key, ""};
    }

    // Check for error
    if (body.contains("emsg") && body["emsg"].is_string()) {
        return {false, "", "", body["emsg"].get<std::string>()};
    }

    if (body.value("stat", "") == "Not ok") {
        const std::string err = body.value("emsg", "Authentication failed");
        return {false, "", "", err};
    }

    return {false, "", "", "Failed to get session ID"};
}

// ============================================================================
// Validate Session
// ============================================================================

ApiResponse<bool> AliceBlueBroker::validate_session(const BrokerCredentials& creds) {
    const int64_t ts = aliceblue_now_ts();
    auto& client = HttpClient::instance();

    auto resp = client.get(std::string(base_url()) + "/limits/getRmsLimits",
                            auth_headers(creds));

    if (!resp.success) {
        return {false, false, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();

    // Response is an array; check first element's stat field
    bool valid = false;
    if (body.is_array() && !body.empty()) {
        const auto& first = body[0];
        valid = first.value("stat", "") != "Not_Ok";
    }

    return {valid, valid, valid ? "" : "Session validation failed", ts};
}

// ============================================================================
// Orders
// ============================================================================

OrderPlaceResponse AliceBlueBroker::place_order(const BrokerCredentials& creds,
                                                  const UnifiedOrder& order) {
    auto& client = HttpClient::instance();

    // AliceBlue expects order as a JSON array
    json order_obj = {
        {"complexty",       "regular"},
        {"discqty",         "0"},
        {"exch",            order.exchange},
        {"pCode",           order.product_type == ProductType::Intraday ? "MIS" : "CNC"},
        {"prctyp",          ab_order_type(order.order_type)},
        {"price",           std::to_string(order.price)},
        {"qty",             std::to_string(order.quantity)},
        {"ret",             "DAY"},
        {"symbol_id",       order.symbol},
        {"trading_symbol",  order.symbol},
        {"transtype",       order.side == OrderSide::Buy ? "BUY" : "SELL"},
        {"trigPrice",       std::to_string(order.stop_price)},
        {"orderTag",        "fincept"}
    };

    json payload = json::array({order_obj});

    auto resp = client.post_json(std::string(base_url()) + "/placeOrder/executePlaceOrder",
                                  payload, auth_headers(creds));

    if (!resp.success) {
        return {false, "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();

    // Response is an array
    if (body.is_array() && !body.empty()) {
        const auto& first = body[0];
        if (first.value("stat", "") == "Ok") {
            const std::string oid = first.value("NOrdNo", "");
            return {true, oid, ""};
        }
        return {false, "", first.value("emsg", "Order placement failed")};
    }

    return {false, "", "Order placement failed"};
}

ApiResponse<json> AliceBlueBroker::modify_order(const BrokerCredentials& creds,
                                                  const std::string& order_id,
                                                  const json& modifications) {
    const int64_t ts = aliceblue_now_ts();
    auto& client = HttpClient::instance();

    json payload = {
        {"nestOrderNumber", order_id},
        {"exch",            modifications.value("exchange", "NSE")},
        {"trading_symbol",  modifications.value("trading_symbol", "")},
        {"transtype",       modifications.value("transaction_type", "BUY")},
        {"pCode",           modifications.value("product", "CNC")},
        {"prctyp",          modifications.value("order_type", "MKT")},
        {"price",           modifications.value("price", 0.0)},
        {"qty",             modifications.value("quantity", 0)},
        {"trigPrice",       std::to_string(modifications.value("trigger_price", 0.0))},
        {"filledQuantity",  0},
        {"discqty",         0}
    };

    auto resp = client.post_json(std::string(base_url()) + "/placeOrder/modifyOrder",
                                  payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (body.value("stat", "") == "Ok") {
        return {true, body, "", ts};
    }
    return {false, std::nullopt, body.value("emsg", "Order modification failed"), ts};
}

ApiResponse<json> AliceBlueBroker::cancel_order(const BrokerCredentials& creds,
                                                  const std::string& order_id) {
    const int64_t ts = aliceblue_now_ts();
    auto& client = HttpClient::instance();

    json payload = {
        {"exch",            "NSE"},
        {"nestOrderNumber", order_id},
        {"trading_symbol",  ""}
    };

    auto resp = client.post_json(std::string(base_url()) + "/placeOrder/cancelOrder",
                                  payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (body.value("stat", "") == "Ok") {
        return {true, body, "", ts};
    }
    return {false, std::nullopt, body.value("emsg", "Order cancellation failed"), ts};
}

ApiResponse<std::vector<BrokerOrderInfo>> AliceBlueBroker::get_orders(const BrokerCredentials& creds) {
    const int64_t ts = aliceblue_now_ts();
    auto& client = HttpClient::instance();

    auto resp = client.get(std::string(base_url()) + "/placeOrder/fetchOrderBook",
                            auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerOrderInfo> orders;

    if (body.is_array()) {
        // Check if first item is error
        if (!body.empty() && body[0].value("stat", "") == "Not_Ok") {
            return {true, std::move(orders), "", ts};  // empty list, not an error
        }

        for (const auto& o : body) {
            BrokerOrderInfo info;
            info.order_id   = o.value("Nstordno", o.value("nestOrderNumber", ""));
            info.symbol     = o.value("Trsym", o.value("trading_symbol", ""));
            info.exchange   = o.value("Exchange", o.value("exch", ""));
            info.side       = o.value("Trantype", o.value("transtype", ""));
            info.order_type = o.value("Prctype", o.value("prctyp", ""));
            info.status     = o.value("Status", "");
            info.quantity   = static_cast<int>(parse_int(o.value("Qty", json(0))));
            info.filled_qty = static_cast<int>(parse_int(o.value("Fillshares", json(0))));
            info.price      = parse_double(o.value("Prc", json(0.0)));
            info.avg_price  = parse_double(o.value("Avgprc", json(0.0)));
            orders.push_back(std::move(info));
        }
    } else if (body.value("stat", "") == "Not_Ok") {
        return {true, std::move(orders), "", ts};
    }

    return {true, std::move(orders), "", ts};
}

ApiResponse<json> AliceBlueBroker::get_trade_book(const BrokerCredentials& creds) {
    const int64_t ts = aliceblue_now_ts();
    auto& client = HttpClient::instance();

    auto resp = client.get(std::string(base_url()) + "/placeOrder/fetchTradeBook",
                            auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();

    // Filter out "Not_Ok" responses
    if (body.is_array() && !body.empty()) {
        if (body[0].value("stat", "") == "Not_Ok") {
            return {true, json::array(), "", ts};
        }
    }

    return {true, body, "", ts};
}

// ============================================================================
// Portfolio
// ============================================================================

ApiResponse<std::vector<BrokerPosition>> AliceBlueBroker::get_positions(const BrokerCredentials& creds) {
    const int64_t ts = aliceblue_now_ts();
    auto& client = HttpClient::instance();

    json payload = {{"ret", "NET"}};
    auto resp = client.post_json(std::string(base_url()) + "/positionAndHoldings/positionBook",
                                  payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerPosition> positions;

    if (body.is_array()) {
        if (!body.empty() && body[0].value("stat", "") == "Not_Ok") {
            return {true, std::move(positions), "", ts};
        }

        for (const auto& p : body) {
            BrokerPosition pos;
            pos.symbol    = p.value("Tsym", p.value("trading_symbol", ""));
            pos.exchange  = p.value("Exchange", p.value("exch", ""));
            pos.quantity  = static_cast<int>(parse_int(p.value("Netqty", json(0))));
            pos.avg_price = parse_double(p.value("NetBuyavgprc", json(0.0)));
            pos.pnl       = parse_double(p.value("Rpnl", json(0.0)));
            positions.push_back(std::move(pos));
        }
    }

    return {true, std::move(positions), "", ts};
}

ApiResponse<std::vector<BrokerHolding>> AliceBlueBroker::get_holdings(const BrokerCredentials& creds) {
    const int64_t ts = aliceblue_now_ts();
    auto& client = HttpClient::instance();

    auto resp = client.get(std::string(base_url()) + "/positionAndHoldings/holdings",
                            auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerHolding> holdings;

    if (body.is_array()) {
        if (!body.empty() && body[0].value("stat", "") == "Not_Ok") {
            return {true, std::move(holdings), "", ts};
        }

        for (const auto& h : body) {
            BrokerHolding hld;
            hld.symbol    = h.value("Tsym", h.value("trading_symbol", ""));
            hld.exchange  = h.value("ExchSeg1", h.value("exchange", "NSE"));
            hld.quantity  = static_cast<int>(parse_int(h.value("SellableQty", json(0))));
            hld.avg_price = parse_double(h.value("Bprc", json(0.0)));
            hld.ltp       = parse_double(h.value("LTP", json(0.0)));
            hld.pnl       = parse_double(h.value("Pnl", json(0.0)));
            holdings.push_back(std::move(hld));
        }
    }

    return {true, std::move(holdings), "", ts};
}

ApiResponse<BrokerFunds> AliceBlueBroker::get_funds(const BrokerCredentials& creds) {
    const int64_t ts = aliceblue_now_ts();
    auto& client = HttpClient::instance();

    auto resp = client.get(std::string(base_url()) + "/limits/getRmsLimits",
                            auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();

    if (body.is_array() && !body.empty()) {
        const auto& first = body[0];
        if (first.value("stat", "") == "Not_Ok") {
            return {false, std::nullopt, first.value("emsg", "Failed to fetch margins"), ts};
        }

        BrokerFunds funds;
        funds.available_balance = parse_double(first.value("net", json(0.0)));
        funds.collateral       = parse_double(first.value("collateralvalue", json(0.0)));
        funds.used_margin      = parse_double(first.value("cncMarginUsed", json(0.0)));
        return {true, std::move(funds), "", ts};
    }

    return {false, std::nullopt, "Failed to fetch margins", ts};
}

// ============================================================================
// Market Data
// ============================================================================

ApiResponse<std::vector<BrokerQuote>> AliceBlueBroker::get_quotes(const BrokerCredentials& creds,
                                                                    const std::vector<std::string>& symbols) {
    const int64_t ts = aliceblue_now_ts();
    auto& client = HttpClient::instance();

    std::vector<BrokerQuote> quotes;

    for (const auto& sym : symbols) {
        // Symbol format: "EXCHANGE:TOKEN" e.g. "NSE:26000"
        std::string exchange = "NSE";
        std::string token = sym;
        const auto colon = sym.find(':');
        if (colon != std::string::npos) {
            exchange = sym.substr(0, colon);
            token = sym.substr(colon + 1);
        }

        json payload = {
            {"exch",   exchange},
            {"symbol", token}
        };

        auto resp = client.post_json(std::string(base_url()) + "/ScripDetails/getScripQuoteDetails",
                                      payload, auth_headers(creds));
        if (!resp.success) continue;

        const auto body = resp.json_body();
        if (body.value("stat", "") == "Not_Ok") continue;

        BrokerQuote q;
        q.symbol   = body.value("symbol", token);
        q.ltp      = parse_double(body.value("ltp", json(0.0)));
        q.open     = parse_double(body.value("open", json(0.0)));
        q.high     = parse_double(body.value("high", json(0.0)));
        q.low      = parse_double(body.value("low", json(0.0)));
        q.close    = parse_double(body.value("close", json(0.0)));
        q.volume   = parse_int(body.value("volume", json(0)));

        q.change = q.ltp - q.close;
        quotes.push_back(std::move(q));
    }

    return {true, std::move(quotes), "", ts};
}

ApiResponse<std::vector<BrokerCandle>> AliceBlueBroker::get_history(const BrokerCredentials& creds,
                                                                      const std::string& symbol,
                                                                      const std::string& resolution,
                                                                      const std::string& from_date,
                                                                      const std::string& to_date) {
    const int64_t ts = aliceblue_now_ts();
    auto& client = HttpClient::instance();

    // Symbol format: "EXCHANGE:TOKEN"
    std::string exchange = "NSE";
    std::string token = symbol;
    const auto colon = symbol.find(':');
    if (colon != std::string::npos) {
        exchange = symbol.substr(0, colon);
        token = symbol.substr(colon + 1);
    }

    // Map resolution
    std::string ab_resolution = "D";
    if (resolution == "1m" || resolution == "1") ab_resolution = "1";
    else if (resolution == "D" || resolution == "day" || resolution == "1d") ab_resolution = "D";

    // Historical API uses different auth: "Bearer {user_id} {session_id}"
    Headers headers = {
        {"Authorization", "Bearer " + creds.user_id + " " + creds.access_token},
        {"Content-Type",  "application/json"}
    };

    json payload = {
        {"token",      token},
        {"exchange",   exchange},
        {"from",       from_date},
        {"to",         to_date},
        {"resolution", ab_resolution}
    };

    auto resp = client.post_json(std::string(base_url()) + "/chart/history",
                                  payload, headers);

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (body.value("stat", "") == "Not_Ok") {
        return {false, std::nullopt, body.value("emsg", "Failed to fetch historical data"), ts};
    }

    std::vector<BrokerCandle> candles;
    if (body.contains("result") && body["result"].is_array()) {
        for (const auto& c : body["result"]) {
            BrokerCandle candle;
            // time field may be string or int
            if (c.contains("time")) {
                candle.timestamp = parse_int(c["time"]);
            }
            candle.open   = c.value("open", 0.0);
            candle.high   = c.value("high", 0.0);
            candle.low    = c.value("low", 0.0);
            candle.close  = c.value("close", 0.0);
            candle.volume = c.value("volume", static_cast<int64_t>(0));
            candles.push_back(candle);
        }
    }

    return {true, std::move(candles), "", ts};
}

} // namespace fincept::trading
