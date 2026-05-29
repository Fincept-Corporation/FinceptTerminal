#pragma once
#include "trading/BrokerInterface.h"
#include "trading/adapter/BrokerEnumMap.h"
#include "trading/brokers/BrokerHttp.h"

namespace fincept::trading {

// Paytm Money — OAuth2 REST broker on https://developer.paytmmoney.com.
// Authenticated calls carry the JWT in the  x-jwt-token  header.
//
// Credential packing:
//   ApiKey    = api_key
//   ApiSecret = api_secret
//   AuthCode  = request_token  (from the merchant-login redirect URL)
//
// Auth flow:
//   POST /accounts/v2/gettoken {api_key, api_secret_key, request_token}
//   → {access_token, public_access_token, read_access_token}
//   access_token is persisted; public_access_token (feed token) is stored in
//   additional_data for the WebSocket adapter.
//
// Order/quote APIs key off the numeric security_id (instrument token). Symbols
// for quotes/history must therefore be passed as "EXCHANGE:TOKEN[:SEGMENT]".

class PaytmBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::Paytm; }
    const char* name() const override { return "Paytm Money"; }
    const char* base_url() const override { return "https://developer.paytmmoney.com"; }
    const char* ws_adapter_name() const override { return "paytm"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "paytm",
            .display_name = "Paytm Money",
            .region = "IN",
            .currency = "INR",
            .credential_fields =
                {
                    {CredentialField::ApiKey, "API KEY", "Enter API Key...", false},
                    {CredentialField::ApiSecret, "API SECRET", "Enter API Secret...", true},
                    {CredentialField::AuthCode, "REQUEST TOKEN", "Enter request token...", false},
                },
            .exchanges = {"NSE", "BSE", "NFO", "BFO"},
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
            .brokerage_info = "₹10/order intraday & F&O, 0% delivery",
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
    static const BrokerEnumMap<QString>& pm_enum_map();
    static QString pm_status(const QString& s);
    QJsonObject find_order(const BrokerCredentials& creds, const QString& order_id);
};

} // namespace fincept::trading
