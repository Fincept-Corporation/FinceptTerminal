#include "screens/watchlist/WatchlistScreen.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolDragSource.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/backtesting/BacktestingService.h"
#include "services/cloud/CloudSyncEngine.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QShortcut>
#include <QShowEvent>
#include <QSplitter>
#include <QTextStream>
#include <QVBoxLayout>

#include <limits>

namespace fincept::screens {

using namespace fincept::ui;

// ── Style builders (read live theme tokens) ─────────────────────────────────

static QString accent_btn_style() {
    QColor acc(colors::AMBER());
    auto rgb = QString("%1,%2,%3").arg(acc.red()).arg(acc.green()).arg(acc.blue());
    return QString("QPushButton { background:rgba(%1,0.1); color:%2; "
                   "border:1px solid %3; padding:0 12px; height:24px; "
                   "font-size:%4px; font-weight:700; font-family:%5; }"
                   "QPushButton:hover { background:%2; color:%6; }")
        .arg(rgb)
        .arg(colors::AMBER())
        .arg(colors::AMBER_DIM())
        .arg(fonts::TINY)
        .arg(fonts::DATA_FAMILY())
        .arg(colors::BG_BASE());
}

static QString std_btn_style() {
    return QString("QPushButton { background:%1; color:%2; border:1px solid %3; "
                   "padding:0 10px; height:24px; "
                   "font-size:%4px; font-weight:700; font-family:%5; }"
                   "QPushButton:hover { color:%6; background:%7; }")
        .arg(colors::BG_RAISED())
        .arg(colors::TEXT_SECONDARY())
        .arg(colors::BORDER_DIM())
        .arg(fonts::TINY)
        .arg(fonts::DATA_FAMILY())
        .arg(colors::TEXT_PRIMARY())
        .arg(colors::BG_HOVER());
}

static QString danger_btn_style() {
    QColor neg(colors::NEGATIVE());
    auto rgb = QString("%1,%2,%3").arg(neg.red()).arg(neg.green()).arg(neg.blue());
    return QString("QPushButton { background:rgba(%1,0.1); color:%2; "
                   "border:1px solid rgba(%1,0.3); padding:0 10px; height:24px; "
                   "font-size:%3px; font-weight:700; font-family:%4; }"
                   "QPushButton:hover { background:%2; color:%5; }")
        .arg(rgb)
        .arg(colors::NEGATIVE())
        .arg(fonts::TINY)
        .arg(fonts::DATA_FAMILY())
        .arg(colors::TEXT_PRIMARY());
}

static QString input_style() {
    QColor acc(colors::AMBER());
    auto acc_rgb = QString("%1,%2,%3").arg(acc.red()).arg(acc.green()).arg(acc.blue());
    return QString("QLineEdit { background:%1; color:%2; border:1px solid %3; "
                   "padding:3px 6px; font-size:%4px; font-family:%5; height:28px; }"
                   "QLineEdit:focus { border-color:%6; }"
                   "QLineEdit::selection { background:%7; color:%8; }")
        .arg(colors::BG_BASE())
        .arg(colors::TEXT_PRIMARY())
        .arg(colors::BORDER_DIM())
        .arg(fonts::SMALL)
        .arg(fonts::DATA_FAMILY())
        .arg(colors::BORDER_BRIGHT())
        .arg(colors::AMBER())
        .arg(colors::BG_BASE());
}

static QString list_style() {
    return QString("QListWidget { background:%1; border:none; font-family:%2; font-size:%3px; }"
                   "QListWidget::item { padding:6px 12px; color:%4; border-bottom:1px solid %5; height:26px; }"
                   "QListWidget::item:selected { background:%6; }"
                   "QListWidget::item:hover { background:%6; }")
        .arg(colors::BG_SURFACE())
        .arg(fonts::DATA_FAMILY())
        .arg(fonts::TINY)
        .arg(colors::TEXT_PRIMARY())
        .arg(colors::BORDER_DIM())
        .arg(colors::BG_HOVER());
}

// ── Constructor ──────────────────────────────────────────────────────────────

WatchlistScreen::WatchlistScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    load_watchlists();

    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });
    refresh_theme();

    // Reload from the local cache when a cloud pull updates watchlists. load_watchlists()
    // re-selects the current list, which already reloads its stocks (calling load_stocks()
    // again here fired a second forced quote request for the same symbols).
    connect(&fincept::services::cloud::CloudSyncEngine::instance(),
            &fincept::services::cloud::CloudSyncEngine::cloud_data_changed, this, [this](const QString& entity) {
                if (entity == QLatin1String("watchlist"))
                    load_watchlists();
            });
}

void WatchlistScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // The lists can change while this screen is hidden — MCP/AI-chat tools, the Equity
    // Trading screen's watchlist panel (same repository) and cloud sync all write to them,
    // and the event subscriptions below only exist while visible. Re-read from the DB on
    // every show so the user never sees a stale list. This also (re)subscribes the quotes,
    // so the former unconditional hub_resubscribe_stocks() here is no longer needed.
    load_watchlists();
    subscribe_mcp_events();
    // Rate-gated pull of cloud watchlists on screen entry (no-op when sync is off).
    fincept::services::cloud::CloudSyncEngine::instance().request_pull(QStringLiteral("watchlist"));
}

void WatchlistScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    hub_unsubscribe_all();
    unsubscribe_mcp_events();
    if (rebuild_timer_)
        rebuild_timer_->stop(); // P3: no work while hidden
}

void WatchlistScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void WatchlistScreen::retranslateUi() {
    if (sidebar_title_)
        sidebar_title_->setText(tr("WATCHLISTS"));
    if (wl_count_)
        wl_count_->setText(tr("%1 lists").arg(watchlists_.size()));

    // Top bar
    if (panel_title_) {
        if (current_wl_id_.isEmpty()) {
            panel_title_->setText(tr("Select a watchlist"));
        } else {
            // Watchlist name itself is user data — only the empty state is translatable.
            for (const auto& wl : watchlists_) {
                if (wl.id == current_wl_id_) {
                    panel_title_->setText(wl.name.toUpper());
                    break;
                }
            }
        }
    }
    if (stock_count_ && !current_wl_id_.isEmpty())
        stock_count_->setText(tr("%1 symbols").arg(stocks_.size()));
    if (refresh_btn_)
        refresh_btn_->setText(tr("REFRESH"));
    if (del_wl_btn_)
        del_wl_btn_->setText(tr("DELETE LIST"));
    if (import_csv_btn_)
        import_csv_btn_->setText(tr("IMPORT CSV"));
    if (export_csv_btn_)
        export_csv_btn_->setText(tr("EXPORT CSV"));

    // Add bar
    if (add_label_)
        add_label_->setText(tr("ADD:"));
    if (add_input_)
        add_input_->setPlaceholderText(tr("AAPL, MSFT, TSLA..."));
    if (add_btn_)
        add_btn_->setText(tr("ADD"));
    if (remove_btn_)
        remove_btn_->setText(tr("REMOVE SELECTED"));

    // Table headers — reapply so the live header row reflects the new language.
    if (table_) {
        table_->set_headers(
            {tr("SYMBOL"), tr("NAME"), tr("PRICE"), tr("CHANGE"), tr("CHG %"), tr("HIGH"), tr("LOW"), tr("VOLUME")});
    }
}

// ── MCP-driven UI sync ──────────────────────────────────────────────────────
// MCP watchlist tools publish watchlist.created / watchlist.deleted /
// watchlist.updated when the LLM mutates watchlists via AI Chat or
// Finagent. We always reload from DB — the events carry only id/symbol,
// not enough to update the table incrementally.

void WatchlistScreen::subscribe_mcp_events() {
    if (!mcp_event_subs_.isEmpty())
        return; // idempotent

    QPointer<WatchlistScreen> self = this;
    auto on_watchlists_changed = [self](const QVariantMap&) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self]() {
                if (!self)
                    return;
                // load_watchlists() preserves current_wl_id_ where possible and re-selects
                // it, which reloads the right-pane table (an extra load_stocks() here doubled
                // the forced quote request).
                self->load_watchlists();
            },
            Qt::QueuedConnection);
    };

    auto& bus = EventBus::instance();
    mcp_event_subs_.append(bus.subscribe(this, "watchlist.created", on_watchlists_changed));
    mcp_event_subs_.append(bus.subscribe(this, "watchlist.deleted", on_watchlists_changed));
    mcp_event_subs_.append(bus.subscribe(this, "watchlist.updated", on_watchlists_changed));
}

void WatchlistScreen::unsubscribe_mcp_events() {
    auto& bus = EventBus::instance();
    for (auto id : mcp_event_subs_)
        bus.unsubscribe(id);
    mcp_event_subs_.clear();
}

void WatchlistScreen::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    splitter_ = new QSplitter(Qt::Horizontal);

    splitter_->addWidget(build_sidebar());
    splitter_->addWidget(build_main_panel());
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);

    root->addWidget(splitter_);
}

// ── Sidebar ──────────────────────────────────────────────────────────────────

QWidget* WatchlistScreen::build_sidebar() {
    sidebar_ = new QWidget(this);
    sidebar_->setMinimumWidth(180);
    sidebar_->setMaximumWidth(280);

    auto* lay = new QVBoxLayout(sidebar_);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Panel header
    sidebar_header_ = new QWidget(this);
    sidebar_header_->setFixedHeight(34);
    auto* hl = new QHBoxLayout(sidebar_header_);
    hl->setContentsMargins(12, 0, 8, 0);
    hl->setSpacing(6);

    sidebar_title_ = new QLabel(tr("WATCHLISTS"));
    hl->addWidget(sidebar_title_);
    hl->addStretch();

    add_wl_btn_ = new QPushButton("+");
    add_wl_btn_->setFixedSize(24, 24);
    connect(add_wl_btn_, &QPushButton::clicked, this, &WatchlistScreen::on_add_watchlist);
    hl->addWidget(add_wl_btn_);

    lay->addWidget(sidebar_header_);

    // Watchlist list
    wl_list_ = new QListWidget;
    connect(wl_list_, &QListWidget::currentRowChanged, this, &WatchlistScreen::on_watchlist_selected);
    // Rename: double-click or right-click → Rename. WatchlistRepository::update() already
    // existed (and syncs to the cloud), but no control ever called it.
    connect(wl_list_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { rename_watchlist(wl_list_->currentRow()); });
    wl_list_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(wl_list_, &QListWidget::customContextMenuRequested, this, &WatchlistScreen::show_watchlist_menu);
    lay->addWidget(wl_list_);

    // Footer count
    wl_count_ = new QLabel(tr("0 lists"));
    wl_count_->setFixedHeight(26);
    wl_count_->setAlignment(Qt::AlignCenter);
    lay->addWidget(wl_count_);

    return sidebar_;
}

// ── Main Panel ───────────────────────────────────────────────────────────────

