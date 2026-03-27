#pragma once
#include "trading/BrokerInterface.h"
#include "trading/brokers/BrokerHttp.h"

namespace fincept::trading {

// Credential packing:
//   ApiKey    = "trade_app_key:::trade_secret"
//   ApiSecret = "market_app_key:::market_secret"
//   access_token stored as "trade_token:::feed_token"

class IIFLBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::IIFL; }
    const char* name() const override { return "IIFL"; }
    const char* base_url() const override { return "https://ttblaze.iifl.com/interactive"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "iifl", .display_name = "IIFL", .region = "IN", .currency = "INR",
            .credential_fields = {
                {CredentialField::ApiKey,    "TRADE KEY (app:::secret)",  "trade_app_key:::trade_secret",   false},
                {CredentialField::ApiSecret, "MARKET KEY (app:::secret)", "market_app_key:::market_secret", true},
            },
            .exchanges = {"NSE", "BSE", "NFO", "BFO", "MCX", "CDS"},
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
    // Unpack "trade_token:::feed_token" → {trade, feed}
    struct TokenParts { QString trade, feed; };
    static TokenParts unpack_token(const QString& packed);

    // Unpack "app_key:::secret" → {app_key, secret}
    struct KeyParts { QString app_key, secret; };
    static KeyParts unpack_key(const QString& packed);

    // Exchange segment string for order API (e.g. NSE → "NSECM")
    static QString iifl_exchange(const QString& exchange);
    // Exchange segment numeric ID for market data API (e.g. NSE → 1)
    static int     iifl_exchange_id(const QString& exchange);
    // Order type mapping
    static QString iifl_order_type(OrderType t);
    // Product type pass-through
    static QString iifl_product(ProductType p);
    // compressionValue for history (resolution string → seconds or "D")
    static QString iifl_compression(const QString& resolution);
    // Format QDateTime to IIFL time string "Jan 01 2024 091500"
    static QString iifl_time(const QString& date_str, bool end_of_day);
};

} // namespace fincept::trading
