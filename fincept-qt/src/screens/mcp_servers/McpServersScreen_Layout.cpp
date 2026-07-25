// src/screens/mcp_servers/McpServersScreen_Layout.cpp
// UI construction: create_* views, populate_marketplace, build_server_card.
// Part of the partial-class split of McpServersScreen.cpp.

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "mcp/McpManager.h"
#include "mcp/McpMarketplace.h"
#include "mcp/McpProvider.h"
#include "screens/mcp_servers/McpServersScreen.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace {
using namespace fincept::ui;

static const QList<fincept::mcp::MarketplaceEntry>& g_catalog = fincept::mcp::marketplace_catalog();

// NOTE: the screen stylesheet (kStyle) lives in McpServersScreen.cpp and is
// applied once to the whole screen. This translation unit used to carry a
// byte-identical [[maybe_unused]] copy (~120 lines) that nothing referenced.

} // namespace

namespace fincept::screens {

using namespace fincept::mcp;
using namespace fincept::ui;

QWidget* McpServersScreen::create_header() {
    auto* bar = new QWidget(this);
    bar->setObjectName("mcpHeader");
    bar->setFixedHeight(44);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(8);

    // Title (text set in retranslateUi)
    header_title_ = new QLabel;
    header_title_->setObjectName("mcpHeaderTitle");
    hl->addWidget(header_title_);

    hl->addSpacing(16);

    // View-tab buttons (text set in retranslateUi). Order of construction
    // matches enum: Marketplace (0), Installed (1), Tools (2).
    for (int i = 0; i < 3; ++i) {
        auto* btn = new QPushButton;
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
    search_input_->setClearButtonEnabled(true);
    search_input_->setAccessibleName(tr("Search servers and tools"));
    connect(search_input_, &QLineEdit::textChanged, this, &McpServersScreen::on_search_changed);
    hl->addWidget(search_input_);

    hl->addSpacing(6);

    refresh_btn_ = new QPushButton;
    refresh_btn_->setObjectName("mcpRefreshBtn");
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    // No F5 shortcut — KeyConfigManager owns F5 globally (Qt::ApplicationShortcut).
    refresh_btn_->setAccessibleName(tr("Refresh"));
    connect(refresh_btn_, &QPushButton::clicked, this, &McpServersScreen::on_refresh);
    hl->addWidget(refresh_btn_);

    return bar;
}

// ── Marketplace view ──────────────────────────────────────────────────────────
// Layout: narrow category filter on left | scrollable card grid on right

QWidget* McpServersScreen::create_marketplace_view() {
    auto* root = new QWidget(this);
    auto* hl = new QHBoxLayout(root);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    // Category sidebar
    auto* sidebar = new QWidget(this);
    sidebar->setFixedWidth(130);
    auto* svl = new QVBoxLayout(sidebar);
    svl->setContentsMargins(0, 0, 0, 0);
    svl->setSpacing(0);

    // Category sidebar header (text set in retranslateUi)
    cat_header_lbl_ = new QLabel;
    cat_header_lbl_->setObjectName("mcpStatusText");
    cat_header_lbl_->setFixedHeight(32);
    cat_header_lbl_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    svl->addWidget(cat_header_lbl_);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color:%1;").arg(colors::BORDER_DIM()));
    svl->addWidget(sep);

    // Category list — display text set in retranslateUi(); the lowercase API
    // key lives in Qt::UserRole so populate_marketplace() can read it without
    // depending on the visible (localized) text.
    mkt_cat_list_ = new QListWidget;
    mkt_cat_list_->setObjectName("catList");
    const QStringList cat_keys = {"all", "utilities", "developer", "database"};
    for (const auto& key : cat_keys) {
        auto* item = new QListWidgetItem(mkt_cat_list_);
        item->setData(Qt::UserRole, key);
    }
    mkt_cat_list_->setCurrentRow(0);
    connect(mkt_cat_list_, &QListWidget::currentRowChanged, this, [this](int) { populate_marketplace(); });
    svl->addWidget(mkt_cat_list_, 1);
    hl->addWidget(sidebar);

    // Vertical separator
    auto* vsep = new QFrame;
    vsep->setFrameShape(QFrame::VLine);
    vsep->setStyleSheet(QString("color:%1;").arg(colors::BORDER_DIM()));
    hl->addWidget(vsep);

    // Card scroll area
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);

    mkt_cards_widget_ = new QWidget(this);
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
    auto* root = new QWidget(this);
    auto* vl = new QVBoxLayout(root);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Scrollable card area
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);

