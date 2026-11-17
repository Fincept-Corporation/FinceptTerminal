// Equity Trading Screen — multi-account coordinator.
//
// Core: ctor/dtor, show/hide events, setup_ui (command bar + responsive workspace
// + status bar), shortcuts, setup_timers, connect_data_stream_signals,
// init_focused_account, update_account_menu, update_connection_status,
// on_group_symbol_changed, current_symbol. Split concerns:
//   - EquityTradingScreen_Streams.cpp  — on_stream_* slots from DataStreamManager
//   - EquityTradingScreen_Handlers.cpp — user-action handlers
//   - EquityTradingScreen_Holdings.cpp — broker→portfolio holdings import
//   - EquityWorkspace / EquityCommandBar / EquityStyle — layout, bar, sheet
#include "screens/equity_trading/EquityTradingScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolDragSource.h"
#include "screens/common/feeds/FeedPanel.h"
#include "screens/equity_trading/AccountManagementDialog.h"
#include "screens/equity_trading/BroadcastOrderDialog.h"
#include "screens/equity_trading/EquityBottomPanel.h"
#include "screens/equity_trading/EquityChartPanel.h"
#include "screens/equity_trading/EquityCommandBar.h"
#include "screens/equity_trading/EquityOrderBook.h"
#include "screens/equity_trading/EquityOrderEntry.h"
#include "screens/equity_trading/EquityQuoteSnapshot.h"
#include "screens/equity_trading/EquitySingleInstance.h"
#include "screens/equity_trading/EquityStyle.h"
#include "screens/equity_trading/EquityTickerBar.h"
#include "screens/equity_trading/EquityWatchlist.h"
#include "screens/equity_trading/EquityWorkspace.h"
#include "screens/equity_trading/PortfolioReplicationDialog.h"
#include "screens/portfolio_monitor/PortfolioMonitorScreen.h"
#include "services/feeds/FeedMonitor.h"
#include "services/markets/FxRateService.h"
#include "trading/PaperMarkService.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/repositories/SettingsRepository.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/DataStreamManager.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "trading/UnifiedTrading.h"
#include "trading/instruments/InstrumentService.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDate>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QToolButton>
#include <QHash>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QStringListModel>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <functional>
#include <memory>

namespace fincept::screens {

using namespace fincept::trading;
using namespace fincept::screens::equity;

static const QString TAG = "EquityTrading";

namespace {

// A link-group symbol this screen must NOT adopt. Watchlist / Research / News /
// Markets share link group A with this screen, and several of them publish
// Yahoo-style DATA tickers — indices (^GSPC), FX (EURUSD=X), futures (GC=F),
// crypto (BTC-USD) — or another market's equities. Retargeting the order ticket
// to one of those would pre-fill an order for an instrument the focused broker
// cannot trade. Two checks:
//   1. the ticker form / asset class says it is not a broker equity, and
//   2. when the focused broker's instrument master is loaded, the symbol must
//      exist on one of that broker's exchanges (an unloaded master proves
//      nothing either way, so it does not block — the broker still validates
//      the symbol at order time).
bool eq_link_symbol_untradeable(const SymbolRef& ref, const QString& broker_id, const QStringList& exchanges) {
    const QString sym = ref.symbol.trimmed();
    if (sym.isEmpty() || sym.startsWith(QLatin1Char('^')) || sym.contains(QLatin1Char('=')))
        return true;
    const QString ac = ref.asset_class.trimmed().toLower();
    if (!ac.isEmpty() && ac != QLatin1String("equity") && ac != QLatin1String("stock") &&
        ac != QLatin1String("etf"))
        return true;
    if (broker_id.isEmpty())
        return false;
    auto& svc = trading::InstrumentService::instance();
    if (!svc.is_loaded(broker_id))
        return false;
    const QString upper = sym.toUpper();
    if (!ref.exchange.isEmpty() && svc.find(upper, ref.exchange.toUpper(), broker_id).has_value())
        return false;
    for (const auto& ex : exchanges) {
        if (svc.find(upper, ex, broker_id).has_value())
            return false;
    }
    return true;
}

// A single-letter shortcut must not act while the user is typing.
bool eqts_typing_in_focus() {
    QWidget* f = QApplication::focusWidget();
    if (!f)
        return false;
    if (qobject_cast<QLineEdit*>(f) || qobject_cast<QAbstractSpinBox*>(f) || qobject_cast<QTextEdit*>(f) ||
        qobject_cast<QPlainTextEdit*>(f))
        return true;
    auto* combo = qobject_cast<QComboBox*>(f);
    return combo && combo->isEditable();
}

} // namespace

EquityTradingScreen::EquityTradingScreen(QWidget* parent) : QWidget(parent) {
    // One screen per panel (EquitySingleInstance.h): a second build into the same
    // host is the shell re-entering its factory while the first screen is being
    // shown. That copy stays an inert twin — no UI, streams, subscriptions or ticket.
    if (auto* first = qobject_cast<EquityTradingScreen*>(
            eq_first_live_twin(parent, this, EquityTradingScreen::staticMetaObject.className()))) {
        become_inert_twin(first);
        return;
    }
    watchlist_symbols_ = DEFAULT_WATCHLIST;
    {
        auto r = SettingsRepository::instance().get(QStringLiteral("equity_trading.skip_paper_confirm"));
        skip_paper_confirm_ = r.is_ok() && r.value() == QLatin1String("1");
        const auto s = ScreenStateManager::instance().load_direct("equity_trading");
        builtin_selected_symbol_ = s.value("builtin_symbol").toString();
        pending_splitter_state_ = s;
    }
    setup_ui();
    setup_timers();
    wire_public_feed();
    connect_data_stream_signals();

    // Delayed-price marks / fills of the paper account land here even while the
    // screen only watches (PaperMarkService runs them, screen or not).
    connect(&trading::PaperMarkService::instance(), &trading::PaperMarkService::portfolio_changed, this,
            [this](const QString& pid) {
                if (pid == focused_paper_portfolio_id_ && isVisible())
                    refresh_paper_panels();
            });
    connect(&trading::PaperMarkService::instance(), &trading::PaperMarkService::prices_marked, this,
            [this](const QString& pid) {
                // Marks flush about once a second; repaint the books at most every 3 s.
                if (pid != focused_paper_portfolio_id_ || !isVisible())
                    return;
                const qint64 now = QDateTime::currentMSecsSinceEpoch();
                if (now - last_paper_refresh_ms_ < 3000)
                    return;
                last_paper_refresh_ms_ = now;
                refresh_paper_panels();
            });
    connect(&trading::AccountManager::instance(), &trading::AccountManager::builtin_paper_reset, this,
            [this](const QString&) {
                if (focused_is_builtin()) {
                    focused_account_id_.clear(); // re-focus onto the new portfolio
                    focus_builtin_account();
                }
            });
    // FX legs arrive asynchronously; multi-currency P&L follows them.
    connect(&services::FxRateService::instance(), &services::FxRateService::rates_changed, this, [this]() {
        if (focused_is_builtin() && isVisible())
            refresh_paper_panels();
    });

    // When InstrumentService finishes loading/downloading a broker's instrument
    // master, reload market data for the active account if its broker matches.
    // Account data works without this, but quotes/charts/depth need the numeric
    // securityId map that only exists once instruments are loaded.
    {
        QPointer<EquityTradingScreen> self = this;
        connect(&trading::InstrumentService::instance(), &trading::InstrumentService::refresh_done, this,
                [self](const QString& broker_id, int /*count*/) {
                    if (self)
                        self->on_instruments_ready(broker_id);
                });
        connect(&trading::InstrumentService::instance(), &trading::InstrumentService::refresh_failed, this,
                [](const QString& broker_id, const QString& error) {
                    LOG_WARN(TAG, QString("Instrument load failed for %1: %2").arg(broker_id, error));
                });
    }

    // Accept symbol drops anywhere on the screen — route through the
    // existing selection path so sub-panels resync and the linked group is
    // published to.
    symbol_dnd::installDropFilter(this, [this](const SymbolRef& ref, SymbolGroup) {
        if (!ref.is_valid())
            return;
        if (canonical_mode())
            on_group_symbol_changed(ref); // canonicalises with the ref's exchange
        else
            on_symbol_selected(ref.symbol);
    });
}

EquityTradingScreen::~EquityTradingScreen() {
    if (fill_cb_id_ >= 0)
        OrderMatcher::instance().remove_fill_callback(fill_cb_id_);
}

void EquityTradingScreen::become_inert_twin(EquityTradingScreen* primary) {
    inert_ = true;
    primary_ = primary;
    setProperty(kEqInertTwinProperty, true);
    setObjectName(QStringLiteral("eqScreenTwin"));
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    setMaximumSize(0, 0); // never takes layout space, even before it is hidden
    setFocusPolicy(Qt::NoFocus);
    LOG_WARN(TAG, "A second Equity Trading screen was built into the same panel (the shell re-entered its factory "
                  "while the first one was being shown); this copy stays empty and hidden, the first one serves "
                  "the panel");
    QMetaObject::invokeMethod(this, &QWidget::hide, Qt::QueuedConnection);
}

// ============================================================================
// Visibility — start/stop data streams (P3)
// ============================================================================

void EquityTradingScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (inert_) {
        QMetaObject::invokeMethod(this, &QWidget::hide, Qt::QueuedConnection);
        return;
    }
    // Streams were paused by a real hide (or this is the first show): resume them.
    // A hide + show inside one event-loop turn left them running — see pause_streams().
    const bool resume = !initialized_ || streams_paused_;

