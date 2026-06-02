#pragma once
#include "trading/BrokerInterface.h"
#include "trading/adapter/BrokerEnumMap.h"
#include "trading/brokers/BrokerHttp.h"

namespace fincept::trading {

class DhanBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::Dhan; }
    const char* name() const override { return "Dhan"; }
    const char* base_url() const override { return "https://api.dhan.co"; }
    const char* ws_adapter_name() const override { return "dhan"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "dhan",
            .display_name = "Dhan",
            .region = "IN",
            .currency = "INR",
            .credential_fields =
                {
                    {CredentialField::ApiKey, "CLIENT ID", "Enter Dhan Client ID...", false},
                    {CredentialField::ApiSecret, "ACCESS TOKEN", "Paste access token...", true},
                },
            .exchanges = {"NSE", "BSE", "NFO", "BFO", "CDS", "MCX"},
            .product_types =
                {
                    {"Intraday (INTRADAY)", ProductType::Intraday},
                    {"Delivery (CNC)", ProductType::Delivery},
                    {"Margin (MARGIN)", ProductType::Margin},
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
            .brokerage_info = "\u20B920/order flat",
        };
    }

    TokenExchangeResponse exchange_token(const QString& api_key, const QString& api_secret,
                                         const QString& auth_code) override;

    // Authoritative token check via GET /v2/profile (DhanHQ-recommended). Logs
    // the raw response, parses the real tokenValidity, and never disconnects on
    // transient/rate-limit errors.
    SessionCheck validate_session(const BrokerCredentials& creds) override;
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

    // --- Multi-quote & Market Depth ---
    ApiResponse<QVector<BrokerQuote>> get_multi_quotes(
        const BrokerCredentials& creds,
        const QVector<QPair<QString, QString>>& symbols) override;

    ApiResponse<MarketDepth> get_market_depth(
        const BrokerCredentials& creds,
        const QString& symbol, const QString& exchange) override;

    // --- Margin Calculator --- POST /v2/margincalculator (native, single order)
    ApiResponse<OrderMargin> get_order_margins(const BrokerCredentials& creds, const UnifiedOrder& order) override;

    static bool is_token_expired(const BrokerHttpResponse& resp);
    static QString checked_error(const BrokerHttpResponse& resp, const QString& fallback);

  protected:
    QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const override;

  private:
    static QString dhan_exchange(const QString& exchange);
    static const BrokerEnumMap<QString>& dhan_enum_map();
    static QString lookup_security_id(const QString& symbol, const QString& exchange, const QString& broker_id);
};

} // namespace fincept::trading
