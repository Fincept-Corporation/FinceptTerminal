#pragma once
#include "trading/BrokerInterface.h"
#include "trading/brokers/BrokerHttp.h"

namespace fincept::trading {

// Credential packing:
//   ApiKey    = uid (User ID)
//   ApiSecret = api_secret (used only to build SHA256 appkey, never sent raw)
//   AuthCode  = "vendor_code:::totp"  (vendor code + 6-digit TOTP)
//
// All requests: application/x-www-form-urlencoded
// Body: jData=<JSON>&jKey=<susertoken>  (login: jData=<JSON> only)

class ShoonyaBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::Shoonya; }
    const char* name() const override { return "Shoonya"; }
    const char* base_url() const override { return "https://api.shoonya.com/NorenWClientTP"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "shoonya",
            .display_name = "Shoonya",
            .region = "IN",
            .currency = "INR",
            .credential_fields =
                {
                    {CredentialField::ApiKey, "USER ID", "Enter User ID...", false},
                    {CredentialField::ApiSecret, "API SECRET", "Enter API Secret...", true},
                    {CredentialField::AuthCode, "VENDOR:::TOTP", "vendor_code:::6-digit TOTP", false},
                },
            .exchanges = {"NSE", "BSE", "NFO", "BFO", "MCX", "CDS", "NCDEX"},
            .product_types =
                {
                    {"Intraday (I)", ProductType::Intraday},
                    {"Delivery (C)", ProductType::Delivery},
                    {"Margin (M)", ProductType::Margin},
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
            .brokerage_info = "Zero brokerage",
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
    // Shoonya uses no auth header — token is passed as jKey form field
    QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const override;

  private:
    static QString sh_product(ProductType p);
    static QString sh_order_type(OrderType t);
    static QString sh_status(const QString& s);

    // Build form body: jData=<json>&jKey=<token>
    static QByteArray make_body(const QJsonObject& jdata, const QString& token);
    // Build form body for login (no jKey)
    static QByteArray make_login_body(const QJsonObject& jdata);
};

} // namespace fincept::trading
