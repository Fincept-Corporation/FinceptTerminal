#pragma once

#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "screens/polymarket/ExchangePresentation.h"
#include "services/polymarket/PolymarketTypes.h"
#include "services/prediction/PredictionTypes.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMetaObject>
#include <QTimer>
#include <QWidget>

#include <atomic>

namespace fincept::screens::polymarket {
class PolymarketCommandBar;
class PolymarketBrowsePanel;
class PolymarketDetailPanel;
class PolymarketLeaderboard;
class PolymarketOrderBlotter;
class PolymarketStatusBar;
} // namespace fincept::screens::polymarket

namespace fincept::services::prediction {
class PredictionExchangeAdapter;
}

namespace fincept::screens {

/// Production-grade Prediction Markets screen.
///
/// Thin coordinator owning 5 sub-widget panels. Data flows through the
/// active prediction::PredictionExchangeAdapter (Polymarket or Kalshi),
/// resolved via PredictionExchangeRegistry. Switching the exchange in the
/// command bar drops current adapter connections, clears the UI, and
/// reloads the view from the new adapter — no legacy-service coupling.
///
/// Polymarket-only enrichments (price summary, top holders, comments,
/// related markets, open interest) are fetched directly from the
/// PolymarketService singleton when the active exchange is Polymarket.
/// On Kalshi, those detail-panel tabs are hidden.
class PolymarketScreen : public QWidget, public fincept::screens::IStatefulScreen, public ISectionLinked {
    Q_OBJECT
  public:
    explicit PolymarketScreen(QWidget* parent = nullptr);
    ~PolymarketScreen() override;

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "polymarket"; }
    int state_version() const override { return 1; }

    /// ISectionLinked — a view of the active exchange: trending | markets | events | sports | resolved
    /// (Polymarket), markets | events | settled | history (Kalshi); "resolved" and "settled" stand in
    /// for each other; also "view:<id>". A view the active exchange lacks returns false. The view
    /// pill's click path (it loads that public list, as a click does); the view on screen is left
    /// alone. focus is unused (a search would query the exchange).
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private slots:
    // Command bar
    void on_view_changed(const QString& view);
    void on_category_changed(const QString& category);
    void on_search_submitted(const QString& query);
    void on_sort_changed(const QString& sort_by);
    void on_refresh();
    void on_exchange_changed(const QString& exchange_id);

    // Browse panel
    void on_market_selected(const fincept::services::prediction::PredictionMarket& market);
    void on_event_selected(const fincept::services::prediction::PredictionEvent& event);

    // Detail panel
    void on_interval_changed(const QString& interval);
    void on_outcome_changed(int index);
    void on_related_market_clicked(const fincept::services::prediction::PredictionMarket& market);

    // Adapter (unified) responses
    void on_markets_ready(const QVector<fincept::services::prediction::PredictionMarket>& markets);
    void on_events_ready(const QVector<fincept::services::prediction::PredictionEvent>& events);
    void on_search_results_ready(const QVector<fincept::services::prediction::PredictionMarket>& markets,
                                 const QVector<fincept::services::prediction::PredictionEvent>& events);
    void on_tags_ready(const QStringList& tags);
    void on_order_book_ready(const fincept::services::prediction::PredictionOrderBook& book);
    void on_price_history_ready(const fincept::services::prediction::PriceHistory& history);
    void on_trades_ready(const QVector<fincept::services::prediction::PredictionTrade>& trades);
    void on_leaderboard_ready(const QVariantList& entries);
    void on_adapter_error(const QString& ctx, const QString& msg);
    void on_ws_price_updated(const QString& asset_id, double price);
    void on_ws_orderbook_updated(const QString& asset_id,
                                 const fincept::services::prediction::PredictionOrderBook& book);
    void on_ws_connection_changed(bool connected);

    // Polymarket-only (PolymarketService) enrichments
    void on_price_summary_received(const fincept::services::polymarket::PriceSummary& summary);
    void on_top_holders_received(const QVector<fincept::services::polymarket::TopHolder>& holders);
    void on_comments_received(const QVector<fincept::services::polymarket::Comment>& comments);
    void on_related_markets_received(const QVector<fincept::services::polymarket::Market>& markets);

    // Kalshi-only enrichments (surfaced via KalshiAdapter-specific signals).
    void on_kalshi_exchange_status(const QJsonObject& status);
    void on_kalshi_exchange_schedule(const QJsonObject& schedule);
    void on_kalshi_ws_trade(const fincept::services::prediction::PredictionTrade& trade);
    void on_kalshi_market_lifecycle(const QString& ticker, const QString& status);
    void on_kalshi_batch_candles(const QHash<QString, fincept::services::prediction::PriceHistory>& histories);
    void on_kalshi_market_detail(const fincept::services::prediction::PredictionMarket& market);
    void on_kalshi_series_detail(const QString& series_ticker, const QJsonObject& series);

    // Open-orders blotter.
    void on_open_orders_ready(const QVector<fincept::services::prediction::OpenOrder>& orders);
    void on_order_refresh_requested(const QString& order_id);
    void on_order_amend_requested(const QString& order_id, const QString& side, double price);
    void on_order_cancel_requested(const QString& order_id);
    void on_orders_cancel_all_requested(const QStringList& order_ids);
    void on_kalshi_single_order(const QJsonObject& order);
    /// Blotter ACCOUNT HISTORY tab asks for a page (Kalshi settlements / fills / orders).
    void on_account_history_requested(const QString& kind, const QString& cursor);

