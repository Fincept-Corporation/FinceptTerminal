#pragma once
#include "trading/BrokerInterface.h"

namespace fincept::trading {

class AlpacaBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::Alpaca; }
    const char* name() const override { return "Alpaca"; }
    const char* base_url() const override { return "https://paper-api.alpaca.markets"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "alpaca",
            .display_name = "Alpaca",
            .region = "US",
            .currency = "USD",
            .credential_fields =
                {
                    {CredentialField::ApiKey, "API KEY ID", "Enter API Key ID...", false},
                    {CredentialField::ApiSecret, "API SECRET KEY", "Enter Secret Key...", true},
                    {CredentialField::Environment, "ENVIRONMENT", "", false},
                },
            .exchanges = {"NYSE", "NASDAQ", "AMEX"},
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
            .brokerage_info = "$0 commission (US equities & crypto)",
        };
    }

    static QString trading_url(const BrokerCredentials& creds);
    static QString data_url() { return "https://data.alpaca.markets"; }

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
    ApiResponse<QVector<MarketCalendarDay>> get_calendar(const BrokerCredentials& creds, const QString& start,
                                                         const QString& end) override;
    ApiResponse<MarketClock> get_clock(const BrokerCredentials& creds) override;

    ApiResponse<QVector<BrokerCandle>> get_latest_bars(const BrokerCredentials& creds,
                                                       const QVector<QString>& symbols) override;
    ApiResponse<QVector<BrokerCandle>> get_historical_bars(const BrokerCredentials& creds,
                                                           const QVector<QString>& symbols, const QString& timeframe,
                                                           const QString& start, const QString& end) override;
    ApiResponse<QVector<BrokerQuote>> get_latest_quotes(const BrokerCredentials& creds,
                                                        const QVector<QString>& symbols) override;
    ApiResponse<QVector<BrokerTrade>> get_latest_trades(const BrokerCredentials& creds,
                                                        const QVector<QString>& symbols) override;
    ApiResponse<QVector<BrokerTrade>> get_historical_trades(const BrokerCredentials& creds,
                                                            const QVector<QString>& symbols, const QString& start,
                                                            const QString& end, int limit = 1000) override;
    ApiResponse<QVector<BrokerAuction>> get_historical_auctions(const BrokerCredentials& creds,
                                                                const QVector<QString>& symbols, const QString& start,
                                                                const QString& end) override;
    ApiResponse<QVector<BrokerMetaEntry>> get_condition_codes(const BrokerCredentials& creds, const QString& ticktype,
                                                              const QString& tape) override;
    ApiResponse<QVector<BrokerMetaEntry>> get_exchange_codes(const BrokerCredentials& creds,
                                                             const QString& asset_class) override;

    ApiResponse<BrokerCandle> get_latest_bar(const BrokerCredentials& creds, const QString& symbol) override;
    ApiResponse<BrokerQuote> get_latest_quote(const BrokerCredentials& creds, const QString& symbol) override;
    ApiResponse<BrokerTrade> get_latest_trade(const BrokerCredentials& creds, const QString& symbol) override;
    ApiResponse<BrokerQuote> get_snapshot(const BrokerCredentials& creds, const QString& symbol) override;
    ApiResponse<QVector<BrokerTrade>> get_historical_trades_single(const BrokerCredentials& creds,
                                                                   const QString& symbol, const QString& start,
                                                                   const QString& end, int limit = 1000) override;
    ApiResponse<QVector<BrokerQuote>> get_historical_quotes_single(const BrokerCredentials& creds,
                                                                   const QString& symbol, const QString& start,
                                                                   const QString& end, int limit = 1000) override;
    ApiResponse<QVector<BrokerAuction>> get_historical_auctions_single(const BrokerCredentials& creds,
                                                                       const QString& symbol, const QString& start,
                                                                       const QString& end) override;

  protected:
    QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const override;
};

} // namespace fincept::trading
