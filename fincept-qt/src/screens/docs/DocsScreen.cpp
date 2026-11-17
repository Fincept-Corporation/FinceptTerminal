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

#include "core/events/EventBus.h"
#include "screens/common/CommandRows.h"
#include "screens/docs/DocsScreen_internal.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"

#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QKeySequence>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QShortcut>
#include <QSplitter>
#include <QTimer>
#include <QTreeWidgetItemIterator>

namespace fincept::screens {

using fincept::screens::docs_internal::SCROLL_SS;

// Sidebar category rows carry their own font and brush (item data, outside any
// stylesheet), so they are styled here and again on every theme change.
static void docs_style_category(QTreeWidgetItem* cat) {
    cat->setFont(0, ui::fonts::font(-1, QFont::Bold));
    cat->setForeground(0, QColor(ui::colors::AMBER()));
}

static QString SIDEBAR_SS() {
    return QString("QTreeWidget { background: %1; color: %2; border: none;"
                   "  font-size: " + fincept::ui::fonts::px(-2) + "px;"
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
        .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(),
             ui::colors::TEXT_PRIMARY(), ui::colors::BG_HOVER(), ui::colors::AMBER(), ui::colors::BORDER_MED(),
             ui::colors::BORDER_BRIGHT());
}

// SCROLL_SS lives in DocsScreen_internal.h so the page-builder TUs can reuse it.

// Maps a documentation topic to the terminal screen it describes, or an empty
// string for topics that are not a screen (Welcome, First Steps, Shortcuts).
// Every id returned here must be registered in WindowFrame::setup_dock_screens()
// — nav.switch_screen silently ignores an unknown id.
static QString docs_screen_target(const QString& topic) {
    static const QSet<QString> kScreenTopics = {
        "dashboard",     "markets",         "news",          "watchlist",       "screener",
        "crypto_trading", "crypto_center",  "equity_trading", "fno",            "algo_trading",
        "backtesting",   "equity_research", "surface_analytics", "derivatives", "portfolio",
        "ma_analytics",  "ai_quant_lab",    "quantlib",      "ai_chat",         "agent_config",
        "alpha_arena",   "dbnomics",        "economics",     "akshare",         "gov_data",
        "data_sources",  "asia_markets",    "trade_viz",     "geopolitics",     "maritime",
        "polymarket",    "alt_investments", "relationship_map", "report_builder", "node_editor",
        "code_editor",   "excel",           "notes",         "mcp_servers",     "data_mapping",
        "file_manager",  "settings",      "profile",       "agent_workbench",
    };
    // Paper trading is a mode of the crypto terminal, not a screen of its own.
    if (topic == QLatin1String("paper_trading"))
        return QStringLiteral("crypto_trading");
    return kScreenTopics.contains(topic) ? topic : QString();
}

// ============================================================================
// Helpers
// ============================================================================

QLabel* DocsScreen::make_heading(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(0) + "px; font-weight: bold; letter-spacing: 0.5px;"
                               " background: transparent;"
                               " padding: 4px 0;")
                           .arg(ui::colors::AMBER()));
    lbl->setWordWrap(true);
    return lbl;
}

QLabel* DocsScreen::make_body_label(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent; line-height: 1.5;")
                           .arg(ui::colors::TEXT_PRIMARY()));
    lbl->setWordWrap(true);
    return lbl;
}

QLabel* DocsScreen::make_muted_label(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent;")
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
    hdr->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: bold; letter-spacing: 0.5px;"
                               " background: %2; padding: 8px 12px; border-bottom: 1px solid %3;")
                           .arg(accent_color, ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    vl->addWidget(hdr);

    // Body
    auto* content = new QLabel(body);
    content->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent; padding: 10px 12px;"
                                   " line-height: 1.6;")
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

    auto* hdr = new QLabel(tr("SKILL LEVELS"));
    hdr->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: bold; letter-spacing: 0.5px;"
                               " background: %2; padding: 8px 12px; border-bottom: 1px solid %3;")
                           .arg(ui::colors::AMBER(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    vl->addWidget(hdr);

    struct Level {
        QString tag;
        const char* color;
        const QString& text;
    };
    Level levels[] = {
        {tr("BEGINNER"), ui::colors::POSITIVE(), beginner},
        {tr("INTERMEDIATE"), ui::colors::INFO(), intermediate},
        {tr("ADVANCED"), ui::colors::AMBER(), advanced},
        {tr("PRO"), ui::colors::NEGATIVE(), pro},
    };

    for (const auto& lvl : levels) {
        auto* row = new QWidget(this);
        row->setStyleSheet(
            QString("background: transparent; border-bottom: 1px solid %1;").arg(ui::colors::BG_RAISED()));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(12, 8, 12, 8);
        rl->setSpacing(10);

        auto* badge = new QLabel(lvl.tag);
        // 105px at the default font size, wider for a larger one.
        badge->setFixedWidth(qMax(105, 105 * ui::fonts::font_px(0) / ui::ThemeManager::kDefaultFontPx));
        badge->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: bold; background: transparent;"
                                     " letter-spacing: 0.5px;")
                                 .arg(lvl.color));
        rl->addWidget(badge);

        auto* desc = new QLabel(lvl.text);
        desc->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent;")
                                .arg(ui::colors::TEXT_PRIMARY()));
        desc->setWordWrap(true);
        rl->addWidget(desc, 1);

        vl->addWidget(row);
    }
    return panel;
}

