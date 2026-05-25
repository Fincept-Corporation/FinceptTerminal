// McpServersScreen.cpp — Primary MCP control panel.
// Core: ctor, lifecycle, setup_ui, save/restore_state.
// Other concerns:
//   - McpServersScreen_Layout.cpp  — create_* views + populate + build_server_card
//   - McpServersScreen_Actions.cpp — on_* handlers + refresh + status bar
// McpServersScreen.cpp — Primary MCP control panel
// Marketplace: curated server cards with category filter + one-click add form pre-fill.
// Installed:   full-width server cards with inline status + actions, no splitter.
// Tools:       searchable table of all internal + external tools with enable/disable.

#include "screens/mcp_servers/McpServersScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "mcp/McpManager.h"
#include "mcp/McpMarketplace.h"
#include "mcp/McpProvider.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

// ── Stylesheet ────────────────────────────────────────────────────────────────


namespace {
using namespace fincept::ui;

inline QString kStyle() {
    return QString(

               // Screen / header
               "#mcpScreen  { background: %1; }"
               "#mcpHeader  { background: %2; border-bottom: 2px solid %3; }"
               "#mcpHeaderTitle { color:%4; font-weight:700; background:transparent; }"

               // Tab buttons
               "#mcpViewBtn { background:transparent; color:%5; border:1px solid %8; "
               "  font-weight:700; padding:4px 14px; }"
               "#mcpViewBtn:hover { color:%4; border-color:%9; }"
               "#mcpViewBtn[active=\"true\"] { background:%3; color:%1; border-color:%3; }"

               // Search / refresh
               "#mcpSearchInput { background:%1; color:%4; border:1px solid %8; "
               "  padding:4px 8px; min-width:200px; }"
               "#mcpSearchInput:focus { border-color:%9; }"
               "#mcpRefreshBtn { background:%7; color:%5; border:1px solid %8; "
               "  padding:4px 10px; font-weight:700; }"
               "#mcpRefreshBtn:hover { color:%4; }"

               // ── Marketplace cards ──
               "#mktCard { background:%7; border:1px solid %8; min-height:130px; }"
               "#mktCard:hover { border-color:%9; }"
               "#mktCardName  { color:%4; font-weight:700; background:transparent; }"
               "#mktCardDesc  { color:%5; background:transparent; }"
               "#mktCardCmd   { color:%13; font-family:monospace; background:transparent; }"
               "#mktCardEnv   { color:%3; background:transparent; }"
               "#mktCatBadge  { color:%3; font-weight:700; "
               "  background:rgba(217,119,6,0.12); padding:1px 6px; border:1px solid rgba(217,119,6,0.3); }"
               "#mktAddBtn    { background:%3; color:%1; border:none; padding:5px 16px; "
               "  font-weight:700; }"
               "#mktAddBtn:hover { background:%10; }"
               "#mktInstalledBadge { color:%6; font-weight:700; "
               "  background:rgba(22,163,74,0.12); padding:5px 12px; border:1px solid rgba(22,163,74,0.3); }"

               // Category filter sidebar
               "#catList { background:%2; border:none; outline:none; }"
               "#catList::item { color:%5; font-weight:700; padding:6px 12px; }"
               "#catList::item:hover { color:%4; background:%12; }"
               "#catList::item:selected { color:%3; background:rgba(217,119,6,0.1); "
               "  border-left:2px solid %3; }"

               // ── Installed server cards ──
               "#srvCard { background:%7; border:1px solid %8; margin:4px 0px; }"
               "#srvCard:hover { border-color:%9; }"
               "#srvCardName  { color:%4; font-weight:700; background:transparent; }"
               "#srvCardDesc  { color:%5; background:transparent; }"
               "#srvCardCmd   { color:%13; font-family:monospace; background:transparent; }"
               "#srvCardCat   { color:%5; background:transparent; }"
               "#pillRunning  { color:%6; font-weight:700; "
               "  background:rgba(22,163,74,0.15); padding:2px 10px; "
               "  border:1px solid rgba(22,163,74,0.4); }"
               "#pillStopped  { color:%14; font-weight:700; "
               "  background:rgba(220,38,38,0.10); padding:2px 10px; "
               "  border:1px solid rgba(220,38,38,0.4); }"
               "#pillError    { color:%14; font-weight:700; "
               "  background:rgba(220,38,38,0.10); padding:2px 10px; "
               "  border:1px solid rgba(220,38,38,0.4); }"
               "#pillAutoOn   { color:%6; background:transparent; }"
               "#pillAutoOff  { color:%11; background:transparent; }"

               // Inline card buttons
               "#cardToggleOn  { background:rgba(22,163,74,0.15); color:%6; "
               "  border:1px solid rgba(22,163,74,0.5); "
               "  padding:4px 12px; font-weight:700; }"
               "#cardToggleOn:hover  { background:rgba(22,163,74,0.25); }"
               "#cardToggleOff { background:%7; color:%5; border:1px solid %8; "
               "  padding:4px 12px; font-weight:700; }"
               "#cardToggleOff:hover { color:%4; border-color:%9; }"
               "#cardLogsBtn   { background:transparent; color:%5; border:1px solid %8; "
               "  padding:4px 12px; font-weight:700; }"
               "#cardLogsBtn:hover   { color:%4; }"
               "#cardRemoveBtn { background:transparent; color:%14; border:1px solid %14; "
               "  padding:4px 12px; font-weight:700; }"
               "#cardRemoveBtn:hover { background:rgba(220,38,38,0.12); }"

               // Add server button (full-width sticky)
               "#addSrvBtn { background:%3; color:%1; border:none; "
               "  padding:8px; font-weight:700; }"
               "#addSrvBtn:hover { background:%10; }"

               // Log expander inside card
               "QTextEdit { background:%1; color:%13; border:none; font-family:monospace; }"

               // Tools table
               "QTableWidget { background:%1; color:%4; border:none; gridline-color:%8; }"
               "QTableWidget::item { padding:2px 6px; border-bottom:1px solid %8; }"
               "QHeaderView::section { background:%2; color:%5; border:none; "
               "  border-bottom:1px solid %8; border-right:1px solid %8; "
               "  padding:4px 6px; font-weight:700; }"

               // Status bar
               "#mcpStatusBar { background:%2; border-top:1px solid %8; }"
               "#mcpStatusText      { color:%5;  background:transparent; }"
               "#mcpStatusHighlight { color:%13; background:transparent; }"

               // Scroll
               "QScrollBar:vertical { background:%1; width:6px; }"
               "QScrollBar::handle:vertical { background:%8; min-height:20px; }"
               "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
               "QScrollArea { border:none; background:transparent; }")
        .arg(colors::BG_BASE())        // %1
        .arg(colors::BG_RAISED())      // %2
        .arg(colors::AMBER())          // %3
        .arg(colors::TEXT_PRIMARY())   // %4
        .arg(colors::TEXT_SECONDARY()) // %5
        .arg(colors::POSITIVE())       // %6
        .arg(colors::BG_SURFACE())     // %7
        .arg(colors::BORDER_DIM())     // %8
        .arg(colors::BORDER_BRIGHT())  // %9
        .arg(colors::AMBER_DIM())      // %10
        .arg(colors::TEXT_DIM())       // %11
        .arg(colors::BG_HOVER())       // %12
        .arg(colors::CYAN())           // %13
        .arg(colors::NEGATIVE())       // %14
        ;
}

} // namespace

