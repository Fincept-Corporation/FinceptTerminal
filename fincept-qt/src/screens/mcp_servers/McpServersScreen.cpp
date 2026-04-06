// McpServersScreen.cpp — Primary MCP control panel
// Marketplace: curated server cards with category filter + one-click add form pre-fill.
// Installed:   full-width server cards with inline status + actions, no splitter.
// Tools:       searchable table of all internal + external tools with enable/disable.

#include "screens/mcp_servers/McpServersScreen.h"

#include "core/logging/Logger.h"
#include "mcp/McpManager.h"
#include "mcp/McpProvider.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
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

static const QString kStyle = QStringLiteral(

    // Screen / header
    "#mcpScreen  { background: %1; }"
    "#mcpHeader  { background: %2; border-bottom: 2px solid %3; }"
    "#mcpHeaderTitle { color:%4; font-size:12px; font-weight:700; background:transparent; }"

    // Tab buttons
    "#mcpViewBtn { background:transparent; color:%5; border:1px solid %8; "
    "  font-size:9px; font-weight:700; padding:4px 14px; }"
    "#mcpViewBtn:hover { color:%4; border-color:%9; }"
    "#mcpViewBtn[active=\"true\"] { background:%3; color:%1; border-color:%3; }"

    // Search / refresh
    "#mcpSearchInput { background:%1; color:%4; border:1px solid %8; "
    "  padding:4px 8px; font-size:11px; min-width:200px; }"
    "#mcpSearchInput:focus { border-color:%9; }"
    "#mcpRefreshBtn { background:%7; color:%5; border:1px solid %8; "
    "  padding:4px 10px; font-size:9px; font-weight:700; }"
    "#mcpRefreshBtn:hover { color:%4; }"

    // ── Marketplace cards ──
    "#mktCard { background:%7; border:1px solid %8; min-height:130px; }"
    "#mktCard:hover { border-color:%9; }"
    "#mktCardName  { color:%4; font-size:11px; font-weight:700; background:transparent; }"
    "#mktCardDesc  { color:%5; font-size:10px; background:transparent; }"
    "#mktCardCmd   { color:%13; font-size:9px; font-family:monospace; background:transparent; }"
    "#mktCardEnv   { color:%3; font-size:9px; background:transparent; }"
    "#mktCatBadge  { color:%3; font-size:8px; font-weight:700; "
    "  background:rgba(217,119,6,0.12); padding:1px 6px; border:1px solid rgba(217,119,6,0.3); }"
    "#mktAddBtn    { background:%3; color:%1; border:none; padding:5px 16px; "
    "  font-size:9px; font-weight:700; }"
    "#mktAddBtn:hover { background:#FF8800; }"
    "#mktInstalledBadge { color:%6; font-size:9px; font-weight:700; "
    "  background:rgba(22,163,74,0.12); padding:5px 12px; border:1px solid rgba(22,163,74,0.3); }"

    // Category filter sidebar
    "#catList { background:%2; border:none; outline:none; }"
    "#catList::item { color:%5; font-size:9px; font-weight:700; padding:6px 12px; }"
    "#catList::item:hover { color:%4; background:%12; }"
    "#catList::item:selected { color:%3; background:rgba(217,119,6,0.1); "
    "  border-left:2px solid %3; }"

    // ── Installed server cards ──
    "#srvCard { background:%7; border:1px solid %8; margin:4px 0px; }"
    "#srvCard:hover { border-color:%9; }"
    "#srvCardName  { color:%4; font-size:12px; font-weight:700; background:transparent; }"
    "#srvCardDesc  { color:%5; font-size:10px; background:transparent; }"
    "#srvCardCmd   { color:%13; font-size:9px; font-family:monospace; background:transparent; }"
    "#srvCardCat   { color:%5; font-size:9px; background:transparent; }"
    "#pillRunning  { color:%6; font-size:9px; font-weight:700; "
    "  background:rgba(22,163,74,0.15); padding:2px 10px; "
    "  border:1px solid rgba(22,163,74,0.4); }"
    "#pillStopped  { color:%14; font-size:9px; font-weight:700; "
    "  background:rgba(220,38,38,0.10); padding:2px 10px; "
    "  border:1px solid rgba(220,38,38,0.4); }"
    "#pillError    { color:%14; font-size:9px; font-weight:700; "
    "  background:rgba(220,38,38,0.10); padding:2px 10px; "
    "  border:1px solid rgba(220,38,38,0.4); }"
    "#pillAutoOn   { color:%6; font-size:8px; background:transparent; }"
    "#pillAutoOff  { color:%11; font-size:8px; background:transparent; }"

    // Inline card buttons
    "#cardToggleOn  { background:rgba(22,163,74,0.15); color:%6; "
    "  border:1px solid rgba(22,163,74,0.5); "
    "  padding:4px 12px; font-size:9px; font-weight:700; }"
    "#cardToggleOn:hover  { background:rgba(22,163,74,0.25); }"
    "#cardToggleOff { background:%7; color:%5; border:1px solid %8; "
    "  padding:4px 12px; font-size:9px; font-weight:700; }"
    "#cardToggleOff:hover { color:%4; border-color:%9; }"
    "#cardLogsBtn   { background:transparent; color:%5; border:1px solid %8; "
    "  padding:4px 12px; font-size:9px; font-weight:700; }"
    "#cardLogsBtn:hover   { color:%4; }"
    "#cardRemoveBtn { background:transparent; color:%14; border:1px solid %14; "
    "  padding:4px 12px; font-size:9px; font-weight:700; }"
    "#cardRemoveBtn:hover { background:rgba(220,38,38,0.12); }"

    // Add server button (full-width sticky)
    "#addSrvBtn { background:%3; color:%1; border:none; "
    "  padding:8px; font-size:10px; font-weight:700; }"
    "#addSrvBtn:hover { background:#FF8800; }"

    // Log expander inside card
    "QTextEdit { background:%1; color:%13; border:none; font-size:10px; font-family:monospace; }"

    // Tools table
    "QTableWidget { background:%1; color:%4; border:none; gridline-color:%8; font-size:11px; }"
    "QTableWidget::item { padding:2px 6px; border-bottom:1px solid %8; }"
    "QHeaderView::section { background:%2; color:%5; border:none; "
    "  border-bottom:1px solid %8; border-right:1px solid %8; "
    "  padding:4px 6px; font-size:10px; font-weight:700; }"

    // Status bar
    "#mcpStatusBar { background:%2; border-top:1px solid %8; }"
    "#mcpStatusText      { color:%5;  font-size:9px; background:transparent; }"
    "#mcpStatusHighlight { color:%13; font-size:9px; background:transparent; }"

    // Scroll
    "QScrollBar:vertical { background:%1; width:6px; }"
    "QScrollBar::handle:vertical { background:%8; min-height:20px; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
    "QScrollArea { border:none; background:transparent; }"
)
.arg(colors::BG_BASE)        // %1
.arg(colors::BG_RAISED)      // %2
.arg(colors::AMBER)          // %3
.arg(colors::TEXT_PRIMARY)   // %4
.arg(colors::TEXT_SECONDARY) // %5
.arg(colors::POSITIVE)       // %6
.arg(colors::BG_SURFACE)     // %7
.arg(colors::BORDER_DIM)     // %8
.arg(colors::BORDER_BRIGHT)  // %9
.arg(colors::AMBER_DIM)      // %10
.arg(colors::TEXT_DIM)       // %11
.arg(colors::BG_HOVER)       // %12
.arg(colors::CYAN)           // %13
.arg(colors::NEGATIVE)       // %14
;
} // namespace

