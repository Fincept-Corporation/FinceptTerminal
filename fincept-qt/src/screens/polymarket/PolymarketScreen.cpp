#include "screens/polymarket/PolymarketScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/polymarket/PolymarketActivityFeed.h"
#include "screens/polymarket/PolymarketBrowsePanel.h"
#include "screens/polymarket/PolymarketCommandBar.h"
#include "screens/polymarket/PolymarketDetailPanel.h"
#include "screens/polymarket/PolymarketLeaderboard.h"
#include "screens/polymarket/PolymarketOrderBlotter.h"
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

#include <cmath>

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

    const QString splitter_css =
        QString("QSplitter::handle { background: %1; }").arg(ui::colors::BORDER_MED());

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet(splitter_css);

    // Browse panel: slightly wider for readability, fixed range so it
    // never squeezes so small cards become unreadable.
    browse_panel_ = new PolymarketBrowsePanel;
    browse_panel_->setMinimumWidth(300);
    browse_panel_->setMaximumWidth(460);

    detail_panel_ = new PolymarketDetailPanel;

    // Order blotter: collapsed by default, expands when account is connected.
    order_blotter_ = new PolymarketOrderBlotter;
    order_blotter_->setMinimumHeight(110);
    order_blotter_->setVisible(false);

    auto* detail_splitter = new QSplitter(Qt::Vertical);
    detail_splitter->setHandleWidth(1);
    detail_splitter->setStyleSheet(splitter_css);
    detail_splitter->addWidget(detail_panel_);
    detail_splitter->addWidget(order_blotter_);
    detail_splitter->setStretchFactor(0, 3);
    detail_splitter->setStretchFactor(1, 1);
    detail_splitter->setSizes({700, 160});

    leaderboard_ = new PolymarketLeaderboard;
    leaderboard_->setMinimumWidth(260);
    leaderboard_->setMaximumWidth(320);
    leaderboard_->setVisible(false);

    splitter->addWidget(browse_panel_);
    splitter->addWidget(detail_splitter);
    splitter->addWidget(leaderboard_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({380, 800, 0});

    root->addWidget(splitter, 1);

    // Blotter signals — dispatch through the active adapter, which is
    // cast to the Kalshi-specific type for amend/batch-cancel operations.
    connect(order_blotter_, &PolymarketOrderBlotter::refresh_order,
            this, &PolymarketScreen::on_order_refresh_requested);
    connect(order_blotter_, &PolymarketOrderBlotter::amend_order,
            this, &PolymarketScreen::on_order_amend_requested);
    connect(order_blotter_, &PolymarketOrderBlotter::cancel_order,
            this, &PolymarketScreen::on_order_cancel_requested);
    connect(order_blotter_, &PolymarketOrderBlotter::cancel_all,
            this, &PolymarketScreen::on_orders_cancel_all_requested);

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
    connect(detail_panel_, &PolymarketDetailPanel::place_order, this,
            [this](const pred::OrderRequest& req) {
                if (auto* a = active_adapter()) a->place_order(req);
            });
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

    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::open_orders_ready,
                                     this, &PolymarketScreen::on_open_orders_ready);
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::order_cancelled, this,
                                     [a](const QString& oid, bool ok, const QString& err) {
                                         if (!ok) {
                                             LOG_WARN("PredictionMarkets", "Cancel failed: " + err);
                                             return;
                                         }
                                         // Refresh the full list after cancel confirms.
                                         a->fetch_open_orders();
                                         Q_UNUSED(oid);
                                     });
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::balance_ready,
                                     this, [this](const pred::AccountBalance& b) {
                                         if (detail_panel_) detail_panel_->set_balance(b);
                                     });
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::positions_ready,
                                     this, [this](const QVector<pred::PredictionPosition>& p) {
                                         if (detail_panel_) detail_panel_->set_positions(p);
                                     });
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::order_placed,
                                     this, [this, a](const pred::OrderResult& r) {
                                         if (detail_panel_) detail_panel_->on_order_result(r);
                                         if (r.ok) {
                                             a->fetch_balance();
                                             a->fetch_open_orders();
                                         }
                                     });
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::ws_price_updated,
                                     this, &PolymarketScreen::on_ws_price_updated);
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::ws_orderbook_updated,
                                     this, &PolymarketScreen::on_ws_orderbook_updated);
    adapter_connections_ << connect(a, &pred::PredictionExchangeAdapter::ws_connection_changed,
                                     this, &PolymarketScreen::on_ws_connection_changed);

    // Kalshi-specific extensions — surfaced via KalshiAdapter's own signals
    // (exchange status, WS trade/lifecycle, batch candles, market detail
    // from lifecycle refreshes). Cast + connect only when the active
    // adapter is Kalshi. Auto-disconnects on ~QObject / adapter destroy.
    if (auto* ks = dynamic_cast<pred::kalshi_ns::KalshiAdapter*>(a)) {
        adapter_connections_ << connect(ks, &pred::kalshi_ns::KalshiAdapter::exchange_status_ready,
                                         this, &PolymarketScreen::on_kalshi_exchange_status);
        adapter_connections_ << connect(ks, &pred::kalshi_ns::KalshiAdapter::exchange_schedule_ready,
                                         this, &PolymarketScreen::on_kalshi_exchange_schedule);
        adapter_connections_ << connect(ks, &pred::kalshi_ns::KalshiAdapter::ws_trade_received,
                                         this, &PolymarketScreen::on_kalshi_ws_trade);
        adapter_connections_ << connect(ks, &pred::kalshi_ns::KalshiAdapter::ws_market_lifecycle_changed,
                                         this, &PolymarketScreen::on_kalshi_market_lifecycle);
        adapter_connections_ << connect(ks, &pred::kalshi_ns::KalshiAdapter::batch_candles_ready,
                                         this, &PolymarketScreen::on_kalshi_batch_candles);
        adapter_connections_ << connect(ks, &pred::kalshi_ns::KalshiAdapter::historical_markets_ready,
                                         this, [this](const QVector<pred::PredictionMarket>& markets,
                                                      const QString& /*cursor*/) {
                                             on_markets_ready(markets);
                                         });
        adapter_connections_ << connect(ks, &pred::kalshi_ns::KalshiAdapter::market_detail_ready,
                                         this, &PolymarketScreen::on_kalshi_market_detail);
        adapter_connections_ << connect(ks, &pred::kalshi_ns::KalshiAdapter::series_detail_ready,
                                         this, &PolymarketScreen::on_kalshi_series_detail);
        adapter_connections_ << connect(ks, &pred::kalshi_ns::KalshiAdapter::single_order_ready,
                                         this, &PolymarketScreen::on_kalshi_single_order);
        adapter_connections_ << connect(ks, &pred::kalshi_ns::KalshiAdapter::order_amended, this,
                                         [ks](const QString& oid, bool ok, const QString& err) {
                                             if (!ok) {
                                                 LOG_WARN("PredictionMarkets",
                                                          "Amend failed: " + err);
                                                 return;
                                             }
                                             ks->fetch_order(oid);
                                         });
        adapter_connections_ << connect(ks, &pred::kalshi_ns::KalshiAdapter::orders_batch_cancelled,
                                         this, [ks](const QStringList&, bool ok,
                                                    const QString& err) {
                                             if (!ok) LOG_WARN("PredictionMarkets",
                                                               "Batch cancel failed: " + err);
                                             ks->fetch_open_orders();
                                         });
        // Kick off the exchange status + schedule fetch; results land in the
        // status bar. These are cheap endpoints with long TTL on Kalshi's
        // side, so one call per adapter swap is fine.
        ks->fetch_exchange_status();
        ks->fetch_exchange_schedule();
    }

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
    if (status_bar_) {
        status_bar_->set_brand(p.display_name, p.accent);
        // Exchange status is only meaningful for exchanges that expose a
        // status endpoint. Clear it on every swap so stale text from a
        // previous exchange doesn't leak.
        status_bar_->set_exchange_status({});
        status_bar_->set_next_session({});
    }

    // Order blotter + trade ticket: both gated on having credentials.
    {
        auto* a = active_adapter();
        const bool has_creds = a && a->has_credentials();
        if (detail_panel_) detail_panel_->set_trading_enabled(has_creds);
        if (has_creds && a) {
            a->fetch_balance();
            a->fetch_positions();
        }
    }

    if (order_blotter_) {
        auto* a = active_adapter();
        const bool has_creds = a && a->has_credentials();
        order_blotter_->setVisible(has_creds);
        if (a) {
            const auto caps = a->capabilities();
            order_blotter_->set_capabilities(caps.supports_decrease_order,  // amend ≈ Kalshi-only
                                             caps.supports_batch_orders);
            order_blotter_->set_presentation(p);
            if (has_creds) a->fetch_open_orders();
            else order_blotter_->clear();
        }
    }

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

    // Clear search state and per-adapter caches so the new exchange starts clean.
    command_bar_->set_search_text(QString());
    batch_candles_cache_.clear();

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
    } else if (active_view_ == "RESOLVED" || active_view_ == "SETTLED") {
        // The adapter interface doesn't expose a closed-markets filter today —
        // fall back to the default list. Kalshi exposes this via event
        // status; that improvement can land with the API fix pass.
        a->list_events(category, QStringLiteral("endDate"), 100, 0);
    } else if (active_view_ == "HISTORY") {
        // Kalshi-only: archived markets served by /historical/markets. No
        // Polymarket equivalent, so the pill is hidden for Polymarket via
        // the ExchangePresentation view_names list.
        if (auto* ks = dynamic_cast<pred::kalshi_ns::KalshiAdapter*>(a)) {
            ks->fetch_historical_markets(category, 100, QString());
        } else {
            a->list_markets(category, active_sort_, 100, 0);
        }
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

    // Refresh positions for the newly-selected market (only if credentialed).
    if (a->has_credentials()) a->fetch_positions();

    // Kalshi: fetch series detail for fee-info tooltip. Cached in the
    // adapter so repeat selections of markets in the same series don't
    // refetch. Triggers on_kalshi_series_detail.
    if (auto* ks = dynamic_cast<pred::kalshi_ns::KalshiAdapter*>(a)) {
        const QString series = market.extras.value(QStringLiteral("series_ticker")).toString();
        if (!series.isEmpty()) ks->fetch_series_detail(series);
        else if (detail_panel_) detail_panel_->set_series_tooltip({});
    }

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
        if (!condition_id.isEmpty()) svc.fetch_comments(condition_id, 50);
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
    browse_panel_->set_loading(false);
    browse_panel_->set_markets(markets);
    command_bar_->set_market_count(markets.size());
    status_bar_->set_count(markets.size(), "markets");

    // Kalshi-only: fire a single /markets/candlesticks call covering all
    // visible tickers. The server returns ~7 daily points per ticker in
    // one round-trip, which populates sparklines without N per-card fetches.
    // Replaces the prior per-row candle-fetch fan-out (~20 HTTP calls per
    // page load in testing).
    if (auto* ks = dynamic_cast<pred::kalshi_ns::KalshiAdapter*>(active_adapter())) {
        QStringList tickers;
        tickers.reserve(markets.size());
        for (const auto& m : markets) {
            const QString& t = m.key.market_id;
            if (!t.isEmpty() && !batch_candles_cache_.contains(t)) tickers.push_back(t);
            if (tickers.size() >= 50) break;  // server caps batch at ~100
        }
        if (!tickers.isEmpty()) {
            const qint64 end = QDateTime::currentSecsSinceEpoch();
            const qint64 start = end - (7 * 24 * 3600);
            ks->fetch_batch_candles(tickers, /*period=*/1440, start, end);
        }
    }
}