    // Always make sure a configured broker is loaded. Re-checked on every show so a
    // broker configured after the tab first opened is picked up automatically,
    // instead of stranding the user on "NO ACCOUNT" while a broker exists in the DB.
    ensure_account_loaded();

    // Restore the rest of the persisted session state on first show.
    if (!initialized_) {
        // The user's own splits (saved on hide, per width class) win over the defaults.
        restore_splitters(pending_splitter_state_);
        pending_splitter_state_.clear();

        const auto s = ScreenStateManager::instance().load_direct("equity_trading");
        if (!s.isEmpty()) {
            const QString sym = s.value("selected_symbol", selected_symbol_).toString();
            if (!sym.isEmpty() && sym != selected_symbol_)
                on_symbol_selected(sym);
            // FEEDS / ALL ACCOUNTS tabs (toggled() builds and shows them).
            if (s.value("feeds_visible").toBool())
                feeds_btn_->setChecked(true);
            if (s.value("unified_visible").toBool())
                unified_btn_->setChecked(true);
        }
    }

    if (resume) {
        streams_paused_ = false;
        // Start all active account data streams
        DataStreamManager::instance().start_all_active();
        DataStreamManager::instance().resume_all();

        // Credentials can change while this screen is hidden (ACCOUNTS dialog elsewhere), which
        // flips an account between the local simulator and a broker paper venue — re-derive
        // that before subscribing so the hub topics match.
        if (!focused_account_id_.isEmpty())
            sync_native_paper();

        // Hub subscriptions for streaming data (D4)
        hub_subscribe_streaming();

        // Pick the market-data source for the focused context (broker stream, or the
        // public fallback when no broker can serve it) and start the public feed if
        // that is the source — it only runs while the screen is visible.
        sync_market_feed();

        // On REOPEN (not first show — init_focused_account handles that), the stream
        // stayed running while hidden, so neither start() nor resume() re-fetches the
        // portfolio. Force an immediate live refresh so Holdings/Positions/Orders show
        // current broker data right away instead of staying stale (or blank) until the
        // 5-min poll. Paper data is repainted by refresh_paper_panels() just below.
        if (initialized_ && !focused_is_paper_ && !focused_native_paper_ && !focused_account_id_.isEmpty())
            DataStreamManager::instance().refresh_portfolio(focused_account_id_);

        // Catch up intraday auto-square for paper portfolios (e.g. the app was closed
        // at 15:30, so yesterday's MIS positions never squared). Refresh picks up the
        // resulting state; it no-ops for live accounts.
        trading::pt_settle_intraday_all();
        refresh_paper_panels();

        fincept::feeds::FeedMonitor::instance().resume_all();
    }

    // UI-local timers (cheap; stopped on every hide)
    if (status_bar_)
        status_bar_->start_clock();
    if (market_clock_timer_)
        market_clock_timer_->start();
    if (status_timer_)
        status_timer_->start();

    if (!initialized_) {
        init_focused_account();
        initialized_ = true;
    }
    update_first_run_note();
    update_status_bar();

    LOG_DEBUG(TAG, resume ? "Screen visible — data streams resumed" : "Screen visible — data streams kept running");
}

void EquityTradingScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (inert_)
        return;
    if (status_bar_)
        status_bar_->stop_clock();
    if (market_clock_timer_)
        market_clock_timer_->stop();
    if (status_timer_)
        status_timer_->stop();

    QVariantMap state = {
        {"focused_account_id", focused_account_id_},
        {"selected_symbol", selected_symbol_},
        {"selected_exchange", selected_exchange_},
        {"builtin_symbol", focused_is_builtin() ? selected_symbol_ : builtin_selected_symbol_},
        {"feeds_visible", feeds_btn_ != nullptr && feeds_btn_->isChecked()},
        {"unified_visible", unified_btn_ != nullptr && unified_btn_->isChecked()},
    };
    const QVariantMap splits = splitter_state();
    for (auto it = splits.cbegin(); it != splits.cend(); ++it)
        state.insert(it.key(), it.value());
    ScreenStateManager::instance().save_direct("equity_trading", state);

    // Streams, hub topics and the public feed stop one event-loop turn later, and
    // only if the screen is still hidden then: when Qt re-creates the native window
    // (the chart's first WebEngine view) every widget is hidden and shown again
    // within one turn, and tearing everything down for that was most of the cost
    // of opening the screen.
    QTimer::singleShot(0, this, [this]() {
        if (!isVisible())
            pause_streams();
    });
}

void EquityTradingScreen::pause_streams() {
    if (streams_paused_ || !initialized_)
        return;
    streams_paused_ = true;
    hub_unsubscribe_all();
    // The public fallback holds its own hub subscriptions; nothing polls Yahoo for
    // a hidden screen. sync_market_feed() on the next show restarts it.
    if (public_feed_)
        public_feed_->stop();
    if (feed_grace_timer_)
        feed_grace_timer_->stop();
    feed_grace_expired_ = false;
    DataStreamManager::instance().pause_all();
    fincept::feeds::FeedMonitor::instance().pause_all();
    LOG_DEBUG(TAG, "Screen hidden — data streams paused, hub unsubscribed");
}

// ============================================================================
// UI Setup — command bar · one row of three zones (EquityWorkspace) · status bar
// ============================================================================