namespace fincept::screens {
using namespace fincept::ui;
using namespace fincept::mcp;

// ── Marketplace catalog ───────────────────────────────────────────────────────

struct MarketplaceEntry {
    QString name;
    QString description;
    QString command;
    QStringList args;
    QStringList env_keys;          // env var names the user must fill in
    QStringList env_placeholders;  // sample values shown as placeholder text
    QString category;
};

static const QList<MarketplaceEntry> g_catalog = {
    {
        "Fetch",
        "HTTP fetch and web content retrieval — lets the AI read any URL.",
        "uvx", {"mcp-server-fetch"}, {}, {}, "utilities"
    },
    {
        "Time",
        "Current time, timezone conversion and date arithmetic.",
        "uvx", {"mcp-server-time"}, {}, {}, "utilities"
    },
    {
        "Git",
        "Git repository operations: log, diff, commit, branch, status.",
        "uvx", {"mcp-server-git"}, {}, {}, "developer"
    },
    {
        "SQLite",
        "SQLite database query and schema inspection.",
        "uvx", {"mcp-server-sqlite"}, {}, {}, "database"
    },
    {
        "PostgreSQL",
        "Query and explore a PostgreSQL database schema and data.",
        "uvx", {"postgres-mcp", "--access-mode=unrestricted"},
        {"DATABASE_URI"}, {"postgresql://user:password@localhost:5432/mydb"},
        "database"
    },
};

// ── Constructor ───────────────────────────────────────────────────────────────

McpServersScreen::McpServersScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("mcpScreen");
    setStyleSheet(kStyle);
    setup_ui();

