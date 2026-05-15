#pragma once
#include "trading/BrokerInterface.h"
#include "trading/adapter/BrokerEnumMap.h"
#include "trading/brokers/BrokerHttp.h"

namespace fincept::trading {

// Credential packing:
//   ApiKey      = access token (from developer.tradier.com)
//   Environment = "sandbox" for paper/sandbox, anything else = live
//
// Base URLs:
//   Live:    https://api.tradier.com
//   Sandbox: https://sandbox.tradier.com
//
// exchange_token: validates token via GET /v1/user/profile, fetches account_id
// access_token stored as-is, account_id stored as user_id

class TradierBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::Tradier; }
    const char* name() const override { return "Tradier"; }
    const char* base_url() const override { return "https://api.tradier.com/v1"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "tradier",
            .display_name = "Tradier",
            .region = "US",
            .currency = "USD",
            .credential_fields =
                {
                    {CredentialField::ApiKey, "ACCESS TOKEN", "Paste access token from developer.tradier.com", false},
                    {CredentialField::Environment, "ENVIRONMENT", "live or sandbox", false},
                },
            .exchanges = {"NYSE", "NASDAQ", "AMEX", "ARCA", "BATS"},
            .product_types =
                {
                    {"Day Order", ProductType::Intraday},
                    {"GTC Order", ProductType::Delivery},
                },
            .supports_intraday = true,
            .supports_bracket_order = false,
            .supports_cover_order = false,
            .has_native_paper = true,
            .default_paper_balance = 100000.0,
            .default_watchlist = {"AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA", "JPM", "V", "JNJ"},
            .default_symbol = "AAPL",
            .default_exchange = "NASDAQ",
            .brokerage_info = "$0 commission (US equities)",
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
    // Returns base URL based on environment credential
    static QString base(const BrokerCredentials& creds);
    /// Tradier "duration": Delivery → gtc; everything else falls back to "day" at callsite.
    static const BrokerEnumMap<QString>& tr_enum_map();
    // Tradier returns single object or array — always normalize to array
    static QJsonArray normalize_array(const QJsonValue& val);
};

} // namespace fincept::trading
