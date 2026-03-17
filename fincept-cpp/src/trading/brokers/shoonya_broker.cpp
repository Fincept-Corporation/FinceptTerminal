#include "shoonya_broker.h"
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

static int64_t shoonya_now_ts() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

// SHA256 hash
std::string ShoonyaBroker::sha256_hex(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()),
           input.size(), hash);
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
    return oss.str();
}

// Build jData=...&jKey=... payload
std::string ShoonyaBroker::make_payload(const json& data, const std::string& auth_token) {
    return "jData=" + data.dump() + "&jKey=" + auth_token;
}

// Build jData=... payload (no auth, for login)
std::string ShoonyaBroker::make_payload_no_auth(const json& data) {
    return "jData=" + data.dump();
}

// ============================================================================
// Auth headers — Shoonya uses form-urlencoded, no Authorization header needed
// (auth is in jKey payload parameter)
// ============================================================================

std::map<std::string, std::string> ShoonyaBroker::auth_headers(const BrokerCredentials& /*creds*/) const {
    return {
        {"Content-Type", "application/x-www-form-urlencoded"}
    };
}

// ============================================================================
// Mapping helpers
// ============================================================================

const char* ShoonyaBroker::shoonya_product_type(ProductType p) {
    switch (p) {
        case ProductType::Delivery:      return "C";   // CNC
        case ProductType::Margin:        return "M";   // NRML
        case ProductType::Intraday:      return "I";   // MIS
        case ProductType::CoverOrder:    return "H";   // CO
        case ProductType::BracketOrder:  return "B";   // BO
        default:                         return "I";
    }
}

const char* ShoonyaBroker::shoonya_order_type(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "MKT";
        case OrderType::Limit:         return "LMT";
        case OrderType::StopLoss:      return "SL-MKT";
        case OrderType::StopLossLimit: return "SL-LMT";
        default:                       return "MKT";
    }
}

bool ShoonyaBroker::is_ok(const json& body) {
    return body.value("stat", "") == "Ok";
}

std::string ShoonyaBroker::extract_error(const json& body, const char* fallback) {
    if (body.contains("emsg") && body["emsg"].is_string())
        return body["emsg"].get<std::string>();
    return fallback;
}

double ShoonyaBroker::parse_double(const json& obj, const char* key, double def) {
    if (!obj.contains(key)) return def;
    const auto& v = obj[key];
    if (v.is_number()) return v.get<double>();
    if (v.is_string()) {
        try { return std::stod(v.get<std::string>()); }
        catch (...) { return def; }
    }
    return def;
}

// ============================================================================
// Token Exchange — SHA256-based QuickAuth with TOTP
// ============================================================================