    connect(&McpManager::instance(), &McpManager::servers_changed, this, [this]() {
        if (!isVisible()) return;
        if (active_view_ == 0) populate_marketplace();
        if (active_view_ == 1) refresh_installed();
        if (active_view_ == 2) refresh_tools();
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

void McpServersScreen::hideEvent(QHideEvent* e) { QWidget::hideEvent(e); }

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

QWidget* McpServersScreen::create_header() {
    auto* bar = new QWidget;
    bar->setObjectName("mcpHeader");
    bar->setFixedHeight(44);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(8);

    auto* title = new QLabel("MCP SERVERS");
    title->setObjectName("mcpHeaderTitle");
    hl->addWidget(title);

    hl->addSpacing(16);

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

    hl->addStretch(1);

    search_input_ = new QLineEdit;
    search_input_->setObjectName("mcpSearchInput");
    search_input_->setPlaceholderText("Search...");
    connect(search_input_, &QLineEdit::textChanged, this, &McpServersScreen::on_search_changed);
    hl->addWidget(search_input_);

    hl->addSpacing(6);

    refresh_btn_ = new QPushButton("↺  REFRESH");
    refresh_btn_->setObjectName("mcpRefreshBtn");
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, &McpServersScreen::on_refresh);
    hl->addWidget(refresh_btn_);

    return bar;
}

// ── Marketplace view ──────────────────────────────────────────────────────────
// Layout: narrow category filter on left | scrollable card grid on right

QWidget* McpServersScreen::create_marketplace_view() {
    auto* root = new QWidget;
    auto* hl   = new QHBoxLayout(root);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    // Category sidebar
    auto* sidebar = new QWidget;
    sidebar->setFixedWidth(130);
    auto* svl = new QVBoxLayout(sidebar);
    svl->setContentsMargins(0, 0, 0, 0);
    svl->setSpacing(0);

    auto* cat_header = new QLabel("  CATEGORY");
    cat_header->setObjectName("mcpStatusText");
    cat_header->setFixedHeight(32);
    cat_header->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    svl->addWidget(cat_header);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color:%1;").arg(colors::BORDER_DIM));
    svl->addWidget(sep);

    mkt_cat_list_ = new QListWidget;
    mkt_cat_list_->setObjectName("catList");
    const QStringList cats = {"All", "utilities", "developer", "database"};
    for (const auto& c : cats)
        mkt_cat_list_->addItem(c.toUpper());
    mkt_cat_list_->setCurrentRow(0);
    connect(mkt_cat_list_, &QListWidget::currentRowChanged,
            this, [this](int) { populate_marketplace(); });
    svl->addWidget(mkt_cat_list_, 1);
    hl->addWidget(sidebar);

    // Vertical separator
    auto* vsep = new QFrame;
    vsep->setFrameShape(QFrame::VLine);
    vsep->setStyleSheet(QString("color:%1;").arg(colors::BORDER_DIM));
    hl->addWidget(vsep);

    // Card scroll area
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);

    mkt_cards_widget_ = new QWidget;
    mkt_cards_layout_ = new QVBoxLayout(mkt_cards_widget_);
    mkt_cards_layout_->setContentsMargins(16, 12, 16, 12);
    mkt_cards_layout_->setSpacing(8);
    mkt_cards_layout_->addStretch(1);

    scroll->setWidget(mkt_cards_widget_);
    hl->addWidget(scroll, 1);

    return root;
}

// ── Installed view ────────────────────────────────────────────────────────────
// Full-width card list — no splitter, no separate detail panel.

QWidget* McpServersScreen::create_installed_view() {
    auto* root = new QWidget;
    auto* vl   = new QVBoxLayout(root);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Scrollable card area
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);

    inst_cards_widget_ = new QWidget;
    inst_cards_layout_ = new QVBoxLayout(inst_cards_widget_);
    inst_cards_layout_->setContentsMargins(16, 12, 16, 8);
    inst_cards_layout_->setSpacing(6);
    inst_cards_layout_->addStretch(1);

    scroll->setWidget(inst_cards_widget_);
    vl->addWidget(scroll, 1);

    // Sticky "Add server" button at the bottom
    add_server_btn_ = new QPushButton("＋  ADD CUSTOM MCP SERVER");
    add_server_btn_->setObjectName("addSrvBtn");
    add_server_btn_->setCursor(Qt::PointingHandCursor);
    add_server_btn_->setFixedHeight(38);
    connect(add_server_btn_, &QPushButton::clicked, this, &McpServersScreen::on_add_server);
    vl->addWidget(add_server_btn_);

    return root;
}

// ── Tools view ────────────────────────────────────────────────────────────────

QWidget* McpServersScreen::create_tools_view() {
    auto* panel = new QWidget;
    auto* vl    = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* toolbar = new QWidget;
    toolbar->setFixedHeight(32);
    auto* tbl = new QHBoxLayout(toolbar);
    tbl->setContentsMargins(12, 0, 12, 0);
    auto* tt = new QLabel("ALL TOOLS — internal + external  (check/uncheck to enable/disable internal tools)");
    tt->setObjectName("mcpStatusText");
    tbl->addWidget(tt);
    tbl->addStretch(1);
    tools_count_ = new QLabel;
    tools_count_->setObjectName("mcpStatusText");
    tbl->addWidget(tools_count_);
    vl->addWidget(toolbar);

    tools_table_ = new QTableWidget;
    tools_table_->setColumnCount(5);
    tools_table_->setHorizontalHeaderLabels({"ON","TOOL NAME","SERVER","CATEGORY","DESCRIPTION"});
    tools_table_->horizontalHeader()->setStretchLastSection(true);
    tools_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    tools_table_->setColumnWidth(0, 42);
    tools_table_->verticalHeader()->setVisible(false);
    tools_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tools_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(tools_table_, &QTableWidget::cellChanged,
            this, &McpServersScreen::on_tool_enabled_changed);
    vl->addWidget(tools_table_, 1);

    return panel;
}

