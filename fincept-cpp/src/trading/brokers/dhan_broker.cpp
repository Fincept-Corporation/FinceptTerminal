#include "dhan_broker.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <chrono>

namespace fincept::trading {

using namespace fincept::http;

// ============================================================================
// Helpers
// ============================================================================

static int64_t dhan_now_ts() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

// ============================================================================
// Auth headers — Dhan uses access-token + client-id custom headers
// ============================================================================

std::map<std::string, std::string> DhanBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"access-token", creds.access_token},
        {"client-id",    creds.user_id},
        {"Content-Type", "application/json"},
        {"Accept",       "application/json"}
    };
}

// ============================================================================
// Common API call helper
// ============================================================================

DhanBroker::DhanApiResult DhanBroker::dhan_api_call(
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
        resp = client.del(url, "", headers);
    } else {
        resp = client.get(url, headers);
    }

    if (!resp.success) {
        return {false, json(), resp.error};
    }

    const auto body = resp.json_body();

    // Dhan returns errors with "errorType" or HTTP status
    if (resp.status_code >= 400) {
        const auto msg = body.value("message", body.value("errorType", "API request failed"));
        return {false, body, msg};
    }

    return {true, body, ""};
}

// ============================================================================
// Exchange/Product/OrderType mappings
// ============================================================================

const char* DhanBroker::map_exchange_to_dhan(const std::string& exchange) {
    if (exchange == "NSE")       return "NSE_EQ";
    if (exchange == "BSE")       return "BSE_EQ";
    if (exchange == "NFO")       return "NSE_FNO";
    if (exchange == "BFO")       return "BSE_FNO";
    if (exchange == "MCX")       return "MCX_COMM";
    if (exchange == "CDS")       return "NSE_CURRENCY";
    if (exchange == "BCD")       return "BSE_CURRENCY";
    if (exchange == "NSE_INDEX") return "IDX_I";
    if (exchange == "BSE_INDEX") return "IDX_I";
    return exchange.c_str();
}

const char* DhanBroker::map_product_to_dhan(ProductType p) {
    switch (p) {
        case ProductType::Delivery:      return "CNC";
        case ProductType::Intraday:      return "INTRADAY";
        case ProductType::Margin:        return "MARGIN";
        case ProductType::CoverOrder:    return "INTRADAY";
        case ProductType::BracketOrder:  return "INTRADAY";
    }
    return "INTRADAY";
}

const char* DhanBroker::map_order_type_to_dhan(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "MARKET";
        case OrderType::Limit:         return "LIMIT";
        case OrderType::StopLoss:      return "STOP_LOSS";
        case OrderType::StopLossLimit: return "STOP_LOSS";
    }
    return "MARKET";
}

// ============================================================================
// Token Exchange — Dhan consent-based OAuth
// POST {auth_base}/app/consumeApp-consent?tokenId={token_id}
// ============================================================================

TokenExchangeResponse DhanBroker::exchange_token(const std::string& api_key,
                                                    const std::string& api_secret,
                                                    const std::string& token_id) {
    LOG_INFO("DhanBroker", "Exchanging consent token");

    Headers headers = {
        {"app_id",       api_key},
        {"app_secret",   api_secret},
        {"Content-Type", "application/json"}
    };

    const std::string url = std::string(auth_base_) +
        "/app/consumeApp-consent?tokenId=" + token_id;

    auto& client = HttpClient::instance();
    auto resp = client.post(url, "", headers);

    TokenExchangeResponse result;
    if (!resp.success) {
        result.error = resp.error;
        return result;
    }

    const auto body = resp.json_body();
    const auto access_token = body.value("accessToken", "");
    if (!access_token.empty()) {
        result.success = true;
        result.access_token = access_token;
        result.user_id = body.value("dhanClientId", "");
    } else {
        result.error = "Failed to get access token";
    }
    return result;
}

// ============================================================================
// Validate Token — GET /v2/fundlimit
// ============================================================================

ApiResponse<bool> DhanBroker::validate_token(const BrokerCredentials& creds) {
    LOG_INFO("DhanBroker", "Validating access token");

    const int64_t ts = dhan_now_ts();
    const auto res = dhan_api_call(creds, "/v2/fundlimit", "GET");

    if (res.success) {
        const bool has_error = res.body.contains("errorType") ||
            res.body.value("status", "") == "error";
        if (!has_error) {
            return {true, true, "", ts};
        }
    }
    return {false, false, "Invalid or expired token", ts};
}

// ============================================================================
// Place Order — POST /v2/orders
// ============================================================================