QWidget* DocsScreen::make_tip_box(const QString& text, const QString& color) {
    auto* box = new QLabel(text);
    box->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; background: %2;"
                               " border: 1px solid %3; padding: 8px 12px;")
                           .arg(color, ui::colors::tint(QColor(color), 0.08), ui::colors::alpha(QColor(color), 0.3)));
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
    search_empty_ = nullptr; // the old one belongs to the old sidebar
    sidebar_->viewport()->installEventFilter(this); // keeps the no-match state sized to the list
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
        docs_style_category(cat);
        cat->setExpanded(true);
        return cat;
    };

    // ── Getting Started ──────────────────────────────────────────────────────
    auto* start = add_category(tr("GETTING STARTED"));
    add_item(start, tr("Welcome"), "welcome");
    add_item(start, tr("First Steps"), "getting_started");
    add_item(start, tr("Keyboard Shortcuts"), "shortcuts");

    // ── Core Screens ─────────────────────────────────────────────────────────
    auto* core = add_category(tr("CORE SCREENS"));
    add_item(core, tr("Dashboard"), "dashboard");
    add_item(core, tr("Markets"), "markets");
    add_item(core, tr("News"), "news");
    add_item(core, tr("Watchlist"), "watchlist");
    add_item(core, tr("Screener"), "screener");

    // ── Trading ──────────────────────────────────────────────────────────────
    auto* trading = add_category(tr("TRADING"));
    add_item(trading, tr("Crypto Trading"), "crypto_trading");
    add_item(trading, tr("Crypto Center"), "crypto_center");
    add_item(trading, tr("Equity Trading"), "equity_trading");
    add_item(trading, tr("F&O"), "fno");
    add_item(trading, tr("Paper Trading"), "paper_trading");
    add_item(trading, tr("Algo Trading"), "algo_trading");
    add_item(trading, tr("Backtesting"), "backtesting");

    // ── Research & Analytics ─────────────────────────────────────────────────
    auto* research = add_category(tr("RESEARCH & ANALYTICS"));
    add_item(research, tr("Equity Research"), "equity_research");
    add_item(research, tr("Surface Analytics"), "surface_analytics");
    add_item(research, tr("Derivatives"), "derivatives");
    add_item(research, tr("Portfolio"), "portfolio");
    add_item(research, tr("M&A Analytics"), "ma_analytics");

    // ── AI & Quantitative ────────────────────────────────────────────────────
    auto* ai = add_category(tr("AI & QUANTITATIVE"));
    add_item(ai, tr("AI Quant Lab"), "ai_quant_lab");
    add_item(ai, tr("QuantLib Suite"), "quantlib");
    add_item(ai, tr("AI Chat"), "ai_chat");
    add_item(ai, tr("Agent Config"), "agent_config");
    add_item(ai, tr("Alpha Arena"), "alpha_arena");
    add_item(ai, tr("Agent Config › Runs"), "agent_workbench");

    // ── Data Sources ─────────────────────────────────────────────────────────
    auto* data_cat = add_category(tr("DATA SOURCES"));
    add_item(data_cat, tr("DBnomics"), "dbnomics");
    add_item(data_cat, tr("Economics"), "economics");
    add_item(data_cat, tr("AkShare Data"), "akshare");
    add_item(data_cat, tr("Government Data"), "gov_data");
    add_item(data_cat, tr("Asia Markets"), "asia_markets");
    add_item(data_cat, tr("Trade Viz"), "trade_viz");
    add_item(data_cat, tr("Data Sources"), "data_sources");

    // ── Geopolitics & Alt ────────────────────────────────────────────────────
    auto* geo = add_category(tr("GEOPOLITICS & ALT"));
    add_item(geo, tr("Geopolitics"), "geopolitics");
    add_item(geo, tr("Maritime"), "maritime");
    add_item(geo, tr("Prediction Markets"), "polymarket");
    add_item(geo, tr("Alt Investments"), "alt_investments");
    add_item(geo, tr("Relationship Map"), "relationship_map");

    // ── Tools ────────────────────────────────────────────────────────────────
    auto* tools = add_category(tr("TOOLS"));
    add_item(tools, tr("Report Builder"), "report_builder");
    add_item(tools, tr("Node Editor"), "node_editor");
    add_item(tools, tr("Code Editor"), "code_editor");
    add_item(tools, tr("Excel"), "excel");
    add_item(tools, tr("Notes"), "notes");
    add_item(tools, tr("MCP Servers"), "mcp_servers");
    add_item(tools, tr("Data Mapping"), "data_mapping");
    add_item(tools, tr("File Manager"), "file_manager");

    // ── Account ──────────────────────────────────────────────────────────────
    auto* account = add_category(tr("ACCOUNT"));
    add_item(account, tr("Settings"), "settings");
    add_item(account, tr("Profile"), "profile");

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
    page_builders_.clear();

    // Each page is registered with an empty placeholder and built on first
    // show (ensure_page) — see page_builders_.
    auto add = [&](const QString& id, PageBuilder builder) {
        int idx = pages_->addWidget(new QWidget);
        page_index_[id] = idx;
        page_builders_.insert(id, builder);
    };

    // Getting Started
    add("welcome", &DocsScreen::page_welcome);
    add("getting_started", &DocsScreen::page_getting_started);
    add("shortcuts", &DocsScreen::page_keyboard_shortcuts);

    // Core
    add("dashboard", &DocsScreen::page_dashboard);
    add("markets", &DocsScreen::page_markets);
    add("news", &DocsScreen::page_news);
    add("watchlist", &DocsScreen::page_watchlist);
    add("screener", &DocsScreen::page_screener);

    // Trading
    add("crypto_trading", &DocsScreen::page_crypto_trading);
    add("crypto_center", &DocsScreen::page_crypto_center);
    add("equity_trading", &DocsScreen::page_equity_trading);
    add("fno", &DocsScreen::page_fno);
    add("paper_trading", &DocsScreen::page_paper_trading);
    add("algo_trading", &DocsScreen::page_algo_trading);
    add("backtesting", &DocsScreen::page_backtesting);

    // Research
    add("equity_research", &DocsScreen::page_equity_research);
    add("surface_analytics", &DocsScreen::page_surface_analytics);
    add("derivatives", &DocsScreen::page_derivatives);
    add("portfolio", &DocsScreen::page_portfolio);
    add("ma_analytics", &DocsScreen::page_ma_analytics);

    // AI
    add("ai_quant_lab", &DocsScreen::page_ai_quant_lab);
    add("quantlib", &DocsScreen::page_quantlib);
    add("ai_chat", &DocsScreen::page_ai_chat);
    add("agent_config", &DocsScreen::page_agent_config);
    add("alpha_arena", &DocsScreen::page_alpha_arena);
    add("agent_workbench", &DocsScreen::page_agent_workbench);

    // Data
    add("dbnomics", &DocsScreen::page_dbnomics);
    add("economics", &DocsScreen::page_economics);
    add("akshare", &DocsScreen::page_akshare);
    add("gov_data", &DocsScreen::page_gov_data);
    add("asia_markets", &DocsScreen::page_asia_markets);
    add("trade_viz", &DocsScreen::page_trade_viz);
    add("data_sources", &DocsScreen::page_data_sources);

    // Geopolitics
    add("geopolitics", &DocsScreen::page_geopolitics);
    add("maritime", &DocsScreen::page_maritime);
    add("polymarket", &DocsScreen::page_polymarket);
    add("alt_investments", &DocsScreen::page_alt_investments);
    add("relationship_map", &DocsScreen::page_relationship_map);

    // Tools
    add("report_builder", &DocsScreen::page_report_builder);
    add("node_editor", &DocsScreen::page_node_editor);
    add("code_editor", &DocsScreen::page_code_editor);
    add("excel", &DocsScreen::page_excel);
    add("notes", &DocsScreen::page_notes);
    add("mcp_servers", &DocsScreen::page_mcp_servers);
    add("data_mapping", &DocsScreen::page_data_mapping);
    add("file_manager", &DocsScreen::page_file_manager);

    // Account
    add("settings", &DocsScreen::page_settings);
    add("profile", &DocsScreen::page_profile);
}