    inst_cards_widget_ = new QWidget(this);
    inst_cards_layout_ = new QVBoxLayout(inst_cards_widget_);
    inst_cards_layout_->setContentsMargins(16, 12, 16, 8);
    inst_cards_layout_->setSpacing(6);
    inst_cards_layout_->addStretch(1);

    scroll->setWidget(inst_cards_widget_);
    vl->addWidget(scroll, 1);

    // Sticky "Add server" button at the bottom (text set in retranslateUi)
    add_server_btn_ = new QPushButton;
    add_server_btn_->setObjectName("addSrvBtn");
    add_server_btn_->setCursor(Qt::PointingHandCursor);
    add_server_btn_->setFixedHeight(38);
    add_server_btn_->setAccessibleName(tr("Add a custom MCP server"));
    connect(add_server_btn_, &QPushButton::clicked, this, &McpServersScreen::on_add_server);
    vl->addWidget(add_server_btn_);

    return root;
}

// ── Tools view ────────────────────────────────────────────────────────────────

QWidget* McpServersScreen::create_tools_view() {
    auto* panel = new QWidget(this);
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* toolbar = new QWidget(this);
    toolbar->setFixedHeight(32);
    auto* tbl = new QHBoxLayout(toolbar);
    tbl->setContentsMargins(12, 0, 12, 0);
    // Toolbar label (text set in retranslateUi)
    tools_toolbar_lbl_ = new QLabel;
    tools_toolbar_lbl_->setObjectName("mcpStatusText");
    tbl->addWidget(tools_toolbar_lbl_);
    tbl->addStretch(1);
    tools_count_ = new QLabel;
    tools_count_->setObjectName("mcpStatusText");
    tbl->addWidget(tools_count_);
    vl->addWidget(toolbar);

    // Tools table (headers set in retranslateUi)
    tools_table_ = new QTableWidget;
    tools_table_->setColumnCount(5);
    tools_table_->horizontalHeader()->setStretchLastSection(true);
    tools_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    tools_table_->setColumnWidth(0, 42);
    tools_table_->verticalHeader()->setVisible(false);
    tools_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tools_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(tools_table_, &QTableWidget::cellChanged, this, &McpServersScreen::on_tool_enabled_changed);
    vl->addWidget(tools_table_, 1);

    return panel;
}

QWidget* McpServersScreen::create_status_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("mcpStatusBar");
    bar->setFixedHeight(26);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    // Screen label + active-view label (text set in retranslateUi)
    status_screen_lbl_ = new QLabel;
    status_screen_lbl_->setObjectName("mcpStatusText");
    hl->addWidget(status_screen_lbl_);
    hl->addStretch(1);
    status_view_ = new QLabel;
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

