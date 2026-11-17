// src/screens/equity_research/EquityResearchScreen.cpp
#include "screens/equity_research/EquityResearchScreen.h"

#include "core/events/EventBus.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolDragSource.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/common/CommandRows.h"
#include "screens/equity_research/EquityAnalysisTab.h"
#include "screens/equity_research/EquityAnalystTab.h"
#include "screens/equity_research/EquityDividendsTab.h"
#include "screens/equity_research/EquityEarningsTab.h"
#include "screens/equity_research/EquityFilingsTab.h"
#include "screens/equity_research/EquityFinancialsTab.h"
#include "screens/equity_research/EquityNewsTab.h"
#include "screens/equity_research/EquityOptionsTab.h"
#include "screens/equity_research/EquityOverviewTab.h"
#include "screens/equity_research/EquityOwnershipTab.h"
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
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
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
#include <QStackedWidget>
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
    {"dividends", QT_TRANSLATE_NOOP("EquityResearchScreen", "Dividends")},
    {"earnings", QT_TRANSLATE_NOOP("EquityResearchScreen", "Earnings")},
    {"options", QT_TRANSLATE_NOOP("EquityResearchScreen", "Options")},
    {"analyst", QT_TRANSLATE_NOOP("EquityResearchScreen", "Analyst")},
    {"ownership", QT_TRANSLATE_NOOP("EquityResearchScreen", "Ownership")},
};
static const char* const kLegacyTabOrder[] = {"overview", "financials", "analysis",  "technicals",
                                              "peers",    "news",       "sentiment", "valuation"};
static constexpr const char* kResearchTabKeyProp = "er_tab_key";

static void er_repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