QWidget* DocsScreen::ensure_page(const QString& section_id) {
    auto idx_it = page_index_.constFind(section_id);
    if (idx_it == page_index_.constEnd() || !pages_)
        return nullptr;
    const int idx = idx_it.value();
    auto builder = page_builders_.find(section_id);
    if (builder == page_builders_.end())
        return pages_->widget(idx); // already built
    const PageBuilder build = builder.value();
    page_builders_.erase(builder);
    QWidget* page = (this->*build)();
    QWidget* placeholder = pages_->widget(idx);
    const bool was_current = pages_->currentIndex() == idx;
    // Same index, so every other topic's stacked-widget index stays valid.
    pages_->insertWidget(idx, page);
    if (placeholder) {
        pages_->removeWidget(placeholder);
        placeholder->deleteLater();
    }
    if (was_current)
        pages_->setCurrentIndex(idx);
    index_page(section_id, page);
    return page;
}

void DocsScreen::navigate_to(const QString& section_id) {
    // The shortcuts page reads the live key bindings, so rebuild it on each visit.
    if (section_id == QLatin1String("shortcuts"))
        refresh_shortcuts_page();
    auto it = page_index_.find(section_id);
    if (it != page_index_.end()) {
        ensure_page(section_id);
        pages_->setCurrentIndex(it.value());
        current_topic_ = section_id;
        update_open_button();
    }
}

