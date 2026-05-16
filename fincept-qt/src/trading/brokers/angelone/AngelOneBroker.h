#pragma once
#include "trading/BrokerInterface.h"
#include "trading/adapter/BrokerEnumMap.h"
#include "trading/brokers/BrokerHttp.h"

namespace fincept::trading {

class AngelOneBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::AngelOne; }
    const char* name() const override { return "AngelOne"; }
    const char* base_url() const override { return "https://apiconnect.angelone.in"; }
    const char* ws_adapter_name() const override { return "angelone"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "angelone",
            .display_name = "Angel One",
            .region = "IN",
            .currency = "INR",
            .credential_fields =
                {
                    {CredentialField::ClientCode, "CLIENT CODE", "Enter login/client ID...", false},
                    {CredentialField::ApiKey, "API KEY", "Enter API key...", false},
                    {CredentialField::ApiSecret, "MPIN", "Enter MPIN/PIN...", true},
                    {CredentialField::AuthCode, "TOTP SECRET", "Enter base32 TOTP secret...", false},
                },
            .exchanges = {"NSE", "BSE", "NFO", "MCX", "NCDEX"},
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
            .default_watchlist = {"HDFCBANK", "ICICIBANK", "SBIN", "TCS", "INFY", "RELIANCE", "TATASTEEL", "BHARTIARTL",
                                  "ITC", "LT"},
            .default_symbol = "RELIANCE",
            .default_exchange = "NSE",
            .brokerage_info = "\u20B920/order flat",
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
    ApiResponse<OrderMargin> get_order_margins(const BrokerCredentials& creds, const UnifiedOrder& order) override;
    ApiResponse<BasketMargin> get_basket_margins(const BrokerCredentials& creds,
                                                 const QVector<UnifiedOrder>& orders) override;

    static bool is_token_expired(const BrokerHttpResponse& resp);
    static QString checked_error(const BrokerHttpResponse& resp, const QString& fallback);

    /// Refresh the JWT pair using the stored refresh_token. On success the
    /// updated tokens are returned in the result; caller persists them.
    /// Endpoint: POST /rest/auth/angelbroking/jwt/v1/generateTokens.
    TokenExchangeResponse refresh_token(const BrokerCredentials& creds);

    /// Look up the numeric instrument token for a symbol+exchange.
    /// Returns 0 if not found. Used by AngelOneWebSocket subscription.
    static quint32 lookup_token_int(const QString& symbol, const QString& exchange);

  protected:
    QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const override;

  private:
    static QString generate_totp(const QString& base32_secret);
    static QString lookup_token(const QString& symbol, const QString& exchange);
    static const BrokerEnumMap<QString>& ao_enum_map();
    static QString ao_variety(OrderType t, bool amo);
};

} // namespace fincept::trading
