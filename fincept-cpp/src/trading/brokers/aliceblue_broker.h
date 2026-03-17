#pragma once
// AliceBlue Broker — Indian stock broker integration
// API Base: https://ant.aliceblueonline.com/rest/AliceBlueAPIService/api
// Auth: SHA256 checksum-based session (userId + apiSecret + encKey)
// Response pattern: { "stat": "Ok"/"Not_Ok", ... } or arrays
// Auth header: "Bearer {api_secret} {session_id}"

#include "trading/broker_interface.h"

namespace fincept::trading {

class AliceBlueBroker : public IBroker {
public:
    BrokerId id() const override { return BrokerId::AliceBlue; }
    const char* name() const override { return "AliceBlue"; }
    const char* base_url() const override {
        return "https://ant.aliceblueonline.com/rest/AliceBlueAPIService/api";
    }

    // AliceBlue uses SHA256 checksum session:
    // api_key = user_id, api_secret = api_secret, auth_code = enc_key
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

    // --- AliceBlue-specific extensions ---
    ApiResponse<bool> validate_session(const BrokerCredentials& creds);

protected:
    // AliceBlue auth: "Bearer {api_secret} {session_id}"
    // session_id is stored in access_token field
    std::map<std::string, std::string> auth_headers(const BrokerCredentials& creds) const override;

private:
    // Generate SHA256 checksum: hex(SHA256(userId + apiSecret + encKey))
    static std::string generate_checksum(const std::string& user_id,
                                          const std::string& api_secret,
                                          const std::string& enc_key);

    // AliceBlue order type mapping
    static const char* ab_order_type(OrderType t);

    // Parse double from string (AliceBlue returns numeric values as strings)
    static double parse_double(const json& val);
    static int64_t parse_int(const json& val);
};

} // namespace fincept::trading
