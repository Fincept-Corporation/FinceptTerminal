#pragma once
#include "trading/BrokerInterface.h"
#include "trading/adapter/BrokerEnumMap.h"
#include "trading/brokers/BrokerHttp.h"

namespace fincept::trading {

// Tradejini is a custom REST broker (NOT Noren) on api.tradejini.com/v2.
// All authenticated calls carry  Authorization: Bearer <api_key>:<access_token>
// where api_key is the app's API secret (BROKER_API_SECRET in OpenAlgo) and
// access_token is the per-session token returned by the individual-token login.
//
// Credential packing:
//   ApiKey    = api_key (app API secret used as the bearer prefix / client id)
//   ApiSecret = password (user account password)
//   AuthCode  = totp     (6-digit time-based OTP)
//
// Auth flow:
//   POST /api-gw/oauth/individual-token-v2  (Bearer <api_key>, form body)
//   body: password=<pwd>&twoFa=<totp>&twoFaTyp=totp
//   → {access_token, token_type:"Bearer", expires_in}
//
// Response envelope for data endpoints: {"s":"ok","d":<payload>}.

class TradejiniBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::Tradejini; }
    const char* name() const override { return "Tradejini"; }
    const char* base_url() const override { return "https://api.tradejini.com/v2"; }
    const char* ws_adapter_name() const override { return "tradejini"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "tradejini",
            .display_name = "Tradejini",
            .region = "IN",
            .currency = "INR",
            .credential_fields =
                {
                    {CredentialField::ApiKey, "API KEY", "Enter API Key...", false},
                    {CredentialField::ApiSecret, "PASSWORD", "Account password", true},
                    {CredentialField::AuthCode, "TOTP", "6-digit TOTP", false},
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
            .brokerage_info = "Flat ₹20/order",
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
    static const BrokerEnumMap<QString>& tj_enum_map();
    static QString tj_status(const QString& s);
    // Bearer value: "<api_key>:<access_token>"
    static QString bearer(const BrokerCredentials& creds);
};

} // namespace fincept::trading
