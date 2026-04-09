#pragma once
#include "trading/BrokerInterface.h"
#include "trading/brokers/BrokerHttp.h"

namespace fincept::trading {

// Credential packing:
//   ApiKey    = "app_key:::user_id:::client_id"
//   ApiSecret = App Secret (EncryKey)
//   AuthCode  = "email:::pin:::totp"  (passed at exchange_token time)

class FivePaisaBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::FivePaisa; }
    const char* name() const override { return "5Paisa"; }
    const char* base_url() const override { return "https://Openapi.5paisa.com"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "fivepaisa",
            .display_name = "5Paisa",
            .region = "IN",
            .currency = "INR",
            .credential_fields =
                {
                    {CredentialField::ApiKey, "APP KEY (app:::user:::client)", "app_key:::user_id:::client_id", false},
                    {CredentialField::ApiSecret, "APP SECRET", "Enter App Secret...", true},
                    {CredentialField::AuthCode, "LOGIN (email:::pin:::totp)", "email:::pin:::totp", false},
                },
            .exchanges = {"NSE", "BSE", "NFO", "BFO", "MCX", "CDS"},
            .product_types =
                {
                    {"Intraday (MIS)", ProductType::Intraday},
                    {"Delivery (CNC)", ProductType::Delivery},
                    {"Margin (NRML)", ProductType::Margin},
                },
            .supports_intraday = true,
            .supports_bracket_order = false,
            .supports_cover_order = false,
            .has_native_paper = false,
            .default_paper_balance = 1000000.0,
            .default_watchlist = {"HDFCBANK", "ICICIBANK", "SBIN", "TCS", "INFY", "RELIANCE", "TATAMOTORS",
                                  "BAJFINANCE", "HINDUNILVR", "ITC"},
            .default_symbol = "RELIANCE",
            .default_exchange = "NSE",
            .brokerage_info = "\u20B910/order flat",
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
    // Unpack "app_key:::user_id:::client_id" → {app_key, user_id, client_id}
    struct KeyParts {
        QString app_key, user_id, client_id;
    };
    static KeyParts unpack_key(const QString& packed);

    static QString fp_exchange(const QString& exchange);
    static QString fp_exchange_type(const QString& exchange);
    static QString fp_interval(const QString& resolution);

    // Build the standard 5paisa request envelope
    static QJsonObject make_body(const QString& app_key, const QString& client_id, const QJsonObject& body_fields);
};

} // namespace fincept::trading
