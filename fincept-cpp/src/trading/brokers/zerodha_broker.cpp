#include "zerodha_broker.h"
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

static int64_t zerodha_now_ts() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

// ============================================================================
// Auth headers: "Authorization: token api_key:access_token"
// Zerodha uses "X-Kite-Version: 3" and form-urlencoded content type
// ============================================================================

std::map<std::string, std::string> ZerodhaBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Authorization",  "token " + creds.api_key + ":" + creds.access_token},
        {"X-Kite-Version", kite_api_version},
        {"Content-Type",   "application/x-www-form-urlencoded"}
    };
}

// ============================================================================
// URL-encode form parameters (Zerodha uses form-urlencoded, not JSON)
// ============================================================================

std::string ZerodhaBroker::url_encode_params(const std::map<std::string, std::string>& params) {
    std::ostringstream ss;
    bool first = true;
    for (const auto& [key, value] : params) {
        if (!first) ss << '&';
        ss << key << '=' << value;
        first = false;
    }
    return ss.str();
}

// ============================================================================
// Order type mappings — Zerodha uses string types
// ============================================================================

const char* ZerodhaBroker::zerodha_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "MARKET";
        case OrderType::Limit:         return "LIMIT";
        case OrderType::StopLoss:      return "SL-M";
        case OrderType::StopLossLimit: return "SL";
    }
    return "MARKET";
}

const char* ZerodhaBroker::zerodha_product(ProductType p) {
    switch (p) {
        case ProductType::Intraday:      return "MIS";
        case ProductType::Delivery:      return "CNC";
        case ProductType::Margin:        return "NRML";
        case ProductType::CoverOrder:    return "MIS";  // CO uses MIS product
        case ProductType::BracketOrder:  return "MIS";  // BO uses MIS product
    }
    return "MIS";
}

const char* ZerodhaBroker::zerodha_side(OrderSide s) {
    return s == OrderSide::Buy ? "BUY" : "SELL";
}

const char* ZerodhaBroker::zerodha_variety(ProductType p) {
    switch (p) {
        case ProductType::CoverOrder:   return "co";
        case ProductType::BracketOrder: return "bo";
        default:                        return "regular";
    }
}

// ============================================================================
// Token Exchange — SHA256(api_key + request_token + api_secret)
// POST /session/token with form-urlencoded body
// ============================================================================

TokenExchangeResponse ZerodhaBroker::exchange_token(const std::string& api_key,
                                                      const std::string& api_secret,
                                                      const std::string& request_token) {
    LOG_INFO("ZerodhaBroker", "Exchanging request token");

    // Compute SHA256(api_key + request_token + api_secret)
    const std::string input = api_key + request_token + api_secret;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);

    std::ostringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
    const std::string checksum = ss.str();

    // Build form-urlencoded body
    std::map<std::string, std::string> params = {
        {"api_key",       api_key},
        {"request_token", request_token},
        {"checksum",      checksum}
    };

    Headers headers = {
        {"X-Kite-Version", kite_api_version},
        {"Content-Type",   "application/x-www-form-urlencoded"}
    };

    auto& client = HttpClient::instance();
    auto resp = client.post(std::string(base_url()) + "/session/token",
                            url_encode_params(params), headers);

    TokenExchangeResponse result;
    if (!resp.success) {
        result.error = resp.error;
        return result;
    }

    const auto body = resp.json_body();
    if (body.value("status", "") == "success") {
        const auto data = body.value("data", json::object());
        result.success = true;
        result.access_token = data.value("access_token", "");
        result.user_id = data.value("user_id", "");
    } else {
        result.error = body.value("message", "Token exchange failed");
    }
    return result;
}

// ============================================================================
// Validate Token — GET /user/profile
// ============================================================================

ApiResponse<bool> ZerodhaBroker::validate_token(const BrokerCredentials& creds) {
    LOG_INFO("ZerodhaBroker", "Validating access token");

    const auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/user/profile", headers);

    const int64_t ts = zerodha_now_ts();
    if (resp.success && resp.status_code >= 200 && resp.status_code < 300) {
        return {true, true, "", ts};
    }
    return {false, false, "Token validation failed", ts};
}

