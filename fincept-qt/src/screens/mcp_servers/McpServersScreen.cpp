#include "screens/mcp_servers/McpServersScreen.h"

#include "core/logging/Logger.h"
#include "mcp/McpManager.h"
#include "storage/repositories/McpServerRepository.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

// ── Stylesheet ──────────────────────────────────────────────────────────────

namespace {
using namespace fincept::ui;

static const QString kStyle = QStringLiteral(
    "#mcpScreen { background: %1; }"

    "#mcpHeader { background: %2; border-bottom: 2px solid %3; }"
    "#mcpHeaderTitle { color: %4; font-size: 12px; font-weight: 700; background: transparent; }"
    "#mcpHeaderSub { color: %5; font-size: 9px; letter-spacing: 0.5px; background: transparent; }"

    "#mcpViewBtn { background: transparent; color: %5; border: 1px solid %8; "
    "  font-size: 9px; font-weight: 700; padding: 4px 12px; }"
    "#mcpViewBtn:hover { color: %4; }"
    "#mcpViewBtn[active=\"true\"] { background: %3; color: %1; border-color: %3; }"

    "#mcpSearchInput { background: %1; color: %4; border: 1px solid %8; "
    "  padding: 4px 8px; font-size: 11px; }"
    "#mcpSearchInput:focus { border-color: %9; }"

    "#mcpRefreshBtn { background: %7; color: %5; border: 1px solid %8; "
    "  padding: 4px 10px; font-size: 9px; font-weight: 700; }"
    "#mcpRefreshBtn:hover { color: %4; }"

    "#mcpList { background: %1; border: none; outline: none; font-size: 11px; }"
    "#mcpList::item { color: %5; padding: 8px 10px; border-bottom: 1px solid %8; }"
    "#mcpList::item:hover { color: %4; background: %12; }"
    "#mcpList::item:selected { color: %3; background: rgba(217,119,6,0.1); "
    "  border-left: 2px solid %3; }"

    "#mcpDetailPanel { background: %7; border-left: 1px solid %8; }"
    "#mcpDetailTitle { color: %4; font-size: 13px; font-weight: 700; background: transparent; }"
    "#mcpDetailLabel { color: %5; font-size: 9px; font-weight: 700; "
    "  letter-spacing: 0.5px; background: transparent; }"
    "#mcpDetailValue { color: %13; font-size: 11px; background: transparent; }"

    "#mcpStatusRunning { color: %6; font-size: 11px; font-weight: 700; "
    "  background: rgba(22,163,74,0.15); padding: 2px 8px; }"
    "#mcpStatusStopped { color: %14; font-size: 11px; font-weight: 700; "
    "  background: rgba(220,38,38,0.15); padding: 2px 8px; }"
    "#mcpStatusError { color: %14; font-size: 11px; font-weight: 700; "
    "  background: rgba(220,38,38,0.15); padding: 2px 8px; }"

    "#mcpActionBtn { background: %3; color: %1; border: none; padding: 5px 14px; "
    "  font-size: 9px; font-weight: 700; }"
    "#mcpActionBtn:hover { background: #FF8800; }"
    "#mcpActionBtn:disabled { background: %10; color: %11; }"

    "#mcpDangerBtn { background: rgba(220,38,38,0.1); color: %14; border: 1px solid %14; "
    "  padding: 5px 14px; font-size: 9px; font-weight: 700; }"
    "#mcpDangerBtn:hover { background: rgba(220,38,38,0.2); }"

    "#mcpSecondaryBtn { background: %7; color: %5; border: 1px solid %8; "
    "  padding: 5px 14px; font-size: 9px; font-weight: 700; }"
    "#mcpSecondaryBtn:hover { color: %4; }"

    "#mcpInstallBtn { background: %6; color: %1; border: none; padding: 4px 12px; "
    "  font-size: 9px; font-weight: 700; }"
    "#mcpInstallBtn:hover { background: #22c55e; }"

    "QTableWidget { background: %1; color: %4; border: none; gridline-color: %8; "
    "  font-size: 11px; }"
    "QTableWidget::item { padding: 2px 6px; border-bottom: 1px solid %8; }"
    "QHeaderView::section { background: %2; color: %5; border: none; "
    "  border-bottom: 1px solid %8; border-right: 1px solid %8; "
    "  padding: 4px 6px; font-size: 10px; font-weight: 700; }"

    "QTextEdit { background: %1; color: %13; border: none; font-size: 10px; }"

    "#mcpMarketplaceCard { background: %7; border: 1px solid %8; padding: 10px; }"
    "#mcpMarketplaceCard:hover { border-color: %9; }"
    "#mcpCardTitle { color: %4; font-size: 11px; font-weight: 700; background: transparent; }"
    "#mcpCardDesc { color: %5; font-size: 10px; background: transparent; }"
    "#mcpCardCategory { color: %3; font-size: 8px; font-weight: 700; "
    "  background: rgba(217,119,6,0.1); padding: 1px 6px; }"

    "#mcpStatusBar { background: %2; border-top: 1px solid %8; }"
    "#mcpStatusText { color: %5; font-size: 9px; background: transparent; }"
    "#mcpStatusHighlight { color: %13; font-size: 9px; background: transparent; }"

    "QSplitter::handle { background: %8; }"
    "QScrollBar:vertical { background: %1; width: 6px; }"
    "QScrollBar::handle:vertical { background: %8; min-height: 20px; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
)
    .arg(colors::BG_BASE)         // %1
    .arg(colors::BG_RAISED)       // %2
    .arg(colors::AMBER)           // %3
    .arg(colors::TEXT_PRIMARY)    // %4
    .arg(colors::TEXT_SECONDARY)  // %5
    .arg(colors::POSITIVE)        // %6
    .arg(colors::BG_SURFACE)      // %7
    .arg(colors::BORDER_DIM)      // %8
    .arg(colors::BORDER_BRIGHT)   // %9
    .arg(colors::AMBER_DIM)       // %10
    .arg(colors::TEXT_DIM)         // %11
    .arg(colors::BG_HOVER)        // %12
    .arg(colors::CYAN)             // %13
    .arg(colors::NEGATIVE)        // %14
    ;
} // namespace

