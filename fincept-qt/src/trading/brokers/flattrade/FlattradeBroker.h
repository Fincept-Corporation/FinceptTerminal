#pragma once
#include "trading/BrokerInterface.h"
#include "trading/adapter/BrokerEnumMap.h"
#include "trading/brokers/BrokerHttp.h"

namespace fincept::trading {

// Flattrade is a Noren/Omnesys (PiConnect) platform broker — same wire protocol
// as Shoonya. Trading endpoints live at https://piconnect.flattrade.in/PiConnectAPI
// and use the jData=<JSON>&jKey=<token> form-urlencoded protocol.
//
// Credential packing:
//   ApiKey    = "clientid:::apikey"  (client id used as uid/actid; apikey used in auth hash)
//   ApiSecret = api_secret           (used to build SHA256 security hash, never sent raw)
//   AuthCode  = request_code         (one-time request code from the Flattrade auth redirect)
//
// Auth flow differs from Shoonya: a separate auth host issues the session token.
//   POST https://authapi.flattrade.in/trade/apitoken
//   body (JSON): {api_key, request_code, api_secret=SHA256(apikey+request_code+secret)}
//   → returns {"stat":"Ok","token":"<susertoken>"}

class FlattradeBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::Flattrade; }
    const char* name() const override { return "Flattrade"; }
    const char* base_url() const override { return "https://piconnect.flattrade.in/PiConnectAPI"; }
    const char* ws_adapter_name() const override { return "flattrade"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "flattrade",
            .display_name = "Flattrade",
            .region = "IN",
            .currency = "INR",
            .credential_fields =
                {
                    {CredentialField::ApiKey, "CLIENT:::APIKEY", "client_id:::api_key", false},
                    {CredentialField::ApiSecret, "API SECRET", "Enter API Secret...", true},
                    {CredentialField::AuthCode, "REQUEST CODE", "Enter request code...", false},
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
            .brokerage_info = "₹10/order or 0.03%",
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
    // Flattrade uses no auth header — token is passed as jKey form field
    QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const override;

  private:
    static const BrokerEnumMap<QString>& ft_enum_map();
    static QString ft_status(const QString& s);

    // The Noren uid/actid is the client-id portion of the packed ApiKey (before ":::").
    static QString uid_from_creds(const BrokerCredentials& creds);

    // Build form body: jData=<json>&jKey=<token>
    static QByteArray make_body(const QJsonObject& jdata, const QString& token);
};

} // namespace fincept::trading
