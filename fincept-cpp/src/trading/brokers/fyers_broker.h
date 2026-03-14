#pragma once
// Fyers Broker — Indian stock broker integration
// API Base: https://api-t1.fyers.in
// Auth: SHA256(api_key:api_secret) for token exchange, api_key:access_token for requests

#include "trading/broker_interface.h"

namespace fincept::trading {

class FyersBroker : public IBroker {
public:
    BrokerId id() const override { return BrokerId::Fyers; }
    const char* name() const override { return "Fyers"; }
    const char* base_url() const override { return "https://api-t1.fyers.in"; }

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
    // Fyers order type mapping: Market=2, Limit=1, StopLoss=3, StopLossLimit=4
    static int fyers_order_type(OrderType t);
    // Fyers side mapping: Buy=1, Sell=-1
    static int fyers_side(OrderSide s);
    // Fyers product type mapping
    static std::string fyers_product(ProductType p);
};

} // namespace fincept::trading
