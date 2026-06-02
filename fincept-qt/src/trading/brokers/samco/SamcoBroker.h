#pragma once
#include "trading/BrokerInterface.h"
#include "trading/adapter/BrokerEnumMap.h"
#include "trading/brokers/BrokerHttp.h"

namespace fincept::trading {

// Samco StockNote API — REST broker on https://tradeapi.samco.in.
// Authenticated calls carry the session token in the  x-session-token  header.
//
// Credential packing:
//   ApiKey    = uid (Samco user/client id)
//   ApiSecret = password (account password)
//   AuthCode  = secret_api_key (permanent secret API key generated via the
//               one-time OTP → secretKeyGenerator flow)
//
// Auth flow (the OTP/secret-key generation is a one-time out-of-band setup;
// we only run the repeatable session-establishing steps):
//   1. POST /accessToken/token  {uid, secretApiKey} → accessToken (24h)
//   2. POST /login              {userId, password, accessToken} → sessionToken
// The sessionToken is what we persist as access_token.

class SamcoBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::Samco; }
    const char* name() const override { return "Samco"; }
    const char* base_url() const override { return "https://tradeapi.samco.in"; }
    const char* ws_adapter_name() const override { return "samco"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "samco",
            .display_name = "Samco",
            .region = "IN",
            .currency = "INR",
            .credential_fields =
                {
                    {CredentialField::ApiKey, "USER ID", "Enter User ID...", false},
                    {CredentialField::ApiSecret, "PASSWORD", "Account password", true},
                    {CredentialField::AuthCode, "SECRET API KEY", "Permanent secret API key", true},
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
            .brokerage_info = "₹20/order or 0.5% (whichever lower)",
        };
    }

    TokenExchangeResponse exchange_token(const QString& api_key, const QString& api_secret,
                                         const QString& auth_code) override;

    // Permanent secretApiKey is persisted, so the daily session is re-mintable.
    bool supports_silent_refresh() const override { return true; }
    TokenExchangeResponse refresh_session(const BrokerCredentials& creds) override;

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
    static const BrokerEnumMap<QString>& sm_enum_map();
    static QString sm_status(const QString& s);
};

} // namespace fincept::trading
