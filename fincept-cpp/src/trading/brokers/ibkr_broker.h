#pragma once
// IBKR (Interactive Brokers) — US/Global broker
// Gateway mode: localhost:5000/v1/api (Client Portal Gateway)
// API mode: api.ibkr.com/v1/api (Cloud API)
// Auth: Session-based with Bearer token, no traditional token exchange
// Uses conid (contract ID) for instruments, account_id for portfolio

#include "trading/broker_interface.h"

namespace fincept::trading {

class IBKRBroker : public IBroker {
public:
    BrokerId id() const override { return BrokerId::IBKR; }
    const char* name() const override { return "IBKR"; }
    const char* base_url() const override { return "https://localhost:5000/v1/api"; }

    // Auth — session-based (check status, tickle keepalive)
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
    // account_id is stored in additional_data or user_id
    static std::string get_account_id(const BrokerCredentials& creds);
    static const char* ibkr_order_type(OrderType t);
    static const char* ibkr_side(OrderSide s);
    static const char* ibkr_period(const std::string& resolution);
    static const char* ibkr_bar(const std::string& resolution);
    static std::string extract_error(const json& body, const char* fallback);
};

} // namespace fincept::trading
