#pragma once
// IIFL (XTS) Broker — Indian stock broker integration
// Interactive API: https://ttblaze.iifl.com/interactive
// Market Data API: https://ttblaze.iifl.com/apimarketdata
// Auth: appKey + secretKey → token, token passed in headers
// Response pattern: { "type": "success", "result": { ... } }

#include "trading/broker_interface.h"

namespace fincept::trading {

class IIFLBroker : public IBroker {
public:
    BrokerId id() const override { return BrokerId::IIFL; }
    const char* name() const override { return "IIFL"; }
    const char* base_url() const override { return "https://ttblaze.iifl.com"; }

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
    // IIFL exchange segment mapping
    static const char* iifl_exchange_segment(const std::string& exchange);
    static int iifl_segment_id(const std::string& exchange);
    static const char* iifl_order_type(OrderType t);
    static const char* iifl_product_type(ProductType p);

    // Response helpers
    static bool is_success(const json& body);
    static std::string extract_error(const json& body, const char* fallback);
};

} // namespace fincept::trading
