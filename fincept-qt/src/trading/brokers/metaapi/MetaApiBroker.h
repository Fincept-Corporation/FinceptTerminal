#pragma once
#include "trading/BrokerInterface.h"
#include "trading/adapter/BrokerEnumMap.h"
#include "trading/brokers/BrokerHttp.h"

#include <QHash>
#include <QJsonObject>
#include <QMutex>

namespace fincept::trading {

// MetaTrader 4 integration via MetaAPI (metaapi.cloud) REST bridge.
//
// Credential packing:
//   api_key        = MetaAPI auth token
//   access_token   = MetaAPI provisioned account ID
//   user_id        = MT4 login number
//   additional_data = JSON {"region","server","platform":"mt4","account_type":"demo|live"}

class MetaApiBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::MetaTrader4; }
    const char* name() const override { return "MetaTrader 4"; }
    const char* base_url() const override { return "https://mt-client-api-v1.new-york-server.agiliumtrade.ai"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "metatrader4",
            .display_name = "MetaTrader 4",
            .region = "Global",
            .currency = "USD",
            .credential_fields =
                {
                    {CredentialField::ApiKey, "METAAPI TOKEN", "Enter MetaAPI auth token...", true},
                    {CredentialField::UserId, "MT4 LOGIN", "Account number e.g. 12345678", false},
                    {CredentialField::Password, "MT4 PASSWORD", "Trading password", true},
                },
            .exchanges = {"FOREX"},
            .product_types =
                {
                    {"Margin Trade", ProductType::Margin},
                },
            .supports_intraday = true,
            .supports_bracket_order = false,
            .supports_cover_order = false,
            .has_native_paper = true,
            .default_paper_balance = 10000.0,
            .default_watchlist = {"EURUSD", "GBPUSD", "USDJPY", "XAUUSD", "US30", "NAS100", "SPX500"},
            .default_symbol = "EURUSD",
            .default_exchange = "FOREX",
            .brokerage_info = "Spread-based (varies by MT4 broker)",
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

    static QString provisioning_url() {
        return QStringLiteral("https://mt-provisioning-api-v1.agiliumtrade.agiliumtrade.ai");
    }
    static QString trading_url(const QString& region);
    static QString market_data_url(const QString& region);
    static QString region_from_creds(const BrokerCredentials& creds);

    QStringList fetch_symbols(const BrokerCredentials& creds);

  protected:
    QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const override;

  private:
    static QString action_type_for(OrderSide side, OrderType type);
    static QString map_timeframe(const QString& resolution);
    static QString checked_error(const BrokerHttpResponse& resp, const QString& fallback);
    static bool is_deployment_error(const BrokerHttpResponse& resp);
    bool try_redeploy(const BrokerCredentials& creds);

    QHash<QString, QStringList> symbol_cache_;
    QHash<QString, int64_t> symbol_cache_time_;
    mutable QMutex symbol_mutex_;
};

} // namespace fincept::trading
