// McpServersSection.cpp — MCP external server management panel (Qt port)

#include "screens/settings/McpServersSection.h"

#include "core/logging/Logger.h"
#include "mcp/McpManager.h"
#include "mcp/McpService.h"
#include "storage/repositories/McpServerRepository.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens {

using mcp::McpManager;
using mcp::McpServerConfig;
using mcp::McpService;
using mcp::ServerStatus;

static constexpr const char* TAG = "McpServersSection";

// ============================================================================
// Construction
// ============================================================================

McpServersSection::McpServersSection(QWidget* parent) : QWidget(parent) {
    build_ui();
    load_servers();
}

void McpServersSection::reload() {
    load_servers();
    if (tab_stack_->currentIndex() == 1)
        load_tools();
}

void McpServersSection::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Title bar
    auto* title_bar = new QWidget;
    title_bar->setFixedHeight(44);
    title_bar->setStyleSheet("background:#0d0d0d;border-bottom:1px solid #1e1e1e;");
    auto* tbl = new QHBoxLayout(title_bar);
    tbl->setContentsMargins(16, 0, 16, 0);
    auto* title = new QLabel("MCP SERVERS");
    title->setStyleSheet("color:#ff6600;font-size:13px;font-weight:700;letter-spacing:1px;");
    tbl->addWidget(title);
    tbl->addStretch();

    // Internal tools count badge
    auto* tools_badge = new QLabel;
    tools_badge->setObjectName("McpToolsBadge");
    tools_badge->setStyleSheet("color:#888;font-size:11px;");
    tbl->addWidget(tools_badge);

    root->addWidget(title_bar);

    // Tab buttons
    auto* tab_bar = new QWidget;
    tab_bar->setFixedHeight(36);
    tab_bar->setStyleSheet("background:#0a0a0a;border-bottom:1px solid #1e1e1e;");
    auto* tbbl = new QHBoxLayout(tab_bar);
    tbbl->setContentsMargins(8, 0, 8, 0);
    tbbl->setSpacing(4);

    auto make_tab = [&](const QString& text, int idx) {
        auto* btn = new QPushButton(text);
        btn->setCheckable(true);
        btn->setFixedHeight(28);
        btn->setStyleSheet("QPushButton{background:transparent;color:#888;border:none;"
                           "border-bottom:2px solid transparent;font-size:12px;padding:0 12px;}"
                           "QPushButton:checked{color:#ff6600;border-bottom:2px solid #ff6600;}"
                           "QPushButton:hover:!checked{color:#ccc;}");
        connect(btn, &QPushButton::clicked, this, [this, idx]() { on_tab_changed(idx); });
        tbbl->addWidget(btn);
        if (idx == 0)
            btn->setChecked(true);
        return btn;
    };
    make_tab("Installed Servers", 0);
    make_tab("Tools", 1);
    tbbl->addStretch();
    root->addWidget(tab_bar);

    tab_stack_ = new QStackedWidget;
    tab_stack_->addWidget(build_servers_tab());
    tab_stack_->addWidget(build_tools_tab());
    root->addWidget(tab_stack_, 1);

    // Update badge
    std::size_t n = McpService::instance().tool_count();
    if (auto* badge = findChild<QLabel*>("McpToolsBadge"))
        badge->setText(QString("%1 internal tools active").arg(n));
}

// ============================================================================
// Servers tab
// ============================================================================