  private:
    void build_ui();
    void connect_active_adapter();
    void disconnect_active_adapter();
    void connect_polymarket_extras();
    void install_presentation(const screens::polymarket::ExchangePresentation& p);
    /// After a theme change: hand every sub-panel a freshly built profile of the
    /// wired exchange, whose accent / badge colours come from the new theme.
    void refresh_presentation_colors();
    /// Enable/disable the trade ticket and open-orders blotter to match the active
    /// adapter's credential state, and pull balance / positions / orders if connected.
    void refresh_trading_state();
    void load_current_view();
    /// Common start of every list load (view / category / sort / search / refresh):
    /// the list's loading state, the detail panel's placeholder when nothing is
    /// selected, the "no results" wording and the no-answer watchdog.
    void begin_list_load(const QString& filter_context);
    /// Nothing is selected and a list is on screen: select the restored market (or
    /// the first card) — every detail tab then loads. Deferred while hidden.
    void auto_select_default();
    /// The list failed / came back empty and nothing is selected: the detail panel
    /// says so instead of waiting for a selection.
    void settle_detail_without_list(const QString& reason);
    /// Re-read both exchanges' stored credentials (with read errors) and update the
    /// account chip and the TRADE tab. A read error is shown as "unreadable — RETRY",
    /// never as "no account".
    void refresh_account_state();
    /// RETRY after an unreadable credential: read again, install what is readable
    /// on the adapters (an unreadable one keeps what its adapter holds).
    void retry_stored_credentials();
    void select_market(const fincept::services::prediction::PredictionMarket& market);
    void subscribe_to_market(const fincept::services::prediction::PredictionMarket& market);
    void unsubscribe_current();
    /// Fetch the chart's price history for the outcome + interval the chart toolbar is
    /// currently showing (chart_outcome_idx_ / chart_interval_). `show_loading`: the
    /// chart shows its skeleton until the reply (off for the quiet periodic re-poll).
    void fetch_chart_history(bool show_loading = true);
    /// The account dialog (CONNECT on the chip, CONNECT ACCOUNT on the TRADE tab).
    void open_account_dialog();
    /// True when `asset_id` is the selected market's primary outcome (the one the
    /// order book panel displays). See on_order_book_ready().
    bool is_primary_asset(const QString& asset_id) const;

    bool active_is_polymarket() const;
    fincept::services::prediction::PredictionExchangeAdapter* active_adapter() const;

    // Sub-widgets
    polymarket::PolymarketCommandBar* command_bar_ = nullptr;
    polymarket::PolymarketBrowsePanel* browse_panel_ = nullptr;
    polymarket::PolymarketDetailPanel* detail_panel_ = nullptr;
    polymarket::PolymarketLeaderboard* leaderboard_ = nullptr;
    polymarket::PolymarketOrderBlotter* order_blotter_ = nullptr;
    polymarket::PolymarketStatusBar* status_bar_ = nullptr;

    // State
    QString active_view_ = "MARKETS";
    QString active_category_ = "ALL";
    QString active_sort_ = "volume";
    fincept::services::prediction::PredictionMarket selected_market_;
    bool has_selection_ = false;
    // Market selected in the previous session (save_state): preferred by the
    // automatic selection once, on the exchange it belonged to.
    QString restored_market_id_;
    QString restored_market_exchange_;
    // A list arrived while the screen was hidden: select on the next show.
    bool auto_select_pending_ = false;
    // Stored credentials that exist but could not be read ("" = readable / none).
    QString pm_read_error_;
    QString ks_read_error_;
    // A list load that never answers (no reply, no error) ends in the failure page.
    QTimer* list_watchdog_ = nullptr;
    // What the chart toolbar is showing, so an interval change keeps the chosen outcome
    // and an outcome / market change keeps the chosen interval.
    QString chart_interval_ = QStringLiteral("1d");
    int chart_outcome_idx_ = 0;
    int orders_refresh_seq_ = 0; // coalesces the open-orders refresh after a burst of cancels
    bool first_show_ = true;
    std::atomic<int> request_generation_{0};
    // True while the list reload issued by on_refresh() (60 s timer / ↻) is in
    // flight: its reply keeps the browse list's page, scroll and highlight.
    // Cleared by every user-driven load (view / category / sort / search).
    bool refresh_reload_pending_ = false;

    // Timer
    QTimer* refresh_timer_ = nullptr;

    // WS tracking
    QStringList subscribed_asset_ids_;

    // Per-adapter Qt connections, dropped whenever the active exchange
    // changes so the next adapter's signals don't clash with the previous
    // one's. Polymarket-only enrichment connections live separately because
    // they target the PolymarketService singleton (not the active adapter).
    QList<QMetaObject::Connection> adapter_connections_;
    QList<QMetaObject::Connection> polymarket_extras_connections_;
    QString wired_adapter_id_;

    // Batch-candles cache for Kalshi browse panel sparklines. Populated in
    // on_markets_ready (Kalshi only). Keyed by market_id.
    QHash<QString, fincept::services::prediction::PriceHistory> batch_candles_cache_;
};

} // namespace fincept::screens
