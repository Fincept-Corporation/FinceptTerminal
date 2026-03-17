#include "fivepaisa_broker.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <chrono>

namespace fincept::trading {

using namespace fincept::http;

// ============================================================================
// Helpers
// ============================================================================

static int64_t fivepaisa_now_ts() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

// ============================================================================
// Auth headers — 5Paisa uses "bearer {access_token}"
// ============================================================================

std::map<std::string, std::string> FivePaisaBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Authorization", "bearer " + creds.access_token},
        {"Content-Type",  "application/json"}
    };
}

// ============================================================================
// Request wrapper
// ============================================================================

json FivePaisaBroker::wrap_request(const std::string& api_key, const json& body) {
    return {
        {"head", {{"key", api_key}}},
        {"body", body}
    };
}

// ============================================================================
// Mapping helpers
// ============================================================================

const char* FivePaisaBroker::fp_exchange(const std::string& exchange) {
    if (exchange == "NSE" || exchange == "NSE_INDEX" || exchange == "NFO" || exchange == "CDS")
        return "N";
    if (exchange == "BSE" || exchange == "BSE_INDEX" || exchange == "BFO" || exchange == "BCD")
        return "B";
    if (exchange == "MCX")
        return "M";
    return "N";
}

const char* FivePaisaBroker::fp_exchange_type(const std::string& exchange) {
    if (exchange == "NSE" || exchange == "BSE" || exchange == "NSE_INDEX" || exchange == "BSE_INDEX")
        return "C";  // Cash
    if (exchange == "NFO" || exchange == "BFO" || exchange == "MCX")
        return "D";  // Derivative
    if (exchange == "CDS" || exchange == "BCD")
        return "U";  // Currency
    return "C";
}

const char* FivePaisaBroker::fp_order_side(OrderSide side) {
    return side == OrderSide::Buy ? "B" : "S";
}

bool FivePaisaBroker::fp_is_intraday(ProductType product) {
    return product == ProductType::Intraday;
}

bool FivePaisaBroker::is_success(const json& resp) {
    if (!resp.contains("head")) return false;
    const auto& head = resp["head"];
    return head.value("statusDescription", "") == "Success" ||
           head.value("status", "") == "0";
}

std::string FivePaisaBroker::extract_error(const json& resp, const char* fallback) {
    if (resp.contains("body")) {
        const auto& body = resp["body"];
        if (body.contains("Message") && body["Message"].is_string())
            return body["Message"].get<std::string>();
    }
    if (resp.contains("head")) {
        const auto& head = resp["head"];
        if (head.contains("statusDescription") && head["statusDescription"].is_string())
            return head["statusDescription"].get<std::string>();
    }
    return fallback;
}

// ============================================================================
// Token Exchange — 2-step: TOTP login then access token exchange
// For exchange_token: api_key = Key, api_secret = EncryKey, auth_code = request_token
// user_id should be in additional_data or handled separately
// ============================================================================

TokenExchangeResponse FivePaisaBroker::exchange_token(const std::string& api_key,
                                                        const std::string& api_secret,
                                                        const std::string& auth_code) {
    auto& client = HttpClient::instance();

    // Step 2: Exchange request token for access token
    // auth_code contains the request_token from TOTP login step
    json body = {
        {"RequestToken", auth_code},
        {"EncryKey",     api_secret},
        {"UserId",       ""}  // user_id should be passed; we accept empty for now
    };

    const json payload = wrap_request(api_key, body);

    Headers headers = {{"Content-Type", "application/json"}};

    auto resp = client.post_json(
        std::string(base_url()) + "/VendorsAPI/Service1.svc/GetAccessToken",
        payload, headers);

    if (!resp.success) {
        return {false, "", "", "Request failed: " + resp.error};
    }

    const auto data = resp.json_body();
    if (data.is_null()) {
        return {false, "", "", "Invalid JSON response"};
    }

    if (data.contains("body") && data["body"].contains("AccessToken") &&
        data["body"]["AccessToken"].is_string()) {
        return {true, data["body"]["AccessToken"].get<std::string>(), "", ""};
    }

    return {false, "", "", extract_error(data, "Failed to obtain access token")};
}

// ============================================================================
// Validate Token
// ============================================================================

ApiResponse<bool> FivePaisaBroker::validate_token(const BrokerCredentials& creds) {
    const int64_t ts = fivepaisa_now_ts();
    auto& client = HttpClient::instance();

    const json payload = wrap_request(creds.api_key, {{"ClientCode", creds.user_id}});

    auto resp = client.post_json(
        std::string(base_url()) + "/VendorsAPI/Service1.svc/V4/Margin",
        payload, auth_headers(creds));

    if (!resp.success) {
        return {false, false, "Request failed: " + resp.error, ts};
    }

    const auto data = resp.json_body();
    const bool valid = is_success(data);
    return {valid, valid, valid ? "" : "Token validation failed", ts};
}