QWidget* McpServersScreen::create_status_bar() {
    auto* bar = new QWidget;
    bar->setObjectName("mcpStatusBar");
    bar->setFixedHeight(26);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    auto* lbl = new QLabel("MCP SERVERS");
    lbl->setObjectName("mcpStatusText");
    hl->addWidget(lbl);
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

// ── Slots ─────────────────────────────────────────────────────────────────────

void McpServersScreen::on_view_changed(int view) {
    active_view_ = view;
    view_stack_->setCurrentIndex(view);
    for (int i = 0; i < view_btns_.size(); ++i) {
        view_btns_[i]->setProperty("active", i == view);
        view_btns_[i]->style()->unpolish(view_btns_[i]);
        view_btns_[i]->style()->polish(view_btns_[i]);
    }
    const QStringList names = {"MARKETPLACE","INSTALLED","TOOLS"};
    status_view_->setText(names[view]);
    if (view == 0) populate_marketplace();
    if (view == 1) refresh_installed();
    if (view == 2) refresh_tools();
}

void McpServersScreen::on_install_server(int index) {
    if (index < 0 || index >= g_catalog.size()) return;
    const auto& e = g_catalog[index];

    // Pre-fill the add-server dialog with catalog data
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Add  " + e.name);
    dlg->setMinimumWidth(460);
    auto* form = new QFormLayout(dlg);
    form->setContentsMargins(16,16,16,8);
    form->setSpacing(10);

    auto* name_edit = new QLineEdit(e.name);
    form->addRow("Name", name_edit);

    auto* desc_edit = new QLineEdit(e.description);
    form->addRow("Description", desc_edit);

    auto* cmd_edit = new QLineEdit(e.command);
    form->addRow("Command", cmd_edit);

    auto* args_edit = new QLineEdit(e.args.join(' '));
    form->addRow("Arguments", args_edit);

    // Env vars — one field per required key with sample placeholder
    QList<QPair<QString,QLineEdit*>> env_fields;
    if (!e.env_keys.isEmpty()) {
        auto* env_header = new QLabel("Environment Variables");
        env_header->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;")
                                      .arg(colors::TEXT_SECONDARY));
        form->addRow(env_header);
        for (int ki = 0; ki < e.env_keys.size(); ++ki) {
            const QString& key = e.env_keys[ki];
            auto* field = new QLineEdit;
            if (ki < e.env_placeholders.size()) {
                // Pre-fill with sample so user edits in-place rather than typing blind
                field->setText(e.env_placeholders[ki]);
                field->selectAll(); // highlight so first keystroke replaces it
            } else {
                field->setPlaceholderText("Enter " + key);
            }
            form->addRow(key, field);
            env_fields.append({key, field});
        }
    }

    auto* cat_combo = new QComboBox;
    cat_combo->addItems({"utilities", "developer", "database"});
    cat_combo->setCurrentText(e.category);
    form->addRow("Category", cat_combo);

    auto* autostart_check = new QCheckBox("Auto-start on launch");
    form->addRow("", autostart_check);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg->exec() != QDialog::Accepted) { dlg->deleteLater(); return; }

    const QString name = name_edit->text().trimmed();
    if (name.isEmpty()) { dlg->deleteLater(); return; }

    McpServerConfig cfg;
    cfg.id          = name.toLower().replace(' ','_');
    cfg.name        = name;
    cfg.description = desc_edit->text().trimmed();
    cfg.command     = cmd_edit->text().trimmed();
    cfg.args        = args_edit->text().trimmed().split(' ', Qt::SkipEmptyParts);
    cfg.category    = cat_combo->currentText();
    cfg.enabled     = true;
    cfg.auto_start  = autostart_check->isChecked();
    for (const auto& [key, field] : env_fields) {
        const QString val = field->text().trimmed();
        if (!val.isEmpty()) cfg.env[key] = val;
    }

    const auto r = McpManager::instance().save_server(cfg);
    if (r.is_ok()) {
        LOG_INFO("McpServers", "Added from catalog: " + name);
        populate_marketplace();
        on_view_changed(1);
    } else {
        LOG_ERROR("McpServers", "Add failed: " + QString::fromStdString(r.error()));
    }
    dlg->deleteLater();
}

void McpServersScreen::on_start_server() {
    if (selected_server_id_.isEmpty()) return;
    const QString id = selected_server_id_;
    QPointer<McpServersScreen> self = this;
    QtConcurrent::run([id, self]() {
        const auto result = McpManager::instance().start_server(id);
        QMetaObject::invokeMethod(self, [self, result]() {
            if (!self) return;
            if (result.is_err())
                LOG_ERROR("McpServers", "Start failed: " + QString::fromStdString(result.error()));
            self->refresh_installed();
            self->update_status_bar();
        }, Qt::QueuedConnection);
    });
}

void McpServersScreen::on_stop_server() {
    if (selected_server_id_.isEmpty()) return;
    const QString id = selected_server_id_;
    QPointer<McpServersScreen> self = this;
    QtConcurrent::run([id, self]() {
        McpManager::instance().stop_server(id);
        QMetaObject::invokeMethod(self, [self]() {
            if (!self) return;
            self->refresh_installed();
            self->update_status_bar();
        }, Qt::QueuedConnection);
    });
}

