#include "upstox_broker.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <chrono>
#include <sstream>

namespace fincept::trading {

using namespace fincept::http;

// ============================================================================
// Helpers
// ============================================================================

static int64_t upstox_now_ts() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

bool UpstoxBroker::is_success(const json& body) {
    return body.value("status", "") == "success";
}

std::string UpstoxBroker::extract_error(const json& body, const char* fallback) {
    // Upstox error format: { "errors": [{ "message": "..." }] }
    const auto& errors = body.value("errors", json::array());
    if (errors.is_array() && !errors.empty()) {
        const auto& first = errors[0];
        if (first.contains("message") && first["message"].is_string()) {
            return first["message"].get<std::string>();
        }
    }
    // Fallback to top-level message
    return body.value("message", fallback);
}

std::string UpstoxBroker::url_encode(const std::string& s) {
    std::ostringstream result;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result << c;
        } else {
            result << '%' << std::hex << std::uppercase
                   << ((c >> 4) & 0xF) << (c & 0xF);
        }
    }
    return result.str();
}

// ============================================================================
// Auth headers: "Authorization: Bearer access_token"
// ============================================================================

std::map<std::string, std::string> UpstoxBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Authorization", "Bearer " + creds.access_token},
        {"Content-Type",  "application/json"},
        {"Accept",        "application/json"}
    };
}

// ============================================================================
// Order type mappings
// ============================================================================

const char* UpstoxBroker::upstox_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "MARKET";
        case OrderType::Limit:         return "LIMIT";
        case OrderType::StopLoss:      return "SL";
        case OrderType::StopLossLimit: return "SL";
    }
    return "MARKET";
}

const char* UpstoxBroker::upstox_side(OrderSide s) {
    return s == OrderSide::Buy ? "BUY" : "SELL";
}

const char* UpstoxBroker::upstox_product(ProductType p) {
    switch (p) {
        case ProductType::Intraday:      return "I";
        case ProductType::Delivery:      return "D";
        case ProductType::Margin:        return "D";
        case ProductType::CoverOrder:    return "I";
        case ProductType::BracketOrder:  return "I";
    }
    return "I";
}

// ============================================================================
// Token Exchange — OAuth2 authorization_code grant
// POST /login/authorization/token (form-urlencoded)
// ============================================================================

TokenExchangeResponse UpstoxBroker::exchange_token(const std::string& api_key,
                                                      const std::string& api_secret,
                                                      const std::string& auth_code) {
    LOG_INFO("UpstoxBroker", "Exchanging authorization code");

    // Build form-urlencoded body
    // redirect_uri defaults to a standard value; caller can override via additional_data
    const std::string redirect_uri = "https://localhost";
    const std::string form_body =
        "code=" + url_encode(auth_code) +
        "&client_id=" + url_encode(api_key) +
        "&client_secret=" + url_encode(api_secret) +
        "&redirect_uri=" + url_encode(redirect_uri) +
        "&grant_type=authorization_code";

    Headers headers = {
        {"Content-Type", "application/x-www-form-urlencoded"},
        {"Accept",       "application/json"}
    };

    auto& client = HttpClient::instance();
    auto resp = client.post(std::string(base_url()) + "/login/authorization/token",
                            form_body, headers);

    TokenExchangeResponse result;
    if (!resp.success) {
        result.error = resp.error;
        return result;
    }

    const auto body = resp.json_body();
    const auto access_token = body.value("access_token", "");
    if (!access_token.empty()) {
        result.success = true;
        result.access_token = access_token;
        result.user_id = body.value("user_id", "");
    } else {
        result.error = extract_error(body, "Token exchange failed");
    }
    return result;
}

// ============================================================================
// Validate Token — GET /user/profile
// ============================================================================

ApiResponse<bool> UpstoxBroker::validate_token(const BrokerCredentials& creds) {
    LOG_INFO("UpstoxBroker", "Validating access token");

    const auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/user/profile", headers);

    const int64_t ts = upstox_now_ts();
    if (resp.success) {
        const auto body = resp.json_body();
        if (is_success(body)) {
            return {true, true, "", ts};
        }
    }
    return {false, false, "Token validation failed", ts};
}

// ============================================================================
// Place Order — POST /order/place
// ============================================================================

