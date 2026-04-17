#include "screens/polymarket/PolymarketScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/polymarket/PolymarketBrowsePanel.h"
#include "screens/polymarket/PolymarketCommandBar.h"
#include "screens/polymarket/PolymarketDetailPanel.h"
#include "screens/polymarket/PolymarketLeaderboard.h"
#include "screens/polymarket/PolymarketStatusBar.h"
#include "services/polymarket/PolymarketService.h"
#include "services/polymarket/PolymarketWebSocket.h"
#include "ui/theme/Theme.h"

#include <QSplitter>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::services::polymarket;
using namespace fincept::screens::polymarket;

// ── Constructor / Destructor ────────────────────────────────────────────────

PolymarketScreen::PolymarketScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("polyScreen");
    setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE()));
    build_ui();
    connect_service();
    connect_websocket();

    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(60000);
    connect(refresh_timer_, &QTimer::timeout, this, &PolymarketScreen::on_refresh);

    LOG_INFO("Polymarket", "Screen constructed");
}

PolymarketScreen::~PolymarketScreen() {
    unsubscribe_current();
}

// ── Visibility (P3) ─────────────────────────────────────────────────────────

void PolymarketScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh_timer_->start();
    if (first_show_) {
        first_show_ = false;
        PolymarketService::instance().fetch_tags();
        load_current_view();
    }
    if (has_selection_ && !subscribed_tokens_.isEmpty()) {
        PolymarketWebSocket::instance().subscribe(subscribed_tokens_);
    }
}

void PolymarketScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    refresh_timer_->stop();
}

// ── UI Build ────────────────────────────────────────────────────────────────

void PolymarketScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    command_bar_ = new PolymarketCommandBar;
    root->addWidget(command_bar_);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet(QString("QSplitter::handle { background: %1; }").arg(ui::colors::BORDER_DIM()));

    browse_panel_ = new PolymarketBrowsePanel;
    browse_panel_->setMinimumWidth(320);
    browse_panel_->setMaximumWidth(420);

    detail_panel_ = new PolymarketDetailPanel;

    leaderboard_ = new PolymarketLeaderboard;
    leaderboard_->setMinimumWidth(280);
    leaderboard_->setMaximumWidth(340);
    leaderboard_->setVisible(false); // shown only in LEADERBOARD view (future)

    splitter->addWidget(browse_panel_);
    splitter->addWidget(detail_panel_);
    splitter->addWidget(leaderboard_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({360, 700, 0});

    root->addWidget(splitter, 1);

    status_bar_ = new PolymarketStatusBar;
    root->addWidget(status_bar_);

    // Wire command bar signals
    connect(command_bar_, &PolymarketCommandBar::view_changed, this, &PolymarketScreen::on_view_changed);
    connect(command_bar_, &PolymarketCommandBar::category_changed, this, &PolymarketScreen::on_category_changed);
    connect(command_bar_, &PolymarketCommandBar::search_submitted, this, &PolymarketScreen::on_search_submitted);
    connect(command_bar_, &PolymarketCommandBar::sort_changed, this, &PolymarketScreen::on_sort_changed);
    connect(command_bar_, &PolymarketCommandBar::refresh_clicked, this, &PolymarketScreen::on_refresh);

    // Wire browse panel signals
    connect(browse_panel_, &PolymarketBrowsePanel::market_selected, this, &PolymarketScreen::on_market_selected);
    connect(browse_panel_, &PolymarketBrowsePanel::event_selected, this, &PolymarketScreen::on_event_selected);

    // Wire detail panel signals
    connect(detail_panel_, &PolymarketDetailPanel::interval_changed, this, &PolymarketScreen::on_interval_changed);
    connect(detail_panel_, &PolymarketDetailPanel::outcome_changed, this, &PolymarketScreen::on_outcome_changed);
    connect(detail_panel_, &PolymarketDetailPanel::related_market_clicked, this,
            &PolymarketScreen::on_related_market_clicked);
}

// ── Service Wiring ──────────────────────────────────────────────────────────

void PolymarketScreen::connect_service() {
    auto& svc = PolymarketService::instance();
    connect(&svc, &PolymarketService::markets_ready, this, &PolymarketScreen::on_markets_received);
    connect(&svc, &PolymarketService::events_ready, this, &PolymarketScreen::on_events_received);
    connect(&svc, &PolymarketService::tags_ready, this, &PolymarketScreen::on_tags_received);
    connect(&svc, &PolymarketService::order_book_ready, this, &PolymarketScreen::on_order_book_received);
    connect(&svc, &PolymarketService::price_history_ready, this, &PolymarketScreen::on_price_history_received);
    connect(&svc, &PolymarketService::trades_ready, this, &PolymarketScreen::on_trades_received);
    connect(&svc, &PolymarketService::price_summary_ready, this, &PolymarketScreen::on_price_summary_received);
    connect(&svc, &PolymarketService::top_holders_ready, this, &PolymarketScreen::on_top_holders_received);
    connect(&svc, &PolymarketService::leaderboard_ready, this, &PolymarketScreen::on_leaderboard_received);
    connect(&svc, &PolymarketService::comments_ready, this, &PolymarketScreen::on_comments_received);
    connect(&svc, &PolymarketService::related_markets_ready, this, &PolymarketScreen::on_related_markets_received);
    connect(&svc, &PolymarketService::request_error, this, &PolymarketScreen::on_service_error);
}

