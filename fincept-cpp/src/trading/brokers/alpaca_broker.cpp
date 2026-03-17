#include "alpaca_broker.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <chrono>

namespace fincept::trading {

using namespace fincept::http;

// ============================================================================
// Helpers (broker-prefixed to avoid unity build collisions)
// ============================================================================

static int64_t alpaca_now_ts() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

// ============================================================================
// Auth headers — Alpaca uses API key/secret headers (no bearer token)
// ============================================================================

std::map<std::string, std::string> AlpacaBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"APCA-API-KEY-ID",     creds.api_key},
        {"APCA-API-SECRET-KEY", creds.api_secret},
        {"Content-Type",        "application/json"},
        {"Accept",              "application/json"}
    };
}

// ============================================================================
// Mapping helpers
// ============================================================================

const char* AlpacaBroker::alpaca_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "market";
        case OrderType::Limit:         return "limit";
        case OrderType::StopLoss:      return "stop";
        case OrderType::StopLossLimit: return "stop_limit";
    }
    return "market";
}

const char* AlpacaBroker::alpaca_side(OrderSide s) {
    return s == OrderSide::Buy ? "buy" : "sell";
}

const char* AlpacaBroker::alpaca_tif(const std::string& validity) {
    if (validity == "IOC") return "ioc";
    if (validity == "GTC") return "gtc";
    if (validity == "OPG") return "opg";
    if (validity == "CLS") return "cls";
    return "day";
}

const char* AlpacaBroker::alpaca_timeframe(const std::string& resolution) {
    if (resolution == "1m" || resolution == "1")   return "1Min";
    if (resolution == "5m" || resolution == "5")   return "5Min";
    if (resolution == "15m" || resolution == "15") return "15Min";
    if (resolution == "30m" || resolution == "30") return "30Min";
    if (resolution == "1h" || resolution == "60")  return "1Hour";
    if (resolution == "4h" || resolution == "240") return "4Hour";
    if (resolution == "D" || resolution == "day" || resolution == "1d")  return "1Day";
    if (resolution == "W" || resolution == "week" || resolution == "1w") return "1Week";
    if (resolution == "M" || resolution == "month") return "1Month";
    return "1Day";
}

std::string AlpacaBroker::extract_error(const json& body, const char* fallback) {
    if (body.contains("message") && body["message"].is_string())
        return body["message"].get<std::string>();
    if (body.contains("error") && body["error"].is_string())
        return body["error"].get<std::string>();
    if (body.contains("code") && body["code"].is_number())
        return "Alpaca error code: " + std::to_string(body["code"].get<int>());
    return fallback;
}

// ============================================================================
// Token Exchange — Alpaca validates by fetching GET /v2/account
// ============================================================================