// Header text for a failed quote — short enough for the quote bar; the full reason goes in
// the tooltip. `code` is LoadFailure::code.
static QString er_short_failure(const QString& code) {
    if (code == QLatin1String("not_found"))
        return EquityResearchScreen::tr("Not found");
    if (code == QLatin1String("rate_limited"))
        return EquityResearchScreen::tr("Rate limited");
    if (code == QLatin1String("network"))
        return EquityResearchScreen::tr("Network error");
    if (code == QLatin1String("no_data"))
        return EquityResearchScreen::tr("No data");
    return EquityResearchScreen::tr("Unavailable");
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
        // A symbol Yahoo said it does not have is not polled again.
        if (!current_symbol_.isEmpty() && !symbol_not_found_)
            services::equity::EquityResearchService::instance().load_quote_only(current_symbol_);
    });

    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::quote_loaded, this, &EquityResearchScreen::on_quote_loaded);
    connect(&svc, &services::equity::EquityResearchService::info_loaded, this, &EquityResearchScreen::on_info_loaded);
    // Financials feed the Valuation tab (DCF / scoring models).
    connect(&svc, &services::equity::EquityResearchService::financials_loaded, this,
            &EquityResearchScreen::on_financials_loaded);
    // Failures carry their symbol: a late one for a symbol the user already left is dropped, a
    // failed 30 s poll only marks the price on screen stale, and a "not found" starts a lookup
    // of what the user typed.
    connect(&svc, &services::equity::EquityResearchService::load_failed, this,
            [this](const services::equity::LoadFailure& failure) { on_load_failed(failure); });

    // Keep the BUY/SELL buttons in sync as broker accounts connect/disconnect or are added/removed.
    auto& am = trading::AccountManager::instance();
    connect(&am, &trading::AccountManager::connection_state_changed, this,
            [this](const QString&, trading::ConnectionState) { update_trade_buttons(); });
    connect(&am, &trading::AccountManager::account_added, this, [this](const QString&) { update_trade_buttons(); });
    connect(&am, &trading::AccountManager::account_removed, this, [this](const QString&) { update_trade_buttons(); });
    connect(&am, &trading::AccountManager::account_updated, this, [this](const QString&) { update_trade_buttons(); });

    // "Load this symbol" requests from elsewhere in the app (command bar search).
    // Peer list pushed by the set_research_peers tool. Queued so a symbol opened in the
    // same call (nav.open_symbol, also queued) is loaded first and the list lands on it.
    EventBus::instance().subscribe(this, "equity_research.set_peers", [this](const QVariantMap& payload) {
        const QString peers = payload.value("peers").toString().trimmed();
        if (peers.isEmpty())
            return;
        QMetaObject::invokeMethod(
            this,
            [this, peers]() {
                if (!peers_tab_ || !tab_widget_)
                    return;
                tab_widget_->setCurrentWidget(peers_tab_);
                peers_tab_->apply_peers(peers);
            },
            Qt::QueuedConnection);
    });

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

    // Created under its final parent: it used to be built parentless, all fourteen tabs
    // inside it, and then moved into the screen by vl->addWidget() — a move re-styles the
    // whole moved subtree (~0.4 s of the open stall).
    tab_widget_ = new QTabWidget(this);
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
            font-size:)" + fincept::ui::fonts::px(-2) + R"(px; text-transform:uppercase; letter-spacing:1px;
        }
        QTabBar::tab:selected { color:%4; border-bottom:2px solid %4; }
        QTabBar::tab:hover    { color:%5; }
        QTabBar::scroller { width:44px; }
        QTabBar QToolButton { background:%6; color:%5; border:1px solid %7; }
        QTabBar QToolButton:hover { border-color:%4; }
    )")
                                   .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(),
                                        ui::colors::AMBER(), ui::colors::TEXT_PRIMARY(), ui::colors::BG_RAISED(),
                                        ui::colors::BORDER_MED()));

    // Pages are created directly in the tab widget's page stack, so addTab() below does not
    // move (and re-style) a built tab. Without the stack (never expected) addTab reparents.
    QWidget* page_parent = tab_widget_->findChild<QStackedWidget*>(QStringLiteral("qt_tabwidget_stackedwidget"),
                                                                  Qt::FindDirectChildrenOnly);
    if (!page_parent)
        page_parent = tab_widget_;

    // Built up front: the tabs that carry saved state (Peers, News) and the two opened most.
    overview_tab_ = new EquityOverviewTab(page_parent);
    financials_tab_ = new EquityFinancialsTab(page_parent);
    peers_tab_ = new EquityPeersTab(page_parent);
    news_tab_ = new EquityNewsTab(page_parent);

    // The rest only load data once shown (activate_tab), and until then hold no state — except
    // Valuation, which is handed the current symbol's info + statements (on_info_loaded /
    // on_financials_loaded); built later, it gets the latest of each at construction. They are
    // now also BUILT on first show: each gets an empty page; materialize_tab() builds the tab
    // inside it.
    auto deferred = [this, page_parent](std::function<QWidget*(QWidget*)> make) -> QWidget* {
        auto* page = new QWidget(page_parent);
        auto* lay = new QVBoxLayout(page);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(0);
        lazy_tabs_.insert(page, LazyTab{std::move(make), nullptr});
        return page;
    };
    // A "did you mean" suggestion on a /data tab researches that symbol here (same tab stays open).
    auto wire_data_tab = [this](EquityDataTab* t) {
        connect(t, &EquityDataTab::research_requested, this, [this](const QString& symbol) { load_symbol(symbol); });
    };

    QWidget* const tabs[] = {
        overview_tab_,
        financials_tab_,
        deferred([this](QWidget* p) -> QWidget* { return filings_tab_ = new EquityFilingsTab(p); }),
        deferred([this](QWidget* p) -> QWidget* { return analysis_tab_ = new EquityAnalysisTab(p); }),
        deferred([this](QWidget* p) -> QWidget* { return technicals_tab_ = new EquityTechnicalsTab(p); }),
        peers_tab_,
        news_tab_,
        deferred([this](QWidget* p) -> QWidget* { return sentiment_tab_ = new EquitySentimentTab(p); }),
        deferred([this](QWidget* p) -> QWidget* {
            valuation_tab_ = new EquityValuationTab(p);
            // What the screen pushed for the current symbol while the tab did not exist (it
            // only keeps the current symbol's: older ones the tab's set_symbol() would drop).
            if (!last_info_.symbol.isEmpty() && last_info_.symbol == current_symbol_)
                valuation_tab_->set_stock_info(last_info_);
            if (!last_financials_.symbol.isEmpty() && last_financials_.symbol == current_symbol_)
                valuation_tab_->set_financials(last_financials_);
            return valuation_tab_;
        }),
        deferred([this](QWidget* p) -> QWidget* { return dividends_tab_ = new EquityDividendsTab(p); }),
        deferred([this](QWidget* p) -> QWidget* { return earnings_tab_ = new EquityEarningsTab(p); }),
        deferred([this, wire_data_tab](QWidget* p) -> QWidget* {
            options_tab_ = new EquityOptionsTab(p);
            wire_data_tab(options_tab_);
            return options_tab_;
        }),
        deferred([this, wire_data_tab](QWidget* p) -> QWidget* {
            analyst_tab_ = new EquityAnalystTab(p);
            wire_data_tab(analyst_tab_);
            return analyst_tab_;
        }),
        deferred([this, wire_data_tab](QWidget* p) -> QWidget* {
            ownership_tab_ = new EquityOwnershipTab(p);
            wire_data_tab(ownership_tab_);
            return ownership_tab_;
        }),
    };
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
    // 48px at the default font size; grows with larger fonts instead of clipping its controls.
    container->setMinimumHeight(48);
    container->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    // `*` keeps the bar's background/border on every child (the original bare-declaration
    // look); the named rules below style the individual controls — one stylesheet for the bar.
    container->setStyleSheet(
        QString("* { background:%1; border-bottom:1px solid %2; }"
                "QLabel#erTitle { color:%5; font-size:" + fincept::ui::fonts::px(0) + "px; font-weight:700; letter-spacing:2px; }"
                "QLabel#erSymbol { color:%4; font-size:" + fincept::ui::fonts::px(0) + "px; font-weight:600; }"
                "QLabel#erHint { color:%9; font-size:" + fincept::ui::fonts::px(-2) + "px; }"
                "QLineEdit#equitySymbolEdit { background:%3; color:%4; border:1px solid %2; padding:2px 8px;"
                " font-size:" + fincept::ui::fonts::px(-2) + "px; font-weight:600; }"
                "QLineEdit#equitySymbolEdit:focus { border-color:%5; }"
                "QPushButton#equityAmberBtn { background:transparent; color:%5; border:1px solid %6;"
                " padding:0 10px; font-size:%7px; font-family:%8; font-weight:700; letter-spacing:0.5px; }"
                "QPushButton#equityAmberBtn:hover { background:%5; color:%12; }"
                "QPushButton#erBuy, QPushButton#erSell { background:transparent; padding:0 14px; font-size:%7px;"
                " font-family:%8; font-weight:700; letter-spacing:1px; }"
                "QPushButton#erBuy { color:%10; border:1px solid %10; } QPushButton#erBuy:hover { background:%10; color:%12; }"
                "QPushButton#erSell { color:%11; border:1px solid %11; } QPushButton#erSell:hover { background:%11; color:%12; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(),
                 ui::colors::AMBER(), ui::colors::AMBER_DIM())
            .arg(ui::fonts::TINY.get())
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::colors::TEXT_TERTIARY(), ui::colors::POSITIVE(), ui::colors::NEGATIVE(),
                 ui::colors::TEXT_ON_ACCENT()));

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
    symbol_edit_ = symbol_edit;
    symbol_edit->setObjectName(QStringLiteral("equitySymbolEdit"));
    symbol_edit->setFixedWidth(130);
    symbol_edit->setMinimumHeight(26);
    symbol_edit->setClearButtonEnabled(true);
    connect(symbol_edit, &QLineEdit::returnPressed, this, [this, symbol_edit]() {
        const QString sym = symbol_edit->text().trimmed().toUpper();
        if (sym.isEmpty())
            return;
        symbol_edit->clear();
        load_symbol(sym);
    });
    hl->addWidget(symbol_edit);

    // DID YOU MEAN? ▾ — shown only after a typed symbol was not found and Yahoo suggested
    // alternatives; opens them as a menu (same look as WATCHLIST / OPEN IN).
    suggest_btn_ = new QPushButton(container);
    suggest_btn_->setObjectName(QStringLiteral("equityAmberBtn"));
    suggest_btn_->setCursor(Qt::PointingHandCursor);
    suggest_btn_->setMinimumHeight(24);
    suggest_btn_->setVisible(false);
    connect(suggest_btn_, &QPushButton::clicked, this, [this]() { show_candidates_menu(suggest_btn_); });
    hl->addWidget(suggest_btn_);

    // BUY / SELL — visible only when a usable broker trades this market; opens the SAME order
    // ticket the Equity Trading tab uses (see on_trade_clicked()).
    auto make_trade_btn = [&](const char* object_name) -> QPushButton* {
        auto* b = new QPushButton(container);
        b->setObjectName(QLatin1String(object_name));
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumHeight(24);
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
        b->setMinimumHeight(24);
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

    hint_label_ = new QLabel(container);
    hint_label_->setObjectName(QStringLiteral("erHint"));
    // The hint takes whatever width is left and never sets a minimum: at its natural width
    // it held the whole screen at ~1,250 px, so a 1,100 px window clipped every sub-tab.
    hint_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    hint_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hl->addWidget(hint_label_, 1);
    return container;
}

