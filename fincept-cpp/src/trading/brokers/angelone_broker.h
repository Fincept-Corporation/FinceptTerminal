#pragma once
// AngelOne (Angel Broking) Broker — Indian stock broker integration
// API Base: https://apiconnect.angelone.in
// Auth: TOTP-based login (client_code + password + TOTP), Bearer token for requests
// API Reference: https://smartapi.angelone.in/docs

#include "trading/broker_interface.h"

namespace fincept::trading {

class AngelOneBroker : public IBroker {
public:
    BrokerId id() const override { return BrokerId::AngelOne; }
    const char* name() const override { return "AngelOne"; }
    const char* base_url() const override { return "https://apiconnect.angelone.in"; }
    const char* ws_adapter_name() const override { return "angelone"; }

    // AngelOne uses TOTP-based login, not standard OAuth token exchange.
    // api_key = API key, api_secret = password (PIN), auth_code = TOTP secret/code
    TokenExchangeResponse exchange_token(const std::string& api_key,
                                          const std::string& password,
                                          const std::string& totp) override;

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
                                                      const std::vector<std::string>& tokens) override;
    ApiResponse<std::vector<BrokerCandle>> get_history(const BrokerCredentials& creds,
                                                        const std::string& symbol_token,
                                                        const std::string& interval,
                                                        const std::string& from_date,
                                                        const std::string& to_date) override;

    // --- AngelOne-specific extensions (match Tauri feature parity) ---

    // Validate access token by fetching profile
    ApiResponse<bool> validate_token(const BrokerCredentials& creds);

    // Refresh session using refresh token
    ApiResponse<json> refresh_token(const BrokerCredentials& creds);

    // Calculate margin for orders
    ApiResponse<json> calculate_margin(const BrokerCredentials& creds, const json& orders);

protected:
    std::map<std::string, std::string> auth_headers(const BrokerCredentials& creds) const override;

private:
    // Common API call helper — builds AngelOne-specific headers and handles response
    struct AngelApiResult {
        bool success = false;
        json body;
        std::string error;
    };
    AngelApiResult angel_api_call(const BrokerCredentials& creds,
                                   const std::string& endpoint,
                                   const std::string& method,
                                   const json& payload = json()) const;

    // AngelOne order type mapping
    static const char* angel_order_type(OrderType t);
    static const char* angel_product(ProductType p);
    static const char* angel_side(OrderSide s);
    static const char* angel_variety(ProductType p);
    static const char* angel_duration(const std::string& validity);
    static const char* angel_exchange(const std::string& exchange);
};

} // namespace fincept::trading
