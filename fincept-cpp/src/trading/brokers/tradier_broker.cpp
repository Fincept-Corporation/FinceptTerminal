#include "tradier_broker.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <chrono>
#include <sstream>

namespace fincept::trading {

using namespace fincept::http;

// ============================================================================
// Helpers (broker-prefixed to avoid unity build collisions)
// ============================================================================

static int64_t tradier_now_ts() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

// ============================================================================
// Auth headers — Tradier uses Bearer token
// ============================================================================

std::map<std::string, std::string> TradierBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Authorization", "Bearer " + creds.access_token},
        {"Accept",        "application/json"}
    };
}

// ============================================================================
// Mapping helpers
// ============================================================================

std::string TradierBroker::get_account_id(const BrokerCredentials& creds) {
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

const char* TradierBroker::tradier_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "market";
        case OrderType::Limit:         return "limit";
        case OrderType::StopLoss:      return "stop";
        case OrderType::StopLossLimit: return "stop_limit";
    }
    return "market";
}

const char* TradierBroker::tradier_side(OrderSide s) {
    return s == OrderSide::Buy ? "buy" : "sell";
}

const char* TradierBroker::tradier_interval(const std::string& resolution) {
    if (resolution == "D" || resolution == "day" || resolution == "1d")   return "daily";
    if (resolution == "W" || resolution == "week" || resolution == "1w")  return "weekly";
    if (resolution == "M" || resolution == "month") return "monthly";
    return "daily";
}

std::string TradierBroker::extract_error(const json& body, const char* fallback) {
    // Tradier errors: fault.faultstring or errors.error
    if (body.contains("fault") && body["fault"].is_object()) {
        return body["fault"].value("faultstring", fallback);
    }
    if (body.contains("errors") && body["errors"].is_object()) {
        const auto& errs = body["errors"];
        if (errs.contains("error") && errs["error"].is_string())
            return errs["error"].get<std::string>();
        if (errs.contains("error") && errs["error"].is_array() && !errs["error"].empty())
            return errs["error"][0].get<std::string>();
    }
    if (body.contains("error") && body["error"].is_string())
        return body["error"].get<std::string>();
    return fallback;
}

std::string TradierBroker::url_encode_form(const std::map<std::string, std::string>& params) {
    std::string result;
    for (const auto& [key, val] : params) {
        if (!result.empty()) result += "&";
        result += key + "=" + val;
    }
    return result;
}

// ============================================================================
// Token Exchange — Tradier validates via /user/profile
// ============================================================================

