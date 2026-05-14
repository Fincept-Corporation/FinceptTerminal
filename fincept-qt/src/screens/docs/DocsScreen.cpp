// Documentation Screen — comprehensive in-app terminal guide.
//
// Core lifecycle: ctor, sidebar/page assembly (build_sidebar,
// build_content_pages), and navigate_to. The make_* helpers that compose
// every page also live here. The page bodies are split across:
//   - DocsScreen_Pages_Intro.cpp    — welcome/dashboard/markets/news/watchlist
//   - DocsScreen_Pages_Trading.cpp  — crypto / paper / algo / backtest / AI quant
//   - DocsScreen_Pages_Data.cpp     — data sources, geopolitics, tools, settings
// Shared QSS helpers live in DocsScreen_internal.h.
#include "screens/docs/DocsScreen.h"
#include "screens/docs/DocsScreen_internal.h"

#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QScrollArea>
#include <QSplitter>


namespace fincept::screens {

using fincept::screens::docs_internal::SCROLL_SS;
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

// SCROLL_SS lives in DocsScreen_internal.h so the page-builder TUs can reuse it.

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
