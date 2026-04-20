#pragma once

#include "services/prediction/PredictionExchangeAdapter.h"
#include "services/prediction/kalshi/KalshiCredentials.h"

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

namespace fincept::services::prediction::kalshi_ns {

class KalshiRestClient;
class KalshiWsClient;

/// Kalshi exchange adapter.
///
/// Owns its own REST client + WS client (unlike Polymarket which wraps
/// existing global singletons). Kalshi has no global singletons today —
/// this class IS the singleton-of-concerns for the Kalshi integration.
///
/// Phase 4 implements public read endpoints. Phase 7 wires RSA-PSS signed
/// trading methods via a Python bridge (py `cryptography` package).
class KalshiAdapter : public fincept::services::prediction::PredictionExchangeAdapter {
    Q_OBJECT
  public:
    explicit KalshiAdapter(QObject* parent = nullptr);
    ~KalshiAdapter() override;

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

    // WebSocket
    void subscribe_market(const QStringList& asset_ids) override;
    void unsubscribe_market(const QStringList& asset_ids) override;
    bool is_ws_connected() const override;

    // Auth / trading (Phase 7 — stubs here)
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

    // Credential injection (Phase 5 will wire this via the account dialog).
    void set_credentials(const KalshiCredentials& creds);

    // ── Kalshi-specific public reads (pass through to REST client) ──────

    /// GET /exchange/status + /exchange/schedule. Results arrive on the
    /// exchange_* signals below.
    void fetch_exchange_status();
    void fetch_exchange_schedule();
    /// GET /markets/candlesticks (batch). Results arrive on batch_candles_ready.
    void fetch_batch_candles(const QStringList& tickers, int period_interval_min,
                             qint64 start_ts, qint64 end_ts);
    /// GET /series/{series_ticker}. Result arrives on series_detail_ready.
    /// Cached per ticker — repeat calls are free.
    void fetch_series_detail(const QString& series_ticker);
    /// Lookup a cached series fee config without refetching. Returns an
    /// empty object if we haven't fetched this series yet.
    QJsonObject cached_series(const QString& series_ticker) const;
    /// GET /historical/markets / candlesticks / trades.
    void fetch_historical_markets(const QString& series_ticker = QString(),
                                  int limit = 100,
                                  const QString& cursor = QString());
    void fetch_historical_candles(const QString& ticker, int period_interval_min,
                                  qint64 start_ts, qint64 end_ts);
    void fetch_historical_trades(const QString& ticker = QString(),
                                 int limit = 100,
                                 const QString& cursor = QString());

    // ── Kalshi-specific trading (pass through to Python bridge) ─────────

    /// POST /portfolio/orders/{order_id}/amend.
    /// price_cents 1-99. `side` is the resting side ("yes" or "no") — the
    /// caller passes the order's current side so we target the right price
    /// field.
    void amend_order(const QString& order_id, const QString& side, int price_cents,
                     const QString& client_order_id = QString());
    /// GET /portfolio/orders/{order_id}. Result arrives on single_order_ready.
    void fetch_order(const QString& order_id);
    /// DELETE /portfolio/orders/batched. Result arrives on orders_batch_cancelled.
    void cancel_orders_batch(const QStringList& order_ids);

  signals:
    // Kalshi-specific extensions (not on the base adapter interface).
    // Consumers cast to KalshiAdapter* to connect.
    void ws_trade_received(const fincept::services::prediction::PredictionTrade& trade);
    void ws_market_lifecycle_changed(const QString& ticker, const QString& status);

    void exchange_status_ready(const QJsonObject& status);
    void exchange_schedule_ready(const QJsonObject& schedule);
    void batch_candles_ready(
        const QHash<QString, fincept::services::prediction::PriceHistory>& histories);
    void series_detail_ready(const QString& series_ticker, const QJsonObject& series);

    void historical_markets_ready(
        const QVector<fincept::services::prediction::PredictionMarket>& markets,
        const QString& next_cursor);
    void historical_candles_ready(
        const fincept::services::prediction::PriceHistory& history, const QString& ticker);
    void historical_trades_ready(
        const QVector<fincept::services::prediction::PredictionTrade>& trades,
        const QString& next_cursor);

    void order_amended(const QString& order_id, bool ok, const QString& error);
    void single_order_ready(const QJsonObject& order);
    void orders_batch_cancelled(const QStringList& order_ids, bool ok,
                                const QString& error);

  private:
    void wire();
    void stub_unsupported(const QString& ctx);

    /// Run a command against prediction_kalshi.py with the stored creds
    /// merged into `extra`. Invokes `on_ok` with the parsed JSON on
    /// success; emits error_occurred() on failure.
    void run_py(const QString& command, const QJsonObject& extra,
                std::function<void(const QJsonObject&)> on_ok, const QString& ctx);
    QJsonObject creds_to_json() const;

    /// Kalshi asset IDs look like "TICKER:yes" / "TICKER:no". Split into
    /// (ticker, side). Returns ("", "") if malformed.
    static std::pair<QString, QString> split_asset_id(const QString& asset_id);

    std::unique_ptr<KalshiRestClient> rest_;
    std::unique_ptr<KalshiWsClient> ws_;
    KalshiCredentials creds_;
    bool hub_registered_ = false;

    // Tracks the last-requested price-history asset id so REST callbacks
    // can attach it to the emitted PriceHistory (Kalshi REST only returns
    // YES-side candles).
    QString last_history_asset_id_;

    // Set to true while a search() call is in-flight so the markets_ready
    // handler can emit search_results_ready instead of markets_ready.
    bool search_pending_ = false;

    // Market ticker → series_ticker cache populated from markets_ready /
    // events_ready. Used by fetch_price_history to build the
    // /series/{s}/markets/{t}/candlesticks path without string heuristics.
    QHash<QString, QString> series_by_market_;

    // Cached /series/{ticker} responses. Keyed by series ticker. Populated
    // via fetch_series_detail; repeat fetches hit the cache.
    QHash<QString, QJsonObject> series_cache_;
};

} // namespace fincept::services::prediction::kalshi_ns
