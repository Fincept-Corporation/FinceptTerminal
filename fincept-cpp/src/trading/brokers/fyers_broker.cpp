#include "fyers_broker.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace fincept::trading {

using namespace fincept::http;

// ============================================================================
// Auth headers: "Authorization: api_key:access_token"
// ============================================================================

std::map<std::string, std::string> FyersBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Authorization", creds.api_key + ":" + creds.access_token},
        {"Content-Type",  "application/json"},
        {"Accept",        "application/json"}
    };
}

// ============================================================================
// Token Exchange — SHA256(api_key:api_secret) as appIdHash
// ============================================================================

TokenExchangeResponse FyersBroker::exchange_token(const std::string& api_key,
                                                    const std::string& api_secret,
                                                    const std::string& auth_code) {
    // Compute SHA256 hash of "api_key:api_secret"
    std::string input = api_key + ":" + api_secret;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);

    std::ostringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        ss << std::hex << std::setfill('0') << std::setw(2) << (int)hash[i];
    std::string app_id_hash = ss.str();

    json payload = {
        {"grant_type", "authorization_code"},
        {"appIdHash", app_id_hash},
        {"code", auth_code}
    };

    auto& client = HttpClient::instance();
    auto resp = client.post_json(std::string(base_url()) + "/api/v3/validate-authcode", payload,
                                  {{"Content-Type", "application/json"}, {"Accept", "application/json"}});

    TokenExchangeResponse result;
    if (!resp.success) {
        result.error = resp.error;
        return result;
    }

    auto body = resp.json_body();
    if (body.value("s", "") == "ok") {
        result.success = true;
        result.access_token = body.value("access_token", "");
        result.user_id = body.value("user_id", "");
    } else {
        result.error = body.value("message", "Token exchange failed");
    }
    return result;
}

// ============================================================================
// Order Type Mappings
// ============================================================================

int FyersBroker::fyers_order_type(OrderType t) {
    switch (t) {
        case OrderType::Limit:         return 1;
        case OrderType::Market:        return 2;
        case OrderType::StopLoss:      return 3;
        case OrderType::StopLossLimit: return 4;
    }
    return 2;
}

int FyersBroker::fyers_side(OrderSide s) {
    return s == OrderSide::Buy ? 1 : -1;
}

std::string FyersBroker::fyers_product(ProductType p) {
    switch (p) {
        case ProductType::Intraday:     return "INTRADAY";
        case ProductType::Delivery:     return "CNC";
        case ProductType::Margin:       return "MARGIN";
        case ProductType::CoverOrder:   return "CO";
        case ProductType::BracketOrder: return "BO";
    }
    return "INTRADAY";
}

// ============================================================================
// Place Order
// ============================================================================

OrderPlaceResponse FyersBroker::place_order(const BrokerCredentials& creds,
                                              const UnifiedOrder& order) {
    auto headers = auth_headers(creds);
    json payload = {
        {"symbol",      order.symbol},
        {"qty",         (int)order.quantity},
        {"type",        fyers_order_type(order.order_type)},
        {"side",        fyers_side(order.side)},
        {"productType", fyers_product(order.product_type)},
        {"validity",    order.validity},
        {"offlineOrder", false}
    };

    if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit)
        payload["limitPrice"] = order.price;
    if (order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit)
        payload["stopPrice"] = order.stop_price;
    if (order.stop_loss > 0)    payload["stopLoss"] = order.stop_loss;
    if (order.take_profit > 0)  payload["takeProfit"] = order.take_profit;

    auto& client = HttpClient::instance();
    auto resp = client.post_json(std::string(base_url()) + "/api/v3/orders", payload, headers);

    OrderPlaceResponse result;
    if (!resp.success) { result.error = resp.error; return result; }

    auto body = resp.json_body();
    if (body.value("s", "") == "ok") {
        result.success = true;
        result.order_id = body.value("id", "");
    } else {
        result.error = body.value("message", "Order placement failed");
    }
    return result;
}

// ============================================================================
// Modify Order
// ============================================================================

ApiResponse<json> FyersBroker::modify_order(const BrokerCredentials& creds,
                                              const std::string& order_id,
                                              const json& modifications) {
    auto headers = auth_headers(creds);
    json payload = modifications;
    payload["id"] = order_id;

    auto& client = HttpClient::instance();
    auto resp = client.put_json(std::string(base_url()) + "/api/v3/orders", payload, headers);

    auto now = std::chrono::system_clock::now().time_since_epoch();
    int64_t ts = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    auto body = resp.json_body();
    if (body.value("s", "") == "ok")
        return {true, body, "", ts};
    return {false, std::nullopt, body.value("message", "Order modification failed"), ts};
}

// ============================================================================
// Cancel Order
// ============================================================================