void McpServersScreen::populate_marketplace() {
    // Determine active category filter (read API key from UserRole — the
    // visible text is localized and would break the catalog lookup).
    const QString filter_raw = mkt_cat_list_ ? mkt_cat_list_->currentItem()
                                                   ? mkt_cat_list_->currentItem()->data(Qt::UserRole).toString()
                                                   : QStringLiteral("all")
                                             : QStringLiteral("all");
    const QString filter = (filter_raw == "all") ? "" : filter_raw;

    const QString search = search_input_ ? search_input_->text().trimmed() : "";

    // Build installed-id set for badge display
    QSet<QString> installed_ids;
    for (const auto& s : McpManager::instance().get_servers())
        installed_ids.insert(s.id);

    // Clear old content (keep stretch at end)
    while (mkt_cards_layout_->count() > 1) {
        auto* item = mkt_cards_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    // Collect matching entries
    QList<int> matched;
    for (int i = 0; i < g_catalog.size(); ++i) {
        const auto& e = g_catalog[i];
        if (!filter.isEmpty() && e.category != filter)
            continue;
        if (!search.isEmpty() && !e.name.contains(search, Qt::CaseInsensitive) &&
            !e.description.contains(search, Qt::CaseInsensitive) && !e.category.contains(search, Qt::CaseInsensitive))
            continue;
        matched.append(i);
    }

    if (matched.isEmpty()) {
        auto* empty = new QLabel(tr("No servers match the current filter."));
        empty->setObjectName("mktCardDesc");
        empty->setAlignment(Qt::AlignCenter);
        mkt_cards_layout_->insertWidget(0, empty);
        return;
    }

    // Build rows of 3 cards each
    constexpr int COLS = 3;
    for (int row = 0; row < (matched.size() + COLS - 1) / COLS; ++row) {
        auto* row_widget = new QWidget(this);
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
            auto* card = new QWidget(this);
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

            // Category badge — translate the known API keys, fall back to
            // toUpper() for unknown values (forward-compatible if the
            // marketplace adds new categories).
            QString cat_text;
            if (e.category == "utilities")
                cat_text = tr("UTILITIES");
            else if (e.category == "developer")
                cat_text = tr("DEVELOPER");
            else if (e.category == "database")
                cat_text = tr("DATABASE");
            else
                cat_text = e.category.toUpper();
            auto* cat_badge = new QLabel(cat_text);
            cat_badge->setObjectName("mktCatBadge");
            bottom->addWidget(cat_badge);

            if (!e.env_keys.isEmpty()) {
                auto* env_lbl = new QLabel(tr("Needs: %1").arg(e.env_keys.join(", ")));
                env_lbl->setObjectName("mktCardEnv");
                bottom->addWidget(env_lbl);
            }

            bottom->addStretch(1);

            if (installed) {
                auto* badge = new QLabel(tr("✓ ADDED"));
                badge->setObjectName("mktInstalledBadge");
                bottom->addWidget(badge);
            } else {
                auto* add_btn = new QPushButton(tr("ADD"));
                add_btn->setObjectName("mktAddBtn");
                add_btn->setCursor(Qt::PointingHandCursor);
                add_btn->setFixedWidth(56);
                add_btn->setAccessibleName(tr("Add %1").arg(e.name));
                connect(add_btn, &QPushButton::clicked, this, [this, i]() { on_install_server(i); });
                bottom->addWidget(add_btn);
            }

            vl->addLayout(bottom);
            row_hl->addWidget(card, 1);
        }

        mkt_cards_layout_->insertWidget(mkt_cards_layout_->count() - 1, row_widget);
    }
}

// ── Launch consent ────────────────────────────────────────────────────────────
// Enabling a server spawns an arbitrary local process that the assistant can
// then invoke as a tool. Show the exact command line (and the names — never the
// values — of the environment variables it will receive) before the first
// launch of each server in a session.

bool McpServersScreen::confirm_server_launch(const McpServerConfig& s) {
    if (launch_approved_.contains(s.id))
        return true;

    QString detail = s.command;
    if (!s.args.isEmpty())
        detail += QLatin1Char(' ') + s.args.join(QLatin1Char(' '));

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Start MCP Server"));
    box.setText(tr("Start \"%1\"?").arg(s.name));
    QString info = tr("This runs a program on your machine and exposes its tools to the assistant, "
                      "which can then call them without asking again.\n\nCommand:\n%1")
                       .arg(detail);
    if (!s.env.isEmpty()) {
        // Names only — never the values, which are usually API keys.
        QStringList keys(s.env.keys());
        keys.sort();
        info += tr("\n\nEnvironment passed to it: %1").arg(keys.join(QStringLiteral(", ")));
    }
    box.setInformativeText(info);
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Cancel);
    if (box.exec() != QMessageBox::Yes)
        return false;

    launch_approved_.insert(s.id);
    return true;
}

