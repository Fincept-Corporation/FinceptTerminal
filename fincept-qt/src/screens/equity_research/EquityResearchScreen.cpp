// src/screens/equity_research/EquityResearchScreen.cpp
#include "screens/equity_research/EquityResearchScreen.h"

#include "core/events/EventBus.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolDragSource.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/equity_research/EquityAnalysisTab.h"
#include "screens/equity_research/EquityFilingsTab.h"
#include "screens/equity_research/EquityFinancialsTab.h"
#include "screens/equity_research/EquityNewsTab.h"
#include "screens/equity_research/EquityOverviewTab.h"
#include "screens/equity_research/EquityPeersTab.h"
#include "screens/equity_research/EquityResearchFormat.h"
#include "screens/equity_research/EquityResearchLinks.h"
#include "screens/equity_research/EquitySentimentTab.h"
#include "screens/equity_research/EquityTechnicalsTab.h"
#include "screens/equity_research/EquityValuationTab.h"
#include "services/backtesting/BacktestingService.h"
#include "services/equity/EquityResearchService.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/BrokerTopic.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
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
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QStyle>
#include <QTabBar>
#include <QTextStream>
#include <QTimeZone>
#include <QVBoxLayout>

#include <cmath>
#include <iterator>

namespace fincept::screens {

// Linked panels describe a security as (symbol, exchange, asset_class); the research data
// source (yfinance) wants "RELIANCE.NS" / "BTC-USD". Equity Trading publishes the bare broker
// symbol with its exchange ("RELIANCE" + "NSE") and Crypto Trading publishes "BTC/USDT", so a
// linked/`nav.open_symbol` hand-off loaded a ticker with no data.
static QString research_symbol_from_ref(const SymbolRef& ref) {
    QString sym = ref.symbol.trimmed();
    if (sym.isEmpty())
        return sym;

    if (ref.asset_class.compare(QLatin1String("crypto"), Qt::CaseInsensitive) == 0) {
        const qsizetype slash = sym.indexOf(QLatin1Char('/'));
        if (slash > 0) {
            QString quote = sym.mid(slash + 1).toUpper();
            if (quote == QLatin1String("USDT") || quote == QLatin1String("USDC") || quote == QLatin1String("BUSD"))
                quote = QStringLiteral("USD"); // stablecoin quotes: yfinance only lists the USD pair
            return sym.left(slash).toUpper() + QLatin1Char('-') + quote;
        }
        return sym;
    }

    if (!sym.contains(QLatin1Char('.'))) {
        static const QHash<QString, QString> kExchangeSuffix = {
            {QStringLiteral("NSE"), QStringLiteral(".NS")}, {QStringLiteral("BSE"), QStringLiteral(".BO")},
            {QStringLiteral("LSE"), QStringLiteral(".L")},  {QStringLiteral("TSX"), QStringLiteral(".TO")},
            {QStringLiteral("HKEX"), QStringLiteral(".HK")}, {QStringLiteral("XETRA"), QStringLiteral(".DE")},
            {QStringLiteral("SIX"), QStringLiteral(".SW")},
        };
        const auto it = kExchangeSuffix.constFind(ref.exchange.trimmed().toUpper());
        if (it != kExchangeSuffix.constEnd())
            sym += it.value();
    }
    return sym;
}

// Tab order + stable keys. State is saved by KEY so inserting a tab (FILINGS) does not
// reopen a saved session on the wrong tab; kLegacyTabOrder maps the old index-based state.
struct ResearchTabSpec {
    const char* key;
    const char* title;
};
static const ResearchTabSpec kResearchTabs[] = {
    {"overview", QT_TRANSLATE_NOOP("EquityResearchScreen", "Overview")},
    {"financials", QT_TRANSLATE_NOOP("EquityResearchScreen", "Financials")},
    {"filings", QT_TRANSLATE_NOOP("EquityResearchScreen", "Filings")},
    {"analysis", QT_TRANSLATE_NOOP("EquityResearchScreen", "Analysis")},
    {"technicals", QT_TRANSLATE_NOOP("EquityResearchScreen", "Technicals")},
    {"peers", QT_TRANSLATE_NOOP("EquityResearchScreen", "Peers")},
    {"news", QT_TRANSLATE_NOOP("EquityResearchScreen", "News")},
    {"sentiment", QT_TRANSLATE_NOOP("EquityResearchScreen", "Sentiment")},
    {"valuation", QT_TRANSLATE_NOOP("EquityResearchScreen", "Valuation")},
};
static const char* const kLegacyTabOrder[] = {"overview", "financials", "analysis",  "technicals",
                                              "peers",    "news",       "sentiment", "valuation"};
static constexpr const char* kResearchTabKeyProp = "er_tab_key";

static void er_repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

// CSV number: fixed-point, trailing zeros trimmed — QTextStream's default 6 significant
// digits wrote 1234567.89 as "1.23457e+06" and BRK-A's cents disappeared.
static QString er_csv_number(double v) {
    if (!std::isfinite(v))
        return QString();
    QString s = QString::number(v, 'f', 8);
    if (s.contains(QLatin1Char('.'))) {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(QLatin1Char('.')))
            s.chop(1);
    }
    return s;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════════════
EquityResearchScreen::EquityResearchScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    retranslateUi();

