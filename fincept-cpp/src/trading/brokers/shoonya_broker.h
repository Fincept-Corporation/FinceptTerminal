#pragma once
// Shoonya (Finvasia) Broker — Indian stock broker integration
// API Base: https://api.shoonya.com/NorenWClientTP
// Auth: SHA256-based QuickAuth with TOTP, jData/jKey payload format
// Response pattern: { "stat": "Ok", ... } or array on success
// Content-Type: application/x-www-form-urlencoded (jData=...&jKey=...)

#include "trading/broker_interface.h"

namespace fincept::trading {

class ShoonyaBroker : public IBroker {
public:
    BrokerId id() const override { return BrokerId::Shoonya; }
    const char* name() const override { return "Shoonya"; }
    const char* base_url() const override { return "https://api.shoonya.com/NorenWClientTP"; }
    const char* ws_adapter_name() const override { return "shoonya"; }

    TokenExchangeResponse exchange_token(const std::string& api_key,
                                          const std::string& api_secret,
                                          const std::string& auth_code) override;

    OrderPlaceResponse place_order(const BrokerCredentials& creds,
                                    const UnifiedOrder& order) override;
    ApiResponse<json> modify_order(const BrokerCredentials& creds,
                                    const std::string& order_id,
                                    const json& modifications) override;
    ApiResponse<json> cancel_order(const BrokerCredentials& creds,
                                    const std::string& order_id) override;
    ApiResponse<std::vector<BrokerOrderInfo>> get_orders(const BrokerCredentials& creds) override;
    ApiResponse<json> get_trade_book(const BrokerCredentials& creds) override;

    ApiResponse<std::vector<BrokerPosition>> get_positions(const BrokerCredentials& creds) override;
    ApiResponse<std::vector<BrokerHolding>> get_holdings(const BrokerCredentials& creds) override;
    ApiResponse<BrokerFunds> get_funds(const BrokerCredentials& creds) override;

    ApiResponse<std::vector<BrokerQuote>> get_quotes(const BrokerCredentials& creds,
                                                      const std::vector<std::string>& symbols) override;
    ApiResponse<std::vector<BrokerCandle>> get_history(const BrokerCredentials& creds,
                                                        const std::string& symbol,
                                                        const std::string& resolution,
                                                        const std::string& from_date,
                                                        const std::string& to_date) override;

protected:
    std::map<std::string, std::string> auth_headers(const BrokerCredentials& creds) const override;

private:
    // Shoonya uses form-urlencoded jData/jKey payloads
    static std::string make_payload(const json& data, const std::string& auth_token);
    static std::string make_payload_no_auth(const json& data);

    // SHA256 hash helper
    static std::string sha256_hex(const std::string& input);

    // Mapping helpers
    static const char* shoonya_product_type(ProductType p);
    static const char* shoonya_order_type(OrderType t);

    // Response helpers — Shoonya uses "stat":"Ok"/"Not_Ok"
    static bool is_ok(const json& body);
    static std::string extract_error(const json& body, const char* fallback);

    // Parse string numerics (Shoonya returns numbers as strings)
    static double parse_double(const json& obj, const char* key, double def = 0.0);
};

} // namespace fincept::trading
