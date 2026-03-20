#pragma once
// Upstox Broker — Indian stock broker integration
// API Base v2: https://api.upstox.com/v2
// API Base v3: https://api.upstox.com/v3
// Auth: OAuth2 (authorization_code grant), Bearer token for requests
// Docs: https://upstox.com/developer/api-documentation

#include "trading/broker_interface.h"

namespace fincept::trading {

class UpstoxBroker : public IBroker {
public:
    BrokerId id() const override { return BrokerId::Upstox; }
    const char* name() const override { return "Upstox"; }
    const char* base_url() const override { return "https://api.upstox.com/v2"; }
    const char* ws_adapter_name() const override { return "upstox"; }

    // Upstox uses OAuth2 authorization_code flow
    // api_key = client_id, api_secret = client_secret, auth_code = authorization code
    // NOTE: redirect_uri is stored in additional_data of BrokerCredentials
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
                                                      const std::vector<std::string>& instruments) override;
    ApiResponse<std::vector<BrokerCandle>> get_history(const BrokerCredentials& creds,
                                                        const std::string& instrument_key,
                                                        const std::string& interval,
                                                        const std::string& from_date,
                                                        const std::string& to_date) override;

    // --- Upstox-specific extensions ---

    // Validate access token by fetching user profile
    ApiResponse<bool> validate_token(const BrokerCredentials& creds);

protected:
    std::map<std::string, std::string> auth_headers(const BrokerCredentials& creds) const override;

private:
    static constexpr const char* api_base_v3_ = "https://api.upstox.com/v3";

    // Upstox order type mapping
    static const char* upstox_order_type(OrderType t);
    static const char* upstox_side(OrderSide s);
    static const char* upstox_product(ProductType p);

    // URL-encode a string for query params
    static std::string url_encode(const std::string& s);

    // Check Upstox "status":"success" pattern
    static bool is_success(const json& body);
    static std::string extract_error(const json& body, const char* fallback);
};

} // namespace fincept::trading