void DocsScreen::update_open_button() {
    if (!open_btn_)
        return;
    const QString target = docs_screen_target(current_topic_);
    open_btn_->setVisible(!target.isEmpty());
}

void DocsScreen::refresh_shortcuts_page() {
    auto it = page_index_.find(QStringLiteral("shortcuts"));
    if (it == page_index_.end() || !pages_)
        return;
    const int idx = it.value();
    QWidget* old_page = pages_->widget(idx);
    if (!old_page)
        return;
    const bool was_current = pages_->currentIndex() == idx;
    // Re-insert at the same index so every other topic's stacked-widget index
    // stays valid. (Replaces the placeholder too if the page was never built.)
    page_builders_.remove(QStringLiteral("shortcuts"));
    pages_->removeWidget(old_page);
    old_page->deleteLater();
    QWidget* page = page_keyboard_shortcuts();
    pages_->insertWidget(idx, page);
    if (was_current)
        pages_->setCurrentIndex(idx);
    index_page(QStringLiteral("shortcuts"), page);
}

// ── Topic count + search ─────────────────────────────────────────────────────

void DocsScreen::update_topic_count() {
    if (!cmd_count_ || !sidebar_)
        return;
    // Counted, not hardcoded — the literal said "35 TOPICS" while the tree had
    // 38, and would drift again on the next page added.
    int topics = 0;
    const int categories = sidebar_->topLevelItemCount();
    for (QTreeWidgetItemIterator it(sidebar_); *it; ++it)
        if (!(*it)->data(0, Qt::UserRole).toString().isEmpty())
            ++topics;
    cmd_count_->setText(tr("%1 TOPICS  |  %2 CATEGORIES").arg(topics).arg(categories));
}

void DocsScreen::index_page_text() {
    // Pages built so far; the rest are indexed as they are built (ensure_page)
    // or by the background pass the first search starts (index_next_page).
    page_text_.clear();
    if (!pages_)
        return;
    for (auto it = page_index_.cbegin(); it != page_index_.cend(); ++it) {
        if (!page_builders_.contains(it.key()))
            index_page(it.key(), pages_->widget(it.value()));
    }
}

void DocsScreen::index_page(const QString& section_id, QWidget* page) {
    if (!page)
        return;
    QString text;
    const auto labels = page->findChildren<QLabel*>();
    for (const QLabel* l : labels)
        text += l->text() + QLatin1Char('\n');
    page_text_.insert(section_id, text.toLower());
}

void DocsScreen::index_next_page() {
    // One page per event-loop turn, so typing stays responsive while the
    // search index fills in; each finished page re-runs the current search.
    if (page_builders_.isEmpty() || !search_input_ || search_input_->text().trimmed().isEmpty()) {
        const bool finished = indexing_ && page_builders_.isEmpty();
        indexing_ = false;
        // Every page is indexed now: settle the result ("no topic matches" is final).
        if (finished && search_input_ && !search_input_->text().trimmed().isEmpty())
            apply_search(search_input_->text());
        return;
    }
    ensure_page(page_builders_.constBegin().key());
    apply_search(search_input_->text());
    QTimer::singleShot(0, this, &DocsScreen::index_next_page);
}