QWidget* McpServersSection::build_servers_tab() {
    auto* page = new QWidget;
    page->setStyleSheet("background:#0a0a0a;");
    auto* hl = new QHBoxLayout(page);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    // Left: server list
    auto* left = new QWidget;
    left->setFixedWidth(240);
    left->setStyleSheet("background:#0a0a0a;border-right:1px solid #1e1e1e;");
    auto* lvl = new QVBoxLayout(left);
    lvl->setContentsMargins(8, 8, 8, 8);
    lvl->setSpacing(6);

    auto* list_lbl = new QLabel("External Servers");
    list_lbl->setStyleSheet("color:#888;font-size:11px;font-weight:700;letter-spacing:1px;");
    lvl->addWidget(list_lbl);

    server_list_ = new QListWidget;
    server_list_->setStyleSheet("QListWidget{background:transparent;border:none;color:#ccc;font-size:12px;}"
                                "QListWidget::item{padding:10px 8px;border-radius:3px;border-bottom:1px solid #111;}"
                                "QListWidget::item:selected{background:#1a1a1a;color:#ff6600;}"
                                "QListWidget::item:hover{background:#111;}");
    connect(server_list_, &QListWidget::currentRowChanged, this, &McpServersSection::on_server_selected);
    lvl->addWidget(server_list_, 1);

    // Action buttons
    auto* btns = new QHBoxLayout;
    add_btn_ = new QPushButton("+ Add");
    add_btn_->setStyleSheet("QPushButton{background:#1a1a1a;color:#ff6600;border:1px solid #ff6600;"
                            "border-radius:3px;padding:5px 10px;font-size:11px;font-weight:600;}"
                            "QPushButton:hover{background:#2a1a0a;}");
    connect(add_btn_, &QPushButton::clicked, this, &McpServersSection::on_add_server);

    remove_btn_ = new QPushButton("Remove");
    remove_btn_->setEnabled(false);
    remove_btn_->setStyleSheet("QPushButton{background:#1a0a0a;color:#cc3300;border:1px solid #330000;"
                               "border-radius:3px;padding:5px 10px;font-size:11px;}"
                               "QPushButton:hover{background:#2a1010;}"
                               "QPushButton:disabled{color:#333;border-color:#222;}");
    connect(remove_btn_, &QPushButton::clicked, this, &McpServersSection::on_remove_server);

    btns->addWidget(add_btn_);
    btns->addWidget(remove_btn_);
    lvl->addLayout(btns);

    hl->addWidget(left);

    // Right: detail panel
    auto* right = new QScrollArea;
    right->setWidgetResizable(true);
    right->setFrameShape(QFrame::NoFrame);
    right->setStyleSheet("QScrollArea{background:#0a0a0a;border:none;}"
                         "QScrollBar:vertical{background:#111;width:6px;}"
                         "QScrollBar::handle:vertical{background:#333;border-radius:3px;}"
                         "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    auto* detail_page = new QWidget;
    detail_page->setStyleSheet("background:#0a0a0a;");
    auto* dvl = new QVBoxLayout(detail_page);
    dvl->setContentsMargins(24, 20, 24, 20);
    dvl->setSpacing(12);

    detail_lbl_ = new QLabel("Select a server to view details.");
    detail_lbl_->setWordWrap(true);
    detail_lbl_->setStyleSheet("color:#666;font-size:12px;");
    dvl->addWidget(detail_lbl_);

    // Start/Stop buttons
    auto* server_btns = new QHBoxLayout;
    start_btn_ = new QPushButton("▶  Start");
    start_btn_->setEnabled(false);
    start_btn_->setFixedHeight(32);
    start_btn_->setStyleSheet("QPushButton{background:#0a2a0a;color:#44cc44;border:1px solid #226622;"
                              "border-radius:4px;font-size:12px;font-weight:600;padding:0 16px;}"
                              "QPushButton:hover{background:#0f3a0f;}"
                              "QPushButton:disabled{color:#333;border-color:#222;background:#0a0a0a;}");
    connect(start_btn_, &QPushButton::clicked, this, &McpServersSection::on_start_server);

    stop_btn_ = new QPushButton("■  Stop");
    stop_btn_->setEnabled(false);
    stop_btn_->setFixedHeight(32);
    stop_btn_->setStyleSheet("QPushButton{background:#2a0a0a;color:#cc4444;border:1px solid #662222;"
                             "border-radius:4px;font-size:12px;font-weight:600;padding:0 16px;}"
                             "QPushButton:hover{background:#3a0f0f;}"
                             "QPushButton:disabled{color:#333;border-color:#222;background:#0a0a0a;}");
    connect(stop_btn_, &QPushButton::clicked, this, &McpServersSection::on_stop_server);

    server_btns->addWidget(start_btn_);
    server_btns->addWidget(stop_btn_);
    server_btns->addStretch();
    dvl->addLayout(server_btns);

    status_lbl_ = new QLabel;
    status_lbl_->setStyleSheet("font-size:11px;");
    status_lbl_->hide();
    dvl->addWidget(status_lbl_);

    dvl->addStretch();
    right->setWidget(detail_page);
    hl->addWidget(right, 1);

    return page;
}

// ============================================================================
// Tools tab
// ============================================================================

QWidget* McpServersSection::build_tools_tab() {
    auto* page = new QWidget;
    page->setStyleSheet("background:#0a0a0a;");
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(16, 12, 16, 12);
    vl->setSpacing(8);

    auto* info =
        new QLabel("All registered MCP tools — both internal (built-in) and external (from connected servers).");
    info->setStyleSheet("color:#888;font-size:11px;");
    info->setWordWrap(true);
    vl->addWidget(info);

    tools_table_ = new QTableWidget;
    tools_table_->setColumnCount(4);
    tools_table_->setHorizontalHeaderLabels({"Tool Name", "Server", "Category", "Description"});
    tools_table_->horizontalHeader()->setStretchLastSection(true);
    tools_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tools_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tools_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tools_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tools_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tools_table_->setAlternatingRowColors(true);
    tools_table_->verticalHeader()->hide();
    tools_table_->setStyleSheet("QTableWidget{background:#0a0a0a;color:#ccc;border:1px solid #1e1e1e;"
                                "gridline-color:#111;font-size:12px;}"
                                "QTableWidget::item{padding:6px;}"
                                "QTableWidget::item:selected{background:#1a1a1a;color:#ff6600;}"
                                "QHeaderView::section{background:#0d0d0d;color:#888;border:none;"
                                "border-bottom:1px solid #1e1e1e;padding:6px;font-size:11px;font-weight:600;}"
                                "QTableWidget{alternate-background-color:#080808;}");
    vl->addWidget(tools_table_, 1);

    return page;
}

// ============================================================================
// Add Server Dialog
// ============================================================================

void McpServersSection::on_add_server() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Add MCP Server");
    dlg->setMinimumWidth(480);
    dlg->setStyleSheet("background:#0d0d0d;color:#e0e0e0;");

    auto* vl = new QVBoxLayout(dlg);
    vl->setSpacing(12);
    vl->setContentsMargins(16, 16, 16, 16);

    auto* title = new QLabel("Add External MCP Server");
    title->setStyleSheet("color:#ff6600;font-size:13px;font-weight:700;");
    vl->addWidget(title);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    auto fe = [](const QString& ph) {
        auto* e = new QLineEdit;
        e->setPlaceholderText(ph);
        e->setStyleSheet("QLineEdit{background:#111;color:#e0e0e0;border:1px solid #2a2a2a;"
                         "border-radius:3px;padding:6px;font-size:12px;}"
                         "QLineEdit:focus{border:1px solid #ff6600;}");
        return e;
    };
    auto fl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet("color:#888;font-size:12px;");
        return l;
    };

    auto* name_edit = fe("e.g. My Database Server");
    auto* cmd_edit = fe("e.g. npx, uvx, python");
    auto* args_edit = fe("e.g. -y @modelcontextprotocol/server-postgres");
    auto* env_edit = fe("KEY=VALUE KEY2=VALUE2 (space-separated)");
    auto* cat_edit = fe("e.g. data, tools, analytics");

    form->addRow(fl("Name"), name_edit);
    form->addRow(fl("Command"), cmd_edit);
    form->addRow(fl("Arguments"), args_edit);
    form->addRow(fl("Environment"), env_edit);
    form->addRow(fl("Category"), cat_edit);

    vl->addLayout(form);

    // Auto-start checkbox
    auto* auto_start_row = new QHBoxLayout;
    auto* as_lbl = new QLabel("Auto-start on launch");
    as_lbl->setStyleSheet("color:#888;font-size:12px;");
    auto* as_cb = new QCheckBox;
    as_cb->setChecked(false);
    auto_start_row->addWidget(as_lbl);
    auto_start_row->addWidget(as_cb);
    auto_start_row->addStretch();
    vl->addLayout(auto_start_row);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btns->setStyleSheet("QPushButton{background:#1a1a1a;color:#e0e0e0;border:1px solid #333;"
                        "border-radius:3px;padding:6px 16px;font-size:12px;}"
                        "QPushButton:hover{background:#222;}"
                        "QPushButton[text='OK']{background:#ff6600;color:#000;border:none;}"
                        "QPushButton[text='OK']:hover{background:#ff8800;}");
    connect(btns, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    vl->addWidget(btns);

    if (dlg->exec() != QDialog::Accepted) {
        dlg->deleteLater();
        return;
    }

    QString name = name_edit->text().trimmed();
    QString cmd = cmd_edit->text().trimmed();
    if (name.isEmpty() || cmd.isEmpty()) {
        show_status("Name and Command are required", true);
        dlg->deleteLater();
        return;
    }

    McpServerConfig cfg;
    cfg.name = name;
    cfg.command = cmd;
    cfg.args = args_edit->text().trimmed().split(' ', Qt::SkipEmptyParts);
    cfg.category = cat_edit->text().trimmed();
    cfg.auto_start = as_cb->isChecked();
    cfg.enabled = true;

    // Parse env pairs
    for (const auto& pair : env_edit->text().trimmed().split(' ', Qt::SkipEmptyParts)) {
        int eq = pair.indexOf('=');
        if (eq > 0)
            cfg.env[pair.left(eq)] = pair.mid(eq + 1);
    }

    auto r = McpManager::instance().save_server(cfg);
    if (r.is_err()) {
        show_status("Failed to save server: " + QString::fromStdString(r.error()), true);
    } else {
        show_status("Server added: " + name, false);
        load_servers();
        LOG_INFO(TAG, "Added MCP server: " + name);
    }

    dlg->deleteLater();
}

