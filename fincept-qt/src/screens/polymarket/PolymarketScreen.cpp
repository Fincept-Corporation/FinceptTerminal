#include "screens/polymarket/PolymarketScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/polymarket/PolymarketActivityFeed.h"
#include "screens/polymarket/PolymarketBrowsePanel.h"
#include "screens/polymarket/PolymarketCommandBar.h"
#include "screens/polymarket/PolymarketDetailPanel.h"
#include "screens/polymarket/PolymarketLeaderboard.h"
#include "screens/polymarket/PolymarketPriceChart.h"
#include "screens/polymarket/PolymarketStatusBar.h"
#include "screens/polymarket/PredictionAccountDialog.h"
#include "services/polymarket/PolymarketService.h"
#include "services/prediction/PredictionCredentialStore.h"
#include "services/prediction/PredictionExchangeAdapter.h"
#include "services/prediction/PredictionExchangeRegistry.h"
#include "services/prediction/kalshi/KalshiAdapter.h"
#include "services/prediction/polymarket/PolymarketAdapter.h"
#include "ui/theme/Theme.h"

#include <QSplitter>
#include <QVBoxLayout>

namespace fincept::screens {

namespace pred = fincept::services::prediction;
namespace pmx = fincept::services::polymarket;
using namespace fincept::screens::polymarket;

static QString kPolymarketId() { return QStringLiteral("polymarket"); }

// ── Constructor / Destructor ────────────────────────────────────────────────

PolymarketScreen::PolymarketScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("polyScreen");
    setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE()));
    build_ui();

    // Polymarket-only enrichments (price summary, top holders, comments,
    // related markets) only fire when the active adapter is Polymarket, but
    // the underlying service signals are always connected so the screen
    // doesn't re-wire every exchange swap. The guard lives at emit time.
    connect_polymarket_extras();

    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(60000);
    connect(refresh_timer_, &QTimer::timeout, this, &PolymarketScreen::on_refresh);

    LOG_INFO("PredictionMarkets", "Screen constructed");
}

PolymarketScreen::~PolymarketScreen() {
    unsubscribe_current();
    disconnect_active_adapter();
    for (const auto& c : polymarket_extras_connections_) QObject::disconnect(c);
    polymarket_extras_connections_.clear();
}

// ── Visibility (P3) ─────────────────────────────────────────────────────────

void PolymarketScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh_timer_->start();
    if (first_show_) {
        first_show_ = false;

        auto& reg = pred::PredictionExchangeRegistry::instance();
        const QStringList ids = reg.available_ids();
        QStringList labels;
        labels.reserve(ids.size());
        for (const auto& id : ids) {
            auto* adapter = reg.adapter(id);
            labels.push_back(adapter ? adapter->display_name() : id);
        }
        if (ids.isEmpty()) {
            command_bar_->set_exchanges({kPolymarketId()}, {"Polymarket"}, kPolymarketId());
        } else {
            command_bar_->set_exchanges(ids, labels, reg.active_id());
        }

        // Reflect any previously-saved credentials in the account chip.
        const bool has_pm = pred::PredictionCredentialStore::load_polymarket().has_value();
        const bool has_ks = pred::PredictionCredentialStore::load_kalshi().has_value();
        QString acct_label;
        if (has_pm && has_ks) acct_label = tr("Polymarket + Kalshi");
        else if (has_pm) acct_label = tr("Polymarket");
        else if (has_ks) acct_label = tr("Kalshi");
        command_bar_->set_account_status(has_pm || has_ks, acct_label);

        // Listen for adapter-side credential / error events across every
        // registered adapter so the status bar + account chip stay in sync
        // regardless of which exchange is active.
        for (const QString& id : reg.available_ids()) {
            if (auto* a = reg.adapter(id)) {
                connect(a, &pred::PredictionExchangeAdapter::credentials_changed, this, [this]() {
                    const bool pm = pred::PredictionCredentialStore::load_polymarket().has_value();
                    const bool ks = pred::PredictionCredentialStore::load_kalshi().has_value();
                    QString l;
                    if (pm && ks) l = tr("Polymarket + Kalshi");
                    else if (pm) l = tr("Polymarket");
                    else if (ks) l = tr("Kalshi");
                    command_bar_->set_account_status(pm || ks, l);
                });
            }
        }

        // Wire the initially-active adapter and pull the first view.
        connect_active_adapter();
        load_current_view();
    }
    if (has_selection_ && !subscribed_asset_ids_.isEmpty()) {
        if (auto* a = active_adapter())
            a->subscribe_market(subscribed_asset_ids_);
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
    leaderboard_->setVisible(false); // shown only when active adapter supports leaderboard.

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

    // Command bar signals
    connect(command_bar_, &PolymarketCommandBar::view_changed, this, &PolymarketScreen::on_view_changed);
    connect(command_bar_, &PolymarketCommandBar::category_changed, this, &PolymarketScreen::on_category_changed);
    connect(command_bar_, &PolymarketCommandBar::search_submitted, this, &PolymarketScreen::on_search_submitted);
    connect(command_bar_, &PolymarketCommandBar::sort_changed, this, &PolymarketScreen::on_sort_changed);
    connect(command_bar_, &PolymarketCommandBar::refresh_clicked, this, &PolymarketScreen::on_refresh);
    connect(command_bar_, &PolymarketCommandBar::exchange_changed, this, &PolymarketScreen::on_exchange_changed);
    connect(command_bar_, &PolymarketCommandBar::account_clicked, this, [this]() {
        auto* dlg = new PredictionAccountDialog(this);
        auto& reg = pred::PredictionExchangeRegistry::instance();
        dlg->set_active_exchange(reg.active_id());
        connect(dlg, &PredictionAccountDialog::credentials_saved, this, [this](const QString& id) {
            auto& reg = pred::PredictionExchangeRegistry::instance();
            if (id == kPolymarketId()) {
                if (auto* pm = dynamic_cast<pred::polymarket_ns::PolymarketAdapter*>(reg.adapter(kPolymarketId())))
                    pm->reload_credentials();
            } else if (id == QStringLiteral("kalshi")) {
                if (auto* ks = dynamic_cast<pred::kalshi_ns::KalshiAdapter*>(reg.adapter(QStringLiteral("kalshi")))) {
                    if (auto creds = pred::PredictionCredentialStore::load_kalshi()) ks->set_credentials(*creds);
                }
            }
            const bool has_pm = pred::PredictionCredentialStore::load_polymarket().has_value();
            const bool has_ks = pred::PredictionCredentialStore::load_kalshi().has_value();
            const bool any = has_pm || has_ks;
            QString label;
            if (has_pm && has_ks) label = tr("Polymarket + Kalshi");
            else if (has_pm) label = tr("Polymarket");
            else if (has_ks) label = tr("Kalshi");
            command_bar_->set_account_status(any, label);
            LOG_INFO("PredictionMarkets", "Credentials saved for " + id);
        });
        connect(dlg, &PredictionAccountDialog::test_requested, this, [](const QString& id) {
            auto* adapter = pred::PredictionExchangeRegistry::instance().adapter(id);
            if (!adapter) return;
            if (id == kPolymarketId()) {
                if (auto* pm = dynamic_cast<pred::polymarket_ns::PolymarketAdapter*>(adapter))
                    pm->reload_credentials();
            } else if (id == QStringLiteral("kalshi")) {
                if (auto* ks = dynamic_cast<pred::kalshi_ns::KalshiAdapter*>(adapter)) {
                    if (auto creds = pred::PredictionCredentialStore::load_kalshi()) ks->set_credentials(*creds);
                }
            }
            adapter->fetch_balance();
        });
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->open();
    });

    // Browse panel signals
    connect(browse_panel_, &PolymarketBrowsePanel::market_selected, this, &PolymarketScreen::on_market_selected);
    connect(browse_panel_, &PolymarketBrowsePanel::event_selected, this, &PolymarketScreen::on_event_selected);

    // Detail panel signals
    connect(detail_panel_, &PolymarketDetailPanel::interval_changed, this, &PolymarketScreen::on_interval_changed);
    connect(detail_panel_, &PolymarketDetailPanel::outcome_changed, this, &PolymarketScreen::on_outcome_changed);
    connect(detail_panel_, &PolymarketDetailPanel::related_market_clicked, this,
            &PolymarketScreen::on_related_market_clicked);
}

// ── Active-adapter wiring ───────────────────────────────────────────────────

pred::PredictionExchangeAdapter* PolymarketScreen::active_adapter() const {
    return pred::PredictionExchangeRegistry::instance().active();
}

bool PolymarketScreen::active_is_polymarket() const {
    return pred::PredictionExchangeRegistry::instance().active_id() == kPolymarketId();
}