bool DocsScreen::eventFilter(QObject* watched, QEvent* event) {
    if (sidebar_ && watched == sidebar_->viewport() && event->type() == QEvent::Resize && search_empty_)
        search_empty_->setGeometry(sidebar_->viewport()->rect());
    if (watched == search_input_ && event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent*>(event)->key();
        auto first_visible = [this]() -> QTreeWidgetItem* {
            for (QTreeWidgetItemIterator it(sidebar_); *it; ++it)
                if (!(*it)->isHidden() && !(*it)->data(0, Qt::UserRole).toString().isEmpty() &&
                    !((*it)->parent() && (*it)->parent()->isHidden()))
                    return *it;
            return nullptr;
        };
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            if (auto* item = first_visible())
                sidebar_->setCurrentItem(item); // navigates via currentItemChanged
            return true;
        }
        if (key == Qt::Key_Down) {
            if (auto* item = first_visible())
                sidebar_->setCurrentItem(item);
            sidebar_->setFocus();
            return true;
        }
        if (key == Qt::Key_Escape && !search_input_->text().isEmpty()) {
            search_input_->clear();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void DocsScreen::apply_search(const QString& text) {
    if (!sidebar_)
        return;
    const QString needle = text.trimmed();

    // Empty query restores the full tree.
    if (needle.isEmpty()) {
        for (int i = 0; i < sidebar_->topLevelItemCount(); ++i) {
            auto* cat = sidebar_->topLevelItem(i);
            cat->setHidden(false);
            cat->setExpanded(true);
            for (int j = 0; j < cat->childCount(); ++j)
                cat->child(j)->setHidden(false);
        }
        update_search_empty(false, QString());
        return;
    }

    // Pages not built yet have no text to search: build them in the background.
    if (!indexing_ && !page_builders_.isEmpty()) {
        indexing_ = true;
        QTimer::singleShot(0, this, &DocsScreen::index_next_page);
    }

    bool any_visible = false;
    for (int i = 0; i < sidebar_->topLevelItemCount(); ++i) {
        auto* cat = sidebar_->topLevelItem(i);
        const bool cat_hit = cat->text(0).contains(needle, Qt::CaseInsensitive);
        int visible_children = 0;
        for (int j = 0; j < cat->childCount(); ++j) {
            auto* child = cat->child(j);
            const QString id = child->data(0, Qt::UserRole).toString();
            const bool hit = cat_hit || child->text(0).contains(needle, Qt::CaseInsensitive) ||
                             id.contains(needle, Qt::CaseInsensitive) ||
                             page_text_.value(id).contains(needle.toLower());
            child->setHidden(!hit);
            if (hit)
                ++visible_children;
        }
        cat->setHidden(visible_children == 0);
        cat->setExpanded(true);
        any_visible = any_visible || visible_children > 0;
    }
    update_search_empty(!any_visible, needle);
}

void DocsScreen::update_search_empty(bool show, const QString& needle) {
    if (!show) {
        if (search_empty_)
            search_empty_->hide();
        return;
    }
    if (!search_empty_) {
        search_empty_ = new ui::EmptyState(sidebar_->viewport());
        search_empty_->set_compact(true);
        search_empty_->set_alignment(Qt::AlignTop);
        connect(search_empty_, &ui::EmptyState::primary_clicked, this, [this]() { search_input_->clear(); });
        // The suggestions are the three starting topics, in this order.
        connect(search_empty_, &ui::EmptyState::suggestion_clicked, this, [this](int index) {
            static const char* const kStartTopics[] = {"welcome", "getting_started", "shortcuts"};
            if (index < 0 || index >= 3)
                return;
            const QString id = QString::fromLatin1(kStartTopics[index]);
            search_input_->clear(); // restores the tree
            for (QTreeWidgetItemIterator it(sidebar_); *it; ++it)
                if ((*it)->data(0, Qt::UserRole).toString() == id) {
                    sidebar_->setCurrentItem(*it); // navigates via currentItemChanged
                    break;
                }
        });
    }
    ui::EmptyState::Spec spec = ui::EmptyState::no_data_spec(
        tr("No topics match"),
        indexing_ ? tr("Still reading the pages not opened yet for \"%1\"…").arg(needle)
                  : tr("No topic title or page text contains \"%1\".").arg(needle),
        tr("CLEAR SEARCH"));
    spec.glyph = QStringLiteral("⌕");
    spec.suggestions = {{tr("Welcome"), QString(), QString()},
                        {tr("First Steps"), QString(), QString()},
                        {tr("Keyboard Shortcuts"), QString(), QString()}};
    spec.suggestions_heading = tr("OR START WITH");
    search_empty_->apply(spec);
    search_empty_->setGeometry(sidebar_->viewport()->rect());
    search_empty_->show();
    search_empty_->raise();
}

// ============================================================================
// Constructor — Main layout
// ============================================================================

DocsScreen::DocsScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("docsScreen");
    // One stylesheet for the screen chrome, including the OPEN SCREEN button
    // (styled by objectName instead of a per-widget setStyleSheet()).
    setStyleSheet(QString("QWidget#docsScreen { background: %1; }"
                          "QPushButton#docsOpenBtn { background: %2; color: %3; border: 1px solid %4;"
                          "  padding: 2px 10px; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: bold; letter-spacing: 0.5px;"
                          "  }"
                          "QPushButton#docsOpenBtn:hover { background: %3; color: %5; }")
                      .arg(ui::colors::BG_BASE(), ui::colors::BG_RAISED(), ui::colors::AMBER(),
                           ui::colors::AMBER_DIM(), ui::colors::TEXT_ON_ACCENT()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Command bar ──────────────────────────────────────────────────────────
    auto* cmd = new QWidget(this);
    cmd->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                           .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    cmd->setMinimumHeight(32);
    auto* cmd_hl = new QHBoxLayout(cmd);
    cmd_hl->setContentsMargins(10, 0, 10, 0);
    cmd_hl->setSpacing(10);

    cmd_title_ = new QLabel(tr("DOCUMENTATION"));
    cmd_title_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: bold; letter-spacing: 1px;"
                                      " background: transparent;")
                                  .arg(ui::colors::AMBER()));
    cmd_hl->addWidget(cmd_title_);

    auto* sep = new QLabel("|");
    sep->setStyleSheet(QString("color: %1; background: transparent;")
                           .arg(ui::colors::BORDER_BRIGHT()));
    cmd_hl->addWidget(sep);

    // Brand + version string — shown verbatim, not translated. Read from the
    // running application rather than hardcoded: the literal here said v4.0.0
    // while the app shipped as 4.0.1.
    breadcrumb_ = new QLabel(QStringLiteral("FINCEPT TERMINAL v%1").arg(QCoreApplication::applicationVersion()));
    breadcrumb_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: bold;"
                                       " background: transparent; letter-spacing: 0.5px;")
                                   .arg(ui::colors::TEXT_SECONDARY()));
    cmd_hl->addWidget(breadcrumb_);

    // ── Search ────────────────────────────────────────────────────────────────
    // 38 topics with no way to search them. Filters the sidebar tree live;
    // Ctrl+F focuses it.
    search_input_ = new QLineEdit;
    search_input_->setFixedWidth(220);
    search_input_->setClearButtonEnabled(true);
    search_input_->setPlaceholderText(tr("Search topics…  (Ctrl+F)"));
    search_input_->setAccessibleName(tr("Search documentation topics"));
    search_input_->setStyleSheet(QString("QLineEdit { background: %1; color: %2; border: 1px solid %3;"
                                         "  padding: 2px 6px; font-size: " + fincept::ui::fonts::px(-3) + "px;"
                                         "  }"
                                         "QLineEdit:focus { border-color: %4; }")
                                     .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(),
                                          ui::colors::BORDER_DIM(), ui::colors::BORDER_BRIGHT()));
    search_input_->setToolTip(tr("Matches topic titles and page text. Enter opens the first match, Down moves "
                                 "into the list, Esc clears."));
    search_input_->installEventFilter(this);
    connect(search_input_, &QLineEdit::textChanged, this, &DocsScreen::apply_search);
    cmd_hl->addWidget(search_input_);

    cmd_hl->addStretch();

    // Jump from a documentation topic to the screen it describes. Hidden for
    // topics that are not a screen (Welcome, First Steps, Shortcuts).
    open_btn_ = new QPushButton(tr("OPEN SCREEN"));
    open_btn_->setObjectName("docsOpenBtn");
    open_btn_->setCursor(Qt::PointingHandCursor);
    open_btn_->setMinimumHeight(22);
    open_btn_->setToolTip(tr("Open the screen this topic describes"));
    open_btn_->setAccessibleName(tr("Open the screen this topic describes"));
    open_btn_->setVisible(false);
    connect(open_btn_, &QPushButton::clicked, this, [this]() {
        const QString target = docs_screen_target(current_topic_);
        if (!target.isEmpty())
            EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                         QVariantMap{{QStringLiteral("screen_id"), target}});
    });
    cmd_hl->addWidget(open_btn_);

    cmd_count_ = new QLabel;
    cmd_count_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent;")
                                  .arg(ui::colors::TEXT_TERTIARY()));
    cmd_hl->addWidget(cmd_count_);

    root->addWidget(cmd);

    // ── Splitter: sidebar + content ──────────────────────────────────────────
    build_sidebar();
    build_content_pages();
    index_page_text();
    update_topic_count();

    auto* find_sc = new QShortcut(QKeySequence::Find, this);
    find_sc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(find_sc, &QShortcut::activated, this, [this]() {
        search_input_->setFocus();
        search_input_->selectAll();
    });

    splitter_ = new QSplitter(Qt::Horizontal);
    splitter_->setStyleSheet(QString("QSplitter { background: %1; }"
                                     "QSplitter::handle { background: %2; width: 1px; }")
                                 .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));
    splitter_->addWidget(sidebar_);
    splitter_->addWidget(pages_);
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    splitter_->setSizes({220, 800});

    root->addWidget(splitter_, 1);

    // Default to welcome page
    navigate_to("welcome");

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        if (!sidebar_)
            return;
        for (int i = 0; i < sidebar_->topLevelItemCount(); ++i)
            docs_style_category(sidebar_->topLevelItem(i));
    });
}