ApiResponse<json> FyersBroker::cancel_order(const BrokerCredentials& creds,
                                              const std::string& order_id) {
    auto headers = auth_headers(creds);
    json payload = {{"id", order_id}};

    auto& client = HttpClient::instance();
    auto resp = client.del(std::string(base_url()) + "/api/v3/orders", payload.dump(), headers);

    auto now = std::chrono::system_clock::now().time_since_epoch();
    int64_t ts = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    auto body = resp.json_body();
    if (body.value("s", "") == "ok")
        return {true, body, "", ts};
    return {false, std::nullopt, body.value("message", "Order cancellation failed"), ts};
}

// ============================================================================
// Get Orders (order book)
// ============================================================================

ApiResponse<std::vector<BrokerOrderInfo>> FyersBroker::get_orders(const BrokerCredentials& creds) {
    auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/api/v3/orders", headers);

    auto now = std::chrono::system_clock::now().time_since_epoch();
    int64_t ts = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    auto body = resp.json_body();
    if (body.value("s", "") != "ok")
        return {false, std::nullopt, body.value("message", "Failed to fetch orders"), ts};

    std::vector<BrokerOrderInfo> orders;
    auto arr = body.value("orderBook", json::array());
    for (auto& o : arr) {
        BrokerOrderInfo info;
        info.order_id     = o.value("id", "");
        info.symbol       = o.value("symbol", "");
        info.exchange      = o.value("exchange", "");
        info.side         = o.value("side", 0) == 1 ? "buy" : "sell";
        info.order_type   = std::to_string(o.value("type", 0));
        info.product_type = o.value("productType", "");
        info.quantity     = o.value("qty", 0.0);
        info.price        = o.value("limitPrice", 0.0);
        info.trigger_price = o.value("stopPrice", 0.0);
        info.filled_qty   = o.value("filledQty", 0.0);
        info.avg_price    = o.value("tradedPrice", 0.0);
        info.status       = std::to_string(o.value("status", 0));
        info.message      = o.value("message", "");
        orders.push_back(info);
    }
    return {true, orders, "", ts};
}

// ============================================================================
// Get Trade Book
// ============================================================================

ApiResponse<json> FyersBroker::get_trade_book(const BrokerCredentials& creds) {
    auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/api/v3/tradebook", headers);

    auto now = std::chrono::system_clock::now().time_since_epoch();
    int64_t ts = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    auto body = resp.json_body();
    if (body.value("s", "") == "ok")
        return {true, body.value("tradeBook", json::array()), "", ts};
    return {false, std::nullopt, body.value("message", "Failed to fetch trade book"), ts};
}

// ============================================================================
// Positions
// ============================================================================

ApiResponse<std::vector<BrokerPosition>> FyersBroker::get_positions(const BrokerCredentials& creds) {
    auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/api/v3/positions", headers);

    auto now = std::chrono::system_clock::now().time_since_epoch();
    int64_t ts = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    auto body = resp.json_body();
    if (body.value("s", "") != "ok")
        return {false, std::nullopt, body.value("message", "Failed to fetch positions"), ts};

    std::vector<BrokerPosition> positions;
    auto arr = body.value("netPositions", json::array());
    for (auto& p : arr) {
        BrokerPosition pos;
        pos.symbol       = p.value("symbol", "");
        pos.exchange     = p.value("exchange", "");
        pos.product_type = p.value("productType", "");
        pos.quantity     = p.value("netQty", 0.0);
        pos.avg_price    = p.value("netAvg", 0.0);
        pos.ltp          = p.value("ltp", 0.0);
        pos.pnl          = p.value("pl", 0.0);
        pos.day_pnl      = p.value("dayBuyValue", 0.0) - p.value("daySellValue", 0.0);
        pos.side         = pos.quantity >= 0 ? "buy" : "sell";
        positions.push_back(pos);
    }
    return {true, positions, "", ts};
}

// ============================================================================
// Holdings
// ============================================================================

ApiResponse<std::vector<BrokerHolding>> FyersBroker::get_holdings(const BrokerCredentials& creds) {
    auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/api/v3/holdings", headers);

    auto now = std::chrono::system_clock::now().time_since_epoch();
    int64_t ts = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    auto body = resp.json_body();
    if (body.value("s", "") != "ok")
        return {false, std::nullopt, body.value("message", "Failed to fetch holdings"), ts};

    std::vector<BrokerHolding> holdings;
    auto arr = body.value("holdings", json::array());
    for (auto& h : arr) {
        BrokerHolding hold;
        hold.symbol        = h.value("symbol", "");
        hold.exchange      = h.value("exchange", "");
        hold.quantity      = h.value("quantity", 0.0);
        hold.avg_price     = h.value("costPrice", 0.0);
        hold.ltp           = h.value("ltp", 0.0);
        hold.invested_value = hold.quantity * hold.avg_price;
        hold.current_value  = hold.quantity * hold.ltp;
        hold.pnl           = hold.current_value - hold.invested_value;
        hold.pnl_pct       = hold.invested_value > 0 ? (hold.pnl / hold.invested_value) * 100 : 0;
        holdings.push_back(hold);
    }
    return {true, holdings, "", ts};
}

