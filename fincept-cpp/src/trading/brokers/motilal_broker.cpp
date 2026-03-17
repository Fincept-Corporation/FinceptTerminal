#include "motilal_broker.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <chrono>
#include <openssl/sha.h>
#include <iomanip>

namespace fincept::trading {

using namespace fincept::http;

// ============================================================================
// Helpers
// ============================================================================

static int64_t motilal_now_ts() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

// SHA256 hash helper
static std::string motilal_sha256(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()),
           input.size(), hash);
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
    return oss.str();
}

// ============================================================================
// Auth headers — Motilal requires many custom headers
// ============================================================================

std::map<std::string, std::string> MotilalBroker::motilal_base_headers(const std::string& api_key) {
    return {
        {"Content-Type",   "application/json"},
        {"Accept",         "application/json"},
        {"ApiKey",         api_key},
        {"ClientLocalIp",  "127.0.0.1"},
        {"ClientPublicIp", "127.0.0.1"},
        {"MacAddress",     "00:00:00:00:00:00"},
        {"SourceId",       "WEB"},
        {"vendorinfo",     api_key},
        {"User-Agent",     "MOSL/V.1.1.0"},
        {"osname",         "Windows"},
        {"osversion",      "11"},
        {"devicemodel",    "Desktop"},
        {"manufacturer",   "PC"},
        {"productname",    "FinceptTerminal"},
        {"productversion", "4.0"},
        {"browsername",    "Desktop"},
        {"browserversion", "4.0"}
    };
}

std::map<std::string, std::string> MotilalBroker::auth_headers(const BrokerCredentials& creds) const {
    auto headers = motilal_base_headers(creds.api_key);
    headers["Authorization"] = creds.access_token;
    return headers;
}

// ============================================================================
// Mapping helpers
// ============================================================================

const char* MotilalBroker::motilal_exchange(const std::string& exchange) {
    if (exchange == "NSE")  return "NSE";
    if (exchange == "BSE")  return "BSE";
    if (exchange == "NFO")  return "NFO";
    if (exchange == "MCX")  return "MCX";
    if (exchange == "CDS")  return "CDS";
    return "NSE";
}

const char* MotilalBroker::motilal_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "MKT";
        case OrderType::Limit:         return "L";
        case OrderType::StopLoss:      return "SL-M";
        case OrderType::StopLossLimit: return "SL";
        default:                       return "MKT";
    }
}

const char* MotilalBroker::motilal_product_type(ProductType p) {
    switch (p) {
        case ProductType::Intraday:      return "INTRADAY";
        case ProductType::Delivery:      return "DELIVERY";
        case ProductType::Margin:        return "NORMAL";
        case ProductType::CoverOrder:    return "CO";
        case ProductType::BracketOrder:  return "BO";
        default:                         return "INTRADAY";
    }
}

bool MotilalBroker::is_success(const json& body) {
    return body.value("status", "") == "SUCCESS";
}

std::string MotilalBroker::extract_error(const json& body, const char* fallback) {
    if (body.contains("message") && body["message"].is_string())
        return body["message"].get<std::string>();
    if (body.contains("errMsg") && body["errMsg"].is_string())
        return body["errMsg"].get<std::string>();
    return fallback;
}

// ============================================================================
// Token Exchange — SHA256(password + apikey) auth
// ============================================================================