    // Quote poll every 30 s while visible (started in showEvent, stopped in hideEvent).
    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(30 * 1000);
    connect(refresh_timer_, &QTimer::timeout, this, [this]() {
        // Quote only — re-running info/candles every 30 s reset the Overview chart period.
        if (!current_symbol_.isEmpty())
            services::equity::EquityResearchService::instance().load_quote_only(current_symbol_);
    });

    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::quote_loaded, this, &EquityResearchScreen::on_quote_loaded);
    connect(&svc, &services::equity::EquityResearchService::info_loaded, this, &EquityResearchScreen::on_info_loaded);
    // Financials feed the Valuation tab (DCF / scoring models).
    connect(&svc, &services::equity::EquityResearchService::financials_loaded, this,
            &EquityResearchScreen::on_financials_loaded);
    // Reset the quote bar off "Loading…" when the quote fetch fails. error_occurred carries no
    // symbol, so only act while the current symbol is still waiting for its first quote — a late
    // failure for a symbol the user already left must not replace a good price.
    connect(&svc, &services::equity::EquityResearchService::error_occurred, this,
            [this](const QString& ctx, const QString&) {
                if (ctx == "Quote" && price_label_ && price_label_->text() == tr("Loading…"))
                    price_label_->setText(tr("Unavailable"));
            });

    // Keep the BUY/SELL buttons in sync as broker accounts connect/disconnect or are added/removed.
    auto& am = trading::AccountManager::instance();
    connect(&am, &trading::AccountManager::connection_state_changed, this,
            [this](const QString&, trading::ConnectionState) { update_trade_buttons(); });
    connect(&am, &trading::AccountManager::account_added, this, [this](const QString&) { update_trade_buttons(); });
    connect(&am, &trading::AccountManager::account_removed, this, [this](const QString&) { update_trade_buttons(); });
    connect(&am, &trading::AccountManager::account_updated, this, [this](const QString&) { update_trade_buttons(); });

    // "Load this symbol" requests from elsewhere in the app (command bar search).
    EventBus::instance().subscribe(this, "equity_research.load_symbol", [this](const QVariantMap& payload) {
        const QString symbol = payload.value("symbol").toString();
        if (!symbol.isEmpty())
            load_symbol(symbol);
    });

    // Default symbol — deferred one turn so a saved session restored right after
    // construction does not first fetch AAPL and then its own symbol.
    QTimer::singleShot(0, this, [this]() {
        if (current_symbol_.isEmpty())
            load_symbol(QStringLiteral("AAPL"));
    });

    // Drop a symbol dragged from another panel to research it.
    symbol_dnd::installDropFilter(this, [this](const SymbolRef& ref, SymbolGroup) {
        if (ref.is_valid())
            load_symbol(research_symbol_from_ref(ref));
    });
}

void EquityResearchScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh_timer_->start();
    hub_subscribe_broker_quote();
    update_trade_buttons(); // a broker may have connected since last shown
}

void EquityResearchScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    hub_unsubscribe_broker_quote();
    refresh_timer_->stop();
}

// ═══════════════════════════════════════════════════════════════════════════════
// UI
// ═══════════════════════════════════════════════════════════════════════════════
void EquityResearchScreen::build_ui() {
    setStyleSheet(QString("background:%1; color:%2;").arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY()));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    vl->addWidget(build_title_bar());
    vl->addWidget(build_quote_bar());

    tab_widget_ = new QTabWidget;
    tab_widget_->setDocumentMode(true);
    // Don't let the style elide tab labels (the macOS style truncates "OVERVIEW"→"OVERVI…").
    tab_widget_->tabBar()->setElideMode(Qt::ElideNone);
    tab_widget_->tabBar()->setExpanding(false);
    tab_widget_->setUsesScrollButtons(true);
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

    overview_tab_ = new EquityOverviewTab;
    financials_tab_ = new EquityFinancialsTab;
    filings_tab_ = new EquityFilingsTab;
    analysis_tab_ = new EquityAnalysisTab;
    technicals_tab_ = new EquityTechnicalsTab;
    peers_tab_ = new EquityPeersTab;
    news_tab_ = new EquityNewsTab;
    sentiment_tab_ = new EquitySentimentTab;
    valuation_tab_ = new EquityValuationTab;

    QWidget* const tabs[] = {overview_tab_, financials_tab_, filings_tab_,   analysis_tab_, technicals_tab_,
                             peers_tab_,    news_tab_,       sentiment_tab_, valuation_tab_};
    static_assert(std::size(tabs) == std::size(kResearchTabs), "tab table out of sync");
    for (size_t i = 0; i < std::size(tabs); ++i) {
        tabs[i]->setProperty(kResearchTabKeyProp, QString::fromLatin1(kResearchTabs[i].key));
        tab_widget_->addTab(tabs[i], QString()); // titles set by retranslateUi()
    }

    // A peer row asks to research that peer here (optionally straight to its chart).
    connect(peers_tab_, &EquityPeersTab::research_requested, this, [this](const QString& symbol, bool show_chart) {
        load_symbol(symbol);
        if (show_chart)
            tab_widget_->setCurrentWidget(overview_tab_);
    });

    connect(tab_widget_, &QTabWidget::currentChanged, this, &EquityResearchScreen::on_tab_changed);
    vl->addWidget(tab_widget_, 1);
}