OrderPlaceResponse DhanBroker::place_order(const BrokerCredentials& creds,
                                             const UnifiedOrder& order) {
    LOG_INFO("DhanBroker", "Placing order: %s", order.symbol.c_str());

    json payload = {
        {"dhanClientId",   creds.user_id},
        {"transactionType", order.side == OrderSide::Buy ? "BUY" : "SELL"},
        {"exchangeSegment", map_exchange_to_dhan(order.exchange)},
        {"productType",    map_product_to_dhan(order.product_type)},
        {"orderType",      map_order_type_to_dhan(order.order_type)},
        {"validity",       "DAY"},
        {"securityId",     order.symbol},
        {"quantity",       static_cast<int>(order.quantity)}
    };

    if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit) {
        payload["price"] = order.price;
    }
    if (order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit) {
        payload["triggerPrice"] = order.stop_price;
    }

    const auto res = dhan_api_call(creds, "/v2/orders", "POST", payload);

    OrderPlaceResponse result;
    if (!res.success) { result.error = res.error; return result; }

    const auto order_id = res.body.value("orderId", "");
    if (!order_id.empty()) {
        result.success = true;
        result.order_id = order_id;
    } else {
        result.error = res.body.value("message", "Order placement failed");
    }
    return result;
}

// ============================================================================
// Modify Order — PUT /v2/orders/{order_id}
// ============================================================================

ApiResponse<json> DhanBroker::modify_order(const BrokerCredentials& creds,
                                             const std::string& order_id,
                                             const json& modifications) {
    LOG_INFO("DhanBroker", "Modifying order: %s", order_id.c_str());

    const int64_t ts = dhan_now_ts();

    json payload = {
        {"dhanClientId", creds.user_id},
        {"orderId",      order_id},
        {"legName",      "ENTRY_LEG"},
        {"validity",     "DAY"}
    };

    // Merge modifications
    if (modifications.contains("order_type"))
        payload["orderType"] = modifications["order_type"];
    if (modifications.contains("quantity"))
        payload["quantity"] = modifications["quantity"];
    if (modifications.contains("price"))
        payload["price"] = modifications["price"];
    if (modifications.contains("trigger_price"))
        payload["triggerPrice"] = modifications["trigger_price"];

    const std::string endpoint = "/v2/orders/" + order_id;
    const auto res = dhan_api_call(creds, endpoint, "PUT", payload);

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    const auto result_id = res.body.value("orderId", "");
    if (!result_id.empty())
        return {true, json{{"order_id", result_id}}, "", ts};
    return {false, std::nullopt, res.body.value("message", "Order modification failed"), ts};
}

// ============================================================================
// Cancel Order — DELETE /v2/orders/{order_id}
// ============================================================================

ApiResponse<json> DhanBroker::cancel_order(const BrokerCredentials& creds,
                                             const std::string& order_id) {
    LOG_INFO("DhanBroker", "Cancelling order: %s", order_id.c_str());

    const int64_t ts = dhan_now_ts();
    const std::string endpoint = "/v2/orders/" + order_id;
    const auto res = dhan_api_call(creds, endpoint, "DELETE");

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    return {true, json{{"order_id", order_id}}, "", ts};
}

// ============================================================================
// Get Orders — GET /v2/orders
// ============================================================================

ApiResponse<std::vector<BrokerOrderInfo>> DhanBroker::get_orders(const BrokerCredentials& creds) {
    LOG_INFO("DhanBroker", "Fetching order book");

    const int64_t ts = dhan_now_ts();
    const auto res = dhan_api_call(creds, "/v2/orders", "GET");

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    std::vector<BrokerOrderInfo> orders;
    const auto& data = res.body.is_array() ? res.body : json::array();
    for (const auto& o : data) {
        BrokerOrderInfo info;
        info.order_id      = o.value("orderId", "");
        info.symbol        = o.value("securityId", o.value("tradingSymbol", ""));
        info.exchange      = o.value("exchangeSegment", "");
        info.side          = o.value("transactionType", "");
        info.order_type    = o.value("orderType", "");
        info.product_type  = o.value("productType", "");
        info.quantity      = o.value("quantity", 0.0);
        info.price         = o.value("price", 0.0);
        info.trigger_price = o.value("triggerPrice", 0.0);
        info.filled_qty    = o.value("filledQty", 0.0);
        info.avg_price     = o.value("averageTradedPrice", 0.0);
        info.status        = o.value("orderStatus", "");
        info.timestamp     = o.value("updateTime", "");
        info.message       = o.value("omsErrorDescription", "");
        orders.push_back(info);
    }
    return {true, orders, "", ts};
}

