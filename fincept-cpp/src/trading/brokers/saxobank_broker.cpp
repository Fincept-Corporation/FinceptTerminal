#include "saxobank_broker.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <chrono>

namespace fincept::trading {

using namespace fincept::http;

// ============================================================================
// Helpers (broker-prefixed to avoid unity build collisions)
// ============================================================================

static int64_t saxo_now_ts() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

// ============================================================================
// Auth headers — SaxoBank uses Bearer token
// ============================================================================

std::map<std::string, std::string> SaxoBankBroker::auth_headers(const BrokerCredentials& creds) const {
    return {
        {"Authorization", "Bearer " + creds.access_token},
        {"Content-Type",  "application/json"},
        {"Accept",        "application/json"}
    };
}

// ============================================================================
// Mapping helpers
// ============================================================================

std::string SaxoBankBroker::get_client_key(const BrokerCredentials& creds) {
    if (!creds.additional_data.empty()) {
        try {
            auto extra = json::parse(creds.additional_data);
            if (extra.contains("ClientKey"))
                return extra["ClientKey"].get<std::string>();
            if (extra.contains("client_key"))
                return extra["client_key"].get<std::string>();
        } catch (...) {}
    }
    return creds.user_id;
}

std::string SaxoBankBroker::get_account_key(const BrokerCredentials& creds) {
    if (!creds.additional_data.empty()) {
        try {
            auto extra = json::parse(creds.additional_data);
            if (extra.contains("AccountKey"))
                return extra["AccountKey"].get<std::string>();
            if (extra.contains("account_key"))
                return extra["account_key"].get<std::string>();
        } catch (...) {}
    }
    return "";
}

int SaxoBankBroker::saxo_horizon(const std::string& resolution) {
    // Saxo Horizon values (minutes): 1, 5, 10, 15, 30, 60, 120, 240, 1440, 10080, 43200
    if (resolution == "1m" || resolution == "1")    return 1;
    if (resolution == "5m" || resolution == "5")    return 5;
    if (resolution == "10m" || resolution == "10")  return 10;
    if (resolution == "15m" || resolution == "15")  return 15;
    if (resolution == "30m" || resolution == "30")  return 30;
    if (resolution == "1h" || resolution == "60")   return 60;
    if (resolution == "2h" || resolution == "120")  return 120;
    if (resolution == "4h" || resolution == "240")  return 240;
    if (resolution == "D" || resolution == "day" || resolution == "1d")   return 1440;
    if (resolution == "W" || resolution == "week" || resolution == "1w")  return 10080;
    if (resolution == "M" || resolution == "month") return 43200;
    return 1440;
}

const char* SaxoBankBroker::saxo_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "Market";
        case OrderType::Limit:         return "Limit";
        case OrderType::StopLoss:      return "Stop";
        case OrderType::StopLossLimit: return "StopLimit";
    }
    return "Market";
}

const char* SaxoBankBroker::saxo_order_duration(const std::string& validity) {
    if (validity == "GTC") return "GoodTillCancel";
    if (validity == "IOC") return "ImmediateOrCancel";
    if (validity == "GTD") return "GoodTillDate";
    return "DayOrder";
}

std::string SaxoBankBroker::extract_error(const json& body, const char* fallback) {
    if (body.contains("ErrorInfo") && body["ErrorInfo"].is_object()) {
        return body["ErrorInfo"].value("Message",
            body["ErrorInfo"].value("ErrorCode", std::string(fallback)));
    }
    if (body.contains("Message") && body["Message"].is_string())
        return body["Message"].get<std::string>();
    if (body.contains("error_description") && body["error_description"].is_string())
        return body["error_description"].get<std::string>();
    if (body.contains("error") && body["error"].is_string())
        return body["error"].get<std::string>();
    return fallback;
}

// ============================================================================
// Token Exchange — OAuth 2.0 authorization_code grant (form-urlencoded)
// ============================================================================

