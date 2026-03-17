#include "groww_broker.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <chrono>

namespace fincept::trading {

using namespace fincept::http;

// ============================================================================
// Helpers
// ============================================================================

static int64_t groww_now_ts() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

// ============================================================================
// Auth headers — Groww uses Bearer token
// ============================================================================

std::map<std::string, std::string> GrowwBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Authorization", "Bearer " + creds.access_token},
        {"Content-Type",  "application/json"},
        {"Accept",        "application/json"}
    };
}

// ============================================================================
// Mapping helpers
// ============================================================================

const char* GrowwBroker::groww_segment(const std::string& exchange) {
    if (exchange == "NFO" || exchange == "BFO") return "FNO";
    return "CASH";
}

const char* GrowwBroker::groww_exchange(const std::string& exchange) {
    if (exchange == "NFO") return "NSE";
    if (exchange == "BFO") return "BSE";
    return exchange.c_str();
}

const char* GrowwBroker::groww_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "MARKET";
        case OrderType::Limit:         return "LIMIT";
        case OrderType::StopLoss:      return "STOP_LOSS_MARKET";
        case OrderType::StopLossLimit: return "STOP_LOSS_LIMIT";
        default:                       return "MARKET";
    }
}

bool GrowwBroker::is_success(const json& body) {
    return body.value("status", "") == "SUCCESS";
}

std::string GrowwBroker::extract_error(const json& body, const char* fallback) {
    if (body.contains("message") && body["message"].is_string())
        return body["message"].get<std::string>();
    if (body.contains("error") && body["error"].is_string())
        return body["error"].get<std::string>();
    return fallback;
}

const char* GrowwBroker::groww_interval(const std::string& resolution) {
    if (resolution == "1m" || resolution == "1")    return "1";
    if (resolution == "5m" || resolution == "5")    return "5";
    if (resolution == "10m" || resolution == "10")  return "10";
    if (resolution == "15m" || resolution == "15")  return "15";
    if (resolution == "30m" || resolution == "30")  return "30";
    if (resolution == "1h" || resolution == "60")   return "60";
    if (resolution == "4h" || resolution == "240")  return "240";
    if (resolution == "D" || resolution == "day" || resolution == "1d") return "1440";
    if (resolution == "W" || resolution == "week" || resolution == "1w") return "10080";
    return "1440";
}

// ============================================================================
// Token Exchange — TOTP-based access token
// ============================================================================