QWidget* WatchlistScreen::build_main_panel() {
    main_panel_ = new QWidget(this);

    auto* lay = new QVBoxLayout(main_panel_);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Top bar: title + controls
    top_bar_ = new QWidget(this);
    top_bar_->setFixedHeight(34);
    auto* tl = new QHBoxLayout(top_bar_);
    tl->setContentsMargins(14, 0, 14, 0);
    tl->setSpacing(8);

    panel_title_ = new QLabel(tr("Select a watchlist"));
    tl->addWidget(panel_title_);

    tl->addStretch();

    stock_count_ = new QLabel;
    tl->addWidget(stock_count_);

    refresh_btn_ = new QPushButton(tr("REFRESH"));
    connect(refresh_btn_, &QPushButton::clicked, this, &WatchlistScreen::on_refresh);
    tl->addWidget(refresh_btn_);

    del_wl_btn_ = new QPushButton(tr("DELETE LIST"));
    connect(del_wl_btn_, &QPushButton::clicked, this, &WatchlistScreen::on_delete_watchlist);
    del_wl_btn_->setEnabled(false);
    tl->addWidget(del_wl_btn_);

    import_csv_btn_ = new QPushButton(tr("IMPORT CSV"));
    connect(import_csv_btn_, &QPushButton::clicked, this, &WatchlistScreen::on_import_csv);
    import_csv_btn_->setEnabled(false);
    tl->addWidget(import_csv_btn_);

    export_csv_btn_ = new QPushButton(tr("EXPORT CSV"));
    connect(export_csv_btn_, &QPushButton::clicked, this, &WatchlistScreen::on_export_csv);
    export_csv_btn_->setEnabled(false);
    tl->addWidget(export_csv_btn_);

    auto* backtest_btn = new QPushButton(tr("BACKTEST"));
    connect(backtest_btn, &QPushButton::clicked, this, [this]() {
        if (stocks_.isEmpty())
            return;
        QJsonArray symbols;
        for (const auto& s : stocks_)
            symbols.append(s.symbol);
        QJsonObject config;
        config["symbols"] = symbols;
        services::backtest::BacktestingService::instance().set_pending_portfolio_config(config);
        EventBus::instance().publish("nav.switch_screen", {{"screen_id", QString("backtesting")}});
    });
    tl->addWidget(backtest_btn);

    lay->addWidget(top_bar_);

    // Add stock bar
    add_bar_ = new QWidget(this);
    add_bar_->setFixedHeight(34);
    auto* al = new QHBoxLayout(add_bar_);
    al->setContentsMargins(14, 0, 14, 0);
    al->setSpacing(6);

    add_label_ = new QLabel(tr("ADD:"));
    al->addWidget(add_label_);

    add_input_ = new QLineEdit;
    add_input_->setPlaceholderText(tr("AAPL, MSFT, TSLA..."));
    add_input_->setFixedHeight(28);
    al->addWidget(add_input_, 1);

    add_btn_ = new QPushButton(tr("ADD"));
    connect(add_btn_, &QPushButton::clicked, this, &WatchlistScreen::on_add_stock);
    connect(add_input_, &QLineEdit::returnPressed, this, &WatchlistScreen::on_add_stock);
    al->addWidget(add_btn_);

    remove_btn_ = new QPushButton(tr("REMOVE SELECTED"));
    connect(remove_btn_, &QPushButton::clicked, this, &WatchlistScreen::on_remove_stock);
    al->addWidget(remove_btn_);

    lay->addWidget(add_bar_);

    // Table — the main data area
    table_ = new ui::DataTable;
    table_->set_headers(
        {tr("SYMBOL"), tr("NAME"), tr("PRICE"), tr("CHANGE"), tr("CHG %"), tr("HIGH"), tr("LOW"), tr("VOLUME")});
    table_->set_column_widths({100, 160, 100, 90, 80, 90, 90, 110});
    table_->setSortingEnabled(true); // opt-in: WatchlistScreen stamps numeric EditRole values
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);

    table_->setAccessibleName(tr("Watchlist quotes"));

    // When the user selects a row, publish its symbol into the linked group.
    // Use itemSelectionChanged rather than cellClicked so keyboard navigation
    // also propagates.
    connect(table_, &QTableWidget::itemSelectionChanged, this, &WatchlistScreen::publish_selection_to_group);

    // Double-click a row → research that symbol; right-click → the full "open in …" menu.
    connect(table_, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int) { open_symbol_in(QStringLiteral("equity_research"), symbol_at_row(row)); });
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QWidget::customContextMenuRequested, this, &WatchlistScreen::show_stock_menu);

    // Delete on a selected row removes the symbol (with the same confirmation
    // as the REMOVE SELECTED button). Scoped to the table so it can't fire
    // while the user is typing in the ADD box.
    auto* del_sc = new QShortcut(QKeySequence::Delete, table_);
    del_sc->setContext(Qt::WidgetShortcut);
    connect(del_sc, &QShortcut::activated, this, &WatchlistScreen::on_remove_stock);

    // Drag-out: hold-and-drag a symbol row to broadcast the ticker to any
    // panel. The provider callback reads the current row at drag-start so
    // keyboard row-changes are reflected without reinstalling the filter.
    symbol_dnd::installDragSource(table_->viewport(), [this]() { return current_symbol(); }, link_group_);

    // Table + empty-state guidance share one slot; only one is visible.
    auto* table_stack = new QWidget(main_panel_);
    auto* ts_lay = new QVBoxLayout(table_stack);
    ts_lay->setContentsMargins(0, 0, 0, 0);
    ts_lay->setSpacing(0);
    ts_lay->addWidget(table_, 1);

    empty_label_ = new QLabel(table_stack);
    empty_label_->setAlignment(Qt::AlignCenter);
    empty_label_->setWordWrap(true);
    empty_label_->setVisible(false);
    ts_lay->addWidget(empty_label_, 1);

    lay->addWidget(table_stack, 1);

    // Drop: dropping a symbol onto the watchlist body adds it to the current
    // watchlist. Happens on the main_panel_ so the drop target is generous
    // (header, table, sidebar edge all count).
    symbol_dnd::installDropFilter(main_panel_, [this](const SymbolRef& ref, SymbolGroup) {
        if (current_wl_id_.isEmpty() || !ref.is_valid())
            return;
        fincept::WatchlistRepository::instance().add_stock(current_wl_id_, ref.symbol);
        load_stocks();
    });

    return main_panel_;
}

// ── Theme refresh ───────────────────────────────────────────────────────────