// ── build_server_card ─────────────────────────────────────────────────────────
// Enable toggle IS start/stop — no manual start/stop buttons.
// Any part of the app can call McpManager::call_external_tool() on enabled servers.

QWidget* McpServersScreen::build_server_card(const McpServerConfig& s) {
    const bool starting = (s.status == ServerStatus::Starting);
    const bool running = (s.status == ServerStatus::Running);
    const bool error = (s.status == ServerStatus::Error);

    auto* card = new QWidget(this);
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

    auto* status_pill = new QLabel(running ? tr("● RUNNING")
                                           : (starting ? tr("⟳ STARTING") : (error ? tr("● ERROR") : tr("○ STOPPED"))));
    status_pill->setObjectName(running ? "pillRunning"
                                       : (starting ? "pillRunning" : (error ? "pillError" : "pillStopped")));
    top->addWidget(status_pill);

    top->addStretch(1);

    // Enable toggle — toggling ON starts the server off-thread, OFF stops it
    const QString sid = s.id;
    QPointer<McpServersScreen> self = this;

    auto* toggle_btn = new QPushButton(running ? tr("● ENABLED") : (starting ? tr("⟳ STARTING...") : tr("○ DISABLED")));
    toggle_btn->setObjectName(running ? "cardToggleOn" : "cardToggleOff");
    toggle_btn->setCursor(Qt::PointingHandCursor);
    toggle_btn->setFixedWidth(110);
    toggle_btn->setAccessibleName(running ? tr("Stop %1").arg(s.name) : tr("Start %1").arg(s.name));
    if (starting)
        toggle_btn->setEnabled(false);
    const McpServerConfig cfg_copy = s; // for the launch-consent prompt
    connect(toggle_btn, &QPushButton::clicked, this, [sid, self, running, toggle_btn, cfg_copy]() {
        if (!self)
            return;
        if (running) {
            // Disable — stop is fast, safe on main thread
            McpManager::instance().stop_server(sid);
            self->refresh_installed();
            self->update_status_bar();
        } else {
            if (!self->confirm_server_launch(cfg_copy))
                return;
            // Enable — start on background thread to avoid freezing the UI.
            // start_server() blocks for process launch + handshake (can take 60s+).
            toggle_btn->setText(tr("⟳ STARTING..."));
            toggle_btn->setEnabled(false);
            (void)QtConcurrent::run([sid, self]() {
                const auto r = McpManager::instance().start_server(sid);
                if (!self)
                    return;
                QMetaObject::invokeMethod(
                    self,
                    [self, r]() {
                        if (!self)
                            return;
                        if (r.is_err()) {
                            const QString msg = QString::fromStdString(r.error());
                            LOG_ERROR("McpServers", "Start failed: " + msg);
                            QMessageBox::warning(self, tr("Server Failed to Start"), msg);
                        }
                        self->refresh_installed();
                        self->update_status_bar();
                    },
                    Qt::QueuedConnection);
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
    // Category — translate the known API keys, fall back to toUpper() for
    // user-added custom categories.
    QString cat_text;
    if (s.category == "utilities")
        cat_text = tr("UTILITIES");
    else if (s.category == "developer")
        cat_text = tr("DEVELOPER");
    else if (s.category == "database")
        cat_text = tr("DATABASE");
    else
        cat_text = s.category.toUpper();
    auto* cat_lbl = new QLabel(cat_text);
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

    auto* logs_btn = new QPushButton(tr("LOGS"));
    logs_btn->setObjectName("cardLogsBtn");
    logs_btn->setCursor(Qt::PointingHandCursor);
    logs_btn->setAccessibleName(tr("Show logs for %1").arg(s.name));
    // Capture the "no output" message at click time via QPointer<this> so we
    // can resolve it through this->tr() — log_view is a plain QTextEdit and
    // doesn't carry McpServersScreen's translation context.
    connect(logs_btn, &QPushButton::clicked, this, [sid, log_view, self]() {
        if (!self)
            return;
        const bool showing = !log_view->isVisible();
        log_view->setVisible(showing);
        if (!showing)
            return;
        const QString empty_msg = self->tr("No output yet. Start the server to see logs here.");
        // Fetch logs off-thread so mutex never stalls UI
        QPointer<QTextEdit> lv = log_view;
        (void)QtConcurrent::run([sid, lv, empty_msg]() {
            const QStringList lines = McpManager::instance().get_logs(sid);
            QMetaObject::invokeMethod(
                lv,
                [lv, lines, empty_msg]() {
                    if (!lv)
                        return;
                    lv->setPlainText(lines.isEmpty() ? empty_msg : lines.join('\n'));
                    QScrollBar* sb = lv->verticalScrollBar();
                    if (sb)
                        sb->setValue(sb->maximum());
                },
                Qt::QueuedConnection);
        });
    });
    btns->addWidget(logs_btn);

    // AUTO-START — previously only settable at Add time; the screen-level
    // on_toggle_autostart() slot existed but was never connected to anything.
    const bool auto_on = s.auto_start;
    auto* auto_btn = new QPushButton(auto_on ? tr("AUTO-START: ON") : tr("AUTO-START: OFF"));
    auto_btn->setObjectName(auto_on ? "cardToggleOn" : "cardToggleOff");
    auto_btn->setCursor(Qt::PointingHandCursor);
    auto_btn->setAccessibleName(tr("Toggle auto-start for %1").arg(s.name));
    auto_btn->setToolTip(tr("When on, this server is launched automatically at terminal startup — without the "
                            "confirmation prompt."));
    connect(auto_btn, &QPushButton::clicked, this, [sid, self, auto_on]() {
        if (!self)
            return;
        if (!auto_on) {
            // Turning it ON means the process starts unattended next launch.
            if (QMessageBox::question(self, self->tr("Auto-start Server"),
                                      self->tr("Launch this server automatically every time the terminal starts?\n\n"
                                               "It will run without asking for confirmation."),
                                      QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
                return;
        }
        auto servers = McpManager::instance().get_servers();
        for (auto& srv : servers) {
            if (srv.id != sid)
                continue;
            srv.auto_start = !srv.auto_start;
            const auto r = McpManager::instance().save_server(srv);
            if (r.is_err())
                LOG_ERROR("McpServers", "Autostart update failed: " + QString::fromStdString(r.error()));
            break;
        }
        self->refresh_installed();
    });
    btns->addWidget(auto_btn);

    btns->addStretch(1);

    // REMOVE
    auto* remove_btn = new QPushButton(tr("REMOVE"));
    remove_btn->setObjectName("cardRemoveBtn");
    remove_btn->setCursor(Qt::PointingHandCursor);
    remove_btn->setAccessibleName(tr("Remove %1").arg(s.name));
    connect(remove_btn, &QPushButton::clicked, this, [sid, self]() {
        if (!self)
            return;
        QString name;
        for (const auto& srv : McpManager::instance().get_servers())
            if (srv.id == sid) {
                name = srv.name;
                break;
            }
        QMessageBox mb(QMessageBox::Question, self->tr("Remove Server"),
                       self->tr("Remove \"%1\"?\nThis cannot be undone.").arg(name),
                       QMessageBox::Yes | QMessageBox::Cancel, self);
        if (mb.exec() != QMessageBox::Yes)
            return;
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

} // namespace fincept::screens