TokenExchangeResponse TradierBroker::exchange_token(const std::string& api_key,
                                                      const std::string& /*api_secret*/,
                                                      const std::string& auth_code) {
    auto& client = HttpClient::instance();

    // api_key is the Bearer token for Tradier
    Headers headers = {
        {"Authorization", "Bearer " + api_key},
        {"Accept",        "application/json"}
    };

    auto resp = client.get(std::string(base_url()) + "/user/profile", headers);

    if (!resp.success) {
        return {false, "", "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();
    if (body.is_null()) {
        return {false, "", "", "Invalid JSON response"};
    }

    // Extract account_id from profile
    std::string account_id = auth_code;  // user may pass account_id as auth_code
    if (body.contains("profile") && body["profile"].is_object()) {
        const auto& profile = body["profile"];
        if (profile.contains("account") && profile["account"].is_object()) {
            account_id = profile["account"].value("account_number", account_id);
        } else if (profile.contains("account") && profile["account"].is_array() &&
                   !profile["account"].empty()) {
            account_id = profile["account"][0].value("account_number", account_id);
        }
        return {true, api_key, account_id, ""};
    }

    return {false, "", "", extract_error(body, "Authentication failed")};
}

// ============================================================================
// Orders (form-urlencoded)
// ============================================================================

OrderPlaceResponse TradierBroker::place_order(const BrokerCredentials& creds,
                                                const UnifiedOrder& order) {
    auto& client = HttpClient::instance();
    const std::string account_id = get_account_id(creds);

    if (account_id.empty()) {
        return {false, "", "No account_id configured for Tradier"};
    }

    std::map<std::string, std::string> form_params;
    form_params["class"]    = "equity";
    form_params["symbol"]   = order.symbol;
    form_params["side"]     = tradier_side(order.side);
    form_params["quantity"] = std::to_string(static_cast<int>(order.quantity));
    form_params["type"]     = tradier_order_type(order.order_type);
    form_params["duration"] = "day";

    if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit) {
        if (order.price > 0.0) form_params["price"] = std::to_string(order.price);
    }
    if (order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit) {
        if (order.stop_price > 0.0) form_params["stop"] = std::to_string(order.stop_price);
    }

    const std::string url = std::string(base_url()) +
        "/accounts/" + account_id + "/orders";

    Headers hdrs = auth_headers(creds);
    hdrs["Content-Type"] = "application/x-www-form-urlencoded";

    auto resp = client.post(url, url_encode_form(form_params), hdrs);

    if (!resp.success) {
        return {false, "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();
    if (body.contains("order") && body["order"].is_object()) {
        const std::string oid = std::to_string(body["order"].value("id", 0));
        return {true, oid, ""};
    }

    return {false, "", extract_error(body, "Order placement failed")};
}

ApiResponse<json> TradierBroker::modify_order(const BrokerCredentials& creds,
                                                const std::string& order_id,
                                                const json& modifications) {
    const int64_t ts = tradier_now_ts();
    auto& client = HttpClient::instance();
    const std::string account_id = get_account_id(creds);

    if (account_id.empty()) {
        return {false, std::nullopt, "No account_id configured", ts};
    }

    // Build form params from modifications
    std::map<std::string, std::string> form_params;
    for (const auto& [key, val] : modifications.items()) {
        if (val.is_string())
            form_params[key] = val.get<std::string>();
        else if (val.is_number())
            form_params[key] = std::to_string(val.get<double>());
    }

    const std::string url = std::string(base_url()) +
        "/accounts/" + account_id + "/orders/" + order_id;

    Headers hdrs = auth_headers(creds);
    hdrs["Content-Type"] = "application/x-www-form-urlencoded";

    auto resp = client.put(url, url_encode_form(form_params), hdrs);

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (body.contains("order") && body["order"].is_object()) {
        return {true, body, "", ts};
    }
    return {false, std::nullopt, extract_error(body, "Order modification failed"), ts};
}

ApiResponse<json> TradierBroker::cancel_order(const BrokerCredentials& creds,
                                                const std::string& order_id) {
    const int64_t ts = tradier_now_ts();
    auto& client = HttpClient::instance();
    const std::string account_id = get_account_id(creds);

    if (account_id.empty()) {
        return {false, std::nullopt, "No account_id configured", ts};
    }

    const std::string url = std::string(base_url()) +
        "/accounts/" + account_id + "/orders/" + order_id;

    auto resp = client.del(url, "", auth_headers(creds));

    if (resp.success) {
        return {true, json{{"order_id", order_id}, {"status", "cancelled"}}, "", ts};
    }

    const auto body = resp.json_body();
    return {false, std::nullopt, extract_error(body, "Order cancellation failed"), ts};
}

ApiResponse<std::vector<BrokerOrderInfo>> TradierBroker::get_orders(const BrokerCredentials& creds) {
    const int64_t ts = tradier_now_ts();
    auto& client = HttpClient::instance();
    const std::string account_id = get_account_id(creds);

    if (account_id.empty()) {
        return {false, std::nullopt, "No account_id configured", ts};
    }

    const std::string url = std::string(base_url()) +
        "/accounts/" + account_id + "/orders";

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerOrderInfo> orders;

    // Response: {"orders": {"order": [...]}} or {"orders": {"order": {...}}}
    if (body.contains("orders") && body["orders"].is_object() &&
        body["orders"].contains("order")) {
        const auto& order_data = body["orders"]["order"];

        auto parse_order = [](const json& o) -> BrokerOrderInfo {
            BrokerOrderInfo info;
            info.order_id   = std::to_string(o.value("id", 0));
            info.symbol     = o.value("symbol", "");
            info.side       = o.value("side", "");
            info.order_type = o.value("type", "");
            info.status     = o.value("status", "");
            info.quantity   = o.value("quantity", 0.0);
            info.filled_qty = o.value("exec_quantity", 0.0);
            info.price      = o.value("price", 0.0);
            info.trigger_price = o.value("stop_price", 0.0);
            info.avg_price  = o.value("avg_fill_price", 0.0);
            info.timestamp  = o.value("create_date", "");
            return info;
        };

        if (order_data.is_array()) {
            for (const auto& o : order_data) {
                orders.push_back(parse_order(o));
            }
        } else if (order_data.is_object()) {
            orders.push_back(parse_order(order_data));
        }
    }

    return {true, std::move(orders), "", ts};
}

ApiResponse<json> TradierBroker::get_trade_book(const BrokerCredentials& creds) {
    const int64_t ts = tradier_now_ts();
    auto& client = HttpClient::instance();
    const std::string account_id = get_account_id(creds);

    if (account_id.empty()) {
        return {false, std::nullopt, "No account_id configured", ts};
    }

    // Tradier history endpoint serves as trade book
    const std::string url = std::string(base_url()) +
        "/accounts/" + account_id + "/history";

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    return {true, resp.json_body(), "", ts};
}

// ============================================================================
// Portfolio
// ============================================================================

ApiResponse<std::vector<BrokerPosition>> TradierBroker::get_positions(const BrokerCredentials& creds) {
    const int64_t ts = tradier_now_ts();
    auto& client = HttpClient::instance();
    const std::string account_id = get_account_id(creds);

    if (account_id.empty()) {
        return {false, std::nullopt, "No account_id configured", ts};
    }

    const std::string url = std::string(base_url()) +
        "/accounts/" + account_id + "/positions";

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerPosition> positions;

    // Response: {"positions": {"position": [...]}} or single object
    if (body.contains("positions") && body["positions"].is_object() &&
        body["positions"].contains("position")) {
        const auto& pos_data = body["positions"]["position"];

        auto parse_pos = [](const json& p) -> BrokerPosition {
            BrokerPosition pos;
            pos.symbol    = p.value("symbol", "");
            pos.exchange  = "US";
            pos.quantity  = p.value("quantity", 0.0);
            pos.avg_price = p.value("cost_basis", 0.0);
            // cost_basis is total cost, derive avg price
            if (std::abs(pos.quantity) > 0.0) {
                pos.avg_price = pos.avg_price / std::abs(pos.quantity);
            }
            pos.side = (pos.quantity >= 0) ? "buy" : "sell";
            return pos;
        };

        if (pos_data.is_array()) {
            for (const auto& p : pos_data) {
                positions.push_back(parse_pos(p));
            }
        } else if (pos_data.is_object()) {
            positions.push_back(parse_pos(pos_data));
        }
    }

    return {true, std::move(positions), "", ts};
}

ApiResponse<std::vector<BrokerHolding>> TradierBroker::get_holdings(const BrokerCredentials& creds) {
    // Tradier positions serve as holdings
    const int64_t ts = tradier_now_ts();
    auto& client = HttpClient::instance();
    const std::string account_id = get_account_id(creds);

    if (account_id.empty()) {
        return {false, std::nullopt, "No account_id configured", ts};
    }

    const std::string url = std::string(base_url()) +
        "/accounts/" + account_id + "/positions";

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerHolding> holdings;

    if (body.contains("positions") && body["positions"].is_object() &&
        body["positions"].contains("position")) {
        const auto& pos_data = body["positions"]["position"];

        auto parse_hld = [](const json& h) -> BrokerHolding {
            BrokerHolding hld;
            hld.symbol         = h.value("symbol", "");
            hld.exchange       = "US";
            hld.quantity       = h.value("quantity", 0.0);
            hld.invested_value = h.value("cost_basis", 0.0);
            if (std::abs(hld.quantity) > 0.0) {
                hld.avg_price = hld.invested_value / std::abs(hld.quantity);
            }
            return hld;
        };

        if (pos_data.is_array()) {
            for (const auto& h : pos_data) {
                holdings.push_back(parse_hld(h));
            }
        } else if (pos_data.is_object()) {
            holdings.push_back(parse_hld(pos_data));
        }
    }

    return {true, std::move(holdings), "", ts};
}

ApiResponse<BrokerFunds> TradierBroker::get_funds(const BrokerCredentials& creds) {
    const int64_t ts = tradier_now_ts();
    auto& client = HttpClient::instance();
    const std::string account_id = get_account_id(creds);

    if (account_id.empty()) {
        return {false, std::nullopt, "No account_id configured", ts};
    }

    const std::string url = std::string(base_url()) +
        "/accounts/" + account_id + "/balances";

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    BrokerFunds funds;

    // Response: {"balances": {...}}
    if (body.contains("balances") && body["balances"].is_object()) {
        const auto& bal = body["balances"];
        funds.total_balance     = bal.value("total_equity", 0.0);
        funds.available_balance = bal.value("total_cash", bal.value("cash_available", 0.0));
        funds.used_margin       = bal.value("margin_used", 0.0);
        funds.collateral        = bal.value("stock_buying_power",
                                    bal.value("option_buying_power", 0.0));

        // Check for margin-specific sub-object
        if (bal.contains("margin") && bal["margin"].is_object()) {
            const auto& margin = bal["margin"];
            funds.available_balance = margin.value("cash", funds.available_balance);
            funds.used_margin       = margin.value("initial_margin", funds.used_margin);
            funds.collateral        = margin.value("stock_buying_power", funds.collateral);
        }
    }

    funds.raw_data = body;
    return {true, std::move(funds), "", ts};
}

// ============================================================================
// Market Data
// ============================================================================

ApiResponse<std::vector<BrokerQuote>> TradierBroker::get_quotes(const BrokerCredentials& creds,
                                                                  const std::vector<std::string>& symbols) {
    const int64_t ts = tradier_now_ts();
    auto& client = HttpClient::instance();

    // Build comma-separated symbol list
    std::string sym_list;
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (i > 0) sym_list += ",";
        sym_list += symbols[i];
    }

    const std::string url = std::string(base_url()) +
        "/markets/quotes?symbols=" + sym_list;

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerQuote> quotes;

    // Response: {"quotes": {"quote": [...]}} or {"quotes": {"quote": {...}}}
    if (body.contains("quotes") && body["quotes"].is_object() &&
        body["quotes"].contains("quote")) {
        const auto& quote_data = body["quotes"]["quote"];

        auto parse_quote = [](const json& q) -> BrokerQuote {
            BrokerQuote quote;
            quote.symbol     = q.value("symbol", "");
            quote.ltp        = q.value("last", 0.0);
            quote.open       = q.value("open", 0.0);
            quote.high       = q.value("high", 0.0);
            quote.low        = q.value("low", 0.0);
            quote.close      = q.value("close", q.value("prevclose", 0.0));
            quote.volume     = q.value("volume", 0.0);
            quote.change     = q.value("change", 0.0);
            quote.change_pct = q.value("change_percentage", 0.0);
            quote.bid        = q.value("bid", 0.0);
            quote.ask        = q.value("ask", 0.0);
            return quote;
        };

        if (quote_data.is_array()) {
            for (const auto& q : quote_data) {
                quotes.push_back(parse_quote(q));
            }
        } else if (quote_data.is_object()) {
            quotes.push_back(parse_quote(quote_data));
        }
    }

    return {true, std::move(quotes), "", ts};
}

ApiResponse<std::vector<BrokerCandle>> TradierBroker::get_history(const BrokerCredentials& creds,
                                                                    const std::string& symbol,
                                                                    const std::string& resolution,
                                                                    const std::string& from_date,
                                                                    const std::string& to_date) {
    const int64_t ts = tradier_now_ts();
    auto& client = HttpClient::instance();

    const std::string url = std::string(base_url()) +
        "/markets/history?symbol=" + symbol +
        "&interval=" + tradier_interval(resolution) +
        "&start=" + from_date +
        "&end=" + to_date;

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerCandle> candles;

    // Response: {"history": {"day": [...]}}
    if (body.contains("history") && body["history"].is_object() &&
        body["history"].contains("day") && body["history"]["day"].is_array()) {
        for (const auto& day : body["history"]["day"]) {
            BrokerCandle candle;
            candle.timestamp = 0;  // date string; consumer should parse
            candle.open      = day.value("open", 0.0);
            candle.high      = day.value("high", 0.0);
            candle.low       = day.value("low", 0.0);
            candle.close     = day.value("close", 0.0);
            candle.volume    = day.value("volume", 0.0);
            candles.push_back(candle);
        }
    }

    return {true, std::move(candles), "", ts};
}

} // namespace fincept::trading