void PolymarketScreen::connect_websocket() {
    auto& ws = PolymarketWebSocket::instance();
    connect(&ws, &PolymarketWebSocket::price_updated, this, &PolymarketScreen::on_ws_price_updated);
    connect(&ws, &PolymarketWebSocket::orderbook_updated, this, &PolymarketScreen::on_ws_orderbook_updated);
    connect(&ws, &PolymarketWebSocket::connection_status_changed, this, &PolymarketScreen::on_ws_status_changed);
}

// ── Command Bar Slots ───────────────────────────────────────────────────────

void PolymarketScreen::on_view_changed(const QString& view) {
    active_view_ = view;
    command_bar_->set_active_view(view);
    status_bar_->set_view(view);
    load_current_view();
    LOG_INFO("Polymarket", "View: " + view);
}

void PolymarketScreen::on_category_changed(const QString& category) {
    active_category_ = category;
    command_bar_->set_active_category(category);
    ScreenStateManager::instance().notify_changed(this);
    load_current_view();
}

void PolymarketScreen::on_search_submitted(const QString& query) {
    if (query.isEmpty()) {
        load_current_view();
        return;
    }
    command_bar_->set_loading(true);
    browse_panel_->set_loading(true);
    ++request_generation_;
    PolymarketService::instance().search_markets(query, 50);
}

void PolymarketScreen::on_sort_changed(const QString& sort_by) {
    active_sort_ = sort_by;
    if (!first_show_)
        load_current_view();
}

void PolymarketScreen::on_refresh() {
    load_current_view();
}

// ── Browse Panel Slots ──────────────────────────────────────────────────────

void PolymarketScreen::on_market_selected(const Market& market) {
    select_market(market);
}

void PolymarketScreen::on_event_selected(const Event& event) {
    if (!event.markets.isEmpty()) {
        select_market(event.markets[0]);
    }
    // Also fetch related markets from this event
    if (event.id > 0) {
        PolymarketService::instance().fetch_related_markets(event.id);
    }
}

// ── Detail Panel Slots ──────────────────────────────────────────────────────

void PolymarketScreen::on_interval_changed(const QString& interval) {
    if (!has_selection_ || selected_market_.clob_token_ids.isEmpty())
        return;
    int fidelity = 5;
    if (interval == "1h" || interval == "6h")
        fidelity = 1;
    else if (interval == "1w")
        fidelity = 30;
    else if (interval == "1m" || interval == "max")
        fidelity = 60;

    PolymarketService::instance().fetch_price_history(selected_market_.clob_token_ids[0], interval, fidelity);
}

void PolymarketScreen::on_outcome_changed(int index) {
    if (!has_selection_ || index >= selected_market_.clob_token_ids.size())
        return;
    PolymarketService::instance().fetch_price_history(selected_market_.clob_token_ids[index], "1d", 5);
}

void PolymarketScreen::on_related_market_clicked(const Market& market) {
    select_market(market);
}

// ── Data Loading ────────────────────────────────────────────────────────────

void PolymarketScreen::load_current_view() {
    command_bar_->set_loading(true);
    browse_panel_->set_loading(true);
    ++request_generation_;

    auto& svc = PolymarketService::instance();

    if (active_view_ == "TRENDING") {
        svc.fetch_markets("volume", 100, 0, false);
    } else if (active_view_ == "MARKETS") {
        if (active_category_ != "ALL") {
            svc.fetch_markets_by_tag(active_category_, active_sort_, 100, 0);
        } else {
            svc.fetch_markets(active_sort_, 100, 0, false);
        }
    } else if (active_view_ == "EVENTS") {
        svc.fetch_events(active_sort_, 100, 0, false);
    } else if (active_view_ == "SPORTS") {
        svc.fetch_markets_by_tag("sports", active_sort_, 100, 0);
    } else if (active_view_ == "RESOLVED") {
        svc.fetch_events("endDate", 100, 0, true);
    }
}

