#pragma once
// Broker Interface — abstract base class for all 16 broker integrations
// All 16 fully implemented: Fyers, Zerodha, AngelOne, Upstox, Dhan, Kotak, Groww, AliceBlue, 5Paisa, IIFL, Motilal, Shoonya, Alpaca, IBKR, Tradier, SaxoBank

#include "broker_types.h"
#include <memory>

namespace fincept::trading {

class IBroker {
public:
    virtual ~IBroker() = default;

    // Identity
    virtual BrokerId id() const = 0;
    virtual const char* name() const = 0;
    virtual const char* base_url() const = 0;

    // --- Authentication ---
    virtual TokenExchangeResponse exchange_token(const std::string& api_key,
                                                  const std::string& api_secret,
                                                  const std::string& auth_code) = 0;

    // --- Orders ---
    virtual OrderPlaceResponse place_order(const BrokerCredentials& creds,
                                            const UnifiedOrder& order) = 0;
    virtual ApiResponse<json> modify_order(const BrokerCredentials& creds,
                                            const std::string& order_id,
                                            const json& modifications) = 0;
    virtual ApiResponse<json> cancel_order(const BrokerCredentials& creds,
                                            const std::string& order_id) = 0;
    virtual ApiResponse<std::vector<BrokerOrderInfo>> get_orders(const BrokerCredentials& creds) = 0;
    virtual ApiResponse<json> get_trade_book(const BrokerCredentials& creds) = 0;

    // --- Portfolio ---
    virtual ApiResponse<std::vector<BrokerPosition>> get_positions(const BrokerCredentials& creds) = 0;
    virtual ApiResponse<std::vector<BrokerHolding>> get_holdings(const BrokerCredentials& creds) = 0;
    virtual ApiResponse<BrokerFunds> get_funds(const BrokerCredentials& creds) = 0;

    // --- Market Data ---
    virtual ApiResponse<std::vector<BrokerQuote>> get_quotes(const BrokerCredentials& creds,
                                                              const std::vector<std::string>& symbols) = 0;
    virtual ApiResponse<std::vector<BrokerCandle>> get_history(const BrokerCredentials& creds,
                                                                const std::string& symbol,
                                                                const std::string& resolution,
                                                                const std::string& from_date,
                                                                const std::string& to_date) = 0;

    // --- Credential helpers ---
    // Load credentials from DB for this broker
    BrokerCredentials load_credentials() const;
    void save_credentials(const BrokerCredentials& creds) const;

protected:
    // HTTP helper: build auth headers for this broker
    virtual std::map<std::string, std::string> auth_headers(const BrokerCredentials& creds) const = 0;
};

using BrokerPtr = std::unique_ptr<IBroker>;

} // namespace fincept::trading