OrderPlaceResponse UpstoxBroker::place_order(const BrokerCredentials& creds,
                                               const UnifiedOrder& order) {
    LOG_INFO("UpstoxBroker", "Placing order: %s", order.symbol.c_str());

    const auto headers = auth_headers(creds);

    json payload = {
        {"instrument_token", order.symbol},
        {"quantity",         static_cast<int>(order.quantity)},
        {"transaction_type", upstox_side(order.side)},
        {"order_type",       upstox_order_type(order.order_type)},
        {"product",          upstox_product(order.product_type)},
        {"validity",         "DAY"},
        {"price",            order.price},
        {"trigger_price",    order.stop_price},
        {"disclosed_quantity", 0},
        {"is_amo",           false},
        {"tag",              "fincept"}
    };

    auto& client = HttpClient::instance();
    auto resp = client.post_json(std::string(base_url()) + "/order/place", payload, headers);

    OrderPlaceResponse result;
    if (!resp.success) { result.error = resp.error; return result; }

    const auto body = resp.json_body();
    if (is_success(body)) {
        const auto data = body.value("data", json::object());
        result.success = true;
        result.order_id = data.value("order_id", "");
    } else {
        result.error = extract_error(body, "Order placement failed");
    }
    return result;
}

// ============================================================================
// Modify Order — PUT /order/modify
// ============================================================================

ApiResponse<json> UpstoxBroker::modify_order(const BrokerCredentials& creds,
                                               const std::string& order_id,
                                               const json& modifications) {
    LOG_INFO("UpstoxBroker", "Modifying order: %s", order_id.c_str());

    const auto headers = auth_headers(creds);
    const int64_t ts = upstox_now_ts();

    json payload = modifications;
    payload["order_id"] = order_id;
    payload["validity"] = "DAY";

    auto& client = HttpClient::instance();
    auto resp = client.put_json(std::string(base_url()) + "/order/modify", payload, headers);

    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (is_success(body))
        return {true, json{{"order_id", order_id}}, "", ts};
    return {false, std::nullopt, extract_error(body, "Order modification failed"), ts};
}

// ============================================================================
// Cancel Order — DELETE /order/cancel?order_id={id}
// ============================================================================

ApiResponse<json> UpstoxBroker::cancel_order(const BrokerCredentials& creds,
                                               const std::string& order_id) {
    LOG_INFO("UpstoxBroker", "Cancelling order: %s", order_id.c_str());

    const auto headers = auth_headers(creds);
    const int64_t ts = upstox_now_ts();

    auto& client = HttpClient::instance();
    auto resp = client.del(std::string(base_url()) + "/order/cancel?order_id=" + order_id,
                           "", headers);

    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (is_success(body))
        return {true, json{{"order_id", order_id}}, "", ts};
    return {false, std::nullopt, extract_error(body, "Order cancellation failed"), ts};
}

// ============================================================================
// Get Orders — GET /order/retrieve-all
// ============================================================================

ApiResponse<std::vector<BrokerOrderInfo>> UpstoxBroker::get_orders(const BrokerCredentials& creds) {
    LOG_INFO("UpstoxBroker", "Fetching order book");

    const auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/order/retrieve-all", headers);

    const int64_t ts = upstox_now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (!is_success(body))
        return {false, std::nullopt, extract_error(body, "Failed to fetch orders"), ts};

    std::vector<BrokerOrderInfo> orders;
    const auto data = body.value("data", json::array());
    for (const auto& o : data) {
        BrokerOrderInfo info;
        info.order_id      = o.value("order_id", "");
        info.symbol        = o.value("instrument_token", o.value("trading_symbol", ""));
        info.exchange      = o.value("exchange", "");
        info.side          = o.value("transaction_type", "");
        info.order_type    = o.value("order_type", "");
        info.product_type  = o.value("product", "");
        info.quantity      = o.value("quantity", 0.0);
        info.price         = o.value("price", 0.0);
        info.trigger_price = o.value("trigger_price", 0.0);
        info.filled_qty    = o.value("filled_quantity", 0.0);
        info.avg_price     = o.value("average_price", 0.0);
        info.status        = o.value("status", "");
        info.timestamp     = o.value("order_timestamp", "");
        info.message       = o.value("status_message", "");
        orders.push_back(info);
    }
    return {true, orders, "", ts};
}

