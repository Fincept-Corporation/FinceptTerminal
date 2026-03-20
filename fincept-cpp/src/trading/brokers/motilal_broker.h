#pragma once
// Motilal Oswal Broker — Indian stock broker integration
// API Base: https://openapi.motilaloswal.com/rest
// Auth: SHA256(password + apikey) → AuthToken
// Response pattern: { "status": "SUCCESS", "AuthToken": "..." }
// Requires many custom headers (ApiKey, vendor info, device info, etc.)

#include "trading/broker_interface.h"

namespace fincept::trading {

class MotilalBroker : public IBroker {
public:
    BrokerId id() const override { return BrokerId::Motilal; }
    const char* name() const override { return "Motilal"; }
    const char* base_url() const override { return "https://openapi.motilaloswal.com/rest"; }
    const char* ws_adapter_name() const override { return "motilal"; }

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
    // Motilal custom headers (extensive set required by their API)
    static std::map<std::string, std::string> motilal_base_headers(const std::string& api_key);

    // Mapping helpers
    static const char* motilal_exchange(const std::string& exchange);
    static const char* motilal_order_type(OrderType t);
    static const char* motilal_product_type(ProductType p);

    // Response helpers
    static bool is_success(const json& body);
    static std::string extract_error(const json& body, const char* fallback);
};

} // namespace fincept::trading