// ============================================================================
// Data Loading
// ============================================================================

void McpServersSection::load_servers() {
    server_list_->blockSignals(true);
    server_list_->clear();

    auto servers = McpManager::instance().get_servers();
    for (const auto& cfg : servers) {
        QString status_icon;
        if (cfg.status == ServerStatus::Running)
            status_icon = "● ";
        else if (cfg.status == ServerStatus::Error)
            status_icon = "✕ ";
        else
            status_icon = "○ ";

        auto* item = new QListWidgetItem(status_icon + cfg.name);
        item->setData(Qt::UserRole, cfg.id);

        if (cfg.status == ServerStatus::Running)
            item->setForeground(QColor("#44cc44"));
        else if (cfg.status == ServerStatus::Error)
            item->setForeground(QColor("#cc4444"));
        else
            item->setForeground(QColor("#888"));

        item->setToolTip(cfg.command + " " + cfg.args.join(' '));
        server_list_->addItem(item);
    }

    server_list_->blockSignals(false);
    remove_btn_->setEnabled(false);
    start_btn_->setEnabled(false);
    stop_btn_->setEnabled(false);

    if (server_list_->count() > 0)
        server_list_->setCurrentRow(0);
    else
        detail_lbl_->setText("No external servers configured.\nClick '+ Add' to add one.");
}