void EquityTradingScreen::setup_ui() {
    setObjectName("eqScreen");
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(eq_screen_sheet()); // once, before any panel exists

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    setup_command_bar();
    main_layout->addWidget(cmd_bar_);

    // ── First-run note (built-in paper account; dismissible, persisted) ────────
    {
        auto* note = new QFrame(this);
        note->setObjectName("eqFirstRunNote");
        note->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum); // a strip: never takes body height
        auto* nl = new QHBoxLayout(note);
        nl->setContentsMargins(10, 4, 6, 4);
        nl->setSpacing(8);
        first_run_text_ = new QLabel(note);
        first_run_text_->setObjectName("eqFirstRunText");
        first_run_text_->setWordWrap(true);
        first_run_text_->setText(
            tr("Paper account ready \xE2\x80\x94 search any stock, ETF, index, FX pair, crypto or future above and trade "
               "it from the ticket. Prices are delayed public data (Yahoo Finance, typically 15 min) and every fill "
               "is simulated; nothing reaches a broker. Connect a broker in ACCOUNTS for live prices and real orders."));
        nl->addWidget(first_run_text_, 1);
        auto* acc = new QPushButton(tr("ACCOUNTS"), note);
        acc->setObjectName("eqNoteBtn");
        acc->setCursor(Qt::PointingHandCursor);
        connect(acc, &QPushButton::clicked, this, &EquityTradingScreen::on_accounts_clicked);
        nl->addWidget(acc, 0, Qt::AlignVCenter);
        auto* close = new QToolButton(note);
        close->setObjectName("eqFirstRunClose");
        close->setText(QStringLiteral("\xE2\x9C\x95")); // ✕
        close->setAutoRaise(true);
        close->setCursor(Qt::PointingHandCursor);
        close->setToolTip(tr("Dismiss"));
        close->setAccessibleName(tr("Dismiss the paper account note"));
        connect(close, &QToolButton::clicked, this, [this]() {
            SettingsRepository::instance().set(QStringLiteral("equity_trading.first_run_note_dismissed"),
                                               QStringLiteral("1"), QStringLiteral("ui"));
            update_first_run_note();
        });
        nl->addWidget(close, 0, Qt::AlignTop);
        first_run_note_ = note;
        first_run_note_->hide();
        main_layout->addWidget(first_run_note_);
    }

    // ── BODY: one row of three zones that re-flows with the window ────────────
    // The workspace joins the screen before it is filled, and every panel is built
    // inside its zone host, so no finished tree is ever re-parented (re-parenting
    // re-computes the stylesheet of every widget in it — the old construction cost).
    workspace_ = new EquityWorkspace(this);
    main_layout->addWidget(workspace_, 1);

    // LEFT: watchlist
    watchlist_ = new EquityWatchlist(workspace_->watchlist_host());
    watchlist_->set_symbols(watchlist_symbols_);
    watchlist_->set_active_symbol(selected_symbol_);
    EquityWorkspace::place(workspace_->watchlist_host(), watchlist_, 1);

    // CENTRE: chart over the blotter
    chart_ = new EquityChartPanel(workspace_->chart_host());
    EquityWorkspace::place(workspace_->chart_host(), chart_, 1);
    bottom_panel_ = new EquityBottomPanel(workspace_->blotter_host());
    // Tab bar + action bar + a few rows: the empty-state text is never cut off.
    bottom_panel_->setMinimumHeight(QFontMetrics(ui::fonts::font()).height() * 7);
    EquityWorkspace::place(workspace_->blotter_host(), bottom_panel_, 1);

    // RIGHT: depth ladder (broker) or the delayed-quote snapshot (public), over the ticket
    orderbook_ = new EquityOrderBook(workspace_->quote_host());
    EquityWorkspace::place(workspace_->quote_host(), orderbook_);
    // Without broker depth the ladder would be an empty box: a compact snapshot of
    // the delayed public data takes its place (update_right_column()).
    snapshot_ = new EquityQuoteSnapshot(workspace_->quote_host());
    snapshot_->hide();
    EquityWorkspace::place(workspace_->quote_host(), snapshot_);
    order_entry_ = new EquityOrderEntry(workspace_->ticket_host());
    order_entry_->setMinimumHeight(QFontMetrics(ui::fonts::font()).height() * 12);
    EquityWorkspace::place(workspace_->ticket_host(), order_entry_, 1);

    // Default widths: the watchlist's longest ticker beside LTP / CHG%, the ticket's own floor.
    workspace_->set_watchlist_preferred_width(watchlist_->content_width());
    workspace_->set_right_minimum_width(order_entry_->minimumSizeHint().width());
    connect(workspace_, &EquityWorkspace::width_class_changed, this, [this](EqWidthClass) { apply_width_plan(); });
    connect(workspace_, &EquityWorkspace::watchlist_collapsed_changed, this, [this](bool folded) {
        cmd_bar_->actions()->set_action_checked(QStringLiteral("watchlist"), !folded);
    });

    setup_status_bar();
    main_layout->addWidget(status_bar_);

    // ── Signal Connections ────────────────────────────────────────────────────
    connect(mode_btn_, &QPushButton::clicked, this, &EquityTradingScreen::on_mode_toggled);
    connect(feeds_btn_, &QPushButton::toggled, this, [this](bool on) {
        if (on) {
            auto* panel = ensure_feed_panel();
            bottom_panel_->set_extra_tab_visible(panel, true);
            bottom_panel_->show_extra_tab(panel);
        } else if (feed_panel_) {
            bottom_panel_->set_extra_tab_visible(feed_panel_, false);
        }
    });
    connect(unified_btn_, &QPushButton::toggled, this, [this](bool on) {
        if (monitor_float_) {
            // Detached: ON raises the floating window; OFF closes it (the
            // close re-docks the panel hidden — see redock_monitor()).
            if (on) {
                monitor_float_->raise();
                monitor_float_->activateWindow();
            } else {
                monitor_float_->close();
            }
            return;
        }
        if (on) {
            auto* monitor = ensure_monitor_panel(); // showEvent → service activate + refresh
            bottom_panel_->set_extra_tab_visible(monitor, true);
            bottom_panel_->show_extra_tab(monitor);
        } else if (monitor_panel_) {
            bottom_panel_->set_extra_tab_visible(monitor_panel_, false);
        }
    });
    connect(watchlist_, &EquityWatchlist::symbol_selected, this, &EquityTradingScreen::on_symbol_selected);
    connect(watchlist_, &EquityWatchlist::symbol_added, this, &EquityTradingScreen::on_watchlist_symbol_added);
    connect(watchlist_, &EquityWatchlist::instrument_added, this, &EquityTradingScreen::on_watchlist_instrument_added);
    connect(watchlist_, &EquityWatchlist::add_unresolved, this, [this](const QString&, const QString& message) {
        order_entry_->show_status_message(message, false);
    });
    connect(watchlist_, &EquityWatchlist::trade_requested, this, [this](const QString& sym, bool is_buy) {
        if (sym != selected_symbol_)
            on_symbol_selected(sym);
        workspace_->reveal_ticket();
        open_order_ticket_for(sym, exchange_for_symbol(sym), canonical_mode() ? QStringLiteral("CNC") : QString(), is_buy);
    });
    connect(watchlist_, &EquityWatchlist::research_requested, this, &EquityTradingScreen::on_watchlist_research);
    connect(watchlist_, &EquityWatchlist::news_requested, this, &EquityTradingScreen::on_watchlist_news);
    connect(watchlist_, &EquityWatchlist::alert_requested, this, &EquityTradingScreen::on_watchlist_alert);
    connect(watchlist_, &EquityWatchlist::symbol_removed, this, &EquityTradingScreen::on_watchlist_symbol_removed);
    connect(watchlist_, &EquityWatchlist::watchlist_selected, this, &EquityTradingScreen::on_watchlist_selected);
    connect(watchlist_, &EquityWatchlist::watchlist_create_requested, this, &EquityTradingScreen::on_watchlist_create);
    connect(watchlist_, &EquityWatchlist::watchlist_rename_requested, this, &EquityTradingScreen::on_watchlist_rename);
    connect(watchlist_, &EquityWatchlist::watchlist_delete_requested, this, &EquityTradingScreen::on_watchlist_delete);
    connect(order_entry_, &EquityOrderEntry::order_submitted, this, &EquityTradingScreen::on_order_submitted);
    connect(order_entry_, &EquityOrderEntry::multi_broker_submit, this, &EquityTradingScreen::on_multi_broker_submit);
    connect(order_entry_, &EquityOrderEntry::strategy_order_submitted, this,
            &EquityTradingScreen::on_strategy_submitted);
    connect(order_entry_, &EquityOrderEntry::broadcast_requested, this, [this](const trading::UnifiedOrder& order) {
        // SL / TP cannot be honoured uniformly across accounts (most brokers would drop
        // them on the entry order; a background paper account has no tick-driven bracket)
        // — refuse rather than broadcast orders the trader believes are protected.
        if (order.stop_loss > 0.0 || order.take_profit > 0.0) {
            order_entry_->show_status_message(tr("Stop-loss / take-profit are not supported on broadcast orders — "
                                                 "nothing was sent. Clear SL/TP or place the order per account."),
                                              false);
            return;
        }
        auto* dlg = new BroadcastOrderDialog(order, this);
        // Repaint the touched accounts' blotters as soon as the broadcast lands (it used
        // to leave Positions / Orders / Funds stale until the 5-minute poll).
        connect(dlg, &BroadcastOrderDialog::broadcast_completed, this,
                [this](const QVector<trading::UnifiedTrading::BroadcastResult>& results) {
                    QStringList ids;
                    for (const auto& r : results)
                        ids << r.account_id;
                    refresh_after_bulk_orders(ids);
                });
        dlg->exec();
        dlg->deleteLater();
    });
    // BASKETS dialog closed — baskets may have run on any account: repaint the blotter.
    connect(order_entry_, &EquityOrderEntry::baskets_closed, this, [this]() { refresh_after_bulk_orders({}); });
    connect(orderbook_, &EquityOrderBook::price_clicked, this, &EquityTradingScreen::on_ob_price_clicked);
    connect(bottom_panel_, &EquityBottomPanel::cancel_order_requested, this, &EquityTradingScreen::on_cancel_order);
    connect(bottom_panel_, &EquityBottomPanel::modify_order_requested, this, &EquityTradingScreen::async_modify_order);
    connect(bottom_panel_, &EquityBottomPanel::cancel_all_orders_requested, this,
            [this](const QString&) { on_cancel_all_orders(); });
    connect(bottom_panel_, &EquityBottomPanel::close_all_positions_requested, this,
            [this](const QString&) { on_close_all_positions(); });
    connect(bottom_panel_, &EquityBottomPanel::square_off_all_holdings_requested, this,
            &EquityTradingScreen::on_square_off_all_holdings);
    connect(bottom_panel_, &EquityBottomPanel::square_off_holding_requested, this,
            &EquityTradingScreen::on_square_off_holding);
    connect(bottom_panel_, &EquityBottomPanel::import_holdings_requested, this,
            &EquityTradingScreen::on_import_holdings_requested);
    connect(bottom_panel_, &EquityBottomPanel::replicate_portfolio_requested, this,
            &EquityTradingScreen::on_replicate_portfolio_requested);
    connect(bottom_panel_, &EquityBottomPanel::convert_position_requested, this,
            &EquityTradingScreen::on_convert_position);
    connect(bottom_panel_, &EquityBottomPanel::orders_day_changed, this, &EquityTradingScreen::on_orders_day_changed);
    connect(bottom_panel_, &EquityBottomPanel::square_off_group_requested, this,
            &EquityTradingScreen::on_square_off_group);
    connect(bottom_panel_, &EquityBottomPanel::trade_symbol_requested, this,
            [this](const QString& symbol, const QString& exchange, const QString& product, bool is_buy, double qty) {
                workspace_->reveal_ticket();
                on_trade_symbol_requested(symbol, exchange, product, is_buy, qty);
            });
    // Click a position/holding row → load that symbol on the chart (same slot the
    // watchlist uses, so it sets the selected symbol, fetches candles + orderbook).
    connect(bottom_panel_, &EquityBottomPanel::chart_symbol_requested, this, &EquityTradingScreen::on_symbol_selected);
    // History for the new timeframe from whichever source serves the screen.
    connect(chart_, &EquityChartPanel::timeframe_changed, this, [this](const QString&) { request_chart_history(); });
    connect(order_entry_, &EquityOrderEntry::accounts_requested, this, &EquityTradingScreen::on_accounts_clicked);
    connect(chart_, &EquityChartPanel::buy_requested, this, &EquityTradingScreen::on_chart_buy_requested);
    connect(chart_, &EquityChartPanel::sell_requested, this, &EquityTradingScreen::on_chart_sell_requested);
    connect(chart_, &EquityChartPanel::add_to_watchlist_requested, this,
            &EquityTradingScreen::on_chart_add_to_watchlist);
    connect(chart_, &EquityChartPanel::exit_position_requested, this, &EquityTradingScreen::on_chart_exit_position);

    install_shortcuts();
}

