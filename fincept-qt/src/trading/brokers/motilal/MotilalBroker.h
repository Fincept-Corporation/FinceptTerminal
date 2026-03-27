#pragma once
#include "trading/BrokerInterface.h"
#include "trading/brokers/BrokerHttp.h"

namespace fincept::trading {

// Credential packing:
//   ApiKey    = api_key (passed as ApiKey: header)
//   ApiSecret = password (SHA256(password + api_key) sent at login)
//   AuthCode  = "dob:::totp"  (date-of-birth as DD/MM/YYYY, 6-digit TOTP)

class MotilalBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::Motilal; }
    const char* name() const override { return "Motilal"; }
    const char* base_url() const override { return "https://openapi.motilaloswal.com"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "motilal", .display_name = "Motilal Oswal", .region = "IN", .currency = "INR",
            .credential_fields = {
                {CredentialField::ApiKey,    "API KEY",              "Enter API Key...",            false},
                {CredentialField::ApiSecret, "PASSWORD",             "Enter Login Password...",     true},
                {CredentialField::AuthCode,  "DOB:::TOTP",           "DD/MM/YYYY:::6-digit TOTP",   false},
            },
            .exchanges = {"NSE", "BSE", "NFO", "MCX", "NCDEX"},
            .product_types = {
                {"Intraday (MIS)",  ProductType::Intraday},
                {"Delivery (CNC)", ProductType::Delivery},
                {"Margin (NRML)",  ProductType::Margin},
            },
            .supports_intraday=true, .supports_bracket_order=false, .supports_cover_order=false,
            .has_native_paper=false, .default_paper_balance=1000000.0,
            .default_watchlist={"HDFCBANK","ICICIBANK","SBIN","TCS","INFY","RELIANCE","TATAMOTORS","BAJFINANCE","HINDUNILVR","ITC"},
            .default_symbol="RELIANCE", .default_exchange="NSE",
            .brokerage_info="\u20B920/order flat",
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

    static bool    is_token_expired(const BrokerHttpResponse& resp);
    static QString checked_error(const BrokerHttpResponse& resp, const QString& fallback);

  protected:
    QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const override;

  private:
    static QString mo_exchange(const QString& exchange);
    static QString mo_product(ProductType p);
    static QString mo_order_type(OrderType t);
    static QString mo_status(const QString& s);
};

} // namespace fincept::trading
