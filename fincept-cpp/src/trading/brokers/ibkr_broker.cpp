#include "ibkr_broker.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <chrono>

namespace fincept::trading {

using namespace fincept::http;

// ============================================================================
// Helpers (broker-prefixed to avoid unity build collisions)
// ============================================================================

static int64_t ibkr_now_ts() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

// ============================================================================
// Auth headers — IBKR uses Bearer token
// ============================================================================

std::map<std::string, std::string> IBKRBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Authorization", "Bearer " + creds.access_token},
        {"Content-Type",  "application/json"},
        {"Accept",        "application/json"}
    };
}

// ============================================================================
// Mapping helpers
// ============================================================================

std::string IBKRBroker::get_account_id(const BrokerCredentials& creds) {
    // Account ID may be in user_id or parsed from additional_data
    if (!creds.user_id.empty()) return creds.user_id;
    if (!creds.additional_data.empty()) {
        try {
            auto extra = json::parse(creds.additional_data);
            if (extra.contains("account_id"))
                return extra["account_id"].get<std::string>();
        } catch (...) {}
    }
    return "";
}

const char* IBKRBroker::ibkr_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "MKT";
        case OrderType::Limit:         return "LMT";
        case OrderType::StopLoss:      return "STP";
        case OrderType::StopLossLimit: return "STP_LMT";
    }
    return "MKT";
}

const char* IBKRBroker::ibkr_side(OrderSide s) {
    return s == OrderSide::Buy ? "BUY" : "SELL";
}

const char* IBKRBroker::ibkr_period(const std::string& resolution) {
    // Period determines how far back to fetch
    if (resolution == "1m" || resolution == "5m" || resolution == "1" || resolution == "5")
        return "1d";
    if (resolution == "15m" || resolution == "30m" || resolution == "15" || resolution == "30")
        return "1w";
    if (resolution == "1h" || resolution == "60")
        return "1m";  // 1 month
    if (resolution == "D" || resolution == "day" || resolution == "1d")
        return "1y";
    if (resolution == "W" || resolution == "week" || resolution == "1w")
        return "5y";
    return "1y";
}

const char* IBKRBroker::ibkr_bar(const std::string& resolution) {
    if (resolution == "1m" || resolution == "1")   return "1min";
    if (resolution == "5m" || resolution == "5")   return "5min";
    if (resolution == "15m" || resolution == "15") return "15min";
    if (resolution == "30m" || resolution == "30") return "30min";
    if (resolution == "1h" || resolution == "60")  return "1h";
    if (resolution == "D" || resolution == "day" || resolution == "1d")  return "1d";
    if (resolution == "W" || resolution == "week" || resolution == "1w") return "1w";
    if (resolution == "M" || resolution == "month") return "1m";
    return "1d";
}

std::string IBKRBroker::extract_error(const json& body, const char* fallback) {
    if (body.contains("error") && body["error"].is_string())
        return body["error"].get<std::string>();
    if (body.contains("message") && body["message"].is_string())
        return body["message"].get<std::string>();
    return fallback;
}

// ============================================================================
// Token Exchange — IBKR session-based auth
// Validates by checking /iserver/auth/status
// ============================================================================