void WatchlistScreen::refresh_theme() {
    // Root
    setStyleSheet(QString("background:%1;").arg(colors::BG_BASE()));

    // Splitter
    if (splitter_)
        splitter_->setStyleSheet(QString("QSplitter::handle { background:%1; width:1px; }").arg(colors::BORDER_DIM()));

    // Sidebar
    if (sidebar_)
        sidebar_->setStyleSheet(
            QString("background:%1; border-right:1px solid %2;").arg(colors::BG_SURFACE(), colors::BORDER_DIM()));

    if (sidebar_header_)
        sidebar_header_->setStyleSheet(
            QString("background:%1; border-bottom:1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM()));

    if (sidebar_title_)
        sidebar_title_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; letter-spacing:0.5px; "
                                              "font-family:%3; background:transparent;")
                                          .arg(colors::AMBER())
                                          .arg(fonts::TINY)
                                          .arg(fonts::DATA_FAMILY()));

    if (add_wl_btn_)
        add_wl_btn_->setStyleSheet(accent_btn_style());

    if (wl_list_)
        wl_list_->setStyleSheet(list_style());

    if (wl_count_)
        wl_count_->setStyleSheet(
            QString("background:%1; color:%2; font-size:%3px; border-top:1px solid %4; font-family:%5;")
                .arg(colors::BG_SURFACE())
                .arg(colors::TEXT_TERTIARY())
                .arg(fonts::TINY)
                .arg(colors::BORDER_DIM())
                .arg(fonts::DATA_FAMILY()));

    // Main panel
    if (main_panel_)
        main_panel_->setStyleSheet(QString("background:%1;").arg(colors::BG_BASE()));

    if (top_bar_)
        top_bar_->setStyleSheet(
            QString("background:%1; border-bottom:1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM()));

    if (panel_title_)
        panel_title_->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; background:transparent;")
                .arg(colors::TEXT_PRIMARY())
                .arg(fonts::SMALL)
                .arg(fonts::DATA_FAMILY()));

    if (stock_count_)
        stock_count_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; background:transparent;")
                                        .arg(colors::TEXT_TERTIARY())
                                        .arg(fonts::TINY)
                                        .arg(fonts::DATA_FAMILY()));

    if (refresh_btn_)
        refresh_btn_->setStyleSheet(std_btn_style());

    if (del_wl_btn_)
        del_wl_btn_->setStyleSheet(danger_btn_style());

    if (import_csv_btn_)
        import_csv_btn_->setStyleSheet(std_btn_style());

    if (export_csv_btn_)
        export_csv_btn_->setStyleSheet(std_btn_style());

    // Add bar
    if (add_bar_)
        add_bar_->setStyleSheet(
            QString("background:%1; border-bottom:1px solid %2;").arg(colors::BG_SURFACE(), colors::BORDER_DIM()));

    if (add_label_)
        add_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; letter-spacing:0.5px; "
                                          "font-family:%3; background:transparent;")
                                      .arg(colors::TEXT_SECONDARY())
                                      .arg(fonts::TINY)
                                      .arg(fonts::DATA_FAMILY()));

    if (add_input_)
        add_input_->setStyleSheet(input_style());

    if (add_btn_)
        add_btn_->setStyleSheet(accent_btn_style());

    if (remove_btn_)
        remove_btn_->setStyleSheet(danger_btn_style());

    if (empty_label_)
        empty_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; background:transparent;")
                                        .arg(colors::TEXT_TERTIARY())
                                        .arg(fonts::SMALL)
                                        .arg(fonts::DATA_FAMILY()));
}

// ── Data Loading ─────────────────────────────────────────────────────────────

void WatchlistScreen::load_watchlists() {
    auto r = fincept::WatchlistRepository::instance().list_all();
    if (r.is_err()) {
        LOG_ERROR("Watchlist", "Failed to load watchlists");
        watchlists_.clear();
    } else {
        watchlists_ = r.value();
    }

    // Ensure at least one default watchlist exists. We deliberately leave it
    // empty rather than seeding US large-caps — those weren't user-chosen and
    // confused the AI Chat flow ("I added RITES" appeared to fail because the
    // user saw the stale starter set after restart).
    if (watchlists_.isEmpty()) {
        QColor acc(colors::AMBER());
        auto cr = fincept::WatchlistRepository::instance().create("Default", acc.name());
        if (cr.is_ok())
            watchlists_.append(cr.value());
    }

    wl_list_->blockSignals(true);
    wl_list_->clear();
    for (const auto& wl : watchlists_) {
        wl_list_->addItem(wl.name);
    }
    wl_list_->blockSignals(false);

    wl_count_->setText(tr("%1 lists").arg(watchlists_.size()));

    if (watchlists_.isEmpty())
        return;

    // Keep the user on the list they were viewing. This is reloaded from a
    // cloud pull and from every MCP watchlist.* event, and the old code always
    // jumped back to row 0 — so an LLM adding a symbol yanked the user out of
    // whichever list they had open.
    int row = 0;
    if (!current_wl_id_.isEmpty()) {
        for (int i = 0; i < watchlists_.size(); ++i) {
            if (watchlists_[i].id == current_wl_id_) {
                row = i;
                break;
            }
        }
    }
    if (wl_list_->currentRow() == row)
        on_watchlist_selected(row); // same row → currentRowChanged won't fire
    else
        wl_list_->setCurrentRow(row);
}

void WatchlistScreen::load_stocks() {
    if (current_wl_id_.isEmpty())
        return;

    auto r = fincept::WatchlistRepository::instance().get_stocks(current_wl_id_);
    if (r.is_err()) {
        stocks_.clear();
    } else {
        stocks_ = r.value();
    }

    stock_count_->setText(tr("%1 symbols").arg(stocks_.size()));
    fetch_quotes();
}

void WatchlistScreen::fetch_quotes() {
    if (stocks_.isEmpty()) {
        table_->clear_data();
        hub_unsubscribe_all();
        update_empty_state();
        return;
    }

    // P3/D3: only hold hub subscriptions while the screen is on screen — showEvent()
    // re-runs this once visible. (The constructor used to subscribe and force-request
    // quotes for a screen nobody had opened yet.)
    if (isVisible())
        hub_resubscribe_stocks();
    // Render placeholder rows synchronously so a newly-added symbol appears
    // in the table immediately. Real prices fill in as the hub delivers
    // quotes via the subscription callbacks.
    rebuild_from_cache();
}

void WatchlistScreen::schedule_table_rebuild() {
    if (!rebuild_timer_) {
        rebuild_timer_ = new QTimer(this);
        rebuild_timer_->setSingleShot(true);
        rebuild_timer_->setInterval(60); // one frame-ish; absorbs a whole hub burst
        connect(rebuild_timer_, &QTimer::timeout, this, &WatchlistScreen::rebuild_from_cache);
    }
    if (!rebuild_timer_->isActive())
        rebuild_timer_->start();
}

void WatchlistScreen::rebuild_from_cache() {
    QVector<services::QuoteData> quotes;
    quotes.reserve(stocks_.size());
    for (const auto& s : stocks_) {
        if (row_cache_.contains(s.symbol))
            quotes.append(row_cache_.value(s.symbol));
    }
    // populate_table() renders "--" placeholder rows for symbols without a quote yet, so
    // the no-data-yet case goes through the same path (and keeps the selection/scroll).
    populate_table(quotes);
    update_empty_state();
}

void WatchlistScreen::update_empty_state() {
    if (!empty_label_ || !table_)
        return;
    // An empty watchlist used to render as a blank grid with no explanation.
    const bool show = current_wl_id_.isEmpty() || stocks_.isEmpty();
    empty_label_->setVisible(show);
    table_->setVisible(!show);
    if (!show)
        return;
    empty_label_->setText(current_wl_id_.isEmpty()
                              ? tr("Select a watchlist on the left, or press + to create one.")
                              : tr("This watchlist is empty.\n\nType one or more tickers in the ADD box above\n"
                                   "(comma-separated), or drag a symbol in from another panel."));
}