void McpServersScreen::on_remove_server() {
    if (selected_server_id_.isEmpty()) return;
    const QString id = selected_server_id_;
    QString name;
    for (const auto& s : McpManager::instance().get_servers())
        if (s.id == id) { name = s.name; break; }

    QMessageBox mb(QMessageBox::Question, "Remove Server",
                   "Remove \"" + name + "\"?\nThis cannot be undone.",
                   QMessageBox::Yes | QMessageBox::Cancel, this);
    if (mb.exec() != QMessageBox::Yes) return;

    McpManager::instance().stop_server(id);
    McpManager::instance().remove_server(id);
    selected_server_id_.clear();
    refresh_installed();
    update_status_bar();
}

void McpServersScreen::on_toggle_autostart() {
    if (selected_server_id_.isEmpty()) return;
    auto servers = McpManager::instance().get_servers();
    for (auto& s : servers) {
        if (s.id != selected_server_id_) continue;
        s.auto_start = !s.auto_start;
        McpManager::instance().save_server(s);
        refresh_installed();
        break;
    }
}

void McpServersScreen::on_server_selected(QListWidgetItem*) {}  // unused — cards are self-contained

void McpServersScreen::on_refresh() {
    if (active_view_ == 0)      populate_marketplace();
    else if (active_view_ == 1) refresh_installed();
    else                         refresh_tools();
    update_status_bar();
}

void McpServersScreen::on_search_changed(const QString& text) {
    if (active_view_ == 2) {
        for (int i = 0; i < tools_table_->rowCount(); ++i) {
            auto* n = tools_table_->item(i,1);
            auto* s = tools_table_->item(i,2);
            auto* d = tools_table_->item(i,4);
            const bool match = text.isEmpty()
                || (n && n->text().contains(text, Qt::CaseInsensitive))
                || (s && s->text().contains(text, Qt::CaseInsensitive))
                || (d && d->text().contains(text, Qt::CaseInsensitive));
            tools_table_->setRowHidden(i, !match);
        }
        return;
    }
    // Marketplace / installed: re-populate with filter
    if (active_view_ == 0) populate_marketplace();
    if (active_view_ == 1) refresh_installed();
}

void McpServersScreen::on_view_logs() {
    // Handled inline per card — this slot kept for compat
}

void McpServersScreen::on_add_server() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Add Custom MCP Server");
    dlg->setMinimumWidth(460);
    auto* form = new QFormLayout(dlg);
    form->setContentsMargins(16,16,16,8);
    form->setSpacing(10);

    auto* name_edit = new QLineEdit;
    name_edit->setPlaceholderText("e.g. My Custom Server");
    form->addRow("Name", name_edit);

    auto* desc_edit = new QLineEdit;
    desc_edit->setPlaceholderText("Short description");
    form->addRow("Description", desc_edit);

    auto* cmd_edit = new QLineEdit("uvx");
    form->addRow("Command", cmd_edit);

    auto* args_edit = new QLineEdit;
    args_edit->setPlaceholderText("e.g. my-mcp-package --flag value");
    form->addRow("Arguments", args_edit);

    auto* env_edit = new QLineEdit;
    env_edit->setPlaceholderText("KEY=value KEY2=value2");
    form->addRow("Env Vars", env_edit);

    auto* cat_combo = new QComboBox;
    cat_combo->addItems({"utilities", "developer", "database"});
    form->addRow("Category", cat_combo);

    auto* autostart_check = new QCheckBox("Auto-start on launch");
    form->addRow("", autostart_check);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg->exec() != QDialog::Accepted) { dlg->deleteLater(); return; }

    const QString name = name_edit->text().trimmed();
    if (name.isEmpty()) { dlg->deleteLater(); return; }

    McpServerConfig cfg;
    cfg.id          = name.toLower().replace(' ','_');
    cfg.name        = name;
    cfg.description = desc_edit->text().trimmed();
    cfg.command     = cmd_edit->text().trimmed();
    cfg.args        = args_edit->text().trimmed().split(' ', Qt::SkipEmptyParts);
    cfg.category    = cat_combo->currentText();
    cfg.enabled     = true;
    cfg.auto_start  = autostart_check->isChecked();

    for (const auto& pair : env_edit->text().trimmed().split(' ', Qt::SkipEmptyParts)) {
        const int eq = pair.indexOf('=');
        if (eq > 0) cfg.env[pair.left(eq)] = pair.mid(eq+1);
    }

    const auto r = McpManager::instance().save_server(cfg);
    if (r.is_ok()) {
        LOG_INFO("McpServers", "Added custom server: " + name);
        refresh_installed();
        update_status_bar();
    } else {
        LOG_ERROR("McpServers", "Failed: " + QString::fromStdString(r.error()));
    }
    dlg->deleteLater();
}

void McpServersScreen::on_tool_enabled_changed(int row, int col) {
    if (col != 0) return;
    auto* ci = tools_table_->item(row,0);
    auto* ni = tools_table_->item(row,1);
    auto* si = tools_table_->item(row,2);
    if (!ci || !ni || !si) return;
    if (si->data(Qt::UserRole).toString() != QString(INTERNAL_SERVER_ID)) return;
    const bool enabled = (ci->checkState() == Qt::Checked);
    McpProvider::instance().set_tool_enabled(ni->text(), enabled);
    LOG_INFO("McpServers", QString("Tool '%1' %2").arg(ni->text(), enabled?"enabled":"disabled"));
}