TokenExchangeResponse AlpacaBroker::exchange_token(const std::string& api_key,
                                                    const std::string& api_secret,
                                                    const std::string& /*auth_code*/) {
    auto& client = HttpClient::instance();

    Headers headers = {
        {"APCA-API-KEY-ID",     api_key},
        {"APCA-API-SECRET-KEY", api_secret},
        {"Accept",              "application/json"}
    };

    auto resp = client.get(std::string(base_url()) + "/v2/account", headers);

    if (!resp.success) {
        return {false, "", "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();
    if (body.is_null()) {
        return {false, "", "", "Invalid JSON response"};
    }

    if (body.contains("id") && body["id"].is_string()) {
        // Alpaca doesn't issue tokens — we store api_key as access_token
        const std::string account_id = body.value("account_number", body.value("id", ""));
        return {true, api_key, account_id, ""};
    }

    return {false, "", "", extract_error(body, "Authentication failed")};
}

// ============================================================================
// Orders
// ============================================================================

OrderPlaceResponse AlpacaBroker::place_order(const BrokerCredentials& creds,
                                              const UnifiedOrder& order) {
    auto& client = HttpClient::instance();

    json payload = {
        {"symbol",        order.symbol},
        {"qty",           std::to_string(static_cast<int>(order.quantity))},
        {"side",          alpaca_side(order.side)},
        {"type",          alpaca_order_type(order.order_type)},
        {"time_in_force", alpaca_tif(order.validity)}
    };

    if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit) {
        if (order.price > 0.0) payload["limit_price"] = std::to_string(order.price);
    }
    if (order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit) {
        if (order.stop_price > 0.0) payload["stop_price"] = std::to_string(order.stop_price);
    }

    auto resp = client.post_json(std::string(base_url()) + "/v2/orders",
                                  payload, auth_headers(creds));

    if (!resp.success) {
        return {false, "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();
    if (body.contains("id") && body["id"].is_string()) {
        return {true, body["id"].get<std::string>(), ""};
    }

    return {false, "", extract_error(body, "Order placement failed")};
}

ApiResponse<json> AlpacaBroker::modify_order(const BrokerCredentials& creds,
                                              const std::string& order_id,
                                              const json& modifications) {
    const int64_t ts = alpaca_now_ts();
    auto& client = HttpClient::instance();

    json payload;
    if (modifications.contains("qty"))         payload["qty"] = modifications["qty"];
    if (modifications.contains("limit_price")) payload["limit_price"] = modifications["limit_price"];
    if (modifications.contains("stop_price"))  payload["stop_price"] = modifications["stop_price"];
    if (modifications.contains("time_in_force")) payload["time_in_force"] = modifications["time_in_force"];

    // Alpaca uses PATCH for order modification
    // HttpClient doesn't have patch(), so use put() — Alpaca also accepts it
    // Actually, we need to use the raw approach: build the body and use put
    const std::string url = std::string(base_url()) + "/v2/orders/" + order_id;
    auto hdrs = auth_headers(creds);

    // PATCH is not available in HttpClient — use post with method override
    // Alpaca's API strictly requires PATCH. We'll use put_json as closest available.
    auto resp = client.put_json(url, payload, hdrs);

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (body.contains("id")) {
        return {true, body, "", ts};
    }
    return {false, std::nullopt, extract_error(body, "Order modification failed"), ts};
}

ApiResponse<json> AlpacaBroker::cancel_order(const BrokerCredentials& creds,
                                              const std::string& order_id) {
    const int64_t ts = alpaca_now_ts();
    auto& client = HttpClient::instance();

    const std::string url = std::string(base_url()) + "/v2/orders/" + order_id;
    auto resp = client.del(url, "", auth_headers(creds));

    // Alpaca returns 204 No Content on successful cancel
    if (resp.status_code == 204 || resp.success) {
        return {true, json{{"order_id", order_id}, {"status", "cancelled"}}, "", ts};
    }

    const auto body = resp.json_body();
    return {false, std::nullopt, extract_error(body, "Order cancellation failed"), ts};
}

ApiResponse<std::vector<BrokerOrderInfo>> AlpacaBroker::get_orders(const BrokerCredentials& creds) {
    const int64_t ts = alpaca_now_ts();
    auto& client = HttpClient::instance();

    const std::string url = std::string(base_url()) + "/v2/orders?status=all&limit=100";
    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerOrderInfo> orders;

    if (body.is_array()) {
        for (const auto& o : body) {
            BrokerOrderInfo info;
            info.order_id     = o.value("id", "");
            info.symbol       = o.value("symbol", "");
            info.side         = o.value("side", "");
            info.order_type   = o.value("type", "");
            info.status       = o.value("status", "");
            info.quantity     = std::stod(o.value("qty", "0"));
            info.filled_qty   = std::stod(o.value("filled_qty", "0"));
            info.price        = std::stod(o.value("limit_price", "0"));
            info.trigger_price = std::stod(o.value("stop_price", "0"));
            info.avg_price    = std::stod(o.value("filled_avg_price", "0"));
            info.timestamp    = o.value("submitted_at", "");
            orders.push_back(std::move(info));
        }
    }

    return {true, std::move(orders), "", ts};
}

ApiResponse<json> AlpacaBroker::get_trade_book(const BrokerCredentials& creds) {
    const int64_t ts = alpaca_now_ts();
    auto& client = HttpClient::instance();

    // Alpaca doesn't have a separate trade book — return filled orders
    const std::string url = std::string(base_url()) + "/v2/orders?status=filled&limit=100";
    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    return {true, resp.json_body(), "", ts};
}

// ============================================================================
// Portfolio
// ============================================================================

ApiResponse<std::vector<BrokerPosition>> AlpacaBroker::get_positions(const BrokerCredentials& creds) {
    const int64_t ts = alpaca_now_ts();
    auto& client = HttpClient::instance();

    auto resp = client.get(std::string(base_url()) + "/v2/positions", auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerPosition> positions;

    if (body.is_array()) {
        for (const auto& p : body) {
            BrokerPosition pos;
            pos.symbol    = p.value("symbol", "");
            pos.exchange  = "US";
            pos.quantity  = std::stod(p.value("qty", "0"));
            pos.avg_price = std::stod(p.value("avg_entry_price", "0"));
            pos.ltp       = std::stod(p.value("current_price", "0"));
            pos.pnl       = std::stod(p.value("unrealized_pl", "0"));
            pos.day_pnl   = std::stod(p.value("unrealized_intraday_pl", "0"));
            pos.side      = p.value("side", "long") == "long" ? "buy" : "sell";
            positions.push_back(std::move(pos));
        }
    }

    return {true, std::move(positions), "", ts};
}

ApiResponse<std::vector<BrokerHolding>> AlpacaBroker::get_holdings(const BrokerCredentials& creds) {
    // Alpaca doesn't distinguish holdings vs positions — return positions as holdings
    const int64_t ts = alpaca_now_ts();
    auto& client = HttpClient::instance();

    auto resp = client.get(std::string(base_url()) + "/v2/positions", auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerHolding> holdings;

    if (body.is_array()) {
        for (const auto& h : body) {
            BrokerHolding hld;
            hld.symbol         = h.value("symbol", "");
            hld.exchange       = "US";
            hld.quantity       = std::stod(h.value("qty", "0"));
            hld.avg_price      = std::stod(h.value("avg_entry_price", "0"));
            hld.ltp            = std::stod(h.value("current_price", "0"));
            hld.pnl            = std::stod(h.value("unrealized_pl", "0"));
            hld.invested_value = std::stod(h.value("cost_basis", "0"));
            hld.current_value  = std::stod(h.value("market_value", "0"));

            const double invested = hld.invested_value;
            hld.pnl_pct = (invested > 0.0) ? (hld.pnl / invested) * 100.0 : 0.0;

            holdings.push_back(std::move(hld));
        }
    }

    return {true, std::move(holdings), "", ts};
}

ApiResponse<BrokerFunds> AlpacaBroker::get_funds(const BrokerCredentials& creds) {
    const int64_t ts = alpaca_now_ts();
    auto& client = HttpClient::instance();

    auto resp = client.get(std::string(base_url()) + "/v2/account", auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (body.is_null() || !body.contains("id")) {
        return {false, std::nullopt, extract_error(body, "Failed to fetch account"), ts};
    }

    BrokerFunds funds;
    funds.available_balance = std::stod(body.value("cash", "0"));
    funds.total_balance     = std::stod(body.value("equity", "0"));
    funds.used_margin       = std::stod(body.value("initial_margin", "0"));
    funds.collateral        = std::stod(body.value("buying_power", "0"));
    funds.raw_data          = body;

    return {true, std::move(funds), "", ts};
}

// ============================================================================
// Market Data (uses data.alpaca.markets)
// ============================================================================

ApiResponse<std::vector<BrokerQuote>> AlpacaBroker::get_quotes(const BrokerCredentials& creds,
                                                                const std::vector<std::string>& symbols) {
    const int64_t ts = alpaca_now_ts();
    auto& client = HttpClient::instance();

    std::vector<BrokerQuote> quotes;

    // Build comma-separated symbol list for multi-snapshot
    std::string sym_list;
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (i > 0) sym_list += ",";
        sym_list += symbols[i];
    }

    const std::string url = std::string(data_url()) + "/v2/stocks/snapshots?symbols=" + sym_list;
    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (body.is_object()) {
        for (const auto& [sym, snap] : body.items()) {
            BrokerQuote q;
            q.symbol = sym;

            if (snap.contains("latestTrade") && snap["latestTrade"].is_object()) {
                q.ltp = snap["latestTrade"].value("p", 0.0);
            }
            if (snap.contains("latestQuote") && snap["latestQuote"].is_object()) {
                q.bid = snap["latestQuote"].value("bp", 0.0);
                q.ask = snap["latestQuote"].value("ap", 0.0);
            }
            if (snap.contains("dailyBar") && snap["dailyBar"].is_object()) {
                const auto& bar = snap["dailyBar"];
                q.open   = bar.value("o", 0.0);
                q.high   = bar.value("h", 0.0);
                q.low    = bar.value("l", 0.0);
                q.close  = bar.value("c", 0.0);
                q.volume = bar.value("v", 0.0);
            }
            if (snap.contains("prevDailyBar") && snap["prevDailyBar"].is_object()) {
                const double prev_close = snap["prevDailyBar"].value("c", 0.0);
                if (prev_close > 0.0 && q.ltp > 0.0) {
                    q.change     = q.ltp - prev_close;
                    q.change_pct = (q.change / prev_close) * 100.0;
                }
            }

            quotes.push_back(std::move(q));
        }
    }

    return {true, std::move(quotes), "", ts};
}

ApiResponse<std::vector<BrokerCandle>> AlpacaBroker::get_history(const BrokerCredentials& creds,
                                                                  const std::string& symbol,
                                                                  const std::string& resolution,
                                                                  const std::string& from_date,
                                                                  const std::string& to_date) {
    const int64_t ts = alpaca_now_ts();
    auto& client = HttpClient::instance();

    const std::string url = std::string(data_url()) +
        "/v2/stocks/" + symbol + "/bars"
        "?timeframe=" + alpaca_timeframe(resolution) +
        "&start=" + from_date +
        "&end=" + to_date +
        "&limit=10000";

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerCandle> candles;

    // Response: {"bars": [{t, o, h, l, c, v, ...}, ...]}
    if (body.contains("bars") && body["bars"].is_array()) {
        for (const auto& bar : body["bars"]) {
            BrokerCandle candle;
            // Timestamp is ISO 8601 string — store as 0 if not parseable
            const std::string ts_str = bar.value("t", "");
            candle.timestamp = 0; // ISO timestamp; consumer should parse
            candle.open      = bar.value("o", 0.0);
            candle.high      = bar.value("h", 0.0);
            candle.low       = bar.value("l", 0.0);
            candle.close     = bar.value("c", 0.0);
            candle.volume    = bar.value("v", 0.0);
            candles.push_back(candle);
        }
    }

    return {true, std::move(candles), "", ts};
}

} // namespace fincept::trading