TokenExchangeResponse MotilalBroker::exchange_token(const std::string& api_key,
                                                       const std::string& api_secret,
                                                       const std::string& auth_code) {
    auto& client = HttpClient::instance();

    // auth_code = password; api_secret = vendor_code
    // Motilal auth: SHA256(password + apikey)
    const std::string pwd_hash = motilal_sha256(auth_code + api_key);

    json payload = {
        {"userid",   api_secret},  // user_id passed as api_secret
        {"password", pwd_hash}
    };

    auto headers = motilal_base_headers(api_key);

    auto resp = client.post_json(std::string(base_url()) + "/login/v3/authdirectapi",
                                  payload, headers);

    if (!resp.success) {
        return {false, "", "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();
    if (body.is_null()) {
        return {false, "", "", "Invalid JSON response"};
    }

    if (is_success(body)) {
        std::string token = body.value("AuthToken", "");
        if (!token.empty()) {
            return {true, token, api_secret, ""};
        }
    }

    return {false, "", "", extract_error(body, "Authentication failed")};
}

// ============================================================================
// Orders
// ============================================================================

OrderPlaceResponse MotilalBroker::place_order(const BrokerCredentials& creds,
                                                 const UnifiedOrder& order) {
    auto& client = HttpClient::instance();

    json payload = {
        {"exchange",          motilal_exchange(order.exchange)},
        {"symboltoken",       order.symbol},
        {"buyorsell",         order.side == OrderSide::Buy ? "B" : "S"},
        {"ordertype",         motilal_order_type(order.order_type)},
        {"producttype",       motilal_product_type(order.product_type)},
        {"orderduration",     "DAY"},
        {"price",             order.price},
        {"triggerprice",      order.stop_price},
        {"quantityinlot",     static_cast<int>(order.quantity)},
        {"disclosedquantity", 0},
        {"amoorder",          "N"}
    };

    auto resp = client.post_json(std::string(base_url()) + "/trans/v1/placeorder",
                                  payload, auth_headers(creds));

    if (!resp.success) {
        return {false, "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();
    if (is_success(body)) {
        std::string oid = body.value("uniqueorderid", "");
        return {true, oid, ""};
    }

    return {false, "", extract_error(body, "Order placement failed")};
}

ApiResponse<json> MotilalBroker::modify_order(const BrokerCredentials& creds,
                                                 const std::string& order_id,
                                                 const json& modifications) {
    const int64_t ts = motilal_now_ts();
    auto& client = HttpClient::instance();

    json payload = {{"uniqueorderid", order_id}};

    if (modifications.contains("quantity"))
        payload["quantityinlot"] = modifications["quantity"];
    if (modifications.contains("price"))
        payload["price"] = modifications["price"];
    if (modifications.contains("trigger_price"))
        payload["triggerprice"] = modifications["trigger_price"];
    if (modifications.contains("order_type"))
        payload["ordertype"] = modifications["order_type"];

    auto resp = client.post_json(std::string(base_url()) + "/trans/v2/modifyorder",
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

ApiResponse<json> MotilalBroker::cancel_order(const BrokerCredentials& creds,
                                                 const std::string& order_id) {
    const int64_t ts = motilal_now_ts();
    auto& client = HttpClient::instance();

    json payload = {{"uniqueorderid", order_id}};

    auto resp = client.post_json(std::string(base_url()) + "/trans/v1/cancelorder",
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

ApiResponse<std::vector<BrokerOrderInfo>> MotilalBroker::get_orders(const BrokerCredentials& creds) {
    const int64_t ts = motilal_now_ts();
    auto& client = HttpClient::instance();

    // Motilal order book is POST
    json payload = json::object();

    auto resp = client.post_json(std::string(base_url()) + "/book/v1/getorderbook",
                                  payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerOrderInfo> orders;

    if (is_success(body) && body.contains("data") && body["data"].is_array()) {
        for (const auto& o : body["data"]) {
            BrokerOrderInfo info;
            info.order_id     = o.value("uniqueorderid", "");
            info.symbol       = o.value("symboltoken", o.value("symbol", ""));
            info.exchange     = o.value("exchange", "");
            info.side         = o.value("buyorsell", "");
            info.order_type   = o.value("ordertype", "");
            info.product_type = o.value("producttype", "");
            info.quantity     = o.value("quantity", 0.0);
            info.filled_qty   = o.value("filledquantity", 0.0);
            info.price        = o.value("price", 0.0);
            info.avg_price    = o.value("averageprice", 0.0);
            info.status       = o.value("orderstatus", "");
            orders.push_back(std::move(info));
        }
    }

    return {true, std::move(orders), "", ts};
}

ApiResponse<json> MotilalBroker::get_trade_book(const BrokerCredentials& creds) {
    const int64_t ts = motilal_now_ts();
    auto& client = HttpClient::instance();

    json payload = json::object();
    auto resp = client.post_json(std::string(base_url()) + "/book/v1/gettradebook",
                                  payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    return {true, resp.json_body(), "", ts};
}

// ============================================================================
// Portfolio
// ============================================================================

ApiResponse<std::vector<BrokerPosition>> MotilalBroker::get_positions(const BrokerCredentials& creds) {
    const int64_t ts = motilal_now_ts();
    auto& client = HttpClient::instance();

    json payload = json::object();
    auto resp = client.post_json(std::string(base_url()) + "/book/v1/getposition",
                                  payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerPosition> positions;

    if (is_success(body) && body.contains("data") && body["data"].is_array()) {
        for (const auto& p : body["data"]) {
            BrokerPosition pos;
            pos.symbol       = p.value("symbol", p.value("symboltoken", ""));
            pos.exchange     = p.value("exchange", "");
            pos.product_type = p.value("producttype", "");
            pos.quantity     = p.value("netqty", 0.0);
            pos.avg_price    = p.value("averageprice", 0.0);
            pos.ltp          = p.value("ltp", 0.0);
            pos.pnl          = p.value("realizedpnl", 0.0);
            pos.day_pnl      = p.value("unrealizedpnl", 0.0);
            positions.push_back(std::move(pos));
        }
    }

    return {true, std::move(positions), "", ts};
}

ApiResponse<std::vector<BrokerHolding>> MotilalBroker::get_holdings(const BrokerCredentials& creds) {
    const int64_t ts = motilal_now_ts();
    auto& client = HttpClient::instance();

    json payload = json::object();
    auto resp = client.post_json(std::string(base_url()) + "/report/v1/getdpholding",
                                  payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerHolding> holdings;

    if (is_success(body) && body.contains("data") && body["data"].is_array()) {
        for (const auto& h : body["data"]) {
            BrokerHolding hld;
            hld.symbol     = h.value("symbol", h.value("symboltoken", ""));
            hld.exchange   = h.value("exchange", "NSE");
            hld.quantity   = h.value("quantity", 0.0);
            hld.avg_price  = h.value("averageprice", 0.0);
            hld.ltp        = h.value("ltp", 0.0);
            hld.pnl        = h.value("pnl", 0.0);
            holdings.push_back(std::move(hld));
        }
    }

    return {true, std::move(holdings), "", ts};
}

ApiResponse<BrokerFunds> MotilalBroker::get_funds(const BrokerCredentials& creds) {
    const int64_t ts = motilal_now_ts();
    auto& client = HttpClient::instance();

    json payload = json::object();
    auto resp = client.post_json(std::string(base_url()) + "/report/v1/getreportmargindetail",
                                  payload, auth_headers(creds));

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    BrokerFunds funds;

    // Motilal funds: parse by srno-based entries
    // srno 102 = available cash, 220 = collateral, 300 = margin used
    if (is_success(body) && body.contains("data") && body["data"].is_array()) {
        for (const auto& item : body["data"]) {
            const int srno = item.value("srno", 0);
            const double value = item.value("value", 0.0);

            switch (srno) {
                case 102: funds.available_balance = value; break;
                case 220: funds.collateral       = value; break;
                case 300: funds.used_margin      = value; break;
                default: break;
            }
        }
        funds.raw_data = body;
    }

    return {true, std::move(funds), "", ts};
}

// ============================================================================
// Market Data
// ============================================================================

ApiResponse<std::vector<BrokerQuote>> MotilalBroker::get_quotes(const BrokerCredentials& creds,
                                                                   const std::vector<std::string>& symbols) {
    const int64_t ts = motilal_now_ts();
    auto& client = HttpClient::instance();

    std::vector<BrokerQuote> quotes;

    for (const auto& sym : symbols) {
        std::string exchange = "NSE";
        std::string symbol_token = sym;
        const auto colon = sym.find(':');
        if (colon != std::string::npos) {
            exchange = sym.substr(0, colon);
            symbol_token = sym.substr(colon + 1);
        }

        json payload = {
            {"exchange",    motilal_exchange(exchange)},
            {"symboltoken", symbol_token}
        };

        auto resp = client.post_json(std::string(base_url()) + "/report/v1/getltpdata",
                                      payload, auth_headers(creds));
        if (!resp.success) continue;

        const auto body = resp.json_body();
        if (!is_success(body)) continue;

        if (body.contains("data") && body["data"].is_object()) {
            const auto& d = body["data"];
            BrokerQuote q;
            q.symbol = symbol_token;
            // Motilal returns values in paisa — divide by 100
            q.ltp    = d.value("ltp", 0.0) / 100.0;
            q.open   = d.value("open", 0.0) / 100.0;
            q.high   = d.value("high", 0.0) / 100.0;
            q.low    = d.value("low", 0.0) / 100.0;
            q.close  = d.value("close", 0.0) / 100.0;
            q.volume = d.value("volume", 0.0);
            quotes.push_back(std::move(q));
        }
    }

    return {true, std::move(quotes), "", ts};
}

ApiResponse<std::vector<BrokerCandle>> MotilalBroker::get_history(const BrokerCredentials& /*creds*/,
                                                                     const std::string& /*symbol*/,
                                                                     const std::string& /*resolution*/,
                                                                     const std::string& /*from_date*/,
                                                                     const std::string& /*to_date*/) {
    // Motilal does not expose a historical candle API
    const int64_t ts = motilal_now_ts();
    return {false, std::nullopt, "Historical data not supported for Motilal", ts};
}

} // namespace fincept::trading