// ── Data helpers ──────────────────────────────────────────────────────────────

void McpServersScreen::populate_marketplace() {
    // Determine active category filter
    const QString filter_raw = mkt_cat_list_
        ? mkt_cat_list_->currentItem()
              ? mkt_cat_list_->currentItem()->text().toLower()
              : "all"
        : "all";
    const QString filter = (filter_raw == "all") ? "" : filter_raw;

    const QString search = search_input_ ? search_input_->text().trimmed() : "";

    // Build installed-id set for badge display
    QSet<QString> installed_ids;
    for (const auto& s : McpManager::instance().get_servers())
        installed_ids.insert(s.id);

    // Clear old content (keep stretch at end)
    while (mkt_cards_layout_->count() > 1) {
        auto* item = mkt_cards_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    // Collect matching entries
    QList<int> matched;
    for (int i = 0; i < g_catalog.size(); ++i) {
        const auto& e = g_catalog[i];
        if (!filter.isEmpty() && e.category != filter) continue;
        if (!search.isEmpty()
            && !e.name.contains(search, Qt::CaseInsensitive)
            && !e.description.contains(search, Qt::CaseInsensitive)
            && !e.category.contains(search, Qt::CaseInsensitive))
            continue;
        matched.append(i);
    }

    if (matched.isEmpty()) {
        auto* empty = new QLabel("No servers match the current filter.");
        empty->setObjectName("mktCardDesc");
        empty->setAlignment(Qt::AlignCenter);
        mkt_cards_layout_->insertWidget(0, empty);
        return;
    }

    // Build rows of 3 cards each
    constexpr int COLS = 3;
    for (int row = 0; row < (matched.size() + COLS - 1) / COLS; ++row) {
        auto* row_widget = new QWidget;
        auto* row_hl = new QHBoxLayout(row_widget);
        row_hl->setContentsMargins(0, 0, 0, 0);
        row_hl->setSpacing(10);

        for (int col = 0; col < COLS; ++col) {
            const int idx = row * COLS + col;
            if (idx >= matched.size()) {
                // Filler to keep grid aligned
                row_hl->addStretch(1);
                continue;
            }
            const int i = matched[idx];
            const auto& e = g_catalog[i];
            const QString expected_id = e.name.toLower().replace(' ', '_');
            const bool installed = installed_ids.contains(expected_id);

            // ── Card ──
            auto* card = new QWidget;
            card->setObjectName("mktCard");
            card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

            auto* vl = new QVBoxLayout(card);
            vl->setContentsMargins(14, 14, 14, 12);
            vl->setSpacing(6);

            // Name row
            auto* name_lbl = new QLabel(e.name);
            name_lbl->setObjectName("mktCardName");
            vl->addWidget(name_lbl);

            // Description (2 lines max via elide)
            auto* desc_lbl = new QLabel(e.description);
            desc_lbl->setObjectName("mktCardDesc");
            desc_lbl->setWordWrap(true);
            desc_lbl->setMaximumHeight(42); // ~2 lines
            vl->addWidget(desc_lbl);

            vl->addStretch(1);

            // Command line
            auto* cmd_lbl = new QLabel(e.command + " " + e.args.join(' '));
            cmd_lbl->setObjectName("mktCardCmd");
            cmd_lbl->setWordWrap(false);
            vl->addWidget(cmd_lbl);

            // Bottom row: category badge + action button
            auto* bottom = new QHBoxLayout;
            bottom->setSpacing(6);
            bottom->setContentsMargins(0, 4, 0, 0);

            auto* cat_badge = new QLabel(e.category.toUpper());
            cat_badge->setObjectName("mktCatBadge");
            bottom->addWidget(cat_badge);

            if (!e.env_keys.isEmpty()) {
                auto* env_lbl = new QLabel("Needs: " + e.env_keys.join(", "));
                env_lbl->setObjectName("mktCardEnv");
                bottom->addWidget(env_lbl);
            }

            bottom->addStretch(1);

            if (installed) {
                auto* badge = new QLabel("✓ ADDED");
                badge->setObjectName("mktInstalledBadge");
                bottom->addWidget(badge);
            } else {
                auto* add_btn = new QPushButton("ADD");
                add_btn->setObjectName("mktAddBtn");
                add_btn->setCursor(Qt::PointingHandCursor);
                add_btn->setFixedWidth(56);
                connect(add_btn, &QPushButton::clicked, this, [this, i]() { on_install_server(i); });
                bottom->addWidget(add_btn);
            }

            vl->addLayout(bottom);
            row_hl->addWidget(card, 1);
        }

        mkt_cards_layout_->insertWidget(mkt_cards_layout_->count() - 1, row_widget);
    }
}

// ── build_server_card ─────────────────────────────────────────────────────────
// Enable toggle IS start/stop — no manual start/stop buttons.
// Any part of the app can call McpManager::call_external_tool() on enabled servers.

QWidget* McpServersScreen::build_server_card(const McpServerConfig& s) {
    const bool starting = (s.status == ServerStatus::Starting);
    const bool running  = (s.status == ServerStatus::Running);
    const bool error    = (s.status == ServerStatus::Error);

    auto* card = new QWidget;
    card->setObjectName("srvCard");

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(16, 14, 16, 12);
    vl->setSpacing(6);

    // ── Row 1: name + status pill + enable toggle (right) ──
    auto* top = new QHBoxLayout;
    top->setSpacing(10);

    auto* name_lbl = new QLabel(s.name);
    name_lbl->setObjectName("srvCardName");
    top->addWidget(name_lbl);

    auto* status_pill = new QLabel(running ? "● RUNNING" : (starting ? "⟳ STARTING" : (error ? "● ERROR" : "○ STOPPED")));
    status_pill->setObjectName(running ? "pillRunning" : (starting ? "pillRunning" : (error ? "pillError" : "pillStopped")));
    top->addWidget(status_pill);

    top->addStretch(1);

    // Enable toggle — toggling ON starts the server off-thread, OFF stops it
    const QString sid = s.id;
    QPointer<McpServersScreen> self = this;

    auto* toggle_btn = new QPushButton(running ? "● ENABLED" : (starting ? "⟳ STARTING..." : "○ DISABLED"));
    toggle_btn->setObjectName(running ? "cardToggleOn" : "cardToggleOff");
    toggle_btn->setCursor(Qt::PointingHandCursor);
    toggle_btn->setFixedWidth(110);
    if (starting)
        toggle_btn->setEnabled(false);
    connect(toggle_btn, &QPushButton::clicked, this, [sid, self, running, toggle_btn]() {
        if (!self) return;
        if (running) {
            // Disable — stop is fast, safe on main thread
            McpManager::instance().stop_server(sid);
            self->refresh_installed();
            self->update_status_bar();
        } else {
            // Enable — start on background thread to avoid freezing the UI.
            // start_server() blocks for process launch + handshake (can take 60s+).
            toggle_btn->setText("⟳ STARTING...");
            toggle_btn->setEnabled(false);
            QtConcurrent::run([sid, self]() {
                const auto r = McpManager::instance().start_server(sid);
                if (!self) return;
                QMetaObject::invokeMethod(self, [self, r]() {
                    if (!self) return;
                    if (r.is_err()) {
                        const QString msg = QString::fromStdString(r.error());
                        LOG_ERROR("McpServers", "Start failed: " + msg);
                        QMessageBox::warning(self, "Server Failed to Start", msg);
                    }
                    self->refresh_installed();
                    self->update_status_bar();
                }, Qt::QueuedConnection);
            });
        }
    });
    top->addWidget(toggle_btn);
    vl->addLayout(top);

    // ── Row 2: description ──
    if (!s.description.isEmpty()) {
        auto* desc = new QLabel(s.description);
        desc->setObjectName("srvCardDesc");
        desc->setWordWrap(true);
        vl->addWidget(desc);
    }

    // ── Row 3: command + category ──
    auto* meta = new QHBoxLayout;
    meta->setSpacing(16);
    auto* cmd_lbl = new QLabel(s.command + " " + s.args.join(' '));
    cmd_lbl->setObjectName("srvCardCmd");
    meta->addWidget(cmd_lbl);
    auto* cat_lbl = new QLabel(s.category.toUpper());
    cat_lbl->setObjectName("srvCardCat");
    meta->addWidget(cat_lbl);
    meta->addStretch(1);
    vl->addLayout(meta);

    // ── Row 4: LOGS + REMOVE only ──
    auto* btns = new QHBoxLayout;
    btns->setSpacing(6);

    // LOGS expander — fetch off-thread to avoid mutex stall
    QTextEdit* log_view = new QTextEdit;
    log_view->setReadOnly(true);
    log_view->setFixedHeight(140);
    log_view->hide();

    auto* logs_btn = new QPushButton("LOGS");
    logs_btn->setObjectName("cardLogsBtn");
    logs_btn->setCursor(Qt::PointingHandCursor);
    connect(logs_btn, &QPushButton::clicked, this, [sid, log_view, self]() {
        if (!self) return;
        const bool showing = !log_view->isVisible();
        log_view->setVisible(showing);
        if (!showing) return;
        // Fetch logs off-thread so mutex never stalls UI
        QPointer<QTextEdit> lv = log_view;
        QtConcurrent::run([sid, lv]() {
            const QStringList lines = McpManager::instance().get_logs(sid);
            QMetaObject::invokeMethod(lv, [lv, lines]() {
                if (!lv) return;
                lv->setPlainText(lines.isEmpty()
                    ? "No output yet. Start the server to see logs here."
                    : lines.join('\n'));
                QScrollBar* sb = lv->verticalScrollBar();
                if (sb) sb->setValue(sb->maximum());
            }, Qt::QueuedConnection);
        });
    });
    btns->addWidget(logs_btn);

    btns->addStretch(1);

    // REMOVE
    auto* remove_btn = new QPushButton("REMOVE");
    remove_btn->setObjectName("cardRemoveBtn");
    remove_btn->setCursor(Qt::PointingHandCursor);
    connect(remove_btn, &QPushButton::clicked, this, [sid, self]() {
        if (!self) return;
        QString name;
        for (const auto& srv : McpManager::instance().get_servers())
            if (srv.id == sid) { name = srv.name; break; }
        QMessageBox mb(QMessageBox::Question, "Remove Server",
                       "Remove \"" + name + "\"?\nThis cannot be undone.",
                       QMessageBox::Yes | QMessageBox::Cancel, self);
        if (mb.exec() != QMessageBox::Yes) return;
        McpManager::instance().stop_server(sid);
        McpManager::instance().remove_server(sid);
        self->refresh_installed();
        self->update_status_bar();
    });
    btns->addWidget(remove_btn);

    vl->addLayout(btns);
    vl->addWidget(log_view);

    return card;
}

void McpServersScreen::refresh_installed() {
    const QString search = search_input_ ? search_input_->text().trimmed() : "";

    // Remove old cards
    while (inst_cards_layout_->count() > 1) {
        auto* item = inst_cards_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    const auto servers = McpManager::instance().get_servers();
    int shown = 0;

    for (const auto& s : servers) {
        if (!search.isEmpty()
            && !s.name.contains(search, Qt::CaseInsensitive)
            && !s.description.contains(search, Qt::CaseInsensitive)
            && !s.category.contains(search, Qt::CaseInsensitive))
            continue;

        auto* card = build_server_card(s);
        inst_cards_layout_->insertWidget(inst_cards_layout_->count() - 1, card);
        ++shown;
    }

    if (shown == 0) {
        auto* empty = new QLabel(
            servers.empty()
                ? "No servers installed yet.\nUse MARKETPLACE to add one, or click ADD CUSTOM MCP SERVER below."
                : "No servers match the search.");
        empty->setObjectName("srvCardDesc");
        empty->setAlignment(Qt::AlignCenter);
        empty->setWordWrap(true);
        inst_cards_layout_->insertWidget(0, empty);
    }
}

void McpServersScreen::refresh_tools() {
    disconnect(tools_table_, &QTableWidget::cellChanged,
               this, &McpServersScreen::on_tool_enabled_changed);

    tools_table_->setSortingEnabled(false);
    tools_table_->setRowCount(0);

    const auto internal_tools = McpProvider::instance().list_all_tools();
    const auto external_tools = McpManager::instance().get_all_external_tools();
    const int total = static_cast<int>(internal_tools.size() + external_tools.size());
    tools_table_->setRowCount(total);

    int row = 0;
    for (const auto& t : internal_tools) {
        const bool en = McpProvider::instance().is_tool_enabled(t.name);
        auto* chk = new QTableWidgetItem;
        chk->setCheckState(en ? Qt::Checked : Qt::Unchecked);
        chk->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        tools_table_->setItem(row, 0, chk);
        tools_table_->setItem(row, 1, new QTableWidgetItem(t.name));
        auto* si = new QTableWidgetItem("Fincept Terminal");
        si->setData(Qt::UserRole, QString(INTERNAL_SERVER_ID));
        si->setForeground(QColor(colors::AMBER()));
        tools_table_->setItem(row, 2, si);
        tools_table_->setItem(row, 3, new QTableWidgetItem("internal"));
        tools_table_->setItem(row, 4, new QTableWidgetItem(t.description));
        tools_table_->setRowHeight(row, 26);
        ++row;
    }
    for (const auto& t : external_tools) {
        auto* chk = new QTableWidgetItem;
        chk->setCheckState(Qt::Checked);
        chk->setFlags(Qt::ItemIsEnabled);
        tools_table_->setItem(row, 0, chk);
        tools_table_->setItem(row, 1, new QTableWidgetItem(t.name));
        auto* si = new QTableWidgetItem(t.server_name);
        si->setData(Qt::UserRole, t.server_id);
        si->setForeground(QColor(colors::CYAN()));
        tools_table_->setItem(row, 2, si);
        tools_table_->setItem(row, 3, new QTableWidgetItem("external"));
        tools_table_->setItem(row, 4, new QTableWidgetItem(t.description));
        tools_table_->setRowHeight(row, 26);
        ++row;
    }

    tools_table_->resizeColumnToContents(1);
    tools_table_->resizeColumnToContents(2);
    tools_table_->resizeColumnToContents(3);
    tools_table_->setSortingEnabled(true);
    tools_count_->setText(QString("%1 tools  (%2 internal · %3 external)")
        .arg(total).arg(internal_tools.size()).arg(external_tools.size()));

    connect(tools_table_, &QTableWidget::cellChanged,
            this, &McpServersScreen::on_tool_enabled_changed);
}

void McpServersScreen::update_status_bar() {
    const auto servers = McpManager::instance().get_servers();
    int running = 0;
    for (const auto& s : servers)
        if (s.status == ServerStatus::Running) ++running;
    status_count_->setText(QString::number(servers.size()) + " servers");
    status_running_->setText(QString::number(running) + " running");
}

} // namespace fincept::screens