// ============================================================================
// Get Trade Book — GET /order/trades/get-trades-for-day
// ============================================================================

ApiResponse<json> UpstoxBroker::get_trade_book(const BrokerCredentials& creds) {
    LOG_INFO("UpstoxBroker", "Fetching trade book");

    const auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/order/trades/get-trades-for-day", headers);

    const int64_t ts = upstox_now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (is_success(body))
        return {true, body.value("data", json::array()), "", ts};
    return {false, std::nullopt, extract_error(body, "Failed to fetch trade book"), ts};
}

// ============================================================================
// Positions — GET /portfolio/short-term-positions
// ============================================================================

ApiResponse<std::vector<BrokerPosition>> UpstoxBroker::get_positions(const BrokerCredentials& creds) {
    LOG_INFO("UpstoxBroker", "Fetching positions");

    const auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/portfolio/short-term-positions", headers);

    const int64_t ts = upstox_now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (!is_success(body))
        return {false, std::nullopt, extract_error(body, "Failed to fetch positions"), ts};

    std::vector<BrokerPosition> positions;
    const auto data = body.value("data", json::array());
    for (const auto& p : data) {
        BrokerPosition pos;
        pos.symbol       = p.value("instrument_token", p.value("trading_symbol", ""));
        pos.exchange     = p.value("exchange", "");
        pos.product_type = p.value("product", "");
        pos.quantity     = p.value("quantity", 0.0);
        pos.avg_price    = p.value("average_price", 0.0);
        pos.ltp          = p.value("last_price", 0.0);
        pos.pnl          = p.value("pnl", 0.0);
        pos.day_pnl      = p.value("day_change", 0.0);
        pos.side         = pos.quantity >= 0 ? "buy" : "sell";
        positions.push_back(pos);
    }
    return {true, positions, "", ts};
}

// ============================================================================
// Holdings — GET /portfolio/long-term-holdings
// ============================================================================

ApiResponse<std::vector<BrokerHolding>> UpstoxBroker::get_holdings(const BrokerCredentials& creds) {
    LOG_INFO("UpstoxBroker", "Fetching holdings");

    const auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/portfolio/long-term-holdings", headers);

    const int64_t ts = upstox_now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (!is_success(body))
        return {false, std::nullopt, extract_error(body, "Failed to fetch holdings"), ts};

    std::vector<BrokerHolding> holdings;
    const auto data = body.value("data", json::array());
    for (const auto& h : data) {
        BrokerHolding hold;
        hold.symbol         = h.value("instrument_token", h.value("trading_symbol", ""));
        hold.exchange       = h.value("exchange", "");
        hold.quantity       = h.value("quantity", 0.0);
        hold.avg_price      = h.value("average_price", 0.0);
        hold.ltp            = h.value("last_price", 0.0);
        hold.invested_value = hold.quantity * hold.avg_price;
        hold.current_value  = hold.quantity * hold.ltp;
        hold.pnl            = hold.current_value - hold.invested_value;
        hold.pnl_pct        = hold.invested_value > 0 ? (hold.pnl / hold.invested_value) * 100.0 : 0.0;
        holdings.push_back(hold);
    }
    return {true, holdings, "", ts};
}

// ============================================================================
// Funds — GET /user/get-funds-and-margin
// ============================================================================

ApiResponse<BrokerFunds> UpstoxBroker::get_funds(const BrokerCredentials& creds) {
    LOG_INFO("UpstoxBroker", "Fetching funds");

    const auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/user/get-funds-and-margin", headers);

    const int64_t ts = upstox_now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();

    // Handle 423 (service outside operating hours)
    if (resp.status_code == 423) {
        BrokerFunds funds;
        funds.raw_data = body;
        return {true, funds, "Service outside operating hours", ts};
    }

    if (!is_success(body))
        return {false, std::nullopt, extract_error(body, "Failed to fetch funds"), ts};

    BrokerFunds funds;
    const auto data = body.value("data", json::object());
    // Upstox returns equity and commodity segments
    const auto equity = data.value("equity", json::object());
    funds.available_balance = equity.value("available_margin", 0.0);
    funds.used_margin       = equity.value("used_margin", 0.0);
    funds.total_balance     = funds.available_balance + funds.used_margin;
    funds.raw_data          = data;
    return {true, funds, "", ts};
}

