#include "angelone_broker.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace fincept::trading {

using namespace fincept::http;
using json = nlohmann::json;

// ============================================================================
// Instrument token cache — lazily loaded from AngelOne's public master JSON
// Key: "EXCHANGE:SYMBOL" (e.g. "NSE:RELIANCE") → instrument token string
// ============================================================================
static std::unordered_map<std::string, std::string> s_token_cache;
static std::once_flag s_token_cache_flag;

static void ensure_token_cache() {
    std::call_once(s_token_cache_flag, []() {
        try {
            auto resp = http::HttpClient::instance().get(
                "https://margincalculator.angelbroking.com/OpenAPI_File/files/OpenAPIScripMaster.json");
            if (!resp.success || resp.body.empty()) {
                LOG_ERROR("AngelOne", "Failed to download instrument master: %s", resp.error.c_str());
                return;
            }
            auto instruments = json::parse(resp.body);
            for (const auto& inst : instruments) {
                std::string sym   = inst.value("symbol", inst.value("name", ""));
                std::string exch  = inst.value("exch_seg", inst.value("exchange", ""));
                std::string token = inst.value("token", inst.value("symboltoken", ""));
                if (!sym.empty() && !exch.empty() && !token.empty()) {
                    // Uppercase keys
                    for (auto& c : sym)  c = (char)toupper((unsigned char)c);
                    for (auto& c : exch) c = (char)toupper((unsigned char)c);
                    s_token_cache[exch + ":" + sym] = token;
                }
            }
            LOG_INFO("AngelOne", "Loaded %zu instrument tokens", s_token_cache.size());
        } catch (const std::exception& e) {
            LOG_ERROR("AngelOne", "Instrument master parse error: %s", e.what());
        }
    });
}

// Resolve "EXCHANGE:SYMBOL" or "EXCHANGE:TOKEN" → "EXCHANGE:TOKEN"
// If the part after colon is already numeric, pass through unchanged.
static std::string resolve_angel_token(const std::string& instrument) {
    ensure_token_cache();
    const auto colon = instrument.find(':');
    if (colon == std::string::npos) return instrument;
    const std::string part = instrument.substr(colon + 1);
    // Already a numeric token
    if (!part.empty() && std::all_of(part.begin(), part.end(), ::isdigit))
        return instrument;
    // Look up symbol → token
    std::string key = instrument;
    for (auto& c : key) c = (char)toupper((unsigned char)c);
    auto it = s_token_cache.find(key);
    if (it != s_token_cache.end())
        return instrument.substr(0, colon + 1) + it->second;
    LOG_ERROR("AngelOne", "Token not found for %s", instrument.c_str());
    return instrument;  // return as-is, API will reject it
}

static int64_t angelone_now_ts() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

// ============================================================================
// Auth headers — AngelOne uses Bearer token + many custom headers
// ============================================================================

std::map<std::string, std::string> AngelOneBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Authorization",    "Bearer " + creds.access_token},
        {"Content-Type",     "application/json"},
        {"Accept",           "application/json"},
        {"X-UserType",       "USER"},
        {"X-SourceID",       "WEB"},
        {"X-ClientLocalIP",  "127.0.0.1"},
        {"X-ClientPublicIP", "127.0.0.1"},
        {"X-MACAddress",     "00:00:00:00:00:00"},
        {"X-PrivateKey",     creds.api_key}
    };
}

// ============================================================================
// Common API call helper — handles all AngelOne-specific request/response patterns
// ============================================================================

AngelOneBroker::AngelApiResult AngelOneBroker::angel_api_call(
        const BrokerCredentials& creds,
        const std::string& endpoint,
        const std::string& method,
        const json& payload) const {
    const auto headers = auth_headers(creds);
    const std::string url = std::string(base_url()) + endpoint;

    auto& client = HttpClient::instance();
    HttpResponse resp;

    if (method == "POST") {
        resp = client.post_json(url, payload, headers);
    } else if (method == "PUT") {
        resp = client.put_json(url, payload, headers);
    } else if (method == "DELETE") {
        resp = client.del(url, payload.dump(), headers);
    } else {
        resp = client.get(url, headers);
    }

    if (!resp.success) {
        return {false, json(), resp.error};
    }

    const auto body = resp.json_body();

    // AngelOne uses HTTP 200 even for errors — check response body
    if (!resp.success || (resp.status_code >= 400)) {
        const auto msg = body.value("message", "API request failed");
        return {false, body, msg};
    }

    return {true, body, ""};
}