TokenExchangeResponse IBKRBroker::exchange_token(const std::string& api_key,
                                                  const std::string& /*api_secret*/,
                                                  const std::string& auth_code) {
    auto& client = HttpClient::instance();

    // api_key is used as Bearer token; auth_code may contain account_id
    Headers headers = {
        {"Authorization", "Bearer " + api_key},
        {"Content-Type",  "application/json"},
        {"Accept",        "application/json"}
    };

    // Check session status
    auto resp = client.get(std::string(base_url()) + "/iserver/auth/status", headers);

    if (!resp.success) {
        return {false, "", "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();
    if (body.is_null()) {
        return {false, "", "", "Invalid JSON response"};
    }

    const bool authenticated = body.value("authenticated", false);
    if (authenticated) {
        // Tickle to keep session alive
        client.post(std::string(base_url()) + "/tickle", "", headers);

        // Get accounts to find account_id
        auto acct_resp = client.get(std::string(base_url()) + "/iserver/accounts", headers);
        std::string account_id = auth_code;  // user may pass account_id as auth_code
        if (acct_resp.success) {
            const auto acct_body = acct_resp.json_body();
            if (acct_body.contains("accounts") && acct_body["accounts"].is_array() &&
                !acct_body["accounts"].empty()) {
                account_id = acct_body["accounts"][0].get<std::string>();
            }
        }

        return {true, api_key, account_id, ""};
    }

    return {false, "", "", "Session not authenticated. Start the IBKR Gateway first."};
}

// ============================================================================
// Orders
// ============================================================================

OrderPlaceResponse IBKRBroker::place_order(const BrokerCredentials& creds,
                                            const UnifiedOrder& order) {
    auto& client = HttpClient::instance();
    const std::string account_id = get_account_id(creds);

    if (account_id.empty()) {
        return {false, "", "No account_id configured for IBKR"};
    }

    // IBKR expects orders in an array
    json order_obj = {
        {"conid",     0},  // Consumer must provide conid via symbol or additional_data
        {"orderType", ibkr_order_type(order.order_type)},
        {"side",      ibkr_side(order.side)},
        {"quantity",  static_cast<int>(order.quantity)},
        {"tif",       "DAY"}
    };

    // Try to parse conid from symbol (numeric) or use as-is
    try {
        order_obj["conid"] = std::stoi(order.symbol);
    } catch (...) {
        // Symbol is not numeric — pass as ticker for the consumer to resolve
        order_obj["ticker"] = order.symbol;
    }

    if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit) {
        if (order.price > 0.0) order_obj["price"] = order.price;
    }
    if (order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit) {
        if (order.stop_price > 0.0) order_obj["auxPrice"] = order.stop_price;
    }

    json payload = {{"orders", json::array({order_obj})}};

    const std::string url = std::string(base_url()) +
        "/iserver/account/" + account_id + "/orders";

    auto resp = client.post_json(url, payload, auth_headers(creds));

    if (!resp.success) {
        return {false, "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();

    // IBKR may return an array of responses
    if (body.is_array() && !body.empty()) {
        const auto& first = body[0];
        if (first.contains("order_id")) {
            return {true, std::to_string(first["order_id"].get<int>()), ""};
        }
        if (first.contains("id")) {
            return {true, first["id"].get<std::string>(), ""};
        }
        // May need order confirmation reply
        if (first.contains("id") && first.contains("message")) {
            return {false, "", first["message"].get<std::string>()};
        }
    }

    return {false, "", extract_error(body, "Order placement failed")};
}

ApiResponse<json> IBKRBroker::modify_order(const BrokerCredentials& creds,
                                            const std::string& order_id,
                                            const json& modifications) {
    const int64_t ts = ibkr_now_ts();
    auto& client = HttpClient::instance();
    const std::string account_id = get_account_id(creds);

    if (account_id.empty()) {
        return {false, std::nullopt, "No account_id configured", ts};
    }

    const std::string url = std::string(base_url()) +
        "/iserver/account/" + account_id + "/order/" + order_id;

    auto resp = client.post_json(url, modifications, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (body.is_array() && !body.empty() && body[0].contains("order_id")) {
        return {true, body, "", ts};
    }
    return {false, std::nullopt, extract_error(body, "Order modification failed"), ts};
}

ApiResponse<json> IBKRBroker::cancel_order(const BrokerCredentials& creds,
                                            const std::string& order_id) {
    const int64_t ts = ibkr_now_ts();
    auto& client = HttpClient::instance();
    const std::string account_id = get_account_id(creds);

    if (account_id.empty()) {
        return {false, std::nullopt, "No account_id configured", ts};
    }

    const std::string url = std::string(base_url()) +
        "/iserver/account/" + account_id + "/order/" + order_id;

    auto resp = client.del(url, "", auth_headers(creds));

    if (resp.success) {
        return {true, json{{"order_id", order_id}, {"status", "cancelled"}}, "", ts};
    }

    const auto body = resp.json_body();
    return {false, std::nullopt, extract_error(body, "Order cancellation failed"), ts};
}

ApiResponse<std::vector<BrokerOrderInfo>> IBKRBroker::get_orders(const BrokerCredentials& creds) {
    const int64_t ts = ibkr_now_ts();
    auto& client = HttpClient::instance();

    const std::string url = std::string(base_url()) + "/iserver/account/orders";
    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerOrderInfo> orders;

    // Response: {"orders": [...]} or direct array
    const auto& order_arr = body.contains("orders") ? body["orders"] : body;

    if (order_arr.is_array()) {
        for (const auto& o : order_arr) {
            BrokerOrderInfo info;
            info.order_id  = std::to_string(o.value("orderId", 0));
            info.symbol    = o.value("ticker", o.value("symbol", ""));
            info.side      = o.value("side", "");
            info.order_type = o.value("orderType", "");
            info.status    = o.value("status", "");
            info.quantity  = o.value("totalSize", 0.0);
            info.filled_qty = o.value("filledQuantity", 0.0);
            info.price     = o.value("price", 0.0);
            info.avg_price = o.value("avgPrice", 0.0);
            orders.push_back(std::move(info));
        }
    }

    return {true, std::move(orders), "", ts};
}

ApiResponse<json> IBKRBroker::get_trade_book(const BrokerCredentials& creds) {
    const int64_t ts = ibkr_now_ts();
    auto& client = HttpClient::instance();

    // IBKR trades endpoint
    const std::string url = std::string(base_url()) + "/iserver/account/trades";
    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    return {true, resp.json_body(), "", ts};
}

// ============================================================================
// Portfolio
// ============================================================================

ApiResponse<std::vector<BrokerPosition>> IBKRBroker::get_positions(const BrokerCredentials& creds) {
    const int64_t ts = ibkr_now_ts();
    auto& client = HttpClient::instance();
    const std::string account_id = get_account_id(creds);

    if (account_id.empty()) {
        return {false, std::nullopt, "No account_id configured", ts};
    }

    const std::string url = std::string(base_url()) +
        "/portfolio/" + account_id + "/positions/0";

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerPosition> positions;

    if (body.is_array()) {
        for (const auto& p : body) {
            BrokerPosition pos;
            pos.symbol    = p.value("ticker", p.value("contractDesc", ""));
            pos.exchange  = p.value("listingExchange", "");
            pos.quantity  = p.value("position", 0.0);
            pos.avg_price = p.value("avgCost", 0.0);
            pos.ltp       = p.value("mktPrice", 0.0);
            pos.pnl       = p.value("unrealizedPnl", 0.0);
            pos.day_pnl   = p.value("realizedPnl", 0.0);
            pos.side      = (pos.quantity >= 0) ? "buy" : "sell";
            positions.push_back(std::move(pos));
        }
    }

    return {true, std::move(positions), "", ts};
}

ApiResponse<std::vector<BrokerHolding>> IBKRBroker::get_holdings(const BrokerCredentials& creds) {
    // IBKR positions serve as holdings
    const int64_t ts = ibkr_now_ts();
    auto& client = HttpClient::instance();
    const std::string account_id = get_account_id(creds);

    if (account_id.empty()) {
        return {false, std::nullopt, "No account_id configured", ts};
    }

    const std::string url = std::string(base_url()) +
        "/portfolio/" + account_id + "/positions/0";

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerHolding> holdings;

    if (body.is_array()) {
        for (const auto& h : body) {
            BrokerHolding hld;
            hld.symbol        = h.value("ticker", h.value("contractDesc", ""));
            hld.exchange      = h.value("listingExchange", "");
            hld.quantity      = h.value("position", 0.0);
            hld.avg_price     = h.value("avgCost", 0.0);
            hld.ltp           = h.value("mktPrice", 0.0);
            hld.pnl           = h.value("unrealizedPnl", 0.0);
            hld.current_value = h.value("mktValue", 0.0);
            hld.invested_value = hld.avg_price * std::abs(hld.quantity);
            hld.pnl_pct = (hld.invested_value > 0.0)
                ? (hld.pnl / hld.invested_value) * 100.0 : 0.0;
            holdings.push_back(std::move(hld));
        }
    }

    return {true, std::move(holdings), "", ts};
}

ApiResponse<BrokerFunds> IBKRBroker::get_funds(const BrokerCredentials& creds) {
    const int64_t ts = ibkr_now_ts();
    auto& client = HttpClient::instance();
    const std::string account_id = get_account_id(creds);

    if (account_id.empty()) {
        return {false, std::nullopt, "No account_id configured", ts};
    }

    const std::string url = std::string(base_url()) +
        "/portfolio/" + account_id + "/summary";

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    BrokerFunds funds;

    // IBKR summary has nested objects with "amount" field
    auto get_amount = [&](const std::string& key) -> double {
        if (body.contains(key) && body[key].is_object())
            return body[key].value("amount", 0.0);
        return body.value(key, 0.0);
    };

    funds.available_balance = get_amount("availablefunds");
    funds.total_balance     = get_amount("netliquidation");
    funds.used_margin       = get_amount("initmarginreq");
    funds.collateral        = get_amount("buyingpower");
    funds.raw_data          = body;

    return {true, std::move(funds), "", ts};
}

// ============================================================================
// Market Data
// ============================================================================

ApiResponse<std::vector<BrokerQuote>> IBKRBroker::get_quotes(const BrokerCredentials& creds,
                                                              const std::vector<std::string>& symbols) {
    const int64_t ts = ibkr_now_ts();
    auto& client = HttpClient::instance();

    std::vector<BrokerQuote> quotes;

    // Build comma-separated conid list
    std::string conids;
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (i > 0) conids += ",";
        conids += symbols[i];
    }

    // Fields: 31=Last, 70=High, 71=Low, 84=Bid, 85=Ask, 86=Open, 87=Close, 7762=Volume
    const std::string url = std::string(base_url()) +
        "/iserver/marketdata/snapshot?conids=" + conids +
        "&fields=31,70,71,84,85,86,87,7762";

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (body.is_array()) {
        for (const auto& snap : body) {
            BrokerQuote q;
            q.symbol = std::to_string(snap.value("conid", 0));
            q.ltp    = snap.value("31", 0.0);
            q.high   = snap.value("70", 0.0);
            q.low    = snap.value("71", 0.0);
            q.bid    = snap.value("84", 0.0);
            q.ask    = snap.value("85", 0.0);
            q.open   = snap.value("86", 0.0);
            q.close  = snap.value("87", 0.0);
            q.volume = snap.value("7762", 0.0);

            if (q.close > 0.0 && q.ltp > 0.0) {
                q.change     = q.ltp - q.close;
                q.change_pct = (q.change / q.close) * 100.0;
            }

            quotes.push_back(std::move(q));
        }
    }

    return {true, std::move(quotes), "", ts};
}

ApiResponse<std::vector<BrokerCandle>> IBKRBroker::get_history(const BrokerCredentials& creds,
                                                                const std::string& symbol,
                                                                const std::string& resolution,
                                                                const std::string& /*from_date*/,
                                                                const std::string& /*to_date*/) {
    const int64_t ts = ibkr_now_ts();
    auto& client = HttpClient::instance();

    // IBKR uses conid + period + bar (not from/to dates)
    const std::string url = std::string(base_url()) +
        "/iserver/marketdata/history?conid=" + symbol +
        "&period=" + ibkr_period(resolution) +
        "&bar=" + ibkr_bar(resolution);

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerCandle> candles;

    // Response: {"data": [{o, h, l, c, v, t}, ...]}
    if (body.contains("data") && body["data"].is_array()) {
        for (const auto& bar : body["data"]) {
            BrokerCandle candle;
            candle.timestamp = bar.value("t", static_cast<int64_t>(0));
            // IBKR timestamps are in milliseconds
            if (candle.timestamp > 4102444800000LL) {
                candle.timestamp /= 1000;
            }
            candle.open   = bar.value("o", 0.0);
            candle.high   = bar.value("h", 0.0);
            candle.low    = bar.value("l", 0.0);
            candle.close  = bar.value("c", 0.0);
            candle.volume = bar.value("v", 0.0);
            candles.push_back(candle);
        }
    }

    return {true, std::move(candles), "", ts};
}

} // namespace fincept::trading
