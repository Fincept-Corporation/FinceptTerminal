#pragma once

#include "services/polymarket/PolymarketTypes.h"
#include "services/prediction/PredictionCredentialStore.h"
#include "services/prediction/PredictionExchangeAdapter.h"

#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

namespace fincept::services::polymarket {
class PolymarketService;
class PolymarketWebSocket;
} // namespace fincept::services::polymarket

namespace fincept::services::prediction::polymarket_ns {

/// Polymarket exchange adapter.
///
/// Translates the exchange-local services::polymarket::PolymarketService
/// (REST) and ::PolymarketWebSocket (WS) singletons into the unified
/// prediction::* types.
///
/// Hub topic ownership: PolymarketWebSocket is the DataHub producer for
/// `prediction:polymarket:*` — it registers itself in main.cpp via
/// ensure_registered_with_hub(). This adapter does NOT publish to the
/// hub; screens that want the typed PredictionOrderBook listen on the
/// adapter's Qt signals instead.
///
/// Authenticated endpoints are stubbed here and implemented in Phase 6
/// via the py_clob_client Python bridge.
class PolymarketAdapter : public fincept::services::prediction::PredictionExchangeAdapter {
    Q_OBJECT
  public:
    explicit PolymarketAdapter(QObject* parent = nullptr);
    ~PolymarketAdapter() override;

    // Identity
    QString id() const override;
    QString display_name() const override;
    fincept::services::prediction::ExchangeCapabilities capabilities() const override;

    // Read
    void list_markets(const QString& category, const QString& sort_by, int limit, int offset) override;
    void list_events(const QString& category, const QString& sort_by, int limit, int offset) override;
    void search(const QString& query, int limit) override;
    void list_tags() override;
    void fetch_market(const fincept::services::prediction::MarketKey& key) override;
    void fetch_event(const fincept::services::prediction::MarketKey& key) override;
    void fetch_order_book(const QString& asset_id) override;
    void fetch_price_history(const QString& asset_id, const QString& interval, int fidelity) override;
    void fetch_recent_trades(const fincept::services::prediction::MarketKey& key, int limit) override;
    void fetch_top_holders(const fincept::services::prediction::MarketKey& key, int limit) override;
    void fetch_leaderboard(int limit) override;

    // WebSocket
    void subscribe_market(const QStringList& asset_ids) override;
    void unsubscribe_market(const QStringList& asset_ids) override;
    bool is_ws_connected() const override;

    // Auth (Phase 6 — stubs here)
    bool has_credentials() const override;
    QString account_label() const override;
    void fetch_balance() override;
    void fetch_positions() override;
    void fetch_open_orders() override;
    void fetch_user_activity(int limit) override;
    void place_order(const fincept::services::prediction::OrderRequest& req) override;
    void cancel_order(const QString& order_id) override;
    void cancel_all_for_market(const fincept::services::prediction::MarketKey& key,
                               const QString& asset_id) override;

    void ensure_registered_with_hub() override;

    /// Reload creds from SecureStorage — screens call this after the
    /// account dialog saves.
    void reload_credentials();

  private slots:
    // From PolymarketService
    void on_markets(const QVector<fincept::services::polymarket::Market>& markets);
    void on_events(const QVector<fincept::services::polymarket::Event>& events);
    void on_tags(const QVector<fincept::services::polymarket::Tag>& tags);
    void on_market_detail(const fincept::services::polymarket::Market& market);
    void on_event_detail(const fincept::services::polymarket::Event& event);
    void on_order_book(const fincept::services::polymarket::OrderBook& book);
    void on_price_history(const fincept::services::polymarket::PriceHistory& history);
    void on_trades(const QVector<fincept::services::polymarket::Trade>& trades);
    void on_service_error(const QString& ctx, const QString& msg);

    // From PolymarketWebSocket
    void on_ws_price(const QString& asset_id, double price);
    void on_ws_orderbook(const QString& asset_id, const fincept::services::polymarket::OrderBook& book);
    void on_ws_status(bool connected);

  private:
    void wire_service();
    void wire_ws();
    void stub_unsupported(const QString& ctx);

    /// Ensure we have L2 api_key/secret/passphrase cached; derive from L1
    /// if not. Returns the creds (with api_key populated) or nullopt on
    /// failure. `then` fires on the main thread with the creds or the
    /// error emitted via error_occurred().
    void ensure_api_creds(std::function<void(const fincept::services::prediction::PolymarketCredentials&)> then,
                          const QString& ctx);
    void run_py(const QString& command, const QJsonObject& payload,
                std::function<void(const QJsonObject&)> on_ok, const QString& ctx);
    QJsonObject creds_to_json(const fincept::services::prediction::PolymarketCredentials& c) const;

    fincept::services::polymarket::PolymarketService* service_ = nullptr;  // singleton ptr, not owned
    fincept::services::polymarket::PolymarketWebSocket* ws_ = nullptr;     // singleton ptr, not owned

    // Per-request context kept between service callback and our signal.
    QString last_history_asset_id_;

    bool hub_registered_ = false;

    // Authenticated-path state.
    std::optional<fincept::services::prediction::PolymarketCredentials> creds_;
    QString cached_wallet_address_;  // derived once from private key
};

} // namespace fincept::services::prediction::polymarket_ns