QWidget* EquityResearchScreen::build_title_bar() {
    auto* container = new QWidget(this);
    container->setFixedHeight(48);
    // `*` keeps the bar's background/border on every child (the original bare-declaration
    // look); the named rules below style the individual controls — one stylesheet for the bar.
    container->setStyleSheet(
        QString("* { background:%1; border-bottom:1px solid %2; }"
                "QLabel#erTitle { color:%5; font-size:14px; font-weight:700; letter-spacing:2px; }"
                "QLabel#erSymbol { color:%4; font-size:14px; font-weight:600; }"
                "QLabel#erHint { color:%9; font-size:12px; }"
                "QLineEdit#equitySymbolEdit { background:%3; color:%4; border:1px solid %2; padding:2px 8px;"
                " font-size:12px; font-weight:600; }"
                "QLineEdit#equitySymbolEdit:focus { border-color:%5; }"
                "QPushButton#equityAmberBtn { background:transparent; color:%5; border:1px solid %6;"
                " padding:0 10px; font-size:%7px; font-family:%8; font-weight:700; letter-spacing:0.5px; }"
                "QPushButton#equityAmberBtn:hover { background:%5; color:%3; }"
                "QPushButton#erBuy, QPushButton#erSell { background:transparent; padding:0 14px; font-size:%7px;"
                " font-family:%8; font-weight:700; letter-spacing:1px; }"
                "QPushButton#erBuy { color:%10; border:1px solid %10; } QPushButton#erBuy:hover { background:%10; color:%3; }"
                "QPushButton#erSell { color:%11; border:1px solid %11; } QPushButton#erSell:hover { background:%11; color:%3; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(),
                 ui::colors::AMBER(), ui::colors::AMBER_DIM())
            .arg(ui::fonts::TINY)
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::colors::TEXT_TERTIARY(), ui::colors::POSITIVE(), ui::colors::NEGATIVE()));

    auto* hl = new QHBoxLayout(container);
    hl->setContentsMargins(16, 8, 16, 8);
    hl->setSpacing(12);

    title_label_ = new QLabel(container);
    title_label_->setObjectName(QStringLiteral("erTitle"));
    hl->addWidget(title_label_);

    // Current symbol — draggable to other panels.
    symbol_label_ = new QLabel(container);
    symbol_label_->setObjectName(QStringLiteral("erSymbol"));
    symbol_label_->setCursor(Qt::OpenHandCursor);
    drag_filter_ = symbol_dnd::installDragSource(symbol_label_, [this]() { return current_symbol(); }, link_group_);
    hl->addWidget(symbol_label_);

    // Type a ticker and press Enter to research it (Esc clears).
    auto* symbol_edit = new QLineEdit(container);
    symbol_edit->setObjectName(QStringLiteral("equitySymbolEdit"));
    symbol_edit->setFixedSize(130, 26);
    symbol_edit->setClearButtonEnabled(true);
    connect(symbol_edit, &QLineEdit::returnPressed, this, [this, symbol_edit]() {
        const QString sym = symbol_edit->text().trimmed().toUpper();
        if (sym.isEmpty())
            return;
        symbol_edit->clear();
        load_symbol(sym);
    });
    hl->addWidget(symbol_edit);

    // BUY / SELL — visible only when a usable broker trades this market; opens the SAME order
    // ticket the Equity Trading tab uses (see on_trade_clicked()).
    auto make_trade_btn = [&](const char* object_name) -> QPushButton* {
        auto* b = new QPushButton(container);
        b->setObjectName(QLatin1String(object_name));
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(24);
        b->setVisible(false);
        hl->addWidget(b);
        return b;
    };
    buy_btn_ = make_trade_btn("erBuy");
    sell_btn_ = make_trade_btn("erSell");
    connect(buy_btn_, &QPushButton::clicked, this, [this]() { on_trade_clicked(true); });
    connect(sell_btn_, &QPushButton::clicked, this, [this]() { on_trade_clicked(false); });

    auto make_amber_btn = [&]() -> QPushButton* {
        auto* b = new QPushButton(container);
        b->setObjectName(QStringLiteral("equityAmberBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(24);
        hl->addWidget(b);
        return b;
    };

    // BACKTEST — runs the default strategy on this symbol in the Backtesting screen.
    backtest_btn_ = make_amber_btn();
    connect(backtest_btn_, &QPushButton::clicked, this, [this]() {
        if (current_symbol_.isEmpty())
            return;
        QJsonObject config;
        config["symbols"] = QJsonArray{current_symbol_};
        config["autoRun"] = true;
        services::backtest::BacktestingService::instance().set_pending_portfolio_config(config);
        EventBus::instance().publish("nav.switch_screen", {{"screen_id", QString("backtesting")}, {"exclusive", true}});
    });

    csv_btn_ = make_amber_btn();
    connect(csv_btn_, &QPushButton::clicked, this, &EquityResearchScreen::on_download_csv_clicked);

    // + WATCHLIST — pick one of the user's watchlists.
    watch_btn_ = make_amber_btn();
    connect(watch_btn_, &QPushButton::clicked, this, [this]() {
        if (current_symbol_.isEmpty())
            return;
        QMenu menu(this);
        equity_links::add_watchlist_entries(&menu, this, current_symbol_);
        menu.exec(watch_btn_->mapToGlobal(QPoint(0, watch_btn_->height())));
    });

    // OPEN IN ▾ — hand the symbol to Equity Trading / News / Backtesting.
    open_in_btn_ = make_amber_btn();
    connect(open_in_btn_, &QPushButton::clicked, this, [this]() { show_open_in_menu(open_in_btn_); });

    hl->addStretch();
    hint_label_ = new QLabel(container);
    hint_label_->setObjectName(QStringLiteral("erHint"));
    hl->addWidget(hint_label_);
    return container;
}

QWidget* EquityResearchScreen::build_quote_bar() {
    auto* bar = new QFrame;
    bar->setObjectName(QStringLiteral("erQuoteBar"));
    bar->setFixedHeight(44);
    // One stylesheet; the change label's colour flips through its "tone" property instead of a
    // fresh setStyleSheet() on every quote — a CSS re-parse per tick while a broker stream runs.
    bar->setStyleSheet(QString("QFrame#erQuoteBar { background:%1; border-bottom:1px solid %2; }"
                               "QFrame#erQuoteBar QLabel { background:transparent; border:0; font-size:13px;"
                               " font-weight:600; color:%3; }"
                               "QLabel#erQuoteSym { color:%4; } QLabel#erQuotePrice { color:%5; }"
                               "QLabel#erQuoteChange[tone=\"up\"] { color:%6; }"
                               "QLabel#erQuoteChange[tone=\"down\"] { color:%7; }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY(),
                                ui::colors::AMBER(), ui::colors::TEXT_PRIMARY(), ui::colors::POSITIVE(),
                                ui::colors::NEGATIVE()));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(24);
    auto make_label = [&](const QString& txt, const char* object_name = nullptr) -> QLabel* {
        auto* l = new QLabel(txt, bar);
        if (object_name)
            l->setObjectName(QLatin1String(object_name));
        hl->addWidget(l);
        return l;
    };
    sym_label_ = make_label(equity_fmt::dash(), "erQuoteSym");
    price_label_ = make_label(equity_fmt::dash(), "erQuotePrice");
    change_label_ = make_label(equity_fmt::dash(), "erQuoteChange");
    vol_label_ = make_label(QString());
    hl_label_ = make_label(QString());
    mktcap_label_ = make_label(QString());
    rec_label_ = make_label(equity_fmt::dash());
    hl->addStretch();
    return bar;
}

void EquityResearchScreen::show_open_in_menu(QWidget* anchor) {
    if (current_symbol_.isEmpty())
        return;
    const QString sym = current_symbol_;
    QMenu menu(this);
    connect(menu.addAction(tr("Price chart (Overview)")), &QAction::triggered, this,
            [this]() { tab_widget_->setCurrentWidget(overview_tab_); });
    connect(menu.addAction(tr("SEC filings")), &QAction::triggered, this,
            [this]() { tab_widget_->setCurrentWidget(filings_tab_); });
    menu.addSeparator();
    equity_links::add_open_in_actions(&menu, this, sym);
    menu.addSeparator();
    connect(menu.addAction(tr("Copy symbol")), &QAction::triggered, this,
            [sym]() { QApplication::clipboard()->setText(sym); });
    menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height())));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Slots
