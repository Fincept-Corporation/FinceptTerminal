#pragma once

#include "services/polymarket/PolymarketTypes.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

/// Production-grade Polymarket Prediction Markets screen.
///
/// Views: Markets · Events · Resolved
/// Detail tabs: Overview · Order Book · Price Chart · Trades
///
/// All data flows through PolymarketService (singleton).
/// Screen renders UI only — no direct HTTP calls (P6).
/// Timer-driven auto-refresh respects visibility (P3).
class PolymarketScreen : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private slots:
    // View navigation
    void on_view_changed(int view);
    void on_search();
    void on_sort_changed(int index);
    void on_refresh();
    void on_prev_page();
    void on_next_page();

    // List selection
    void on_market_clicked(QListWidgetItem* item);

    // Detail tabs
    void on_detail_tab_changed(int tab);
    void on_history_interval_changed(int index);

    // Service signals
    void on_markets_received(const QVector<services::polymarket::Market>& markets);
    void on_events_received(const QVector<services::polymarket::Event>& events);
    void on_order_book_received(const services::polymarket::OrderBook& book);
    void on_price_history_received(const services::polymarket::PriceHistory& history);
    void on_trades_received(const QVector<services::polymarket::Trade>& trades);
    void on_price_summary_received(const services::polymarket::PriceSummary& summary);
    void on_service_error(const QString& ctx, const QString& msg);

  private:
    void setup_ui();
    void connect_service();

    QWidget* create_header();
    QWidget* create_list_panel();
    QWidget* create_detail_panel();
    QWidget* create_overview_page();
    QWidget* create_orderbook_page();
    QWidget* create_chart_page();
    QWidget* create_trades_page();
    QWidget* create_status_bar();

    void load_current_view();
    void display_market_list();
    void display_event_list();
    void select_market(const services::polymarket::Market& market);
    void set_loading(bool loading);
    void update_pagination();

    // ── State ────────────────────────────────────────────────────────────
    enum View { MARKETS = 0, EVENTS = 1, RESOLVED = 2 };
    View active_view_ = MARKETS;
    int detail_tab_ = 0;
    int current_page_ = 0;
    static constexpr int PAGE_SIZE = 20;
    bool loading_ = false;
    bool first_show_ = true;

    // Cached data
    QVector<services::polymarket::Market> markets_;
    QVector<services::polymarket::Event> events_;
    services::polymarket::Market selected_market_;
    bool has_selection_ = false;

    // ── Timers ───────────────────────────────────────────────────────────
    QTimer* refresh_timer_ = nullptr;

    // ── Header ───────────────────────────────────────────────────────────
    QList<QPushButton*> view_btns_;
    QLineEdit* search_input_ = nullptr;
    QComboBox* sort_combo_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;

    // ── List panel ───────────────────────────────────────────────────────
    QListWidget* market_list_ = nullptr;
    QLabel* list_header_ = nullptr;
    QPushButton* prev_btn_ = nullptr;
    QPushButton* next_btn_ = nullptr;
    QLabel* page_label_ = nullptr;

    // ── Detail panel ─────────────────────────────────────────────────────
    QStackedWidget* detail_stack_ = nullptr;
    QList<QPushButton*> detail_tab_btns_;

    // Overview
    QLabel* detail_question_ = nullptr;
    QLabel* detail_volume_ = nullptr;
    QLabel* detail_liquidity_ = nullptr;
    QLabel* detail_end_date_ = nullptr;
    QLabel* detail_midpoint_ = nullptr;
    QLabel* detail_spread_ = nullptr;
    QLabel* detail_last_trade_ = nullptr;
    QLabel* detail_status_ = nullptr;
    QWidget* outcome_container_ = nullptr;

    // Order book
    QTableWidget* orderbook_table_ = nullptr;

    // Price chart
    QWidget* chart_widget_ = nullptr;
    QComboBox* interval_combo_ = nullptr;

    // Trades
    QTableWidget* trades_table_ = nullptr;

    // ── Status bar ───────────────────────────────────────────────────────
    QLabel* status_view_ = nullptr;
    QLabel* status_market_ = nullptr;
    QLabel* status_count_ = nullptr;
};

} // namespace fincept::screens