// ============================================================================
// Get Trade Book — GET /v2/trades
// ============================================================================

ApiResponse<json> DhanBroker::get_trade_book(const BrokerCredentials& creds) {
    LOG_INFO("DhanBroker", "Fetching trade book");

    const int64_t ts = dhan_now_ts();
    const auto res = dhan_api_call(creds, "/v2/trades", "GET");

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    return {true, res.body, "", ts};
}

// ============================================================================
// Positions — GET /v2/positions
// ============================================================================

ApiResponse<std::vector<BrokerPosition>> DhanBroker::get_positions(const BrokerCredentials& creds) {
    LOG_INFO("DhanBroker", "Fetching positions");

    const int64_t ts = dhan_now_ts();
    const auto res = dhan_api_call(creds, "/v2/positions", "GET");

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    std::vector<BrokerPosition> positions;
    const auto& data = res.body.is_array() ? res.body : json::array();
    for (const auto& p : data) {
        BrokerPosition pos;
        pos.symbol       = p.value("securityId", p.value("tradingSymbol", ""));
        pos.exchange     = p.value("exchangeSegment", "");
        pos.product_type = p.value("productType", "");
        pos.quantity     = p.value("netQty", 0.0);
        pos.avg_price    = p.value("costPrice", 0.0);
        pos.ltp          = p.value("currentPrice", 0.0);
        pos.pnl          = p.value("realizedProfit", 0.0) + p.value("unrealizedProfit", 0.0);
        pos.day_pnl      = p.value("dayBuyValue", 0.0) - p.value("daySellValue", 0.0);
        pos.side         = pos.quantity >= 0 ? "buy" : "sell";
        positions.push_back(pos);
    }
    return {true, positions, "", ts};
}

// ============================================================================
// Holdings — GET /v2/holdings
// ============================================================================

ApiResponse<std::vector<BrokerHolding>> DhanBroker::get_holdings(const BrokerCredentials& creds) {
    LOG_INFO("DhanBroker", "Fetching holdings");

    const int64_t ts = dhan_now_ts();
    const auto res = dhan_api_call(creds, "/v2/holdings", "GET");

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    std::vector<BrokerHolding> holdings;
    const auto& data = res.body.is_array() ? res.body : json::array();
    for (const auto& h : data) {
        BrokerHolding hold;
        hold.symbol         = h.value("securityId", h.value("tradingSymbol", ""));
        hold.exchange       = h.value("exchange", "");
        hold.quantity       = h.value("totalQty", h.value("quantity", 0.0));
        hold.avg_price      = h.value("avgCostPrice", 0.0);
        hold.ltp            = h.value("currentPrice", 0.0);
        hold.invested_value = hold.quantity * hold.avg_price;
        hold.current_value  = hold.quantity * hold.ltp;
        hold.pnl            = hold.current_value - hold.invested_value;
        hold.pnl_pct        = hold.invested_value > 0 ? (hold.pnl / hold.invested_value) * 100.0 : 0.0;
        holdings.push_back(hold);
    }
    return {true, holdings, "", ts};
}

// ============================================================================
// Funds — GET /v2/fundlimit
// ============================================================================

ApiResponse<BrokerFunds> DhanBroker::get_funds(const BrokerCredentials& creds) {
    LOG_INFO("DhanBroker", "Fetching funds");

    const int64_t ts = dhan_now_ts();
    const auto res = dhan_api_call(creds, "/v2/fundlimit", "GET");

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    BrokerFunds funds;

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

    funds.available_balance = safe_double(res.body, "availabelBalance");  // Dhan typo in API
    if (funds.available_balance == 0.0) {
        funds.available_balance = safe_double(res.body, "availableBalance");
    }
    funds.used_margin   = safe_double(res.body, "utilizedAmount");
    funds.total_balance = safe_double(res.body, "sodLimit");
    funds.collateral    = safe_double(res.body, "collateralAmount");
    funds.raw_data      = res.body;
    return {true, funds, "", ts};
}

// ============================================================================
// Quotes — POST /v2/marketfeed/quote
// Dhan expects: { "NSE_EQ": [security_id_1, ...], "BSE_EQ": [...] }
// Input format: "EXCHANGE|security_id" per instrument
// ============================================================================

