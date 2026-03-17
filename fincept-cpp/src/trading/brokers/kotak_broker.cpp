#include "kotak_broker.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <chrono>
#include <sstream>

namespace fincept::trading {

using namespace fincept::http;

// ============================================================================
// Helpers
// ============================================================================

static int64_t kotak_now_ts() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

// ============================================================================
// URL encode
// ============================================================================

std::string KotakBroker::url_encode(const std::string& s) {
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

std::string KotakBroker::encode_jdata(const json& data) {
    return "jData=" + url_encode(data.dump());
}

// ============================================================================
// Parse composite auth token: "trading_token:::trading_sid:::base_url:::access_token"
// ============================================================================

KotakBroker::KotakAuth KotakBroker::parse_auth_token(const std::string& auth_token) {
    KotakAuth auth;
    std::istringstream stream(auth_token);
    std::string part;
    std::vector<std::string> parts;

    // Split by ":::" delimiter
    size_t pos = 0;
    std::string remaining = auth_token;
    while ((pos = remaining.find(":::")) != std::string::npos) {
        parts.push_back(remaining.substr(0, pos));
        remaining = remaining.substr(pos + 3);
    }
    parts.push_back(remaining);

    if (parts.size() == 4) {
        auth.trading_token    = parts[0];
        auth.trading_sid      = parts[1];
        auth.trading_base_url = parts[2];
        auth.access_token     = parts[3];
        auth.valid = true;
    }
    return auth;
}

// ============================================================================
// Auth headers — Kotak uses Sid + Auth headers for trading API
// ============================================================================

std::map<std::string, std::string> KotakBroker::auth_headers(const BrokerCredentials& creds) const {
    // Parse the composite auth token stored in access_token
    const auto auth = parse_auth_token(creds.access_token);
    return {
        {"Sid",          auth.trading_sid},
        {"Auth",         auth.trading_token},
        {"neo-fin-key",  "neotradeapi"},
        {"accept",       "application/json"}
    };
}

// ============================================================================
// Exchange / order type mappings
// ============================================================================

const char* KotakBroker::map_exchange_to_kotak(const std::string& exchange) {
    if (exchange == "NSE") return "nse_cm";
    if (exchange == "BSE") return "bse_cm";
    if (exchange == "NFO") return "nse_fo";
    if (exchange == "BFO") return "bse_fo";
    if (exchange == "CDS") return "cde_fo";
    if (exchange == "BCD") return "bcs_fo";
    if (exchange == "MCX") return "mcx_fo";
    return exchange.c_str();
}

const char* KotakBroker::map_order_type_to_kotak(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "MKT";
        case OrderType::Limit:         return "L";
        case OrderType::StopLoss:      return "SL";
        case OrderType::StopLossLimit: return "SL";
    }
    return "MKT";
}

// ============================================================================
// Common API call — Kotak trading endpoint (Sid/Auth headers, form-urlencoded)
// ============================================================================

KotakBroker::KotakApiResult KotakBroker::kotak_api_call(
        const KotakAuth& auth,
        const std::string& endpoint,
        const std::string& method,
        const std::string& payload) const {

    const std::string url = auth.trading_base_url + endpoint;
    Headers headers = {
        {"accept",       "application/json"},
        {"Sid",          auth.trading_sid},
        {"Auth",         auth.trading_token},
        {"neo-fin-key",  "neotradeapi"}
    };

    if (!payload.empty()) {
        headers["Content-Type"] = "application/x-www-form-urlencoded";
    }

    auto& client = HttpClient::instance();
    HttpResponse resp;

    if (method == "POST") {
        resp = client.post(url, payload, headers);
    } else if (method == "PUT") {
        resp = client.put(url, payload, headers);
    } else if (method == "DELETE") {
        resp = client.del(url, payload, headers);
    } else {
        resp = client.get(url, headers);
    }

    if (!resp.success) {
        return {false, json(), resp.error};
    }

    const auto body = resp.json_body();
    return {true, body, ""};
}

// ============================================================================
// Quotes API call — Kotak uses Authorization header, JSON content
// ============================================================================

KotakBroker::KotakApiResult KotakBroker::kotak_quotes_call(
        const KotakAuth& auth,
        const std::string& endpoint) const {

    const std::string url = auth.trading_base_url + endpoint;
    Headers headers = {
        {"Authorization", auth.access_token},
        {"Content-Type",  "application/json"}
    };

    auto& client = HttpClient::instance();
    auto resp = client.get(url, headers);

    if (!resp.success) {
        return {false, json(), resp.error};
    }

    return {true, resp.json_body(), ""};
}

// ============================================================================
// Token Exchange — 2-step: TOTP login + MPIN validate
// api_key = access_token (consumer key), api_secret = "ucc:mobile:totp:mpin"
// ============================================================================

TokenExchangeResponse KotakBroker::exchange_token(const std::string& api_key,
                                                     const std::string& api_secret,
                                                     const std::string& /*auth_code*/) {
    LOG_INFO("KotakBroker", "Starting 2-step authentication");

    // Parse api_secret: "ucc:mobile_number:totp:mpin"
    std::vector<std::string> parts;
    std::istringstream stream(api_secret);
    std::string part;
    while (std::getline(stream, part, ':')) {
        parts.push_back(part);
    }

    if (parts.size() < 4) {
        return {false, "", "", "Invalid api_secret format. Expected: ucc:mobile:totp:mpin"};
    }

    const std::string& ucc = parts[0];
    std::string mobile = parts[1];
    const std::string& totp = parts[2];
    const std::string& mpin = parts[3];

    // Normalize mobile number
    if (!mobile.empty() && mobile[0] != '+') {
        if (mobile.size() == 12 && mobile.substr(0, 2) == "91") {
            mobile = "+" + mobile;
        } else {
            mobile = "+91" + mobile;
        }
    }

    auto& client = HttpClient::instance();

    // --- Step 1: TOTP Login ---
    json totp_payload = {
        {"mobileNumber", mobile},
        {"ucc", ucc},
        {"totp", totp}
    };

    Headers step1_headers = {
        {"Authorization", api_key},
        {"neo-fin-key",   "neotradeapi"},
        {"Content-Type",  "application/json"}
    };

    auto step1_resp = client.post_json(
        std::string(base_url()) + "/login/1.0/tradeApiLogin",
        totp_payload, step1_headers);

    if (!step1_resp.success) {
        return {false, "", "", step1_resp.error};
    }

    const auto step1_body = step1_resp.json_body();
    const auto step1_data = step1_body.value("data", json::object());

    if (step1_data.value("status", "") != "success") {
        const auto err = step1_body.value("errMsg",
            step1_body.value("message", "TOTP login failed"));
        return {false, "", "", err};
    }

    const std::string view_token = step1_data.value("token", "");
    const std::string view_sid = step1_data.value("sid", "");

    // --- Step 2: MPIN Validate ---
    json mpin_payload = {{"mpin", mpin}};

    Headers step2_headers = {
        {"Authorization", api_key},
        {"neo-fin-key",   "neotradeapi"},
        {"sid",           view_sid},
        {"Auth",          view_token},
        {"Content-Type",  "application/json"}
    };

    auto step2_resp = client.post_json(
        std::string(base_url()) + "/login/1.0/tradeApiValidate",
        mpin_payload, step2_headers);

    if (!step2_resp.success) {
        return {false, "", "", step2_resp.error};
    }

    const auto step2_body = step2_resp.json_body();
    const auto step2_data = step2_body.value("data", json::object());

    if (step2_data.value("status", "") != "success") {
        const auto err = step2_body.value("errMsg",
            step2_body.value("message", "MPIN validation failed"));
        return {false, "", "", err};
    }

    const std::string trading_token = step2_data.value("token", "");
    const std::string trading_sid = step2_data.value("sid", "");
    const std::string trading_base_url = step2_data.value("baseUrl", "");

    // Compose auth string: trading_token:::trading_sid:::base_url:::access_token
    const std::string auth_string = trading_token + ":::" + trading_sid +
        ":::" + trading_base_url + ":::" + api_key;

    TokenExchangeResponse result;
    result.success = true;
    result.access_token = auth_string;
    result.user_id = ucc;
    return result;
}

// ============================================================================
// Validate Token — GET /quick/user/orders
// ============================================================================

ApiResponse<bool> KotakBroker::validate_token(const BrokerCredentials& creds) {
    LOG_INFO("KotakBroker", "Validating auth token");

    const int64_t ts = kotak_now_ts();
    const auto auth = parse_auth_token(creds.access_token);
    if (!auth.valid) {
        return {false, false, "Invalid auth token format", ts};
    }

    const auto res = kotak_api_call(auth, "/quick/user/orders", "GET");
    if (res.success) {
        const bool is_valid = res.body.value("stat", "") == "Ok" || res.body.contains("data");
        return {true, is_valid, "", ts};
    }
    return {true, false, "", ts};
}

// ============================================================================
// Place Order — POST /quick/order/rule/ms/place (form-urlencoded jData)
// ============================================================================

OrderPlaceResponse KotakBroker::place_order(const BrokerCredentials& creds,
                                              const UnifiedOrder& order) {
    LOG_INFO("KotakBroker", "Placing order: %s", order.symbol.c_str());

    const auto auth = parse_auth_token(creds.access_token);
    if (!auth.valid) {
        return {false, "", "Invalid auth token format"};
    }

    const std::string product = [&]() -> std::string {
        switch (order.product_type) {
            case ProductType::Intraday:  return "MIS";
            case ProductType::Delivery:  return "CNC";
            case ProductType::Margin:    return "NRML";
            default:                     return "MIS";
        }
    }();

    json order_data = {
        {"am",  "NO"},
        {"dq",  "0"},
        {"bc",  "1"},
        {"es",  map_exchange_to_kotak(order.exchange)},
        {"mp",  "0"},
        {"pc",  product},
        {"pf",  "N"},
        {"pr",  std::to_string(order.price)},
        {"pt",  map_order_type_to_kotak(order.order_type)},
        {"qt",  std::to_string(static_cast<int>(order.quantity))},
        {"rt",  "DAY"},
        {"tp",  std::to_string(order.stop_price)},
        {"ts",  order.symbol},
        {"tt",  order.side == OrderSide::Buy ? "B" : "S"}
    };

    const auto res = kotak_api_call(auth, "/quick/order/rule/ms/place", "POST",
                                     encode_jdata(order_data));

    OrderPlaceResponse result;
    if (!res.success) { result.error = res.error; return result; }

    if (res.body.value("stat", "") == "Ok") {
        result.success = true;
        result.order_id = res.body.value("nOrdNo", "");
    } else {
        result.error = res.body.value("emsg", "Order placement failed");
    }
    return result;
}

// ============================================================================
// Modify Order — POST /quick/order/vr/modify
// ============================================================================

ApiResponse<json> KotakBroker::modify_order(const BrokerCredentials& creds,
                                              const std::string& order_id,
                                              const json& modifications) {
    LOG_INFO("KotakBroker", "Modifying order: %s", order_id.c_str());

    const int64_t ts = kotak_now_ts();
    const auto auth = parse_auth_token(creds.access_token);
    if (!auth.valid) {
        return {false, std::nullopt, "Invalid auth token format", ts};
    }

    json order_data = {
        {"no",  order_id},
        {"dq",  "0"},
        {"mp",  "0"},
        {"dd",  "NA"},
        {"vd",  "DAY"}
    };

    // Merge modifications — Kotak uses short field names
    if (modifications.contains("exchange"))
        order_data["es"] = map_exchange_to_kotak(modifications["exchange"].get<std::string>());
    if (modifications.contains("price"))
        order_data["pr"] = std::to_string(modifications["price"].get<double>());
    if (modifications.contains("quantity"))
        order_data["qt"] = std::to_string(modifications["quantity"].get<int>());
    if (modifications.contains("trigger_price"))
        order_data["tp"] = std::to_string(modifications["trigger_price"].get<double>());
    if (modifications.contains("order_type"))
        order_data["pt"] = modifications["order_type"].get<std::string>();
    if (modifications.contains("symbol"))
        order_data["ts"] = modifications["symbol"].get<std::string>();
    if (modifications.contains("side"))
        order_data["tt"] = modifications["side"].get<std::string>() == "BUY" ? "B" : "S";

    const auto res = kotak_api_call(auth, "/quick/order/vr/modify", "POST",
                                     encode_jdata(order_data));

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    if (res.body.value("stat", "") == "Ok") {
        return {true, json{{"order_id", res.body.value("nOrdNo", order_id)}}, "", ts};
    }
    return {false, std::nullopt, res.body.value("emsg", "Order modification failed"), ts};
}

// ============================================================================
// Cancel Order — POST /quick/order/cancel
// ============================================================================

ApiResponse<json> KotakBroker::cancel_order(const BrokerCredentials& creds,
                                              const std::string& order_id) {
    LOG_INFO("KotakBroker", "Cancelling order: %s", order_id.c_str());

    const int64_t ts = kotak_now_ts();
    const auto auth = parse_auth_token(creds.access_token);
    if (!auth.valid) {
        return {false, std::nullopt, "Invalid auth token format", ts};
    }

    json cancel_data = {{"on", order_id}, {"am", "NO"}};
    const auto res = kotak_api_call(auth, "/quick/order/cancel", "POST",
                                     encode_jdata(cancel_data));

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    if (res.body.value("stat", "") == "Ok") {
        return {true, json{{"order_id", res.body.value("nOrdNo", order_id)}}, "", ts};
    }
    return {false, std::nullopt, res.body.value("emsg", "Order cancellation failed"), ts};
}

// ============================================================================
// Get Orders — GET /quick/user/orders
// ============================================================================

ApiResponse<std::vector<BrokerOrderInfo>> KotakBroker::get_orders(const BrokerCredentials& creds) {
    LOG_INFO("KotakBroker", "Fetching order book");

    const int64_t ts = kotak_now_ts();
    const auto auth = parse_auth_token(creds.access_token);
    if (!auth.valid) {
        return {false, std::nullopt, "Invalid auth token format", ts};
    }

    const auto res = kotak_api_call(auth, "/quick/user/orders", "GET");

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    std::vector<BrokerOrderInfo> orders;
    // Kotak returns { "data": [...] } or array directly
    const auto& data = res.body.contains("data") ? res.body["data"] : res.body;
    if (data.is_array()) {
        for (const auto& o : data) {
            BrokerOrderInfo info;
            info.order_id      = o.value("nOrdNo", o.value("orderId", ""));
            info.symbol        = o.value("trdSym", o.value("symbol", ""));
            info.exchange      = o.value("exSeg", "");
            info.side          = o.value("trnsTp", "") == "B" ? "BUY" : "SELL";
            info.order_type    = o.value("prcTp", "");
            info.product_type  = o.value("prod", "");
            info.quantity      = o.value("qty", 0.0);
            info.price         = o.value("prc", 0.0);
            info.trigger_price = o.value("trgPrc", 0.0);
            info.filled_qty    = o.value("fldQty", 0.0);
            info.avg_price     = o.value("avgPrc", 0.0);
            info.status        = o.value("ordSt", "");
            info.timestamp     = o.value("ordDtTm", "");
            info.message       = o.value("rejRsn", "");
            orders.push_back(info);
        }
    }
    return {true, orders, "", ts};
}

// ============================================================================
// Get Trade Book — GET /quick/user/trades
// ============================================================================

ApiResponse<json> KotakBroker::get_trade_book(const BrokerCredentials& creds) {
    LOG_INFO("KotakBroker", "Fetching trade book");

    const int64_t ts = kotak_now_ts();
    const auto auth = parse_auth_token(creds.access_token);
    if (!auth.valid) {
        return {false, std::nullopt, "Invalid auth token format", ts};
    }

    const auto res = kotak_api_call(auth, "/quick/user/trades", "GET");

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    return {true, res.body, "", ts};
}

// ============================================================================
// Positions — GET /quick/user/positions
// ============================================================================

ApiResponse<std::vector<BrokerPosition>> KotakBroker::get_positions(const BrokerCredentials& creds) {
    LOG_INFO("KotakBroker", "Fetching positions");

    const int64_t ts = kotak_now_ts();
    const auto auth = parse_auth_token(creds.access_token);
    if (!auth.valid) {
        return {false, std::nullopt, "Invalid auth token format", ts};
    }

    const auto res = kotak_api_call(auth, "/quick/user/positions", "GET");

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    std::vector<BrokerPosition> positions;
    const auto& data = res.body.contains("data") ? res.body["data"] : res.body;
    if (data.is_array()) {
        for (const auto& p : data) {
            BrokerPosition pos;
            pos.symbol       = p.value("trdSym", p.value("symbol", ""));
            pos.exchange     = p.value("exSeg", "");
            pos.product_type = p.value("prod", "");
            pos.quantity     = p.value("netQty", p.value("flBuyQty", 0.0) - p.value("flSellQty", 0.0));
            pos.avg_price    = p.value("avgPrc", 0.0);
            pos.ltp          = p.value("ltp", 0.0);
            pos.pnl          = p.value("mtm", 0.0);
            pos.day_pnl      = p.value("urmtom", 0.0);
            pos.side         = pos.quantity >= 0 ? "buy" : "sell";
            positions.push_back(pos);
        }
    }
    return {true, positions, "", ts};
}

// ============================================================================
// Holdings — GET /portfolio/v1/holdings
// ============================================================================

ApiResponse<std::vector<BrokerHolding>> KotakBroker::get_holdings(const BrokerCredentials& creds) {
    LOG_INFO("KotakBroker", "Fetching holdings");

    const int64_t ts = kotak_now_ts();
    const auto auth = parse_auth_token(creds.access_token);
    if (!auth.valid) {
        return {false, std::nullopt, "Invalid auth token format", ts};
    }

    const auto res = kotak_api_call(auth, "/portfolio/v1/holdings", "GET");

    if (!res.success)
        return {false, std::nullopt, res.error, ts};

    std::vector<BrokerHolding> holdings;
    const auto& data = res.body.contains("data") ? res.body["data"] : res.body;
    if (data.is_array()) {
        for (const auto& h : data) {
            BrokerHolding hold;
            hold.symbol         = h.value("trdSym", h.value("symbol", ""));
            hold.exchange       = h.value("exSeg", "");
            hold.quantity       = h.value("qty", h.value("totQty", 0.0));
            hold.avg_price      = h.value("avgPrc", 0.0);
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
// Funds — POST /quick/user/limits (jData={seg:ALL,exch:ALL,prod:ALL})
// ============================================================================

ApiResponse<BrokerFunds> KotakBroker::get_funds(const BrokerCredentials& creds) {
    LOG_INFO("KotakBroker", "Fetching funds/limits");

    const int64_t ts = kotak_now_ts();
    const auto auth = parse_auth_token(creds.access_token);
    if (!auth.valid) {
        return {false, std::nullopt, "Invalid auth token format", ts};
    }

    json funds_data = {{"seg", "ALL"}, {"exch", "ALL"}, {"prod", "ALL"}};
    const auto res = kotak_api_call(auth, "/quick/user/limits", "POST",
                                     encode_jdata(funds_data));

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

    funds.available_balance = safe_double(res.body, "availCash");
    if (funds.available_balance == 0.0) {
        funds.available_balance = safe_double(res.body, "cash");
    }
    funds.used_margin   = safe_double(res.body, "marginUsed");
    funds.total_balance = safe_double(res.body, "totalBal");
    if (funds.total_balance == 0.0) {
        funds.total_balance = funds.available_balance + funds.used_margin;
    }
    funds.collateral = safe_double(res.body, "collateral");
    funds.raw_data   = res.body;
    return {true, funds, "", ts};
}

// ============================================================================
// Quotes — GET /script-details/1.0/quotes/neosymbol/{exchange|pSymbol}/all
// ============================================================================

ApiResponse<std::vector<BrokerQuote>> KotakBroker::get_quotes(
        const BrokerCredentials& creds, const std::vector<std::string>& symbols) {
    LOG_INFO("KotakBroker", "Fetching quotes for %zu symbols", symbols.size());

    const int64_t ts = kotak_now_ts();
    const auto auth = parse_auth_token(creds.access_token);
    if (!auth.valid) {
        return {false, std::nullopt, "Invalid auth token format", ts};
    }

    std::vector<BrokerQuote> quotes;
    for (const auto& symbol : symbols) {
        // Symbol format: "EXCHANGE:SYMBOL" or just "SYMBOL" (defaults to NSE)
        std::string exchange = "NSE";
        std::string sym = symbol;
        const auto colon = symbol.find(':');
        if (colon != std::string::npos) {
            exchange = symbol.substr(0, colon);
            sym = symbol.substr(colon + 1);
        }

        const std::string kotak_exchange = map_exchange_to_kotak(exchange);
        const std::string query = kotak_exchange + std::string("|") + sym;
        const std::string encoded = url_encode(query);
        const std::string endpoint = "/script-details/1.0/quotes/neosymbol/" + encoded + "/all";

        const auto res = kotak_quotes_call(auth, endpoint);
        if (!res.success) continue;

        BrokerQuote q;
        q.symbol     = symbol;
        q.ltp        = res.body.value("ltp", 0.0);
        q.open       = res.body.value("open", 0.0);
        q.high       = res.body.value("high", 0.0);
        q.low        = res.body.value("low", 0.0);
        q.close      = res.body.value("close", 0.0);
        q.volume     = res.body.value("volume", 0.0);
        q.change     = q.ltp - q.close;
        q.change_pct = q.close > 0 ? (q.change / q.close) * 100.0 : 0.0;
        q.bid        = res.body.value("bestBidPrice", 0.0);
        q.ask        = res.body.value("bestAskPrice", 0.0);
        q.timestamp  = ts;
        quotes.push_back(q);
    }
    return {true, quotes, "", ts};
}

// ============================================================================
// History — NOT SUPPORTED by Kotak Neo API
// ============================================================================

ApiResponse<std::vector<BrokerCandle>> KotakBroker::get_history(
        const BrokerCredentials& /*creds*/,
        const std::string& /*symbol*/,
        const std::string& /*interval*/,
        const std::string& /*from_date*/,
        const std::string& /*to_date*/) {
    return {false, std::nullopt, "Historical data is not supported by Kotak Neo API", kotak_now_ts()};
}

} // namespace fincept::trading