// ============================================================================
// Order type mappings — AngelOne uses string identifiers
// ============================================================================

const char* AngelOneBroker::angel_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "MARKET";
        case OrderType::Limit:         return "LIMIT";
        case OrderType::StopLoss:      return "STOPLOSS_MARKET";
        case OrderType::StopLossLimit: return "STOPLOSS_LIMIT";
    }
    return "MARKET";
}

const char* AngelOneBroker::angel_product(ProductType p) {
    switch (p) {
        case ProductType::Intraday:      return "INTRADAY";
        case ProductType::Delivery:      return "DELIVERY";
        case ProductType::Margin:        return "MARGIN";
        case ProductType::CoverOrder:    return "INTRADAY";
        case ProductType::BracketOrder:  return "BO";
    }
    return "INTRADAY";
}

const char* AngelOneBroker::angel_side(OrderSide s) {
    return s == OrderSide::Buy ? "BUY" : "SELL";
}

const char* AngelOneBroker::angel_variety(ProductType p) {
    switch (p) {
        case ProductType::BracketOrder: return "ROBO";
        default:                        return "NORMAL";
    }
}

const char* AngelOneBroker::angel_duration(const std::string& validity) {
    if (validity == "IOC") return "IOC";
    return "DAY";
}

const char* AngelOneBroker::angel_exchange(const std::string& exchange) {
    if (exchange == "BSE") return "BSE";
    if (exchange == "NFO") return "NFO";
    if (exchange == "BFO") return "BFO";
    if (exchange == "CDS") return "CDS";
    if (exchange == "MCX") return "MCX";
    return "NSE"; // default
}

// ============================================================================
// Token Exchange — AngelOne login with client_code + password + TOTP
// POST /rest/auth/angelbroking/user/v1/loginByPassword
// ============================================================================

TokenExchangeResponse AngelOneBroker::exchange_token(const std::string& api_key,
                                                       const std::string& password,
                                                       const std::string& totp) {
    LOG_INFO("AngelOneBroker", "Logging in with TOTP");

    // Build headers without auth token (login request)
    Headers headers = {
        {"Content-Type",     "application/json"},
        {"Accept",           "application/json"},
        {"X-UserType",       "USER"},
        {"X-SourceID",       "WEB"},
        {"X-ClientLocalIP",  "127.0.0.1"},
        {"X-ClientPublicIP", "127.0.0.1"},
        {"X-MACAddress",     "00:00:00:00:00:00"},
        {"X-PrivateKey",     api_key}
    };

    // Extract client_code from additional_data or use api_secret field
    // In the C++ flow: api_key = API key, api_secret (password param) = PIN, auth_code (totp param) = TOTP
    // But we need client_code — it's stored in the user_id field
    // For login, the frontend passes: api_key, client_code (as api_secret), password is separate
    // We'll use the convention: api_key = API key, password = "client_code:pin", totp = TOTP code/secret

    std::string client_code;
    std::string pin;
    const auto colon_pos = password.find(':');
    if (colon_pos != std::string::npos) {
        client_code = password.substr(0, colon_pos);
        pin = password.substr(colon_pos + 1);
    } else {
        // Fallback: assume password is just the PIN, client_code from additional context
        pin = password;
        client_code = password; // caller should provide client_code:pin
    }

    // TOTP: if it's a base32 secret, we'd generate the code
    // For now, pass through (TOTP generation requires additional library)
    // The frontend should pass the 6-digit TOTP code directly
    const std::string totp_code = totp;

    json payload = {
        {"clientcode", client_code},
        {"password", pin},
        {"totp", totp_code}
    };

    auto& client = HttpClient::instance();
    auto resp = client.post_json(
        std::string(base_url()) + "/rest/auth/angelbroking/user/v1/loginByPassword",
        payload, headers);

    TokenExchangeResponse result;
    if (!resp.success) {
        result.error = resp.error;
        return result;
    }

    const auto body = resp.json_body();
    const auto data = body.value("data", json::object());
    const auto jwt_token = data.value("jwtToken", "");

    if (!jwt_token.empty()) {
        result.success      = true;
        result.access_token = jwt_token;
        result.user_id      = client_code;
        json extra = {
            {"feed_token",    data.value("feedToken",    "")},
            {"refresh_token", data.value("refreshToken", "")}
        };
        result.additional_data = extra.dump();
    } else {
        result.error = body.value("message", "Authentication failed");
    }
    return result;
}

