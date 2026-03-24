#pragma once

#include "services/polymarket/PolymarketTypes.h"

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

namespace fincept::screens {

/// Production-grade Polymarket Prediction Markets screen.
/// Thin coordinator owning 5 sub-widget panels, wiring service + WebSocket.
class PolymarketScreen : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketScreen(QWidget* parent = nullptr);
    ~PolymarketScreen() override;

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

    // Browse panel
    void on_market_selected(const services::polymarket::Market& market);
    void on_event_selected(const services::polymarket::Event& event);

    // Detail panel
    void on_interval_changed(const QString& interval);
    void on_outcome_changed(int index);
    void on_related_market_clicked(const services::polymarket::Market& market);

    // Service responses
    void on_markets_received(const QVector<services::polymarket::Market>& markets);
    void on_events_received(const QVector<services::polymarket::Event>& events);
    void on_tags_received(const QVector<services::polymarket::Tag>& tags);
    void on_order_book_received(const services::polymarket::OrderBook& book);
    void on_price_history_received(const services::polymarket::PriceHistory& history);
    void on_trades_received(const QVector<services::polymarket::Trade>& trades);
    void on_price_summary_received(const services::polymarket::PriceSummary& summary);
    void on_top_holders_received(const QVector<services::polymarket::TopHolder>& holders);
    void on_leaderboard_received(const QVector<services::polymarket::LeaderboardEntry>& entries);
    void on_comments_received(const QVector<services::polymarket::Comment>& comments);
    void on_related_markets_received(const QVector<services::polymarket::Market>& markets);
    void on_service_error(const QString& ctx, const QString& msg);

    // WebSocket
    void on_ws_price_updated(const QString& asset_id, double price);
    void on_ws_orderbook_updated(const QString& asset_id, const services::polymarket::OrderBook& book);
    void on_ws_status_changed(bool connected);

  private:
    void build_ui();
    void connect_service();
    void connect_websocket();
    void load_current_view();
    void select_market(const services::polymarket::Market& market);
    void subscribe_to_market(const services::polymarket::Market& market);
    void unsubscribe_current();

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
    services::polymarket::Market selected_market_;
    bool has_selection_ = false;
    bool first_show_ = true;
    std::atomic<int> request_generation_{0};

    // Timer
    QTimer* refresh_timer_ = nullptr;

    // WS tracking
    QStringList subscribed_tokens_;
};

} // namespace fincept::screens