void McpServersSection::load_tools() {
    auto tools = McpService::instance().get_all_tools();
    tools_table_->setRowCount(0);
    tools_table_->setRowCount(static_cast<int>(tools.size()));

    int row = 0;
    for (const auto& t : tools) {
        auto* name_item = new QTableWidgetItem(t.name);
        auto* server_item = new QTableWidgetItem(t.server_name);
        auto* cat_item = new QTableWidgetItem(t.is_internal ? "internal" : "external");
        auto* desc_item = new QTableWidgetItem(t.description);

        if (t.is_internal) {
            server_item->setForeground(QColor("#ff6600"));
            cat_item->setForeground(QColor("#888"));
        } else {
            server_item->setForeground(QColor("#44cc44"));
            cat_item->setForeground(QColor("#4488cc"));
        }

        tools_table_->setItem(row, 0, name_item);
        tools_table_->setItem(row, 1, server_item);
        tools_table_->setItem(row, 2, cat_item);
        tools_table_->setItem(row, 3, desc_item);
        ++row;
    }
}

void McpServersSection::refresh_server_detail(const QString& server_id) {
    auto servers = McpManager::instance().get_servers();
    for (const auto& cfg : servers) {
        if (cfg.id != server_id)
            continue;

        QString status_str;
        if (cfg.status == ServerStatus::Running)
            status_str = "Running";
        else if (cfg.status == ServerStatus::Error)
            status_str = "Error";
        else
            status_str = "Stopped";

        detail_lbl_->setText(QString("<b style='color:#e0e0e0'>%1</b><br>"
                                     "<span style='color:#888;font-size:11px;'>Status: %2</span><br>"
                                     "<span style='color:#666;font-size:11px;'>Command: %3 %4</span><br>"
                                     "<span style='color:#666;font-size:11px;'>Category: %5</span><br>"
                                     "<span style='color:#666;font-size:11px;'>Auto-start: %6</span>")
                                 .arg(cfg.name, status_str, cfg.command, cfg.args.join(' '),
                                      cfg.category.isEmpty() ? "-" : cfg.category, cfg.auto_start ? "Yes" : "No"));

        bool running = (cfg.status == ServerStatus::Running);
        start_btn_->setEnabled(!running);
        stop_btn_->setEnabled(running);
        return;
    }
}