// ============================================================================
// Validate Token — GET /rest/secure/angelbroking/user/v1/getProfile
// ============================================================================

ApiResponse<bool> AngelOneBroker::validate_token(const BrokerCredentials& creds) {
    LOG_INFO("AngelOneBroker", "Validating access token");

    const auto res = angel_api_call(creds,
        "/rest/secure/angelbroking/user/v1/getProfile", "GET");

    const int64_t ts = angelone_now_ts();
    if (res.success) {
        // Check for error patterns in body
        const auto msg = res.body.value("message", "");
        if (msg.find("Invalid Token") != std::string::npos ||
            msg.find("Session expired") != std::string::npos) {
            return {false, false, msg, ts};
        }
        return {true, true, "", ts};
    }
    return {false, false, res.error, ts};
}

// ============================================================================
// Refresh Token — POST /rest/auth/angelbroking/jwt/v1/generateTokens
// ============================================================================

ApiResponse<json> AngelOneBroker::refresh_token(const BrokerCredentials& creds) {
    LOG_INFO("AngelOneBroker", "Refreshing session token");

    const int64_t ts = angelone_now_ts();
    json payload = {{"refreshToken", creds.refresh_token}};

    const auto res = angel_api_call(creds,
        "/rest/auth/angelbroking/jwt/v1/generateTokens", "POST", payload);

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    const auto data = res.body.value("data", json::object());
    const auto jwt = data.value("jwtToken", "");
    if (!jwt.empty()) {
        return {true, json{
            {"access_token", jwt},
            {"feed_token", data.value("feedToken", "")},
            {"refresh_token", data.value("refreshToken", "")}
        }, "", ts};
    }
    return {false, std::nullopt, "Token refresh failed", ts};
}

// ============================================================================
// Place Order — POST /rest/secure/angelbroking/order/v1/placeOrder
// ============================================================================

OrderPlaceResponse AngelOneBroker::place_order(const BrokerCredentials& creds,
                                                 const UnifiedOrder& order) {
    LOG_INFO("AngelOneBroker", "Placing order: %s", order.symbol.c_str());

    json payload = {
        {"variety",          angel_variety(order.product_type)},
        {"tradingsymbol",    order.symbol},
        {"symboltoken",      ""},  // Must be filled by caller or looked up
        {"transactiontype",  angel_side(order.side)},
        {"exchange",         angel_exchange(order.exchange)},
        {"ordertype",        angel_order_type(order.order_type)},
        {"producttype",      angel_product(order.product_type)},
        {"duration",         angel_duration(order.validity)},
        {"quantity",         std::to_string(static_cast<int>(order.quantity))}
    };

    if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit) {
        payload["price"] = std::to_string(order.price);
    } else {
        payload["price"] = "0";
    }
    if (order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit) {
        payload["triggerprice"] = std::to_string(order.stop_price);
    } else {
        payload["triggerprice"] = "0";
    }

    const auto res = angel_api_call(creds,
        "/rest/secure/angelbroking/order/v1/placeOrder", "POST", payload);

    OrderPlaceResponse result;
    if (!res.success) { result.error = res.error; return result; }

    // AngelOne returns status as bool or string "true"
    const auto status = res.body.value("status", false);
    const auto status_str = res.body.contains("status") && res.body["status"].is_string()
        ? res.body["status"].get<std::string>() : "";
    if (status || status_str == "true") {
        const auto data = res.body.value("data", json::object());
        result.success = true;
        result.order_id = data.value("orderid", "");
    } else {
        result.error = res.body.value("message", "Order placement failed");
    }
    return result;
}

// ============================================================================
// Modify Order — POST /rest/secure/angelbroking/order/v1/modifyOrder
// ============================================================================

ApiResponse<json> AngelOneBroker::modify_order(const BrokerCredentials& creds,
                                                 const std::string& order_id,
                                                 const json& modifications) {
    LOG_INFO("AngelOneBroker", "Modifying order: %s", order_id.c_str());

    const int64_t ts = angelone_now_ts();
    json payload = modifications;
    payload["orderid"] = order_id;

    const auto res = angel_api_call(creds,
        "/rest/secure/angelbroking/order/v1/modifyOrder", "POST", payload);

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    const auto status = res.body.value("status", false);
    const auto status_str = res.body.contains("status") && res.body["status"].is_string()
        ? res.body["status"].get<std::string>() : "";
    if (status || status_str == "true") {
        const auto data = res.body.value("data", json::object());
        return {true, json{{"order_id", data.value("orderid", "")}}, "", ts};
    }
    return {false, std::nullopt, res.body.value("message", "Order modification failed"), ts};
}