namespace fincept::screens {

using namespace fincept::mcp;
using namespace fincept::ui;

// ── Constructor ───────────────────────────────────────────────────────────────

McpServersScreen::McpServersScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("mcpScreen");
    setStyleSheet(kStyle());
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { setStyleSheet(kStyle()); });
    setup_ui();
    retranslateUi();

    connect(&McpManager::instance(), &McpManager::servers_changed, this, [this]() {
        if (!isVisible())
            return;
        if (active_view_ == 0)
            populate_marketplace();
        if (active_view_ == 1)
            refresh_installed();
        if (active_view_ == 2)
            refresh_tools();
        update_status_bar();
    });

    LOG_INFO("McpServers", "Screen constructed");
}

void McpServersScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!loaded_) {
        loaded_ = true;
        populate_marketplace();
        refresh_installed();
        update_status_bar();
    }
}

void McpServersScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
}

void McpServersScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

// ── Re-translation ───────────────────────────────────────────────────────────
// retranslateUi() handles static chrome (header title, view-tab buttons,
// sidebar header, search placeholder, refresh button, tools-view toolbar,
// status bar). Dynamic content — marketplace cards, installed server cards,
// tools rows — is rebuilt by populate_marketplace / refresh_installed /
// refresh_tools using current tr() values, so we just trigger the helper
// matching the active view.