// ============================================================================
// Re-translation
// ============================================================================
// The sidebar tree and every documentation page are static content composed of
// hundreds of labels. Rather than caching each one, we rebuild both on a
// language change and restore the section the user was reading.

void DocsScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void DocsScreen::retranslateUi() {
    // Command bar
    if (cmd_title_)
        cmd_title_->setText(tr("DOCUMENTATION"));
    if (search_input_)
        search_input_->setPlaceholderText(tr("Search topics…  (Ctrl+F)"));
    if (open_btn_) {
        open_btn_->setText(tr("OPEN SCREEN"));
        open_btn_->setToolTip(tr("Open the screen this topic describes"));
        open_btn_->setAccessibleName(tr("Open the screen this topic describes"));
    }

    // Rebuild sidebar + content pages so their tr() strings pick up the new
    // language. Preserve the currently displayed section across the rebuild.
    if (!splitter_ || !sidebar_ || !pages_)
        return;

    QString current_id;
    if (auto* cur = sidebar_->currentItem())
        current_id = cur->data(0, Qt::UserRole).toString();
    if (current_id.isEmpty()) {
        // Fall back to whatever page is on top of the stack.
        const int idx = pages_->currentIndex();
        for (auto it = page_index_.cbegin(); it != page_index_.cend(); ++it)
            if (it.value() == idx) {
                current_id = it.key();
                break;
            }
    }

    QTreeWidget* old_sidebar = sidebar_;
    QStackedWidget* old_pages = pages_;
    const QList<int> sizes = splitter_->sizes();

    page_index_.clear();
    build_sidebar();       // assigns a fresh sidebar_
    build_content_pages(); // assigns a fresh pages_
    index_page_text();

    splitter_->insertWidget(0, sidebar_);
    splitter_->insertWidget(1, pages_);
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    if (sizes.size() == 2)
        splitter_->setSizes(sizes);

    old_sidebar->deleteLater();
    old_pages->deleteLater();

    update_topic_count();
    if (search_input_)
        apply_search(search_input_->text()); // re-apply the active filter to the new tree

    // Restore the section the user was on (also re-selects the sidebar row via
    // the currentItemChanged → navigate_to wiring).
    if (!current_id.isEmpty()) {
        navigate_to(current_id);
        for (QTreeWidgetItemIterator it(sidebar_); *it; ++it) {
            if ((*it)->data(0, Qt::UserRole).toString() == current_id) {
                sidebar_->setCurrentItem(*it);
                break;
            }
        }
    } else {
        navigate_to("welcome");
    }
}