// ============================================================================
// Orders
// ============================================================================

OrderPlaceResponse FivePaisaBroker::place_order(const BrokerCredentials& creds,
                                                  const UnifiedOrder& order) {
    auto& client = HttpClient::instance();

    // 5Paisa uses numeric scrip codes. The scrip code should be encoded
    // in the symbol field as "EXCHANGE:SCRIP_CODE" or just the scrip code.
    int64_t scrip_code = 0;
    {
        const auto colon = order.symbol.find(':');
        const std::string code_str = (colon != std::string::npos)
            ? order.symbol.substr(colon + 1)
            : order.symbol;
        try { scrip_code = std::stoll(code_str); }
        catch (...) { /* leave as 0 */ }
    }

    json body = {
        {"ClientCode",    creds.user_id},
        {"OrderType",     fp_order_side(order.side)},
        {"Exchange",      fp_exchange(order.exchange)},
        {"ExchangeType",  fp_exchange_type(order.exchange)},
        {"ScripCode",     scrip_code},
        {"Price",         order.price},
        {"Qty",           order.quantity},
        {"StopLossPrice", order.stop_price},
        {"DisQty",        0},
        {"IsIntraday",    fp_is_intraday(order.product_type)},
        {"AHPlaced",      "N"},
        {"RemoteOrderID", "FinceptTerminal"}
    };

    const json payload = wrap_request(creds.api_key, body);

    auto resp = client.post_json(
        std::string(base_url()) + "/VendorsAPI/Service1.svc/V1/PlaceOrderRequest",
        payload, auth_headers(creds));

    if (!resp.success) {
        return {false, "", "Request failed: " + resp.error};
    }

    const auto data = resp.json_body();
    if (is_success(data)) {
        std::string oid;
        if (data.contains("body") && data["body"].contains("BrokerOrderID")) {
            const auto& boid = data["body"]["BrokerOrderID"];
            if (boid.is_number()) oid = std::to_string(boid.get<int64_t>());
            else if (boid.is_string()) oid = boid.get<std::string>();
        }
        return {true, oid, ""};
    }

    return {false, "", extract_error(data, "Order placement failed")};
}

