// src/screens/equity_research/EquityResearchScreen.cpp

// ═══════════════════════════════════════════════════════════════════════════════
// INCLUDES
// These are like "import" statements. They tell the compiler which other files
// this file needs to use. Without these, the compiler doesn't know what
// QWidget, QLabel, etc. are.
// ═══════════════════════════════════════════════════════════════════════════════
#include "screens/equity_research/EquityResearchScreen.h"

#include "core/events/EventBus.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolDragSource.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "trading/AccountManager.h"
#include "trading/BrokerTopic.h"
#include "screens/equity_research/EquityAnalysisTab.h"
#include "screens/equity_research/EquityFinancialsTab.h"
#include "screens/equity_research/EquityNewsTab.h"
#include "screens/equity_research/EquityOverviewTab.h"
#include "screens/equity_research/EquityPeersTab.h"
#include "screens/equity_research/EquitySentimentTab.h"
#include "screens/equity_research/EquityTalippTab.h"
#include "screens/equity_research/EquityTechnicalsTab.h"

#include "screens/equity_research/EquityValuationTab.h"
#include "services/backtesting/BacktestingService.h"
#include "services/equity/EquityResearchService.h"
#include "ui/theme/Theme.h"
#include "python/PythonRunner.h"

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTextStream>
#include <QTimeZone>
#include <QVBoxLayout>

// ═══════════════════════════════════════════════════════════════════════════════
// NAMESPACE
// "namespace fincept::screens" is like a folder/package for the code.
// All the code below belongs to that namespace so names don't clash with
// other parts of the app that might use the same class names.
// ═══════════════════════════════════════════════════════════════════════════════
namespace fincept::screens {

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR
// This runs once when EquityResearchScreen is first created.
// Think of it as the "setup" function for the whole screen.
// ═══════════════════════════════════════════════════════════════════════════════
EquityResearchScreen::EquityResearchScreen(QWidget* parent) : QWidget(parent) {
    build_ui();        // Step 1: build all the visual widgets
    retranslateUi();   // Step 2: set all the text labels (supports multiple languages)

    // A timer that refreshes the stock quote every 30 seconds automatically.
    // It only starts when the screen is visible (see showEvent below).
    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(30 * 1000); // 30,000 milliseconds = 30 seconds
    connect(refresh_timer_, &QTimer::timeout, this, [this]() {
        if (!current_symbol_.isEmpty())
            services::equity::EquityResearchService::instance().load_symbol(current_symbol_);
    });

    // "connect" wires signals to slots.
    // When the service finishes loading a quote or stock info,
    // it emits a signal → our slot functions get called automatically.
    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::quote_loaded,
            this, &EquityResearchScreen::on_quote_loaded);
    connect(&svc, &services::equity::EquityResearchService::info_loaded,
            this, &EquityResearchScreen::on_info_loaded);
    connect(&svc, &services::equity::EquityResearchService::financials_loaded,
        this, &EquityResearchScreen::on_financials_loaded);

    // Listen on the app-wide event bus for "load this symbol" requests
    // from other parts of the app (e.g. the command bar search).
    EventBus::instance().subscribe("equity_research.load_symbol", [this](const QVariantMap& payload) {
        const QString symbol = payload.value("symbol").toString();
        if (!symbol.isEmpty())
            load_symbol(symbol);
    });

    // Load Apple as the default stock when the screen first opens.
    load_symbol("AAPL");

    // Allow dragging a symbol from another panel and dropping it here
    // to switch the research view to that stock.
    symbol_dnd::installDropFilter(this, [this](const SymbolRef& ref, SymbolGroup) {
        if (ref.is_valid())
            load_symbol(ref.symbol);
    });
}

// ═══════════════════════════════════════════════════════════════════════════════
// showEvent / hideEvent
// Qt calls these automatically when the screen becomes visible or hidden.
// We use them to start/stop the refresh timer so we don't waste API calls
// when the user isn't even looking at this screen.
// ═══════════════════════════════════════════════════════════════════════════════
void EquityResearchScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh_timer_->start();
    hub_subscribe_broker_quote();
}

void EquityResearchScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    hub_unsubscribe_broker_quote();
    refresh_timer_->stop();
}