// ============================================================================
// Place Order — POST /orders/{variety} with form-urlencoded body
// ============================================================================

OrderPlaceResponse ZerodhaBroker::place_order(const BrokerCredentials& creds,
                                                const UnifiedOrder& order) {
    LOG_INFO("ZerodhaBroker", "Placing order: %s", order.symbol.c_str());

    auto headers = auth_headers(creds);
    const std::string variety = zerodha_variety(order.product_type);

    std::map<std::string, std::string> params = {
        {"tradingsymbol",   order.symbol},
        {"exchange",        order.exchange},
        {"transaction_type", zerodha_side(order.side)},
        {"quantity",        std::to_string(static_cast<int>(order.quantity))},
        {"product",         zerodha_product(order.product_type)},
        {"order_type",      zerodha_order_type(order.order_type)},
        {"validity",        order.validity}
    };

    if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit) {
        params["price"] = std::to_string(order.price);
    }
    if (order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit) {
        params["trigger_price"] = std::to_string(order.stop_price);
    }

    auto& client = HttpClient::instance();
    auto resp = client.post(std::string(base_url()) + "/orders/" + variety,
                            url_encode_params(params), headers);

    OrderPlaceResponse result;
    if (!resp.success) { result.error = resp.error; return result; }

    const auto body = resp.json_body();
    if (body.value("status", "") == "success") {
        const auto data = body.value("data", json::object());
        result.success = true;
        result.order_id = data.value("order_id", "");
    } else {
        result.error = body.value("message", "Order placement failed");
    }
    return result;
}

// ============================================================================
// Modify Order — PUT /orders/regular/{order_id}
// ============================================================================

ApiResponse<json> ZerodhaBroker::modify_order(const BrokerCredentials& creds,
                                                const std::string& order_id,
                                                const json& modifications) {
    LOG_INFO("ZerodhaBroker", "Modifying order: %s", order_id.c_str());

    auto headers = auth_headers(creds);
    const int64_t ts = zerodha_now_ts();

    // Convert JSON modifications to form params
    std::map<std::string, std::string> params;
    for (auto& [key, val] : modifications.items()) {
        if (val.is_string()) params[key] = val.get<std::string>();
        else if (val.is_number()) params[key] = std::to_string(val.get<double>());
        else params[key] = val.dump();
    }

    auto& client = HttpClient::instance();
    auto resp = client.put(std::string(base_url()) + "/orders/regular/" + order_id,
                           url_encode_params(params), headers);

    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (body.value("status", "") == "success") {
        const auto data = body.value("data", json::object());
        return {true, data, "", ts};
    }
    return {false, std::nullopt, body.value("message", "Order modification failed"), ts};
}

// ============================================================================
// Cancel Order — DELETE /orders/regular/{order_id}
// ============================================================================

ApiResponse<json> ZerodhaBroker::cancel_order(const BrokerCredentials& creds,
                                                const std::string& order_id) {
    LOG_INFO("ZerodhaBroker", "Cancelling order: %s", order_id.c_str());

    auto headers = auth_headers(creds);
    const int64_t ts = zerodha_now_ts();

    auto& client = HttpClient::instance();
    auto resp = client.del(std::string(base_url()) + "/orders/regular/" + order_id, "", headers);

    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (body.value("status", "") == "success") {
        const auto data = body.value("data", json::object());
        return {true, data, "", ts};
    }
    return {false, std::nullopt, body.value("message", "Order cancellation failed"), ts};
}

// ============================================================================
// Get Orders — GET /orders
// ============================================================================

