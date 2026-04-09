#pragma once
#include "trading/BrokerInterface.h"
#include "trading/brokers/BrokerHttp.h"

namespace fincept::trading {

// Credential packing:
//   ApiKey    = account ID (e.g. "U1234567")
//   ApiSecret = gateway URL (e.g. "https://localhost:5000")
//   access_token stored as gateway URL (no OAuth token — gateway uses local session)
//
// IBKR Client Portal Gateway must be running locally.
// Auth is browser-based SSO managed by the gateway process itself.

class IBKRBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::IBKR; }
    const char* name() const override { return "IBKR"; }
    const char* base_url() const override { return "https://localhost:5000/v1/api"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "ibkr",
            .display_name = "IBKR",
            .region = "US",
            .currency = "USD",
            .credential_fields =
                {
                    {CredentialField::ApiKey, "ACCOUNT ID", "Enter Account ID (e.g. U1234567)...", false},
                    {CredentialField::ApiSecret, "GATEWAY URL", "https://localhost:5000", false},
                },
            .exchanges = {"NYSE", "NASDAQ", "AMEX", "ARCA", "BATS", "CBOE", "LSE", "TSX"},
            .product_types =
                {
                    {"Day Order", ProductType::Intraday},
                    {"GTC Order", ProductType::Delivery},
                    {"Margin", ProductType::Margin},
                },
            .supports_intraday = true,
            .supports_bracket_order = true,
            .supports_cover_order = false,
            .has_native_paper = true,
            .default_paper_balance = 100000.0,
            .default_watchlist = {"AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA", "JPM", "V", "JNJ"},
            .default_symbol = "AAPL",
            .default_exchange = "NASDAQ",
            .brokerage_info = "$0.005/share (min $1)",
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
    // Extract gateway base URL from access_token (stored as gateway URL)
    static QString gateway_url(const BrokerCredentials& creds);

    static QString ibkr_order_type(OrderType t);
    static QString ibkr_tif(ProductType p);
    // Map resolution string to IBKR bar size and period
    struct HistoryParams {
        QString bar;
        QString period;
    };
    static HistoryParams ibkr_history_params(const QString& resolution, const QString& from_date,
                                             const QString& to_date);
};

} // namespace fincept::trading