void PolymarketScreen::select_market(const Market& market) {
    unsubscribe_current();

    selected_market_ = market;
    has_selection_ = true;

    detail_panel_->set_market(market);
    status_bar_->set_selected(market.question);

    auto& svc = PolymarketService::instance();

    if (!market.clob_token_ids.isEmpty()) {
        QString token = market.clob_token_ids[0];
        svc.fetch_order_book(token);
        svc.fetch_price_summary(token);
        svc.fetch_price_history(token, "1d", 5);
    }
    if (!market.condition_id.isEmpty()) {
        svc.fetch_trades(market.condition_id, 100);
        svc.fetch_top_holders(market.condition_id, 20);
        svc.fetch_open_interest({market.condition_id});
    }
    if (!market.slug.isEmpty()) {
        svc.fetch_comments(market.slug, 50);
    }
    if (market.event_id > 0) {
        svc.fetch_related_markets(market.event_id);
    }

    subscribe_to_market(market);
}

void PolymarketScreen::subscribe_to_market(const Market& market) {
    subscribed_tokens_ = market.clob_token_ids;
    if (!subscribed_tokens_.isEmpty()) {
        PolymarketWebSocket::instance().subscribe(subscribed_tokens_);
    }
}

void PolymarketScreen::unsubscribe_current() {
    if (!subscribed_tokens_.isEmpty()) {
        PolymarketWebSocket::instance().unsubscribe(subscribed_tokens_);
        subscribed_tokens_.clear();
    }
}

// ── Service Response Handlers ───────────────────────────────────────────────

void PolymarketScreen::on_markets_received(const QVector<Market>& markets) {
    command_bar_->set_loading(false);
    browse_panel_->set_markets(markets);
    command_bar_->set_market_count(markets.size());
    status_bar_->set_count(markets.size(), "markets");
}

void PolymarketScreen::on_events_received(const QVector<Event>& events) {
    command_bar_->set_loading(false);
    browse_panel_->set_events(events);
    command_bar_->set_market_count(events.size());
    status_bar_->set_count(events.size(), "events");
}

void PolymarketScreen::on_tags_received(const QVector<Tag>& tags) {
    command_bar_->set_categories(tags);
}

void PolymarketScreen::on_order_book_received(const OrderBook& book) {
    detail_panel_->set_order_book(book);
}

void PolymarketScreen::on_price_history_received(const PriceHistory& history) {
    detail_panel_->set_price_history(history);
}

void PolymarketScreen::on_trades_received(const QVector<Trade>& trades) {
    detail_panel_->set_trades(trades);
}

void PolymarketScreen::on_price_summary_received(const PriceSummary& summary) {
    detail_panel_->set_price_summary(summary);
}

void PolymarketScreen::on_top_holders_received(const QVector<TopHolder>& holders) {
    detail_panel_->set_top_holders(holders);
}

void PolymarketScreen::on_leaderboard_received(const QVector<LeaderboardEntry>& entries) {
    leaderboard_->set_entries(entries);
}

void PolymarketScreen::on_comments_received(const QVector<Comment>& comments) {
    detail_panel_->set_comments(comments);
}

void PolymarketScreen::on_related_markets_received(const QVector<Market>& markets) {
    detail_panel_->set_related_markets(markets);
}

void PolymarketScreen::on_service_error(const QString& ctx, const QString& msg) {
    command_bar_->set_loading(false);
    LOG_ERROR("Polymarket", ctx + ": " + msg);
}

// ── WebSocket Handlers ──────────────────────────────────────────────────────

void PolymarketScreen::on_ws_price_updated(const QString& asset_id, double price) {
    // Update detail panel if this is our selected market
    if (has_selection_) {
        for (int i = 0; i < selected_market_.clob_token_ids.size(); ++i) {
            if (selected_market_.clob_token_ids[i] == asset_id) {
                if (i < selected_market_.outcomes.size()) {
                    selected_market_.outcomes[i].price = price;
                    detail_panel_->set_market(selected_market_);
                }
                break;
            }
        }
    }
}

void PolymarketScreen::on_ws_orderbook_updated(const QString& /*asset_id*/, const OrderBook& book) {
    detail_panel_->set_order_book(book);
}

void PolymarketScreen::on_ws_status_changed(bool connected) {
    command_bar_->set_ws_status(connected);
    status_bar_->set_ws_status(connected);
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap PolymarketScreen::save_state() const {
    return {
        {"category", active_category_},
        {"view", active_view_},
        {"sort", active_sort_},
    };
}

void PolymarketScreen::restore_state(const QVariantMap& state) {
    const QString cat = state.value("category", "ALL").toString();
    const QString view = state.value("view", "MARKETS").toString();
    const QString sort = state.value("sort", "volume").toString();

    active_sort_ = sort;
    active_view_ = view;
    on_category_changed(cat);
}

} // namespace fincept::screens