void PolymarketScreen::connect_active_adapter() {
    disconnect_active_adapter();

    auto* a = active_adapter();
    if (!a) {
        LOG_WARN("PredictionMarkets", "connect_active_adapter: no active adapter registered");
        return;
    }
    wired_adapter_id_ = a->id();

    // Read-side signals emit prediction::* types — both Polymarket and
    // Kalshi adapters conform, so sub-panels receive identical payloads
    // regardless of the underlying exchange.
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::markets_ready,
                                     this, &PolymarketScreen::on_markets_ready);
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::events_ready,
                                     this, &PolymarketScreen::on_events_ready);
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::search_results_ready,
                                     this, &PolymarketScreen::on_search_results_ready);
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::tags_ready,
                                     this, &PolymarketScreen::on_tags_ready);
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::order_book_ready,
                                     this, &PolymarketScreen::on_order_book_ready);
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::price_history_ready,
                                     this, &PolymarketScreen::on_price_history_ready);
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::recent_trades_ready,
                                     this, &PolymarketScreen::on_trades_ready);
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::leaderboard_ready,
                                     this, &PolymarketScreen::on_leaderboard_ready);
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::error_occurred,
                                     this, &PolymarketScreen::on_adapter_error);

    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::ws_price_updated,
                                     this, &PolymarketScreen::on_ws_price_updated);
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::ws_orderbook_updated,
                                     this, &PolymarketScreen::on_ws_orderbook_updated);
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::ws_connection_changed,
                                     this, &PolymarketScreen::on_ws_connection_changed);

    // Install the per-exchange presentation profile across every sub-panel
    // that has per-exchange visual identity. This fans out accent color,
    // price formatters, view pill set, stat visibility, and status-badge
    // wording in a single call so the whole surface stays internally
    // consistent. Done before `list_tags()` so the command bar's category
    // row is built in the right mode before tags arrive.
    install_presentation(polymarket::ExchangePresentation::for_adapter(a));

    // Leaderboard panel is currently Polymarket-only (Kalshi has no public
    // leaderboard endpoint). Show it only when the adapter advertises it.
    const auto caps = a->capabilities();
    leaderboard_->setVisible(caps.has_leaderboard);

    // Tags are populated per-adapter so the category row reflects the
    // active exchange's taxonomy.
    a->list_tags();

    // WS status resets on adapter swap — the new adapter's connection
    // state takes over.
    on_ws_connection_changed(a->is_ws_connected());
}

void PolymarketScreen::disconnect_active_adapter() {
    for (const auto& c : adapter_connections_) QObject::disconnect(c);
    adapter_connections_.clear();
    wired_adapter_id_.clear();
}

void PolymarketScreen::install_presentation(const polymarket::ExchangePresentation& p) {
    // Fan-out order matters slightly: command bar first so view pills are
    // rebuilt before we reset active_view_; detail panel next so status
    // badge re-renders with the new profile before any data callback lands.
    if (command_bar_) command_bar_->set_presentation(p);
    if (detail_panel_) detail_panel_->set_presentation(p);
    if (browse_panel_) browse_panel_->set_presentation(p);

    // The detail panel's embedded sub-widgets (chart, trades) are built
    // inside the panel and reachable via its own plumbing — set_presentation
    // on the panel is enough. But the panel doesn't expose them, so we
    // reach through here to keep the chart + feed consistent. This is the
    // only cross-widget reach in the refactor; everything else is panel-local.
    // Anchored off named children so we don't bake in a friend relationship.
    if (detail_panel_) {
        if (auto* chart = detail_panel_->findChild<PolymarketPriceChart*>())
            chart->set_presentation(p);
        if (auto* feed = detail_panel_->findChild<PolymarketActivityFeed*>())
            feed->set_presentation(p);
    }

    // If the active view from the previous exchange doesn't exist in the
    // new profile's view list, fall back to its default. Keeps the pill
    // row in a valid state after Polymarket→Kalshi (SPORTS/TRENDING drop).
    if (!p.view_names.contains(active_view_)) {
        active_view_ = p.default_view.isEmpty() ? QStringLiteral("MARKETS") : p.default_view;
        if (command_bar_) command_bar_->set_active_view(active_view_);
    }
}