ApiResponse<std::vector<BrokerQuote>> DhanBroker::get_quotes(
        const BrokerCredentials& creds, const std::vector<std::string>& instruments) {
    LOG_INFO("DhanBroker", "Fetching quotes for %zu instruments", instruments.size());

    const int64_t ts = dhan_now_ts();

    // Group instruments by exchange segment
    json exchange_groups = json::object();
    for (const auto& key : instruments) {
        const auto pipe = key.find('|');
        if (pipe == std::string::npos) continue;
        const std::string exchange = key.substr(0, pipe);
        const std::string sec_id_str = key.substr(pipe + 1);
        try {
            const int64_t sec_id = std::stoll(sec_id_str);
            if (!exchange_groups.contains(exchange)) {
                exchange_groups[exchange] = json::array();
            }
            exchange_groups[exchange].push_back(sec_id);
        } catch (...) {
            continue;
        }
    }

    const auto res = dhan_api_call(creds, "/v2/marketfeed/quote", "POST", exchange_groups);

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    // Parse response — Dhan returns nested structure by exchange
    std::vector<BrokerQuote> quotes;
    const auto& data = res.body.value("data", res.body);
    for (auto& [exchange, sec_data] : data.items()) {
        if (!sec_data.is_object()) continue;
        for (auto& [sec_id, qdata] : sec_data.items()) {
            BrokerQuote q;
            q.symbol     = exchange + "|" + sec_id;
            q.ltp        = qdata.value("last_price", qdata.value("LTP", 0.0));
            q.open       = qdata.value("open", 0.0);
            q.high       = qdata.value("high", 0.0);
            q.low        = qdata.value("low", 0.0);
            q.close      = qdata.value("close", 0.0);
            q.volume     = qdata.value("volume", 0.0);
            q.change     = q.ltp - q.close;
            q.change_pct = q.close > 0 ? (q.change / q.close) * 100.0 : 0.0;
            q.timestamp  = ts;
            quotes.push_back(q);
        }
    }
    return {true, quotes, "", ts};
}

// ============================================================================
// History — POST /v2/charts/historical or /v2/charts/intraday
// ============================================================================

ApiResponse<std::vector<BrokerCandle>> DhanBroker::get_history(
        const BrokerCredentials& creds,
        const std::string& security_id,
        const std::string& interval,
        const std::string& from_date,
        const std::string& to_date) {
    LOG_INFO("DhanBroker", "Fetching history for %s", security_id.c_str());

    const int64_t ts = dhan_now_ts();

    // Determine endpoint based on interval
    const bool is_daily = (interval == "D" || interval == "1D");
    const std::string endpoint = is_daily ? "/v2/charts/historical" : "/v2/charts/intraday";

    json payload = {
        {"securityId",      security_id},
        {"exchangeSegment", "NSE_EQ"},      // Default; caller should include in additional_data
        {"instrument",      "EQUITY"},
        {"fromDate",        from_date},
        {"toDate",          to_date},
        {"oi",              true}
    };

    if (!is_daily) {
        // Extract numeric interval: "5m" -> "5"
        std::string interval_val;
        for (char c : interval) {
            if (std::isdigit(c)) interval_val += c;
        }
        if (interval_val.empty()) interval_val = "5";
        payload["interval"] = interval_val;
    }

    if (payload["instrument"] == "EQUITY") {
        payload["expiryCode"] = 0;
    }

    const auto res = dhan_api_call(creds, endpoint, "POST", payload);

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    std::vector<BrokerCandle> candles;
    // Dhan returns candles in various formats — handle both array-of-arrays and objects
    const auto& data = res.body;
    const auto open_arr = data.value("open", json::array());
    const auto high_arr = data.value("high", json::array());
    const auto low_arr = data.value("low", json::array());
    const auto close_arr = data.value("close", json::array());
    const auto volume_arr = data.value("volume", json::array());
    const auto timestamp_arr = data.value("timestamp", data.value("start_Time", json::array()));

    if (open_arr.is_array() && !open_arr.empty()) {
        // Parallel arrays format
        const size_t count = open_arr.size();
        for (size_t i = 0; i < count; ++i) {
            BrokerCandle candle;
            candle.open   = i < open_arr.size() ? open_arr[i].get<double>() : 0.0;
            candle.high   = i < high_arr.size() ? high_arr[i].get<double>() : 0.0;
            candle.low    = i < low_arr.size() ? low_arr[i].get<double>() : 0.0;
            candle.close  = i < close_arr.size() ? close_arr[i].get<double>() : 0.0;
            candle.volume = i < volume_arr.size() ? volume_arr[i].get<double>() : 0.0;
            candle.timestamp = i < timestamp_arr.size() && timestamp_arr[i].is_number()
                ? timestamp_arr[i].get<int64_t>() : ts;
            candles.push_back(candle);
        }
    }
    return {true, candles, "", ts};
}

} // namespace fincept::trading
