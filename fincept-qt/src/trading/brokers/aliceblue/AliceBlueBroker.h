#pragma once
#include "trading/BrokerInterface.h"
#include "trading/adapter/BrokerEnumMap.h"
#include "trading/brokers/BrokerHttp.h"

namespace fincept::trading {

class AliceBlueBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::AliceBlue; }
    const char* name() const override { return "AliceBlue"; }
    const char* base_url() const override { return "https://a3.aliceblueonline.com"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "aliceblue",
            .display_name = "AliceBlue",
            .region = "IN",
            .currency = "INR",
            .credential_fields =
                {
                    {CredentialField::ApiKey, "USER ID", "Enter User ID...", false},
                    {CredentialField::ApiSecret, "API SECRET", "Enter API Secret...", true},
                    {CredentialField::AuthCode, "AUTH CODE", "Enter Auth/Request Token...", false},
                },
            // AliceBlue ANT supports NSE/BSE equity + NFO/BFO/CDS derivatives +
            // MCX commodities + INDICES. NCDEX is not on the supported list.
            .exchanges = {"NSE", "BSE", "NFO", "BFO", "CDS", "MCX", "INDICES"},
            .product_types =
                {
                    {"Intraday (INTRADAY)", ProductType::Intraday},
                    {"Delivery (LONGTERM)", ProductType::Delivery},
                    {"Margin (MTF)", ProductType::Margin},
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
            .brokerage_info = "\u20B915/order flat",
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
    static const BrokerEnumMap<QString>& ab_enum_map();
    static QString ab_resolution(const QString& resolution);
};

} // namespace fincept::trading