void PolymarketScreen::connect_polymarket_extras() {
    auto& svc = pmx::PolymarketService::instance();
    polymarket_extras_connections_ << connect(&svc, &pmx::PolymarketService::price_summary_ready,
                                               this, &PolymarketScreen::on_price_summary_received);
    polymarket_extras_connections_ << connect(&svc, &pmx::PolymarketService::top_holders_ready,
                                               this, &PolymarketScreen::on_top_holders_received);
    polymarket_extras_connections_ << connect(&svc, &pmx::PolymarketService::comments_ready,
                                               this, &PolymarketScreen::on_comments_received);
    polymarket_extras_connections_ << connect(&svc, &pmx::PolymarketService::related_markets_ready,
                                               this, &PolymarketScreen::on_related_markets_received);
}

// ── Command Bar Slots ───────────────────────────────────────────────────────

void PolymarketScreen::on_view_changed(const QString& view) {
    active_view_ = view;
    if (command_bar_) command_bar_->set_active_view(view);
    if (status_bar_) status_bar_->set_view(view);
    load_current_view();
    LOG_INFO("PredictionMarkets", "View: " + view);
}

void PolymarketScreen::on_category_changed(const QString& category) {
    active_category_ = category;
    if (command_bar_) command_bar_->set_active_category(category);
    ScreenStateManager::instance().notify_changed(this);
    load_current_view();
}

void PolymarketScreen::on_search_submitted(const QString& query) {
    if (query.isEmpty()) {
        load_current_view();
        return;
    }
    auto* a = active_adapter();
    if (!a) return;
    command_bar_->set_loading(true);
    browse_panel_->set_loading(true);
    ++request_generation_;
    a->search(query, 50);
}

void PolymarketScreen::on_sort_changed(const QString& sort_by) {
    active_sort_ = sort_by;
    if (!first_show_) load_current_view();
}

void PolymarketScreen::on_refresh() { load_current_view(); }

void PolymarketScreen::on_exchange_changed(const QString& exchange_id) {
    auto& reg = pred::PredictionExchangeRegistry::instance();
    if (reg.active_id() == exchange_id) return;

    // Drop the current market subscription before swapping adapters — the
    // old adapter is the one that owns that WS channel.
    unsubscribe_current();
    has_selection_ = false;
    selected_market_ = {};

    reg.set_active(exchange_id);
    LOG_INFO("PredictionMarkets", "Active exchange -> " + exchange_id);

    // Clear the rendered surface so stale markets from the previous
    // exchange don't sit under the new one.
    browse_panel_->clear();
    detail_panel_->clear();
    if (status_bar_) {
        auto* a = reg.adapter(exchange_id);
        status_bar_->set_selected(tr("Showing %1 data")
                                      .arg(a ? a->display_name() : exchange_id));
    }

    connect_active_adapter();
    load_current_view();
}

// ── Browse Panel Slots ──────────────────────────────────────────────────────

void PolymarketScreen::on_market_selected(const pred::PredictionMarket& market) {
    select_market(market);
}

void PolymarketScreen::on_event_selected(const pred::PredictionEvent& event) {
    if (!event.markets.isEmpty()) select_market(event.markets.first());
    // Polymarket-only: event-level related markets.
    if (active_is_polymarket()) {
        bool ok = false;
        const int event_id = event.key.event_id.toInt(&ok);
        if (ok && event_id > 0) pmx::PolymarketService::instance().fetch_related_markets(event_id);
    }
}

// ── Detail Panel Slots ──────────────────────────────────────────────────────

void PolymarketScreen::on_interval_changed(const QString& interval) {
    if (!has_selection_ || selected_market_.outcomes.isEmpty()) return;
    auto* a = active_adapter();
    if (!a) return;

    int fidelity = 5;
    if (interval == "1h" || interval == "6h") fidelity = 1;
    else if (interval == "1w") fidelity = 30;
    else if (interval == "1m" || interval == "max") fidelity = 60;

    a->fetch_price_history(selected_market_.outcomes.first().asset_id, interval, fidelity);
}

void PolymarketScreen::on_outcome_changed(int index) {
    if (!has_selection_ || index < 0 || index >= selected_market_.outcomes.size()) return;
    if (auto* a = active_adapter())
        a->fetch_price_history(selected_market_.outcomes[index].asset_id, "1d", 5);
}

void PolymarketScreen::on_related_market_clicked(const pred::PredictionMarket& market) {
    select_market(market);
}

// ── Data Loading ────────────────────────────────────────────────────────────

