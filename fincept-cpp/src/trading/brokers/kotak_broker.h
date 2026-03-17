#pragma once
// Kotak Neo Broker — Indian stock broker integration
// Auth Base: https://mis.kotaksecurities.com
// Trading API: Dynamic base URL returned after MPIN validation
// Auth Flow: 2-step (TOTP login -> MPIN validate)
// Auth token format: trading_token:::trading_sid:::base_url:::access_token
// Docs: Kotak Neo Trade API

#include "trading/broker_interface.h"

namespace fincept::trading {

class KotakBroker : public IBroker {
public:
    BrokerId id() const override { return BrokerId::Kotak; }
    const char* name() const override { return "Kotak"; }
    const char* base_url() const override { return "https://mis.kotaksecurities.com"; }

    // Kotak uses a 2-step auth: TOTP login + MPIN validate
    // For the unified interface, we combine both steps:
    //   api_key = access_token (consumer key token)
    //   api_secret = "ucc:mobile_number:totp:mpin" (colon-separated)
    //   auth_code = unused (pass empty)
    // Alternatively, the caller can perform both steps and pass the composite auth string
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

    // --- Kotak-specific extensions ---

    // Validate auth token by fetching orders
    ApiResponse<bool> validate_token(const BrokerCredentials& creds);

protected:
    std::map<std::string, std::string> auth_headers(const BrokerCredentials& creds) const override;

private:
    // Parse composite auth token: "trading_token:::trading_sid:::base_url:::access_token"
    struct KotakAuth {
        std::string trading_token;
        std::string trading_sid;
        std::string trading_base_url;
        std::string access_token;
        bool valid = false;
    };
    static KotakAuth parse_auth_token(const std::string& auth_token);

    // Make request to Kotak trading API (uses Auth/Sid headers, form-urlencoded jData payload)
    struct KotakApiResult {
        bool success = false;
        json body;
        std::string error;
    };
    KotakApiResult kotak_api_call(const KotakAuth& auth,
                                    const std::string& endpoint,
                                    const std::string& method,
                                    const std::string& payload = "") const;

    // Make quotes request (uses Authorization header, JSON content)
    KotakApiResult kotak_quotes_call(const KotakAuth& auth,
                                      const std::string& endpoint) const;

    // URL-encode a string
    static std::string url_encode(const std::string& s);

    // Encode JSON as jData={url_encoded_json}
    static std::string encode_jdata(const json& data);

    // Exchange mapping
    static const char* map_exchange_to_kotak(const std::string& exchange);
    static const char* map_order_type_to_kotak(OrderType t);
};

} // namespace fincept::trading