void WatchlistScreen::hub_resubscribe_stocks() {
    auto& hub = datahub::DataHub::instance();
    // Stocks set is dynamic (user selected another watchlist or added/removed
    // a stock). Drop every prior subscription owned by this screen.
    hub.unsubscribe(this);
    hub_active_ = false;
    // Keep last-known quotes for symbols that are still listed so the table can show them
    // immediately (P11) instead of blanking to "--" on every tab return; drop the rest so
    // the cache cannot outgrow the list.
    {
        QSet<QString> keep;
        for (const auto& s : stocks_)
            keep.insert(s.symbol);
        for (auto it = row_cache_.begin(); it != row_cache_.end();) {
            if (keep.contains(it.key()))
                ++it;
            else
                it = row_cache_.erase(it);
        }
    }

    if (stocks_.isEmpty())
        return;

    // Human-readable names for the NAME column. Cached on disk by the service, so after the
    // first resolution this is a cheap lookup; re-render when new names arrive.
    {
        QStringList symbols;
        symbols.reserve(stocks_.size());
        for (const auto& s : stocks_)
            symbols.append(s.symbol);
        QPointer<WatchlistScreen> self = this;
        services::MarketDataService::instance().resolve_names(symbols, [self](const QHash<QString, QString>& m) {
            if (!self || m.isEmpty())
                return;
            bool changed = false;
            for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
                if (self->names_.value(it.key()) != it.value()) {
                    self->names_.insert(it.key(), it.value());
                    changed = true;
                }
            }
            if (changed && self->isVisible()) // hidden: the next show re-renders from names_
                self->schedule_table_rebuild();
        });
    }

    QStringList topics;
    topics.reserve(stocks_.size());
    for (const auto& s : stocks_) {
        const QString sym = s.symbol;
        const QString topic = QStringLiteral("market:quote:") + sym;
        topics.append(topic);
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            row_cache_.insert(sym, v.value<services::QuoteData>());
            // Do NOT rebuild the whole table here. A hub delivery burst fans out
            // one callback per symbol, so a 50-symbol watchlist used to run 50
            // full clear_data()+50-row repopulations back-to-back (2500 row
            // constructions) and lose the user's sort/selection each time.
            // Coalesce into a single rebuild at the end of the burst.
            schedule_table_rebuild();
        });
    }
    LOG_DEBUG("Watchlist", QString("subscribed + requesting %1 quote topics").arg(topics.size()));
    // force=true: watchlist symbols change on user edit; bypass min_interval
    // so newly-added tickers resolve immediately instead of waiting for the
    // scheduler tick.
    hub.request(topics, /*force=*/true);
    hub_active_ = true;
}

void WatchlistScreen::hub_unsubscribe_all() {
    if (!hub_active_)
        return;
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void WatchlistScreen::populate_table(const QVector<services::QuoteData>& quotes) {
    // Every rebuild used to drop the selected row and the scroll offset (and a quote burst
    // rebuilds every refresh) — remember both and put them back afterwards.
    const QString selected_symbol = symbol_at_row(table_->currentRow());
    const int scroll_pos = table_->verticalScrollBar() ? table_->verticalScrollBar()->value() : 0;

    // Disable sorting during population to prevent per-row re-sorting
    // (avoids both visual flickering and O(n log n) overhead per insert).
    table_->setSortingEnabled(false);
    table_->clear_data();

    // Build a map for quick lookup
    QMap<QString, services::QuoteData> quote_map;
    for (const auto& q : quotes) {
        quote_map[q.symbol] = q;
    }

    // Numeric columns use a cell that DISPLAYS the formatted text but SORTS by value. The old
    // DataTable::set_cell_numeric() wrote the number to Qt::EditRole, which a QTableWidgetItem
    // shares with DisplayRole — so the formatted "$182.52" / "+1.23%" / "52.4M" text was
    // replaced by the bare double. A missing figure sorts below every real one.
    const QString kMissingText = QStringLiteral("--");
    const double kMissingKey = -std::numeric_limits<double>::infinity();
    auto put_num = [this](int r, int c, const QString& text, double key, const QColor& fg = QColor()) {
        auto* cell = new ui::NumericTableWidgetItem(text, key);
        cell->setForeground(fg.isValid() ? fg : QColor(colors::WHITE()));
        table_->setItem(r, c, cell);
    };

    for (const auto& s : stocks_) {
        const auto it = quote_map.constFind(s.symbol);

        // NAME: resolved company name, else the one stored with the list entry (CSV import /
        // AI chat), else whatever the quote carries (which is just the ticker).
        QString name = names_.value(s.symbol);
        if (name.isEmpty())
            name = s.name;
        if (name.isEmpty() && it != quote_map.constEnd())
            name = it.value().name;
        table_->add_row({s.symbol, name, kMissingText, kMissingText, kMissingText, kMissingText, kMissingText,
                         kMissingText});
        const int row = table_->rowCount() - 1;

        if (it == quote_map.constEnd()) {
            for (int c = 2; c <= 7; ++c)
                put_num(row, c, kMissingText, kMissingKey);
            continue;
        }

        const auto& q = it.value();
        // Currency symbol of the instrument once known (₹ for .NS, … ); "$" until names resolve.
        const QString cur = names_.contains(s.symbol)
                                ? services::MarketDataService::instance().currency_prefix(s.symbol)
                                : QStringLiteral("$");
        // 0 is how a missing figure arrives (JSON null → 0): show "--", not "$0.00".
        auto money = [&cur, &kMissingText](double v) {
            return v > 0.0 ? cur + QString::number(v, 'f', 2) : kMissingText;
        };

        // Green = good, Red = bad
        const QColor chg_color(q.change_pct >= 0 ? colors::POSITIVE() : colors::NEGATIVE());

        put_num(row, 2, money(q.price), q.price > 0.0 ? q.price : kMissingKey);                 // PRICE
        put_num(row, 3, QString("%1%2").arg(q.change >= 0 ? "+" : "").arg(q.change, 0, 'f', 2), // CHANGE
                q.change, chg_color);
        put_num(row, 4, QString("%1%2%").arg(q.change_pct >= 0 ? "+" : "").arg(q.change_pct, 0, 'f', 2), // CHG %
                q.change_pct, chg_color);
        put_num(row, 5, money(q.high), q.high > 0.0 ? q.high : kMissingKey); // HIGH
        put_num(row, 6, money(q.low), q.low > 0.0 ? q.low : kMissingKey);    // LOW
        put_num(row, 7, fincept::ui::formatting::format_compact_volume(static_cast<qint64>(q.volume)),
                q.volume > 0.0 ? q.volume : kMissingKey); // VOLUME
    }

    // Re-enable sorting — Qt will apply the current sort column/order once.
    table_->setSortingEnabled(true);

    if (!selected_symbol.isEmpty()) {
        for (int r = 0; r < table_->rowCount(); ++r) {
            if (symbol_at_row(r) == selected_symbol) {
                QSignalBlocker block(table_); // restoring, not a user pick — don't re-publish to the group
                table_->selectRow(r);
                break;
            }
        }
    }
    if (table_->verticalScrollBar())
        table_->verticalScrollBar()->setValue(scroll_pos);
}

QString WatchlistScreen::symbol_at_row(int row) const {
    if (!table_ || row < 0 || row >= table_->rowCount())
        return {};
    auto* item = table_->item(row, 0);
    return item ? item->text() : QString();
}

// ── Slots ────────────────────────────────────────────────────────────────────

void WatchlistScreen::on_watchlist_selected(int row) {
    if (row < 0 || row >= watchlists_.size())
        return;
    current_wl_id_ = watchlists_[row].id;
    panel_title_->setText(watchlists_[row].name.toUpper());
    load_stocks();
    ScreenStateManager::instance().notify_changed(this);
    if (del_wl_btn_)
        del_wl_btn_->setEnabled(true);
    if (import_csv_btn_)
        import_csv_btn_->setEnabled(true);
    if (export_csv_btn_)
        export_csv_btn_->setEnabled(true);
}

void WatchlistScreen::on_add_watchlist() {
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("New Watchlist"), tr("Name:"), QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    auto r = fincept::WatchlistRepository::instance().create(name.trimmed());
    if (r.is_ok()) {
        load_watchlists();
        // Select the new one (last in list)
        wl_list_->setCurrentRow(watchlists_.size() - 1);
    } else {
        LOG_ERROR("Watchlist", QString("Failed to create watchlist: %1").arg(QString::fromStdString(r.error())));
        QMessageBox::warning(this, tr("New Watchlist"), tr("Could not create the watchlist."));
    }
}

void WatchlistScreen::rename_watchlist(int row) {
    if (row < 0 || row >= watchlists_.size())
        return;
    fincept::Watchlist wl = watchlists_[row];

    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("Rename Watchlist"), tr("Name:"), QLineEdit::Normal, wl.name, &ok).trimmed();
    if (!ok || name.isEmpty() || name == wl.name)
        return;

    wl.name = name;
    auto r = fincept::WatchlistRepository::instance().update(wl);
    if (r.is_err()) {
        LOG_ERROR("Watchlist", QString("Failed to rename watchlist: %1").arg(QString::fromStdString(r.error())));
        QMessageBox::warning(this, tr("Rename Watchlist"), tr("Could not rename the watchlist."));
        return;
    }
    load_watchlists(); // keeps the current selection and refreshes the title
}

