// Documentation Screen — comprehensive in-app terminal guide
#include "screens/docs/DocsScreen.h"

#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QScrollArea>
#include <QSplitter>

namespace fincept::screens {

// ── Style constants ─────────────────────────────────────────────────────────

static const char* FONT = "'Consolas','Courier New',monospace";

static QString SIDEBAR_SS() {
    return QString("QTreeWidget { background: %1; color: %2; border: none;"
                   "  font-size: 12px; font-family: 'Consolas','Courier New',monospace;"
                   "  outline: none; }"
                   "QTreeWidget::item { height: 26px; padding-left: 6px; border: none; }"
                   "QTreeWidget::item:hover { background: %3; color: %4; }"
                   "QTreeWidget::item:selected { background: %5; color: %6; }"
                   "QTreeWidget::branch { background: %1; }"
                   "QTreeWidget::branch:hover { background: %3; }"
                   "QTreeWidget::branch:has-children:!has-siblings:closed,"
                   "QTreeWidget::branch:closed:has-children:has-siblings"
                   " { border-image: none; image: none; }"
                   "QTreeWidget::branch:open:has-children:!has-siblings,"
                   "QTreeWidget::branch:open:has-children:has-siblings"
                   " { border-image: none; image: none; }"
                   "QScrollBar:vertical { width: 5px; background: transparent; }"
                   "QScrollBar::handle:vertical { background: %7; }"
                   "QScrollBar::handle:vertical:hover { background: %8; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
             ui::colors::BG_HOVER(), ui::colors::AMBER(), ui::colors::BORDER_MED(), ui::colors::BORDER_BRIGHT());
}

static QString SCROLL_SS() {
    return QString("QScrollArea { border: none; background: transparent; }"
                   "QScrollBar:vertical { width: 5px; background: transparent; }"
                   "QScrollBar::handle:vertical { background: %1; }"
                   "QScrollBar::handle:vertical:hover { background: %2; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(ui::colors::BORDER_MED(), ui::colors::BORDER_BRIGHT());
}

// ============================================================================
// Helpers
// ============================================================================

QLabel* DocsScreen::make_heading(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold; letter-spacing: 0.5px;"
                               " background: transparent; font-family: 'Consolas','Courier New',monospace;"
                               " padding: 4px 0;")
                           .arg(ui::colors::AMBER()));
    lbl->setWordWrap(true);
    return lbl;
}

QLabel* DocsScreen::make_body_label(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent; line-height: 1.5;"
                               " font-family: 'Consolas','Courier New',monospace;")
                           .arg(ui::colors::TEXT_PRIMARY()));
    lbl->setWordWrap(true);
    return lbl;
}

QLabel* DocsScreen::make_muted_label(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;"
                               " font-family: 'Consolas','Courier New',monospace;")
                           .arg(ui::colors::TEXT_SECONDARY()));
    lbl->setWordWrap(true);
    return lbl;
}

QWidget* DocsScreen::make_section_panel(const QString& icon, const QString& title, const QString& body,
                                        const QString& accent_color) {
    auto* panel = new QWidget(this);
    panel->setStyleSheet(
        QString("background: %1; border: 1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* hdr = new QLabel(QString("%1  %2").arg(icon, title));
    hdr->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; letter-spacing: 0.5px;"
                               " background: %2; padding: 8px 12px; border-bottom: 1px solid %3;"
                               " font-family: 'Consolas','Courier New',monospace;")
                           .arg(accent_color, ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    vl->addWidget(hdr);

    // Body
    auto* content = new QLabel(body);
    content->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent; padding: 10px 12px;"
                                   " font-family: 'Consolas','Courier New',monospace; line-height: 1.6;")
                               .arg(ui::colors::TEXT_PRIMARY()));
    content->setWordWrap(true);
    vl->addWidget(content);

    return panel;
}

QWidget* DocsScreen::make_skill_panel(const QString& beginner, const QString& intermediate, const QString& advanced,
                                      const QString& pro) {
    auto* panel = new QWidget(this);
    panel->setStyleSheet(
        QString("background: %1; border: 1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QLabel("SKILL LEVELS");
    hdr->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; letter-spacing: 0.5px;"
                               " background: %2; padding: 8px 12px; border-bottom: 1px solid %3;"
                               " font-family: 'Consolas','Courier New',monospace;")
                           .arg(ui::colors::AMBER(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    vl->addWidget(hdr);

    struct Level {
        const char* tag;
        const char* color;
        const QString& text;
    };
    Level levels[] = {
        {"BEGINNER", ui::colors::POSITIVE(), beginner},
        {"INTERMEDIATE", ui::colors::INFO(), intermediate},
        {"ADVANCED", ui::colors::AMBER(), advanced},
        {"PRO", ui::colors::NEGATIVE(), pro},
    };

    for (const auto& lvl : levels) {
        auto* row = new QWidget(this);
        row->setStyleSheet(QString("background: transparent; border-bottom: 1px solid %1;").arg(ui::colors::BG_RAISED()));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(12, 8, 12, 8);
        rl->setSpacing(10);

        auto* badge = new QLabel(lvl.tag);
        badge->setFixedWidth(105);
        badge->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;"
                                     " letter-spacing: 0.5px; font-family: 'Consolas','Courier New',monospace;")
                                 .arg(lvl.color));
        rl->addWidget(badge);

        auto* desc = new QLabel(lvl.text);
        desc->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;"
                                    " font-family: 'Consolas','Courier New',monospace;")
                                .arg(ui::colors::TEXT_PRIMARY()));
        desc->setWordWrap(true);
        rl->addWidget(desc, 1);

        vl->addWidget(row);
    }
    return panel;
}

QWidget* DocsScreen::make_tip_box(const QString& text, const QString& color) {
    auto* box = new QLabel(text);
    box->setStyleSheet(QString("color: %1; font-size: 11px; background: rgba(%2, 0.08);"
                               " border: 1px solid rgba(%2, 0.3); padding: 8px 12px;"
                               " font-family: 'Consolas','Courier New',monospace;")
                           .arg(color, color));
    box->setWordWrap(true);
    return box;
}

