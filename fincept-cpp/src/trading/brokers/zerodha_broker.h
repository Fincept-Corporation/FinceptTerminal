#pragma once
// Zerodha (Kite) Broker — India's largest stock broker integration
// API Base: https://api.kite.trade
// Auth: SHA256(api_key + request_token + api_secret), token api_key:access_token for requests
// Content-Type: application/x-www-form-urlencoded for most endpoints
// API Version: 3

#include "trading/broker_interface.h"

namespace fincept::trading {

class ZerodhaBroker : public IBroker {
public:
    BrokerId id() const override { return BrokerId::Zerodha; }
    const char* name() const override { return "Zerodha"; }
    const char* base_url() const override { return "https://api.kite.trade"; }
    const char* ws_adapter_name() const override { return "zerodha"; }

    TokenExchangeResponse exchange_token(const std::string& api_key,
                                          const std::string& api_secret,
                                          const std::string& request_token) override;

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
                                                      const std::vector<std::string>& instruments) override;
    ApiResponse<std::vector<BrokerCandle>> get_history(const BrokerCredentials& creds,
                                                        const std::string& instrument_token,
                                                        const std::string& interval,
                                                        const std::string& from_date,
                                                        const std::string& to_date) override;

    // --- Zerodha-specific extensions (match Tauri feature parity) ---

    // Validate access token by fetching user profile
    ApiResponse<bool> validate_token(const BrokerCredentials& creds);

    // Download master contract (instruments CSV)
    ApiResponse<json> download_master_contract(const BrokerCredentials& creds);

    // Calculate margin for orders
    ApiResponse<json> calculate_margin(const BrokerCredentials& creds, const json& orders);

    // Close all open positions
    ApiResponse<json> close_all_positions(const BrokerCredentials& creds);

    // Cancel all open orders
    ApiResponse<json> cancel_all_orders(const BrokerCredentials& creds);

protected:
    std::map<std::string, std::string> auth_headers(const BrokerCredentials& creds) const override;

private:
    static constexpr const char* kite_api_version = "3";

    // Zerodha uses form-urlencoded for orders, not JSON
    static std::string url_encode_params(const std::map<std::string, std::string>& params);

    // Map unified order types to Zerodha strings
    static const char* zerodha_order_type(OrderType t);
    static const char* zerodha_product(ProductType p);
    static const char* zerodha_side(OrderSide s);
    static const char* zerodha_variety(ProductType p);
};

} // namespace fincept::trading