void EquityTradingScreen::setup_command_bar() {
    cmd_bar_ = new EquityCommandBar(this);

    // Account button + menu
    account_btn_ = cmd_bar_->account_button();
    account_btn_->set_label(tr("NO ACCOUNT"));
    account_menu_ = new QMenu(account_btn_);
    account_menu_->setObjectName(QStringLiteral("eqAccountMenu"));
    account_menu_->setStyleSheet(ui::ScreenCommandBar::menu_css());
    account_btn_->setMenu(account_menu_);
    account_btn_->setAccessibleName(tr("Focused account"));

    // Symbol search: any instrument, any market (shared picker via the factory).
    symbol_box_ = make_symbol_box(SymbolBoxRole::Selector, cmd_bar_);
    symbol_box_->setObjectName("eqSymbolInput");
    auto fit_symbol_box = [this]() {
        // Wide enough for "RELIANCE · NSE" at any font size; never the bar's stretch.
        const QFontMetrics fm(ui::fonts::font(-1, QFont::Bold));
        symbol_box_->setMinimumWidth(std::max(130, fm.horizontalAdvance(QStringLiteral("XXXXXXXXXXX")) + 24));
        symbol_box_->setMaximumWidth(std::max(240, fm.horizontalAdvance(QStringLiteral("XXXXXXXXXXXXXXXXXX")) + 24));
    };
    fit_symbol_box();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, symbol_box_, fit_symbol_box);
    connect(symbol_box_, &SymbolBox::instrument_picked, this, &EquityTradingScreen::on_symbol_picked);
    connect(symbol_box_, &SymbolBox::unresolved, this, [this](const QStringList&, const QString& message) {
        order_entry_->show_status_message(message, false);
    });
    symbol_box_->setAccessibleName(tr("Symbol search"));
    symbol_box_->setToolTip(tr("Any symbol, name or ISIN in any market (/ or Ctrl+Shift+F to focus)"));
    cmd_bar_->set_symbol_widget(symbol_box_);

    // Instrument line (venue · name · currency): shrinks to the venue, then to an ellipsis.
    exchange_label_ = cmd_bar_->instrument_label();
    ticker_bar_ = cmd_bar_->ribbon();

    // Mode chip (PAPER / LIVE; per-account toggle for broker accounts)
    mode_btn_ = cmd_bar_->mode_button();
    mode_btn_->set_label(tr("PAPER"));
    mode_btn_->setProperty("mode", "paper");
    mode_btn_->setProperty("fincept_agent_risk", "account"); // paper <-> live switch
    mode_btn_->setCheckable(true);
    mode_btn_->setAccessibleName(tr("Paper or live trading mode"));

    // Data-source / connection state ("◐ Delayed · Yahoo" / "● Live · <broker>").
    conn_label_ = cmd_bar_->connection_label();
    set_conn_status(tr("○ NO ACCOUNTS"), QString(), QString());
    conn_label_->installEventFilter(this); // on public data, a click opens ACCOUNTS
    conn_label_->setAccessibleName(tr("Broker connection status"));

    // Actions — the shared bar's own buttons: key caps, collapse into "⋯".
    using Role = ui::ScreenCommandBar::Role;
    auto* acts = cmd_bar_->actions();
    acts->set_overflow_text(tr("More"));
    acts->add_action(QStringLiteral("buy"), tr("BUY"), QStringLiteral("B"),
                     tr("Buy the selected symbol: the order ticket on the buy side (B)"), Role::Normal, 100);
    acts->add_action(QStringLiteral("sell"), tr("SELL"), QStringLiteral("S"),
                     tr("Sell the selected symbol: the order ticket on the sell side (S)"), Role::Normal, 100);
    acts->add_action(QStringLiteral("ticket"), tr("TICKET"), QStringLiteral("T"),
                     tr("Order ticket: show it and focus the quantity (T)"), Role::Normal, 95);
    acts->add_separator(92);
    accounts_btn_ = acts->add_action(QStringLiteral("accounts"), tr("ACCOUNTS"), QStringLiteral("A"),
                                     tr("Manage broker accounts (A)"), Role::Normal, 90);
    watchlist_btn_ = acts->add_toggle(QStringLiteral("watchlist"), tr("WATCHLIST"), true, QStringLiteral("W"),
                                      tr("Show or fold the watchlist (W)"), 60);
    unified_btn_ = acts->add_toggle(QStringLiteral("unified"), tr("ALL ACCOUNTS"), false, QStringLiteral("U"),
                                    tr("All-accounts positions & holdings, every connected broker (U)"), 55);
    feeds_btn_ = acts->add_toggle(QStringLiteral("feeds"), tr("FEEDS"), false, QStringLiteral("F"),
                                  tr("Feed monitor (F)"), 50);
    cmd_bar_->reserve_actions({QStringLiteral("buy"), QStringLiteral("sell")});
    connect(acts, &ui::ScreenCommandBar::action_triggered, this, &EquityTradingScreen::on_bar_action);
    connect(acts, &ui::ScreenCommandBar::action_toggled, this, [this](const QString& id, bool on) {
        if (id == QLatin1String("watchlist"))
            workspace_->set_watchlist_collapsed(!on);
        // feeds / unified: their buttons' own toggled() connections act.
    });
}

void EquityTradingScreen::setup_status_bar() {
    using SB = ui::ScreenStatusBar;
    status_bar_ = new ui::ScreenStatusBar(this);
    status_bar_->add_item(QStringLiteral("source"), SB::Segment::Left, QString(), SB::Tone::Info, 100);
    status_bar_->add_item(QStringLiteral("fresh"), SB::Segment::Left, QString(), SB::Tone::Dim, 50);
    status_bar_->add_item(QStringLiteral("session"), SB::Segment::Centre, QString(), SB::Tone::Normal, 70);
    status_bar_->add_item(QStringLiteral("account"), SB::Segment::Right, QString(), SB::Tone::Normal, 95);
    status_bar_->add_item(QStringLiteral("funds"), SB::Segment::Right, QString(), SB::Tone::Normal, 80);
    status_bar_->add_item(QStringLiteral("pnl"), SB::Segment::Right, QString(), SB::Tone::Normal, 85);
    status_bar_->set_clock(true, /*seconds=*/true);
}