// ═══════════════════════════════════════════════════════════════════════════════
// build_ui()
// Creates every visual element on screen:
//   - the top title bar
//   - the quote bar (price, change, volume)
//   - the tab widget with all the sub-tabs
// ═══════════════════════════════════════════════════════════════════════════════
void EquityResearchScreen::build_ui() {
    // Set dark background and primary text color for the whole screen
    setStyleSheet(QString("background:%1; color:%2;")
        .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY()));

    // QVBoxLayout stacks children vertically (top to bottom)
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_title_bar());  // row 1: "EQUITY RESEARCH  AAPL  [BACKTEST] [CSV]"
    vl->addWidget(build_quote_bar());  // row 2: price, change %, volume, H/L, mkt cap

    // ── Tab Widget ───────────────────────────────────────────────────────────
    // QTabWidget is the clickable tabs bar + the content area below it.
    tab_widget_ = new QTabWidget;
    tab_widget_->setDocumentMode(true);  // cleaner look, no border around pane

    // Style the tab bar using Qt StyleSheets (similar to CSS)
    tab_widget_->setStyleSheet(QString(R"(
        QTabWidget::pane { border:0; background:%1; }
        QTabBar::tab {
            background:%2; color:%3; padding:8px 18px;
            border:0; border-bottom:2px solid transparent;
            font-size:12px; text-transform:uppercase; letter-spacing:1px;
        }
        QTabBar::tab:selected { color:%4; border-bottom:2px solid %4; }
        QTabBar::tab:hover    { color:%5; }
    )")
    .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(),
         ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));

    // Create one instance of each tab widget.
    // Each tab is its own class that knows how to display its own content.
    overview_tab_   = new EquityOverviewTab;
    financials_tab_ = new EquityFinancialsTab;
    analysis_tab_   = new EquityAnalysisTab;
    technicals_tab_ = new EquityTechnicalsTab;
    talipp_tab_     = new EquityTalippTab;
    peers_tab_      = new EquityPeersTab;
    news_tab_       = new EquityNewsTab;
    sentiment_tab_  = new EquitySentimentTab;
    valuation_tab_  = new EquityValuationTab;  // ← our new tab

    // addTab(widget, label) registers each tab.
    // We pass empty QString() as label because retranslateUi() sets
    // the real text — this way tab ORDER never depends on text width.
    tab_widget_->addTab(overview_tab_,   QString());  // index 0
    tab_widget_->addTab(financials_tab_, QString());  // index 1
    tab_widget_->addTab(analysis_tab_,   QString());  // index 2
    tab_widget_->addTab(technicals_tab_, QString());  // index 3
    tab_widget_->addTab(talipp_tab_,     QString());  // index 4
    tab_widget_->addTab(peers_tab_,      QString());  // index 5
    tab_widget_->addTab(news_tab_,       QString());  // index 6
    tab_widget_->addTab(sentiment_tab_,  QString());  // index 7
    tab_widget_->addTab(valuation_tab_,  QString());  // index 8 ← new

    // When the user clicks a different tab, call on_tab_changed()
    connect(tab_widget_, &QTabWidget::currentChanged,
            this, &EquityResearchScreen::on_tab_changed);

    vl->addWidget(tab_widget_, 1);  // "1" = stretch factor, fills remaining height
}