void PolymarketScreen::load_current_view() {
    auto* a = active_adapter();
    if (!a) {
        LOG_WARN("PredictionMarkets", "load_current_view: no active adapter");
        return;
    }
    command_bar_->set_loading(true);
    browse_panel_->set_loading(true);
    ++request_generation_;

    const QString category = (active_category_ == "ALL") ? QString() : active_category_;

    if (active_view_ == "TRENDING") {
        a->list_markets(QString(), "volume", 100, 0);
    } else if (active_view_ == "MARKETS") {
        a->list_markets(category, active_sort_, 100, 0);
    } else if (active_view_ == "EVENTS") {
        a->list_events(category, active_sort_, 100, 0);
    } else if (active_view_ == "SPORTS") {
        a->list_markets(QStringLiteral("sports"), active_sort_, 100, 0);
    } else if (active_view_ == "RESOLVED") {
        // The adapter interface doesn't expose a closed-markets filter today —
        // fall back to the default list. Kalshi exposes this via event
        // status; that improvement can land with the API fix pass.
        a->list_events(category, QStringLiteral("endDate"), 100, 0);
    } else {
        a->list_markets(category, active_sort_, 100, 0);
    }
}

void PolymarketScreen::select_market(const pred::PredictionMarket& market) {
    unsubscribe_current();
    selected_market_ = market;
    has_selection_ = true;

    detail_panel_->set_market(market);
    if (status_bar_) status_bar_->set_selected(market.question);

    auto* a = active_adapter();
    if (!a) return;

    if (!market.outcomes.isEmpty()) {
        const QString primary = market.outcomes.first().asset_id;
        a->fetch_order_book(primary);
        a->fetch_price_history(primary, "1d", 5);
    }
    a->fetch_recent_trades(market.key, 100);

    if (active_is_polymarket()) {
        // Polymarket-only enrichments. Kalshi exposes none of these today.
        auto& svc = pmx::PolymarketService::instance();
        if (!market.outcomes.isEmpty())
            svc.fetch_price_summary(market.outcomes.first().asset_id);
        const QString condition_id = market.key.market_id; // Polymarket: condition_id
        if (!condition_id.isEmpty()) {
            svc.fetch_top_holders(condition_id, 20);
            svc.fetch_open_interest({condition_id});
        }
        const QString slug = market.extras.value("slug").toString();
        if (!slug.isEmpty()) svc.fetch_comments(slug, 50);
        bool ok = false;
        const int event_id = market.key.event_id.toInt(&ok);
        if (ok && event_id > 0) svc.fetch_related_markets(event_id);
    }

    subscribe_to_market(market);
}

void PolymarketScreen::subscribe_to_market(const pred::PredictionMarket& market) {
    subscribed_asset_ids_.clear();
    subscribed_asset_ids_.reserve(market.outcomes.size());
    for (const auto& o : market.outcomes) {
        if (!o.asset_id.isEmpty()) subscribed_asset_ids_.push_back(o.asset_id);
    }
    if (subscribed_asset_ids_.isEmpty()) return;
    if (auto* a = active_adapter())
        a->subscribe_market(subscribed_asset_ids_);
}

void PolymarketScreen::unsubscribe_current() {
    if (subscribed_asset_ids_.isEmpty()) return;
    if (auto* a = active_adapter())
        a->unsubscribe_market(subscribed_asset_ids_);
    subscribed_asset_ids_.clear();
}

// ── Adapter response handlers ───────────────────────────────────────────────

void PolymarketScreen::on_markets_ready(const QVector<pred::PredictionMarket>& markets) {
    command_bar_->set_loading(false);
    browse_panel_->set_markets(markets);
    command_bar_->set_market_count(markets.size());
    status_bar_->set_count(markets.size(), "markets");
}

void PolymarketScreen::on_events_ready(const QVector<pred::PredictionEvent>& events) {
    command_bar_->set_loading(false);
    browse_panel_->set_events(events);
    command_bar_->set_market_count(events.size());
    status_bar_->set_count(events.size(), "events");
}

void PolymarketScreen::on_search_results_ready(const QVector<pred::PredictionMarket>& markets,
                                               const QVector<pred::PredictionEvent>& events) {
    command_bar_->set_loading(false);
    if (!markets.isEmpty()) {
        browse_panel_->set_markets(markets);
        command_bar_->set_market_count(markets.size());
        status_bar_->set_count(markets.size(), "markets");
    } else {
        browse_panel_->set_events(events);
        command_bar_->set_market_count(events.size());
        status_bar_->set_count(events.size(), "events");
    }
}

