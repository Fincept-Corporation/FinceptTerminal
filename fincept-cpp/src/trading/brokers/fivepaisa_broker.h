#pragma once
// 5Paisa Broker — Indian stock broker integration
// API Base: https://Openapi.5paisa.com
// Auth: 2-step TOTP login then request token exchange
// Request pattern: { "head": { "key": "..." }, "body": { ... } }
// Response pattern: { "head": { "statusDescription": "Success", "status": "0" }, "body": { ... } }
// All requests require "bearer {access_token}" Authorization header

#include "trading/broker_interface.h"

namespace fincept::trading {

class FivePaisaBroker : public IBroker {
public:
    BrokerId id() const override { return BrokerId::FivePaisa; }
    const char* name() const override { return "5Paisa"; }
    const char* base_url() const override { return "https://Openapi.5paisa.com"; }
    const char* ws_adapter_name() const override { return "fivepaisa"; }

    // 5Paisa uses 2-step auth:
    // Step 1: TOTP login to get request_token (api_key = app key, api_secret = enc_key, auth_code = "email:pin:totp")
    // Step 2: Exchange request_token for access_token
    // For exchange_token: api_key = Key, api_secret = EncryKey, auth_code = request_token
    // user_id must be in BrokerCredentials
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

    // --- 5Paisa-specific extensions ---
    ApiResponse<bool> validate_token(const BrokerCredentials& creds);

protected:
    std::map<std::string, std::string> auth_headers(const BrokerCredentials& creds) const override;

private:
    // Build standard 5Paisa request wrapper: { "head": { "key": ... }, "body": { ... } }
    static json wrap_request(const std::string& api_key, const json& body);

    // 5Paisa exchange mapping (NSE -> "N", BSE -> "B", MCX -> "M")
    static const char* fp_exchange(const std::string& exchange);
    // 5Paisa exchange type (Cash -> "C", Derivative -> "D", Currency -> "U")
    static const char* fp_exchange_type(const std::string& exchange);
    // Order side (BUY -> "B", SELL -> "S")
    static const char* fp_order_side(OrderSide side);
    // Product type to IsIntraday boolean
    static bool fp_is_intraday(ProductType product);

    // Check 5Paisa response success
    static bool is_success(const json& body);
    static std::string extract_error(const json& body, const char* fallback);
};

} // namespace fincept::trading
