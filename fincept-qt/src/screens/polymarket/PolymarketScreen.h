#pragma once

#include "screens/IStatefulScreen.h"
#include "screens/polymarket/ExchangePresentation.h"
#include "services/polymarket/PolymarketTypes.h"
#include "services/prediction/PredictionTypes.h"

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
class PolymarketScreen : public QWidget, public fincept::screens::IStatefulScreen {
    Q_OBJECT
  public:
    explicit PolymarketScreen(QWidget* parent = nullptr);
    ~PolymarketScreen() override;

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "polymarket"; }
    int state_version() const override { return 1; }

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

  private:
    void build_ui();
    void connect_active_adapter();
    void disconnect_active_adapter();
    void connect_polymarket_extras();
    void install_presentation(const screens::polymarket::ExchangePresentation& p);
    void load_current_view();
    void select_market(const fincept::services::prediction::PredictionMarket& market);
    void subscribe_to_market(const fincept::services::prediction::PredictionMarket& market);
    void unsubscribe_current();

    bool active_is_polymarket() const;
    fincept::services::prediction::PredictionExchangeAdapter* active_adapter() const;

    // Sub-widgets
    polymarket::PolymarketCommandBar* command_bar_ = nullptr;
    polymarket::PolymarketBrowsePanel* browse_panel_ = nullptr;
    polymarket::PolymarketDetailPanel* detail_panel_ = nullptr;
    polymarket::PolymarketLeaderboard* leaderboard_ = nullptr;
    polymarket::PolymarketStatusBar* status_bar_ = nullptr;

    // State
    QString active_view_ = "MARKETS";
    QString active_category_ = "ALL";
    QString active_sort_ = "volume";
    fincept::services::prediction::PredictionMarket selected_market_;
    bool has_selection_ = false;
    bool first_show_ = true;
    std::atomic<int> request_generation_{0};

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
};

} // namespace fincept::screens