void PolymarketScreen::on_events_ready(const QVector<pred::PredictionEvent>& events) {
    command_bar_->set_loading(false);
    browse_panel_->set_loading(false);
    browse_panel_->set_events(events);
    command_bar_->set_market_count(events.size());
    status_bar_->set_count(events.size(), "events");
}

void PolymarketScreen::on_search_results_ready(const QVector<pred::PredictionMarket>& markets,
                                               const QVector<pred::PredictionEvent>& events) {
    command_bar_->set_loading(false);
    browse_panel_->set_loading(false);
    if (!markets.isEmpty()) {
        browse_panel_->set_markets(markets);
        command_bar_->set_market_count(markets.size());
        status_bar_->set_count(markets.size(), "markets");
    } else if (!events.isEmpty()) {
        browse_panel_->set_events(events);
        command_bar_->set_market_count(events.size());
        status_bar_->set_count(events.size(), "events");
    } else {
        command_bar_->set_market_count(0);
        status_bar_->set_count(0, "results");
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
    browse_panel_->set_loading(false);
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

// ── Kalshi-specific handlers ────────────────────────────────────────────────

void PolymarketScreen::on_kalshi_exchange_status(const QJsonObject& status) {
    if (!status_bar_) return;
    // Kalshi responds with { "exchange_active": bool, "trading_active": bool,
    // "exchange_estopped": bool } on /exchange/status. We map this to a
    // user-facing label: PAUSED if estopped, OPEN if both booleans true,
    // CLOSED otherwise.
    const bool exchange_active = status.value("exchange_active").toBool();
    const bool trading_active = status.value("trading_active").toBool();
    const bool estopped = status.value("exchange_estopped").toBool();
    QString label;
    if (estopped) label = QStringLiteral("PAUSED");
    else if (exchange_active && trading_active) label = QStringLiteral("OPEN");
    else if (exchange_active) label = QStringLiteral("MAINT");
    else label = QStringLiteral("CLOSED");
    status_bar_->set_exchange_status(label);
}

void PolymarketScreen::on_kalshi_exchange_schedule(const QJsonObject& schedule) {
    if (!status_bar_) return;
    // Best-effort "next open" label. Kalshi's schedule shape is nested
    // (standard_hours + maintenance_windows). Fall back to an empty string
    // if the shape changes so we don't surface nonsense.
    const auto standard = schedule.value("schedule").toObject()
                              .value("standard_hours").toArray();
    QString next;
    if (!standard.isEmpty()) {
        const auto first = standard.first().toObject();
        const QString open = first.value("open_time").toString();
        if (!open.isEmpty())
            next = QStringLiteral("Next open: ") + open;
    }
    status_bar_->set_next_session(next);
}

void PolymarketScreen::on_kalshi_ws_trade(const pred::PredictionTrade& trade) {
    if (!detail_panel_ || !has_selection_) return;
    // Only surface trades for the currently-selected market. Kalshi's trade
    // channel broadcasts across all subscribed tickers, so we filter here.
    const QString aid_prefix = selected_market_.key.market_id + QStringLiteral(":");
    if (!trade.asset_id.startsWith(aid_prefix)) return;
    if (auto* feed = detail_panel_->findChild<polymarket::PolymarketActivityFeed*>())
        feed->append_trade(trade);
}

void PolymarketScreen::on_kalshi_market_lifecycle(const QString& ticker,
                                                  const QString& status) {
    LOG_INFO("PredictionMarkets",
             QStringLiteral("Kalshi lifecycle: %1 → %2").arg(ticker, status));
    // When a visible market flips status, refresh just that row. Avoid
    // reloading the whole view — a market going paused/closed shouldn't
    // disturb the rest of the browse panel.
    if (!active_adapter()) return;
    // If the currently-selected market just flipped, refresh detail too.
    if (has_selection_ && selected_market_.key.market_id == ticker) {
        pred::MarketKey k;
        k.market_id = ticker;
        active_adapter()->fetch_market(k);
    }
}

void PolymarketScreen::on_kalshi_batch_candles(
    const QHash<QString, pred::PriceHistory>& histories) {
    // Store for sparkline consumers (browse panel cards). The browse panel
    // will re-read from the cache on its next rebuild.
    for (auto it = histories.constBegin(); it != histories.constEnd(); ++it)
        batch_candles_cache_.insert(it.key(), it.value());
    // If the browse panel exposes a set_sparklines() hook, wire it here.
    // Without a hook it's still useful for the detail chart which checks
    // the cache before issuing its own fetch.
}

void PolymarketScreen::on_kalshi_market_detail(const pred::PredictionMarket& market) {
    // Single-row refresh triggered by a lifecycle event. Update the browse
    // panel row in place if possible; otherwise no-op (panel won't show
    // stale data because the full-view refresh will replace it soon).
    if (browse_panel_) browse_panel_->update_market_row(market);
    if (has_selection_ && selected_market_.key.market_id == market.key.market_id) {
        selected_market_ = market;
        if (detail_panel_) detail_panel_->set_market(market);
    }
}

void PolymarketScreen::on_open_orders_ready(const QVector<pred::OpenOrder>& orders) {
    if (order_blotter_) order_blotter_->set_orders(orders);
}

void PolymarketScreen::on_order_refresh_requested(const QString& order_id) {
    // Only Kalshi supports the single-order fetch today. Polymarket has no
    // REST equivalent — the blotter's refresh button is wired for everyone
    // but the cast below makes the no-op explicit on other adapters.
    auto* a = active_adapter();
    if (auto* ks = dynamic_cast<pred::kalshi_ns::KalshiAdapter*>(a)) {
        ks->fetch_order(order_id);
    } else if (a) {
        a->fetch_open_orders();  // fall back to refetching the whole list
    }
}

void PolymarketScreen::on_order_amend_requested(const QString& order_id, const QString& side,
                                                double price) {
    auto* a = active_adapter();
    auto* ks = dynamic_cast<pred::kalshi_ns::KalshiAdapter*>(a);
    if (!ks) {
        LOG_WARN("PredictionMarkets", "Amend not supported on this adapter");
        return;
    }
    // Kalshi accepts integer cents 1-99.
    const int cents = qBound(1, int(std::round(price * 100.0)), 99);
    ks->amend_order(order_id, side, cents);
}

void PolymarketScreen::on_order_cancel_requested(const QString& order_id) {
    if (auto* a = active_adapter()) a->cancel_order(order_id);
}

void PolymarketScreen::on_orders_cancel_all_requested(const QStringList& order_ids) {
    auto* a = active_adapter();
    if (!a) return;
    // Prefer the batched endpoint when the adapter supports it (Kalshi);
    // otherwise fall back to N individual cancels so Polymarket users still
    // get the Cancel All action.
    if (auto* ks = dynamic_cast<pred::kalshi_ns::KalshiAdapter*>(a)) {
        ks->cancel_orders_batch(order_ids);
    } else {
        for (const auto& oid : order_ids) a->cancel_order(oid);
    }
}

void PolymarketScreen::on_kalshi_single_order(const QJsonObject& order) {
    if (!order_blotter_) return;
    // Translate the raw Kalshi order JSON into the unified OpenOrder shape
    // the blotter expects. Field names match the Python bridge's
    // cmd_open_orders normalization so the two paths produce identical
    // rows.
    pred::OpenOrder o;
    o.order_id = order.value("order_id").toString();
    const QString side = order.value("side").toString().toLower();
    o.outcome = side.toUpper();
    o.asset_id = order.value("ticker").toString() + ":" + side;
    o.market_id = order.value("ticker").toString();
    o.side = order.value("action").toString().toUpper();
    o.order_type = order.value("type").toString().toUpper();
    const QString price_key = (side == "yes") ? "yes_price_dollars" : "no_price_dollars";
    o.price = order.value(price_key).toString().toDouble();
    o.size = order.value("remaining_count_fp").toString().toDouble();
    const double initial = order.value("initial_count_fp").toString().toDouble();
    o.filled = qMax(0.0, initial - o.size);
    o.status = order.value("status").toString().toUpper();
    order_blotter_->update_order(o);
}

void PolymarketScreen::on_kalshi_series_detail(const QString& series_ticker,
                                               const QJsonObject& series) {
    if (!detail_panel_ || !has_selection_) return;
    // Only apply if the currently-selected market is in this series.
    const QString selected_series =
        selected_market_.extras.value(QStringLiteral("series_ticker")).toString();
    if (series_ticker != selected_series) return;

    const QString title = series.value(QStringLiteral("title")).toString();
    const QString frequency = series.value(QStringLiteral("frequency")).toString();
    const QString fee_type = series.value(QStringLiteral("fee_type")).toString();
    const double fee_mult = series.value(QStringLiteral("fee_multiplier")).toDouble();
    const QString contract_url = series.value(QStringLiteral("contract_url")).toString();

    QStringList lines;
    if (!title.isEmpty()) lines << QStringLiteral("<b>%1</b>").arg(title.toHtmlEscaped());
    if (!series_ticker.isEmpty())
        lines << QStringLiteral("Series: %1").arg(series_ticker);
    if (!frequency.isEmpty())
        lines << QStringLiteral("Frequency: %1").arg(frequency);
    if (!fee_type.isEmpty()) {
        QString fee_line = QStringLiteral("Fees: %1").arg(fee_type);
        if (fee_mult > 0.0)
            fee_line += QStringLiteral(" (×%1)").arg(fee_mult);
        lines << fee_line;
    }
    if (!contract_url.isEmpty())
        lines << QStringLiteral("<i>%1</i>").arg(contract_url.toHtmlEscaped());

    detail_panel_->set_series_tooltip(lines.join(QStringLiteral("<br>")));
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
