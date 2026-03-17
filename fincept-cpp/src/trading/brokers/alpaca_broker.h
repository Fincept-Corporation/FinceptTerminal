#pragma once
// Alpaca Broker — US stock/crypto broker with paper trading support
// Paper API: paper-api.alpaca.markets, Live: api.alpaca.markets
// Data API: data.alpaca.markets (separate base URL for market data)
// Auth: APCA-API-KEY-ID + APCA-API-SECRET-KEY headers (no token exchange)

#include "trading/broker_interface.h"

namespace fincept::trading {

class AlpacaBroker : public IBroker {
public:
    BrokerId id() const override { return BrokerId::Alpaca; }
    const char* name() const override { return "Alpaca"; }
    const char* base_url() const override { return "https://paper-api.alpaca.markets"; }

    // Data API uses a different base URL
    const char* data_url() const { return "https://data.alpaca.markets"; }

    // Auth — Alpaca validates by fetching /v2/account
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

    // Market Data (uses data API)
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
    static const char* alpaca_order_type(OrderType t);
    static const char* alpaca_side(OrderSide s);
    static const char* alpaca_tif(const std::string& validity);
    static const char* alpaca_timeframe(const std::string& resolution);
    static std::string extract_error(const json& body, const char* fallback);
};

} // namespace fincept::trading