// ============================================================================
// Cancel Order — POST /rest/secure/angelbroking/order/v1/cancelOrder
// ============================================================================

ApiResponse<json> AngelOneBroker::cancel_order(const BrokerCredentials& creds,
                                                 const std::string& order_id) {
    LOG_INFO("AngelOneBroker", "Cancelling order: %s", order_id.c_str());

    const int64_t ts = angelone_now_ts();
    json payload = {
        {"variety", "NORMAL"},
        {"orderid", order_id}
    };

    const auto res = angel_api_call(creds,
        "/rest/secure/angelbroking/order/v1/cancelOrder", "POST", payload);

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    const auto status = res.body.value("status", false);
    const auto status_str = res.body.contains("status") && res.body["status"].is_string()
        ? res.body["status"].get<std::string>() : "";
    if (status || status_str == "true") {
        const auto data = res.body.value("data", json::object());
        return {true, json{{"order_id", data.value("orderid", order_id)}}, "", ts};
    }
    return {false, std::nullopt, res.body.value("message", "Order cancellation failed"), ts};
}

// ============================================================================
// Get Orders — GET /rest/secure/angelbroking/order/v1/getOrderBook
// ============================================================================

ApiResponse<std::vector<BrokerOrderInfo>> AngelOneBroker::get_orders(const BrokerCredentials& creds) {
    LOG_INFO("AngelOneBroker", "Fetching order book");

    const int64_t ts = angelone_now_ts();
    const auto res = angel_api_call(creds,
        "/rest/secure/angelbroking/order/v1/getOrderBook", "GET");

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    std::vector<BrokerOrderInfo> orders;
    const auto data = res.body.value("data", json::array());
    if (data.is_array()) {
        for (const auto& o : data) {
            BrokerOrderInfo info;
            info.order_id      = o.value("orderid", "");
            info.symbol        = o.value("tradingsymbol", "");
            info.exchange      = o.value("exchange", "");
            info.side          = o.value("transactiontype", "");
            info.order_type    = o.value("ordertype", "");
            info.product_type  = o.value("producttype", "");
            info.quantity      = o.value("quantity", 0.0);
            info.price         = o.value("price", 0.0);
            info.trigger_price = o.value("triggerprice", 0.0);
            info.filled_qty    = o.value("filledshares", 0.0);
            info.avg_price     = o.value("averageprice", 0.0);
            info.status        = o.value("orderstatus", "");
            info.timestamp     = o.value("updatetime", "");
            info.message       = o.value("text", "");
            orders.push_back(info);
        }
    }
    return {true, orders, "", ts};
}

// ============================================================================
// Get Trade Book — GET /rest/secure/angelbroking/order/v1/getTradeBook
// ============================================================================

ApiResponse<json> AngelOneBroker::get_trade_book(const BrokerCredentials& creds) {
    LOG_INFO("AngelOneBroker", "Fetching trade book");

    const int64_t ts = angelone_now_ts();
    const auto res = angel_api_call(creds,
        "/rest/secure/angelbroking/order/v1/getTradeBook", "GET");

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    return {true, res.body.value("data", json::array()), "", ts};
}

// ============================================================================
// Positions — GET /rest/secure/angelbroking/order/v1/getPosition
// ============================================================================

ApiResponse<std::vector<BrokerPosition>> AngelOneBroker::get_positions(const BrokerCredentials& creds) {
    LOG_INFO("AngelOneBroker", "Fetching positions");

    const int64_t ts = angelone_now_ts();
    const auto res = angel_api_call(creds,
        "/rest/secure/angelbroking/order/v1/getPosition", "GET");

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    std::vector<BrokerPosition> positions;
    const auto data = res.body.value("data", json::array());
    if (data.is_array()) {
        for (const auto& p : data) {
            BrokerPosition pos;
            pos.symbol       = p.value("tradingsymbol", "");
            pos.exchange     = p.value("exchange", "");
            pos.product_type = p.value("producttype", "");
            pos.quantity     = p.value("netqty", 0.0);
            pos.avg_price    = p.value("netprice", 0.0);
            pos.ltp          = p.value("ltp", 0.0);
            pos.pnl          = p.value("pnl", 0.0);
            pos.day_pnl      = p.value("unrealised", 0.0);
            pos.side         = pos.quantity >= 0 ? "buy" : "sell";
            positions.push_back(pos);
        }
    }
    return {true, positions, "", ts};
}