// ============================================================================
// ISectionLinked + search box rows (core/command/CommandRegistry.h)
// ============================================================================

bool DocsScreen::open_section(const QString& section, const QString& focus) {
    QString key = section.trimmed();
    if (key.isEmpty()) {
        // A focus alone: the topic search (filters the topic list; nothing else happens).
        const QString text = focus.trimmed();
        if (!text.isEmpty() && search_input_)
            search_input_->setText(text);
        return true;
    }
    QString rest;
    if (command_rows::split_kind(key, "page", &rest) || command_rows::split_kind(key, "topic", &rest))
        key = rest;
    QString id = page_index_.contains(key) ? key : QString();
    for (auto it = page_index_.cbegin(); id.isEmpty() && it != page_index_.cend(); ++it)
        if (it.key().compare(key, Qt::CaseInsensitive) == 0)
            id = it.key();
    if (id.isEmpty())
        return false;
    QTreeWidgetItem* item = nullptr;
    if (sidebar_) {
        for (QTreeWidgetItemIterator it(sidebar_); *it; ++it) {
            if ((*it)->data(0, Qt::UserRole).toString() == id) {
                item = *it;
                break;
            }
        }
    }
    // A topic search that hides the row is cleared first, so the list shows where the page is.
    if (item && search_input_ && (item->isHidden() || (item->parent() && item->parent()->isHidden())))
        search_input_->clear();
    if (item && sidebar_->currentItem() != item) {
        sidebar_->setCurrentItem(item); // the sidebar click path: currentItemChanged → navigate_to
        sidebar_->scrollToItem(item);
    } else if (current_topic_ != id) {
        navigate_to(id);
    }
    return true;
}