QWidget* DocsScreen::make_page(const QString& title, const QString& subtitle,
                               const std::vector<std::pair<QString, QString>>& sections) {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(SCROLL_SS());

    auto* page = new QWidget(this);
    page->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(20, 16, 20, 20);
    vl->setSpacing(12);

    // Page title
    vl->addWidget(make_heading(title));
    if (!subtitle.isEmpty()) {
        vl->addWidget(make_muted_label(subtitle));
    }

    // Separator
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color: %1;").arg(ui::colors::BORDER_DIM()));
    vl->addWidget(sep);

    // Sections
    for (const auto& [heading, body] : sections) {
        vl->addWidget(make_section_panel("■", heading, body, ui::colors::AMBER));
    }

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

// ============================================================================
// Page builders — Welcome & Getting Started
// ============================================================================

QWidget* DocsScreen::page_welcome() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(SCROLL_SS());

    auto* page = new QWidget(this);
    page->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(20, 16, 20, 20);
    vl->setSpacing(12);

    vl->addWidget(make_heading("FINCEPT TERMINAL  —  DOCUMENTATION"));
    vl->addWidget(make_muted_label("v4.0.0  |  Native C++ Financial Intelligence Terminal"));

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color: %1;").arg(ui::colors::BORDER_DIM()));
    vl->addWidget(sep);

    vl->addWidget(
        make_section_panel("■", "WHAT IS FINCEPT TERMINAL?",
                           "Fincept Terminal is a professional-grade desktop financial intelligence platform "
                           "built in native C++ with Qt6. It provides institutional-quality market data, "
                           "trading capabilities, quantitative research tools, and AI-powered analytics — "
                           "all in a single unified terminal interface.\n\n"
                           "With 45+ integrated screens, real-time WebSocket feeds, embedded Python analytics, "
                           "and support for 15+ broker integrations, Fincept Terminal bridges the gap between "
                           "retail and institutional tooling.",
                           ui::colors::AMBER));

    vl->addWidget(make_section_panel("■", "KEY CAPABILITIES",
                                     "■  Real-time market data across equities, crypto, forex, commodities\n"
                                     "■  Multi-exchange crypto trading (Kraken, HyperLiquid, Binance, etc.)\n"
                                     "■  Paper trading engine with simulated order matching\n"
                                     "■  100+ Python analytics scripts (CFA-level: equity, portfolio, derivatives)\n"
                                     "■  18-module QuantLib quantitative analysis suite (590+ endpoints)\n"
                                     "■  AI Quant Lab with ML models, factor discovery, HFT, RL trading\n"
                                     "■  Multiple AI agent frameworks (Geopolitics, Economic, Hedge Fund)\n"
                                     "■  Visual node editor for workflow automation\n"
                                     "■  DBnomics access to 100+ data providers, 500K+ datasets\n"
                                     "■  Surface analytics for derivatives, fixed income, credit, risk\n"
                                     "■  Report builder with drag-and-drop components\n"
                                     "■  Backtesting with 6 providers and 50+ strategies\n"
                                     "■  Algorithmic trading with strategy builder and scanner",
                                     ui::colors::POSITIVE));

    vl->addWidget(make_section_panel("■", "WHO IS THIS FOR?",
                                     "■  Retail traders seeking institutional-quality tools\n"
                                     "■  Quantitative researchers and data scientists\n"
                                     "■  Portfolio managers and financial analysts\n"
                                     "■  Finance students learning markets and analytics\n"
                                     "■  Algorithmic trading developers\n"
                                     "■  Crypto traders needing multi-exchange access\n"
                                     "■  Economics researchers working with global datasets",
                                     ui::colors::INFO));

    vl->addWidget(make_section_panel("■", "NAVIGATION",
                                     "Use the sidebar on the left to browse documentation by category. "
                                     "Each section covers a terminal feature with:\n\n"
                                     "■  Overview — what the feature does\n"
                                     "■  Key Features — detailed capabilities\n"
                                     "■  Real-World Usage — practical applications\n"
                                     "■  Skill Levels — Beginner through Pro guidance",
                                     ui::colors::TEXT_SECONDARY));

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* DocsScreen::page_getting_started() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(SCROLL_SS());

    auto* page = new QWidget(this);
    page->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(20, 16, 20, 20);
    vl->setSpacing(12);

    vl->addWidget(make_heading("GETTING STARTED"));
    vl->addWidget(make_muted_label("Your first steps with Fincept Terminal"));

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color: %1;").arg(ui::colors::BORDER_DIM()));
    vl->addWidget(sep);

    vl->addWidget(make_section_panel("1", "LAUNCH & LOGIN",
                                     "When you first launch Fincept Terminal, you'll see the login screen. "
                                     "You can either:\n\n"
                                     "■  Register a new account with email and password\n"
                                     "■  Continue as Guest (limited features)\n"
                                     "■  Log in with existing credentials\n\n"
                                     "After login, you'll land on the Dashboard — your home base.",
                                     ui::colors::POSITIVE));

    vl->addWidget(make_section_panel("2", "THE INTERFACE",
                                     "The terminal has four main zones:\n\n"
                                     "TOOLBAR (top) — File, Navigate, View, Help menus + session info\n"
                                     "TAB BAR — 14 primary tabs: Dashboard, Markets, Crypto, Portfolio, etc.\n"
                                     "CONTENT AREA — The active screen fills this zone\n"
                                     "STATUS BAR (bottom) — Version, market indicators, connection status\n\n"
                                     "Use the Navigate menu (in toolbar) to access 30+ additional screens "
                                     "organized by category: Markets & Data, Trading, Research, Tools, etc.",
                                     ui::colors::INFO));

    vl->addWidget(make_section_panel("3", "KEYBOARD SHORTCUTS",
                                     "F11  — Toggle fullscreen\n"
                                     "F10  — Focus mode (hide tab/status bars for maximum screen space)\n"
                                     "F5   — Refresh current screen\n"
                                     "Ctrl+P — Take screenshot (saved to home directory)",
                                     ui::colors::AMBER));

    vl->addWidget(make_section_panel("4", "SUBSCRIPTION PLANS",
                                     "Fincept Terminal offers tiered access:\n\n"
                                     "■  FREE — Basic market data, limited screens, paper trading\n"
                                     "■  PRO — Full market data, all screens, real trading, AI chat\n"
                                     "■  ENTERPRISE — Everything + API access, priority support\n\n"
                                     "Manage your plan from Settings or the Pricing screen.",
                                     ui::colors::AMBER));

    vl->addWidget(
        make_skill_panel("Explore the Dashboard, set up a watchlist, browse market data",
                         "Configure broker connections, set up paper trading, explore analytics",
                         "Deploy algo strategies, use QuantLib suite, build custom workflows",
                         "Multi-agent AI systems, custom MCP servers, HFT backtesting, node editor automation"));

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* DocsScreen::page_keyboard_shortcuts() {
    return make_page("KEYBOARD SHORTCUTS", "Global shortcuts and navigation keys",
                     {
                         {"GLOBAL SHORTCUTS", "F11  — Toggle fullscreen mode\n"
                                              "F10  — Toggle focus mode (hides tab bar and status bar)\n"
                                              "F5   — Refresh the current screen data\n"
                                              "Ctrl+P — Capture screenshot (saved to ~/FinceptScreenshot_*.png)"},
                         {"FILE MENU", "New Workspace — Create a fresh workspace layout\n"
                                       "Open Workspace — Load a saved workspace\n"
                                       "Save Workspace — Persist current layout\n"
                                       "Import Data — Import external data files\n"
                                       "Export Data — Export current view data\n"
                                       "Refresh All — Refresh all active data feeds"},
                         {"NAVIGATE MENU", "Access 30+ screens organized in sub-menus:\n"
                                           "■  Markets & Data — Screener, Economics, DBnomics, AkShare, Gov Data\n"
                                           "■  Trading & Portfolio — Equity Trading, Derivatives, Watchlist\n"
                                           "■  Research — Equity Research, M&A, Geopolitics, Surface Analytics\n"
                                           "■  Tools — Report Builder, Node Editor, Code Editor, Excel, Notes"},
                         {"VIEW MENU", "Fullscreen (F11) — Use full monitor space\n"
                                       "Focus Mode (F10) — Hide chrome for maximum content area\n"
                                       "Refresh (F5) — Reload current screen\n"
                                       "Screenshot (Ctrl+P) — Capture to file"},
                     });
}

// ============================================================================
// Core Screens
// ============================================================================

QWidget* DocsScreen::page_dashboard() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(SCROLL_SS());

    auto* page = new QWidget(this);
    page->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(20, 16, 20, 20);
    vl->setSpacing(12);

    vl->addWidget(make_heading("DASHBOARD"));
    vl->addWidget(make_muted_label("Your home base — customizable widget grid with live market data"));

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color: %1;").arg(ui::colors::BORDER_DIM()));
    vl->addWidget(sep);

    vl->addWidget(
        make_section_panel("■", "OVERVIEW",
                           "The Dashboard is your primary workspace. It features a draggable widget grid "
                           "where you can arrange market widgets, a scrolling ticker bar showing live prices, "
                           "a market pulse panel with sector performance, and a status bar showing connection state.",
                           ui::colors::AMBER));

    vl->addWidget(make_section_panel("■", "AVAILABLE WIDGETS",
                                     "■  Stock Quote — Real-time price, change, volume for any symbol\n"
                                     "■  Watchlist — Your tracked symbols with live updates\n"
                                     "■  Top Movers — Biggest gainers and losers of the session\n"
                                     "■  Market Sentiment — Bull/bear indicators and fear/greed index\n"
                                     "■  News — Latest headlines with sentiment tagging\n"
                                     "■  Economic Calendar — Upcoming economic events and releases\n"
                                     "■  Sector Heatmap — Visual sector performance map\n"
                                     "■  Performance — Portfolio return tracking\n"
                                     "■  Risk Metrics — VaR, Sharpe, beta, drawdown indicators\n"
                                     "■  Screener — Quick stock screener with filters\n"
                                     "■  Quote Table — Multi-symbol comparison table\n"
                                     "■  Quick Trade — One-click trade entry\n"
                                     "■  Indices — Major index tracking (S&P 500, NASDAQ, DOW)\n"
                                     "■  Forex — Currency pair rates\n"
                                     "■  Crypto — Top cryptocurrency prices\n"
                                     "■  Commodities — Gold, oil, silver, natural gas\n"
                                     "■  Portfolio Summary — Holdings overview with allocation",
                                     ui::colors::POSITIVE));

    vl->addWidget(
        make_section_panel("■", "REAL-WORLD USAGE",
                           "■  Morning routine: Check top movers, review overnight news, scan economic calendar\n"
                           "■  Active trading: Pin stock quote + quick trade widgets, monitor watchlist\n"
                           "■  Portfolio management: Use portfolio summary + risk metrics + performance\n"
                           "■  Sector rotation: Combine sector heatmap + top movers + indices",
                           ui::colors::INFO));

    vl->addWidget(make_skill_panel(
        "Start with default layout. Add a Stock Quote widget for a symbol you follow. Watch the ticker bar.",
        "Customize your grid layout. Add multiple watchlists for different sectors. Use the market pulse panel.",
        "Build specialized layouts for different strategies (day trading vs swing). Use risk metrics + performance "
        "together.",
        "Create multi-monitor layouts. Combine dashboard with algo trading feeds. Use economic calendar for "
        "event-driven setups."));

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* DocsScreen::page_markets() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(SCROLL_SS());

    auto* page = new QWidget(this);
    page->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(20, 16, 20, 20);
    vl->setSpacing(12);

    vl->addWidget(make_heading("MARKETS"));
    vl->addWidget(make_muted_label("Global and regional market overview with auto-refresh"));

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color: %1;").arg(ui::colors::BORDER_DIM()));
    vl->addWidget(sep);

    vl->addWidget(make_section_panel("■", "OVERVIEW",
                                     "The Markets screen provides a comprehensive view of global markets organized "
                                     "by region. It displays indices, equities, forex, commodities, and crypto in "
                                     "panel-based layouts with configurable auto-refresh (default: 10 minutes).",
                                     ui::colors::AMBER));

    vl->addWidget(make_section_panel("■", "KEY FEATURES",
                                     "■  Regional panels — US, Europe, Asia, Global\n"
                                     "■  Auto-refresh with configurable interval\n"
                                     "■  Price, change, % change with color coding (green/red)\n"
                                     "■  Market hours and session status indicators\n"
                                     "■  Sort by name, price, change, or volume\n"
                                     "■  Click any instrument to navigate to detailed view",
                                     ui::colors::POSITIVE));

    vl->addWidget(make_section_panel("■", "REAL-WORLD USAGE",
                                     "■  Pre-market: Scan global indices to gauge overnight sentiment\n"
                                     "■  Cross-market analysis: Compare US vs Europe vs Asia performance\n"
                                     "■  Commodity tracking: Monitor gold, oil, natural gas alongside equities\n"
                                     "■  FX correlation: Watch currency pairs relative to equity moves",
                                     ui::colors::INFO));

    vl->addWidget(
        make_skill_panel("Browse the default view. Learn to read green (up) and red (down) color coding.",
                         "Compare multiple regions. Notice correlations between indices and currencies.",
                         "Use markets as a macro overlay for your trading decisions. Track relative strength.",
                         "Combine with economics data and geopolitics for macro-driven trading strategies."));

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* DocsScreen::page_news() {
    return make_page(
        "NEWS", "Real-time news with clustering, sentiment analysis, and deviation monitoring",
        {
            {"OVERVIEW", "The News screen aggregates financial news from multiple sources with intelligent "
                         "clustering, sentiment analysis, and customizable keyword monitors that alert you "
                         "to significant deviations from baseline coverage patterns."},
            {"KEY FEATURES", "■  Category filtering — All, Markets, Economy, Tech, Crypto, Geopolitics\n"
                             "■  Time range selection — 1H, 4H, 24H, 7D, 30D\n"
                             "■  Sort by recency, relevance, or sentiment score\n"
                             "■  News clustering — related articles grouped together\n"
                             "■  Sentiment analysis — bullish/bearish/neutral tagging\n"
                             "■  Keyword monitors — set alerts for specific topics\n"
                             "■  Deviation detection — flags unusual coverage patterns\n"
                             "■  Full article reader with clean formatting"},
            {"REAL-WORLD USAGE", "■  Event-driven trading: Monitor breaking news for tradable events\n"
                                 "■  Sentiment tracking: Gauge market mood before making positions\n"
                                 "■  Due diligence: Research a company before investing\n"
                                 "■  Macro awareness: Track central bank news, policy changes"},
            {"SKILL LEVELS", "BEGINNER: Browse headlines, filter by category, read articles\n"
                             "INTERMEDIATE: Set up keyword monitors for your positions, use sentiment filters\n"
                             "ADVANCED: Use deviation alerts to catch unusual news patterns early\n"
                             "PRO: Combine news sentiment with quantitative signals for alpha generation"},
        });
}

QWidget* DocsScreen::page_watchlist() {
    return make_page("WATCHLIST", "Track your favorite instruments with live quotes",
                     {
                         {"OVERVIEW", "The Watchlist screen lets you create and manage multiple watchlists, each with "
                                      "live price quotes, change indicators, and quick access to detailed analysis."},
                         {"KEY FEATURES", "■  Multiple named watchlists (Favorites, Day Trades, Swing, etc.)\n"
                                          "■  Live price, change, % change, volume\n"
                                          "■  Add/remove symbols easily\n"
                                          "■  Color-coded performance (green/red)\n"
                                          "■  Click to navigate to detailed chart/analysis\n"
                                          "■  Persistent storage — watchlists saved between sessions"},
                         {"REAL-WORLD USAGE",
                          "■  Organize by strategy: separate watchlists for day trades, swing trades, long-term\n"
                          "■  Sector watchlists: group symbols by industry for sector rotation\n"
                          "■  Earnings watchlist: track companies reporting this week\n"
                          "■  Correlation pairs: group correlated instruments together"},
                         {"SKILL LEVELS", "BEGINNER: Create your first watchlist with 5-10 symbols you know\n"
                                          "INTERMEDIATE: Multiple watchlists organized by strategy or sector\n"
                                          "ADVANCED: Use watchlists as a pre-screened universe for your scanning\n"
                                          "PRO: Dynamic watchlists driven by screener output and quantitative filters"},
                     });
}

// ============================================================================
// Trading
// ============================================================================

QWidget* DocsScreen::page_crypto_trading() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(SCROLL_SS());

    auto* page = new QWidget(this);
    page->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(20, 16, 20, 20);
    vl->setSpacing(12);

    vl->addWidget(make_heading("CRYPTO TRADING"));
    vl->addWidget(make_muted_label("Multi-exchange crypto trading terminal"));

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color: %1;").arg(ui::colors::BORDER_DIM()));
    vl->addWidget(sep);

    vl->addWidget(make_section_panel("■", "OVERVIEW",
                                     "A full-featured crypto trading terminal supporting 10+ exchanges with real-time "
                                     "WebSocket feeds. Features OHLC charts, order book depth, order entry with market/"
                                     "limit/stop orders, watchlist, and a paper trading engine for risk-free practice.",
                                     ui::colors::AMBER));

    vl->addWidget(make_section_panel("■", "SUPPORTED EXCHANGES",
                                     "Kraken  |  Binance  |  Bybit  |  OKX  |  Coinbase  |  Bitget\n"
                                     "Gate  |  KuCoin  |  MEXC  |  HTX\n\n"
                                     "Each exchange supports: spot trading, real-time orderbook, OHLC candles, "
                                     "ticker data, and trade history via WebSocket.",
                                     ui::colors::POSITIVE));

    vl->addWidget(make_section_panel("■", "PANELS",
                                     "■  Command Bar — Exchange selector, symbol input, paper/live toggle, API config\n"
                                     "■  Ticker Bar — Real-time price, 24h change, high/low, volume\n"
                                     "■  Watchlist — 8+ default pairs with live prices\n"
                                     "■  OHLC Chart — Candlestick chart with multiple timeframes\n"
                                     "■  Order Entry — Market, Limit, Stop-Limit with SL/TP\n"
                                     "■  Order Book — Live bid/ask depth visualization\n"
                                     "■  Bottom Panel — Open orders, order history, positions, balances",
                                     ui::colors::INFO));

    vl->addWidget(make_section_panel("■", "PAPER TRADING",
                                     "Built-in paper trading engine with:\n"
                                     "■  Simulated order matching with realistic fills\n"
                                     "■  Virtual portfolio tracking (default $100,000 USDT)\n"
                                     "■  P&L tracking per position\n"
                                     "■  Order history persistence\n"
                                     "■  Switch between Paper and Live mode with one click",
                                     ui::colors::CYAN));

    vl->addWidget(make_skill_panel(
        "Start in Paper mode. Pick BTC/USDT, place a small market buy, watch it fill. Learn the order book.",
        "Try limit orders at support levels. Use stop-loss. Track P&L across multiple positions.",
        "Switch to Live mode with API keys. Use multiple exchanges. Implement SL/TP strategies.",
        "Run simultaneous positions across exchanges. Use order flow data for scalping. Combine with algo "
        "strategies."));

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* DocsScreen::page_paper_trading() {
    return make_page(
        "PAPER TRADING", "Risk-free simulated trading engine",
        {
            {"OVERVIEW", "The paper trading engine simulates real market execution with a virtual portfolio. "
                         "Available in Crypto Trading screen via the Paper/Live toggle. Your paper portfolio "
                         "starts with $100,000 USDT and supports all order types."},
            {"KEY FEATURES", "■  Realistic order matching with market simulation\n"
                             "■  Market, Limit, and Stop-Limit order types\n"
                             "■  Stop-loss and take-profit support\n"
                             "■  Per-position P&L tracking\n"
                             "■  Full order history with timestamps\n"
                             "■  Portfolio balance and equity tracking\n"
                             "■  Persistent state — resumes between sessions"},
            {"REAL-WORLD USAGE", "■  Strategy testing: Validate a new strategy without risking capital\n"
                                 "■  Learning: Understand order types, market mechanics, P&L calculation\n"
                                 "■  Confidence building: Track your paper P&L before going live\n"
                                 "■  Backtesting comparison: Compare paper results with backtest predictions"},
            {"SKILL LEVELS", "BEGINNER: Place market orders, understand fills, track basic P&L\n"
                             "INTERMEDIATE: Use limit orders at key levels, implement stop-losses\n"
                             "ADVANCED: Develop a consistent paper trading journal and strategy\n"
                             "PRO: Use paper trading for A/B testing new strategies before live deployment"},
        });
}

QWidget* DocsScreen::page_algo_trading() {
    return make_page(
        "ALGO TRADING", "Algorithmic strategy builder, scanner, and deployment",
        {
            {"OVERVIEW", "The Algo Trading screen provides a complete environment for building, testing, "
                         "and deploying algorithmic trading strategies. It includes a strategy builder, "
                         "market scanner, and deployment dashboard."},
            {"KEY FEATURES", "■  Strategy Builder — Define entry/exit rules, position sizing, risk limits\n"
                             "■  Market Scanner — Scan markets for strategy-matching conditions\n"
                             "■  Deployment Dashboard — Monitor live strategy performance\n"
                             "■  Strategy Library — 50+ pre-built strategies as templates\n"
                             "■  Risk Management — Max drawdown, position limits, daily loss limits\n"
                             "■  Real-time monitoring with P&L and trade logs"},
            {"REAL-WORLD USAGE", "■  Momentum strategies: Scan for breakouts, deploy auto-entry\n"
                                 "■  Mean reversion: Detect overbought/oversold conditions\n"
                                 "■  Pairs trading: Monitor spread and auto-execute when divergence detected\n"
                                 "■  Market making: Automated bid/ask placement with spread capture"},
            {"SKILL LEVELS", "BEGINNER: Explore pre-built strategies, understand entry/exit logic\n"
                             "INTERMEDIATE: Modify strategy parameters, run scanner on your watchlist\n"
                             "ADVANCED: Build custom strategies, combine multiple signals, set risk limits\n"
                             "PRO: Deploy multi-strategy portfolios, optimize parameters, run live with real capital"},
        });
}

QWidget* DocsScreen::page_backtesting() {
    return make_page(
        "BACKTESTING", "Historical strategy testing with 6 providers and 50+ strategies",
        {
            {"OVERVIEW", "The Backtesting screen lets you test trading strategies against historical data "
                         "using multiple backtesting providers. Supports 9 commands, 50+ built-in strategies, "
                         "and detailed performance analytics."},
            {"PROVIDERS", "■  Backtrader — Python-based, full-featured backtesting engine\n"
                          "■  Zipline — Quantopian's backtesting library\n"
                          "■  VectorBT — Vectorized backtesting for high performance\n"
                          "■  QuantConnect (Lean) — Cloud-based institutional backtesting\n"
                          "■  Custom Python — Write your own backtesting scripts\n"
                          "■  Built-in Engine — Simple built-in for quick tests"},
            {"KEY METRICS", "■  Total Return, CAGR, Max Drawdown\n"
                            "■  Sharpe Ratio, Sortino Ratio, Calmar Ratio\n"
                            "■  Win Rate, Profit Factor, Average Win/Loss\n"
                            "■  Trade count, Average holding period\n"
                            "■  Equity curve, Drawdown chart, Trade distribution"},
            {"SKILL LEVELS",
             "BEGINNER: Run a pre-built SMA crossover strategy on SPY, read the performance report\n"
             "INTERMEDIATE: Compare multiple strategies, optimize parameters, test on different assets\n"
             "ADVANCED: Build multi-asset strategies, walk-forward optimization, Monte Carlo simulation\n"
             "PRO: Custom backtesting engines, survivorship bias correction, transaction cost modeling"},
        });
}

// ============================================================================
// Research & Analytics
// ============================================================================

QWidget* DocsScreen::page_equity_research() {
    return make_page(
        "EQUITY RESEARCH", "Comprehensive stock analysis and fundamental research",
        {
            {"OVERVIEW", "The Equity Research screen provides institutional-grade fundamental analysis tools "
                         "including financial statements, valuation models, peer comparison, and analyst estimates."},
            {"KEY FEATURES", "■  Financial statements — Income, Balance Sheet, Cash Flow (quarterly/annual)\n"
                             "■  Valuation models — DCF, comparable analysis, precedent transactions\n"
                             "■  Peer comparison — Compare metrics across competitors\n"
                             "■  Analyst estimates — Consensus EPS, revenue, price targets\n"
                             "■  Technical overlays — Combine fundamental with technical analysis\n"
                             "■  Export to report builder for presentation-ready output"},
            {"REAL-WORLD USAGE", "■  Due diligence before investing in a new position\n"
                                 "■  Quarterly earnings analysis and comparison to estimates\n"
                                 "■  Sector analysis with peer group comparison\n"
                                 "■  Valuation-driven investment thesis development"},
            {"SKILL LEVELS", "BEGINNER: Look up basic financials (revenue, earnings, P/E ratio) for a stock\n"
                             "INTERMEDIATE: Compare peers, analyze quarterly trends, read cash flow statements\n"
                             "ADVANCED: Build DCF models, identify valuation discrepancies, create investment theses\n"
                             "PRO: Multi-factor screening + fundamental overlay, integrate with algo trading signals"},
        });
}

QWidget* DocsScreen::page_surface_analytics() {
    return make_page(
        "SURFACE ANALYTICS", "35 financial surfaces across derivatives, fixed income, FX, credit, and more",
        {
            {"OVERVIEW", "Surface Analytics provides 3D visualization of financial surfaces including "
                         "volatility surfaces, yield curves, credit spreads, and more. Supports 35 surface "
                         "types across 7 asset classes with real-time and historical data."},
            {"SURFACE CATEGORIES (35 TOTAL)",
             "EQUITY DERIVATIVES (7): Vol surface, local vol, implied distribution, skew, "
             "term structure, risk reversal, butterfly spread\n\n"
             "FIXED INCOME (5): Yield curve, forward curve, swap spread, basis swap, "
             "inflation breakeven\n\n"
             "FX (5): Vol surface, risk reversal, butterfly, cross-currency basis, carry\n\n"
             "CREDIT (5): CDS spread, default probability, recovery rate, basis, "
             "capital structure\n\n"
             "COMMODITIES (5): Forward curve, vol surface, calendar spread, crack spread, "
             "basis\n\n"
             "RISK (4): Correlation, covariance, VaR, stress test\n\n"
             "MACRO (4): GDP growth, inflation, rates, employment"},
            {"REAL-WORLD USAGE", "■  Options trading: Analyze vol surface for mispriced options\n"
                                 "■  Fixed income: Monitor yield curve shape for recession signals\n"
                                 "■  Risk management: Correlation and VaR surfaces for portfolio risk\n"
                                 "■  Relative value: Compare surfaces across maturities and strikes"},
            {"SKILL LEVELS", "BEGINNER: View pre-built surfaces, understand 3D visualization axes\n"
                             "INTERMEDIATE: Compare surface snapshots over time, identify trends\n"
                             "ADVANCED: Use surface analytics for options trading edge, relative value\n"
                             "PRO: Real-time surface monitoring, custom Databento feeds, surface arbitrage"},
        });
}

QWidget* DocsScreen::page_derivatives() {
    return make_page(
        "DERIVATIVES PRICING", "Bond, equity option, FX option, swap, and CDS pricing",
        {
            {"OVERVIEW", "The Derivatives screen provides pricing models for major derivative types using "
                         "embedded Python analytics. Supports bonds, equity options, FX options, interest "
                         "rate swaps, and credit default swaps."},
            {"SUPPORTED INSTRUMENTS", "■  Bonds — Price, yield, duration, convexity, spread\n"
                                      "■  Equity Options — Black-Scholes, binomial, Monte Carlo pricing\n"
                                      "■  FX Options — Garman-Kohlhagen, barrier options, digital options\n"
                                      "■  Interest Rate Swaps — Fixed-float, basis swaps, forward starting\n"
                                      "■  Credit Default Swaps — Spread pricing, default probability, CVA"},
            {"REAL-WORLD USAGE", "■  Options traders: Price options, calculate Greeks, find mispricing\n"
                                 "■  Fixed income: Price bonds, analyze yield sensitivity, duration matching\n"
                                 "■  Risk management: Calculate CVA, price CDS for credit hedging\n"
                                 "■  Treasury: Swap valuation for interest rate risk management"},
            {"SKILL LEVELS", "BEGINNER: Price a simple call option with Black-Scholes, understand Greeks\n"
                             "INTERMEDIATE: Compare pricing models, analyze sensitivity to volatility\n"
                             "ADVANCED: Price exotic options, build structured products, calculate CVA\n"
                             "PRO: Custom pricing models, volatility surface calibration, Greeks hedging"},
        });
}

QWidget* DocsScreen::page_portfolio() {
    return make_page(
        "PORTFOLIO", "Multi-portfolio management with analytics and AI agent",
        {
            {"OVERVIEW", "The Portfolio screen supports multiple portfolios with holdings tracking, performance "
                         "analytics, sector heatmap, historical chart, blotter, and an AI agent panel for "
                         "intelligent portfolio insights."},
            {"KEY FEATURES", "■  Multiple named portfolios with separate tracking\n"
                             "■  Holdings table — symbol, quantity, cost basis, market value, P&L\n"
                             "■  Sector heatmap — Visual allocation by sector\n"
                             "■  Performance chart — Time series of portfolio returns\n"
                             "■  Trade blotter — Complete transaction history\n"
                             "■  FFN analytics — Advanced factor analysis\n"
                             "■  AI Agent — Natural language portfolio insights"},
            {"REAL-WORLD USAGE", "■  Track your actual brokerage positions with real-time P&L\n"
                                 "■  Compare multiple portfolio strategies side by side\n"
                                 "■  Monitor sector allocation drift and rebalance\n"
                                 "■  Use AI agent to ask questions about your portfolio performance"},
            {"SKILL LEVELS", "BEGINNER: Create a portfolio, add holdings, track basic P&L\n"
                             "INTERMEDIATE: Use sector heatmap for diversification, review trade blotter\n"
                             "ADVANCED: Multiple portfolios for different strategies, FFN analytics\n"
                             "PRO: AI agent for portfolio optimization, factor exposure analysis, risk budgeting"},
        });
}

QWidget* DocsScreen::page_ma_analytics() {
    return make_page(
        "M&A ANALYTICS", "Merger and acquisition analysis tools",
        {
            {"OVERVIEW", "The M&A Analytics screen provides tools for analyzing mergers, acquisitions, "
                         "and corporate actions including deal comparison, accretion/dilution analysis, "
                         "and precedent transaction databases."},
            {"KEY FEATURES", "■  Deal screen — Browse recent and historical M&A transactions\n"
                             "■  Accretion/Dilution analysis — Model EPS impact of acquisitions\n"
                             "■  Comparable transactions — Find similar deals for valuation\n"
                             "■  Premium analysis — Analyze deal premiums by sector\n"
                             "■  Timeline tracking — Monitor deal progress and regulatory approvals"},
            {"REAL-WORLD USAGE", "■  Investment banking: Model potential acquisition targets\n"
                                 "■  Event-driven trading: Analyze merger arbitrage opportunities\n"
                                 "■  Due diligence: Research comparable transactions for valuation\n"
                                 "■  Risk assessment: Track regulatory risk for pending deals"},
            {"SKILL LEVELS", "BEGINNER: Browse recent deals, understand deal structure basics\n"
                             "INTERMEDIATE: Compare deal multiples, analyze premiums paid\n"
                             "ADVANCED: Build accretion/dilution models, identify merger arb opportunities\n"
                             "PRO: Custom deal screening, multi-factor M&A prediction models"},
        });
}

// ============================================================================
// AI & Quantitative
// ============================================================================

QWidget* DocsScreen::page_ai_quant_lab() {
    return make_page(
        "AI QUANT LAB", "18-module quantitative research platform with ML, factor discovery, and RL trading",
        {
            {"OVERVIEW", "The AI Quant Lab is a comprehensive quantitative research environment with 18 modules "
                         "covering machine learning, factor discovery, high-frequency trading, reinforcement "
                         "learning, and advanced statistical methods."},
            {"MODULES (18)", "PREDICTION: ML price prediction, time series forecasting\n"
                             "FACTORS: Factor discovery, alpha generation, smart beta\n"
                             "RISK: ML risk models, tail risk, regime detection\n"
                             "HFT: High-frequency signals, microstructure analysis\n"
                             "RL TRADING: Reinforcement learning agents for trading\n"
                             "NLP: Sentiment analysis, news parsing, earnings call analysis\n"
                             "ALTERNATIVE DATA: Satellite, social media, web scraping\n"
                             "PORTFOLIO: ML portfolio optimization, dynamic allocation\n"
                             "EXECUTION: Smart order routing, execution quality analysis\n"
                             "And 9 more covering derivatives, crypto, macro, and cross-asset"},
            {"REAL-WORLD USAGE", "■  Quant research: Discover new alpha factors from alternative data\n"
                                 "■  ML models: Train prediction models on historical market data\n"
                                 "■  Risk management: Regime detection for dynamic hedging\n"
                                 "■  Strategy development: RL agents that learn optimal trading policies"},
            {"SKILL LEVELS", "BEGINNER: Explore pre-built modules, run demo predictions on sample data\n"
                             "INTERMEDIATE: Train basic ML models, test factor strategies on historical data\n"
                             "ADVANCED: Custom factor research, multi-model ensembles, walk-forward validation\n"
                             "PRO: Production RL trading agents, HFT signal research, custom model deployment"},
        });
}

QWidget* DocsScreen::page_quantlib() {
    return make_page(
        "QUANTLIB SUITE", "18 quantitative analysis modules with 590+ endpoints",
        {
            {"OVERVIEW", "The QuantLib Suite provides access to 590+ quantitative analysis endpoints organized "
                         "into 18 modules. Powered by a REST API backend, it covers everything from basic "
                         "statistics to complex derivative pricing and risk modeling."},
            {"MODULES (18)", "■  Core — Fundamental quantitative operations\n"
                             "■  Analysis — Statistical analysis and hypothesis testing\n"
                             "■  Curves — Yield curves, forward curves, discount factors\n"
                             "■  Economics — Macro modeling, GDP, inflation, employment\n"
                             "■  Instruments — Bond, swap, option, futures pricing\n"
                             "■  ML — Machine learning models for finance\n"
                             "■  Models — Interest rate models (Hull-White, HJM, LMM)\n"
                             "■  Numerical — Numerical methods, PDE solvers, Monte Carlo\n"
                             "■  Physics — Physics-inspired financial models\n"
                             "■  Portfolio — Portfolio optimization, efficient frontier\n"
                             "■  Pricing — Exotic option pricing, structured products\n"
                             "■  Regulatory — Basel III/IV, FRTB, CVA/DVA/FVA\n"
                             "■  Risk — VaR, CVaR, stress testing, scenario analysis\n"
                             "■  Scheduling — Date math, day count conventions, roll rules\n"
                             "■  Solver — Root finding, optimization, calibration\n"
                             "■  Statistics — Distributions, regression, time series\n"
                             "■  Stochastic — Stochastic processes, Brownian motion, jump diffusion\n"
                             "■  Volatility — Vol models, SABR, Heston, local vol"},
            {"REAL-WORLD USAGE", "■  Quantitative analyst: Price complex derivatives, calibrate models\n"
                                 "■  Risk manager: Run VaR, stress tests, regulatory calculations\n"
                                 "■  Portfolio manager: Optimize allocation, efficient frontier analysis\n"
                                 "■  Fixed income: Yield curve construction, swap valuation"},
            {"SKILL LEVELS", "BEGINNER: Use the Statistics module for basic data analysis\n"
                             "INTERMEDIATE: Price bonds and options, build yield curves\n"
                             "ADVANCED: Calibrate vol models, run Monte Carlo simulations, regulatory calculations\n"
                             "PRO: Custom model development, multi-curve pricing, XVA calculations"},
        });
}

QWidget* DocsScreen::page_ai_chat() {
    return make_page(
        "AI CHAT", "AI-powered financial assistant",
        {
            {"OVERVIEW", "The AI Chat provides a conversational interface powered by large language models "
                         "for financial analysis, market insights, and terminal assistance."},
            {"KEY FEATURES", "■  Natural language market queries\n"
                             "■  Financial analysis and interpretation\n"
                             "■  Code generation for trading strategies\n"
                             "■  Portfolio insights and suggestions\n"
                             "■  Configurable LLM provider and model"},
            {"REAL-WORLD USAGE", "■  Quick analysis: \"What's driving NVDA today?\"\n"
                                 "■  Strategy help: \"Write a mean reversion strategy for crypto\"\n"
                                 "■  Learning: \"Explain how VaR is calculated\"\n"
                                 "■  Data queries: \"Compare AAPL and MSFT P/E ratios over 5 years\""},
            {"SKILL LEVELS", "BEGINNER: Ask basic market questions, get explanations of financial concepts\n"
                             "INTERMEDIATE: Generate analysis code, get portfolio recommendations\n"
                             "ADVANCED: Complex multi-step analysis, custom strategy development\n"
                             "PRO: Multi-agent workflows, MCP server integration, automated research pipelines"},
        });
}

QWidget* DocsScreen::page_agent_config() {
    return make_page(
        "AGENT STUDIO", "Configure and manage AI agents, teams, and workflows",
        {
            {"OVERVIEW", "The Agent Studio (Agent Config) provides an 8-view interface for creating, "
                         "managing, and orchestrating AI agents with specialized financial capabilities."},
            {"VIEWS (8)", "■  Agents — Browse and manage configured agents\n"
                          "■  Create — Build new agents with custom capabilities\n"
                          "■  Teams — Organize agents into collaborative teams\n"
                          "■  Workflows — Define multi-step agent pipelines\n"
                          "■  Planner — AI task planning and decomposition\n"
                          "■  Tools — Manage tools available to agents\n"
                          "■  Chat — Interact with agents in conversation\n"
                          "■  System — System-level agent configuration"},
            {"AGENT TYPES", "■  Geopolitics Agent — Monitors global events, conflict analysis\n"
                            "■  Economic Agent — Macro analysis, indicator tracking\n"
                            "■  Hedge Fund Agent — Multi-strategy portfolio management\n"
                            "■  Trader Agent — Technical analysis and trade execution\n"
                            "■  Investor Agent — Long-term fundamental analysis\n"
                            "■  Deep Agent — Complex multi-step research tasks"},
            {"SKILL LEVELS", "BEGINNER: Browse pre-configured agents, chat with the general assistant\n"
                             "INTERMEDIATE: Create custom agents with specific tool access\n"
                             "ADVANCED: Build agent teams, define multi-step workflows\n"
                             "PRO: Custom MCP server integration, multi-agent orchestration, automated research"},
        });
}

QWidget* DocsScreen::page_alpha_arena() {
    return make_page("ALPHA ARENA", "Competitive alpha research and strategy ranking",
                     {
                         {"OVERVIEW", "Alpha Arena is a competitive environment where trading strategies are ranked "
                                      "by performance. Submit strategies, compare against peers, and discover new "
                                      "alpha sources through community insights."},
                         {"KEY FEATURES", "■  Strategy leaderboard with real-time rankings\n"
                                          "■  Performance metrics comparison (Sharpe, returns, drawdown)\n"
                                          "■  Strategy submission and backtesting\n"
                                          "■  Community insights and discussion\n"
                                          "■  Historical performance tracking"},
                         {"REAL-WORLD USAGE", "■  Benchmark your strategies against others\n"
                                              "■  Discover new trading ideas from top performers\n"
                                              "■  Competitive motivation to improve strategy quality\n"
                                              "■  Community learning through shared insights"},
                         {"SKILL LEVELS", "BEGINNER: Browse the leaderboard, study top strategies\n"
                                          "INTERMEDIATE: Submit your first strategy, analyze performance metrics\n"
                                          "ADVANCED: Optimize strategies for ranking, study factor exposures\n"
                                          "PRO: Multi-strategy submission, alpha decay analysis, ensemble approaches"},
                     });
}

// ============================================================================
// Data Sources
// ============================================================================

QWidget* DocsScreen::page_dbnomics() {
    return make_page("DBNOMICS", "Access 100+ data providers with 500K+ economic datasets",
                     {
                         {"OVERVIEW", "DBnomics provides access to the world's largest open economic database. Browse "
                                      "100+ data providers (IMF, World Bank, OECD, ECB, Fed, etc.) with 500K+ datasets "
                                      "covering macroeconomics, finance, demographics, and trade."},
                         {"KEY FEATURES", "■  Provider browser — 100+ statistical agencies and central banks\n"
                                          "■  Dataset search — Full-text search across 500K+ datasets\n"
                                          "■  Series explorer — Browse individual time series with metadata\n"
                                          "■  Single view — Detailed chart + data table for one series\n"
                                          "■  Comparison view — 2x2 chart grid comparing multiple series\n"
                                          "■  CSV export — Download any series for external analysis\n"
                                          "■  Loading animations and progressive data loading"},
                         {"REAL-WORLD USAGE", "■  Macro research: GDP growth, inflation, employment data by country\n"
                                              "■  Central bank analysis: Interest rates, money supply, balance sheets\n"
                                              "■  Trade analysis: Import/export data, trade balances, tariffs\n"
                                              "■  Demographics: Population, labor force, migration data"},
                         {"SKILL LEVELS", "BEGINNER: Browse providers, look up US GDP or inflation data\n"
                                          "INTERMEDIATE: Compare series across countries, export data for analysis\n"
                                          "ADVANCED: Build macro dashboards, track leading indicators\n"
                                          "PRO: Integrate DBnomics data into quantitative macro trading models"},
                     });
}

QWidget* DocsScreen::page_economics() {
    return make_page("ECONOMICS", "Macroeconomic data and analysis tools",
                     {
                         {"OVERVIEW", "The Economics screen provides comprehensive macroeconomic data visualization "
                                      "and analysis tools for tracking economic indicators, central bank policies, "
                                      "and global economic trends."},
                         {"KEY FEATURES", "■  Economic indicators dashboard — GDP, CPI, unemployment, PMI\n"
                                          "■  Central bank tracker — Fed, ECB, BOJ, BOE rate decisions\n"
                                          "■  Economic calendar — Upcoming releases with forecasts\n"
                                          "■  Historical comparisons — Indicator trends over time\n"
                                          "■  Country comparison — Side-by-side economic profiles"},
                         {"REAL-WORLD USAGE", "■  Macro trading: Position ahead of rate decisions\n"
                                              "■  Risk management: Monitor recession indicators\n"
                                              "■  FX trading: Track rate differentials for carry trades\n"
                                              "■  Asset allocation: Shift sectors based on economic cycle"},
                         {"SKILL LEVELS", "BEGINNER: Check upcoming economic events, understand key indicators\n"
                                          "INTERMEDIATE: Track leading vs lagging indicators, spot economic trends\n"
                                          "ADVANCED: Build economic models, forecast indicator releases\n"
                                          "PRO: Multi-factor macro models, economic surprise index trading"},
                     });
}

QWidget* DocsScreen::page_akshare() {
    return make_page(
        "AKSHARE DATA", "Chinese and Asian market data via AkShare",
        {
            {"OVERVIEW", "AkShare provides access to Chinese financial market data including A-shares, "
                         "Hong Kong stocks, Chinese futures, and macro data from Chinese statistical agencies."},
            {"KEY FEATURES", "■  A-Share market data — SSE, SZSE listed companies\n"
                             "■  Hong Kong market data — HKEX listings\n"
                             "■  Chinese futures — commodity and financial futures\n"
                             "■  Macro data — NBS statistics, PBOC data\n"
                             "■  Fund data — Mutual funds, ETFs listed in China"},
            {"REAL-WORLD USAGE", "■  China market exposure: Research A-share opportunities\n"
                                 "■  Emerging market analysis: Compare Chinese vs global markets\n"
                                 "■  Commodity trading: Track Chinese commodity demand\n"
                                 "■  Macro research: Monitor Chinese economic indicators"},
            {"SKILL LEVELS", "BEGINNER: Browse Chinese market indices and major stocks\n"
                             "INTERMEDIATE: Compare A-share sectors, track PBOC policy\n"
                             "ADVANCED: Chinese commodity-equity correlation analysis\n"
                             "PRO: Cross-market arbitrage between A-shares, H-shares, and ADRs"},
        });
}

QWidget* DocsScreen::page_gov_data() {
    return make_page("GOVERNMENT DATA", "Official government statistical data",
                     {
                         {"OVERVIEW", "Access official government data from statistical agencies worldwide including "
                                      "census data, economic surveys, trade statistics, and regulatory filings."},
                         {"KEY FEATURES", "■  Multi-country government data sources\n"
                                          "■  Census and demographic data\n"
                                          "■  Trade and commerce statistics\n"
                                          "■  Regulatory filings and disclosures\n"
                                          "■  Historical data series with long time horizons"},
                         {"REAL-WORLD USAGE", "■  Research: Access primary source economic data\n"
                                              "■  Policy analysis: Track government spending and fiscal policy\n"
                                              "■  Demographics: Population and labor force trends\n"
                                              "■  Trade analysis: Import/export flows by country and sector"},
                         {"SKILL LEVELS", "BEGINNER: Browse available datasets, download basic reports\n"
                                          "INTERMEDIATE: Cross-reference government data with market data\n"
                                          "ADVANCED: Build predictive models using government leading indicators\n"
                                          "PRO: Automated policy tracking, fiscal surprise detection"},
                     });
}

// ============================================================================
// Geopolitics & Alt
// ============================================================================

QWidget* DocsScreen::page_geopolitics() {
    return make_page(
        "GEOPOLITICS", "Conflict monitoring, HDX data, trade analysis, and relationship mapping",
        {
            {"OVERVIEW", "The Geopolitics screen provides a comprehensive view of global geopolitical events "
                         "with conflict monitoring, humanitarian data (HDX), trade flow analysis, and entity "
                         "relationship mapping for understanding geopolitical dynamics."},
            {"KEY FEATURES", "■  Conflict monitor — Active global conflicts with severity tracking\n"
                             "■  HDX integration — Humanitarian Data Exchange for crisis data\n"
                             "■  Trade analysis — Sanctions, tariffs, trade flow disruptions\n"
                             "■  Relationship mapping — Entity relationships and influence networks\n"
                             "■  Risk scoring — Country-level geopolitical risk assessments\n"
                             "■  AI agent — Geopolitics-specialized analysis agent"},
            {"REAL-WORLD USAGE", "■  Risk management: Monitor geopolitical risks to your portfolio\n"
                                 "■  Commodity trading: Track supply disruptions from conflicts\n"
                                 "■  FX trading: Geopolitical risk premium in currency pricing\n"
                                 "■  ESG investing: Humanitarian impact assessment"},
            {"SKILL LEVELS", "BEGINNER: Monitor the conflict dashboard, understand risk scores\n"
                             "INTERMEDIATE: Correlate geopolitical events with market movements\n"
                             "ADVANCED: Build geopolitical risk models, trade around events\n"
                             "PRO: Multi-factor geopolitical alpha, automated event-driven trading"},
        });
}

QWidget* DocsScreen::page_maritime() {
    return make_page("MARITIME", "Maritime shipping and trade flow data",
                     {
                         {"OVERVIEW", "The Maritime screen provides shipping and trade flow data for monitoring global "
                                      "supply chains, commodity transport, and maritime trade patterns."},
                         {"KEY FEATURES", "■  Vessel tracking — AIS data for major shipping routes\n"
                                          "■  Port analytics — Throughput, congestion, wait times\n"
                                          "■  Trade flows — Commodity shipping volumes by route\n"
                                          "■  Supply chain indicators — Shipping cost indices\n"
                                          "■  Geopolitical overlay — Chokepoint risk assessment"},
                         {"REAL-WORLD USAGE", "■  Commodity trading: Track oil tanker movements for supply signals\n"
                                              "■  Supply chain: Monitor port congestion for inflation signals\n"
                                              "■  Shipping stocks: Analyze Baltic Dry Index and freight rates\n"
                                              "■  Geopolitical risk: Chokepoint monitoring (Suez, Strait of Hormuz)"},
                         {"SKILL LEVELS", "BEGINNER: Browse shipping routes, understand trade flow basics\n"
                                          "INTERMEDIATE: Track commodity shipments, correlate with futures prices\n"
                                          "ADVANCED: Build supply chain disruption models\n"
                                          "PRO: Satellite-verified shipping data for alternative data alpha"},
                     });
}

QWidget* DocsScreen::page_polymarket() {
    return make_page(
        "POLYMARKET", "Prediction markets and event probability trading",
        {
            {"OVERVIEW", "The Polymarket screen provides access to prediction market data, allowing you to "
                         "track event probabilities and trade on real-world outcomes."},
            {"KEY FEATURES", "■  Event browser — Political, economic, sports, crypto events\n"
                             "■  Probability charts — Historical probability movement\n"
                             "■  Volume and liquidity data\n"
                             "■  Category filtering and search\n"
                             "■  Correlation with traditional markets"},
            {"REAL-WORLD USAGE", "■  Election trading: Track political event probabilities\n"
                                 "■  Hedging: Use prediction markets to hedge event risk\n"
                                 "■  Sentiment gauge: Market-implied probabilities as sentiment\n"
                                 "■  Research: Compare prediction market accuracy with polls"},
            {"SKILL LEVELS", "BEGINNER: Browse events, understand probability pricing\n"
                             "INTERMEDIATE: Track probability changes, identify momentum\n"
                             "ADVANCED: Arbitrage between prediction markets and traditional assets\n"
                             "PRO: Build prediction market-enhanced trading models"},
        });
}

QWidget* DocsScreen::page_alt_investments() {
    return make_page(
        "ALTERNATIVE INVESTMENTS", "Private equity, hedge funds, real assets, and alternatives data",
        {
            {"OVERVIEW", "The Alternative Investments screen covers non-traditional asset classes including "
                         "private equity, hedge funds, real estate, commodities, and other alternatives."},
            {"KEY FEATURES", "■  PE/VC deal flow and fund performance\n"
                             "■  Hedge fund strategy tracking\n"
                             "■  Real estate market data (REITs, cap rates)\n"
                             "■  Commodity index tracking\n"
                             "■  Correlation analysis with traditional assets"},
            {"REAL-WORLD USAGE", "■  Portfolio diversification with alternative assets\n"
                                 "■  Hedge fund replication strategy development\n"
                                 "■  Real asset allocation for inflation hedging\n"
                                 "■  Private market opportunity screening"},
            {"SKILL LEVELS", "BEGINNER: Understand alternative asset classes and their role\n"
                             "INTERMEDIATE: Analyze REIT performance, commodity correlations\n"
                             "ADVANCED: Factor-based alternative investment allocation\n"
                             "PRO: Private market deal analysis, hedge fund replication, liquid alternatives"},
        });
}

// ============================================================================
// Tools
// ============================================================================

QWidget* DocsScreen::page_report_builder() {
    return make_page("REPORT BUILDER", "Drag-and-drop financial report creation",
                     {
                         {"OVERVIEW", "The Report Builder lets you create professional financial reports with a "
                                      "drag-and-drop interface. Combine charts, tables, text, and data from any "
                                      "terminal screen into presentation-ready documents."},
                         {"KEY FEATURES", "■  Component toolbar — Charts, tables, text blocks, images, headers\n"
                                          "■  Drag-and-drop canvas with grid snapping\n"
                                          "■  Properties panel — Customize colors, fonts, data sources\n"
                                          "■  Multi-page support with page navigation\n"
                                          "■  Export to PDF, PNG, or print directly\n"
                                          "■  Template library for common report types"},
                         {"REAL-WORLD USAGE", "■  Client reports: Weekly market summary for advisory clients\n"
                                              "■  Investment memos: Structured investment thesis documents\n"
                                              "■  Risk reports: Portfolio risk metrics for compliance\n"
                                              "■  Research reports: Published equity research format"},
                         {"SKILL LEVELS", "BEGINNER: Create a simple one-page report with a chart and text\n"
                                          "INTERMEDIATE: Multi-page reports with tables and data binding\n"
                                          "ADVANCED: Templated report generation, automated weekly reports\n"
                                          "PRO: Branded client deliverables, automated report pipelines"},
                     });
}

QWidget* DocsScreen::page_node_editor() {
    return make_page("NODE EDITOR", "Visual workflow automation with drag-and-drop nodes",
                     {
                         {"OVERVIEW", "The Node Editor provides a visual programming environment for building "
                                      "data processing and trading workflows by connecting nodes."},
                         {"KEY FEATURES", "■  Drag-and-drop node placement\n"
                                          "■  Data source nodes — Market data, news, economic indicators\n"
                                          "■  Processing nodes — Filters, transformations, calculations\n"
                                          "■  Action nodes — Alerts, trades, report generation\n"
                                          "■  Workflow persistence and sharing\n"
                                          "■  Real-time execution monitoring"},
                         {"REAL-WORLD USAGE", "■  Alert systems: Price crosses MA → send notification\n"
                                              "■  Data pipelines: Fetch → filter → analyze → report\n"
                                              "■  Trading automation: Signal → validation → order → monitor\n"
                                              "■  Research workflows: Data collection → processing → visualization"},
                         {"SKILL LEVELS", "BEGINNER: Connect a data source to a simple alert node\n"
                                          "INTERMEDIATE: Multi-step workflows with conditional branching\n"
                                          "ADVANCED: Complex trading workflows with risk checks\n"
                                          "PRO: Production automation pipelines, multi-feed aggregation"},
                     });
}

QWidget* DocsScreen::page_code_editor() {
    return make_page("CODE EDITOR", "Built-in code editor for trading strategies and scripts",
                     {
                         {"OVERVIEW", "The Code Editor provides a built-in development environment for writing "
                                      "and testing Python trading strategies and analytics scripts."},
                         {"KEY FEATURES", "■  Syntax highlighting for Python\n"
                                          "■  Script execution with output panel\n"
                                          "■  Access to terminal data via APIs\n"
                                          "■  Template library for common patterns\n"
                                          "■  Integration with backtesting engine"},
                         {"REAL-WORLD USAGE", "■  Strategy development: Write and test Python strategies\n"
                                              "■  Custom analytics: Build bespoke analysis scripts\n"
                                              "■  Data processing: Transform and analyze raw data\n"
                                              "■  Automation: Create scheduled data processing jobs"},
                         {"SKILL LEVELS", "BEGINNER: Run example scripts, modify parameters\n"
                                          "INTERMEDIATE: Write simple strategies from templates\n"
                                          "ADVANCED: Custom analytics with pandas, numpy, scipy\n"
                                          "PRO: Full strategy development with custom backtesting integration"},
                     });
}

QWidget* DocsScreen::page_excel() {
    return make_page("EXCEL", "Spreadsheet interface for financial analysis",
                     {
                         {"OVERVIEW", "The Excel screen provides a spreadsheet-style interface for financial "
                                      "analysis with formula support, data import/export, and charting."},
                         {"KEY FEATURES", "■  Spreadsheet grid with formula support\n"
                                          "■  Data import from terminal screens\n"
                                          "■  Financial functions (NPV, IRR, XIRR, etc.)\n"
                                          "■  Chart creation from spreadsheet data\n"
                                          "■  CSV import/export compatibility"},
                         {"REAL-WORLD USAGE", "■  Financial modeling: Build DCF models in a familiar interface\n"
                                              "■  Data analysis: Import market data for custom calculations\n"
                                              "■  Reporting: Create tabular reports with formulas\n"
                                              "■  Comparison: Side-by-side metric comparison tables"},
                         {"SKILL LEVELS", "BEGINNER: Enter data, use basic formulas (SUM, AVG)\n"
                                          "INTERMEDIATE: Financial functions, data import from terminal\n"
                                          "ADVANCED: Complex financial models with linked cells\n"
                                          "PRO: Automated model updates with live data feeds"},
                     });
}

QWidget* DocsScreen::page_notes() {
    return make_page("NOTES", "Persistent note-taking for research and trade journals",
                     {
                         {"OVERVIEW", "The Notes screen provides a persistent note-taking environment for maintaining "
                                      "research notes, trade journals, and analysis records."},
                         {"KEY FEATURES", "■  Rich text editing\n"
                                          "■  Multiple note organization\n"
                                          "■  Persistent storage between sessions\n"
                                          "■  Search across all notes\n"
                                          "■  Timestamp tracking for each note"},
                         {"REAL-WORLD USAGE", "■  Trade journal: Record entry/exit rationale for every trade\n"
                                              "■  Research notes: Document investment theses and analysis\n"
                                              "■  Meeting notes: Record calls with management or analysts\n"
                                              "■  Watchlist notes: Add context to symbols you're tracking"},
                         {"SKILL LEVELS", "BEGINNER: Create your first trade journal entry\n"
                                          "INTERMEDIATE: Organized note system by category (trades, research, ideas)\n"
                                          "ADVANCED: Systematic trade journaling with performance review\n"
                                          "PRO: Integrated research workflow with notes linked to positions"},
                     });
}

QWidget* DocsScreen::page_mcp_servers() {
    return make_page("MCP SERVERS", "Model Context Protocol server management",
                     {
                         {"OVERVIEW", "The MCP Servers screen manages Model Context Protocol servers that extend "
                                      "AI agent capabilities with external tool access and data sources."},
                         {"KEY FEATURES", "■  Server registration and management\n"
                                          "■  Tool discovery and configuration\n"
                                          "■  Connection status monitoring\n"
                                          "■  Custom server integration\n"
                                          "■  Security and access control"},
                         {"REAL-WORLD USAGE", "■  Extend AI agents with custom data sources\n"
                                              "■  Connect to proprietary APIs and databases\n"
                                              "■  Add specialized tools for domain-specific analysis\n"
                                              "■  Build custom research pipelines with external tools"},
                         {"SKILL LEVELS", "BEGINNER: Browse available MCP servers, understand the concept\n"
                                          "INTERMEDIATE: Connect pre-configured MCP servers to agents\n"
                                          "ADVANCED: Deploy custom MCP servers for proprietary data\n"
                                          "PRO: Build production MCP infrastructure, multi-server orchestration"},
                     });
}

QWidget* DocsScreen::page_data_mapping() {
    return make_page("DATA MAPPING", "Map and configure external data source connections",
                     {
                         {"OVERVIEW", "The Data Mapping screen lets you configure connections between external data "
                                      "sources and terminal screens, mapping fields and transforming data formats."},
                         {"KEY FEATURES", "■  Data source configuration\n"
                                          "■  Field mapping and transformation\n"
                                          "■  Schema validation\n"
                                          "■  Connection testing\n"
                                          "■  Scheduled refresh configuration"},
                         {"REAL-WORLD USAGE", "■  Connect proprietary data feeds to terminal widgets\n"
                                              "■  Map custom API responses to standard formats\n"
                                              "■  Integrate internal databases with terminal screens\n"
                                              "■  Configure automated data refresh schedules"},
                         {"SKILL LEVELS", "BEGINNER: Browse configured data sources\n"
                                          "INTERMEDIATE: Add new data source connections\n"
                                          "ADVANCED: Custom field mapping and transformation logic\n"
                                          "PRO: Complex multi-source data pipelines with validation"},
                     });
}

// ============================================================================
// Community
// ============================================================================

QWidget* DocsScreen::page_settings() {
    return make_page("SETTINGS", "Application configuration and preferences",
                     {
                         {"OVERVIEW", "The Settings screen provides comprehensive application configuration "
                                      "including display preferences, data sources, LLM configuration, MCP servers, "
                                      "and integration settings."},
                         {"KEY FEATURES", "■  Display preferences — Theme, font size, layout options\n"
                                          "■  Data configuration — API keys, refresh intervals, cache settings\n"
                                          "■  LLM Configuration — Model provider, API key, parameters\n"
                                          "■  MCP Servers — Server management and configuration\n"
                                          "■  Notification preferences\n"
                                          "■  Keyboard shortcut customization"},
                         {"SECTIONS", "■  General — Language, timezone, display density\n"
                                      "■  Trading — Default exchange, order confirmations, risk limits\n"
                                      "■  Data — API keys for market data providers\n"
                                      "■  AI — LLM provider, model selection, temperature\n"
                                      "■  MCP — Server endpoints and tool access\n"
                                      "■  Notifications — Alert channels and thresholds"},
                         {"SKILL LEVELS", "BEGINNER: Set your timezone and preferred market\n"
                                          "INTERMEDIATE: Configure API keys for data providers and LLM\n"
                                          "ADVANCED: Fine-tune LLM parameters, set up MCP servers\n"
                                          "PRO: Full infrastructure configuration, custom integrations"},
                     });
}

QWidget* DocsScreen::page_profile() {
    return make_page("PROFILE", "User account, subscription, and usage tracking",
                     {
                         {"OVERVIEW", "The Profile screen displays your account information, subscription status, "
                                      "usage statistics, and billing history."},
                         {"KEY FEATURES", "■  Account details — Name, email, registration date\n"
                                          "■  Subscription status — Current plan, expiry, features\n"
                                          "■  Usage statistics — API calls, data consumed, trades placed\n"
                                          "■  Credit balance — Remaining AI/compute credits\n"
                                          "■  Billing history — Past invoices and payments"},
                         {"REAL-WORLD USAGE", "■  Monitor your subscription and credit usage\n"
                                              "■  Upgrade or change your plan\n"
                                              "■  Track API usage to stay within limits\n"
                                              "■  Download billing records for expense tracking"},
                         {"SKILL LEVELS", "BEGINNER: Check your account status and plan\n"
                                          "INTERMEDIATE: Monitor credit usage, plan API call budget\n"
                                          "ADVANCED: Optimize usage patterns for cost efficiency\n"
                                          "PRO: Enterprise account management, team usage tracking"},
                     });
}

// ============================================================================
// Sidebar
// ============================================================================

void DocsScreen::build_sidebar() {
    sidebar_ = new QTreeWidget;
    sidebar_->setHeaderHidden(true);
    sidebar_->setStyleSheet(SIDEBAR_SS());
    sidebar_->setIndentation(16);
    sidebar_->setAnimated(true);
    sidebar_->setRootIsDecorated(true);

    auto add_item = [&](QTreeWidgetItem* parent, const QString& label, const QString& id) {
        auto* item = new QTreeWidgetItem(parent, {label});
        item->setData(0, Qt::UserRole, id);
        return item;
    };

    auto add_category = [&](const QString& label) {
        auto* cat = new QTreeWidgetItem(sidebar_, {label});
        cat->setFlags(cat->flags() & ~Qt::ItemIsSelectable);
        QFont f(FONT, 10);
        f.setBold(true);
        cat->setFont(0, f);
        cat->setForeground(0, QColor(ui::colors::AMBER.get()));
        cat->setExpanded(true);
        return cat;
    };

    // ── Getting Started ──────────────────────────────────────────────────────
    auto* start = add_category("GETTING STARTED");
    add_item(start, "Welcome", "welcome");
    add_item(start, "First Steps", "getting_started");
    add_item(start, "Keyboard Shortcuts", "shortcuts");

    // ── Core Screens ─────────────────────────────────────────────────────────
    auto* core = add_category("CORE SCREENS");
    add_item(core, "Dashboard", "dashboard");
    add_item(core, "Markets", "markets");
    add_item(core, "News", "news");
    add_item(core, "Watchlist", "watchlist");

    // ── Trading ──────────────────────────────────────────────────────────────
    auto* trading = add_category("TRADING");
    add_item(trading, "Crypto Trading", "crypto_trading");
    add_item(trading, "Paper Trading", "paper_trading");
    add_item(trading, "Algo Trading", "algo_trading");
    add_item(trading, "Backtesting", "backtesting");

    // ── Research & Analytics ─────────────────────────────────────────────────
    auto* research = add_category("RESEARCH & ANALYTICS");
    add_item(research, "Equity Research", "equity_research");
    add_item(research, "Surface Analytics", "surface_analytics");
    add_item(research, "Derivatives", "derivatives");
    add_item(research, "Portfolio", "portfolio");
    add_item(research, "M&A Analytics", "ma_analytics");

    // ── AI & Quantitative ────────────────────────────────────────────────────
    auto* ai = add_category("AI & QUANTITATIVE");
    add_item(ai, "AI Quant Lab", "ai_quant_lab");
    add_item(ai, "QuantLib Suite", "quantlib");
    add_item(ai, "AI Chat", "ai_chat");
    add_item(ai, "Agent Studio", "agent_config");
    add_item(ai, "Alpha Arena", "alpha_arena");

    // ── Data Sources ─────────────────────────────────────────────────────────
    auto* data_cat = add_category("DATA SOURCES");
    add_item(data_cat, "DBnomics", "dbnomics");
    add_item(data_cat, "Economics", "economics");
    add_item(data_cat, "AkShare Data", "akshare");
    add_item(data_cat, "Government Data", "gov_data");

    // ── Geopolitics & Alt ────────────────────────────────────────────────────
    auto* geo = add_category("GEOPOLITICS & ALT");
    add_item(geo, "Geopolitics", "geopolitics");
    add_item(geo, "Maritime", "maritime");
    add_item(geo, "Prediction Markets", "polymarket");
    add_item(geo, "Alt Investments", "alt_investments");

    // ── Tools ────────────────────────────────────────────────────────────────
    auto* tools = add_category("TOOLS");
    add_item(tools, "Report Builder", "report_builder");
    add_item(tools, "Node Editor", "node_editor");
    add_item(tools, "Code Editor", "code_editor");
    add_item(tools, "Excel", "excel");
    add_item(tools, "Notes", "notes");
    add_item(tools, "MCP Servers", "mcp_servers");
    add_item(tools, "Data Mapping", "data_mapping");

    // ── Account ──────────────────────────────────────────────────────────────
    auto* account = add_category("ACCOUNT");
    add_item(account, "Settings", "settings");
    add_item(account, "Profile", "profile");

    // Navigation
    connect(sidebar_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* current, QTreeWidgetItem*) {
        if (!current)
            return;
        QString id = current->data(0, Qt::UserRole).toString();
        if (!id.isEmpty()) {
            navigate_to(id);
        }
    });
}

