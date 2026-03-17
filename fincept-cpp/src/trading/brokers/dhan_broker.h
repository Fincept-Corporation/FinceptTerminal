#pragma once
// Dhan Broker — Indian stock broker integration
// API Base: https://api.dhan.co
// Auth Base: https://auth.dhan.co
// Auth: access-token + client-id headers for all requests
// Docs: https://dhanhq.co/docs/v2/

#include "trading/broker_interface.h"

namespace fincept::trading {

class DhanBroker : public IBroker {
public:
    BrokerId id() const override { return BrokerId::Dhan; }
    const char* name() const override { return "Dhan"; }
    const char* base_url() const override { return "https://api.dhan.co"; }

    // Dhan uses consent-based OAuth
    // api_key = app_id, api_secret = app_secret, auth_code = token_id from consent flow
    TokenExchangeResponse exchange_token(const std::string& api_key,
                                          const std::string& api_secret,
                                          const std::string& token_id) override;

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
                                                        const std::string& security_id,
                                                        const std::string& interval,
                                                        const std::string& from_date,
                                                        const std::string& to_date) override;

    // --- Dhan-specific extensions ---

    // Validate token by fetching fund limits
    ApiResponse<bool> validate_token(const BrokerCredentials& creds);

protected:
    std::map<std::string, std::string> auth_headers(const BrokerCredentials& creds) const override;

private:
    static constexpr const char* auth_base_ = "https://auth.dhan.co";

    // Common API helper — sends request with Dhan-specific headers
    struct DhanApiResult {
        bool success = false;
        json body;
        std::string error;
    };
    DhanApiResult dhan_api_call(const BrokerCredentials& creds,
                                 const std::string& endpoint,
                                 const std::string& method,
                                 const json& payload = json()) const;

    // Exchange segment mapping: NSE -> NSE_EQ, BSE -> BSE_EQ, etc.
    static const char* map_exchange_to_dhan(const std::string& exchange);
    static const char* map_product_to_dhan(ProductType p);
    static const char* map_order_type_to_dhan(OrderType t);
};

} // namespace fincept::trading