namespace fincept::screens {

using namespace fincept::ui;
using namespace fincept::mcp;

// ── Preset marketplace servers ──────────────────────────────────────────────

struct MarketplaceServer {
    QString name;
    QString description;
    QString command;
    QString args;
    QString category;
};

static QList<MarketplaceServer> g_marketplace = {
    {"Filesystem", "Read/write files and directories",
     "npx", "-y @modelcontextprotocol/server-filesystem /path/to/dir", "utilities"},
    {"GitHub", "GitHub API integration — repos, issues, PRs",
     "npx", "-y @modelcontextprotocol/server-github", "developer"},
    {"PostgreSQL", "PostgreSQL database query and schema exploration",
     "npx", "-y @modelcontextprotocol/server-postgres postgres://localhost/mydb", "database"},
    {"Brave Search", "Web search via Brave Search API",
     "npx", "-y @modelcontextprotocol/server-brave-search", "search"},
    {"Puppeteer", "Browser automation and web scraping",
     "npx", "-y @modelcontextprotocol/server-puppeteer", "automation"},
    {"SQLite", "SQLite database operations",
     "npx", "-y @modelcontextprotocol/server-sqlite /path/to/db.sqlite", "database"},
    {"Memory", "Knowledge graph-based persistent memory",
     "npx", "-y @modelcontextprotocol/server-memory", "utilities"},
    {"Fetch", "HTTP fetch and web content retrieval",
     "npx", "-y @modelcontextprotocol/server-fetch", "utilities"},
    {"Sequential Thinking", "Dynamic problem-solving via thought sequences",
     "npx", "-y @modelcontextprotocol/server-sequential-thinking", "reasoning"},
    {"Google Maps", "Google Maps Platform integration",
     "npx", "-y @modelcontextprotocol/server-google-maps", "location"},
    {"Slack", "Slack workspace integration",
     "npx", "-y @modelcontextprotocol/server-slack", "communication"},
    {"Sentry", "Sentry issue tracking and error monitoring",
     "npx", "-y @modelcontextprotocol/server-sentry", "developer"},
    {"Playwright", "Browser testing and automation with Playwright",
     "npx", "-y @executeautomation/playwright-mcp-server", "automation"},
    {"Context7", "Up-to-date library documentation provider",
     "npx", "-y @upstash/context7-mcp", "developer"},
    {"Everything", "Local file search via Everything SDK",
     "npx", "-y @modelcontextprotocol/server-everything", "utilities"},
    {"Linear", "Linear project management integration",
     "npx", "-y @modelcontextprotocol/server-linear", "productivity"},
};

// ── Constructor ─────────────────────────────────────────────────────────────

McpServersScreen::McpServersScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("mcpScreen");
    setStyleSheet(kStyle);
    setup_ui();
    LOG_INFO("McpServers", "Screen constructed");
}