TokenExchangeResponse SaxoBankBroker::exchange_token(const std::string& api_key,
                                                      const std::string& api_secret,
                                                      const std::string& auth_code) {
    auto& client = HttpClient::instance();

    // OAuth token endpoint
    const std::string token_url = "https://live.logonvalidation.net/token";

    // Build form-urlencoded body
    std::string form_body =
        "grant_type=authorization_code"
        "&code=" + auth_code +
        "&client_id=" + api_key +
        "&client_secret=" + api_secret +
        "&redirect_uri=http://localhost/callback";

    Headers headers = {
        {"Content-Type", "application/x-www-form-urlencoded"},
        {"Accept",       "application/json"}
    };

    auto resp = client.post(token_url, form_body, headers);

    if (!resp.success) {
        return {false, "", "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();
    if (body.is_null()) {
        return {false, "", "", "Invalid JSON response"};
    }

    if (body.contains("access_token") && body["access_token"].is_string()) {
        const std::string token = body["access_token"].get<std::string>();

        // Fetch client info to get ClientKey
        Headers auth_hdrs = {
            {"Authorization", "Bearer " + token},
            {"Accept",        "application/json"}
        };

        std::string client_key;
        std::string account_key;

        auto client_resp = client.get(std::string(base_url()) + "/port/v1/clients/me", auth_hdrs);
        if (client_resp.success) {
            const auto client_body = client_resp.json_body();
            client_key = client_body.value("ClientKey", "");
            account_key = client_body.value("DefaultAccountKey", "");
        }

        // Build user_id as JSON with keys for later use
        json user_data = {
            {"ClientKey", client_key},
            {"AccountKey", account_key}
        };

        return {true, token, user_data.dump(), ""};
    }

    return {false, "", "", extract_error(body, "Token exchange failed")};
}

// ============================================================================
// Orders
// ============================================================================

OrderPlaceResponse SaxoBankBroker::place_order(const BrokerCredentials& creds,
                                                const UnifiedOrder& order) {
    auto& client = HttpClient::instance();
    const std::string account_key = get_account_key(creds);

    if (account_key.empty()) {
        return {false, "", "No AccountKey configured for Saxo Bank"};
    }

    // Saxo uses UIC (instrument ID) — symbol should contain the UIC
    int uic = 0;
    try {
        uic = std::stoi(order.symbol);
    } catch (...) {
        return {false, "", "Symbol must be a numeric UIC for Saxo Bank"};
    }

    json payload = {
        {"AccountKey",     account_key},
        {"Uic",            uic},
        {"AssetType",      "Stock"},
        {"BuySell",        order.side == OrderSide::Buy ? "Buy" : "Sell"},
        {"Amount",         static_cast<int>(order.quantity)},
        {"OrderType",      saxo_order_type(order.order_type)},
        {"OrderDuration",  {{"DurationType", saxo_order_duration(order.validity)}}},
        {"ManualOrder",    true}
    };

    if (!order.exchange.empty()) {
        payload["AssetType"] = order.exchange;  // Consumer may pass asset type via exchange field
    }

    if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit) {
        if (order.price > 0.0) payload["OrderPrice"] = order.price;
    }
    if (order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit) {
        if (order.stop_price > 0.0) payload["StopLimitPrice"] = order.stop_price;
    }

    auto resp = client.post_json(std::string(base_url()) + "/trade/v2/orders",
                                  payload, auth_headers(creds));

    if (!resp.success) {
        return {false, "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();
    if (body.contains("OrderId") && body["OrderId"].is_string()) {
        return {true, body["OrderId"].get<std::string>(), ""};
    }

    return {false, "", extract_error(body, "Order placement failed")};
}

ApiResponse<json> SaxoBankBroker::modify_order(const BrokerCredentials& creds,
                                                const std::string& order_id,
                                                const json& modifications) {
    const int64_t ts = saxo_now_ts();
    auto& client = HttpClient::instance();

    // Saxo uses PATCH for order modification — use put_json as closest available
    const std::string url = std::string(base_url()) + "/trade/v2/orders/" + order_id;
    auto resp = client.put_json(url, modifications, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (body.contains("OrderId")) {
        return {true, body, "", ts};
    }
    return {false, std::nullopt, extract_error(body, "Order modification failed"), ts};
}

ApiResponse<json> SaxoBankBroker::cancel_order(const BrokerCredentials& creds,
                                                const std::string& order_id) {
    const int64_t ts = saxo_now_ts();
    auto& client = HttpClient::instance();
    const std::string account_key = get_account_key(creds);

    const std::string url = std::string(base_url()) +
        "/trade/v2/orders/" + order_id + "?AccountKey=" + account_key;

    auto resp = client.del(url, "", auth_headers(creds));

    if (resp.success) {
        return {true, json{{"OrderId", order_id}, {"status", "cancelled"}}, "", ts};
    }

    const auto body = resp.json_body();
    return {false, std::nullopt, extract_error(body, "Order cancellation failed"), ts};
}

ApiResponse<std::vector<BrokerOrderInfo>> SaxoBankBroker::get_orders(const BrokerCredentials& creds) {
    const int64_t ts = saxo_now_ts();
    auto& client = HttpClient::instance();
    const std::string client_key = get_client_key(creds);

    if (client_key.empty()) {
        return {false, std::nullopt, "No ClientKey configured", ts};
    }

    const std::string url = std::string(base_url()) +
        "/port/v1/orders?ClientKey=" + client_key;

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerOrderInfo> orders;

    // Response: {"Data": [...], "__count": N}
    const auto& data = body.contains("Data") ? body["Data"] : body;

    if (data.is_array()) {
        for (const auto& o : data) {
            BrokerOrderInfo info;
            info.order_id   = o.value("OrderId", "");
            info.symbol     = std::to_string(o.value("Uic", 0));
            info.side       = o.value("BuySell", "");
            info.order_type = o.value("OrderType", "");
            info.status     = o.value("Status", "");
            info.quantity   = o.value("Amount", 0.0);
            info.filled_qty = o.value("FilledAmount", 0.0);
            info.price      = o.value("Price", 0.0);
            info.avg_price  = o.value("AveragePrice", 0.0);

            // Try to get display name
            if (o.contains("DisplayAndFormat") && o["DisplayAndFormat"].is_object()) {
                const std::string desc = o["DisplayAndFormat"].value("Description", "");
                if (!desc.empty()) info.symbol = desc;
            }

            orders.push_back(std::move(info));
        }
    }

    return {true, std::move(orders), "", ts};
}

ApiResponse<json> SaxoBankBroker::get_trade_book(const BrokerCredentials& creds) {
    const int64_t ts = saxo_now_ts();
    auto& client = HttpClient::instance();
    const std::string client_key = get_client_key(creds);

    if (client_key.empty()) {
        return {false, std::nullopt, "No ClientKey configured", ts};
    }

    // Closed positions serve as trade history
    const std::string url = std::string(base_url()) +
        "/port/v1/closedpositions?ClientKey=" + client_key;

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    return {true, resp.json_body(), "", ts};
}

// ============================================================================
// Portfolio
// ============================================================================

ApiResponse<std::vector<BrokerPosition>> SaxoBankBroker::get_positions(const BrokerCredentials& creds) {
    const int64_t ts = saxo_now_ts();
    auto& client = HttpClient::instance();
    const std::string client_key = get_client_key(creds);

    if (client_key.empty()) {
        return {false, std::nullopt, "No ClientKey configured", ts};
    }

    const std::string url = std::string(base_url()) +
        "/port/v1/positions?ClientKey=" + client_key +
        "&FieldGroups=PositionBase,PositionView,DisplayAndFormat,ExchangeInfo";

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerPosition> positions;

    const auto& data = body.contains("Data") ? body["Data"] : body;

    if (data.is_array()) {
        for (const auto& p : data) {
            BrokerPosition pos;

            // PositionBase contains core data
            if (p.contains("PositionBase") && p["PositionBase"].is_object()) {
                const auto& pb = p["PositionBase"];
                pos.quantity  = pb.value("Amount", 0.0);
                pos.avg_price = pb.value("OpenPrice", 0.0);
                pos.side      = (pos.quantity >= 0) ? "buy" : "sell";
            }

            // PositionView contains P&L
            if (p.contains("PositionView") && p["PositionView"].is_object()) {
                const auto& pv = p["PositionView"];
                pos.pnl       = pv.value("ProfitLossOnTrade", 0.0);
                pos.ltp       = pv.value("CurrentPrice", 0.0);
            }

            // DisplayAndFormat contains symbol/description
            if (p.contains("DisplayAndFormat") && p["DisplayAndFormat"].is_object()) {
                const auto& df = p["DisplayAndFormat"];
                pos.symbol = df.value("Description", df.value("Symbol", ""));
            }

            // ExchangeInfo
            if (p.contains("ExchangeInfo") && p["ExchangeInfo"].is_object()) {
                pos.exchange = p["ExchangeInfo"].value("ExchangeId", "");
            }

            positions.push_back(std::move(pos));
        }
    }

    return {true, std::move(positions), "", ts};
}

ApiResponse<std::vector<BrokerHolding>> SaxoBankBroker::get_holdings(const BrokerCredentials& creds) {
    // Saxo positions serve as holdings for long-term positions
    const int64_t ts = saxo_now_ts();
    auto& client = HttpClient::instance();
    const std::string client_key = get_client_key(creds);

    if (client_key.empty()) {
        return {false, std::nullopt, "No ClientKey configured", ts};
    }

    const std::string url = std::string(base_url()) +
        "/port/v1/netpositions?ClientKey=" + client_key +
        "&FieldGroups=NetPositionBase,NetPositionView,DisplayAndFormat,ExchangeInfo";

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerHolding> holdings;

    const auto& data = body.contains("Data") ? body["Data"] : body;

    if (data.is_array()) {
        for (const auto& h : data) {
            BrokerHolding hld;

            if (h.contains("NetPositionBase") && h["NetPositionBase"].is_object()) {
                const auto& nb = h["NetPositionBase"];
                hld.quantity  = nb.value("Amount", 0.0);
                hld.avg_price = nb.value("AverageOpenPrice", 0.0);
            }

            if (h.contains("NetPositionView") && h["NetPositionView"].is_object()) {
                const auto& nv = h["NetPositionView"];
                hld.pnl           = nv.value("ProfitLossOnTrade", 0.0);
                hld.current_value = nv.value("MarketValue", 0.0);
                hld.ltp           = nv.value("CurrentPrice", 0.0);
            }

            if (h.contains("DisplayAndFormat") && h["DisplayAndFormat"].is_object()) {
                hld.symbol = h["DisplayAndFormat"].value("Description",
                    h["DisplayAndFormat"].value("Symbol", ""));
            }

            if (h.contains("ExchangeInfo") && h["ExchangeInfo"].is_object()) {
                hld.exchange = h["ExchangeInfo"].value("ExchangeId", "");
            }

            hld.invested_value = hld.avg_price * std::abs(hld.quantity);
            hld.pnl_pct = (hld.invested_value > 0.0)
                ? (hld.pnl / hld.invested_value) * 100.0 : 0.0;

            holdings.push_back(std::move(hld));
        }
    }

    return {true, std::move(holdings), "", ts};
}

ApiResponse<BrokerFunds> SaxoBankBroker::get_funds(const BrokerCredentials& creds) {
    const int64_t ts = saxo_now_ts();
    auto& client = HttpClient::instance();
    const std::string account_key = get_account_key(creds);

    if (account_key.empty()) {
        return {false, std::nullopt, "No AccountKey configured", ts};
    }

    const std::string url = std::string(base_url()) +
        "/port/v1/balances?AccountKey=" + account_key;

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    BrokerFunds funds;

    funds.available_balance = body.value("CashBalance", 0.0);
    funds.total_balance     = body.value("TotalValue", 0.0);
    funds.used_margin       = body.value("MarginUsedByCurrentPositions", 0.0);
    funds.collateral        = body.value("MarginAvailableForTrading", 0.0);
    funds.raw_data          = body;

    return {true, std::move(funds), "", ts};
}

// ============================================================================
// Market Data (UIC-based)
// ============================================================================

ApiResponse<std::vector<BrokerQuote>> SaxoBankBroker::get_quotes(const BrokerCredentials& creds,
                                                                  const std::vector<std::string>& symbols) {
    const int64_t ts = saxo_now_ts();
    auto& client = HttpClient::instance();

    std::vector<BrokerQuote> quotes;

    // Build comma-separated UIC list
    std::string uic_list;
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (i > 0) uic_list += ",";
        uic_list += symbols[i];
    }

    // Batch quote endpoint
    const std::string url = std::string(base_url()) +
        "/trade/v1/infoprices/list?Uics=" + uic_list +
        "&AssetType=Stock&FieldGroups=DisplayAndFormat,Quote";

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    const auto& data = body.contains("Data") ? body["Data"] : body;

    if (data.is_array()) {
        for (const auto& item : data) {
            BrokerQuote q;

            // DisplayAndFormat
            if (item.contains("DisplayAndFormat") && item["DisplayAndFormat"].is_object()) {
                q.symbol = item["DisplayAndFormat"].value("Description",
                    item["DisplayAndFormat"].value("Symbol", ""));
            }
            if (q.symbol.empty()) {
                q.symbol = std::to_string(item.value("Uic", 0));
            }

            // Quote data
            if (item.contains("Quote") && item["Quote"].is_object()) {
                const auto& qt = item["Quote"];
                q.ltp    = qt.value("Mid", qt.value("Last", 0.0));
                q.bid    = qt.value("Bid", 0.0);
                q.ask    = qt.value("Ask", 0.0);
                q.high   = qt.value("High", 0.0);
                q.low    = qt.value("Low", 0.0);
                q.open   = qt.value("Open", 0.0);
                q.close  = qt.value("PreviousClose", 0.0);
                q.volume = qt.value("Volume", 0.0);

                if (q.close > 0.0 && q.ltp > 0.0) {
                    q.change     = q.ltp - q.close;
                    q.change_pct = (q.change / q.close) * 100.0;
                }
            }

            quotes.push_back(std::move(q));
        }
    }

    return {true, std::move(quotes), "", ts};
}

ApiResponse<std::vector<BrokerCandle>> SaxoBankBroker::get_history(const BrokerCredentials& creds,
                                                                    const std::string& symbol,
                                                                    const std::string& resolution,
                                                                    const std::string& from_date,
                                                                    const std::string& to_date) {
    const int64_t ts = saxo_now_ts();
    auto& client = HttpClient::instance();

    // Saxo chart API: /chart/v1/charts?AssetType=Stock&Uic=X&Horizon=H&Mode=From&Time=T
    std::string url = std::string(base_url()) +
        "/chart/v1/charts?AssetType=Stock"
        "&Uic=" + symbol +
        "&Horizon=" + std::to_string(saxo_horizon(resolution)) +
        "&Mode=From&Time=" + from_date;

    if (!to_date.empty()) {
        url += "&ToTime=" + to_date;
    }

    auto resp = client.get(url, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerCandle> candles;

    // Response: {"Data": [{Time, OpenBid, HighBid, LowBid, CloseBid, ...}, ...]}
    const auto& data = body.contains("Data") ? body["Data"] : body;

    if (data.is_array()) {
        for (const auto& bar : data) {
            BrokerCandle candle;
            candle.timestamp = 0;  // ISO date string; consumer should parse
            candle.open      = bar.value("OpenBid", bar.value("Open", 0.0));
            candle.high      = bar.value("HighBid", bar.value("High", 0.0));
            candle.low       = bar.value("LowBid", bar.value("Low", 0.0));
            candle.close     = bar.value("CloseBid", bar.value("Close", 0.0));
            candle.volume    = bar.value("Volume", 0.0);
            candles.push_back(candle);
        }
    }

    return {true, std::move(candles), "", ts};
}

} // namespace fincept::trading