TokenExchangeResponse GrowwBroker::exchange_token(const std::string& api_key,
                                                    const std::string& /*api_secret*/,
                                                    const std::string& auth_code) {
    // Groww uses TOTP-based auth. The TOTP code is passed as auth_code.
    // api_key is the bearer key for the TOTP request.
    auto& client = HttpClient::instance();

    json payload = {{"totp", auth_code}};

    Headers headers = {
        {"Authorization", "Bearer " + api_key},
        {"Content-Type",  "application/json"}
    };

    auto resp = client.post_json(std::string(base_url()) + "/v1/token/api/access",
                                  payload, headers);

    if (!resp.success) {
        return {false, "", "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();
    if (body.is_null()) {
        return {false, "", "", "Invalid JSON response"};
    }

    if (body.contains("token") && body["token"].is_string()) {
        return {true, body["token"].get<std::string>(), "", ""};
    }

    return {false, "", "", extract_error(body, "No token in response")};
}

// ============================================================================
// Validate Token
// ============================================================================

ApiResponse<bool> GrowwBroker::validate_token(const BrokerCredentials& creds) {
    const int64_t ts = groww_now_ts();
    auto& client = HttpClient::instance();
    auto resp = client.get(std::string(base_url()) + "/v1/margins/detail/user",
                            auth_headers(creds));

    if (!resp.success) {
        return {false, false, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    const bool valid = is_success(body);
    return {valid, valid, valid ? "" : "Token validation failed", ts};
}

// ============================================================================
// Orders
// ============================================================================

OrderPlaceResponse GrowwBroker::place_order(const BrokerCredentials& creds,
                                              const UnifiedOrder& order) {
    auto& client = HttpClient::instance();

    json payload = {
        {"trading_symbol",  order.symbol},
        {"exchange",        groww_exchange(order.exchange)},
        {"segment",         groww_segment(order.exchange)},
        {"transaction_type", order.side == OrderSide::Buy ? "BUY" : "SELL"},
        {"order_type",      groww_order_type(order.order_type)},
        {"quantity",        order.quantity},
        {"product",         order.product_type == ProductType::Intraday ? "MIS" : "CNC"},
        {"validity",        "DAY"}
    };

    if (order.price > 0.0) payload["price"] = order.price;
    if (order.stop_price > 0.0) payload["trigger_price"] = order.stop_price;

    auto resp = client.post_json(std::string(base_url()) + "/v1/order/create",
                                  payload, auth_headers(creds));

    if (!resp.success) {
        return {false, "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();
    if (is_success(body)) {
        std::string oid;
        if (body.contains("payload") && body["payload"].is_object()) {
            const auto& pl = body["payload"];
            if (pl.contains("groww_order_id") && pl["groww_order_id"].is_string())
                oid = pl["groww_order_id"].get<std::string>();
            else if (pl.contains("order_id") && pl["order_id"].is_string())
                oid = pl["order_id"].get<std::string>();
        }
        return {true, oid, ""};
    }

    return {false, "", extract_error(body, "Order placement failed")};
}

ApiResponse<json> GrowwBroker::modify_order(const BrokerCredentials& creds,
                                              const std::string& order_id,
                                              const json& modifications) {
    const int64_t ts = groww_now_ts();
    auto& client = HttpClient::instance();

    json payload = {{"groww_order_id", order_id}};
    if (modifications.contains("segment"))
        payload["segment"] = modifications["segment"];
    else
        payload["segment"] = "CASH";

    if (modifications.contains("quantity"))    payload["quantity"]      = modifications["quantity"];
    if (modifications.contains("price"))       payload["price"]         = modifications["price"];
    if (modifications.contains("trigger_price")) payload["trigger_price"] = modifications["trigger_price"];
    if (modifications.contains("order_type"))  payload["order_type"]    = modifications["order_type"];
    if (modifications.contains("validity"))    payload["validity"]      = modifications["validity"];

    auto resp = client.post_json(std::string(base_url()) + "/v1/order/modify",
                                  payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (is_success(body)) {
        return {true, body, "", ts};
    }
    return {false, std::nullopt, extract_error(body, "Order modification failed"), ts};
}

ApiResponse<json> GrowwBroker::cancel_order(const BrokerCredentials& creds,
                                              const std::string& order_id) {
    const int64_t ts = groww_now_ts();
    auto& client = HttpClient::instance();

    // Groww cancel needs segment — default to CASH
    json payload = {
        {"groww_order_id", order_id},
        {"segment", "CASH"}
    };

    auto resp = client.post_json(std::string(base_url()) + "/v1/order/cancel",
                                  payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (is_success(body)) {
        return {true, body, "", ts};
    }
    return {false, std::nullopt, extract_error(body, "Order cancellation failed"), ts};
}

ApiResponse<std::vector<BrokerOrderInfo>> GrowwBroker::get_orders(const BrokerCredentials& creds) {
    const int64_t ts = groww_now_ts();
    auto& client = HttpClient::instance();

    std::vector<BrokerOrderInfo> all_orders;
    const std::vector<std::string> segments = {"CASH", "FNO"};

    for (const auto& segment : segments) {
        const std::string url = std::string(base_url()) +
            "/v1/order/list?segment=" + segment + "&page=0&page_size=100";

        auto resp = client.get(url, auth_headers(creds));
        if (!resp.success) continue;

        const auto body = resp.json_body();
        if (!is_success(body)) continue;

        if (body.contains("payload") && body["payload"].contains("order_list") &&
            body["payload"]["order_list"].is_array()) {
            for (const auto& o : body["payload"]["order_list"]) {
                BrokerOrderInfo info;
                info.order_id     = o.value("groww_order_id", o.value("order_id", ""));
                info.symbol       = o.value("trading_symbol", "");
                info.exchange     = o.value("exchange", "");
                info.side         = o.value("transaction_type", "");
                info.order_type   = o.value("order_type", "");
                info.status       = o.value("order_status", "");
                info.quantity     = o.value("quantity", 0);
                info.filled_qty   = o.value("filled_quantity", 0);
                info.price        = o.value("price", 0.0);
                info.avg_price    = o.value("average_price", 0.0);
                all_orders.push_back(std::move(info));
            }
        }
    }

    return {true, std::move(all_orders), "", ts};
}

ApiResponse<json> GrowwBroker::get_trade_book(const BrokerCredentials& creds) {
    // Groww does not expose a separate trade book endpoint — return orders as raw JSON
    const int64_t ts = groww_now_ts();
    auto& client = HttpClient::instance();

    auto resp = client.get(std::string(base_url()) + "/v1/order/list?segment=CASH&page=0&page_size=100",
                            auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    return {true, resp.json_body(), "", ts};
}

// ============================================================================
// Portfolio
// ============================================================================

ApiResponse<std::vector<BrokerPosition>> GrowwBroker::get_positions(const BrokerCredentials& creds) {
    const int64_t ts = groww_now_ts();
    auto& client = HttpClient::instance();

    std::vector<BrokerPosition> all_positions;
    const std::vector<std::string> segments = {"CASH", "FNO"};

    for (const auto& segment : segments) {
        auto resp = client.get(std::string(base_url()) +
            "/v1/positions/user?segment=" + segment, auth_headers(creds));
        if (!resp.success) continue;

        const auto body = resp.json_body();
        if (!is_success(body)) continue;

        if (body.contains("payload") && body["payload"].contains("positions") &&
            body["payload"]["positions"].is_array()) {
            for (const auto& p : body["payload"]["positions"]) {
                BrokerPosition pos;
                pos.symbol     = p.value("trading_symbol", "");
                pos.exchange   = p.value("exchange", "");
                pos.quantity   = p.value("net_qty", 0);
                pos.avg_price  = p.value("avg_price", 0.0);
                pos.pnl        = p.value("realized_profit", 0.0);
                all_positions.push_back(std::move(pos));
            }
        }
    }

    return {true, std::move(all_positions), "", ts};
}

ApiResponse<std::vector<BrokerHolding>> GrowwBroker::get_holdings(const BrokerCredentials& creds) {
    const int64_t ts = groww_now_ts();
    auto& client = HttpClient::instance();

    auto resp = client.get(std::string(base_url()) + "/v1/holdings/detail/user",
                            auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerHolding> holdings;

    if (is_success(body) && body.contains("payload") && body["payload"].is_object()) {
        // Groww returns holdings in payload; structure may vary
        const auto& pl = body["payload"];
        if (pl.contains("holdings") && pl["holdings"].is_array()) {
            for (const auto& h : pl["holdings"]) {
                BrokerHolding hld;
                hld.symbol     = h.value("trading_symbol", h.value("symbol", ""));
                hld.exchange   = h.value("exchange", "NSE");
                hld.quantity   = h.value("quantity", 0);
                hld.avg_price  = h.value("average_price", 0.0);
                hld.ltp        = h.value("ltp", 0.0);
                hld.pnl        = h.value("pnl", 0.0);
                holdings.push_back(std::move(hld));
            }
        }
    }

    return {true, std::move(holdings), "", ts};
}

ApiResponse<BrokerFunds> GrowwBroker::get_funds(const BrokerCredentials& creds) {
    const int64_t ts = groww_now_ts();
    auto& client = HttpClient::instance();

    auto resp = client.get(std::string(base_url()) + "/v1/margins/detail/user",
                            auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (!is_success(body)) {
        return {false, std::nullopt, extract_error(body, "Failed to fetch margins"), ts};
    }

    BrokerFunds funds;
    if (body.contains("payload") && body["payload"].is_object()) {
        const auto& pl = body["payload"];
        funds.available_balance = pl.value("clear_cash", 0.0);
        funds.collateral       = pl.value("collateral_available", 0.0);
        funds.used_margin      = pl.value("net_margin_used", 0.0);
    }

    return {true, std::move(funds), "", ts};
}

// ============================================================================
// Market Data
// ============================================================================

ApiResponse<std::vector<BrokerQuote>> GrowwBroker::get_quotes(const BrokerCredentials& creds,
                                                                const std::vector<std::string>& symbols) {
    const int64_t ts = groww_now_ts();
    auto& client = HttpClient::instance();

    std::vector<BrokerQuote> quotes;

    for (const auto& sym : symbols) {
        // Symbol format expected: "EXCHANGE:SYMBOL" e.g. "NSE:RELIANCE"
        std::string exchange = "NSE";
        std::string trading_symbol = sym;
        const auto colon = sym.find(':');
        if (colon != std::string::npos) {
            exchange = sym.substr(0, colon);
            trading_symbol = sym.substr(colon + 1);
        }

        const std::string url = std::string(base_url()) +
            "/v1/live-data/quote?exchange=" + groww_exchange(exchange) +
            "&segment=" + groww_segment(exchange) +
            "&trading_symbol=" + trading_symbol;

        auto resp = client.get(url, auth_headers(creds));
        if (!resp.success) continue;

        const auto body = resp.json_body();
        if (!is_success(body)) continue;

        if (body.contains("payload") && body["payload"].is_object()) {
            const auto& pl = body["payload"];
            BrokerQuote q;
            q.symbol   = trading_symbol;
            q.ltp      = pl.value("last_price", 0.0);
            q.volume   = pl.value("volume", 0);
            q.change   = pl.value("day_change", 0.0);

            if (pl.contains("ohlc") && pl["ohlc"].is_object()) {
                q.open  = pl["ohlc"].value("open", 0.0);
                q.high  = pl["ohlc"].value("high", 0.0);
                q.low   = pl["ohlc"].value("low", 0.0);
                q.close = pl["ohlc"].value("close", 0.0);
            }

            quotes.push_back(std::move(q));
        }
    }

    return {true, std::move(quotes), "", ts};
}

ApiResponse<std::vector<BrokerCandle>> GrowwBroker::get_history(const BrokerCredentials& creds,
                                                                  const std::string& symbol,
                                                                  const std::string& interval,
                                                                  const std::string& from_date,
                                                                  const std::string& to_date) {
    const int64_t ts = groww_now_ts();
    auto& client = HttpClient::instance();

    // Symbol format: "EXCHANGE:SYMBOL"
    std::string exchange = "NSE";
    std::string trading_symbol = symbol;
    const auto colon = symbol.find(':');
    if (colon != std::string::npos) {
        exchange = symbol.substr(0, colon);
        trading_symbol = symbol.substr(colon + 1);
    }

    const std::string url = std::string(base_url()) +
        "/v1/historical/candle/range?exchange=" + groww_exchange(exchange) +
        "&segment=" + groww_segment(exchange) +
        "&trading_symbol=" + trading_symbol +
        "&start_time=" + from_date + "+09:15:00" +
        "&end_time=" + to_date + "+15:30:00" +
        "&interval_in_minutes=" + groww_interval(interval);

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (!is_success(body)) {
        return {false, std::nullopt, extract_error(body, "Failed to fetch historical data"), ts};
    }

    std::vector<BrokerCandle> candles;
    if (body.contains("payload") && body["payload"].contains("candles") &&
        body["payload"]["candles"].is_array()) {
        for (const auto& c : body["payload"]["candles"]) {
            if (!c.is_array() || c.size() < 6) continue;

            BrokerCandle candle;
            int64_t raw_ts = c[0].get<int64_t>();
            candle.timestamp = (raw_ts > 4102444800) ? raw_ts / 1000 : raw_ts;
            candle.open      = c[1].get<double>();
            candle.high      = c[2].get<double>();
            candle.low       = c[3].get<double>();
            candle.close     = c[4].get<double>();
            candle.volume    = c[5].get<int64_t>();
            candles.push_back(candle);
        }
    }

    return {true, std::move(candles), "", ts};
}

} // namespace fincept::trading
