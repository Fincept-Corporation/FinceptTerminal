#pragma once
#include "trading/BrokerInterface.h"

namespace fincept::trading {

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
                    {CredentialField::ApiKey, "APP KEY", "Enter App Key...", false},
                    {CredentialField::ApiSecret, "APP SECRET", "Enter App Secret...", true},
                    {CredentialField::AuthCode, "AUTH CODE", "Paste OAuth code...", false},
                },
            .exchanges = {"NYSE", "NASDAQ", "LSE", "XETRA", "EURONEXT"},
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
            .default_watchlist = {"AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "VOD.L", "ADS.XETRA", "ASML.EURONEXT",
                                  "SIE.XETRA", "AIR.EURONEXT"},
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

  protected:
    QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const override;
};

} // namespace fincept::trading