// ============================================================================
// Content pages
// ============================================================================

void DocsScreen::build_content_pages() {
    pages_ = new QStackedWidget;
    pages_->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE()));

    auto add = [&](const QString& id, QWidget* page) {
        int idx = pages_->addWidget(page);
        page_index_[id] = idx;
    };

    // Getting Started
    add("welcome", page_welcome());
    add("getting_started", page_getting_started());
    add("shortcuts", page_keyboard_shortcuts());

    // Core
    add("dashboard", page_dashboard());
    add("markets", page_markets());
    add("news", page_news());
    add("watchlist", page_watchlist());

    // Trading
    add("crypto_trading", page_crypto_trading());
    add("paper_trading", page_paper_trading());
    add("algo_trading", page_algo_trading());
    add("backtesting", page_backtesting());

    // Research
    add("equity_research", page_equity_research());
    add("surface_analytics", page_surface_analytics());
    add("derivatives", page_derivatives());
    add("portfolio", page_portfolio());
    add("ma_analytics", page_ma_analytics());

    // AI
    add("ai_quant_lab", page_ai_quant_lab());
    add("quantlib", page_quantlib());
    add("ai_chat", page_ai_chat());
    add("agent_config", page_agent_config());
    add("alpha_arena", page_alpha_arena());

    // Data
    add("dbnomics", page_dbnomics());
    add("economics", page_economics());
    add("akshare", page_akshare());
    add("gov_data", page_gov_data());

    // Geopolitics
    add("geopolitics", page_geopolitics());
    add("maritime", page_maritime());
    add("polymarket", page_polymarket());
    add("alt_investments", page_alt_investments());

    // Tools
    add("report_builder", page_report_builder());
    add("node_editor", page_node_editor());
    add("code_editor", page_code_editor());
    add("excel", page_excel());
    add("notes", page_notes());
    add("mcp_servers", page_mcp_servers());
    add("data_mapping", page_data_mapping());

    // Account
    add("settings", page_settings());
    add("profile", page_profile());
}