// ═══════════════════════════════════════════════════════════════════════════════
void EquityResearchScreen::on_quote_loaded(services::equity::QuoteData q) {
    // A late quote for a symbol the user already left must not flash over the new one.
    if (q.symbol != current_symbol_)
        return;
    update_quote_bar(q);
}

void EquityResearchScreen::on_info_loaded(services::equity::StockInfo info) {
    if (info.symbol != current_symbol_)
        return;
    const bool currency_changed = current_currency_ != info.currency;
    current_currency_ = info.currency;
    // The quote normally lands before the info, so it was rendered with the USD fallback —
    // repaint it now that the instrument's own currency is known.
    if (currency_changed && last_quote_.symbol == current_symbol_)
        update_quote_bar(last_quote_);

    if (mktcap_label_) {
        mktcap_label_->setText(info.market_cap > 0
                                   ? tr("MKT CAP: %1%2").arg(equity_fmt::currency_symbol(info.currency),
                                                             equity_fmt::large(info.market_cap))
                                   : tr("MKT CAP: %1").arg(equity_fmt::dash()));
    }
    if (rec_label_) {
        const QString key = info.recommendation_key.toLower();
        QString text;
        if (key == QLatin1String("strong_buy"))
            text = tr("STRONG BUY");
        else if (key == QLatin1String("buy"))
            text = tr("BUY");
        else if (key == QLatin1String("hold") || key == QLatin1String("neutral"))
            text = tr("HOLD");
        else if (key == QLatin1String("underperform") || key == QLatin1String("sell"))
            text = tr("SELL");
        else if (key == QLatin1String("strong_sell"))
            text = tr("STRONG SELL");
        rec_label_->setText(tr("REC: %1").arg(text.isEmpty() ? equity_fmt::dash() : text));
        rec_label_->setToolTip(info.analyst_count > 0 ? tr("Consensus of %n analyst(s)", "", info.analyst_count)
                                                      : QString());
    }
    if (valuation_tab_)
        valuation_tab_->set_stock_info(info);
}