// ============================================================================
// Holdings — GET /rest/secure/angelbroking/portfolio/v1/getAllHolding
// ============================================================================

ApiResponse<std::vector<BrokerHolding>> AngelOneBroker::get_holdings(const BrokerCredentials& creds) {
    LOG_INFO("AngelOneBroker", "Fetching holdings");

    const int64_t ts = angelone_now_ts();
    const auto res = angel_api_call(creds,
        "/rest/secure/angelbroking/portfolio/v1/getAllHolding", "GET");

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    std::vector<BrokerHolding> holdings;
    const auto data = res.body.value("data", json());
    // AngelOne returns holdings either as array or as object with "holdings" key
    const auto arr = data.is_array() ? data : data.value("holdings", json::array());
    if (arr.is_array()) {
        for (const auto& h : arr) {
            BrokerHolding hold;
            hold.symbol         = h.value("tradingsymbol", "");
            hold.exchange       = h.value("exchange", "");
            hold.quantity       = h.value("quantity", 0.0);
            hold.avg_price      = h.value("averageprice", 0.0);
            hold.ltp            = h.value("ltp", 0.0);
            hold.invested_value = hold.quantity * hold.avg_price;
            hold.current_value  = hold.quantity * hold.ltp;
            hold.pnl            = hold.current_value - hold.invested_value;
            hold.pnl_pct        = hold.invested_value > 0 ? (hold.pnl / hold.invested_value) * 100.0 : 0.0;
            holdings.push_back(hold);
        }
    }
    return {true, holdings, "", ts};
}

// ============================================================================
// Funds (RMS) — GET /rest/secure/angelbroking/user/v1/getRMS
// ============================================================================

ApiResponse<BrokerFunds> AngelOneBroker::get_funds(const BrokerCredentials& creds) {
    LOG_INFO("AngelOneBroker", "Fetching funds/RMS");

    const int64_t ts = angelone_now_ts();
    const auto res = angel_api_call(creds,
        "/rest/secure/angelbroking/user/v1/getRMS", "GET");

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    const auto data = res.body.value("data", json::object());
    BrokerFunds funds;

    // Parse string values to double (AngelOne returns strings)
    auto safe_double = [](const json& j, const char* key) -> double {
        if (!j.contains(key)) return 0.0;
        const auto& v = j[key];
        if (v.is_number()) return v.get<double>();
        if (v.is_string()) {
            try { return std::stod(v.get<std::string>()); }
            catch (...) { return 0.0; }
        }
        return 0.0;
    };

    funds.available_balance = safe_double(data, "availablecash");
    funds.used_margin       = safe_double(data, "utiliseddebits");
    funds.total_balance     = safe_double(data, "net");
    funds.collateral        = safe_double(data, "collateral");
    funds.raw_data          = data;
    return {true, funds, "", ts};
}

// ============================================================================
// Quotes — POST /rest/secure/angelbroking/market/v1/quote/
// ============================================================================