// ═══════════════════════════════════════════════════════════════════════════════
// build_title_bar()
// Builds the top bar: "EQUITY RESEARCH  AAPL  [BACKTEST] [↓ CSV]"
// Returns a QWidget* that build_ui() adds to the layout.
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* EquityResearchScreen::build_title_bar() {
    auto* container = new QWidget(this);
    container->setFixedHeight(48);
    container->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;")
        .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    // QHBoxLayout stacks children horizontally (left to right)
    auto* hl = new QHBoxLayout(container);
    hl->setContentsMargins(16, 8, 16, 8);
    hl->setSpacing(12);

    // "EQUITY RESEARCH" text on the left
    title_label_ = new QLabel;
    title_label_->setStyleSheet(
        QString("color:%1; font-size:14px; font-weight:700; letter-spacing:2px;")
        .arg(ui::colors::AMBER()));
    hl->addWidget(title_label_);

    // Current symbol (e.g. "AAPL") — draggable to other panels
    symbol_label_ = new QLabel;
    symbol_label_->setStyleSheet(
        QString("color:%1; font-size:14px; font-weight:600;")
        .arg(ui::colors::TEXT_PRIMARY()));
    symbol_label_->setCursor(Qt::OpenHandCursor);
    symbol_dnd::installDragSource(
        symbol_label_,
        [this]() { return current_symbol(); },
        link_group_);
    hl->addWidget(symbol_label_);

    // BACKTEST button — sends user to the backtesting screen with this symbol
    auto* backtest_btn = new QPushButton(tr("BACKTEST"), container);
    backtest_btn->setCursor(Qt::PointingHandCursor);
    backtest_btn->setFixedHeight(24);
    backtest_btn->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                "padding:0 10px; font-size:%3px; font-family:%4; font-weight:700; letter-spacing:0.5px; }"
                "QPushButton:hover { background:%5; color:%6; }")
        .arg(ui::colors::AMBER(), ui::colors::AMBER_DIM())
        .arg(ui::fonts::TINY)
        .arg(ui::fonts::DATA_FAMILY)
        .arg(ui::colors::AMBER(), ui::colors::BG_BASE()));
    connect(backtest_btn, &QPushButton::clicked, this, [this]() {
        if (current_symbol_.isEmpty()) return;
        QJsonObject config;
        QJsonArray symbols;
        symbols.append(current_symbol_);
        config["symbols"] = symbols;
        config["autoRun"] = true;
        services::backtest::BacktestingService::instance().set_pending_portfolio_config(config);
        EventBus::instance().publish("nav.switch_screen",
            {{"screen_id", QString("backtesting")}, {"exclusive", true}});
    });
    hl->addWidget(backtest_btn);

    // CSV download button — opens a dialog to download price history
    auto* csv_btn = new QPushButton(tr("↓ CSV"), container);
    csv_btn->setCursor(Qt::PointingHandCursor);
    csv_btn->setFixedHeight(24);
    csv_btn->setToolTip(tr("Download price history (CSV) from Yahoo Finance"));
    csv_btn->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                "padding:0 10px; font-size:%3px; font-family:%4; font-weight:700; letter-spacing:0.5px; }"
                "QPushButton:hover { background:%5; color:%6; }")
        .arg(ui::colors::AMBER(), ui::colors::AMBER_DIM())
        .arg(ui::fonts::TINY)
        .arg(ui::fonts::DATA_FAMILY)
        .arg(ui::colors::AMBER(), ui::colors::BG_BASE()));
    connect(csv_btn, &QPushButton::clicked, this, &EquityResearchScreen::on_download_csv_clicked);
    hl->addWidget(csv_btn);

    hl->addStretch();  // pushes the hint label to the far right

    hint_label_ = new QLabel;
    hint_label_->setStyleSheet(
        QString("color:%1; font-size:12px;").arg(ui::colors::TEXT_TERTIARY()));
    hl->addWidget(hint_label_);

    return container;
}

// ═══════════════════════════════════════════════════════════════════════════════
// build_quote_bar()
// The second bar showing: price, % change, volume, H/L, market cap
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* EquityResearchScreen::build_quote_bar() {
    auto* bar = new QFrame;
    bar->setFixedHeight(44);
    bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;")
        .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(24);

    // Lambda helper to create a styled label and add it to the layout
    auto make_label = [&](const QString& txt, const QString& color = "") -> QLabel* {
        auto* l = new QLabel(txt);
        QString style = "font-size:13px; font-weight:600;";
        if (!color.isEmpty())
            style += "color:" + color + ";";
        else
            style += "color:" + QString(ui::colors::TEXT_SECONDARY()) + ";";
        l->setStyleSheet(style);
        hl->addWidget(l);
        return l;
    };

    // Each make_label call creates one piece of the quote bar and saves
    // the pointer so we can update the text later when data arrives.
    sym_label_    = make_label(QStringLiteral("—"), ui::colors::AMBER());
    price_label_  = make_label(QStringLiteral("—"), ui::colors::TEXT_PRIMARY());
    change_label_ = make_label(QStringLiteral("—"));
    vol_label_    = make_label(QString());
    hl_label_     = make_label(QString());
    mktcap_label_ = make_label(QString());
    rec_label_    = make_label(QStringLiteral("—"));
    hl->addStretch();

    return bar;
}

// ═══════════════════════════════════════════════════════════════════════════════
// SLOTS
// Slots are functions that get called automatically when a signal fires.
// ═══════════════════════════════════════════════════════════════════════════════