void DocsScreen::navigate_to(const QString& section_id) {
    auto it = page_index_.find(section_id);
    if (it != page_index_.end()) {
        pages_->setCurrentIndex(it.value());
    }
}

// ============================================================================
// Constructor — Main layout
// ============================================================================

DocsScreen::DocsScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("docsScreen");
    setStyleSheet(QString("QWidget#docsScreen { background: %1; }").arg(ui::colors::BG_BASE()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Command bar ──────────────────────────────────────────────────────────
    auto* cmd = new QWidget(this);
    cmd->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    cmd->setFixedHeight(32);
    auto* cmd_hl = new QHBoxLayout(cmd);
    cmd_hl->setContentsMargins(10, 0, 10, 0);
    cmd_hl->setSpacing(10);

    auto* title = new QLabel("DOCUMENTATION");
    title->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold; letter-spacing: 1px;"
                                 " background: transparent; font-family: 'Consolas','Courier New',monospace;")
                             .arg(ui::colors::AMBER()));
    cmd_hl->addWidget(title);

    auto* sep = new QLabel("|");
    sep->setStyleSheet(QString("color: %1; background: transparent; font-family: 'Consolas',monospace;")
                           .arg(ui::colors::BORDER_BRIGHT()));
    cmd_hl->addWidget(sep);

    breadcrumb_ = new QLabel("FINCEPT TERMINAL v4.0.0");
    breadcrumb_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold;"
                                       " background: transparent; letter-spacing: 0.5px;"
                                       " font-family: 'Consolas','Courier New',monospace;")
                                   .arg(ui::colors::TEXT_SECONDARY()));
    cmd_hl->addWidget(breadcrumb_);

    cmd_hl->addStretch();

    auto* count = new QLabel("35 TOPICS  |  9 CATEGORIES");
    count->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;"
                                 " font-family: 'Consolas','Courier New',monospace;")
                             .arg(ui::colors::TEXT_TERTIARY()));
    cmd_hl->addWidget(count);

    root->addWidget(cmd);

    // ── Splitter: sidebar + content ──────────────────────────────────────────
    build_sidebar();
    build_content_pages();

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setStyleSheet(QString("QSplitter { background: %1; }"
                                    "QSplitter::handle { background: %2; width: 1px; }")
                                .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));
    splitter->addWidget(sidebar_);
    splitter->addWidget(pages_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({220, 800});

    root->addWidget(splitter, 1);

    // Default to welcome page
    navigate_to("welcome");
}

} // namespace fincept::screens