void WatchlistScreen::show_watchlist_menu(const QPoint& pos) {
    auto* item = wl_list_->itemAt(pos);
    if (!item)
        return;
    const int row = wl_list_->row(item);
    wl_list_->setCurrentRow(row);

    QMenu menu(this);
    QAction* rename_act = menu.addAction(tr("Rename…"));
    connect(rename_act, &QAction::triggered, this, [this, row]() { rename_watchlist(row); });
    QAction* delete_act = menu.addAction(tr("Delete…"));
    connect(delete_act, &QAction::triggered, this, &WatchlistScreen::on_delete_watchlist);
    menu.exec(wl_list_->viewport()->mapToGlobal(pos));
}

void WatchlistScreen::on_delete_watchlist() {
    if (current_wl_id_.isEmpty())
        return;

    auto reply = QMessageBox::question(this, tr("Delete Watchlist"),
                                       tr("Are you sure you want to delete this watchlist and all its stocks?"),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    fincept::WatchlistRepository::instance().remove(current_wl_id_);
    current_wl_id_.clear();
    stocks_.clear();
    table_->clear_data();
    update_empty_state();
    panel_title_->setText(tr("Select a watchlist"));
    stock_count_->clear();
    if (del_wl_btn_)
        del_wl_btn_->setEnabled(false);
    if (import_csv_btn_)
        import_csv_btn_->setEnabled(false);
    if (export_csv_btn_)
        export_csv_btn_->setEnabled(false);
    load_watchlists();
}

void WatchlistScreen::on_add_stock() {
    if (current_wl_id_.isEmpty())
        return;

    QString text = add_input_->text().trimmed().toUpper();
    if (text.isEmpty())
        return;

    // Accept "AAPL, MSFT TSLA; NVDA": splitting on "," alone stored "MSFT TSLA" as ONE
    // (unquotable) symbol when the user separated tickers with spaces.
    static const QRegularExpression kSeparators(QStringLiteral("[,;\\s]+"));
    static const QRegularExpression kValidSymbol(QStringLiteral("^[A-Z0-9^][A-Z0-9.\\-=^&_]{0,24}$"));
    const QStringList parts = text.split(kSeparators, Qt::SkipEmptyParts);

    QSet<QString> existing;
    for (const auto& s : stocks_)
        existing.insert(s.symbol.toUpper());

    auto& repo = fincept::WatchlistRepository::instance();
    QStringList duplicates;
    QStringList invalid;
    for (const QString& symbol : parts) {
        if (!kValidSymbol.match(symbol).hasMatch()) {
            invalid.append(symbol);
            continue;
        }
        if (existing.contains(symbol)) {
            duplicates.append(symbol);
            continue;
        }
        auto r = repo.add_stock(current_wl_id_, symbol);
        if (r.is_err()) {
            LOG_WARN("Watchlist", QString("add_stock(%1) failed: %2").arg(symbol, QString::fromStdString(r.error())));
            invalid.append(symbol);
            continue;
        }
        existing.insert(symbol);
    }

    // Keep what could not be added in the box so the user can fix it; clear on full success.
    add_input_->setText(invalid.join(QStringLiteral(", ")));
    load_stocks();

    // The skips used to be silent, so "I added X and nothing happened" had no explanation.
    if (!duplicates.isEmpty() || !invalid.isEmpty()) {
        QStringList lines;
        if (!duplicates.isEmpty())
            lines << tr("Already in this watchlist: %1").arg(duplicates.join(QStringLiteral(", ")));
        if (!invalid.isEmpty())
            lines << tr("Not a valid ticker (or could not be saved): %1").arg(invalid.join(QStringLiteral(", ")));
        QMessageBox::information(this, tr("Add symbols"), lines.join(QLatin1Char('\n')));
    }
}

void WatchlistScreen::on_remove_stock() {
    if (current_wl_id_.isEmpty())
        return;

    // Read the symbol from the selected VISUAL row's first column, not
    // stocks_[currentRow()]: sorting is enabled, so the visual row index no
    // longer maps to the insertion-ordered stocks_ vector — indexing it would
    // remove the WRONG symbol from the watchlist.
    const int row = table_->currentRow();
    auto* sym_item = (row >= 0) ? table_->item(row, 0) : nullptr;
    if (!sym_item) {
        QMessageBox::information(this, tr("Remove symbol"), tr("Select a row in the table first."));
        return;
    }
    const QString symbol = sym_item->text();
    if (symbol.isEmpty())
        return;
    // Destructive and previously one misclick away with no confirmation.
    if (QMessageBox::question(this, tr("Remove symbol"), tr("Remove %1 from this watchlist?").arg(symbol),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    fincept::WatchlistRepository::instance().remove_stock(current_wl_id_, symbol);
    load_stocks();
}

void WatchlistScreen::on_refresh() {
    if (!current_wl_id_.isEmpty()) {
        fetch_quotes();
    }
}

void WatchlistScreen::on_export_csv() {
    if (current_wl_id_.isEmpty())
        return;

    QString wl_name;
    for (const auto& wl : watchlists_) {
        if (wl.id == current_wl_id_) {
            wl_name = wl.name;
            break;
        }
    }

    if (wl_name.isEmpty())
        return;

    const QString suggested = wl_name + QStringLiteral(".csv");

    const QString path =
        QFileDialog::getSaveFileName(this, tr("Export Watchlist to CSV"), suggested, tr("CSV Files (*.csv)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export failed"), tr("Could not open file for writing:\n%1").arg(path));
        return;
    }

    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);

    out << "SYMBOL,NAME,PRICE,CHANGE,CHG %,HIGH,LOW,VOLUME\n";

    auto csv_escape = [](const QString& s) -> QString {
        if (s.contains(',') || s.contains('"') || s.contains('\n')) {
            QString e = s;
            e.replace('"', QStringLiteral("\"\""));
            return '"' + e + '"';
        }
        return s;
    };

    for (const auto& s : stocks_) {
        // Resolved company name when we have it (otherwise the stored one, never just the ticker).
        QString name = names_.value(s.symbol);
        if (name.isEmpty())
            name = s.name;
        const auto it = row_cache_.find(s.symbol);
        if (it == row_cache_.end()) {
            out << csv_escape(s.symbol) << ',' << csv_escape(name) << ",,,,,,\n";
            continue;
        }
        const auto& q = it.value();
        // Raw numbers (the on-screen "52.4M" form can't be re-imported or summed); a missing
        // figure is an empty cell, not 0.00.
        auto num = [](double v) { return v > 0.0 ? QString::number(v, 'f', 2) : QString(); };
        out << csv_escape(q.symbol) << ',' << csv_escape(name) << ',' << num(q.price) << ','
            << QString::number(q.change, 'f', 2) << ',' << QString::number(q.change_pct, 'f', 2) << ','
            << num(q.high) << ',' << num(q.low) << ','
            << (q.volume > 0.0 ? QString::number(static_cast<qint64>(q.volume)) : QString()) << '\n';
    }

    // The export used to finish without a word — and without checking the write.
    out.flush();
    const bool write_ok = (out.status() == QTextStream::Ok) && (f.error() == QFile::NoError);
    f.close();
    if (!write_ok) {
        QMessageBox::warning(this, tr("Export failed"), tr("Writing the file failed:\n%1").arg(path));
        return;
    }
    QMessageBox::information(this, tr("Export complete"),
                             tr("Exported %n symbol(s) to:\n%1", "", static_cast<int>(stocks_.size())).arg(path));
}

void WatchlistScreen::on_import_csv() {
    if (current_wl_id_.isEmpty())
        return;

    const QString path = QFileDialog::getOpenFileName(this, tr("Import CSV"), "", tr("CSV Files (*.csv)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Import failed"), tr("Could not open file for reading:\n%1").arg(path));
        return;
    }

    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);

    QString header_line = in.readLine();
    if (header_line.trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Import failed"), tr("The file is empty."));
        return;
    }

    auto parse_csv_line = [](const QString& line) -> QStringList {
        QStringList fields;
        QString current;
        bool in_quotes = false;
        for (int i = 0; i < line.length(); ++i) {
            QChar c = line[i];
            if (c == '"') {
                if (i + 1 < line.length() && line[i + 1] == '"') {
                    current += '"';
                    ++i;
                } else {
                    in_quotes = !in_quotes;
                }
            } else if (c == ',' && !in_quotes) {
                fields.append(current);
                current.clear();
            } else {
                current += c;
            }
        }
        fields.append(current);
        return fields;
    };

    QStringList headers = parse_csv_line(header_line);
    int sym_col = -1;
    int name_col = -1;
    for (int i = 0; i < headers.size(); ++i) {
        QString h = headers[i].trimmed().toUpper();
        if (h == "SYMBOL" || h == "TICKER")
            sym_col = i;
        else if (h == "NAME")
            name_col = i;
    }

    // A plain one-ticker-per-line list (no header row) is the commonest hand-made file;
    // treat its first line as data instead of rejecting the file.
    QStringList first_data_line;
    if (sym_col == -1 && headers.size() == 1) {
        sym_col = 0;
        first_data_line = headers;
    }

    if (sym_col == -1) {
        QMessageBox::warning(this, tr("Import failed"), tr("CSV missing SYMBOL column."));
        return;
    }

    auto& repo = fincept::WatchlistRepository::instance();

    QSet<QString> existing;
    auto stocks_res = repo.get_stocks(current_wl_id_);
    if (stocks_res.is_ok()) {
        for (const auto& s : stocks_res.value()) {
            existing.insert(s.symbol.trimmed().toUpper());
        }
    }

    int imported = 0;
    int skipped = 0;

    bool use_first_line = !first_data_line.isEmpty();
    while (use_first_line || !in.atEnd()) {
        QString line;
        QStringList fields;
        if (use_first_line) {
            fields = first_data_line;
            use_first_line = false;
        } else {
            line = in.readLine();
            if (line.trimmed().isEmpty())
                continue;
            fields = parse_csv_line(line);
        }
        if (fields.size() <= sym_col)
            continue;

        QString sym = fields[sym_col].trimmed();
        if (sym.isEmpty())
            continue;

        QString upper_sym = sym.toUpper();
        if (existing.contains(upper_sym)) {
            skipped++;
            continue;
        }

        QString name = "";
        if (name_col != -1 && fields.size() > name_col) {
            name = fields[name_col].trimmed();
        }

        repo.add_stock(current_wl_id_, sym, name);
        existing.insert(upper_sym);
        imported++;
    }

    if (wl_list_) {
        on_watchlist_selected(wl_list_->currentRow());
    }

    QMessageBox::information(this, tr("Import Complete"),
                             tr("Imported %1, skipped %2 duplicates.").arg(imported).arg(skipped));
}