QWidget* EquityResearchScreen::build_quote_bar() {
    auto* bar = new QFrame(this); // under its final parent: no re-style when the layout takes it
    bar->setObjectName(QStringLiteral("erQuoteBar"));
    bar->setFixedHeight(44);
    // One stylesheet; the change label's colour flips through its "tone" property instead of a
    // fresh setStyleSheet() on every quote — a CSS re-parse per tick while a broker stream runs.
    bar->setStyleSheet(QString("QFrame#erQuoteBar { background:%1; border-bottom:1px solid %2; }"
                               "QFrame#erQuoteBar QLabel { background:transparent; border:0; font-size:" + fincept::ui::fonts::px(-1) + "px;"
                               " font-weight:600; color:%3; }"
                               "QLabel#erQuoteSym { color:%4; } QLabel#erQuotePrice { color:%5; }"
                               "QLabel#erQuoteChange[tone=\"up\"] { color:%6; }"
                               "QLabel#erQuoteChange[tone=\"down\"] { color:%7; }"
                               // no price for this symbol / last price kept after a failed refresh
                               "QLabel#erQuotePrice[tone=\"error\"] { color:%7; }"
                               "QLabel#erQuotePrice[tone=\"stale\"] { color:%3; }")
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
    // The Filings tab may not be built yet: switch to its page (on_tab_changed builds it).
    connect(menu.addAction(tr("SEC filings")), &QAction::triggered, this, [this]() {
        if (QWidget* page = tab_page("filings"))
            tab_widget_->setCurrentWidget(page);
    });
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

    last_info_ = info;
    update_info_labels(info);
    if (valuation_tab_)
        valuation_tab_->set_stock_info(info);
}

void EquityResearchScreen::update_info_labels(const services::equity::StockInfo& info) {
    if (mktcap_label_) {
        // Yahoo reports market cap in the quote's MAJOR currency (pounds for a pence quote).
        mktcap_label_->setText(tr("MKT CAP: %1")
                                   .arg(info.market_cap > 0 ? equity_fmt::money(info.market_cap, info.currency)
                                                            : equity_fmt::dash()));
    }
    if (rec_label_) {
        // Shared with the Overview tab's badge (equity_fmt::recommendation).
        const QString text = equity_fmt::recommendation(info.recommendation_key).text;
        rec_label_->setText(tr("REC: %1").arg(text.isEmpty() ? equity_fmt::dash() : text));
        rec_label_->setToolTip(info.analyst_count > 0 ? tr("Consensus of %n analyst(s)", "", info.analyst_count)
                                                      : QString());
    }
}

void EquityResearchScreen::on_tab_changed(int index) {
    ScreenStateManager::instance().notify_changed(this);
    activate_tab(tab_widget_->widget(index));
}

QWidget* EquityResearchScreen::materialize_tab(QWidget* page) {
    auto it = lazy_tabs_.find(page);
    if (it == lazy_tabs_.end())
        return page; // built up front: the page is the tab
    if (!it->content) {
        // Built directly inside its page (no move into a styled parent afterwards).
        const std::function<QWidget*(QWidget*)> make = it->make;
        QWidget* tab = make(page);
        it = lazy_tabs_.find(page); // nothing should touch lazy_tabs_ meanwhile; stay safe
        if (it == lazy_tabs_.end() || !tab)
            return page;
        it->content = tab;
        if (QLayout* lay = page->layout())
            lay->addWidget(tab);
        tab->show(); // a child added to an already visible page is not shown by itself
    }
    return it->content;
}

QWidget* EquityResearchScreen::tab_page(const char* key) const {
    if (!tab_widget_)
        return nullptr;
    const QString k = QString::fromLatin1(key);
    for (int i = 0; i < tab_widget_->count(); ++i) {
        QWidget* w = tab_widget_->widget(i);
        if (w && w->property(kResearchTabKeyProp).toString() == k)
            return w;
    }
    return nullptr;
}

void EquityResearchScreen::activate_tab(QWidget* tab) {
    // A deferred tab is built when its page is first shown — with or without a symbol, so the
    // page is never blank. From here on `tab` is the tab itself, not its page.
    if (tab)
        tab = materialize_tab(tab);
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
    else if (tab == options_tab_)
        options_tab_->set_symbol(s);
    else if (tab == analyst_tab_)
        analyst_tab_->set_symbol(s);
    else if (tab == ownership_tab_)
        ownership_tab_->set_symbol(s);
    else if (tab == dividends_tab_)
        dividends_tab_->set_symbol(s);
    else if (tab == earnings_tab_)
        earnings_tab_->set_symbol(s);
}

void EquityResearchScreen::load_symbol(const QString& requested, bool publish_to_group, bool allow_resolve) {
    // Text resolved earlier this session (RELIANCE → RELIANCE.NS) goes straight to its Yahoo
    // form: no doomed fetch first, and no new lookup when a linked panel echoes the bare form.
    QString symbol = requested;
    QString resolved_from;
    if (const auto it = resolved_aliases_.constFind(requested); it != resolved_aliases_.constEnd()) {
        resolved_from = requested;
        symbol = it.value();
    }
    if (symbol.isEmpty() || symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    last_price_ = 0.0; // stale until the new symbol's quote arrives

    ++load_seq_; // any lookup still running belongs to the previous entry
    resolve_allowed_ = allow_resolve;
    resolve_tried_for_.clear();
    symbol_not_found_ = false;
    resolved_from_ = resolved_from;
    not_found_candidates_.clear();
    if (suggest_btn_)
        suggest_btn_->setVisible(false);
    update_hint();

    symbol_label_->setText(symbol);
    sym_label_->setText(symbol);
    price_label_->setText(tr("Loading…"));
    set_price_tone(QString(), QString());
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

    // A resolved symbol is a translated form (like the ".NS" mapping in
    // on_group_symbol_changed): the group keeps the text the user typed.
    if (publish_to_group && link_group_ != SymbolGroup::None)
        SymbolContext::instance().set_group_symbol(
            link_group_, SymbolRef::equity(resolved_from.isEmpty() ? symbol : resolved_from), this);
    ScreenStateManager::instance().notify_changed(this);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Failures + symbol resolution
// ═══════════════════════════════════════════════════════════════════════════════
void EquityResearchScreen::on_load_failed(const services::equity::LoadFailure& f) {
    if (f.symbol != current_symbol_)
        return; // a late failure for a symbol the user already left
    const bool have_quote = last_quote_.symbol == current_symbol_;

    if (f.leg == QLatin1String("Quote") && price_label_) {
        if (have_quote) {
            // A background poll failed while a good price is on screen: keep it, mark it stale.
            set_price_tone(QStringLiteral("stale"),
                           tr("Quote refresh failed at %1 — showing the last price.\n%2")
                               .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), f.message));
        } else if (!symbol_not_found_) {
            price_label_->setText(er_short_failure(f.code));
            set_price_tone(QStringLiteral("error"), f.message);
        }
    }

    // Resolve what the user typed: once per load, never for a resolved reload, never once a
    // quote proved the symbol exists, and never for a transport failure (the lookup would
    // fail the same way).
    if (!resolve_allowed_ || resolve_tried_for_ == current_symbol_ || have_quote)
        return;
    if (f.leg != QLatin1String("Quote") && f.leg != QLatin1String("Info") && f.leg != QLatin1String("Historical"))
        return;
    const bool not_found = f.code == QLatin1String("not_found");
    const bool transport = f.code == QLatin1String("network") || f.code == QLatin1String("rate_limited");
    const bool bare = !current_symbol_.contains(QLatin1Char('.')); // no exchange suffix
    if (not_found || (f.leg == QLatin1String("Quote") && bare && !transport))
        start_resolve(current_symbol_);
}

void EquityResearchScreen::start_resolve(const QString& query) {
    resolve_tried_for_ = query;
    const quint64 seq = load_seq_;
    if (hint_label_)
        hint_label_->setText(tr("Looking up “%1” on Yahoo Finance…").arg(query));
    QPointer<EquityResearchScreen> self = this;
    services::equity::EquityResearchService::instance().resolve_symbol(
        query, [self, seq, query](bool ok, services::equity::SymbolResolution res, QString error) {
            Q_UNUSED(error); // the load failure already on screen says what went wrong
            if (!self)
                return;
            // The user entered another symbol while this ran — the answer is for nothing on screen.
            if (seq != self->load_seq_ || self->current_symbol_ != query)
                return;
            self->update_hint();
            if (!ok)
                return; // the lookup itself failed: keep the load error that is showing
            if (self->last_quote_.symbol == query)
                return; // a quote for the typed symbol arrived meanwhile — it exists as typed
            if (res.found && res.resolved_symbol.compare(query, Qt::CaseInsensitive) != 0) {
                // RELIANCE → RELIANCE.NS: reload transparently under the Yahoo symbol. The alias
                // makes load_symbol() swap it in (and record where it came from for the header).
                self->resolved_aliases_.insert(query, res.resolved_symbol);
                self->load_symbol(query, /*publish_to_group=*/false, /*allow_resolve=*/false);
                return;
            }
            if (res.found)
                return; // Yahoo knows it exactly as typed: the failure was something else
            self->show_symbol_not_found(res);
        });
}

void EquityResearchScreen::show_symbol_not_found(const services::equity::SymbolResolution& res) {
    const QString& query = res.query;
    symbol_not_found_ = true;
    not_found_candidates_.clear();
    for (const auto& c : res.candidates) {
        if (c.symbol.compare(query, Qt::CaseInsensitive) == 0)
            continue;
        QString text = c.symbol;
        if (!c.name.isEmpty())
            text += QStringLiteral("  —  ") + c.name;
        if (!c.exchange.isEmpty())
            text += QStringLiteral("  (") + c.exchange + QLatin1Char(')');
        not_found_candidates_.append({c.symbol, text});
    }

    price_label_->setText(tr("Symbol not found"));
    QString tip = not_found_candidates_.isEmpty()
                      ? tr("Yahoo Finance has no listing for “%1”. Try the Yahoo form, e.g. RELIANCE.NS or BRK-B.")
                            .arg(query)
                      : tr("Yahoo Finance has no listing for “%1”. Pick a suggestion from DID YOU MEAN? ▾").arg(query);
    if (!res.message.isEmpty())
        tip += QLatin1Char('\n') + res.message;
    set_price_tone(QStringLiteral("error"), tip);
    if (suggest_btn_)
        suggest_btn_->setVisible(!not_found_candidates_.isEmpty());

    // The user is still at the symbol box (pressed Enter and has not started typing again):
    // open the suggestions for them. Otherwise the button waits until they look.
    if (!not_found_candidates_.isEmpty() && isVisible() && window()->isActiveWindow() && symbol_edit_ &&
        symbol_edit_->hasFocus() && symbol_edit_->text().isEmpty())
        show_candidates_menu(symbol_edit_);
}

void EquityResearchScreen::show_candidates_menu(QWidget* anchor) {
    if (not_found_candidates_.isEmpty() || !anchor)
        return;
    // Heap + popup(): this can open from an async callback, where exec()'s nested loop is unwelcome.
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    for (const auto& c : not_found_candidates_) {
        const QString sym = c.first;
        connect(menu->addAction(c.second), &QAction::triggered, this, [this, sym]() { load_symbol(sym); });
    }
    menu->popup(anchor->mapToGlobal(QPoint(0, anchor->height())));
}

void EquityResearchScreen::set_price_tone(const QString& tone, const QString& tooltip) {
    if (!price_label_)
        return;
    price_label_->setToolTip(tooltip);
    if (price_label_->property("tone").toString() != tone) {
        price_label_->setProperty("tone", tone);
        er_repolish(price_label_);
    }
}

void EquityResearchScreen::update_hint() {
    if (!hint_label_)
        return;
    if (!resolved_from_.isEmpty()) {
        hint_label_->setText(tr("“%1” resolved to %2").arg(resolved_from_, current_symbol_));
        hint_label_->setToolTip(tr("Yahoo Finance lists “%1” as %2").arg(resolved_from_, current_symbol_));
    } else {
        hint_label_->setText(tr("Use /stock, /fund, /index... in command bar to search"));
        hint_label_->setToolTip(QString());
    }
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
    // Echo guard: after this screen publishes NBIX to its link group, a linked Equity
    // Trading panel adopts it and re-broadcasts it tagged with its own default exchange
    // (NSE), which mapped NBIX to NBIX.NS and swapped the listing the user had opened.
    // The same bare ticker coming back with an exchange is that echo, not a new request.
    if (!current_symbol_.isEmpty() && mapped != current_symbol_ &&
        ref.symbol.trimmed().compare(current_symbol_, Qt::CaseInsensitive) == 0)
        return;
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

    // The instrument's own currency as quoted (GBp → "116.95p", JPY → "¥3,418"); no symbol
    // until the info names it, rather than a "$" that may be wrong.
    const QString ccy = current_currency_;
    const QString kDash = equity_fmt::dash();

    sym_label_->setText(q.symbol);
    price_label_->setText(q.price > 0.0 ? equity_fmt::price(q.price, ccy) : kDash);
    set_price_tone(QString(), QString()); // a fresh quote clears any stale / error marking

    if (q.price > 0.0) {
        // An unchanged price (RELIANCE.NS before the open: 0.00, 0.00%) is neither up nor
        // down: no arrow and no direction colour — it used to read "▲0.00%" in green.
        const bool flat = q.change == 0.0 && q.change_pct == 0.0;
        const bool up = q.change >= 0.0;
        ui::formatting::InstrumentFmt inst;
        inst.currency = ccy;
        change_label_->setText(QStringLiteral("%1  %2%3")
                                   .arg(ui::formatting::price_change(q.change, q.price, inst),
                                        flat ? QString() : up ? QStringLiteral("▲") : QStringLiteral("▼"),
                                        ui::formatting::pct(std::fabs(q.change_pct), 2, false)));
        const QString tone = flat ? QStringLiteral("flat") : up ? QStringLiteral("up") : QStringLiteral("down");
        if (change_label_->property("tone").toString() != tone) {
            change_label_->setProperty("tone", tone);
            er_repolish(change_label_);
        }
    } else {
        change_label_->setText(kDash);
    }

    update_quote_details(q);
}

void EquityResearchScreen::update_quote_details(const services::equity::QuoteData& q) {
    if (!vol_label_ || !hl_label_)
        return;
    const QString ccy = current_currency_;
    const QString kDash = equity_fmt::dash();
    // Zero = the source sent nothing (halted/thin instruments, indices) — a dash, not "VOL: 0".
    vol_label_->setText(tr("VOL: %1").arg(q.volume > 0.0 ? ui::formatting::volume(q.volume) : kDash));
    hl_label_->setText(q.high > 0.0 && q.low > 0.0
                           ? tr("H:%1  L:%2").arg(equity_fmt::price(q.high, ccy), equity_fmt::price(q.low, ccy))
                           : tr("H/L: %1").arg(kDash));
}

void EquityResearchScreen::reset_quote_bar() {
    const QString dash = equity_fmt::dash();
    last_quote_ = {};
    last_info_ = {};
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
    dlg->setStyleSheet(QString("QLabel#erCsvNote { color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; }"
                               "QLabel#erCsvStatus { color:%2; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
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
    if (suggest_btn_) {
        suggest_btn_->setText(tr("DID YOU MEAN? ▾"));
        suggest_btn_->setToolTip(tr("Yahoo Finance listings that match what you typed"));
    }
    update_hint();

    // Re-render the loaded symbol's VOL / H-L / MKT CAP / REC in the new language — they used
    // to be blanked to "—" (MKT CAP until the symbol was reloaded). Dashes only when nothing
    // is loaded for the current symbol.
    if (!current_symbol_.isEmpty() && last_quote_.symbol == current_symbol_) {
        update_quote_details(last_quote_);
    } else {
        if (vol_label_)
            vol_label_->setText(tr("VOL: %1").arg(equity_fmt::dash()));
        if (hl_label_)
            hl_label_->setText(tr("H/L: %1").arg(equity_fmt::dash()));
    }
    if (!current_symbol_.isEmpty() && last_info_.symbol == current_symbol_)
        update_info_labels(last_info_);
    else if (mktcap_label_)
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
    last_financials_ = data; // for a Valuation tab built later (materialize_tab)
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

// ═══════════════════════════════════════════════════════════════════════════════
// Deep links (ISectionLinked) + the search box's rows
// ═══════════════════════════════════════════════════════════════════════════════

/// A Financials part ("income", "balance_sheet", "cash flow" …) as EquityFinancialsTab::show_section
/// names it; empty when it is none of them.
static QString er_fin_part(const QString& text) {
    QString k = text.trimmed().toLower();
    k.replace(QLatin1Char(' '), QLatin1Char('_'));
    k.replace(QLatin1Char('-'), QLatin1Char('_'));
    if (k == QLatin1String("overview") || k == QLatin1String("summary"))
        return QStringLiteral("overview");
    if (k == QLatin1String("income") || k == QLatin1String("income_statement") || k == QLatin1String("p&l"))
        return QStringLiteral("income");
    if (k == QLatin1String("balance") || k == QLatin1String("balance_sheet"))
        return QStringLiteral("balance");
    if (k == QLatin1String("cashflow") || k == QLatin1String("cash_flow") || k == QLatin1String("cash_flow_statement"))
        return QStringLiteral("cashflow");
    if (k == QLatin1String("ratios"))
        return QStringLiteral("ratios");
    if (k == QLatin1String("peers"))
        return QStringLiteral("peers");
    return {};
}

bool EquityResearchScreen::open_section(const QString& section, const QString& focus) {
    QString key = section.trimmed().toLower();
    if (key.isEmpty())
        return true; // symbols arrive through nav.open_symbol (IGroupLinked)
    if (key.startsWith(QLatin1String("tab:")))
        key = key.mid(4);
    QString part;
    int cut = key.indexOf(QLatin1Char('/'));
    if (cut < 0)
        cut = key.indexOf(QLatin1Char(':'));
    if (cut >= 0) {
        part = er_fin_part(key.mid(cut + 1));
        key = key.left(cut);
        if (part.isEmpty() || key != QLatin1String("financials"))
            return false; // an unknown part: nothing changes
    } else if (key == QLatin1String("financials")) {
        part = er_fin_part(focus); // optional; anything else is ignored
    }
    const QByteArray k = key.toLatin1();
    QWidget* page = tab_page(k.constData());
    if (!page || !tab_widget_)
        return false;
    tab_widget_->setCurrentWidget(page); // currentChanged -> on_tab_changed: as a click
    if (!part.isEmpty() && financials_tab_)
        financials_tab_->show_section(part);
    return true;
}

namespace {

QList<command::CommandItem> er_command_rows() {
    namespace rows = command_rows;
    const QString screen = QStringLiteral("equity_research");
    const QString where = EquityResearchScreen::tr("Equity Research");
    struct Row {
        const char* section;
        const char* title;
        const char* keywords;
    };
    static const Row kRows[] = {
        {"overview", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Company overview"),
         "overview, chart, quote, profile, company"},
        {"financials", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Financial statements"),
         "financials, fundamentals, statements, annual report"},
        {"filings", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Company filings"),
         "filings, sec, 10-k, 10-q, 8-k, edgar, annual report"},
        {"analysis", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Fundamental analysis"),
         "analysis, ratios, quality, scores, piotroski, altman"},
        {"technicals", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Technical analysis"),
         "technicals, indicators, rsi, macd, moving averages, signals"},
        {"peers", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Peer comparison"),
         "peers, comparables, comps, compare companies, competitors"},
        {"news", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Company news"),
         "news, headlines, stock news"},
        {"sentiment", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Stock sentiment"),
         "sentiment, social, mood, bullish, bearish"},
        {"valuation", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Stock valuation (DCF)"),
         "valuation, dcf, discounted cash flow, fair value, intrinsic value"},
        {"dividends", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Dividends"),
         "dividends, dividend yield, payout, ex-dividend"},
        {"earnings", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Earnings history"),
         "earnings, eps, surprises, estimates, quarterly results"},
        {"options", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Stock options chain"),
         "options, option chain, calls, puts, implied volatility"},
        {"analyst", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Analyst ratings"),
         "analyst, ratings, price targets, recommendations, upgrades, downgrades"},
        {"ownership", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Ownership"),
         "ownership, institutional holders, insiders, insider trading, major holders"},
        {"financials/income", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Income statement"),
         "income statement, p&l, profit and loss, revenue, net income, financials"},
        {"financials/balance", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Balance sheet"),
         "balance sheet, assets, liabilities, equity, debt, financials"},
        {"financials/cashflow", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Cash flow statement"),
         "cash flow, free cash flow, fcf, operating cash flow, capex, financials"},
        {"financials/ratios", QT_TRANSLATE_NOOP("fincept::screens::EquityResearchScreen", "Financial ratios"),
         "ratios, margins, roe, roic, leverage, liquidity, financials"},
    };
    QList<command::CommandItem> out;
    for (const Row& r : kRows) {
        QStringList kw = QString::fromLatin1(r.keywords).split(QStringLiteral(", "), Qt::SkipEmptyParts);
        kw << QStringLiteral("research") << QStringLiteral("stock");
        out.append(rows::section(screen, QString::fromLatin1(r.section),
                                 QCoreApplication::translate("fincept::screens::EquityResearchScreen", r.title), kw,
                                 where));
    }
    return out;
}

void er_install_commands() {
    command_rows::install(QStringLiteral("equity_research"), &er_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(er_install_commands)

} // namespace fincept::screens