TokenExchangeResponse ShoonyaBroker::exchange_token(const std::string& api_key,
                                                       const std::string& api_secret,
                                                       const std::string& auth_code) {
    auto& client = HttpClient::instance();

    // auth_code format: "userid|password|totp"
    // api_key = vendor_code, api_secret = api_secretkey
    // Parse auth_code
    std::string user_id, password, totp;
    {
        const auto pipe1 = auth_code.find('|');
        if (pipe1 == std::string::npos) {
            return {false, "", "", "auth_code must be 'userid|password|totp'"};
        }
        const auto pipe2 = auth_code.find('|', pipe1 + 1);
        if (pipe2 == std::string::npos) {
            return {false, "", "", "auth_code must be 'userid|password|totp'"};
        }
        user_id  = auth_code.substr(0, pipe1);
        password = auth_code.substr(pipe1 + 1, pipe2 - pipe1 - 1);
        totp     = auth_code.substr(pipe2 + 1);
    }

    // Hash password with SHA256
    const std::string pwd_hash = sha256_hex(password);

    // Hash appkey: SHA256(userid|api_secret)
    const std::string appkey_hash = sha256_hex(user_id + "|" + api_secret);

    json payload = {
        {"uid",         user_id},
        {"pwd",         pwd_hash},
        {"factor2",     totp},
        {"apkversion",  "1.0.0"},
        {"appkey",      appkey_hash},
        {"imei",        "abc1234"},
        {"vc",          api_key},
        {"source",      "API"}
    };

    Headers headers = {
        {"Content-Type", "application/x-www-form-urlencoded"}
    };

    auto resp = client.post(std::string(base_url()) + "/QuickAuth",
                             make_payload_no_auth(payload), headers);

    if (!resp.success) {
        return {false, "", "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();
    if (body.is_null()) {
        return {false, "", "", "Invalid JSON response"};
    }

    if (is_ok(body)) {
        std::string token = body.value("susertoken", "");
        return {true, token, user_id, ""};
    }

    return {false, "", "", extract_error(body, "Authentication failed")};
}

// ============================================================================
// Orders
// ============================================================================

OrderPlaceResponse ShoonyaBroker::place_order(const BrokerCredentials& creds,
                                                 const UnifiedOrder& order) {
    auto& client = HttpClient::instance();

    json payload = {
        {"uid",              creds.user_id},
        {"actid",            creds.user_id},
        {"exch",             order.exchange},
        {"tsym",             order.symbol},
        {"qty",              std::to_string(static_cast<int>(order.quantity))},
        {"prc",              std::to_string(order.price)},
        {"trgprc",           std::to_string(order.stop_price)},
        {"dscqty",           "0"},
        {"prd",              shoonya_product_type(order.product_type)},
        {"trantype",         order.side == OrderSide::Buy ? "B" : "S"},
        {"prctyp",           shoonya_order_type(order.order_type)},
        {"mkt_protection",   "0"},
        {"ret",              "DAY"},
        {"ordersource",      "API"}
    };

    Headers headers = {{"Content-Type", "application/x-www-form-urlencoded"}};

    auto resp = client.post(std::string(base_url()) + "/PlaceOrder",
                             make_payload(payload, creds.access_token), headers);

    if (!resp.success) {
        return {false, "", "Request failed: " + resp.error};
    }

    const auto body = resp.json_body();
    if (is_ok(body)) {
        return {true, body.value("norenordno", ""), ""};
    }

    return {false, "", extract_error(body, "Order placement failed")};
}

ApiResponse<json> ShoonyaBroker::modify_order(const BrokerCredentials& creds,
                                                 const std::string& order_id,
                                                 const json& modifications) {
    const int64_t ts = shoonya_now_ts();
    auto& client = HttpClient::instance();

    json payload = {
        {"uid",              creds.user_id},
        {"norenordno",       order_id},
        {"ret",              "DAY"},
        {"mkt_protection",   "0"}
    };

    if (modifications.contains("exchange"))
        payload["exch"] = modifications["exchange"];
    if (modifications.contains("symbol"))
        payload["tsym"] = modifications["symbol"];
    if (modifications.contains("quantity"))
        payload["qty"] = std::to_string(modifications["quantity"].get<int>());
    if (modifications.contains("price"))
        payload["prc"] = std::to_string(modifications["price"].get<double>());
    if (modifications.contains("trigger_price"))
        payload["trgprc"] = std::to_string(modifications["trigger_price"].get<double>());
    if (modifications.contains("order_type"))
        payload["prctyp"] = modifications["order_type"];

    Headers headers = {{"Content-Type", "application/x-www-form-urlencoded"}};

    auto resp = client.post(std::string(base_url()) + "/ModifyOrder",
                             make_payload(payload, creds.access_token), headers);

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (is_ok(body)) {
        return {true, body, "", ts};
    }
    return {false, std::nullopt, extract_error(body, "Order modification failed"), ts};
}

ApiResponse<json> ShoonyaBroker::cancel_order(const BrokerCredentials& creds,
                                                 const std::string& order_id) {
    const int64_t ts = shoonya_now_ts();
    auto& client = HttpClient::instance();

    json payload = {
        {"uid",         creds.user_id},
        {"norenordno",  order_id}
    };

    Headers headers = {{"Content-Type", "application/x-www-form-urlencoded"}};

    auto resp = client.post(std::string(base_url()) + "/CancelOrder",
                             make_payload(payload, creds.access_token), headers);

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    if (is_ok(body)) {
        return {true, body, "", ts};
    }
    return {false, std::nullopt, extract_error(body, "Order cancellation failed"), ts};
}

ApiResponse<std::vector<BrokerOrderInfo>> ShoonyaBroker::get_orders(const BrokerCredentials& creds) {
    const int64_t ts = shoonya_now_ts();
    auto& client = HttpClient::instance();

    json payload = {{"uid", creds.user_id}, {"actid", creds.user_id}};
    Headers headers = {{"Content-Type", "application/x-www-form-urlencoded"}};

    auto resp = client.post(std::string(base_url()) + "/OrderBook",
                             make_payload(payload, creds.access_token), headers);

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerOrderInfo> orders;

    // Shoonya returns array on success, object with stat=Not_Ok on error/empty
    if (body.is_array()) {
        for (const auto& o : body) {
            BrokerOrderInfo info;
            info.order_id     = o.value("norenordno", "");
            info.symbol       = o.value("tsym", "");
            info.exchange     = o.value("exch", "");
            info.side         = o.value("trantype", "");
            info.order_type   = o.value("prctyp", "");
            info.product_type = o.value("prd", "");
            info.quantity     = parse_double(o, "qty");
            info.filled_qty   = parse_double(o, "fillshares");
            info.price        = parse_double(o, "prc");
            info.avg_price    = parse_double(o, "avgprc");
            info.status       = o.value("status", "");
            info.timestamp    = o.value("norentm", "");
            orders.push_back(std::move(info));
        }
    }

    return {true, std::move(orders), "", ts};
}

ApiResponse<json> ShoonyaBroker::get_trade_book(const BrokerCredentials& creds) {
    const int64_t ts = shoonya_now_ts();
    auto& client = HttpClient::instance();

    json payload = {{"uid", creds.user_id}, {"actid", creds.user_id}};
    Headers headers = {{"Content-Type", "application/x-www-form-urlencoded"}};

    auto resp = client.post(std::string(base_url()) + "/TradeBook",
                             make_payload(payload, creds.access_token), headers);

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    // Return array as-is, or empty array if Not_Ok
    if (body.is_array()) {
        return {true, body, "", ts};
    }
    return {true, json::array(), "", ts};
}

// ============================================================================
// Portfolio
// ============================================================================

ApiResponse<std::vector<BrokerPosition>> ShoonyaBroker::get_positions(const BrokerCredentials& creds) {
    const int64_t ts = shoonya_now_ts();
    auto& client = HttpClient::instance();

    json payload = {{"uid", creds.user_id}, {"actid", creds.user_id}};
    Headers headers = {{"Content-Type", "application/x-www-form-urlencoded"}};

    auto resp = client.post(std::string(base_url()) + "/PositionBook",
                             make_payload(payload, creds.access_token), headers);

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerPosition> positions;

    if (body.is_array()) {
        for (const auto& p : body) {
            BrokerPosition pos;
            pos.symbol       = p.value("tsym", "");
            pos.exchange     = p.value("exch", "");
            pos.product_type = p.value("prd", "");
            pos.quantity     = parse_double(p, "netqty");
            pos.avg_price    = parse_double(p, "netavgprc");
            pos.ltp          = parse_double(p, "lp");
            pos.pnl          = parse_double(p, "rpnl");
            pos.day_pnl      = parse_double(p, "urmtom");
            positions.push_back(std::move(pos));
        }
    }

    return {true, std::move(positions), "", ts};
}

ApiResponse<std::vector<BrokerHolding>> ShoonyaBroker::get_holdings(const BrokerCredentials& creds) {
    const int64_t ts = shoonya_now_ts();
    auto& client = HttpClient::instance();

    json payload = {
        {"uid",   creds.user_id},
        {"actid", creds.user_id},
        {"prd",   "C"}  // Holdings are always CNC
    };
    Headers headers = {{"Content-Type", "application/x-www-form-urlencoded"}};

    auto resp = client.post(std::string(base_url()) + "/Holdings",
                             make_payload(payload, creds.access_token), headers);

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerHolding> holdings;

    if (body.is_array()) {
        for (const auto& h : body) {
            BrokerHolding hld;
            hld.symbol     = h.value("exch_tsym", h.value("tsym", ""));
            hld.exchange   = h.value("exch", "NSE");
            hld.quantity   = parse_double(h, "holdqty");
            hld.avg_price  = parse_double(h, "upldprc");
            hld.ltp        = parse_double(h, "lp");
            hld.pnl        = parse_double(h, "rpnl");
            holdings.push_back(std::move(hld));
        }
    }

    return {true, std::move(holdings), "", ts};
}

ApiResponse<BrokerFunds> ShoonyaBroker::get_funds(const BrokerCredentials& creds) {
    const int64_t ts = shoonya_now_ts();
    auto& client = HttpClient::instance();

    json payload = {{"uid", creds.user_id}, {"actid", creds.user_id}};
    Headers headers = {{"Content-Type", "application/x-www-form-urlencoded"}};

    auto resp = client.post(std::string(base_url()) + "/Limits",
                             make_payload(payload, creds.access_token), headers);

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();

    if (!is_ok(body)) {
        return {false, std::nullopt, extract_error(body, "Failed to fetch funds"), ts};
    }

    BrokerFunds funds;
    // Formula from OpenAlgo: total_available = cash + payin - marginused
    const double cash = parse_double(body, "cash");
    const double payin = parse_double(body, "payin");
    const double margin_used = parse_double(body, "marginused");

    funds.available_balance = cash + payin - margin_used;
    funds.collateral       = parse_double(body, "brkcollamt");
    funds.used_margin      = margin_used;
    funds.raw_data         = body;

    return {true, std::move(funds), "", ts};
}

// ============================================================================
// Market Data
// ============================================================================

ApiResponse<std::vector<BrokerQuote>> ShoonyaBroker::get_quotes(const BrokerCredentials& creds,
                                                                   const std::vector<std::string>& symbols) {
    const int64_t ts = shoonya_now_ts();
    auto& client = HttpClient::instance();

    std::vector<BrokerQuote> quotes;

    for (const auto& sym : symbols) {
        // Symbol format: "EXCHANGE:TOKEN" e.g. "NSE:22"
        std::string exchange = "NSE";
        std::string token = sym;
        const auto colon = sym.find(':');
        if (colon != std::string::npos) {
            exchange = sym.substr(0, colon);
            token = sym.substr(colon + 1);
        }

        // Normalize index exchanges
        if (exchange == "NSE_INDEX") exchange = "NSE";
        if (exchange == "BSE_INDEX") exchange = "BSE";

        json payload = {
            {"uid",   creds.user_id},
            {"exch",  exchange},
            {"token", token}
        };

        Headers headers = {{"Content-Type", "application/x-www-form-urlencoded"}};

        auto resp = client.post(std::string(base_url()) + "/GetQuotes",
                                 make_payload(payload, creds.access_token), headers);
        if (!resp.success) continue;

        const auto body = resp.json_body();
        if (!is_ok(body)) continue;

        BrokerQuote q;
        q.symbol = body.value("tsym", token);
        q.ltp    = parse_double(body, "lp");
        q.open   = parse_double(body, "o");
        q.high   = parse_double(body, "h");
        q.low    = parse_double(body, "l");
        q.close  = parse_double(body, "c");
        q.volume = parse_double(body, "v");
        q.bid    = parse_double(body, "bp1");
        q.ask    = parse_double(body, "sp1");
        quotes.push_back(std::move(q));
    }

    return {true, std::move(quotes), "", ts};
}

ApiResponse<std::vector<BrokerCandle>> ShoonyaBroker::get_history(const BrokerCredentials& creds,
                                                                     const std::string& symbol,
                                                                     const std::string& resolution,
                                                                     const std::string& from_date,
                                                                     const std::string& to_date) {
    const int64_t ts = shoonya_now_ts();
    auto& client = HttpClient::instance();

    // Parse symbol format "EXCHANGE:TOKEN:TRADINGSYMBOL"
    std::string exchange = "NSE";
    std::string token = symbol;
    std::string tsym = symbol;
    {
        const auto colon1 = symbol.find(':');
        if (colon1 != std::string::npos) {
            exchange = symbol.substr(0, colon1);
            const auto colon2 = symbol.find(':', colon1 + 1);
            if (colon2 != std::string::npos) {
                token = symbol.substr(colon1 + 1, colon2 - colon1 - 1);
                tsym = symbol.substr(colon2 + 1);
            } else {
                token = symbol.substr(colon1 + 1);
                tsym = token;
            }
        }
    }

    if (exchange == "NSE_INDEX") exchange = "NSE";
    if (exchange == "BSE_INDEX") exchange = "BSE";

    // Convert dates (YYYY-MM-DD) to Unix timestamps
    auto parse_date = [](const std::string& date_str, int hour, int min, int sec) -> int64_t {
        int y = 0, m = 0, d = 0;
        if (sscanf(date_str.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return 0;
        struct tm t = {};
        t.tm_year = y - 1900;
        t.tm_mon  = m - 1;
        t.tm_mday = d;
        t.tm_hour = hour;
        t.tm_min  = min;
        t.tm_sec  = sec;
#ifdef _WIN32
        return _mkgmtime(&t);
#else
        return timegm(&t);
#endif
    };

    const int64_t start_ts = parse_date(from_date, 0, 0, 0);
    const int64_t end_ts = parse_date(to_date, 23, 59, 59);

    // Choose endpoint and build payload
    std::string endpoint;
    json payload;

    if (resolution == "D" || resolution == "day" || resolution == "1d") {
        // Daily data uses EODChartData
        endpoint = std::string(base_url()) + "/EODChartData";
        const std::string sym_str = exchange + ":" + tsym;
        payload = {
            {"uid",  creds.user_id},
            {"sym",  sym_str},
            {"from", std::to_string(start_ts)},
            {"to",   std::to_string(end_ts)}
        };
    } else {
        // Intraday uses TPSeries
        endpoint = std::string(base_url()) + "/TPSeries";
        payload = {
            {"uid",   creds.user_id},
            {"exch",  exchange},
            {"token", token},
            {"st",    std::to_string(start_ts)},
            {"et",    std::to_string(end_ts)},
            {"intrv", resolution}
        };
    }

    Headers headers = {{"Content-Type", "application/x-www-form-urlencoded"}};

    auto resp = client.post(endpoint,
                             make_payload(payload, creds.access_token), headers);

    if (!resp.success) {
        return {false, std::nullopt, "Request failed: " + resp.error, ts};
    }

    const auto body = resp.json_body();
    std::vector<BrokerCandle> candles;

    if (body.is_array()) {
        for (const auto& c : body) {
            const double into = parse_double(c, "into");
            const double inth = parse_double(c, "inth");
            const double intl = parse_double(c, "intl");
            const double intc = parse_double(c, "intc");

            // Skip zero candles
            if (into == 0.0 && inth == 0.0 && intl == 0.0 && intc == 0.0)
                continue;

            BrokerCandle candle;
            candle.open   = into;
            candle.high   = inth;
            candle.low    = intl;
            candle.close  = intc;
            candle.volume = parse_double(c, "intv");

            // Parse timestamp
            if (resolution == "D" || resolution == "day" || resolution == "1d") {
                // EOD: ssboe field
                candle.timestamp = static_cast<int64_t>(parse_double(c, "ssboe"));
            } else {
                // Intraday: "time" field as "DD-MM-YYYY HH:MM:SS"
                if (c.contains("time") && c["time"].is_string()) {
                    const std::string time_str = c["time"].get<std::string>();
                    int dy = 0, dm = 0, dd = 0, th = 0, tm_val = 0, ts_val = 0;
                    if (sscanf(time_str.c_str(), "%d-%d-%d %d:%d:%d",
                               &dd, &dm, &dy, &th, &tm_val, &ts_val) == 6) {
                        struct tm t = {};
                        t.tm_year = dy - 1900;
                        t.tm_mon  = dm - 1;
                        t.tm_mday = dd;
                        t.tm_hour = th;
                        t.tm_min  = tm_val;
                        t.tm_sec  = ts_val;
#ifdef _WIN32
                        candle.timestamp = _mkgmtime(&t);
#else
                        candle.timestamp = timegm(&t);
#endif
                    }
                }
            }

            candles.push_back(candle);
        }
    }

    return {true, std::move(candles), "", ts};
}

} // namespace fincept::trading