ApiResponse<std::vector<BrokerOrderInfo>> ZerodhaBroker::get_orders(const BrokerCredentials& creds) {
    LOG_INFO("ZerodhaBroker", "Fetching orders");

    const auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/orders", headers);

    const int64_t ts = zerodha_now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (body.value("status", "") != "success")
        return {false, std::nullopt, body.value("message", "Failed to fetch orders"), ts};

    std::vector<BrokerOrderInfo> orders;
    const auto data = body.value("data", json::array());
    for (const auto& o : data) {
        BrokerOrderInfo info;
        info.order_id      = o.value("order_id", "");
        info.symbol        = o.value("tradingsymbol", "");
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
// Get Trade Book — GET /trades
// ============================================================================

ApiResponse<json> ZerodhaBroker::get_trade_book(const BrokerCredentials& creds) {
    LOG_INFO("ZerodhaBroker", "Fetching trade book");

    const auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/trades", headers);

    const int64_t ts = zerodha_now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (body.value("status", "") == "success")
        return {true, body.value("data", json::array()), "", ts};
    return {false, std::nullopt, body.value("message", "Failed to fetch trade book"), ts};
}

// ============================================================================
// Positions — GET /portfolio/positions
// ============================================================================

ApiResponse<std::vector<BrokerPosition>> ZerodhaBroker::get_positions(const BrokerCredentials& creds) {
    LOG_INFO("ZerodhaBroker", "Fetching positions");

    const auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/portfolio/positions", headers);

    const int64_t ts = zerodha_now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (body.value("status", "") != "success")
        return {false, std::nullopt, body.value("message", "Failed to fetch positions"), ts};

    std::vector<BrokerPosition> positions;
    const auto data = body.value("data", json::object());
    const auto net = data.value("net", json::array());
    for (const auto& p : net) {
        BrokerPosition pos;
        pos.symbol       = p.value("tradingsymbol", "");
        pos.exchange     = p.value("exchange", "");
        pos.product_type = p.value("product", "");
        pos.quantity     = p.value("quantity", 0.0);
        pos.avg_price    = p.value("average_price", 0.0);
        pos.ltp          = p.value("last_price", 0.0);
        pos.pnl          = p.value("pnl", 0.0);
        pos.day_pnl      = p.value("day_m2m", 0.0);
        pos.side         = pos.quantity >= 0 ? "buy" : "sell";
        positions.push_back(pos);
    }
    return {true, positions, "", ts};
}

// ============================================================================
// Holdings — GET /portfolio/holdings
// ============================================================================

ApiResponse<std::vector<BrokerHolding>> ZerodhaBroker::get_holdings(const BrokerCredentials& creds) {
    LOG_INFO("ZerodhaBroker", "Fetching holdings");

    const auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/portfolio/holdings", headers);

    const int64_t ts = zerodha_now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (body.value("status", "") != "success")
        return {false, std::nullopt, body.value("message", "Failed to fetch holdings"), ts};

    std::vector<BrokerHolding> holdings;
    const auto data = body.value("data", json::array());
    for (const auto& h : data) {
        BrokerHolding hold;
        hold.symbol         = h.value("tradingsymbol", "");
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
// Funds — GET /user/margins
// ============================================================================

ApiResponse<BrokerFunds> ZerodhaBroker::get_funds(const BrokerCredentials& creds) {
    LOG_INFO("ZerodhaBroker", "Fetching margins/funds");

    const auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/user/margins", headers);

    const int64_t ts = zerodha_now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (body.value("status", "") != "success")
        return {false, std::nullopt, body.value("message", "Failed to fetch margins"), ts};

    BrokerFunds funds;
    const auto data = body.value("data", json::object());
    // Zerodha returns equity and commodity segments
    const auto equity = data.value("equity", json::object());
    funds.available_balance = equity.value("available", json::object()).value("live_balance", 0.0);
    funds.used_margin       = equity.value("utilised", json::object()).value("debits", 0.0);
    funds.total_balance     = funds.available_balance + funds.used_margin;
    funds.collateral        = equity.value("available", json::object()).value("collateral", 0.0);
    funds.raw_data          = data;
    return {true, funds, "", ts};
}

// ============================================================================
// Quotes — GET /quote?i=EXCHANGE:SYMBOL&i=EXCHANGE:SYMBOL
// ============================================================================

ApiResponse<std::vector<BrokerQuote>> ZerodhaBroker::get_quotes(
        const BrokerCredentials& creds, const std::vector<std::string>& instruments) {
    LOG_INFO("ZerodhaBroker", "Fetching quotes for %zu instruments", instruments.size());

    const auto headers = auth_headers(creds);

    // Build URL with repeated i= params: /quote?i=NSE:RELIANCE&i=NSE:INFY
    std::string url = std::string(base_url()) + "/quote?";
    for (size_t i = 0; i < instruments.size(); ++i) {
        if (i > 0) url += "&";
        url += "i=" + instruments[i];
    }

    auto& client = HttpClient::instance();
    auto resp = client.get(url, headers);

    const int64_t ts = zerodha_now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (body.value("status", "") != "success")
        return {false, std::nullopt, body.value("message", "Failed to fetch quotes"), ts};

    std::vector<BrokerQuote> quotes;
    const auto data = body.value("data", json::object());
    for (auto& [instrument_key, qdata] : data.items()) {
        BrokerQuote q;
        q.symbol     = instrument_key;
        q.ltp        = qdata.value("last_price", 0.0);
        q.volume     = qdata.value("volume", 0.0);
        q.change     = qdata.value("net_change", 0.0);

        const auto ohlc = qdata.value("ohlc", json::object());
        q.open  = ohlc.value("open", 0.0);
        q.high  = ohlc.value("high", 0.0);
        q.low   = ohlc.value("low", 0.0);
        q.close = ohlc.value("close", 0.0);

        q.change_pct = q.close > 0 ? (q.change / q.close) * 100.0 : 0.0;

        const auto depth = qdata.value("depth", json::object());
        const auto buy_depth = depth.value("buy", json::array());
        const auto sell_depth = depth.value("sell", json::array());
        if (!buy_depth.empty())  q.bid = buy_depth[0].value("price", 0.0);
        if (!sell_depth.empty()) q.ask = sell_depth[0].value("price", 0.0);

        q.timestamp = qdata.value("last_trade_time", static_cast<int64_t>(0));
        quotes.push_back(q);
    }
    return {true, quotes, "", ts};
}

// ============================================================================
// History — GET /instruments/historical/{instrument_token}/{interval}?from=&to=
// ============================================================================

ApiResponse<std::vector<BrokerCandle>> ZerodhaBroker::get_history(
        const BrokerCredentials& creds,
        const std::string& instrument_token,
        const std::string& interval,
        const std::string& from_date,
        const std::string& to_date) {
    LOG_INFO("ZerodhaBroker", "Fetching historical data for %s", instrument_token.c_str());

    const auto headers = auth_headers(creds);
    const std::string url = std::string(base_url()) + "/instruments/historical/"
        + instrument_token + "/" + interval
        + "?from=" + from_date + "&to=" + to_date;

    auto& client = HttpClient::instance();
    auto resp = client.get(url, headers);

    const int64_t ts = zerodha_now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (body.value("status", "") != "success")
        return {false, std::nullopt, body.value("message", "Failed to fetch historical data"), ts};

    std::vector<BrokerCandle> candles;
    const auto data = body.value("data", json::object());
    const auto arr = data.value("candles", json::array());
    for (const auto& c : arr) {
        if (!c.is_array() || c.size() < 6) continue;
        BrokerCandle candle;
        // Zerodha returns: [timestamp_string, open, high, low, close, volume]
        // Parse timestamp string or use epoch
        if (c[0].is_number()) {
            candle.timestamp = c[0].get<int64_t>();
        } else {
            candle.timestamp = ts; // fallback
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

// ============================================================================
// Download Master Contract — GET /instruments (CSV)
// ============================================================================

ApiResponse<json> ZerodhaBroker::download_master_contract(const BrokerCredentials& creds) {
    LOG_INFO("ZerodhaBroker", "Downloading master contract (instruments)");

    const auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/instruments", headers);

    const int64_t ts = zerodha_now_ts();
    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    // Parse CSV response into JSON array
    json instruments = json::array();
    std::istringstream stream(resp.body);
    std::string line;

    // Read header line
    std::vector<std::string> csv_headers;
    if (std::getline(stream, line)) {
        std::istringstream header_stream(line);
        std::string field;
        while (std::getline(header_stream, field, ',')) {
            csv_headers.push_back(field);
        }
    }

    // Parse data lines
    while (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        std::string field;
        json obj = json::object();
        size_t col = 0;
        while (std::getline(line_stream, field, ',') && col < csv_headers.size()) {
            obj[csv_headers[col]] = field;
            ++col;
        }
        if (col == csv_headers.size()) {
            instruments.push_back(obj);
        }
    }

    LOG_INFO("ZerodhaBroker", "Downloaded %zu instruments", instruments.size());
    return {true, instruments, "", ts};
}

// ============================================================================
// Calculate Margin — POST /margins/orders or /margins/basket
// ============================================================================

ApiResponse<json> ZerodhaBroker::calculate_margin(const BrokerCredentials& creds,
                                                    const json& orders) {
    LOG_INFO("ZerodhaBroker", "Calculating margin");

    auto headers = auth_headers(creds);
    headers["Content-Type"] = "application/json";
    const int64_t ts = zerodha_now_ts();

    const std::string endpoint = orders.is_array() && orders.size() > 1
        ? "/margins/basket" : "/margins/orders";

    auto& client = HttpClient::instance();
    auto resp = client.post_json(std::string(base_url()) + endpoint, orders, headers);

    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    const auto body = resp.json_body();
    if (body.value("status", "") == "success")
        return {true, body.value("data", json::object()), "", ts};
    return {false, std::nullopt, body.value("message", "Failed to calculate margin"), ts};
}

// ============================================================================
// Close All Positions — fetch positions then place market exit orders
// ============================================================================

ApiResponse<json> ZerodhaBroker::close_all_positions(const BrokerCredentials& creds) {
    LOG_INFO("ZerodhaBroker", "Closing all open positions");

    const int64_t ts = zerodha_now_ts();
    auto pos_resp = get_positions(creds);
    if (!pos_resp.success)
        return {false, std::nullopt, pos_resp.error, ts};

    json results = json::array();
    for (const auto& pos : *pos_resp.data) {
        if (pos.quantity == 0) continue;

        UnifiedOrder exit_order;
        exit_order.symbol       = pos.symbol;
        exit_order.exchange     = pos.exchange;
        exit_order.side         = pos.quantity > 0 ? OrderSide::Sell : OrderSide::Buy;
        exit_order.order_type   = OrderType::Market;
        exit_order.quantity     = std::abs(pos.quantity);
        // Map product string back to enum
        if (pos.product_type == "CNC") exit_order.product_type = ProductType::Delivery;
        else if (pos.product_type == "NRML") exit_order.product_type = ProductType::Margin;
        else exit_order.product_type = ProductType::Intraday;

        auto order_resp = place_order(creds, exit_order);
        results.push_back(json{
            {"symbol", pos.symbol},
            {"success", order_resp.success},
            {"order_id", order_resp.order_id},
            {"error", order_resp.error}
        });
    }
    return {true, results, "", ts};
}

// ============================================================================
// Cancel All Orders — fetch open orders then cancel each
// ============================================================================

ApiResponse<json> ZerodhaBroker::cancel_all_orders(const BrokerCredentials& creds) {
    LOG_INFO("ZerodhaBroker", "Cancelling all open orders");

    const int64_t ts = zerodha_now_ts();
    auto orders_resp = get_orders(creds);
    if (!orders_resp.success)
        return {false, std::nullopt, orders_resp.error, ts};

    json results = json::array();
    for (const auto& order : *orders_resp.data) {
        if (order.status != "OPEN" && order.status != "TRIGGER PENDING") continue;
        if (order.order_id.empty()) continue;

        auto cancel_resp = cancel_order(creds, order.order_id);
        results.push_back(json{
            {"order_id", order.order_id},
            {"success", cancel_resp.success},
            {"error", cancel_resp.error}
        });
    }
    return {true, results, "", ts};
}

} // namespace fincept::trading