// ── Row actions ──────────────────────────────────────────────────────────────

void WatchlistScreen::open_symbol_in(const QString& screen_id, const QString& symbol) {
    if (symbol.isEmpty())
        return;
    // nav.open_symbol navigates first (constructing the target if needed) and then delivers the
    // ticker through IGroupLinked — unlike nav.switch_screen + a screen-specific load event,
    // which drops the symbol when the target has not been opened yet.
    EventBus::instance().publish("nav.open_symbol", {{"screen_id", screen_id}, {"symbol", symbol}});
}

void WatchlistScreen::show_stock_menu(const QPoint& pos) {
    auto* item = table_->itemAt(pos);
    if (!item)
        return;
    table_->selectRow(item->row()); // act on the row under the cursor
    const QString symbol = symbol_at_row(item->row());
    if (symbol.isEmpty())
        return;

    QMenu menu(this);
    connect(menu.addAction(tr("Open in Equity Research")), &QAction::triggered, this,
            [this, symbol]() { open_symbol_in(QStringLiteral("equity_research"), symbol); });
    // Indices, futures/FX and crypto pairs are not broker-tradable equities (see MarketPanel).
    if (!symbol.startsWith(QLatin1Char('^')) && !symbol.contains(QLatin1Char('=')) &&
        !symbol.endsWith(QLatin1String("-USD")))
        connect(menu.addAction(tr("Open in Equity Trading")), &QAction::triggered, this,
                [this, symbol]() { open_symbol_in(QStringLiteral("equity_trading"), symbol); });
    connect(menu.addAction(tr("Open in News")), &QAction::triggered, this,
            [this, symbol]() { open_symbol_in(QStringLiteral("news"), symbol); });
    connect(menu.addAction(tr("Backtest This Symbol")), &QAction::triggered, this, [symbol]() {
        QJsonObject config;
        QJsonArray symbols;
        symbols.append(symbol);
        config["symbols"] = symbols;
        services::backtest::BacktestingService::instance().set_pending_portfolio_config(config);
        EventBus::instance().publish("nav.switch_screen", {{"screen_id", QString("backtesting")}});
    });
    menu.addSeparator();
    connect(menu.addAction(tr("Copy Symbol")), &QAction::triggered, this,
            [symbol]() { QApplication::clipboard()->setText(symbol); });
    connect(menu.addAction(tr("Remove from Watchlist")), &QAction::triggered, this, &WatchlistScreen::on_remove_stock);
    menu.exec(table_->viewport()->mapToGlobal(pos));
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap WatchlistScreen::save_state() const {
    return {
        {"watchlist_id", current_wl_id_},
    };
}

void WatchlistScreen::restore_state(const QVariantMap& state) {
    const QString wl_id = state.value("watchlist_id").toString();
    if (wl_id.isEmpty())
        return;

    // Find and select the matching watchlist row
    for (int i = 0; i < watchlists_.size(); ++i) {
        if (watchlists_[i].id == wl_id) {
            // setCurrentRow() already runs on_watchlist_selected() through currentRowChanged;
            // only call it directly when the row is already current (no signal then). Calling
            // both loaded the list — and force-requested its quotes — twice.
            if (wl_list_->currentRow() == i)
                on_watchlist_selected(i);
            else
                wl_list_->setCurrentRow(i);
            return;
        }
    }
    // Watchlist not found (may have been deleted) — leave default selection
}

// ── IGroupLinked ─────────────────────────────────────────────────────────────

void WatchlistScreen::on_group_symbol_changed(const SymbolRef& ref) {
    if (!table_ || !ref.is_valid())
        return;
    // Match against the table's VISUAL rows (column 0 = symbol), not the
    // insertion-ordered stocks_ vector: with sorting enabled, selectRow(stocks_
    // index) would highlight the wrong row. No-op if the ticker isn't shown.
    for (int r = 0; r < table_->rowCount(); ++r) {
        auto* it = table_->item(r, 0);
        if (it && it->text().compare(ref.symbol, Qt::CaseInsensitive) == 0) {
            QSignalBlocker block(table_); // avoid re-emitting publish
            table_->selectRow(r);
            table_->scrollToItem(it, QAbstractItemView::PositionAtCenter);
            return;
        }
    }
}

SymbolRef WatchlistScreen::current_symbol() const {
    if (!table_)
        return {};
    // Symbol from the selected visual row's column 0 (sort-safe), not
    // stocks_[currentRow()] which is wrong once the user sorts a column.
    const int r = table_->currentRow();
    auto* sym_item = (r >= 0) ? table_->item(r, 0) : nullptr;
    if (!sym_item || sym_item->text().isEmpty())
        return {};
    return SymbolRef::equity(sym_item->text());
}

void WatchlistScreen::publish_selection_to_group() {
    if (link_group_ == SymbolGroup::None || !table_)
        return;
    const int r = table_->currentRow();
    auto* sym_item = (r >= 0) ? table_->item(r, 0) : nullptr;
    if (!sym_item || sym_item->text().isEmpty())
        return;
    const SymbolRef ref = SymbolRef::equity(sym_item->text());
    SymbolContext::instance().set_group_symbol(link_group_, ref, this);
}

} // namespace fincept::screens