ApiResponse<std::vector<BrokerQuote>> AngelOneBroker::get_quotes(
        const BrokerCredentials& creds, const std::vector<std::string>& tokens) {
    LOG_INFO("AngelOneBroker", "Fetching quotes for %zu tokens", tokens.size());

    const int64_t ts = angelone_now_ts();

    // AngelOne expects: { "mode": "FULL", "exchangeTokens": { "NSE": ["token1", "token2"] } }
    // The tokens vector should contain "EXCHANGE:TOKEN" pairs
    // Group tokens by exchange
    std::map<std::string, json> exchange_tokens;
    for (const auto& raw_token : tokens) {
        const std::string token = resolve_angel_token(raw_token);
        const auto colon = token.find(':');
        std::string exchange = "NSE";
        std::string symbol_token = token;
        if (colon != std::string::npos) {
            exchange = token.substr(0, colon);
            symbol_token = token.substr(colon + 1);
        }
        if (!exchange_tokens.count(exchange)) {
            exchange_tokens[exchange] = json::array();
        }
        exchange_tokens[exchange].push_back(symbol_token);
    }

    json exchange_tokens_json = json::object();
    for (const auto& [ex, toks] : exchange_tokens) {
        exchange_tokens_json[ex] = toks;
    }

    json payload = {
        {"mode", "FULL"},
        {"exchangeTokens", exchange_tokens_json}
    };

    const auto res = angel_api_call(creds,
        "/rest/secure/angelbroking/market/v1/quote/", "POST", payload);

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    // Check status field
    const auto status = res.body.value("status", false);
    const auto status_str = res.body.contains("status") && res.body["status"].is_string()
        ? res.body["status"].get<std::string>() : "";
    if (!status && status_str != "true") {
        return {false, std::nullopt, res.body.value("message", "Quote fetch failed"), ts};
    }

    std::vector<BrokerQuote> quotes;
    const auto data = res.body.value("data", json::object());
    // AngelOne returns: { "fetched": [...], "unfetched": [...] }
    const auto fetched = data.value("fetched", json::array());
    if (fetched.is_array()) {
        for (const auto& q : fetched) {
            BrokerQuote quote;
            quote.symbol     = q.value("tradingSymbol", q.value("tradingsymbol", ""));
            quote.ltp        = q.value("ltp", 0.0);
            quote.open       = q.value("open", 0.0);
            quote.high       = q.value("high", 0.0);
            quote.low        = q.value("low", 0.0);
            quote.close      = q.value("close", 0.0);
            quote.volume     = q.value("tradeVolume", q.value("volume", 0.0));
            quote.change     = q.value("netChange", 0.0);
            quote.change_pct = q.value("percentChange", 0.0);
            quote.bid        = q.value("bestBidPrice", 0.0);
            quote.ask        = q.value("bestAskPrice", 0.0);
            quote.timestamp  = q.value("exchFeedTime", static_cast<int64_t>(0));
            quotes.push_back(quote);
        }
    }
    return {true, quotes, "", ts};
}

// ============================================================================
// History — POST /rest/secure/angelbroking/historical/v1/getCandleData
// ============================================================================

ApiResponse<std::vector<BrokerCandle>> AngelOneBroker::get_history(
        const BrokerCredentials& creds,
        const std::string& symbol_token,
        const std::string& interval,
        const std::string& from_date,
        const std::string& to_date) {
    LOG_INFO("AngelOneBroker", "Fetching historical data for %s", symbol_token.c_str());

    const int64_t ts = angelone_now_ts();

    // Parse exchange from symbol_token if it contains "EXCHANGE:TOKEN"
    std::string exchange = "NSE";
    std::string token = symbol_token;
    const auto colon = symbol_token.find(':');
    if (colon != std::string::npos) {
        exchange = symbol_token.substr(0, colon);
        token = symbol_token.substr(colon + 1);
    }

    json payload = {
        {"exchange", exchange},
        {"symboltoken", token},
        {"interval", interval},
        {"fromdate", from_date},
        {"todate", to_date}
    };

    const auto res = angel_api_call(creds,
        "/rest/secure/angelbroking/historical/v1/getCandleData", "POST", payload);

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    const auto status = res.body.value("status", false);
    const auto status_str = res.body.contains("status") && res.body["status"].is_string()
        ? res.body["status"].get<std::string>() : "";
    if (!status && status_str != "true") {
        return {false, std::nullopt, res.body.value("message", "No historical data available"), ts};
    }

    std::vector<BrokerCandle> candles;
    const auto data = res.body.value("data", json::array());
    // AngelOne returns array of arrays: [[timestamp, open, high, low, close, volume], ...]
    if (data.is_array()) {
        for (const auto& c : data) {
            if (!c.is_array() || c.size() < 6) continue;
            BrokerCandle candle;
            if (c[0].is_number()) {
                candle.timestamp = c[0].get<int64_t>();
            } else {
                candle.timestamp = ts;
            }
            candle.open   = c[1].get<double>();
            candle.high   = c[2].get<double>();
            candle.low    = c[3].get<double>();
            candle.close  = c[4].get<double>();
            candle.volume = c[5].get<double>();
            candles.push_back(candle);
        }
    }
    return {true, candles, "", ts};
}

// ============================================================================
// Calculate Margin — POST /rest/secure/angelbroking/margin/v1/batch
// ============================================================================

ApiResponse<json> AngelOneBroker::calculate_margin(const BrokerCredentials& creds,
                                                     const json& orders) {
    LOG_INFO("AngelOneBroker", "Calculating margin");

    const int64_t ts = angelone_now_ts();
    json payload = {{"positions", orders}};

    const auto res = angel_api_call(creds,
        "/rest/secure/angelbroking/margin/v1/batch", "POST", payload);

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    return {true, res.body.value("data", json::object()), "", ts};
}

} // namespace fincept::trading