// ── UI Setup ────────────────────────────────────────────────────────────────

void McpServersScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(create_header());

    view_stack_ = new QStackedWidget;
    view_stack_->addWidget(create_marketplace_view());
    view_stack_->addWidget(create_installed_view());
    view_stack_->addWidget(create_tools_view());

    root->addWidget(view_stack_, 1);
    root->addWidget(create_status_bar());

    // Load initial data
    populate_marketplace();
    refresh_installed();
}

QWidget* McpServersScreen::create_header() {
    auto* bar = new QWidget;
    bar->setObjectName("mcpHeader");
    bar->setFixedHeight(42);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(10);

    auto* title = new QLabel("MCP SERVERS");
    title->setObjectName("mcpHeaderTitle");
    hl->addWidget(title);

    hl->addSpacing(12);

    const QStringList views = {"MARKETPLACE", "INSTALLED", "TOOLS"};
    for (int i = 0; i < views.size(); ++i) {
        auto* btn = new QPushButton(views[i]);
        btn->setObjectName("mcpViewBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_view_changed(i); });
        hl->addWidget(btn);
        view_btns_.append(btn);
    }

    hl->addSpacing(12);

    search_input_ = new QLineEdit;
    search_input_->setObjectName("mcpSearchInput");
    search_input_->setPlaceholderText("Search servers...");
    search_input_->setFixedWidth(200);
    connect(search_input_, &QLineEdit::textChanged, this, &McpServersScreen::on_search_changed);
    hl->addWidget(search_input_);

    hl->addStretch(1);

    refresh_btn_ = new QPushButton("REFRESH");
    refresh_btn_->setObjectName("mcpRefreshBtn");
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, &McpServersScreen::on_refresh);
    hl->addWidget(refresh_btn_);

    return bar;
}

QWidget* McpServersScreen::create_marketplace_view() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);

    auto* content = new QWidget;
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(8);

    marketplace_list_ = new QListWidget;
    marketplace_list_->setObjectName("mcpList");
    vl->addWidget(marketplace_list_);

    scroll->setWidget(content);
    return scroll;
}