// ============================================================================
// Slots
// ============================================================================

void McpServersSection::on_server_selected(int row) {
    if (row < 0 || row >= server_list_->count()) {
        remove_btn_->setEnabled(false);
        start_btn_->setEnabled(false);
        stop_btn_->setEnabled(false);
        return;
    }

    selected_server_id_ = server_list_->item(row)->data(Qt::UserRole).toString();
    remove_btn_->setEnabled(true);
    refresh_server_detail(selected_server_id_);
}

void McpServersSection::on_remove_server() {
    if (selected_server_id_.isEmpty())
        return;

    auto reply = QMessageBox::question(this, "Remove Server", "Remove this MCP server configuration?",
                                       QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    McpManager::instance().remove_server(selected_server_id_);
    selected_server_id_.clear();
    load_servers();
}

void McpServersSection::on_start_server() {
    if (selected_server_id_.isEmpty())
        return;
    show_status("Starting server...", false);
    start_btn_->setEnabled(false);

    QPointer<McpServersSection> self = this;
    QString id = selected_server_id_;
    QtConcurrent::run([self, id]() {
        auto r = McpManager::instance().start_server(id);
        QMetaObject::invokeMethod(
            qApp,
            [self, r, id]() {
                if (!self)
                    return;
                if (r.is_err())
                    self->show_status("Start failed: " + QString::fromStdString(r.error()), true);
                else
                    self->show_status("Server started", false);
                self->load_servers();
            },
            Qt::QueuedConnection);
    });
}

void McpServersSection::on_stop_server() {
    if (selected_server_id_.isEmpty())
        return;
    McpManager::instance().stop_server(selected_server_id_);
    show_status("Server stopped", false);
    load_servers();
}

void McpServersSection::on_tab_changed(int idx) {
    tab_stack_->setCurrentIndex(idx);
    if (idx == 1)
        load_tools();

    // Update tab button states
    QList<QPushButton*> btns = findChildren<QPushButton*>();
    // The tab bar buttons are the first two checkable buttons
    int checked_count = 0;
    for (auto* btn : btns) {
        if (btn->isCheckable()) {
            btn->setChecked(checked_count == idx);
            ++checked_count;
            if (checked_count > 1)
                break;
        }
    }
}

void McpServersSection::show_status(const QString& msg, bool error) {
    status_lbl_->setText(msg);
    status_lbl_->setStyleSheet(error ? "color:#ff4444;font-size:11px;" : "color:#44cc44;font-size:11px;");
    status_lbl_->show();
    QTimer::singleShot(4000, status_lbl_, &QLabel::hide);
}

} // namespace fincept::screens
