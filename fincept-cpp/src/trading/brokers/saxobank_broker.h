#pragma once
// Saxo Bank Broker — EU/Global multi-asset broker
// Sim API: gateway.saxobank.com/sim/openapi
// Live API: gateway.saxobank.com/openapi
// Auth: OAuth 2.0 (authorization_code grant), Bearer token
// Uses ClientKey/AccountKey for portfolio, UIC (instrument ID) for market data
// Documentation: https://www.developer.saxo/openapi/learn

#include "trading/broker_interface.h"

namespace fincept::trading {

class SaxoBankBroker : public IBroker {
public:
    BrokerId id() const override { return BrokerId::SaxoBank; }
    const char* name() const override { return "Saxo Bank"; }
    const char* base_url() const override { return "https://gateway.saxobank.com/openapi"; }

    // Sim (paper) mode base URL
    const char* sim_url() const { return "https://gateway.saxobank.com/sim/openapi"; }

    // Auth — OAuth 2.0 authorization_code exchange
    TokenExchangeResponse exchange_token(const std::string& api_key,
                                          const std::string& api_secret,
                                          const std::string& auth_code) override;

    // Orders
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

    // Market Data (UIC-based)
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
    // ClientKey and AccountKey are stored in additional_data JSON
    static std::string get_client_key(const BrokerCredentials& creds);
    static std::string get_account_key(const BrokerCredentials& creds);
    static int saxo_horizon(const std::string& resolution);
    static const char* saxo_order_type(OrderType t);
    static const char* saxo_order_duration(const std::string& validity);
    static std::string extract_error(const json& body, const char* fallback);
};

} // namespace fincept::trading