namespace {

struct DocsLinkRow {
    const char* topic;
    const char* title;
    const char* keywords;
};

// Every page of build_content_pages(); the seed's "docs.<topic>" rows are replaced (group help).
const DocsLinkRow kDocsLinkRows[] = {
    {"welcome", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Welcome guide"), "introduction, start, overview"},
    {"getting_started", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "First steps"),
     "getting started, beginner, tutorial, how to start"},
    {"shortcuts", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Keyboard shortcuts guide"),
     "keys, hotkeys, cheat sheet, keyboard"},
    {"dashboard", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Dashboard"), "widgets, home"},
    {"markets", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Markets"), "quotes, indices"},
    {"news", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: News"), "headlines, feeds"},
    {"watchlist", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Watchlist"), "alerts, lists"},
    {"screener", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Screener"), "stock screen, filter"},
    {"crypto_trading", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Crypto Trading"),
     "bitcoin, exchanges, kraken, binance"},
    {"crypto_center", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Crypto Center"), "crypto, wallet"},
    {"equity_trading", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Equity Trading"),
     "broker, stocks, orders"},
    {"fno", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: F&O"), "options, futures, option chain"},
    {"paper_trading", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Paper trading guide"),
     "paper, practice, simulated, demo account"},
    {"algo_trading", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Algo Trading"),
     "algorithmic, strategies, automation"},
    {"backtesting", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Backtesting"), "backtest, strategy test"},
    {"equity_research", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Equity Research"),
     "fundamentals, financials, company analysis"},
    {"surface_analytics", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Surface Analytics"),
     "volatility surface, 3d"},
    {"derivatives", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Derivatives"), "pricing, options pricer"},
    {"portfolio", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Portfolio"), "holdings, performance"},
    {"ma_analytics", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: M&A Analytics"),
     "mergers, acquisitions, valuation"},
    {"ai_quant_lab", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: AI Quant Lab"),
     "quant, machine learning, factors"},
    {"quantlib", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: QuantLib Suite"), "quantlib, pricing models"},
    {"ai_chat", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: AI Chat"), "assistant, ask ai"},
    {"agent_config", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Agent Config"),
     "agent studio, agents, teams, workflows"},
    {"alpha_arena", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Alpha Arena"), "competition, models"},
    {"agent_workbench", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Agent Config › Runs"),
     "agent workbench, claude code, codex, runs"},
    {"dbnomics", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: DBnomics"), "economic series"},
    {"economics", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Economics"), "macro, fred, world bank"},
    {"akshare", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: AkShare Data"), "china data"},
    {"gov_data", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Government Data"),
     "treasury, open data"},
    {"asia_markets", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Asia Markets"),
     "china stocks, hong kong"},
    {"trade_viz", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Trade Visualization"),
     "trade flows, comtrade"},
    {"data_sources", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Data Sources"),
     "connectors, connections"},
    {"geopolitics", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Geopolitics"), "conflicts, risk"},
    {"maritime", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Maritime"), "shipping, vessels, ports"},
    {"polymarket", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Prediction Markets"),
     "polymarket, kalshi"},
    {"alt_investments", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Alt. Investments"),
     "alternative investments, private markets, real estate"},
    {"relationship_map", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Relationship Map"),
     "company graph, holders"},
    {"report_builder", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Report Builder"), "reports, pdf"},
    {"node_editor", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Node Editor"), "workflow, nodes"},
    {"code_editor", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Code Editor"), "notebooks, python"},
    {"excel", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Excel"), "spreadsheet"},
    {"notes", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Notes"), "journal"},
    {"mcp_servers", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: MCP Servers"), "mcp, tools"},
    {"data_mapping", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Data Mapping"), "mapping, schema"},
    {"file_manager", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: File Manager"), "files, exports"},
    {"settings", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Settings"), "preferences, options"},
    {"profile", QT_TRANSLATE_NOOP("fincept::screens::DocsScreen", "Guide: Profile"), "account, billing"},
};

QList<command::CommandItem> docs_command_rows() {
    namespace rows = command_rows;
    const QString where = DocsScreen::tr("Docs");
    QList<command::CommandItem> out;
    for (const DocsLinkRow& r : kDocsLinkRows) {
        QStringList keywords;
        for (const QString& w : QString::fromLatin1(r.keywords).split(QLatin1Char(','), Qt::SkipEmptyParts))
            keywords.append(w.trimmed());
        keywords << QStringLiteral("how to") << QStringLiteral("docs") << QStringLiteral("help")
                 << QStringLiteral("guide");
        command::CommandItem it =
            rows::section(QStringLiteral("docs"), QString::fromLatin1(r.topic),
                          QCoreApplication::translate("fincept::screens::DocsScreen", r.title), keywords, where);
        it.group = QString::fromLatin1(command::groups::kHelp);
        out.append(it);
    }
    return out;
}

void docs_install_commands() {
    command_rows::install(QStringLiteral("docs"), &docs_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(docs_install_commands)

} // namespace fincept::screens