QWidget* McpServersScreen::create_installed_view() {
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    // Left: server list
    auto* left = new QWidget;
    auto* lvl = new QVBoxLayout(left);
    lvl->setContentsMargins(0, 0, 0, 0);
    lvl->setSpacing(0);

    installed_list_ = new QListWidget;
    installed_list_->setObjectName("mcpList");
    connect(installed_list_, &QListWidget::itemClicked, this, &McpServersScreen::on_server_selected);
    lvl->addWidget(installed_list_, 1);
    left->setMinimumWidth(280);
    left->setMaximumWidth(380);

    // Right: detail panel
    detail_panel_ = new QWidget;
    detail_panel_->setObjectName("mcpDetailPanel");
    auto* dvl = new QVBoxLayout(detail_panel_);
    dvl->setContentsMargins(16, 16, 16, 16);
    dvl->setSpacing(10);

    server_name_ = new QLabel("Select a server");
    server_name_->setObjectName("mcpDetailTitle");
    dvl->addWidget(server_name_);

    server_status_ = new QLabel;
    server_status_->setObjectName("mcpStatusStopped");
    dvl->addWidget(server_status_);

    // Info rows
    auto make_info = [&](const QString& label, QLabel*& val) {
        auto* row = new QWidget;
        auto* rl = new QVBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(2);
        auto* lbl = new QLabel(label);
        lbl->setObjectName("mcpDetailLabel");
        val = new QLabel("--");
        val->setObjectName("mcpDetailValue");
        val->setWordWrap(true);
        rl->addWidget(lbl);
        rl->addWidget(val);
        dvl->addWidget(row);
    };

    make_info("COMMAND", server_command_);
    make_info("CATEGORY", server_category_);
    make_info("AUTO-START", server_autostart_);

    dvl->addSpacing(8);

    // Action buttons
    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(6);

    start_btn_ = new QPushButton("START");
    start_btn_->setObjectName("mcpActionBtn");
    start_btn_->setCursor(Qt::PointingHandCursor);
    connect(start_btn_, &QPushButton::clicked, this, &McpServersScreen::on_start_server);
    btn_row->addWidget(start_btn_);

    stop_btn_ = new QPushButton("STOP");
    stop_btn_->setObjectName("mcpSecondaryBtn");
    stop_btn_->setCursor(Qt::PointingHandCursor);
    connect(stop_btn_, &QPushButton::clicked, this, &McpServersScreen::on_stop_server);
    btn_row->addWidget(stop_btn_);

    autostart_btn_ = new QPushButton("TOGGLE AUTO-START");
    autostart_btn_->setObjectName("mcpSecondaryBtn");
    autostart_btn_->setCursor(Qt::PointingHandCursor);
    connect(autostart_btn_, &QPushButton::clicked, this, &McpServersScreen::on_toggle_autostart);
    btn_row->addWidget(autostart_btn_);

    btn_row->addStretch(1);
    dvl->addLayout(btn_row);

    auto* btn_row2 = new QHBoxLayout;
    btn_row2->setSpacing(6);

    logs_btn_ = new QPushButton("VIEW LOGS");
    logs_btn_->setObjectName("mcpSecondaryBtn");
    logs_btn_->setCursor(Qt::PointingHandCursor);
    connect(logs_btn_, &QPushButton::clicked, this, &McpServersScreen::on_view_logs);
    btn_row2->addWidget(logs_btn_);

    remove_btn_ = new QPushButton("REMOVE");
    remove_btn_->setObjectName("mcpDangerBtn");
    remove_btn_->setCursor(Qt::PointingHandCursor);
    connect(remove_btn_, &QPushButton::clicked, this, &McpServersScreen::on_remove_server);
    btn_row2->addWidget(remove_btn_);

    btn_row2->addStretch(1);
    dvl->addLayout(btn_row2);

    dvl->addSpacing(8);

    // Logs area
    logs_view_ = new QTextEdit;
    logs_view_->setReadOnly(true);
    logs_view_->setPlaceholderText("Server logs will appear here...");
    logs_view_->hide();
    dvl->addWidget(logs_view_, 1);

    dvl->addStretch(1);

    splitter->addWidget(left);
    splitter->addWidget(detail_panel_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({320, 700});

    return splitter;
}

QWidget* McpServersScreen::create_tools_view() {
    auto* panel = new QWidget;
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Toolbar
    auto* toolbar = new QWidget;
    toolbar->setFixedHeight(32);
    auto* tbl = new QHBoxLayout(toolbar);
    tbl->setContentsMargins(12, 0, 12, 0);

    auto* tt = new QLabel("AVAILABLE TOOLS");
    tt->setObjectName("mcpDetailLabel");
    tbl->addWidget(tt);
    tbl->addStretch(1);

    tools_count_ = new QLabel;
    tools_count_->setObjectName("mcpStatusText");
    tbl->addWidget(tools_count_);
    vl->addWidget(toolbar);

    tools_table_ = new QTableWidget;
    tools_table_->setColumnCount(4);
    tools_table_->setHorizontalHeaderLabels({"TOOL NAME", "SERVER", "DESCRIPTION", "STATUS"});
    tools_table_->horizontalHeader()->setStretchLastSection(true);
    tools_table_->verticalHeader()->setVisible(false);
    tools_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tools_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    vl->addWidget(tools_table_, 1);

    return panel;
}

QWidget* McpServersScreen::create_status_bar() {
    auto* bar = new QWidget;
    bar->setObjectName("mcpStatusBar");
    bar->setFixedHeight(26);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* left = new QLabel("MCP SERVERS");
    left->setObjectName("mcpStatusText");
    hl->addWidget(left);
    hl->addStretch(1);

    status_view_ = new QLabel("MARKETPLACE");
    status_view_->setObjectName("mcpStatusText");
    hl->addWidget(status_view_);

    hl->addSpacing(12);
    status_count_ = new QLabel;
    status_count_->setObjectName("mcpStatusText");
    hl->addWidget(status_count_);

    hl->addSpacing(12);
    status_running_ = new QLabel;
    status_running_->setObjectName("mcpStatusHighlight");
    hl->addWidget(status_running_);

    return bar;
}

// ── Slots ───────────────────────────────────────────────────────────────────

void McpServersScreen::on_view_changed(int view) {
    active_view_ = view;
    view_stack_->setCurrentIndex(view);

    for (int i = 0; i < view_btns_.size(); ++i) {
        view_btns_[i]->setProperty("active", i == view);
        view_btns_[i]->style()->unpolish(view_btns_[i]);
        view_btns_[i]->style()->polish(view_btns_[i]);
    }

    const QStringList names = {"MARKETPLACE", "INSTALLED", "TOOLS"};
    status_view_->setText(names[view]);

    if (view == 1) refresh_installed();
    if (view == 2) refresh_tools();

    LOG_INFO("McpServers", "View: " + names[view]);
}

void McpServersScreen::on_install_server(int index) {
    if (index < 0 || index >= g_marketplace.size()) return;

    const auto& ms = g_marketplace[index];

    McpServerConfig config;
    config.id = ms.name.toLower().replace(' ', '_');
    config.name = ms.name;
    config.description = ms.description;
    config.command = ms.command;
    config.args = ms.args.split(' ');
    config.category = ms.category;
    config.enabled = true;
    config.auto_start = false;

    auto result = McpManager::instance().save_server(config);
    if (result.is_ok()) {
        LOG_INFO("McpServers", "Installed: " + ms.name);
        on_view_changed(1); // switch to installed
    } else {
        LOG_ERROR("McpServers", "Install failed: " + QString::fromStdString(result.error()));
    }
}

void McpServersScreen::on_start_server() {
    if (selected_server_id_.isEmpty()) return;
    auto result = McpManager::instance().start_server(selected_server_id_);
    if (result.is_ok()) {
        LOG_INFO("McpServers", "Started: " + selected_server_id_);
    } else {
        LOG_ERROR("McpServers", "Start failed: " + QString::fromStdString(result.error()));
    }
    refresh_installed();
}

void McpServersScreen::on_stop_server() {
    if (selected_server_id_.isEmpty()) return;
    auto result = McpManager::instance().stop_server(selected_server_id_);
    if (result.is_ok()) {
        LOG_INFO("McpServers", "Stopped: " + selected_server_id_);
    } else {
        LOG_ERROR("McpServers", "Stop failed: " + QString::fromStdString(result.error()));
    }
    refresh_installed();
}

void McpServersScreen::on_remove_server() {
    if (selected_server_id_.isEmpty()) return;

    McpManager::instance().stop_server(selected_server_id_);
    auto result = McpManager::instance().remove_server(selected_server_id_);
    if (result.is_ok()) {
        selected_server_id_.clear();
        server_name_->setText("Select a server");
        server_status_->clear();
        LOG_INFO("McpServers", "Removed server");
    }
    refresh_installed();
}

void McpServersScreen::on_toggle_autostart() {
    if (selected_server_id_.isEmpty()) return;

    auto servers = McpManager::instance().get_servers();
    for (auto& s : servers) {
        if (s.id == selected_server_id_) {
            s.auto_start = !s.auto_start;
            McpManager::instance().save_server(s);
            server_autostart_->setText(s.auto_start ? "Yes" : "No");
            LOG_INFO("McpServers", "Auto-start toggled for: " + s.name);
            break;
        }
    }
}

void McpServersScreen::on_server_selected(QListWidgetItem* item) {
    if (!item) return;
    selected_server_id_ = item->data(Qt::UserRole).toString();

    auto servers = McpManager::instance().get_servers();
    for (const auto& s : servers) {
        if (s.id == selected_server_id_) {
            server_name_->setText(s.name);

            bool running = (s.status == ServerStatus::Running);
            server_status_->setText(running ? "RUNNING" : "STOPPED");
            server_status_->setObjectName(running ? "mcpStatusRunning" : "mcpStatusStopped");
            server_status_->style()->unpolish(server_status_);
            server_status_->style()->polish(server_status_);

            server_command_->setText(s.command + " " + s.args.join(" "));
            server_category_->setText(s.category);
            server_autostart_->setText(s.auto_start ? "Yes" : "No");

            start_btn_->setEnabled(!running);
            stop_btn_->setEnabled(running);
            break;
        }
    }
}

void McpServersScreen::on_refresh() {
    if (active_view_ == 0) populate_marketplace();
    else if (active_view_ == 1) refresh_installed();
    else refresh_tools();
}

void McpServersScreen::on_search_changed(const QString& text) {
    QListWidget* list = (active_view_ == 0) ? marketplace_list_ : installed_list_;
    for (int i = 0; i < list->count(); ++i) {
        auto* item = list->item(i);
        bool match = text.isEmpty() || item->text().contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

void McpServersScreen::on_view_logs() {
    logs_view_->setVisible(!logs_view_->isVisible());
    if (logs_view_->isVisible()) {
        logs_view_->setPlainText("Server logs are available in the application log file.\n"
                                  "Path: %APPDATA%/fincept-terminal/logs/\n\n"
                                  "MCP server stdio output is captured by McpClient.");
    }
}

// ── Data ────────────────────────────────────────────────────────────────────

void McpServersScreen::populate_marketplace() {
    marketplace_list_->clear();

    for (int i = 0; i < g_marketplace.size(); ++i) {
        const auto& ms = g_marketplace[i];
        QString text = ms.name + "\n" + ms.description + "\n[" + ms.category.toUpper() + "]";

        auto* item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, i);

        // Add install button via widget
        marketplace_list_->addItem(item);
    }

    // Add install buttons after items
    for (int i = 0; i < marketplace_list_->count(); ++i) {
        auto* w = new QWidget;
        auto* wl = new QHBoxLayout(w);
        wl->setContentsMargins(8, 4, 8, 4);
        wl->setSpacing(8);

        auto* info = new QLabel(g_marketplace[i].name + " — " + g_marketplace[i].description);
        info->setObjectName("mcpCardTitle");
        info->setWordWrap(true);
        wl->addWidget(info, 1);

        auto* cat_badge = new QLabel(g_marketplace[i].category.toUpper());
        cat_badge->setObjectName("mcpCardCategory");
        wl->addWidget(cat_badge);

        auto* install_btn = new QPushButton("INSTALL");
        install_btn->setObjectName("mcpInstallBtn");
        install_btn->setCursor(Qt::PointingHandCursor);
        install_btn->setFixedWidth(70);
        connect(install_btn, &QPushButton::clicked, this, [this, i]() { on_install_server(i); });
        wl->addWidget(install_btn);

        marketplace_list_->setItemWidget(marketplace_list_->item(i), w);
    }

    status_count_->setText(QString::number(g_marketplace.size()) + " available");
}

void McpServersScreen::refresh_installed() {
    installed_list_->clear();

    auto servers = McpManager::instance().get_servers();
    int running_count = 0;

    for (const auto& s : servers) {
        bool running = (s.status == ServerStatus::Running);
        if (running) running_count++;

        QString status_icon = running ? "[RUN]" : "[OFF]";
        auto* item = new QListWidgetItem(status_icon + " " + s.name + "\n" + s.description);
        item->setData(Qt::UserRole, s.id);
        if (running) {
            item->setForeground(QColor(colors::POSITIVE));
        }
        installed_list_->addItem(item);
    }

    status_count_->setText(QString::number(servers.size()) + " installed");
    status_running_->setText(QString::number(running_count) + " running");
}

void McpServersScreen::refresh_tools() {
    tools_table_->setSortingEnabled(false);
    tools_table_->setRowCount(0);

    auto tools = McpManager::instance().get_all_external_tools();

    tools_table_->setRowCount(static_cast<int>(tools.size()));
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        const auto& t = tools[i];
        tools_table_->setItem(i, 0, new QTableWidgetItem(t.name));
        tools_table_->setItem(i, 1, new QTableWidgetItem(t.server_id));
        tools_table_->setItem(i, 2, new QTableWidgetItem(t.description));

        auto* status_item = new QTableWidgetItem("READY");
        status_item->setForeground(QColor(colors::POSITIVE));
        tools_table_->setItem(i, 3, status_item);
    }

    tools_table_->resizeColumnsToContents();
    tools_table_->setSortingEnabled(true);
    tools_count_->setText(QString::number(tools.size()) + " tools");
}

} // namespace fincept::screens
