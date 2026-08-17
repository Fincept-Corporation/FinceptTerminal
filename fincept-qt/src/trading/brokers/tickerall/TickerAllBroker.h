#pragma once
#include "trading/BrokerInterface.h"
#include "trading/brokers/BrokerHttp.h"

#include <QHash>
#include <QJsonObject>
#include <QMutex>

namespace fincept::trading {

// MetaTrader 5 integration via the TickerAll hosted REST bridge (tickerall.com).
//
// Why a hosted bridge: the official MetaTrader5 package is Windows-only, drives a
// local terminal over single-threaded IPC, and pins one running terminal per
// account. TickerAll holds the broker connection server-side and exposes it over
// REST, so an MT5 account is reachable from Linux/macOS/containers with no local
// terminal. Same shape as MetaApiBroker (MT4 via metaapi.cloud), one platform over.
//
// Credential packing — deliberately identical in shape to MetaApiBroker so
// AccountManager, the credential dialog and the session-validation sweep treat
// both bridges alike:
//   api_key         = TickerAll API key ("cf_live_…" / "cf_test_…")
//   access_token    = TickerAll accountId returned by POST /v1/sessions
//   user_id         = MT5 login number
//   additional_data = JSON {"server","platform":"mt5","account_type":"demo|live"}
//
// Quotes: TickerAll exposes no REST tick endpoint — live bid/ask arrive on its
// WebSocket. get_quotes() is a synchronous IBroker call, so it reads what REST
// does offer: the account snapshot (open positions carry currentPrice) and, for
// symbols not held, a single M1 candle whose `bid` + `spread` fields yield a real
// bid/ask pair rather than a synthetic mid. See get_quotes() for the detail.
class TickerAllBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::MetaTrader5; }
    const char* name() const override { return "MetaTrader 5"; }
    const char* base_url() const override { return "https://api.tickerall.com"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "metatrader5",
            .display_name = "MetaTrader 5",
            .region = "Global",
            .currency = "USD",
            // Rendered by AccountManagementDialog's custom multi-sub-field form
            // (custom_cred_fields("metatrader5")) because the MT5 server name has
            // no CredentialField of its own. Kept here so the profile still
            // describes what this broker needs.
            .credential_fields =
                {
                    {CredentialField::ApiKey, "TICKERALL API KEY", "cf_live_… from tickerall.com", true},
                    {CredentialField::UserId, "MT5 LOGIN", "Account number e.g. 12345678", false},
                    {CredentialField::Password, "MT5 PASSWORD", "Trading password", true},
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
            .brokerage_info = "Spread-based (varies by MT5 broker)",
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

    // Streaming deferred, same as IciciDirectBroker. TickerAll does expose a
    // WebSocket (wss://api.tickerall.com/v1/stream: ticks, positions, orders),
    // and wiring an adapter here is the natural follow-up — it would also let
    // get_quotes() serve real streamed bid/ask instead of the per-symbol M1
    // candle fallback it uses today.
    const char* ws_adapter_name() const override { return ""; }

    // Tradeable symbol list for this account, cached for an hour. Mirrors
    // MetaApiBroker::fetch_symbols so the order-form symbol picker can share a path.
    QStringList fetch_symbols(const BrokerCredentials& creds);

  protected:
    QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const override;

  private:
    // GET /v1/accounts/{id} answers funds AND positions AND live prices in one
    // payload, and get_funds/get_positions/get_holdings/get_quotes each want a
    // slice of it. A short TTL collapses that burst (the portfolio view calls all
    // four back-to-back) into one request instead of four.
    struct Snapshot {
        QJsonObject payload;
        int64_t fetched_at = 0;
    };
    ApiResponse<QJsonObject> account_snapshot(const BrokerCredentials& creds, int max_age_s);

    static QString order_type_for(OrderType type);
    static QString map_timeframe(const QString& resolution);
    static QString checked_error(const BrokerHttpResponse& resp, const QString& fallback);

    QHash<QString, Snapshot> snapshot_cache_;
    mutable QMutex snapshot_mutex_;

    QHash<QString, QStringList> symbol_cache_;
    QHash<QString, int64_t> symbol_cache_time_;
    mutable QMutex symbol_mutex_;
};

} // namespace fincept::trading