// ============================================================================
// Funds
// ============================================================================

ApiResponse<BrokerFunds> FyersBroker::get_funds(const BrokerCredentials& creds) {
    auto headers = auth_headers(creds);
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/api/v3/funds", headers);

    auto now = std::chrono::system_clock::now().time_since_epoch();
    int64_t ts = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    auto body = resp.json_body();
    if (body.value("s", "") != "ok")
        return {false, std::nullopt, body.value("message", "Failed to fetch funds"), ts};

    BrokerFunds funds;
    auto fl = body.value("fund_limit", json::array());
    for (auto& item : fl) {
        auto title = item.value("title", "");
        auto val = item.value("equityAmount", 0.0);
        if (title == "Total Balance")          funds.total_balance = val;
        else if (title == "Available Balance")  funds.available_balance = val;
        else if (title == "Used Margin")        funds.used_margin = val;
    }
    funds.raw_data = body;
    return {true, funds, "", ts};
}

// ============================================================================
// Quotes
// ============================================================================

ApiResponse<std::vector<BrokerQuote>> FyersBroker::get_quotes(
        const BrokerCredentials& creds, const std::vector<std::string>& symbols) {
    auto headers = auth_headers(creds);
    std::string syms;
    for (size_t i = 0; i < symbols.size(); i++) {
        if (i > 0) syms += ",";
        syms += symbols[i];
    }

    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/data/quotes?symbols=" + syms, headers);

    auto now = std::chrono::system_clock::now().time_since_epoch();
    int64_t ts = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    auto body = resp.json_body();
    if (body.value("s", "") != "ok")
        return {false, std::nullopt, body.value("message", "Failed to fetch quotes"), ts};

    std::vector<BrokerQuote> quotes;
    auto d_arr = body.value("d", json::array());
    for (auto& item : d_arr) {
        if (item.value("s", "") != "ok") continue;
        auto v = item.value("v", json::object());
        BrokerQuote q;
        q.symbol     = v.value("symbol", "");
        q.ltp        = v.value("lp", 0.0);
        q.open       = v.value("open_price", 0.0);
        q.high       = v.value("high_price", 0.0);
        q.low        = v.value("low_price", 0.0);
        q.close      = v.value("prev_close_price", 0.0);
        q.volume     = v.value("volume", 0.0);
        q.change     = v.value("ch", 0.0);
        q.change_pct = v.value("chp", 0.0);
        q.bid        = v.value("bid", 0.0);
        q.ask        = v.value("ask", 0.0);
        q.timestamp  = v.value("tt", (int64_t)0);
        quotes.push_back(q);
    }
    return {true, quotes, "", ts};
}

// ============================================================================
// History (OHLCV)
// ============================================================================

ApiResponse<std::vector<BrokerCandle>> FyersBroker::get_history(
        const BrokerCredentials& creds,
        const std::string& symbol,
        const std::string& resolution,
        const std::string& from_date,
        const std::string& to_date) {
    auto headers = auth_headers(creds);

    std::string url = std::string(base_url()) + "/data/history"
        "?symbol=" + symbol +
        "&resolution=" + resolution +
        "&date_format=1" +
        "&range_from=" + from_date +
        "&range_to=" + to_date +
        "&cont_flag=1";

    auto& client = HttpClient::instance();
    auto resp = client.get(url, headers);

    auto now = std::chrono::system_clock::now().time_since_epoch();
    int64_t ts = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    if (!resp.success)
        return {false, std::nullopt, resp.error, ts};

    auto body = resp.json_body();
    if (body.value("s", "") != "ok")
        return {false, std::nullopt, body.value("message", "Failed to fetch history"), ts};

    std::vector<BrokerCandle> candles;
    auto arr = body.value("candles", json::array());
    for (auto& c : arr) {
        if (!c.is_array() || c.size() < 6) continue;
        BrokerCandle candle;
        candle.timestamp = c[0].get<int64_t>();
        candle.open      = c[1].get<double>();
        candle.high      = c[2].get<double>();
        candle.low       = c[3].get<double>();
        candle.close     = c[4].get<double>();
        candle.volume    = c[5].get<double>();
        candles.push_back(candle);
    }
    return {true, candles, "", ts};
}

} // namespace fincept::trading
