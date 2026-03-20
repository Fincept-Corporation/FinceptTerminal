#pragma once
// Groww Broker — Indian stock broker integration
// API Base: https://api.groww.in
// Auth: TOTP-based flow, Bearer token for requests
// Response pattern: { "status": "SUCCESS", "payload": { ... } }

#include "trading/broker_interface.h"

namespace fincept::trading {

class GrowwBroker : public IBroker {
public:
    BrokerId id() const override { return BrokerId::Groww; }
    const char* name() const override { return "Groww"; }
    const char* base_url() const override { return "https://api.groww.in"; }
    const char* ws_adapter_name() const override { return "groww"; }

    // Groww uses TOTP-based token exchange
    // api_key = bearer key for TOTP request, api_secret = TOTP secret (base32), auth_code = unused
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
                                                        const std::string& interval,
                                                        const std::string& from_date,
                                                        const std::string& to_date) override;

    // --- Groww-specific extensions ---
    ApiResponse<bool> validate_token(const BrokerCredentials& creds);

protected:
    std::map<std::string, std::string> auth_headers(const BrokerCredentials& creds) const override;

private:
    // Groww segment mapping
    static const char* groww_segment(const std::string& exchange);
    static const char* groww_exchange(const std::string& exchange);
    static const char* groww_order_type(OrderType t);

    // Check Groww "status":"SUCCESS" pattern
    static bool is_success(const json& body);
    static std::string extract_error(const json& body, const char* fallback);

    // Interval mapping for historical data (minutes)
    static const char* groww_interval(const std::string& resolution);
};

} // namespace fincept::trading