void PolymarketScreen::on_tags_ready(const QStringList& tags) {
    command_bar_->set_categories(tags);
}

void PolymarketScreen::on_order_book_ready(const pred::PredictionOrderBook& book) {
    if (detail_panel_) detail_panel_->set_order_book(book);
}

void PolymarketScreen::on_price_history_ready(const pred::PriceHistory& history) {
    if (detail_panel_) detail_panel_->set_price_history(history);
}

void PolymarketScreen::on_trades_ready(const QVector<pred::PredictionTrade>& trades) {
    if (detail_panel_) detail_panel_->set_trades(trades);
}

void PolymarketScreen::on_leaderboard_ready(const QVariantList& /*entries*/) {
    // Leaderboard shape is exchange-specific and not modeled in prediction::
    // today. The legacy Polymarket leaderboard panel takes services::polymarket::
    // types, so when the active adapter is Polymarket we fetch through the
    // PolymarketService directly. Adapter-emitted QVariantList leaderboards
    // are ignored here until the panel is retyped.
}

void PolymarketScreen::on_adapter_error(const QString& ctx, const QString& msg) {
    command_bar_->set_loading(false);
    LOG_WARN("PredictionMarkets", ctx + ": " + msg);
    if (status_bar_) status_bar_->set_selected(QString("%1: %2").arg(ctx, msg));
}

// ── WebSocket handlers ──────────────────────────────────────────────────────

void PolymarketScreen::on_ws_price_updated(const QString& asset_id, double price) {
    if (!detail_panel_ || !has_selection_) return;
    for (int i = 0; i < selected_market_.outcomes.size(); ++i) {
        if (selected_market_.outcomes[i].asset_id == asset_id) {
            selected_market_.outcomes[i].price = price;
            detail_panel_->set_market(selected_market_);
            return;
        }
    }
}

void PolymarketScreen::on_ws_orderbook_updated(const QString& /*asset_id*/, const pred::PredictionOrderBook& book) {
    if (detail_panel_) detail_panel_->set_order_book(book);
}

void PolymarketScreen::on_ws_connection_changed(bool connected) {
    command_bar_->set_ws_status(connected);
    status_bar_->set_ws_status(connected);
}

// ── Polymarket-only enrichment handlers ─────────────────────────────────────

void PolymarketScreen::on_price_summary_received(const pmx::PriceSummary& summary) {
    if (!active_is_polymarket() || !detail_panel_) return;
    detail_panel_->set_price_summary(summary);
}

void PolymarketScreen::on_top_holders_received(const QVector<pmx::TopHolder>& holders) {
    if (!active_is_polymarket() || !detail_panel_) return;
    detail_panel_->set_top_holders(holders);
}

void PolymarketScreen::on_comments_received(const QVector<pmx::Comment>& comments) {
    if (!active_is_polymarket() || !detail_panel_) return;
    detail_panel_->set_comments(comments);
}

void PolymarketScreen::on_related_markets_received(const QVector<pmx::Market>& markets) {
    if (!active_is_polymarket() || !detail_panel_) return;
    // Legacy polymarket::Market → prediction::PredictionMarket shim. The
    // detail panel only cares about question + volume for the related-market
    // cards, so we fill in the minimum.
    QVector<pred::PredictionMarket> out;
    out.reserve(markets.size());
    for (const auto& m : markets) {
        pred::PredictionMarket pm;
        pm.key.exchange_id = kPolymarketId();
        pm.key.market_id = m.condition_id;
        pm.question = m.question;
        pm.volume = m.volume;
        pm.liquidity = m.liquidity;
        pm.active = m.active;
        pm.closed = m.closed;
        for (const auto& o : m.outcomes) {
            pred::Outcome po;
            po.name = o.name;
            po.price = o.price;
            pm.outcomes.push_back(po);
        }
        // Map clob_token_ids into outcomes so the user can click through
        // and drive select_market() with a full asset_id list.
        for (int i = 0; i < m.clob_token_ids.size() && i < pm.outcomes.size(); ++i)
            pm.outcomes[i].asset_id = m.clob_token_ids[i];
        pm.extras["slug"] = m.slug;
        pm.key.event_id = QString::number(m.event_id);
        out.push_back(pm);
    }
    detail_panel_->set_related_markets(out);
}

// ── IStatefulScreen ─────────────────────────────────────────────────────────

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