void McpServersScreen::retranslateUi() {
    if (header_title_) header_title_->setText(tr("MCP SERVERS"));

    // View tabs: index order matches enum (Marketplace, Installed, Tools)
    const QStringList view_labels = {tr("MARKETPLACE"), tr("INSTALLED"), tr("TOOLS")};
    for (int i = 0; i < view_btns_.size() && i < view_labels.size(); ++i) {
        view_btns_[i]->setText(view_labels[i]);
    }

    if (search_input_) search_input_->setPlaceholderText(tr("Search..."));
    if (refresh_btn_)  refresh_btn_->setText(tr("↺  REFRESH"));

    if (cat_header_lbl_) cat_header_lbl_->setText(tr("  CATEGORY"));

    // Category list — user data holds the lowercase API key; display text is
    // re-set per row from its key. "all" maps to the localized "All" label.
    if (mkt_cat_list_) {
        for (int i = 0; i < mkt_cat_list_->count(); ++i) {
            auto* item = mkt_cat_list_->item(i);
            const QString key = item->data(Qt::UserRole).toString();
            if (key == "all")             item->setText(tr("ALL"));
            else if (key == "utilities")  item->setText(tr("UTILITIES"));
            else if (key == "developer")  item->setText(tr("DEVELOPER"));
            else if (key == "database")   item->setText(tr("DATABASE"));
            else                          item->setText(key.toUpper());
        }
    }

    if (add_server_btn_) add_server_btn_->setText(tr("＋  ADD CUSTOM MCP SERVER"));

    if (tools_toolbar_lbl_)
        tools_toolbar_lbl_->setText(
            tr("ALL TOOLS — internal + external  (check/uncheck to enable/disable internal tools)"));

    if (tools_table_)
        tools_table_->setHorizontalHeaderLabels(
            {tr("ON"), tr("TOOL NAME"), tr("SERVER"), tr("CATEGORY"), tr("DESCRIPTION")});

    if (status_screen_lbl_) status_screen_lbl_->setText(tr("MCP SERVERS"));

    if (status_view_) {
        // Re-derive from active_view_ rather than reading the (now-stale) text.
        const QStringList names = {tr("MARKETPLACE"), tr("INSTALLED"), tr("TOOLS")};
        if (active_view_ >= 0 && active_view_ < names.size())
            status_view_->setText(names[active_view_]);
    }

    // Re-render whichever view is active so cards/rows pick up new strings.
    if (loaded_) {
        if (active_view_ == 0) populate_marketplace();
        else if (active_view_ == 1) refresh_installed();
        else if (active_view_ == 2) refresh_tools();
        update_status_bar();
    }
}

// ── UI skeleton ───────────────────────────────────────────────────────────────

void McpServersScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(create_header());

    view_stack_ = new QStackedWidget;
    view_stack_->addWidget(create_marketplace_view()); // 0
    view_stack_->addWidget(create_installed_view());   // 1
    view_stack_->addWidget(create_tools_view());       // 2
    root->addWidget(view_stack_, 1);
    root->addWidget(create_status_bar());
}

QVariantMap McpServersScreen::save_state() const {
    QVariantMap state{{"active_view", active_view_}};
    if (search_input_) state["search"] = search_input_->text();
    return state;
}

void McpServersScreen::restore_state(const QVariantMap& state) {
    const int view = state.value("active_view", 0).toInt();
    if (view != active_view_)
        on_view_changed(view);
    if (search_input_ && state.contains("search"))
        search_input_->setText(state.value("search").toString());
}

} // namespace fincept::screens
