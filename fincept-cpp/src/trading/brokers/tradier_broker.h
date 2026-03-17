#pragma once
// Tradier Broker — US stock/options broker
// Live: api.tradier.com/v1, Sandbox: sandbox.tradier.com/v1
// Auth: Bearer token
// Orders use application/x-www-form-urlencoded (not JSON)
// Errors in fault.faultstring or errors.error

#include "trading/broker_interface.h"

namespace fincept::trading {

class TradierBroker : public IBroker {
public:
    BrokerId id() const override { return BrokerId::Tradier; }
    const char* name() const override { return "Tradier"; }
    const char* base_url() const override { return "https://api.tradier.com/v1"; }

    // Auth — Bearer token, validated via /user/profile
    TokenExchangeResponse exchange_token(const std::string& api_key,
                                          const std::string& api_secret,
                                          const std::string& auth_code) override;

    // Orders (form-urlencoded)
    OrderPlaceResponse place_order(const BrokerCredentials& creds,
                                    const UnifiedOrder& order) override;
    ApiResponse<json> modify_order(const BrokerCredentials& creds,
                                    const std::string& order_id,
                                    const json& modifications) override;
    ApiResponse<json> cancel_order(const BrokerCredentials& creds,
                                    const std::string& order_id) override;
    ApiResponse<std::vector<BrokerOrderInfo>> get_orders(const BrokerCredentials& creds) override;
    ApiResponse<json> get_trade_book(const BrokerCredentials& creds) override;

    // Portfolio
    ApiResponse<std::vector<BrokerPosition>> get_positions(const BrokerCredentials& creds) override;
    ApiResponse<std::vector<BrokerHolding>> get_holdings(const BrokerCredentials& creds) override;
    ApiResponse<BrokerFunds> get_funds(const BrokerCredentials& creds) override;

    // Market Data
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
    // account_id stored in user_id or additional_data
    static std::string get_account_id(const BrokerCredentials& creds);
    static const char* tradier_order_type(OrderType t);
    static const char* tradier_side(OrderSide s);
    static const char* tradier_interval(const std::string& resolution);
    static std::string extract_error(const json& body, const char* fallback);
    static std::string url_encode_form(const std::map<std::string, std::string>& params);
};

} // namespace fincept::trading