ApiResponse<json> FivePaisaBroker::modify_order(const BrokerCredentials& creds,
                                                  const std::string& order_id,
                                                  const json& modifications) {
    const int64_t ts = fivepaisa_now_ts();
    auto& client = HttpClient::instance();

    json body = {
        {"ExchOrderID",   order_id},
        {"Price",         modifications.value("price", 0.0)},
        {"Qty",           modifications.value("quantity", 0)},
        {"StopLossPrice", modifications.value("trigger_price", 0.0)},
        {"DisQty",        modifications.value("disclosed_quantity", 0)}
    };

    const json payload = wrap_request(creds.api_key, body);

    auto resp = client.post_json(
        std::string(base_url()) + "/VendorsAPI/Service1.svc/V1/ModifyOrderRequest",
        payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto data = resp.json_body();
    if (is_success(data)) {
        return {true, data.value("body", json{}), "", ts};
    }
    return {false, std::nullopt, extract_error(data, "Order modification failed"), ts};
}

ApiResponse<json> FivePaisaBroker::cancel_order(const BrokerCredentials& creds,
                                                  const std::string& order_id) {
    const int64_t ts = fivepaisa_now_ts();
    auto& client = HttpClient::instance();

    json body = {{"ExchOrderID", order_id}};
    const json payload = wrap_request(creds.api_key, body);

    auto resp = client.post_json(
        std::string(base_url()) + "/VendorsAPI/Service1.svc/V1/CancelOrderRequest",
        payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto data = resp.json_body();
    if (is_success(data)) {
        return {true, data.value("body", json{}), "", ts};
    }
    return {false, std::nullopt, extract_error(data, "Order cancellation failed"), ts};
}

ApiResponse<std::vector<BrokerOrderInfo>> FivePaisaBroker::get_orders(const BrokerCredentials& creds) {
    const int64_t ts = fivepaisa_now_ts();
    auto& client = HttpClient::instance();

    const json payload = wrap_request(creds.api_key, {{"ClientCode", creds.user_id}});

    auto resp = client.post_json(
        std::string(base_url()) + "/VendorsAPI/Service1.svc/V3/OrderBook",
        payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto data = resp.json_body();
    std::vector<BrokerOrderInfo> orders;

    if (data.contains("body") && data["body"].contains("OrderBookDetail") &&
        data["body"]["OrderBookDetail"].is_array()) {
        for (const auto& o : data["body"]["OrderBookDetail"]) {
            BrokerOrderInfo info;
            info.order_id   = std::to_string(o.value("BrokerOrderId", 0));
            info.symbol     = o.value("ScripName", "");
            info.exchange   = o.value("Exch", "");
            info.side       = o.value("BuySell", "");
            info.order_type = o.value("AtMarket", "");
            info.status     = o.value("OrderStatus", "");
            info.quantity   = o.value("Qty", 0);
            info.filled_qty = o.value("TradedQty", 0);
            info.price      = o.value("Rate", 0.0);
            info.avg_price  = o.value("AvgRate", 0.0);
            orders.push_back(std::move(info));
        }
    }

    return {true, std::move(orders), "", ts};
}

ApiResponse<json> FivePaisaBroker::get_trade_book(const BrokerCredentials& creds) {
    const int64_t ts = fivepaisa_now_ts();
    auto& client = HttpClient::instance();

    const json payload = wrap_request(creds.api_key, {{"ClientCode", creds.user_id}});

    auto resp = client.post_json(
        std::string(base_url()) + "/VendorsAPI/Service1.svc/V1/TradeBook",
        payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto data = resp.json_body();
    json trades = json::array();
    if (data.contains("body") && data["body"].contains("TradeBookDetail")) {
        trades = data["body"]["TradeBookDetail"];
    }

    return {true, std::move(trades), "", ts};
}

// ============================================================================
// Portfolio
// ============================================================================

ApiResponse<std::vector<BrokerPosition>> FivePaisaBroker::get_positions(const BrokerCredentials& creds) {
    const int64_t ts = fivepaisa_now_ts();
    auto& client = HttpClient::instance();

    const json payload = wrap_request(creds.api_key, {{"ClientCode", creds.user_id}});

    auto resp = client.post_json(
        std::string(base_url()) + "/VendorsAPI/Service1.svc/V2/NetPositionNetWise",
        payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto data = resp.json_body();
    std::vector<BrokerPosition> positions;

    if (data.contains("body") && data["body"].contains("NetPositionDetail") &&
        data["body"]["NetPositionDetail"].is_array()) {
        for (const auto& p : data["body"]["NetPositionDetail"]) {
            BrokerPosition pos;
            pos.symbol    = p.value("ScripName", "");
            pos.exchange  = p.value("Exch", "");
            pos.quantity  = p.value("NetQty", 0);
            pos.avg_price = p.value("BuyAvgRate", 0.0);
            pos.pnl       = p.value("MTOM", 0.0);
            positions.push_back(std::move(pos));
        }
    }

    return {true, std::move(positions), "", ts};
}

ApiResponse<std::vector<BrokerHolding>> FivePaisaBroker::get_holdings(const BrokerCredentials& creds) {
    const int64_t ts = fivepaisa_now_ts();
    auto& client = HttpClient::instance();

    const json payload = wrap_request(creds.api_key, {{"ClientCode", creds.user_id}});

    auto resp = client.post_json(
        std::string(base_url()) + "/VendorsAPI/Service1.svc/V3/Holding",
        payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto data = resp.json_body();
    std::vector<BrokerHolding> holdings;

    if (data.contains("body") && data["body"].contains("Data") &&
        data["body"]["Data"].is_array()) {
        for (const auto& h : data["body"]["Data"]) {
            BrokerHolding hld;
            hld.symbol    = h.value("ScripName", "");
            hld.exchange  = h.value("Exch", "NSE");
            hld.quantity  = h.value("Quantity", 0);
            hld.avg_price = h.value("BuyRate", 0.0);
            hld.ltp       = h.value("CurrentPrice", 0.0);
            hld.pnl       = h.value("ProfitLoss", 0.0);
            holdings.push_back(std::move(hld));
        }
    }

    return {true, std::move(holdings), "", ts};
}

ApiResponse<BrokerFunds> FivePaisaBroker::get_funds(const BrokerCredentials& creds) {
    const int64_t ts = fivepaisa_now_ts();
    auto& client = HttpClient::instance();

    const json payload = wrap_request(creds.api_key, {{"ClientCode", creds.user_id}});

    auto resp = client.post_json(
        std::string(base_url()) + "/VendorsAPI/Service1.svc/V4/Margin",
        payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto data = resp.json_body();
    BrokerFunds funds;

    // Extract equity margin from array
    if (data.contains("body") && data["body"].contains("EquityMargin") &&
        data["body"]["EquityMargin"].is_array() &&
        !data["body"]["EquityMargin"].empty()) {
        const auto& margin = data["body"]["EquityMargin"][0];
        funds.available_balance = margin.value("NetAvailableMargin", 0.0);
        funds.used_margin      = margin.value("MarginUtilized", 0.0);
        funds.collateral       = margin.value("Collateral", 0.0);
    }

    return {true, std::move(funds), "", ts};
}

// ============================================================================
// Market Data
// ============================================================================

ApiResponse<std::vector<BrokerQuote>> FivePaisaBroker::get_quotes(const BrokerCredentials& creds,
                                                                    const std::vector<std::string>& symbols) {
    const int64_t ts = fivepaisa_now_ts();
    auto& client = HttpClient::instance();

    std::vector<BrokerQuote> quotes;

    for (const auto& sym : symbols) {
        // Symbol format: "EXCHANGE:SCRIP_CODE" e.g. "NSE:1660"
        std::string exchange = "NSE";
        int64_t scrip_code = 0;
        const auto colon = sym.find(':');
        if (colon != std::string::npos) {
            exchange = sym.substr(0, colon);
            try { scrip_code = std::stoll(sym.substr(colon + 1)); }
            catch (...) { continue; }
        } else {
            try { scrip_code = std::stoll(sym); }
            catch (...) { continue; }
        }

        json body = {
            {"ClientCode",  creds.user_id},
            {"Exchange",    fp_exchange(exchange)},
            {"ExchangeType", fp_exchange_type(exchange)},
            {"ScripCode",   scrip_code},
            {"ScripData",   ""}
        };

        const json payload = wrap_request(creds.api_key, body);

        auto resp = client.post_json(
            std::string(base_url()) + "/VendorsAPI/Service1.svc/V2/MarketDepth",
            payload, auth_headers(creds));

        if (!resp.success) continue;

        const auto data = resp.json_body();
        if (!is_success(data)) continue;

        if (data.contains("body") && data["body"].is_object()) {
            const auto& b = data["body"];
            BrokerQuote q;
            q.symbol   = b.value("ScripName", sym);
            q.ltp      = b.value("LastRate", 0.0);
            q.open     = b.value("OpenRate", 0.0);
            q.high     = b.value("High", 0.0);
            q.low      = b.value("Low", 0.0);
            q.close    = b.value("PClose", 0.0);
            q.volume   = b.value("TotalQty", static_cast<int64_t>(0));
            q.change   = q.ltp - q.close;
            quotes.push_back(std::move(q));
        }
    }

    return {true, std::move(quotes), "", ts};
}

ApiResponse<std::vector<BrokerCandle>> FivePaisaBroker::get_history(const BrokerCredentials& creds,
                                                                      const std::string& symbol,
                                                                      const std::string& resolution,
                                                                      const std::string& from_date,
                                                                      const std::string& to_date) {
    const int64_t ts = fivepaisa_now_ts();
    auto& client = HttpClient::instance();

    // Symbol format: "EXCHANGE:SCRIP_CODE"
    std::string exchange = "NSE";
    int64_t scrip_code = 0;
    const auto colon = symbol.find(':');
    if (colon != std::string::npos) {
        exchange = symbol.substr(0, colon);
        try { scrip_code = std::stoll(symbol.substr(colon + 1)); }
        catch (...) { return {false, std::nullopt, "Invalid scrip code", ts}; }
    } else {
        try { scrip_code = std::stoll(symbol); }
        catch (...) { return {false, std::nullopt, "Invalid scrip code", ts}; }
    }

    json body = {
        {"ClientCode", creds.user_id},
        {"Exch",       fp_exchange(exchange)},
        {"ExchType",   fp_exchange_type(exchange)},
        {"ScripCode",  scrip_code},
        {"Interval",   resolution},
        {"FromDate",   from_date},
        {"ToDate",     to_date}
    };

    const json payload = wrap_request(creds.api_key, body);

    auto resp = client.post_json(
        std::string(base_url()) + "/VendorsAPI/Service1.svc/V1/HistoricalData",
        payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto data = resp.json_body();
    std::vector<BrokerCandle> candles;

    if (data.contains("body") && data["body"].contains("Data") &&
        data["body"]["Data"].is_array()) {
        for (const auto& c : data["body"]["Data"]) {
            BrokerCandle candle;
            candle.timestamp = c.value("Datetime", static_cast<int64_t>(0));
            candle.open      = c.value("Open", 0.0);
            candle.high      = c.value("High", 0.0);
            candle.low       = c.value("Low", 0.0);
            candle.close     = c.value("Close", 0.0);
            candle.volume    = c.value("Volume", static_cast<int64_t>(0));
            candles.push_back(candle);
        }
    }

    return {true, std::move(candles), "", ts};
}

} // namespace fincept::trading
