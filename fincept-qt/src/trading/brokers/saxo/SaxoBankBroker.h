#pragma once
#include "trading/BrokerInterface.h"
#include "trading/adapter/BrokerEnumMap.h"
#include "trading/brokers/BrokerHttp.h"

namespace fincept::trading {

// Credential packing:
//   ApiKey    = App Key (client_id)
//   ApiSecret = App Secret (client_secret)
//   AuthCode  = OAuth authorization code (from redirect)
//
// exchange_token: POST to Saxo token endpoint with auth code
// access_token  = Bearer token (expires ~20min)
// refresh_token stored in additional field
// user_id       = AccountKey (from /port/v1/clients/me)
//
// Sim/paper base: https://gateway.saxobank.com/sim/openapi
// Live base:      https://gateway.saxobank.com/openapi
//
// Symbol convention: "NYSE:AAPL:211" (exchange:symbol:Uic)
// Uic (integer) required for quotes, history, and orders

class SaxoBankBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::SaxoBank; }
    const char* name() const override { return "SaxoBank"; }
    const char* base_url() const override { return "https://gateway.saxobank.com/openapi"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "saxobank",
            .display_name = "Saxo Bank",
            .region = "EU",
            .currency = "EUR",
            .credential_fields =
                {
                    {CredentialField::ApiKey, "APP KEY", "Enter App Key (client_id)...", false},
                    {CredentialField::ApiSecret, "APP SECRET", "Enter App Secret (client_secret)...", true},
                    {CredentialField::AuthCode, "AUTH CODE", "Paste OAuth authorization code...", false},
                },
            .exchanges = {"NYSE", "NASDAQ", "LSE", "XETRA", "EURONEXT", "XNYS", "XNAS"},
            .product_types =
                {
                    {"Day Order", ProductType::Intraday},
                    {"GTC Order", ProductType::Delivery},
                    {"Margin", ProductType::Margin},
                },
            .supports_intraday = true,
            .supports_bracket_order = false,
            .supports_cover_order = false,
            .has_native_paper = true,
            .default_paper_balance = 50000.0,
            .default_watchlist = {"AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "VOD", "ADS", "ASML", "SIE", "AIR"},
            .default_symbol = "AAPL",
            .default_exchange = "NYSE",
            .brokerage_info = "0.05% (min \u20AC3)",
        };
    }

    TokenExchangeResponse exchange_token(const QString& api_key, const QString& api_secret,
                                         const QString& auth_code) override;
    OrderPlaceResponse place_order(const BrokerCredentials& creds, const UnifiedOrder& order) override;
    ApiResponse<QJsonObject> modify_order(const BrokerCredentials& creds, const QString& order_id,
                                          const QJsonObject& mods) override;
    ApiResponse<QJsonObject> cancel_order(const BrokerCredentials& creds, const QString& order_id) override;
    ApiResponse<QVector<BrokerOrderInfo>> get_orders(const BrokerCredentials& creds) override;
    ApiResponse<QJsonObject> get_trade_book(const BrokerCredentials& creds) override;
    ApiResponse<QVector<BrokerPosition>> get_positions(const BrokerCredentials& creds) override;
    ApiResponse<QVector<BrokerHolding>> get_holdings(const BrokerCredentials& creds) override;
    ApiResponse<BrokerFunds> get_funds(const BrokerCredentials& creds) override;
    ApiResponse<QVector<BrokerQuote>> get_quotes(const BrokerCredentials& creds,
                                                 const QVector<QString>& symbols) override;
    ApiResponse<QVector<BrokerCandle>> get_history(const BrokerCredentials& creds, const QString& symbol,
                                                   const QString& resolution, const QString& from_date,
                                                   const QString& to_date) override;

    static bool is_token_expired(const BrokerHttpResponse& resp);
    static QString checked_error(const BrokerHttpResponse& resp, const QString& fallback);

  protected:
    QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const override;

  private:
    /// Saxo "Duration": Delivery → GoodTillCancel; everything else → DayOrder (fallback).
    static const BrokerEnumMap<QString>& saxo_enum_map();
    static int saxo_horizon(const QString& resolution);
    // Extract Uic from symbol "NYSE:AAPL:211" → "211"
    static QString extract_uic(const QString& symbol);
};

} // namespace fincept::trading