void EquityTradingScreen::install_shortcuts() {
    // Single letters act only with the focus inside this screen and never while a
    // text input has it (the shell's key routing keeps typing keys with inputs too).
    // No reserved shell key (F5, F9–F11, Ctrl+1..9) is bound here.
    auto add = [this](const QKeySequence& key, std::function<void()> fn) {
        auto* sc = new QShortcut(key, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, [fn = std::move(fn)]() {
            if (!eqts_typing_in_focus())
                fn();
        });
    };
    add(QKeySequence(Qt::Key_B), [this]() { open_ticket_side(true); });
    add(QKeySequence(Qt::Key_S), [this]() { open_ticket_side(false); });
    add(QKeySequence(Qt::Key_T), [this]() { on_bar_action(QStringLiteral("ticket")); });
    add(QKeySequence(Qt::Key_A), [this]() { on_accounts_clicked(); });
    add(QKeySequence(Qt::Key_W), [this]() { watchlist_btn_->toggle(); });
    add(QKeySequence(Qt::Key_U), [this]() { unified_btn_->toggle(); });
    add(QKeySequence(Qt::Key_F), [this]() { feeds_btn_->toggle(); });
    add(QKeySequence(Qt::Key_Slash), [this]() { symbol_box_->focus_and_select(); });

    auto* focus_sym = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), this);
    focus_sym->setContext(Qt::WindowShortcut);
    connect(focus_sym, &QShortcut::activated, this, [this]() { symbol_box_->focus_and_select(); });

    // CANCEL ALL is destructive, so the shortcut carries its own confirmation
    // (the blotter button has one; a bare hotkey must not be the exception).
    auto* cancel_all = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K), this);
    cancel_all->setContext(Qt::WindowShortcut);
    connect(cancel_all, &QShortcut::activated, this, [this]() {
        if (focused_account_id_.isEmpty())
            return;
        if (QMessageBox::warning(this, tr("Cancel All Orders"), tr("Cancel ALL pending orders on this account?"),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
            on_cancel_all_orders();
    });
}

void EquityTradingScreen::on_bar_action(const QString& id) {
    if (id == QLatin1String("buy")) {
        open_ticket_side(true);
    } else if (id == QLatin1String("sell")) {
        open_ticket_side(false);
    } else if (id == QLatin1String("ticket")) {
        workspace_->reveal_ticket();
        order_entry_->focus_quantity();
    } else if (id == QLatin1String("accounts")) {
        on_accounts_clicked();
    }
}

void EquityTradingScreen::open_ticket_side(bool is_buy) {
    // Never places anything: the ticket takes the side and the quantity gets the focus.
    workspace_->reveal_ticket();
    order_entry_->focus_side(is_buy);
}

void EquityTradingScreen::apply_width_plan() {
    const EqLayoutPlan& plan = workspace_->plan();
    orderbook_->set_visible_levels(plan.depth_levels);
    cmd_bar_->actions()->set_action_checked(QStringLiteral("watchlist"), !workspace_->watchlist_collapsed());
    LOG_DEBUG(TAG, QString("Layout width class: %1").arg(eq_width_class_key(plan.cls)));
}

fincept::feeds::FeedPanel* EquityTradingScreen::ensure_feed_panel() {
    if (!feed_panel_) {
        feed_panel_ = new fincept::feeds::FeedPanel(bottom_panel_);
        bottom_panel_->add_extra_tab(feed_panel_, tr("FEEDS"));
    }
    return feed_panel_;
}

PortfolioMonitorScreen* EquityTradingScreen::ensure_monitor_panel() {
    if (!monitor_panel_) {
        // Aggregates positions/holdings across every connected INR broker — see
        // screens/portfolio_monitor. Built on first use only.
        monitor_panel_ = new PortfolioMonitorScreen(bottom_panel_);
        bottom_panel_->add_extra_tab(monitor_panel_, tr("ALL ACCOUNTS"));
        connect(monitor_panel_, &PortfolioMonitorScreen::float_requested, this, &EquityTradingScreen::float_monitor);
        connect(monitor_panel_, &PortfolioMonitorScreen::dock_requested, this, [this]() {
            if (monitor_float_)
                monitor_float_->close();
        }); // close → redock
    }
    return monitor_panel_;
}

// ============================================================================
// Timers (only UI-local timers remain)
// ============================================================================

void EquityTradingScreen::setup_timers() {
    // Market clock — poll every 60s via focused account's stream
    market_clock_timer_ = new QTimer(this);
    connect(market_clock_timer_, &QTimer::timeout, this, [this]() {
        auto* stream = DataStreamManager::instance().stream_for(focused_account_id_);
        if (stream && focused_is_us_market_)
            stream->fetch_clock(); // US-only market clock (Alpaca calendar tab)
        // Paper intraday auto-square at 15:30 IST. Cheap no-op until the cutoff;
        // refreshes the panels only when something was actually squared off.
        if (trading::pt_settle_intraday_all() > 0)
            refresh_paper_panels();
    });
    market_clock_timer_->setInterval(60000);

    // Status bar: data freshness ("updated 12 s ago") and the session line tick.
    // Interval only; runs while visible (showEvent / hideEvent).
    status_timer_ = new QTimer(this);
    status_timer_->setInterval(5000);
    connect(status_timer_, &QTimer::timeout, this, [this]() {
        update_status_bar();
        update_session_lines();
    });

    // Grace period before a dropped broker session hands the screen to the public
    // feed (see sync_market_feed). Single-shot; armed only on a disconnect.
    feed_grace_timer_ = new QTimer(this);
    feed_grace_timer_->setSingleShot(true);
    feed_grace_timer_->setInterval(15000);
    connect(feed_grace_timer_, &QTimer::timeout, this, [this]() {
        feed_grace_expired_ = true;
        sync_market_feed();
    });

    // Deferred init — auto-select the configured broker if none is focused yet.
    // Routes through the single credential-aware resolver so it can't pick an
    // unconfigured (no-credentials) broker that merely sorts first in the map.
    QTimer::singleShot(100, this, [this]() {
        ensure_account_loaded();
        update_account_menu();
        sync_market_feed(); // also refreshes the connection label
    });
}

void EquityTradingScreen::set_instrument_line(const QString& full, const QString& compact) {
    exchange_label_->set_status_text(full, compact);
    exchange_label_->setToolTip(full);
}

void EquityTradingScreen::set_conn_status(const QString& full, const QString& compact, const QString& tone) {
    conn_label_->set_status_text(full, compact);
    if (tone != conn_tone_) {
        conn_tone_ = tone;
        conn_label_->setProperty("tone", tone);
        conn_label_->style()->unpolish(conn_label_);
        conn_label_->style()->polish(conn_label_);
    }
}

void EquityTradingScreen::changeEvent(QEvent* event) {
    if (!inert_ && event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquityTradingScreen::retranslateUi() {
    auto* acts = cmd_bar_->actions();
    acts->set_action_text(QStringLiteral("buy"), tr("BUY"));
    acts->set_action_text(QStringLiteral("sell"), tr("SELL"));
    acts->set_action_text(QStringLiteral("ticket"), tr("TICKET"));
    acts->set_action_text(QStringLiteral("accounts"), tr("ACCOUNTS"));
    acts->set_action_text(QStringLiteral("watchlist"), tr("WATCHLIST"));
    acts->set_action_text(QStringLiteral("unified"), tr("ALL ACCOUNTS"));
    acts->set_action_text(QStringLiteral("feeds"), tr("FEEDS"));
    acts->set_overflow_text(tr("More"));

    // Mode button reflects the focused account's live/paper state.
    if (mode_btn_)
        mode_btn_->set_label(mode_btn_->isChecked() ? tr("LIVE") : tr("PAPER"));

    // Account button + aggregate connection status are data-driven; re-running
    // their builders re-applies the translated chrome (and the "NO ACCOUNT" /
    // "NO ACCOUNTS" placeholders) while preserving the live state.
    update_account_menu();
    update_connection_status();
    update_feed_notices(); // ticket banner, depth + blotter empty states
    update_status_bar();
    // exchange_label_ holds a market code (NSE/BSE/…), the symbol box the ticker —
    // data, not translated.
}

// ============================================================================
// DataStreamManager signal connections
// ============================================================================

void EquityTradingScreen::ensure_instruments_loaded(const QString& account_id) {
    if (account_id.isEmpty() || trading::is_builtin_paper_account(account_id))
        return; // the built-in paper account has no broker instrument master
    const auto creds = AccountManager::instance().load_credentials(account_id);
    const QString broker_id = creds.broker_id;
    if (broker_id.isEmpty())
        return;

    auto& svc = trading::InstrumentService::instance();
    if (svc.is_loaded(broker_id)) {
        // Already in memory — make sure market data reflects it.
        on_instruments_ready(broker_id);
        return;
    }

    QPointer<EquityTradingScreen> self = this;
    svc.load_from_db_async(broker_id, [self, broker_id, creds](int count) {
        if (!self)
            return;
        if (count > 0) {
            // Cache hit: instruments now in memory → reload market data.
            self->on_instruments_ready(broker_id);
            return;
        }
        // No cache on disk — download from the broker. refresh() emits
        // refresh_done on completion, which routes to on_instruments_ready().
        LOG_DEBUG(TAG, QString("Downloading %1 instruments from broker...").arg(broker_id));
        trading::InstrumentService::instance().refresh(broker_id, creds);
    });
}

void EquityTradingScreen::on_instruments_ready(const QString& broker_id) {
    // Only act if the currently-focused account uses this broker.
    if (focused_account_id_.isEmpty() || focused_is_builtin())
        return;
    const QString focused_broker = AccountManager::instance().load_credentials(focused_account_id_).broker_id;
    if (focused_broker != broker_id)
        return;

    LOG_INFO(TAG, QString("Instruments ready for %1 — reloading market data").arg(broker_id));

    // Rebuild the watchlist so its completer/validation pick up the new cache,
    // then re-issue the data fetches for the active symbol + watchlist via the
    // stream (the same path on_account_changed uses on connect).
    watchlist_->set_symbols(watchlist_symbols_);

    auto* stream = DataStreamManager::instance().stream_for(focused_account_id_);
    if (stream) {
        stream->set_selected_symbol(selected_symbol_, selected_exchange_);
        stream->subscribe_symbols(QStringLiteral("equity:watchlist"), stream_symbols());
        // Market-data fetches only while the broker is the screen's source; on the
        // public fallback they would race (and wipe) the public chart.
        if (!public_mode_) {
            request_chart_history();
            stream->fetch_orderbook(selected_symbol_);
            stream->fetch_time_sales(selected_symbol_);
        }
    }

    // Re-subscribe hub topics so streaming quotes flow for the resolved symbols.
    if (isVisible())
        hub_subscribe_streaming();
}

void EquityTradingScreen::connect_data_stream_signals() {
    auto& dsm = DataStreamManager::instance();
    // Streaming data (quotes, positions, holdings, orders, funds) now comes
    // via DataHub subscriptions — see hub_subscribe_streaming().
    // Only on-demand / one-shot signals remain wired here. Market data uses the
    // symbol(+timeframe)-tagged signals: the untagged legacy ones could paint a
    // slow answer for the previous symbol/timeframe under the current one.
    connect(&dsm, &DataStreamManager::candles_for_symbol, this, &EquityTradingScreen::on_stream_candles_for_symbol);
    connect(&dsm, &DataStreamManager::candles_failed, this, &EquityTradingScreen::on_stream_candles_failed);
    connect(&dsm, &DataStreamManager::orderbook_for_symbol, this,
            &EquityTradingScreen::on_stream_orderbook_for_symbol);
    connect(&dsm, &DataStreamManager::orderbook_failed, this, &EquityTradingScreen::on_stream_orderbook_failed);
    connect(&dsm, &DataStreamManager::time_sales_for_symbol, this,
            &EquityTradingScreen::on_stream_time_sales_for_symbol);
    connect(&dsm, &DataStreamManager::time_sales_failed, this, &EquityTradingScreen::on_stream_time_sales_failed);
    connect(&dsm, &DataStreamManager::latest_trade_for_symbol, this,
            &EquityTradingScreen::on_stream_latest_trade_for_symbol);
    connect(&dsm, &DataStreamManager::calendar_fetched, this, &EquityTradingScreen::on_stream_calendar_fetched);
    connect(&dsm, &DataStreamManager::clock_fetched, this, &EquityTradingScreen::on_stream_clock_fetched);
    // US brokers (Alpaca): AUCTIONS tab rows + the TIME & SALES condition-code legend.
    // Fetchers, signals and panel setters existed; nothing connected them.
    connect(&dsm, &DataStreamManager::auctions_fetched, this,
            [this](const QString& account_id, const QString& symbol, const QVector<trading::BrokerAuction>& auctions) {
                if (is_broker_symbol_target(account_id, symbol))
                    bottom_panel_->set_auctions(auctions);
            });
    connect(&dsm, &DataStreamManager::condition_codes_fetched, this,
            [this](const QString& account_id, const QMap<QString, QString>& codes) {
                if (account_id == focused_account_id_)
                    bottom_panel_->set_condition_codes(codes);
            });
    connect(&dsm, &DataStreamManager::connection_state_changed, this,
            [this](const QString& /*account_id*/, ConnectionState /*state*/) {
                update_connection_status();
                schedule_market_feed_sync(); // a dropped / restored session may switch the source
            });
    connect(&dsm, &DataStreamManager::token_expired, this, &EquityTradingScreen::handle_token_expired);

    // Everything that decides whether the focused account can feed this screen —
    // connection state (session sweep, token expiry, the ACCOUNTS dialog),
    // credentials, accounts coming and going, trading mode — re-runs the source
    // decision. Coalesced and queued: these arrive in bursts (one per account per
    // sweep) and often from inside handlers that are still mutating state.
    auto& am = AccountManager::instance();
    connect(&am, &AccountManager::connection_state_changed, this,
            [this](const QString&, ConnectionState) { schedule_market_feed_sync(); });
    connect(&am, &AccountManager::credentials_changed, this, [this](const QString& account_id) {
        schedule_market_feed_sync();
        // A connect that completes after the ACCOUNTS dialog was closed never delivers the
        // dialog's credentials_saved (it is emitted only while the dialog still exists), so
        // the instrument master / stream / focus never started. Run the same start-up here.
        // Queued: save_credentials() fires this before the connect path marks the account
        // Connected. Only for a Connected account that is focused (or when nothing is), and
        // not a second time when the open dialog already delivered credentials_saved.
        QTimer::singleShot(0, this, [this, account_id]() {
            if (creds_saved_handled_.remove(account_id))
                return; // the dialog path already ran on_credentials_saved for this connect
            const auto acct = AccountManager::instance().get_account(account_id);
            if (acct.account_id.isEmpty() || acct.state != ConnectionState::Connected)
                return;
            // A broker that just connected while the built-in paper account is in
            // view takes the focus, as it would with nothing focused.
            if (!focused_account_id_.isEmpty() && !focused_is_builtin() && account_id != focused_account_id_)
                return;
            on_credentials_saved(account_id);
        });
    });
    connect(&am, &AccountManager::account_added, this, [this](const QString&) { schedule_market_feed_sync(); });
    connect(&am, &AccountManager::account_removed, this, [this](const QString&) { schedule_market_feed_sync(); });
    connect(&am, &AccountManager::account_updated, this, [this](const QString&) { schedule_market_feed_sync(); });
}

// ============================================================================
// Init
// ============================================================================

void EquityTradingScreen::ensure_account_loaded() {
    auto& am = AccountManager::instance();

    // Respect the current focus when it's a live session, or — once the user has
    // started interacting (post-init) — any valid account, so manual broker switches
    // are never undone. The smart auto-pick only happens at startup / when stranded.
    if (focused_is_builtin() && initialized_)
        return; // the user's (or the default) built-in paper account stays
    const bool focus_valid = !focused_account_id_.isEmpty() && am.has_account(focused_account_id_);
    if (focus_valid) {
        const bool live = am.get_account(focused_account_id_).state == ConnectionState::Connected;
        if (live || initialized_)
            return;
    }

    const QString saved =
        ScreenStateManager::instance().load_direct("equity_trading").value("focused_account_id").toString();
    // The built-in paper account was the last one in use: keep it.
    if (trading::is_builtin_paper_account(saved) && !focused_is_builtin()) {
        focus_builtin_account();
        return;
    }

    // Rank every account so the *connected* broker wins over one whose token is
    // expired or that was merely added. Tiers (×2 so the saved tie-breaker can only
    // reorder within a tier, never across):
    //   Connected (live session)            → 4   ← what the user means by "configured"
    //   Connecting (token, unknown expiry)  → 3
    //   has credentials (api_key) but dead  → 1
    //   added, no credentials               → 0
    auto score = [&am](const BrokerAccount& a) -> int {
        switch (a.state) {
            case ConnectionState::Connected:
                return 4;
            case ConnectionState::Connecting:
                return 3;
            default:
                break; // TokenExpired / Error / Disconnected
        }
        return am.load_credentials(a.account_id).api_key.isEmpty() ? 0 : 1;
    };

    QString best;
    int best_rank = -1;
    for (const auto& a : am.list_accounts()) {
        const int rank = score(a) * 2 + (a.account_id == saved ? 1 : 0);
        if (rank > best_rank) {
            best_rank = rank;
            best = a.account_id;
        }
    }

    // No broker account that can do anything (none, or only accounts never given
    // credentials): the built-in paper account, ready to trade on delayed prices.
    if (best.isEmpty() || best_rank < 2) {
        if (!focused_is_builtin())
            focus_builtin_account();
        return;
    }

    // on_account_changed() configures the UI and (when credentials exist) loads the
    // instrument master + starts the data stream; it is a no-op for a focused match.
    if (best != focused_account_id_)
        on_account_changed(best);
}

void EquityTradingScreen::init_focused_account() {
    if (focused_account_id_.isEmpty())
        return;

    auto account = AccountManager::instance().get_account(focused_account_id_);
    if (account.account_id.isEmpty())
        return;

    if (focused_is_builtin()) {
        // No broker, no stream, no instrument master: the paper engine is fed by
        // PaperMarkService on delayed public prices. Only the fill callback (panel
        // refresh) is wired here.
        if (fill_cb_id_ < 0) {
            QPointer<EquityTradingScreen> self = this;
            fill_cb_id_ = OrderMatcher::instance().on_order_fill([self](const OrderFillEvent&) {
                if (!self)
                    return;
                QMetaObject::invokeMethod(
                    self,
                    [self]() {
                        if (self)
                            self->refresh_paper_panels();
                    },
                    Qt::QueuedConnection);
            });
        }
        update_account_menu();
        update_connection_status();
        return;
    }

    // Configure UI for this account's broker profile
    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (broker) {
        const auto prof = broker->profile();
        order_entry_->configure_for_broker(prof);
        order_entry_->set_broker_id(account.broker_id);
        order_entry_->set_account_id(focused_account_id_); // margin line reads THIS account's credentials
        watchlist_->set_broker_id(account.broker_id);
    }

    // Load the broker instrument master (numeric securityId map) for the
    // restored/active account so quotes, charts and depth resolve on app
    // restart / account switch. Loads from the SQLite cache when present,
    // else downloads; on_instruments_ready() reloads market data when done.
    ensure_instruments_loaded(focused_account_id_);

    // Register fill callback for paper trading
    if (fill_cb_id_ < 0) {
        QPointer<EquityTradingScreen> self = this;
        fill_cb_id_ = OrderMatcher::instance().on_order_fill([self](const OrderFillEvent& ev) {
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, ev]() {
                    if (!self)
                        return;
                    // The fill is audited by the paper engine (Order filled).
                    LOG_DEBUG(TAG, QString("Paper order filled: %1 %2 @ %3")
                                       .arg(ev.side, ev.symbol)
                                       .arg(ev.fill_price, 0, 'f', 2));
                    // Refresh paper panels for the focused account (positions,
                    // orders, trades, stats, funds) — single source of truth.
                    self->refresh_paper_panels();
                },
                Qt::QueuedConnection);
        });
    }

    // Load pending paper orders into matcher
    if (!account.paper_portfolio_id.isEmpty()) {
        auto pending = pt_get_orders(account.paper_portfolio_id, "pending");
        for (const auto& o : pending) {
            if (o.order_type != "market")
                OrderMatcher::instance().add_order(o);
        }
    }

    // Start the data stream for focused account
    auto& dsm = DataStreamManager::instance();
    dsm.start_stream(focused_account_id_);
    auto* stream = dsm.stream_for(focused_account_id_);
    if (stream) {
        stream->set_selected_symbol(selected_symbol_, selected_exchange_);
        stream->subscribe_symbols(QStringLiteral("equity:watchlist"), stream_symbols());
        // Chart / depth / tape only while the broker serves the screen; the public
        // fallback (sync_market_feed) owns them otherwise.
        if (!public_mode_) {
            request_chart_history();
            stream->fetch_orderbook(selected_symbol_);
            stream->fetch_time_sales(selected_symbol_);
        }
        stream->fetch_calendar();
        stream->fetch_clock();
    }

    update_account_menu();
    update_connection_status();
    LOG_DEBUG(TAG, QString("Initialized focused account: %1").arg(focused_account_id_));
}

// ============================================================================
// Account menu and status
// ============================================================================

void EquityTradingScreen::update_account_menu() {
    account_menu_->clear();
    const auto accounts = AccountManager::instance().active_accounts();

    // The built-in paper account is always first: it needs no broker.
    {
        const auto builtin = AccountManager::instance().builtin_paper_account();
        auto* action = account_menu_->addAction(tr("%1  \xC2\xB7  built-in, no broker").arg(builtin.display_name), this,
                                                [this]() { on_account_changed(QString::fromLatin1(kBuiltinPaperAccountId)); });
        action->setCheckable(true);
        action->setChecked(focused_is_builtin());
        action->setToolTip(tr("Simulated trading on delayed public prices — no order ever reaches a broker"));
    }
    if (!accounts.isEmpty())
        account_menu_->addSeparator();
    for (const auto& acct : accounts) {
        auto* broker = BrokerRegistry::instance().get(acct.broker_id);
        const QString broker_name = broker ? broker->profile().display_name : acct.broker_id;
        const QString mode = acct.trading_mode == QLatin1String("live") ? tr("LIVE") : tr("PAPER");
        const QString text = QString("%1 [%2] \xC2\xB7 %3").arg(acct.display_name, broker_name, mode);
        auto* action = account_menu_->addAction(text, this, [this, id = acct.account_id]() { on_account_changed(id); });
        action->setCheckable(true);
        action->setChecked(acct.account_id == focused_account_id_);
    }
    account_menu_->addSeparator();
    account_menu_->addAction(tr("Paper account settings…"), this, &EquityTradingScreen::open_paper_settings);
    account_menu_->addAction(tr("Manage broker accounts…"), this, &EquityTradingScreen::on_accounts_clicked);

    // Button text: the focused account
    auto focused = AccountManager::instance().get_account(focused_account_id_);
    account_btn_->set_label(focused.account_id.isEmpty() ? tr("NO ACCOUNT") : focused.display_name.toUpper());
    account_btn_->setToolTip(focused_is_builtin()
                                 ? tr("Built-in paper account: simulated fills on delayed public prices")
                                 : tr("Focused account — orders from this screen go here"));
}

void EquityTradingScreen::update_connection_status() {
    const auto accounts = AccountManager::instance().active_accounts();
    int connected = 0;
    int expired = 0;
    int errored = 0;
    QString first_error;
    for (const auto& a : accounts) {
        if (a.state == ConnectionState::Connected)
            ++connected;
        else if (a.state == ConnectionState::TokenExpired)
            ++expired;
        else if (a.state == ConnectionState::Error) {
            ++errored;
            if (first_error.isEmpty())
                first_error = a.error_message;
        }
    }
    // A stored credential that exists but could not be read (secure storage busy,
    // key unavailable) is not "not connected": say so, focused account first.
    QString cred_read_error =
        focused_is_builtin() ? QString() : AccountManager::instance().credential_read_error(focused_account_id_);
    for (const auto& a : accounts) {
        if (!cred_read_error.isEmpty())
            break;
        cred_read_error = AccountManager::instance().credential_read_error(a.account_id);
    }
    // Default: no hover detail. Only the Error branch attaches the broker's reason
    // (e.g. "no active Kite Connect subscription") as a tooltip.
    conn_label_->setToolTip(QString());

    // Public fallback: say what is on screen (delayed Yahoo data) and why, and
    // point at ACCOUNTS — the label itself opens it (see eventFilter).
    if (public_mode_) {
        using Status = EquityPublicFeed::BrokerStatus;
        // Full text when the bar has room, the compact form when it does not (the
        // label picks; see EquityFitLabel). The tooltip carries the whole story.
        QString text;
        QString compact;
        bool urgent = false;
        if (feed_state_.status == Status::BuiltinPaper) {
            // The normal state of the built-in paper account: say what the data is.
            int delay = -1;
            const auto d = quote_detail_.constFind(selected_symbol_);
            if (d != quote_detail_.constEnd())
                delay = d->exchange_delay_min;
            text = delay > 0 ? tr("◐ DELAYED %1 MIN · YAHOO").arg(delay)
                             : delay == 0 ? tr("◐ PUBLIC · YAHOO") : tr("◐ DELAYED · YAHOO");
            compact = tr("◐ DELAYED");
            set_conn_status(text, compact, QStringLiteral("info"));
            conn_label_->setToolTip(tr("Prices, ranges, volume and charts are public market data from Yahoo Finance, "
                                       "typically delayed 15 minutes (crypto and FX are usually near real time). "
                                       "Paper fills are simulated on these prices.\n\nConnect a broker in ACCOUNTS for "
                                       "live prices, market depth and real orders. Click here to open ACCOUNTS."));
            conn_label_->setCursor(Qt::PointingHandCursor);
            return;
        }
        switch (feed_state_.status) {
            case Status::TokenExpired:
                text = tr("⚠ TOKEN EXPIRED — DELAYED YAHOO DATA");
                compact = tr("⚠ EXPIRED · YAHOO");
                urgent = true;
                break;
            case Status::NoMarketData:
                text = tr("⚠ NO BROKER FEED — DELAYED YAHOO DATA");
                compact = tr("⚠ NO FEED · YAHOO");
                urgent = true;
                break;
            case Status::Disconnected:
                text = tr("◐ BROKER OFFLINE — DELAYED YAHOO DATA");
                compact = tr("◐ OFFLINE · YAHOO");
                break;
            case Status::NoCredentials:
                if (!cred_read_error.isEmpty()) {
                    text = tr("⚠ STORED CREDENTIAL UNREADABLE — DELAYED YAHOO DATA");
                    compact = tr("⚠ CREDENTIAL · YAHOO");
                    urgent = true;
                    break;
                }
                text = tr("◐ BROKER NOT CONNECTED — DELAYED YAHOO DATA");
                compact = tr("◐ NO BROKER · YAHOO");
                break;
            default:
                text = tr("◐ DELAYED YAHOO DATA — CONNECT A BROKER FOR LIVE");
                compact = tr("◐ DELAYED · YAHOO");
                break;
        }
        set_conn_status(text, compact, urgent ? QStringLiteral("danger") : QStringLiteral("warn"));
        QString tip = tr("Prices, day range, volume and charts on this screen are delayed public market data from "
                         "Yahoo Finance: %1.\n\nConnect a broker in ACCOUNTS for live prices, market depth and order "
                         "placement. Click here to open ACCOUNTS.")
                          .arg(public_feed_reason());
        if (!accounts.isEmpty())
            tip += tr("\n\nBroker accounts connected: %1 of %2.").arg(connected).arg(accounts.size());
        conn_label_->setToolTip(tip);
        conn_label_->setCursor(Qt::PointingHandCursor);
        return;
    }
    conn_label_->unsetCursor();

    if (accounts.isEmpty()) {
        set_conn_status(tr("○ NO ACCOUNTS"), QString(), QString());
    } else if (expired > 0 && connected == 0) {
        set_conn_status(tr("\xe2\x9a\xa0 TOKEN EXPIRED \xe2\x80\x94 click ACCOUNTS"), tr("\xe2\x9a\xa0 TOKEN EXPIRED"),
                        QStringLiteral("danger"));
        conn_label_->setToolTip(tr("The broker session has expired. Reconnect the account in ACCOUNTS."));
    } else if (!cred_read_error.isEmpty() && connected == 0) {
        // The account has credentials; they could not be read this time.
        set_conn_status(tr("\xe2\x9a\xa0 COULD NOT READ THE STORED CREDENTIAL \xe2\x80\x94 try again"),
                        tr("\xe2\x9a\xa0 CREDENTIAL UNREADABLE"), QStringLiteral("danger"));
        conn_label_->setToolTip(cred_read_error);
    } else if (errored > 0 && connected == 0) {
        // Authenticated but the broker refused live market data (e.g. Kite 403 — no
        // active Kite Connect subscription). Account data may still load; only the
        // live feed is gated. Full reason on hover.
        set_conn_status(tr("\xe2\x9a\xa0 NO MARKET DATA \xe2\x80\x94 hover for details"), tr("\xe2\x9a\xa0 NO MARKET DATA"),
                        QStringLiteral("danger"));
        conn_label_->setToolTip(first_error);
    } else if (connected == accounts.size()) {
        set_conn_status(tr("● %1/%2 CONNECTED").arg(connected).arg(accounts.size()), QString(), QStringLiteral("ok"));
    } else if (connected > 0) {
        set_conn_status(tr("◐ %1/%2 CONNECTED").arg(connected).arg(accounts.size()), QString(), QStringLiteral("warn"));
    } else {
        set_conn_status(tr("○ 0/%1 CONNECTED").arg(accounts.size()), QString(), QString());
    }
}

// ============================================================================
// DataStream signal handlers
// ============================================================================

void EquityTradingScreen::set_group(SymbolGroup g) {
    if (inert_) {
        if (primary_)
            primary_->set_group(g);
        return;
    }
    link_group_ = g;
}

SymbolGroup EquityTradingScreen::group() const {
    if (inert_)
        return primary_ ? primary_->group() : SymbolGroup::None;
    return link_group_;
}

void EquityTradingScreen::on_group_symbol_changed(const SymbolRef& ref) {
    if (inert_) {
        if (primary_)
            primary_->on_group_symbol_changed(ref);
        return;
    }
    if (!ref.is_valid() || ref.symbol == selected_symbol_)
        return;
    if (canonical_mode()) {
        // Paper account: any instrument with public data — the linked symbol is
        // already a Yahoo-form ticker (or canonicalises with its exchange).
        const auto inst = eq_instrument_for_stored(ref.symbol, ref.exchange);
        if (inst.symbol.isEmpty()) {
            LOG_INFO(TAG, QString("Ignoring linked symbol %1 — no public market data").arg(ref.symbol));
            return;
        }
        instruments_.insert(inst.symbol, inst);
        on_symbol_selected(inst.symbol);
        return;
    }
    // Never retarget the order ticket to something the focused broker can't
    // trade (index / FX / future / crypto data tickers, other-market equities).
    QString broker_id;
    QStringList exchanges;
    if (!focused_account_id_.isEmpty()) {
        broker_id = AccountManager::instance().get_account(focused_account_id_).broker_id;
        if (auto* broker = BrokerRegistry::instance().get(broker_id))
            exchanges = broker->profile().exchanges;
    }
    if (eq_link_symbol_untradeable(ref, broker_id, exchanges)) {
        LOG_INFO(TAG, QString("Ignoring linked symbol %1 (%2) — not tradable on the focused broker %3")
                          .arg(ref.symbol, ref.asset_class.isEmpty() ? QStringLiteral("?") : ref.asset_class,
                               broker_id.isEmpty() ? QStringLiteral("(none)") : broker_id));
        return;
    }
    // Route through the existing selection path so sub-panels sync.
    on_symbol_selected(ref.symbol);
}

SymbolRef EquityTradingScreen::current_symbol() const {
    if (inert_)
        return primary_ ? primary_->current_symbol() : SymbolRef{};
    if (selected_symbol_.isEmpty())
        return {};
    if (canonical_mode()) {
        // A canonical (Yahoo-form) symbol: linked panels quote it as it is.
        const auto inst = instrument_of(selected_symbol_);
        SymbolRef ref = SymbolRef::equity(selected_symbol_, inst.venue);
        const QString cls = fincept::core::instrument::asset_class_slug(inst.asset_class);
        if (!cls.isEmpty())
            ref.asset_class = cls;
        return ref;
    }
    // The exchange this symbol is actually known to trade on. selected_exchange_ is
    // the focused broker's default market ("NSE" before any account), not a fact
    // about a symbol adopted from the link group: AAPL opened in Equity Research
    // came back out to the group — and the status bar — as "AAPL NSE Equity"
    // (and a linked panel resolving "NSE" would ask for AAPL.NS). So:
    //   1. the exchange stored with it in the active watchlist;
    //   2. adopted from the link group: whatever its publisher said (often none);
    //   3. picked on this screen: this screen's market, as before.
    QString exchange = watchlist_exchanges_.value(selected_symbol_);
    if (exchange.isEmpty()) {
        const auto& ctx = SymbolContext::instance();
        if (link_group_ != SymbolGroup::None && ctx.has_group_symbol(link_group_) &&
            ctx.group_symbol(link_group_).symbol == selected_symbol_)
            exchange = ctx.group_symbol(link_group_).exchange;
        else
            exchange = selected_exchange_;
    }
    return SymbolRef::equity(selected_symbol_, exchange);
}

// ── IStatefulScreen ──────────────────────────────────────────────────────────

QVariantMap EquityTradingScreen::save_state() const {
    if (inert_)
        return primary_ ? primary_->save_state() : QVariantMap{};
    return {
        {"symbol", selected_symbol_},
        {"exchange", selected_exchange_},
        {"account_id", focused_account_id_},
        {"watchlist_id", active_watchlist_id_},
    };
}

void EquityTradingScreen::restore_state(const QVariantMap& state) {
    // An inert twin restores nothing: the shell restores the real screen itself.
    if (inert_)
        return;
    // Set the active watchlist first so load_watchlists() (via on_account_changed
    // or the trailing call below) selects it.
    const QString wl = state.value("watchlist_id").toString();
    if (!wl.isEmpty())
        active_watchlist_id_ = wl;
    const QString sym = state.value("symbol", "RELIANCE").toString();
    if (!sym.isEmpty() && sym != selected_symbol_)
        on_symbol_selected(sym);
    // Account selection goes through the single connection-aware resolver (which
    // reads the saved account itself) rather than forcing state["account_id"] — that
    // direct path used to override the resolver and load an expired/unconfigured broker.
    ensure_account_loaded();
    // Honour the restored list even when the account didn't change (idempotent).
    if (!wl.isEmpty() && !focused_account_id_.isEmpty())
        load_watchlists();
}

bool EquityTradingScreen::open_section(const QString& section, const QString& focus) {
    if (inert_)
        return primary_ ? primary_->open_section(section, focus) : false;
    const QString key = section.trimmed().toLower();
    // A link that carries a focus alone acts in the view that is open.
    if (key.isEmpty())
        return !focus.trimmed().isEmpty();
    // "accounts": the account manager (connect / choose a broker account). Opened on the
    // next event-loop turn — it is a dialog, and this call arrives while the shell is still
    // finishing its navigation. Opening it changes nothing by itself.
    if (key == QLatin1String("accounts")) {
        QMetaObject::invokeMethod(this, [this]() { on_accounts_clicked(); }, Qt::QueuedConnection);
        return true;
    }
    if (key == QLatin1String("watchlist")) {
        workspace_->reveal_zone(EquityWorkspace::Zone::Watchlist);
        if (focus.compare(QLatin1String("add"), Qt::CaseInsensitive) == 0)
            watchlist_->add_box()->focus_and_select();
        return true;
    }
    if (key == QLatin1String("chart")) {
        workspace_->reveal_zone(EquityWorkspace::Zone::Centre);
        return true;
    }
    if (key == QLatin1String("ticket")) {
        if (focus.compare(QLatin1String("buy"), Qt::CaseInsensitive) == 0 ||
            focus.compare(QLatin1String("sell"), Qt::CaseInsensitive) == 0) {
            open_ticket_side(focus.compare(QLatin1String("buy"), Qt::CaseInsensitive) == 0);
        } else {
            workspace_->reveal_ticket();
            order_entry_->focus_quantity();
        }
        return true;
    }
    // Blotter tabs (POSITIONS · HOLDINGS · ORDERS · FILLS · FUNDS · STATS).
    if (bottom_panel_->show_section(key)) {
        workspace_->reveal_zone(EquityWorkspace::Zone::Centre);
        return true;
    }
    return false;
}

void EquityTradingScreen::on_replicate_portfolio_requested() {
    fincept::screens::PortfolioReplicationDialog dlg(this);
    dlg.exec();
    // Reflect any new paper positions/holdings/funds in the panels. Safe to call
    // unconditionally — it no-ops unless the focused account is paper.
    refresh_paper_panels();
}

// ============================================================================
// UNIFIED monitor detach / re-dock (feeds pop-out pattern)
// ============================================================================

void EquityTradingScreen::float_monitor() {
    if (monitor_float_ || !monitor_panel_)
        return; // already detached
    // Top-level child window (Qt::Window flag, parented for auto-cleanup —
    // same construction FloatingFeedWindow uses).
    monitor_float_ = new QWidget(this, Qt::Window);
    monitor_float_->setWindowTitle(tr("Unified Portfolio — All Accounts"));
    monitor_float_->setObjectName("floatingMonitorWindow");
    monitor_float_->setStyleSheet(
        QString("#floatingMonitorWindow{background:%1;}").arg(fincept::ui::colors::BG_BASE()));
    monitor_float_->resize(980, 620);
    auto* lay = new QVBoxLayout(monitor_float_);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(monitor_panel_); // reparents out of the blotter (its tab goes with it)
    monitor_panel_->set_floating(true);
    monitor_panel_->setVisible(true);
    monitor_float_->installEventFilter(this); // Close → redock_monitor()
    monitor_float_->show();
}

void EquityTradingScreen::redock_monitor() {
    if (!monitor_float_)
        return;
    auto* win = monitor_float_;
    monitor_float_ = nullptr; // clear first — eventFilter fires during teardown
    // Back as the ALL ACCOUNTS blotter tab.
    monitor_panel_->set_floating(false);
    bottom_panel_->add_extra_tab(monitor_panel_, tr("ALL ACCOUNTS"));
    const bool show = unified_btn_ != nullptr && unified_btn_->isChecked();
    bottom_panel_->set_extra_tab_visible(monitor_panel_, show);
    if (show)
        bottom_panel_->show_extra_tab(monitor_panel_);
    win->removeEventFilter(this);
    win->deleteLater();
}

bool EquityTradingScreen::eventFilter(QObject* watched, QEvent* event) {
    if (inert_)
        return QWidget::eventFilter(watched, event);
    if (watched == monitor_float_ && event->type() == QEvent::Close) {
        redock_monitor();
        return false; // let the (now empty) window finish closing
    }
    // On public data the connection label is the call to action: click -> ACCOUNTS.
    if (watched == conn_label_ && public_mode_ && event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            // Queued: the dialog is modal, don't run its loop inside the label's event.
            QMetaObject::invokeMethod(this, &EquityTradingScreen::on_accounts_clicked, Qt::QueuedConnection);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace fincept::screens
