#pragma once
// Generic Broker — base class with default "not implemented" for all operations
// Concrete brokers override only what they support.
// This lets us register all 16 brokers immediately.

#include "trading/broker_interface.h"
#include <chrono>

namespace fincept::trading {

class GenericBroker : public IBroker {
public:
    TokenExchangeResponse exchange_token(const std::string&, const std::string&,
                                          const std::string&) override {
        return {false, "", "", "Authentication not implemented for " + std::string(name())};
    }

    OrderPlaceResponse place_order(const BrokerCredentials&, const UnifiedOrder&) override {
        return {false, "", "Order placement not implemented for " + std::string(name())};
    }

    ApiResponse<json> modify_order(const BrokerCredentials&, const std::string&,
                                    const json&) override {
        return {false, std::nullopt, "Not implemented for " + std::string(name()), now_ts()};
    }

    ApiResponse<json> cancel_order(const BrokerCredentials&, const std::string&) override {
        return {false, std::nullopt, "Not implemented for " + std::string(name()), now_ts()};
    }

    ApiResponse<std::vector<BrokerOrderInfo>> get_orders(const BrokerCredentials&) override {
        return {false, std::nullopt, "Not implemented for " + std::string(name()), now_ts()};
    }

    ApiResponse<json> get_trade_book(const BrokerCredentials&) override {
        return {false, std::nullopt, "Not implemented for " + std::string(name()), now_ts()};
    }

    ApiResponse<std::vector<BrokerPosition>> get_positions(const BrokerCredentials&) override {
        return {false, std::nullopt, "Not implemented for " + std::string(name()), now_ts()};
    }

    ApiResponse<std::vector<BrokerHolding>> get_holdings(const BrokerCredentials&) override {
        return {false, std::nullopt, "Not implemented for " + std::string(name()), now_ts()};
    }

    ApiResponse<BrokerFunds> get_funds(const BrokerCredentials&) override {
        return {false, std::nullopt, "Not implemented for " + std::string(name()), now_ts()};
    }

    ApiResponse<std::vector<BrokerQuote>> get_quotes(const BrokerCredentials&,
                                                      const std::vector<std::string>&) override {
        return {false, std::nullopt, "Not implemented for " + std::string(name()), now_ts()};
    }

    ApiResponse<std::vector<BrokerCandle>> get_history(const BrokerCredentials&,
                                                        const std::string&, const std::string&,
                                                        const std::string&, const std::string&) override {
        return {false, std::nullopt, "Not implemented for " + std::string(name()), now_ts()};
    }

protected:
    std::map<std::string, std::string> auth_headers(const BrokerCredentials&) const override {
        return {};
    }

    static int64_t now_ts() {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::seconds>(now).count();
    }
};

// ============================================================================
// Macro to define a stub broker in one line
// ============================================================================
#define DEFINE_STUB_BROKER(ClassName, BrokerIdVal, BrokerName, BaseUrl) \
    class ClassName : public GenericBroker { \
    public: \
        BrokerId id() const override { return BrokerIdVal; } \
        const char* name() const override { return BrokerName; } \
        const char* base_url() const override { return BaseUrl; } \
    };

// --- Indian Brokers ---
DEFINE_STUB_BROKER(ZerodhaBroker,   BrokerId::Zerodha,   "Zerodha",    "https://api.kite.trade")
DEFINE_STUB_BROKER(UpstoxBroker,    BrokerId::Upstox,    "Upstox",     "https://api.upstox.com/v2")
DEFINE_STUB_BROKER(DhanBroker,      BrokerId::Dhan,      "Dhan",       "https://api.dhan.co/v2")
DEFINE_STUB_BROKER(KotakBroker,     BrokerId::Kotak,     "Kotak",      "https://tradeapi.kotaksecurities.com")
DEFINE_STUB_BROKER(GrowwBroker,     BrokerId::Groww,     "Groww",      "https://groww.in/api/v1")
DEFINE_STUB_BROKER(AliceBlueBroker, BrokerId::AliceBlue, "AliceBlue",  "https://ant.aliceblueonline.com/rest/AliceBlueAPIService/api")
DEFINE_STUB_BROKER(AngelOneBroker,  BrokerId::AngelOne,  "AngelOne",   "https://apiconnect.angelbroking.com")
DEFINE_STUB_BROKER(FivePaisaBroker, BrokerId::FivePaisa, "5Paisa",     "https://openapi.5paisa.com")
DEFINE_STUB_BROKER(IIFLBroker,      BrokerId::IIFL,      "IIFL",       "https://ttblaze.iifl.com/apimarketdata/instruments")
DEFINE_STUB_BROKER(MotilalBroker,   BrokerId::Motilal,   "Motilal",    "https://openapi.motilaloswal.com/rest")
DEFINE_STUB_BROKER(ShoonyaBroker,   BrokerId::Shoonya,   "Shoonya",    "https://api.shoonya.com/NorenWClientTP")

// --- US Brokers ---
DEFINE_STUB_BROKER(AlpacaBroker,    BrokerId::Alpaca,    "Alpaca",     "https://paper-api.alpaca.markets/v2")
DEFINE_STUB_BROKER(IBKRBroker,      BrokerId::IBKR,      "IBKR",       "https://localhost:5000/v1")
DEFINE_STUB_BROKER(TradierBroker,   BrokerId::Tradier,   "Tradier",    "https://api.tradier.com/v1")

// --- EU Brokers ---
DEFINE_STUB_BROKER(SaxoBankBroker,  BrokerId::SaxoBank,  "Saxo Bank",  "https://gateway.saxobank.com/openapi")

#undef DEFINE_STUB_BROKER

} // namespace fincept::trading