// Called when a new quote (price/change/volume) arrives from the service
void EquityResearchScreen::on_quote_loaded(services::equity::QuoteData q) {
    update_quote_bar(q);
}

// Called when full stock info (company details, market cap, etc.) arrives
void EquityResearchScreen::on_info_loaded(services::equity::StockInfo info) {
    // Guard: ignore data for a different symbol (can happen if user switches fast)
    if (info.symbol != current_symbol_)
        return;
    current_currency_ = info.currency;

    // Update the market cap label in the title bar
    if (mktcap_label_) {
        if (info.market_cap > 0) {
            const QString cs = EquityOverviewTab::currency_symbol(
                info.currency.isEmpty() ? "USD" : info.currency);
            mktcap_label_->setText(
                tr("MKT CAP: %1%2").arg(cs, EquityOverviewTab::fmt_large(info.market_cap)));
        } else {
            mktcap_label_->setText(tr("MKT CAP: %1").arg(QStringLiteral("—")));
        }
    }
    if (valuation_tab_)
    valuation_tab_->set_stock_info(info);
}

// ═══════════════════════════════════════════════════════════════════════════════
// on_tab_changed(int index)
// Called every time the user clicks a different tab.
// "index" is which tab they clicked (0=Overview, 1=Financials, etc.)
//
// This is "lazy loading" — we only fetch data for a tab when the user
// actually opens it, instead of loading everything upfront.
// ═══════════════════════════════════════════════════════════════════════════════
void EquityResearchScreen::on_tab_changed(int index) {
    ScreenStateManager::instance().notify_changed(this);
    if (current_symbol_.isEmpty())
        return;

    auto& svc = services::equity::EquityResearchService::instance();

    
    switch (index) {
        case 1:
            financials_tab_->set_symbol(current_symbol_);
            svc.fetch_financials(current_symbol_);
            break;
        case 2:
            analysis_tab_->set_symbol(current_symbol_);
            svc.load_symbol(current_symbol_);
            break;
        case 3:
            technicals_tab_->set_symbol(current_symbol_);
            svc.fetch_technicals(current_symbol_);
            break;
        case 4:
            talipp_tab_->set_symbol(current_symbol_);
            break;
        case 5:
            peers_tab_->set_symbol(current_symbol_);
            break;
        case 6:
            news_tab_->set_symbol(current_symbol_);
            svc.fetch_news(current_symbol_);
            break;
        case 7:
            sentiment_tab_->set_symbol(current_symbol_);
            break;
        case 8:
            // FIX 3: "set_symbol" not "set symbol" (no space!)
            valuation_tab_->set_symbol(current_symbol_);
            // Also fetch financial data needed for scoring models
            svc.fetch_financials(current_symbol_);
            break;
        default:
            break;  // case 0 (Overview) needs no lazy load
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// load_symbol(symbol)
// The main entry point when the user wants to research a different stock.
// Called from: command bar, drag-and-drop, EventBus, restore_state().
// ═══════════════════════════════════════════════════════════════════════════════
void EquityResearchScreen::load_symbol(const QString& symbol) {
    // Don't reload if it's already the current symbol
    if (symbol.isEmpty() || symbol == current_symbol_)
        return;
    current_symbol_ = symbol;

    // Update the text in title bar and quote bar immediately
    symbol_label_->setText(symbol);
    sym_label_->setText(symbol);
    price_label_->setText(tr("Loading…"));

    // Overview tab always refreshes (it's the default landing tab)
    overview_tab_->set_symbol(symbol);

    // Also refresh whichever tab is currently open
    on_tab_changed(tab_widget_->currentIndex());

    // Tell the service to go fetch quote + info data from the API
    services::equity::EquityResearchService::instance().load_symbol(symbol);

    if (isVisible())
        hub_subscribe_broker_quote();

    // Broadcast the new symbol to other linked panels (watchlist, trading, etc.)
    if (link_group_ != SymbolGroup::None) {
        SymbolContext::instance().set_group_symbol(
            link_group_, SymbolRef::equity(symbol), this);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// IGroupLinked — when another panel in the same link group changes symbol,
// this screen follows automatically.
// ═══════════════════════════════════════════════════════════════════════════════
void EquityResearchScreen::on_group_symbol_changed(const SymbolRef& ref) {
    if (ref.is_valid())
        load_symbol(ref.symbol);
}

SymbolRef EquityResearchScreen::current_symbol() const {
    if (current_symbol_.isEmpty())
        return {};
    return SymbolRef::equity(current_symbol_);
}

// ═══════════════════════════════════════════════════════════════════════════════
// update_quote_bar(q)
// Fills in the live price data row from a QuoteData object.
// Called by on_quote_loaded() and hub_subscribe_broker_quote() callback.
// ═══════════════════════════════════════════════════════════════════════════════
void EquityResearchScreen::update_quote_bar(const services::equity::QuoteData& q) {
    if (q.symbol != current_symbol_)
        return;

    const QString cs = EquityOverviewTab::currency_symbol(
        current_currency_.isEmpty() ? "USD" : current_currency_);

    sym_label_->setText(q.symbol);
    price_label_->setText(QString("%1%2").arg(cs).arg(q.price, 0, 'f', 2));

    bool up = q.change_pct >= 0;
    QString arrow    = up ? "\xe2\x96\xb2" : "\xe2\x96\xbc";  // ▲ or ▼
    QString chg_color = up ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
    change_label_->setText(QString("%1%2  %3%4%")
        .arg(up ? "+" : "")
        .arg(q.change, 0, 'f', 2)
        .arg(arrow)
        .arg(qAbs(q.change_pct), 0, 'f', 2));
    change_label_->setStyleSheet(
        QString("font-size:13px; font-weight:600; color:%1;").arg(chg_color));

    // Format volume: show 1.2B, 450M, 12K instead of raw numbers
    auto fmt_vol = [](double v) -> QString {
        if (v >= 1e9) return QString("%1B").arg(v / 1e9, 0, 'f', 1);
        if (v >= 1e6) return QString("%1M").arg(v / 1e6, 0, 'f', 1);
        if (v >= 1e3) return QString("%1K").arg(v / 1e3, 0, 'f', 0);
        return QString::number(static_cast<qint64>(v));
    };

    vol_label_->setText(tr("VOL: %1").arg(fmt_vol(q.volume)));
    hl_label_->setText(tr("H:%1%2  L:%1%3")
        .arg(cs).arg(q.high, 0, 'f', 2).arg(q.low, 0, 'f', 2));
}

// ═══════════════════════════════════════════════════════════════════════════════
// on_download_csv_clicked()
// Opens a dialog to let the user choose period/interval, then downloads
// OHLCV price data from Yahoo Finance and saves it as a CSV file.
// Uses PythonRunner to call yfinance in the background (async).
// ═══════════════════════════════════════════════════════════════════════════════
void EquityResearchScreen::on_download_csv_clicked() {
    const QString symbol = current_symbol_.trimmed();
    if (symbol.isEmpty()) {
        QMessageBox::information(this, tr("Download Price Data"), tr("Load a symbol first."));
        return;
    }

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Download Price Data — %1").arg(symbol));
    dlg->setModal(true);
    dlg->setMinimumWidth(380);

    auto* root = new QVBoxLayout(dlg);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto* title = new QLabel(
        tr("Export Yahoo Finance price history for <b>%1</b> as CSV.").arg(symbol), dlg);
    title->setWordWrap(true);
    root->addWidget(title);

    auto* form = new QFormLayout();
    form->setSpacing(8);

    auto* period_combo = new QComboBox(dlg);
    period_combo->addItems({"1mo", "3mo", "6mo", "1y", "2y", "5y", "10y", "max"});
    period_combo->setCurrentText("1y");
    form->addRow(tr("Period:"), period_combo);

    auto* interval_combo = new QComboBox(dlg);
    interval_combo->addItems({"1m", "5m", "15m", "30m", "1h", "1d", "1wk", "1mo"});
    interval_combo->setCurrentText("1d");
    form->addRow(tr("Interval:"), interval_combo);
    root->addLayout(form);

    auto* note = new QLabel(
        tr("Note: intraday intervals (below 1d) are only available for roughly the last 60 days."), dlg);
    note->setWordWrap(true);
    note->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
    root->addWidget(note);

    auto* status = new QLabel(dlg);
    status->setWordWrap(true);
    status->setVisible(false);
    root->addWidget(status);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    auto* cancel_btn   = new QPushButton(tr("Cancel"), dlg);
    auto* download_btn = new QPushButton(tr("Download"), dlg);
    download_btn->setDefault(true);
    buttons->addWidget(cancel_btn);
    buttons->addWidget(download_btn);
    root->addLayout(buttons);

    connect(cancel_btn, &QPushButton::clicked, dlg, &QDialog::reject);

    connect(download_btn, &QPushButton::clicked, dlg,
        [dlg, symbol, period_combo, interval_combo, status, download_btn, cancel_btn]() {
            const QString period   = period_combo->currentText();
            const QString interval = interval_combo->currentText();

            auto set_busy = [=](bool busy) {
                download_btn->setEnabled(!busy);
                cancel_btn->setEnabled(!busy);
                period_combo->setEnabled(!busy);
                interval_combo->setEnabled(!busy);
            };
            set_busy(true);
            status->setVisible(true);
            status->setStyleSheet(
                QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_SECONDARY()));
            status->setText(tr("Fetching from Yahoo Finance…"));

            auto finish = [=](const QJsonArray& candles) {
                if (candles.isEmpty()) {
                    status->setStyleSheet(
                        QString("color:%1; font-size:11px;").arg(ui::colors::NEGATIVE()));
                    status->setText(tr("No data returned for %1 (%2). Try a longer period or a daily interval.")
                        .arg(symbol, interval));
                    set_busy(false);
                    return;
                }

                QString safe;
                for (const QChar ch : symbol)
                    safe += ch.isLetterOrNumber() ? ch : QChar('_');
                const QString suggested =
                    QStringLiteral("%1_%2_%3.csv").arg(safe, period, interval);
                const QString path = QFileDialog::getSaveFileName(
                    dlg, tr("Save Price Data"), suggested, tr("CSV Files (*.csv)"));
                if (path.isEmpty()) {
                    status->setText(tr("Save cancelled."));
                    set_busy(false);
                    return;
                }

                QFile file(path);
                if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                    QMessageBox::warning(dlg, tr("Export failed"),
                        tr("Could not open the file for writing:\n%1").arg(file.errorString()));
                    set_busy(false);
                    return;
                }
                QTextStream out(&file);
                out.setEncoding(QStringConverter::Utf8);
                out << "datetime,timestamp,open,high,low,close,volume\n";
                for (const auto& v : candles) {
                    const QJsonObject c = v.toObject();
                    const auto ts = static_cast<qint64>(c.value("timestamp").toDouble());
                    const QString dt = QDateTime::fromSecsSinceEpoch(ts, QTimeZone::UTC)
                        .toString("yyyy-MM-dd HH:mm:ss");
                    out << dt << ',' << ts << ','
                        << c.value("open").toDouble()  << ','
                        << c.value("high").toDouble()  << ','
                        << c.value("low").toDouble()   << ','
                        << c.value("close").toDouble() << ','
                        << static_cast<qint64>(c.value("volume").toDouble()) << '\n';
                }
                file.close();
                QMessageBox::information(dlg, tr("Download complete"),
                    tr("Saved %1 rows for %2 to:\n%3").arg(candles.size()).arg(symbol, path));
                dlg->accept();
            };

            // QPointer guard: if the dialog is closed before the Python call
            // returns, "guard" becomes null and the callback safely does nothing.
            QPointer<QDialog> guard(dlg);
            python::PythonRunner::instance().run(
                "yfinance_data.py", {"historical_period", symbol, period, interval},
                [guard, finish](python::PythonResult res) {
                    if (!guard) return;
                    finish(QJsonDocument::fromJson(
                        python::extract_json(res.output).toUtf8()).array());
                });
        });

    dlg->exec();
    dlg->deleteLater();
}

// ═══════════════════════════════════════════════════════════════════════════════
// changeEvent / retranslateUi
// Qt calls changeEvent when the app language changes.
// retranslateUi() re-sets all user-visible text to the new language.
// tr("...") marks a string as translatable.
// ═══════════════════════════════════════════════════════════════════════════════
void EquityResearchScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void EquityResearchScreen::retranslateUi() {
    if (title_label_)  title_label_->setText(tr("EQUITY RESEARCH"));
    if (symbol_label_) symbol_label_->setToolTip(tr("Drag to broadcast this symbol to any panel"));
    if (hint_label_)   hint_label_->setText(tr("Use /stock, /fund, /index... in command bar to search"));

    if (vol_label_)    vol_label_->setText(tr("VOL: %1").arg(QStringLiteral("—")));
    if (hl_label_)     hl_label_->setText(tr("H/L: %1").arg(QStringLiteral("—")));
    if (mktcap_label_) mktcap_label_->setText(tr("MKT CAP: %1").arg(QStringLiteral("—")));

    if (tab_widget_) {
        tab_widget_->setTabText(0, tr("Overview"));
        tab_widget_->setTabText(1, tr("Financials"));
        tab_widget_->setTabText(2, tr("Analysis"));
        tab_widget_->setTabText(3, tr("Technicals"));
        tab_widget_->setTabText(4, QStringLiteral("TALIpp")); // library name, not translated
        tab_widget_->setTabText(5, tr("Peers"));
        tab_widget_->setTabText(6, tr("News"));
        tab_widget_->setTabText(7, tr("Sentiment"));
        tab_widget_->setTabText(8, tr("Valuation"));  // ← our new tab label
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// save_state / restore_state
// Used by the screen manager to remember which symbol and tab were open
// when the user closes and reopens the panel.
// ═══════════════════════════════════════════════════════════════════════════════
QVariantMap EquityResearchScreen::save_state() const {
    QVariantMap state{
        {"symbol",    current_symbol_},
        {"tab_index", tab_widget_ ? tab_widget_->currentIndex() : 0}
    };
    if (peers_tab_) state["peers"] = peers_tab_->peers_text();
    return state;
}

void EquityResearchScreen::restore_state(const QVariantMap& state) {
    const QString sym = state.value("symbol").toString();
    if (!sym.isEmpty()) {
        current_symbol_.clear();
        load_symbol(sym);
    }
    if (tab_widget_) {
        const int idx = state.value("tab_index", 0).toInt();
        if (idx >= 0 && idx < tab_widget_->count())
            tab_widget_->setCurrentIndex(idx);
    }
    if (peers_tab_ && state.contains("peers"))
        peers_tab_->set_peers_text(state.value("peers").toString());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Broker live quote subscription (Fyers integration)
// For Indian stocks (.NS/.BO), if Fyers is connected, we subscribe to a
// real-time data stream instead of the 30s polling timer.
// ═══════════════════════════════════════════════════════════════════════════════
void EquityResearchScreen::hub_subscribe_broker_quote() {
    hub_unsubscribe_broker_quote();

    if (current_symbol_.isEmpty()) return;
    if (!current_symbol_.endsWith(QStringLiteral(".NS")) &&
        !current_symbol_.endsWith(QStringLiteral(".BO")))
        return;

    const auto accounts = trading::AccountManager::instance().active_accounts();
    QString fyers_account_id;
    for (const auto& a : accounts) {
        if (a.broker_id == QStringLiteral("fyers") &&
            a.state == trading::ConnectionState::Connected) {
            fyers_account_id = a.account_id;
            break;
        }
    }
    if (fyers_account_id.isEmpty()) return;

    QString broker_sym = current_symbol_.left(current_symbol_.length() - 3);
    const QString topic = trading::broker_topic(
        QStringLiteral("fyers"), fyers_account_id, QStringLiteral("quote"), broker_sym);
    const QString sym = current_symbol_;

    datahub::DataHub::instance().subscribe(this, topic, [this, sym](const QVariant& v) {
        if (!v.canConvert<trading::BrokerQuote>()) return;
        const auto bq = v.value<trading::BrokerQuote>();
        services::equity::QuoteData qd;
        qd.symbol     = sym;
        qd.price      = bq.ltp;
        qd.change     = bq.change;
        qd.change_pct = bq.change_pct;
        qd.high       = bq.high;
        qd.low        = bq.low;
        qd.volume     = bq.volume;
        qd.timestamp  = bq.timestamp;
        update_quote_bar(qd);
    });

    refresh_timer_->stop();  // broker stream replaces polling
    hub_broker_active_ = true;
}

void EquityResearchScreen::hub_unsubscribe_broker_quote() {
    if (!hub_broker_active_) return;
    datahub::DataHub::instance().unsubscribe(this);
    hub_broker_active_ = false;
    if (isVisible())
        refresh_timer_->start();
}
void EquityResearchScreen::on_financials_loaded(services::equity::FinancialsData data) {
    if (valuation_tab_)
        valuation_tab_->set_financials(data);
}

} // namespace fincept::screens