// ============================================================================
// Quotes — GET /market-quote/ohlc?instrument_key={keys}&interval=1d  (v3 API)
// ============================================================================

ApiResponse<std::vector<BrokerQuote>> UpstoxBroker::get_quotes(
        const BrokerCredentials& creds, const std::vector<std::string>& instruments) {
    LOG_INFO("UpstoxBroker", "Fetching quotes for %zu instruments", instruments.size());

    const auto headers = auth_headers(creds);
    const int64_t ts = upstox_now_ts();

    // Build comma-separated URL-encoded instrument keys
    std::string keys_param;
    for (size_t i = 0; i < instruments.size(); ++i) {
        if (i > 0) keys_param += ",";
        keys_param += url_encode(instruments[i]);
    }

    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(api_base_v3_) +
                           "/market-quote/ohlc?instrument_key=" + keys_param + "&interval=1d",
                           headers);

    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (!is_success(body))
        return {false, std::nullopt, extract_error(body, "Failed to fetch quotes"), ts};

    std::vector<BrokerQuote> quotes;
    const auto data = body.value("data", json::object());
    for (auto& [instrument_key, qdata] : data.items()) {
        BrokerQuote q;
        q.symbol = instrument_key;

        const auto ohlc = qdata.value("ohlc", json::object());
        q.open  = ohlc.value("open", 0.0);
        q.high  = ohlc.value("high", 0.0);
        q.low   = ohlc.value("low", 0.0);
        q.close = ohlc.value("close", 0.0);

        q.ltp        = qdata.value("last_price", q.close);
        q.volume     = qdata.value("volume", 0.0);
        q.change     = qdata.value("net_change", 0.0);
        q.change_pct = q.close > 0 ? (q.change / q.close) * 100.0 : 0.0;
        q.timestamp  = ts;
        quotes.push_back(q);
    }
    return {true, quotes, "", ts};
}

// ============================================================================
// History — GET /historical-candle/{instrument_key}/{interval_val}/{unit}?from_date=&to_date=  (v3 API)
// ============================================================================

ApiResponse<std::vector<BrokerCandle>> UpstoxBroker::get_history(
        const BrokerCredentials& creds,
        const std::string& instrument_key,
        const std::string& interval,
        const std::string& from_date,
        const std::string& to_date) {
    LOG_INFO("UpstoxBroker", "Fetching history for %s", instrument_key.c_str());

    const auto headers = auth_headers(creds);
    const int64_t ts = upstox_now_ts();

    // Map interval to Upstox v3 format: unit/value
    std::string unit = "day";
    std::string interval_val = "1";
    if (interval == "1m")       { unit = "minute"; interval_val = "1"; }
    else if (interval == "5m")  { unit = "minute"; interval_val = "5"; }
    else if (interval == "15m") { unit = "minute"; interval_val = "15"; }
    else if (interval == "30m") { unit = "minute"; interval_val = "30"; }
    else if (interval == "60m" || interval == "1h") { unit = "minute"; interval_val = "60"; }
    else if (interval == "D" || interval == "1D")   { unit = "day"; interval_val = "1"; }
    else if (interval == "W" || interval == "1W")   { unit = "week"; interval_val = "1"; }
    else if (interval == "M" || interval == "1M")   { unit = "month"; interval_val = "1"; }

    const std::string encoded_key = url_encode(instrument_key);
    const std::string url = std::string(api_base_v3_) + "/historical-candle/"
        + encoded_key + "/" + interval_val + "/" + unit
        + "?from_date=" + from_date + "&to_date=" + to_date;

    auto& client = HttpClient::instance();
    auto resp = client.get(url, headers);

    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (!is_success(body))
        return {false, std::nullopt, extract_error(body, "Failed to fetch historical data"), ts};

    std::vector<BrokerCandle> candles;
    const auto data = body.value("data", json::object());
    const auto arr = data.value("candles", json::array());
    for (const auto& c : arr) {
        if (!c.is_array() || c.size() < 6) continue;
        BrokerCandle candle;
        // Upstox candle format: [timestamp, open, high, low, close, volume, oi]
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
    return {true, candles, "", ts};
}

} // namespace fincept::trading