void EquityResearchScreen::on_tab_changed(int index) {
    ScreenStateManager::instance().notify_changed(this);
    activate_tab(tab_widget_->widget(index));
}

void EquityResearchScreen::activate_tab(QWidget* tab) {
    if (current_symbol_.isEmpty() || !tab)
        return;
    const QString& s = current_symbol_;
    if (tab == overview_tab_)
        overview_tab_->set_symbol(s);
    else if (tab == financials_tab_)
        financials_tab_->set_symbol(s);
    else if (tab == filings_tab_)
        filings_tab_->set_symbol(s);
    else if (tab == analysis_tab_)
        analysis_tab_->set_symbol(s);
    else if (tab == technicals_tab_)
        technicals_tab_->set_symbol(s);
    else if (tab == peers_tab_)
        peers_tab_->set_symbol(s);
    else if (tab == news_tab_)
        news_tab_->set_symbol(s);
    else if (tab == sentiment_tab_)
        sentiment_tab_->set_symbol(s);
    else if (tab == valuation_tab_)
        valuation_tab_->set_symbol(s);
}

void EquityResearchScreen::load_symbol(const QString& symbol, bool publish_to_group) {
    if (symbol.isEmpty() || symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    last_price_ = 0.0; // stale until the new symbol's quote arrives

    symbol_label_->setText(symbol);
    sym_label_->setText(symbol);
    price_label_->setText(tr("Loading…"));
    reset_quote_bar(); // don't leave the previous symbol's change/volume/H-L/mkt-cap on show
    current_currency_.clear();
    update_trade_buttons(); // a new symbol may (un)hide BUY/SELL

    // Only the tab on screen fetches now; the others catch up when first shown, so opening
    // a symbol no longer fans out into every tab's quote/financials/news/technicals calls.
    activate_tab(tab_widget_->currentWidget());

    // Quote + fundamentals for the header bar (cached for 30 s / 5 min; a request already
    // in flight from a tab is shared, not repeated).
    auto& svc = services::equity::EquityResearchService::instance();
    svc.load_quote_only(symbol);
    svc.load_info_only(symbol);

    if (isVisible())
        hub_subscribe_broker_quote();

    if (publish_to_group && link_group_ != SymbolGroup::None)
        SymbolContext::instance().set_group_symbol(link_group_, SymbolRef::equity(symbol), this);
    ScreenStateManager::instance().notify_changed(this);
}

// ═══════════════════════════════════════════════════════════════════════════════
// IGroupLinked
// ═══════════════════════════════════════════════════════════════════════════════
void EquityResearchScreen::set_group(SymbolGroup g) {
    link_group_ = g;
    if (drag_filter_)
        drag_filter_->set_source_group(g); // the drag carries the group the panel is linked to now
}

void EquityResearchScreen::on_group_symbol_changed(const SymbolRef& ref) {
    if (!ref.is_valid())
        return;
    const QString mapped = research_symbol_from_ref(ref);
    // A translated symbol (".NS" suffix, "BTC-USD") is not the form the group uses — don't echo it back.
    load_symbol(mapped, /*publish_to_group=*/mapped == ref.symbol.trimmed());
}

SymbolRef EquityResearchScreen::current_symbol() const {
    if (current_symbol_.isEmpty())
        return {};
    return SymbolRef::equity(current_symbol_);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Quote bar
// ═══════════════════════════════════════════════════════════════════════════════
void EquityResearchScreen::update_quote_bar(const services::equity::QuoteData& q) {
    if (q.symbol != current_symbol_)
        return;

    // Cache the freshest price (yfinance poll and live broker stream both funnel through
    // here) so a BUY/SELL ticket can seed it for paper market fills.
    if (q.price > 0.0)
        last_price_ = q.price;
    last_quote_ = q;

    const QString ccy = current_currency_.isEmpty() ? QStringLiteral("USD") : current_currency_;
    const QString kDash = equity_fmt::dash();

    sym_label_->setText(q.symbol);
    price_label_->setText(q.price > 0.0 ? equity_fmt::price(q.price, ccy, q.price < 1.0 ? 4 : 2) : kDash);

    if (q.price > 0.0) {
        const bool up = q.change >= 0.0;
        change_label_->setText(QString("%1%2  %3%4%")
                                   .arg(up ? "+" : "-")
                                   .arg(std::fabs(q.change), 0, 'f', q.price < 1.0 ? 4 : 2)
                                   .arg(up ? "\xe2\x96\xb2" : "\xe2\x96\xbc") // ▲ / ▼
                                   .arg(std::fabs(q.change_pct), 0, 'f', 2));
        const QString tone = up ? QStringLiteral("up") : QStringLiteral("down");
        if (change_label_->property("tone").toString() != tone) {
            change_label_->setProperty("tone", tone);
            er_repolish(change_label_);
        }
    } else {
        change_label_->setText(kDash);
    }

    auto fmt_vol = [](double v) -> QString {
        if (v >= 1e9)
            return QString("%1B").arg(v / 1e9, 0, 'f', 1);
        if (v >= 1e6)
            return QString("%1M").arg(v / 1e6, 0, 'f', 1);
        if (v >= 1e3)
            return QString("%1K").arg(v / 1e3, 0, 'f', 0);
        return QString::number(static_cast<qint64>(v));
    };
    // Zero = the source sent nothing (halted/thin instruments, indices) — a dash, not "VOL: 0".
    vol_label_->setText(tr("VOL: %1").arg(q.volume > 0.0 ? fmt_vol(q.volume) : kDash));
    hl_label_->setText(q.high > 0.0 && q.low > 0.0
                           ? tr("H:%1  L:%2").arg(equity_fmt::price(q.high, ccy), equity_fmt::price(q.low, ccy))
                           : tr("H/L: %1").arg(kDash));
}

void EquityResearchScreen::reset_quote_bar() {
    const QString dash = equity_fmt::dash();
    last_quote_ = {};
    if (change_label_) {
        change_label_->setText(dash);
        if (!change_label_->property("tone").toString().isEmpty()) {
            change_label_->setProperty("tone", QString());
            er_repolish(change_label_);
        }
    }
    if (vol_label_)
        vol_label_->setText(tr("VOL: %1").arg(dash));
    if (hl_label_)
        hl_label_->setText(tr("H/L: %1").arg(dash));
    if (mktcap_label_)
        mktcap_label_->setText(tr("MKT CAP: %1").arg(dash));
    if (rec_label_)
        rec_label_->setText(dash);
}

// ═══════════════════════════════════════════════════════════════════════════════
// CSV export
// ═══════════════════════════════════════════════════════════════════════════════
void EquityResearchScreen::on_download_csv_clicked() {
    const QString symbol = current_symbol_.trimmed();
    if (symbol.isEmpty()) {
        QMessageBox::information(this, tr("Download Price Data"), tr("Load a symbol first."));
        return;
    }

    auto* dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("Download Price Data — %1").arg(symbol));
    dlg->setModal(true);
    dlg->setMinimumWidth(380);
    dlg->setStyleSheet(QString("QLabel#erCsvNote { color:%1; font-size:10px; }"
                               "QLabel#erCsvStatus { color:%2; font-size:11px; }"
                               "QLabel#erCsvStatus[tone=\"error\"] { color:%3; }")
                           .arg(ui::colors::TEXT_TERTIARY(), ui::colors::TEXT_SECONDARY(), ui::colors::NEGATIVE()));

    auto* root = new QVBoxLayout(dlg);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto* title = new QLabel(tr("Export Yahoo Finance price history for <b>%1</b> as CSV.").arg(symbol.toHtmlEscaped()), dlg);
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

    auto* note = new QLabel(tr("Intraday intervals (below 1d) are only available for roughly the last 60 days. "
                               "Times are UTC; prices are as Yahoo reports them (split-adjusted)."),
                            dlg);
    note->setObjectName(QStringLiteral("erCsvNote"));
    note->setWordWrap(true);
    root->addWidget(note);

    auto* status = new QLabel(dlg);
    status->setObjectName(QStringLiteral("erCsvStatus"));
    status->setWordWrap(true);
    status->setVisible(false);
    root->addWidget(status);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    auto* cancel_btn = new QPushButton(tr("Cancel"), dlg);
    auto* download_btn = new QPushButton(tr("Download"), dlg);
    download_btn->setDefault(true);
    buttons->addWidget(cancel_btn);
    buttons->addWidget(download_btn);
    root->addLayout(buttons);
    connect(cancel_btn, &QPushButton::clicked, dlg, &QDialog::reject);

    connect(download_btn, &QPushButton::clicked, dlg,
            [dlg, symbol, period_combo, interval_combo, status, download_btn, cancel_btn]() {
                const QString period = period_combo->currentText();
                const QString interval = interval_combo->currentText();
                auto set_busy = [=](bool busy) {
                    download_btn->setEnabled(!busy);
                    cancel_btn->setEnabled(!busy);
                    period_combo->setEnabled(!busy);
                    interval_combo->setEnabled(!busy);
                };
                auto set_status = [status](const QString& text, bool error) {
                    status->setProperty("tone", error ? QStringLiteral("error") : QString());
                    er_repolish(status);
                    status->setText(text);
                    status->setVisible(true);
                };
                set_busy(true);
                set_status(tr("Fetching from Yahoo Finance…"), false);

                // QPointer guard: the dialog may be closed before the fetch returns.
                QPointer<QDialog> guard(dlg);
                services::equity::EquityResearchService::instance().fetch_price_history(
                    symbol, period, interval,
                    [guard, dlg, symbol, period, interval, set_busy, set_status](
                        bool ok, QVector<services::equity::Candle> candles, QString error) {
                        if (!guard)
                            return;
                        if (!ok || candles.isEmpty()) {
                            set_status(ok ? tr("No data returned for %1 (%2). Try a longer period or a daily interval.")
                                                .arg(symbol, interval)
                                          : tr("Download failed: %1").arg(error),
                                       true);
                            set_busy(false);
                            return;
                        }
                        QString safe;
                        for (const QChar ch : symbol)
                            safe += ch.isLetterOrNumber() ? ch : QChar('_');
                        const QString suggested = QStringLiteral("%1_%2_%3.csv").arg(safe, period, interval);
                        const QString path =
                            QFileDialog::getSaveFileName(dlg, tr("Save Price Data"), suggested, tr("CSV Files (*.csv)"));
                        if (!guard)
                            return;
                        if (path.isEmpty()) {
                            set_status(tr("Save cancelled."), false);
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
                        out << "datetime_utc,timestamp,open,high,low,close,volume\n";
                        for (const auto& c : candles) {
                            out << QDateTime::fromSecsSinceEpoch(c.timestamp, QTimeZone::UTC)
                                       .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                                << ',' << c.timestamp << ',' << er_csv_number(c.open) << ',' << er_csv_number(c.high)
                                << ',' << er_csv_number(c.low) << ',' << er_csv_number(c.close) << ',' << c.volume
                                << '\n';
                        }
                        out.flush();
                        const bool write_ok = out.status() == QTextStream::Ok && file.error() == QFile::NoError;
                        file.close();
                        if (!write_ok) {
                            QMessageBox::warning(dlg, tr("Export failed"), tr("Writing the CSV file failed:\n%1").arg(path));
                            set_busy(false);
                            return;
                        }
                        QMessageBox::information(
                            dlg, tr("Download complete"),
                            tr("Saved %1 rows for %2 to:\n%3").arg(candles.size()).arg(symbol, path));
                        dlg->accept();
                    });
            });

    dlg->open(); // window-modal without a nested event loop on the GUI thread
}

// ═══════════════════════════════════════════════════════════════════════════════
// i18n
// ═══════════════════════════════════════════════════════════════════════════════
void EquityResearchScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquityResearchScreen::retranslateUi() {
    if (title_label_)
        title_label_->setText(tr("EQUITY RESEARCH"));
    if (symbol_label_)
        symbol_label_->setToolTip(tr("Drag to broadcast this symbol to any panel"));
    if (auto* symbol_edit = findChild<QLineEdit*>(QStringLiteral("equitySymbolEdit"))) {
        symbol_edit->setPlaceholderText(tr("Symbol…"));
        symbol_edit->setAccessibleName(tr("Research a symbol"));
        symbol_edit->setToolTip(tr("Type a ticker (AAPL, RELIANCE.NS, BRK-B) and press Enter"));
    }
    if (buy_btn_)
        buy_btn_->setText(tr("BUY"));
    if (sell_btn_)
        sell_btn_->setText(tr("SELL"));
    if (backtest_btn_) {
        backtest_btn_->setText(tr("BACKTEST"));
        backtest_btn_->setToolTip(tr("Run the default backtest on this symbol in the Backtesting screen"));
    }
    if (csv_btn_) {
        csv_btn_->setText(tr("↓ CSV"));
        csv_btn_->setToolTip(tr("Download price history (CSV) from Yahoo Finance"));
    }
    if (watch_btn_) {
        watch_btn_->setText(tr("+ WATCHLIST"));
        watch_btn_->setToolTip(tr("Add this symbol to a watchlist"));
    }
    if (open_in_btn_) {
        open_in_btn_->setText(tr("OPEN IN ▾"));
        open_in_btn_->setToolTip(tr("Send this symbol to Equity Trading, News, Backtesting or a watchlist"));
    }
    if (hint_label_)
        hint_label_->setText(tr("Use /stock, /fund, /index... in command bar to search"));

    if (vol_label_)
        vol_label_->setText(tr("VOL: %1").arg(equity_fmt::dash()));
    if (hl_label_)
        hl_label_->setText(tr("H/L: %1").arg(equity_fmt::dash()));
    if (mktcap_label_)
        mktcap_label_->setText(tr("MKT CAP: %1").arg(equity_fmt::dash()));

    if (tab_widget_) {
        for (int i = 0; i < tab_widget_->count() && i < static_cast<int>(std::size(kResearchTabs)); ++i)
            tab_widget_->setTabText(i, tr(kResearchTabs[i].title));
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// State
// ═══════════════════════════════════════════════════════════════════════════════
QVariantMap EquityResearchScreen::save_state() const {
    QVariantMap state{{"symbol", current_symbol_}};
    if (tab_widget_ && tab_widget_->currentWidget())
        state["tab"] = tab_widget_->currentWidget()->property(kResearchTabKeyProp).toString();
    if (peers_tab_)
        state["peers"] = peers_tab_->peers_text();
    if (news_tab_)
        state["news_provider"] = news_tab_->provider_key();
    return state;
}

void EquityResearchScreen::restore_state(const QVariantMap& state) {
    // Apply the saved news provider BEFORE the symbol so the News tab's first fetch uses it.
    if (news_tab_ && state.contains("news_provider"))
        news_tab_->set_provider_key(state.value("news_provider").toString());
    if (peers_tab_ && state.contains("peers"))
        peers_tab_->set_peers_text(state.value("peers").toString());

    // Select the tab first, then load the symbol, so only that tab fetches.
    QString key = state.value("tab").toString();
    if (key.isEmpty() && state.contains("tab_index")) {
        const int legacy = state.value("tab_index").toInt();
        if (legacy >= 0 && legacy < static_cast<int>(std::size(kLegacyTabOrder)))
            key = QString::fromLatin1(kLegacyTabOrder[legacy]);
    }
    if (tab_widget_ && !key.isEmpty()) {
        for (int i = 0; i < tab_widget_->count(); ++i) {
            if (tab_widget_->widget(i)->property(kResearchTabKeyProp).toString() == key) {
                const QSignalBlocker block(tab_widget_);
                tab_widget_->setCurrentIndex(i);
                break;
            }
        }
    }

    const QString sym = state.value("symbol").toString();
    if (!sym.isEmpty()) {
        current_symbol_.clear();
        load_symbol(sym);
    } else {
        activate_tab(tab_widget_ ? tab_widget_->currentWidget() : nullptr);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Broker live quote (Indian listings stream through a connected IN broker)
// ═══════════════════════════════════════════════════════════════════════════════
void EquityResearchScreen::hub_subscribe_broker_quote() {
    hub_unsubscribe_broker_quote();

    if (current_symbol_.isEmpty())
        return;
    if (!current_symbol_.endsWith(QStringLiteral(".NS")) && !current_symbol_.endsWith(QStringLiteral(".BO")))
        return;

    // Any connected Indian-region broker (not only Fyers) can stream NSE/BSE quotes.
    const auto accounts = trading::AccountManager::instance().active_accounts();
    QString quote_broker_id, quote_account_id;
    for (const auto& a : accounts) {
        if (a.state != trading::ConnectionState::Connected)
            continue;
        auto* b = trading::BrokerRegistry::instance().get(a.broker_id);
        if (b && b->profile().region == QLatin1String("IN")) {
            quote_broker_id = a.broker_id;
            quote_account_id = a.account_id;
            break;
        }
    }
    if (quote_account_id.isEmpty())
        return;

    const QString broker_sym = current_symbol_.left(current_symbol_.length() - 3);
    const QString topic = trading::broker_topic(quote_broker_id, quote_account_id, QStringLiteral("quote"), broker_sym);
    const QString sym = current_symbol_;

    datahub::DataHub::instance().subscribe(this, topic, [this, sym](const QVariant& v) {
        if (!v.canConvert<trading::BrokerQuote>())
            return;
        const auto bq = v.value<trading::BrokerQuote>();
        services::equity::QuoteData qd;
        qd.symbol = sym;
        qd.price = bq.ltp;
        qd.change = bq.change;
        qd.change_pct = bq.change_pct;
        qd.high = bq.high;
        qd.low = bq.low;
        qd.volume = bq.volume;
        qd.timestamp = bq.timestamp;
        update_quote_bar(qd);
    });

    refresh_timer_->stop(); // broker stream replaces polling
    hub_broker_active_ = true;
}

void EquityResearchScreen::hub_unsubscribe_broker_quote() {
    if (!hub_broker_active_)
        return;
    datahub::DataHub::instance().unsubscribe(this);
    hub_broker_active_ = false;
    if (isVisible())
        refresh_timer_->start();
}

void EquityResearchScreen::on_financials_loaded(services::equity::FinancialsData data) {
    // A statement set for a symbol the user already left would feed the scoring models
    // (Altman/Piotroski/Beneish) with the wrong company's numbers.
    if (data.symbol != current_symbol_)
        return;
    if (valuation_tab_)
        valuation_tab_->set_financials(data);
}

// ── In-tab trading ──────────────────────────────────────────────────────────

// True when at least one usable account (paper, or live + Connected) is on a broker
// that serves one of `match_exchanges`.
static bool er_any_usable_broker_trades(const QStringList& match_exchanges) {
    if (match_exchanges.isEmpty())
        return false;
    for (const auto& a : trading::AccountManager::instance().list_accounts()) {
        if (!a.is_active)
            continue;
        if (!(a.trading_mode == QLatin1String("paper") || a.state == trading::ConnectionState::Connected))
            continue;
        auto* b = trading::BrokerRegistry::instance().get(a.broker_id);
        if (!b)
            continue;
        const QStringList exchanges = b->profile().exchanges;
        for (const auto& me : match_exchanges)
            if (exchanges.contains(me, Qt::CaseInsensitive))
                return true;
    }
    return false;
}

void EquityResearchScreen::update_trade_buttons() {
    if (!buy_btn_ || !sell_btn_)
        return;
    // Show BUY/SELL only when the symbol is routable AND a usable broker trades that market —
    // SPGI for Alpaca/IBKR/Saxo (US venues), RELIANCE.NS for an Indian broker, nothing for forex.
    const auto route = equity_links::trade_route(current_symbol_);
    const bool show = route.routable && er_any_usable_broker_trades(route.match_exchanges);
    buy_btn_->setVisible(show);
    sell_btn_->setVisible(show);
}

void EquityResearchScreen::on_trade_clicked(bool is_buy) {
    if (current_symbol_.isEmpty())
        return;
    const auto route = equity_links::trade_route(current_symbol_);
    if (!route.routable)
        return;
    // Hand off to the Equity Trading screen, which owns the order ticket + the paper/live
    // placement path and routes to a broker that serves match_exchanges.
    EventBus::instance().publish("equity.open_order_ticket", {{"symbol", route.bare},
                                                              {"exchange", route.order_exchange},
                                                              {"match_exchanges", route.match_exchanges},
                                                              {"is_buy", is_buy},
                                                              {"price", last_price_}});
}

} // namespace fincept::screens
