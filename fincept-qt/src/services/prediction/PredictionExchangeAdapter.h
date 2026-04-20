#pragma once

#include "services/prediction/PredictionTypes.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

namespace fincept::services::prediction {

/// Abstract base for every prediction-market exchange integration.
///
/// One instance per exchange exists for the lifetime of the process, owned
/// by PredictionExchangeRegistry. Adapters encapsulate REST, WebSocket,
/// type mapping, credential storage, and DataHub producer registration.
///
/// Lifecycle:
///   * UI calls list_markets/list_events/search/... and listens for the
///     matching *_ready signals. Calls are async; a single inflight
///     generation is enough for the browse panel (results arrive in order
///     the adapter decides).
///   * subscribe_market(asset_ids) triggers WS streaming. Price/orderbook
///     updates flow via DataHub topics AND the ws_*_updated signals.
///   * Authenticated methods no-op + emit error() if has_credentials() is
///     false. Screens must inspect has_credentials() before showing the
///     order ticket / portfolio panel.
class PredictionExchangeAdapter : public QObject {
    Q_OBJECT
  public:
    explicit PredictionExchangeAdapter(QObject* parent = nullptr) : QObject(parent) {}
    ~PredictionExchangeAdapter() override = default;

    // ── Identity ─────────────────────────────────────────────────────────

    virtual QString id() const = 0;            // "polymarket" | "kalshi"
    virtual QString display_name() const = 0;  // "Polymarket", "Kalshi"
    virtual ExchangeCapabilities capabilities() const = 0;

    // ── Public / unsigned endpoints ──────────────────────────────────────

    virtual void list_markets(const QString& category, const QString& sort_by, int limit, int offset) = 0;
    virtual void list_events(const QString& category, const QString& sort_by, int limit, int offset) = 0;
    virtual void search(const QString& query, int limit) = 0;
    virtual void list_tags() = 0;
    virtual void fetch_market(const MarketKey& key) = 0;
    virtual void fetch_event(const MarketKey& key) = 0;
    virtual void fetch_order_book(const QString& asset_id) = 0;
    virtual void fetch_price_history(const QString& asset_id, const QString& interval, int fidelity) = 0;
    virtual void fetch_recent_trades(const MarketKey& key, int limit) = 0;

    /// Optional — adapters without this endpoint may emit a "not supported"
    /// error() and return. Default impl is a no-op.
    virtual void fetch_top_holders(const MarketKey& key, int limit) {
        Q_UNUSED(key);
        Q_UNUSED(limit);
    }
    virtual void fetch_leaderboard(int limit) { Q_UNUSED(limit); }

    // ── Real-time (WebSocket) ────────────────────────────────────────────

    virtual void subscribe_market(const QStringList& asset_ids) = 0;
    virtual void unsubscribe_market(const QStringList& asset_ids) = 0;
    virtual bool is_ws_connected() const = 0;

    // ── Authenticated / trading ──────────────────────────────────────────

    virtual bool has_credentials() const = 0;
    virtual QString account_label() const = 0;  // e.g. truncated wallet address
    virtual void fetch_balance() = 0;
    virtual void fetch_positions() = 0;
    virtual void fetch_open_orders() = 0;
    virtual void fetch_user_activity(int limit) = 0;
    virtual void place_order(const OrderRequest& req) = 0;
    virtual void cancel_order(const QString& order_id) = 0;
    virtual void cancel_all_for_market(const MarketKey& key, const QString& asset_id) = 0;

    // ── Hub registration ─────────────────────────────────────────────────

    /// Register this adapter's producers (WS + REST) with DataHub and
    /// install topic policies. Idempotent. Called from main.cpp once
    /// per adapter.
    virtual void ensure_registered_with_hub() = 0;

  signals:
    // Read
    void markets_ready(const QVector<fincept::services::prediction::PredictionMarket>& markets);
    void events_ready(const QVector<fincept::services::prediction::PredictionEvent>& events);
    void search_results_ready(const QVector<fincept::services::prediction::PredictionMarket>& markets,
                              const QVector<fincept::services::prediction::PredictionEvent>& events);
    void tags_ready(const QStringList& tags);
    void market_detail_ready(const fincept::services::prediction::PredictionMarket& market);
    void event_detail_ready(const fincept::services::prediction::PredictionEvent& event);
    void order_book_ready(const fincept::services::prediction::PredictionOrderBook& book);
    void price_history_ready(const fincept::services::prediction::PriceHistory& history);
    void recent_trades_ready(const QVector<fincept::services::prediction::PredictionTrade>& trades);
    void top_holders_ready(const QVariantList& holders);
    void leaderboard_ready(const QVariantList& entries);

    // WebSocket
    void ws_price_updated(const QString& asset_id, double price);
    void ws_orderbook_updated(const QString& asset_id, const fincept::services::prediction::PredictionOrderBook& book);
    void ws_connection_changed(bool connected);

    // Account / trading
    void credentials_changed();  // emitted when user adds/removes a credential set
    void balance_ready(const fincept::services::prediction::AccountBalance& balance);
    void positions_ready(const QVector<fincept::services::prediction::PredictionPosition>& positions);
    void open_orders_ready(const QVector<fincept::services::prediction::OpenOrder>& orders);
    void order_placed(const fincept::services::prediction::OrderResult& result);
    void order_cancelled(const QString& order_id, bool ok, const QString& error_message);
    void account_activity_ready(const QVariantList& activities);

    void error_occurred(const QString& context, const QString& message);
};

} // namespace fincept::services::prediction
