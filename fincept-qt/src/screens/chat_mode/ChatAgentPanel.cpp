#include "screens/chat_mode/ChatAgentPanel.h"

#include "core/logging/Logger.h"
#include "screens/chat_mode/ChatModeService.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QVBoxLayout>

namespace fincept::chat_mode {

static QString list_ss() {
    return QString("QListWidget{background:%1;border:1px solid %2;color:%3;"
                   "font-size:12px;font-family:'Consolas','Courier New',monospace;"
                   "border-radius:0px;outline:none;}"
                   "QListWidget::item{padding:6px 8px;border-bottom:1px solid %4;}"
                   "QListWidget::item:selected{background:%4;color:%5;}"
                   "QListWidget::item:hover{background:%7;}"
                   "QScrollBar:vertical{background:transparent;width:4px;}"
                   "QScrollBar::handle:vertical{background:%6;border-radius:0px;}"
                   "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
        .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(), ui::colors::BG_RAISED(),
             ui::colors::AMBER(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER());
}

static QString section_title_ss() {
    return QString("color:%1;font-size:11px;font-weight:700;letter-spacing:0.5px;"
                   "font-family:'Consolas','Courier New',monospace;background:transparent;")
        .arg(ui::colors::TEXT_SECONDARY());
}

static QString hint_ss() {
    return QString("color:%1;font-size:11px;"
                   "font-family:'Consolas','Courier New',monospace;background:transparent;")
        .arg(ui::colors::TEXT_DIM());
}

// ── Constructor ───────────────────────────────────────────────────────────────

ChatAgentPanel::ChatAgentPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    refresh_memory();
    refresh_schedules();
    refresh_tasks();
    refresh_mcp_servers();
    refresh_monitors();
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void ChatAgentPanel::build_ui() {
    setFixedWidth(280);
    setStyleSheet(
        QString("background:%1;border-left:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    tab_bar_ = new QTabBar;
    tab_bar_->setExpanding(true);
    tab_bar_->addTab(tr("Mem"));
    tab_bar_->addTab(tr("Sched"));
    tab_bar_->addTab(tr("Tasks"));
    tab_bar_->addTab(tr("MCP"));
    tab_bar_->addTab(tr("Monitor"));
    tab_bar_->setStyleSheet(QString("QTabBar{background:%1;border-bottom:1px solid %2;}"
                                    "QTabBar::tab{background:%1;color:%3;padding:6px 0;"
                                    "font-size:11px;font-family:'Consolas','Courier New',monospace;"
                                    "border:none;border-bottom:2px solid transparent;}"
                                    "QTabBar::tab:selected{color:%4;border-bottom:2px solid %4;}"
                                    "QTabBar::tab:hover{color:%5;}")
                                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_TERTIARY(),
                                     ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));
    connect(tab_bar_, &QTabBar::currentChanged, this, &ChatAgentPanel::on_tab_changed);
    vl->addWidget(tab_bar_);

    stack_ = new QStackedWidget;
    stack_->addWidget(build_memory_tab());
    stack_->addWidget(build_schedules_tab());
    stack_->addWidget(build_tasks_tab());
    stack_->addWidget(build_mcp_tab());
    stack_->addWidget(build_monitors_tab());
    vl->addWidget(stack_, 1);
}

QPushButton* ChatAgentPanel::make_btn(const QString& text, const QString& tooltip) {
    auto* btn = new QPushButton(text);
    btn->setToolTip(tooltip);
    btn->setFixedHeight(22);
    btn->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid %3;"
                               "border-radius:0px;font-size:11px;"
                               "font-family:'Consolas','Courier New',monospace;}"
                               "QPushButton:hover{background:%4;color:%5;border-color:%6;}"
                               "QPushButton:disabled{color:%7;border-color:%1;}")
                           .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(),
                                ui::colors::BG_HOVER(), ui::colors::TEXT_PRIMARY(), ui::colors::AMBER(),
                                ui::colors::BORDER_BRIGHT()));
    return btn;
}

QWidget* ChatAgentPanel::build_memory_tab() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    mem_title_ = new QLabel(tr("AGENT MEMORY"));
    mem_title_->setStyleSheet(section_title_ss());
    vl->addWidget(mem_title_);

    memory_list_ = new QListWidget;
    memory_list_->setStyleSheet(list_ss());
    vl->addWidget(memory_list_, 1);

    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(4);
    mem_add_btn_ = make_btn(tr("+ Add"), tr("Add memory entry"));
    mem_del_btn_ = make_btn(tr("Delete"), tr("Delete selected"));
    mem_clear_btn_ = make_btn(tr("Clear"), tr("Clear all memory"));
    mem_del_btn_->setEnabled(false);
    btn_row->addWidget(mem_add_btn_);
    btn_row->addWidget(mem_del_btn_);
    btn_row->addWidget(mem_clear_btn_);
    vl->addLayout(btn_row);

    connect(memory_list_, &QListWidget::itemSelectionChanged, this,
            [this]() { mem_del_btn_->setEnabled(!memory_list_->selectedItems().isEmpty()); });
    connect(mem_add_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_add_memory);
    connect(mem_del_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_delete_memory);
    connect(mem_clear_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_clear_all_memory);

    return w;
}

QWidget* ChatAgentPanel::build_schedules_tab() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    sched_title_ = new QLabel(tr("SCHEDULED QUERIES"));
    sched_title_->setStyleSheet(section_title_ss());
    vl->addWidget(sched_title_);

    sched_hint_ = new QLabel(tr("Cron-based agent queries (e.g. daily 9 AM)."));
    sched_hint_->setWordWrap(true);
    sched_hint_->setStyleSheet(hint_ss());
    vl->addWidget(sched_hint_);

    sched_list_ = new QListWidget;
    sched_list_->setStyleSheet(list_ss());
    vl->addWidget(sched_list_, 1);

    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(4);
    sched_add_btn_ = make_btn(tr("+ Add"), tr("Create schedule"));
    sched_del_btn_ = make_btn(tr("Delete"), tr("Delete selected"));
    sched_toggle_btn_ = make_btn(tr("Pause"), tr("Pause/resume"));
    sched_del_btn_->setEnabled(false);
    sched_toggle_btn_->setEnabled(false);
    btn_row->addWidget(sched_add_btn_);
    btn_row->addWidget(sched_toggle_btn_);
    btn_row->addWidget(sched_del_btn_);
    vl->addLayout(btn_row);

    connect(sched_list_, &QListWidget::itemSelectionChanged, this, [this]() {
        const bool sel = !sched_list_->selectedItems().isEmpty();
        sched_del_btn_->setEnabled(sel);
        sched_toggle_btn_->setEnabled(sel);
    });
    connect(sched_add_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_add_schedule);
    connect(sched_del_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_delete_schedule);
    connect(sched_toggle_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_toggle_schedule);

    return w;
}

QWidget* ChatAgentPanel::build_tasks_tab() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    tasks_title_ = new QLabel(tr("BACKGROUND TASKS"));
    tasks_title_->setStyleSheet(section_title_ss());
    vl->addWidget(tasks_title_);

    task_status_lbl_ = new QLabel(tr("Long-running agent queries."));
    task_status_lbl_->setWordWrap(true);
    task_status_lbl_->setStyleSheet(hint_ss());
    vl->addWidget(task_status_lbl_);

    task_list_ = new QListWidget;
    task_list_->setStyleSheet(list_ss());
    vl->addWidget(task_list_, 1);

    auto* row1 = new QHBoxLayout;
    row1->setSpacing(4);
    task_refresh_btn_ = make_btn(tr("Refresh"), tr("Refresh task list"));
    task_detail_btn_ = make_btn(tr("Detail"), tr("View task result"));
    task_detail_btn_->setEnabled(false);
    row1->addWidget(task_refresh_btn_);
    row1->addWidget(task_detail_btn_);
    vl->addLayout(row1);

    auto* row2 = new QHBoxLayout;
    row2->setSpacing(4);
    task_feedback_btn_ = make_btn(tr("Feedback"), tr("Send feedback"));
    task_cancel_btn_ = make_btn(tr("Cancel"), tr("Cancel task"));
    task_feedback_btn_->setEnabled(false);
    task_cancel_btn_->setEnabled(false);
    row2->addWidget(task_feedback_btn_);
    row2->addWidget(task_cancel_btn_);
    row2->addStretch();
    vl->addLayout(row2);

    connect(task_list_, &QListWidget::itemSelectionChanged, this, [this]() {
        const bool sel = !task_list_->selectedItems().isEmpty();
        task_cancel_btn_->setEnabled(sel);
        task_detail_btn_->setEnabled(sel);
        task_feedback_btn_->setEnabled(sel);
    });
    connect(task_refresh_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_refresh_tasks);
    connect(task_cancel_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_cancel_task);
    connect(task_detail_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_view_task_detail);
    connect(task_feedback_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_send_feedback);

    return w;
}

QWidget* ChatAgentPanel::build_mcp_tab() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    mcp_title_ = new QLabel(tr("MCP SERVERS"));
    mcp_title_->setStyleSheet(section_title_ss());
    vl->addWidget(mcp_title_);

    mcp_tools_lbl_ = new QLabel(tr("Model Context Protocol tool servers."));
    mcp_tools_lbl_->setWordWrap(true);
    mcp_tools_lbl_->setStyleSheet(hint_ss());
    vl->addWidget(mcp_tools_lbl_);

    mcp_list_ = new QListWidget;
    mcp_list_->setStyleSheet(list_ss());
    vl->addWidget(mcp_list_, 1);

    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(4);
    mcp_add_btn_ = make_btn(tr("+ Add"), tr("Add MCP server"));
    mcp_del_btn_ = make_btn(tr("Delete"), tr("Remove selected"));
    mcp_refresh_btn_ = make_btn(tr("Refresh"), tr("Refresh all"));
    mcp_del_btn_->setEnabled(false);
    btn_row->addWidget(mcp_add_btn_);
    btn_row->addWidget(mcp_refresh_btn_);
    btn_row->addWidget(mcp_del_btn_);
    vl->addLayout(btn_row);

    connect(mcp_list_, &QListWidget::itemSelectionChanged, this,
            [this]() { mcp_del_btn_->setEnabled(!mcp_list_->selectedItems().isEmpty()); });
    connect(mcp_add_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_add_mcp_server);
    connect(mcp_del_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_delete_mcp_server);
    connect(mcp_refresh_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_refresh_mcp_servers);

    return w;
}

QWidget* ChatAgentPanel::build_monitors_tab() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    mon_title_ = new QLabel(tr("DATA MONITORS"));
    mon_title_->setStyleSheet(section_title_ss());
    vl->addWidget(mon_title_);

    mon_hint_ = new QLabel(tr("Watch sources, trigger agent analysis."));
    mon_hint_->setWordWrap(true);
    mon_hint_->setStyleSheet(hint_ss());
    vl->addWidget(mon_hint_);

    monitor_list_ = new QListWidget;
    monitor_list_->setStyleSheet(list_ss());
    vl->addWidget(monitor_list_, 1);

    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(4);
    mon_add_btn_ = make_btn(tr("+ Add"), tr("Create monitor"));
    mon_del_btn_ = make_btn(tr("Delete"), tr("Delete selected"));
    mon_toggle_btn_ = make_btn(tr("Pause"), tr("Pause/resume"));
    mon_del_btn_->setEnabled(false);
    mon_toggle_btn_->setEnabled(false);
    btn_row->addWidget(mon_add_btn_);
    btn_row->addWidget(mon_toggle_btn_);
    btn_row->addWidget(mon_del_btn_);
    vl->addLayout(btn_row);

    connect(monitor_list_, &QListWidget::itemSelectionChanged, this, [this]() {
        const bool sel = !monitor_list_->selectedItems().isEmpty();
        mon_del_btn_->setEnabled(sel);
        mon_toggle_btn_->setEnabled(sel);
    });
    connect(mon_add_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_add_monitor);
    connect(mon_del_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_delete_monitor);
    connect(mon_toggle_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_toggle_monitor);

    return w;
}

// ── Tab switch ────────────────────────────────────────────────────────────────

void ChatAgentPanel::on_tab_changed(int index) {
    stack_->setCurrentIndex(index);
}

// ── Refresh methods ───────────────────────────────────────────────────────────

void ChatAgentPanel::refresh_memory() {
    ChatModeService::instance().list_memory([this](bool ok, QVector<AgentMemory> memories, QString err) {
        memory_list_->clear();
        if (!ok) {
            auto* item = new QListWidgetItem(tr("Not available."));
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            memory_list_->addItem(item);
            LOG_WARN("ChatAgentPanel", "Memory load failed: " + err);
            return;
        }
        memories_ = std::move(memories);
        if (memories_.isEmpty()) {
            auto* item = new QListWidgetItem(tr("No entries yet."));
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            memory_list_->addItem(item);
            return;
        }
        for (const auto& m : memories_) {
            auto* item = new QListWidgetItem(memory_list_);
            item->setText(QString("[%1] %2: %3").arg(m.memory_type, m.key, m.value));
            item->setData(Qt::UserRole, m.key);
        }
    });
}

void ChatAgentPanel::refresh_schedules() {
    ChatModeService::instance().list_schedules([this](bool ok, QVector<AgentSchedule> schedules, QString err) {
        sched_list_->clear();
        if (!ok) {
            auto* item = new QListWidgetItem(tr("Not available."));
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            sched_list_->addItem(item);
            LOG_WARN("ChatAgentPanel", "Schedules load failed: " + err);
            return;
        }
        schedules_ = std::move(schedules);
        if (schedules_.isEmpty()) {
            auto* item = new QListWidgetItem(tr("No schedules yet."));
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            sched_list_->addItem(item);
            return;
        }
        for (const auto& s : schedules_) {
            auto* item = new QListWidgetItem(sched_list_);
            const QString icon = (s.status == "active") ? ">" : "||";
            item->setText(QString("%1 %2\n  %3").arg(icon, s.query.left(30), s.cron_expression));
            item->setData(Qt::UserRole, s.schedule_id);
            item->setData(Qt::UserRole + 1, s.status);
        }
    });
}

void ChatAgentPanel::refresh_tasks() {
    ChatModeService::instance().list_tasks([this](bool ok, QVector<AgentTask> tasks, QString err) {
        task_list_->clear();
        if (!ok) {
            auto* item = new QListWidgetItem(tr("Not available."));
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            task_list_->addItem(item);
            LOG_WARN("ChatAgentPanel", "Tasks load failed: " + err);
            return;
        }
        tasks_ = std::move(tasks);
        task_status_lbl_->setText(tr("%1 task(s)").arg(tasks_.size()));
        if (tasks_.isEmpty()) {
            auto* item = new QListWidgetItem(tr("No tasks yet."));
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            task_list_->addItem(item);
            return;
        }
        for (const auto& t : tasks_) {
            auto* item = new QListWidgetItem(task_list_);
            const QString icon = t.status == "completed"   ? "[OK]"
                                 : t.status == "error"     ? "[!]"
                                 : t.status == "cancelled" ? "[x]"
                                 : t.status == "running"   ? "[..]"
                                                           : "[.]";
            item->setText(QString("%1 %2\n  %3").arg(icon, t.query.left(35), t.status));
            item->setData(Qt::UserRole, t.task_id);
            // Color by status
            if (t.status == "completed")
                item->setForeground(QColor(ui::colors::POSITIVE()));
            else if (t.status == "error")
                item->setForeground(QColor(ui::colors::NEGATIVE()));
            else if (t.status == "running")
                item->setForeground(QColor(ui::colors::AMBER()));
        }
    });
}

void ChatAgentPanel::refresh_mcp_servers() {
    ChatModeService::instance().list_mcp_servers(
        [this](bool ok, QVector<McpServer> servers, int total_tools, QString err) {
            mcp_list_->clear();
            if (!ok) {
                auto* item = new QListWidgetItem(tr("Not available."));
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                mcp_list_->addItem(item);
                mcp_tools_lbl_->setText(tr("MCP not available."));
                LOG_WARN("ChatAgentPanel", "MCP load failed: " + err);
                return;
            }
            mcp_servers_ = std::move(servers);
            mcp_tools_lbl_->setText(tr("%1 server(s) | %2 tools").arg(mcp_servers_.size()).arg(total_tools));

            if (mcp_servers_.isEmpty()) {
                auto* item = new QListWidgetItem(tr("No servers connected."));
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                mcp_list_->addItem(item);
                return;
            }
            for (const auto& s : mcp_servers_) {
                auto* item = new QListWidgetItem(mcp_list_);
                item->setText(tr("%1\n  %2 | %3 tools").arg(s.name, s.status).arg(s.tools_count));
                item->setData(Qt::UserRole, s.name);
                if (s.status == "connected")
                    item->setForeground(QColor(ui::colors::POSITIVE()));
                else if (s.status == "error")
                    item->setForeground(QColor(ui::colors::NEGATIVE()));
                else
                    item->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
            }
        });
}

void ChatAgentPanel::refresh_monitors() {
    ChatModeService::instance().list_monitors([this](bool ok, QVector<AgentMonitor> monitors, QString err) {
        monitor_list_->clear();
        if (!ok) {
            auto* item = new QListWidgetItem(tr("Not available."));
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            monitor_list_->addItem(item);
            LOG_WARN("ChatAgentPanel", "Monitors load failed: " + err);
            return;
        }
        monitors_ = std::move(monitors);
        if (monitors_.isEmpty()) {
            auto* item = new QListWidgetItem(tr("No monitors configured."));
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            monitor_list_->addItem(item);
            return;
        }
        for (const auto& m : monitors_) {
            auto* item = new QListWidgetItem(monitor_list_);
            const QString icon = (m.status == "active") ? ">" : "||";
            item->setText(QString("%1 %2\n  %3 | %4s").arg(icon, m.name, m.source_type).arg(m.check_interval_seconds));
            item->setData(Qt::UserRole, m.monitor_id);
            item->setData(Qt::UserRole + 1, m.status);
            if (m.status == "active")
                item->setForeground(QColor(ui::colors::POSITIVE()));
        }
    });
}

// ── Memory actions ────────────────────────────────────────────────────────────

void ChatAgentPanel::on_add_memory() {
    bool ok = false;
    const QString key = QInputDialog::getText(this, tr("Add Memory"), tr("Key:"), QLineEdit::Normal, {}, &ok);
    if (!ok || key.trimmed().isEmpty())
        return;

    const QString value =
        QInputDialog::getText(this, tr("Add Memory"), tr("Value for \"%1\":").arg(key), QLineEdit::Normal, {}, &ok);
    if (!ok || value.trimmed().isEmpty())
        return;

    // Memory-type values are stored keys passed to the service — not translated.
    const QStringList types = {"fact", "preference", "entity"};
    const QString type = QInputDialog::getItem(this, tr("Type"), tr("Memory type:"), types, 0, false, &ok);
    if (!ok)
        return;

    ChatModeService::instance().save_memory(key.trimmed(), value.trimmed(), type, [this](bool saved_ok, QString err) {
        if (!saved_ok)
            LOG_WARN("ChatAgentPanel", "Save memory failed: " + err);
        refresh_memory();
    });
}

void ChatAgentPanel::on_delete_memory() {
    auto* item = memory_list_->currentItem();
    if (!item)
        return;
    const QString key = item->data(Qt::UserRole).toString();
    if (QMessageBox::question(this, tr("Delete Memory"), tr("Delete \"%1\"?").arg(key),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    ChatModeService::instance().delete_memory(key, [this](bool ok, QString err) {
        if (!ok)
            LOG_WARN("ChatAgentPanel", "Delete memory failed: " + err);
        refresh_memory();
    });
}

void ChatAgentPanel::on_clear_all_memory() {
    if (QMessageBox::question(this, tr("Clear Memory"), tr("Delete ALL memory entries?"), QMessageBox::Yes | QMessageBox::No) !=
        QMessageBox::Yes)
        return;
    ChatModeService::instance().clear_all_memory([this](bool ok, QString err) {
        if (!ok)
            LOG_WARN("ChatAgentPanel", "Clear memory failed: " + err);
        refresh_memory();
    });
}

// ── Schedule actions ──────────────────────────────────────────────────────────

void ChatAgentPanel::on_add_schedule() {
    bool ok = false;
    const QString query = QInputDialog::getText(this, tr("New Schedule"), tr("Query:"), QLineEdit::Normal, {}, &ok);
    if (!ok || query.trimmed().isEmpty())
        return;

    const QString cron =
        QInputDialog::getText(this, tr("New Schedule"), tr("Cron (e.g. 0 9 * * 1-5):"), QLineEdit::Normal, "0 9 * * 1-5", &ok);
    if (!ok || cron.trimmed().isEmpty())
        return;

    ChatModeService::instance().create_schedule(query.trimmed(), cron.trimmed(), {},
                                                [this](bool created_ok, AgentSchedule, QString err) {
                                                    if (!created_ok)
                                                        LOG_WARN("ChatAgentPanel", "Create schedule failed: " + err);
                                                    refresh_schedules();
                                                });
}

void ChatAgentPanel::on_delete_schedule() {
    auto* item = sched_list_->currentItem();
    if (!item)
        return;
    ChatModeService::instance().delete_schedule(item->data(Qt::UserRole).toString(), [this](bool ok, QString err) {
        if (!ok)
            LOG_WARN("ChatAgentPanel", "Delete schedule failed: " + err);
        refresh_schedules();
    });
}

void ChatAgentPanel::on_toggle_schedule() {
    auto* item = sched_list_->currentItem();
    if (!item)
        return;
    const QString id = item->data(Qt::UserRole).toString();
    const QString status = item->data(Qt::UserRole + 1).toString();
    auto cb = [this](bool ok, QString err) {
        if (!ok)
            LOG_WARN("ChatAgentPanel", "Toggle schedule failed: " + err);
        refresh_schedules();
    };
    if (status == "active")
        ChatModeService::instance().pause_schedule(id, cb);
    else
        ChatModeService::instance().resume_schedule(id, cb);
}

// ── Task actions ──────────────────────────────────────────────────────────────

void ChatAgentPanel::on_refresh_tasks() {
    refresh_tasks();
}

void ChatAgentPanel::on_cancel_task() {
    auto* item = task_list_->currentItem();
    if (!item)
        return;
    ChatModeService::instance().cancel_task(item->data(Qt::UserRole).toString(), [this](bool ok, QString err) {
        if (!ok)
            LOG_WARN("ChatAgentPanel", "Cancel task failed: " + err);
        refresh_tasks();
    });
}

void ChatAgentPanel::on_view_task_detail() {
    auto* item = task_list_->currentItem();
    if (!item)
        return;
    ChatModeService::instance().get_task(
        item->data(Qt::UserRole).toString(), [this](bool ok, AgentTask task, QString err) {
            if (!ok) {
                QMessageBox::warning(this, tr("Task"), tr("Failed: %1").arg(err));
                return;
            }
            QString info = tr("ID: %1\nQuery: %2\nStatus: %3\n"
                              "Created: %4\nStarted: %5\nCompleted: %6\n\n"
                              "Result:\n%7")
                               .arg(task.task_id, task.query, task.status, task.created_at, task.started_at,
                                    task.completed_at, task.result.isEmpty() ? tr("(none)") : task.result.left(2000));
            QMessageBox::information(this, tr("Task Detail"), info);
        });
}

void ChatAgentPanel::on_send_feedback() {
    auto* item = task_list_->currentItem();
    if (!item)
        return;
    bool ok = false;
    const QString feedback = QInputDialog::getText(this, tr("Task Feedback"), tr("Feedback:"), QLineEdit::Normal, {}, &ok);
    if (!ok || feedback.trimmed().isEmpty())
        return;

    ChatModeService::instance().send_task_feedback(item->data(Qt::UserRole).toString(), feedback.trimmed(),
                                                   [this](bool sent_ok, QString err) {
                                                       if (!sent_ok)
                                                           LOG_WARN("ChatAgentPanel", "Feedback failed: " + err);
                                                       else
                                                           QMessageBox::information(this, tr("Feedback"), tr("Sent."));
                                                   });
}

// ── MCP Server actions ────────────────────────────────────────────────────────

void ChatAgentPanel::on_add_mcp_server() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Add MCP Server"), tr("Name:"), QLineEdit::Normal, {}, &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    // Transport values are protocol identifiers passed to the service — not translated.
    const QStringList transports = {"stdio", "sse"};
    const QString transport = QInputDialog::getItem(this, tr("Transport"), tr("Type:"), transports, 0, false, &ok);
    if (!ok)
        return;

    QJsonObject config;
    config["transport"] = transport;

    if (transport == "stdio") {
        const QString command =
            QInputDialog::getText(this, tr("Command"), tr("Command (e.g. uvx, npx):"), QLineEdit::Normal, "uvx", &ok);
        if (!ok || command.trimmed().isEmpty())
            return;

        const QString args_str =
            QInputDialog::getText(this, tr("Arguments"), tr("Args (space-separated):"), QLineEdit::Normal, {}, &ok);
        if (!ok)
            return;

        config["command"] = command.trimmed();
        QJsonArray args;
        for (const auto& a : args_str.split(' ', Qt::SkipEmptyParts))
            args.append(a.trimmed());
        config["args"] = args;
    } else {
        const QString url = QInputDialog::getText(this, tr("SSE URL"), tr("URL:"), QLineEdit::Normal, {}, &ok);
        if (!ok || url.trimmed().isEmpty())
            return;
        config["url"] = url.trimmed();
    }

    ChatModeService::instance().add_mcp_server(
        name.trimmed(), config, [this](bool added_ok, McpServer srv, QString err) {
            if (!added_ok) {
                QMessageBox::warning(this, tr("MCP Server"), tr("Failed: %1").arg(err));
                return;
            }
            if (srv.status == "error")
                QMessageBox::warning(this, tr("MCP Server"), tr("'%1' added with errors.").arg(srv.name));
            refresh_mcp_servers();
        });
}

void ChatAgentPanel::on_delete_mcp_server() {
    auto* item = mcp_list_->currentItem();
    if (!item)
        return;
    const QString name = item->data(Qt::UserRole).toString();
    if (QMessageBox::question(this, tr("Remove Server"), tr("Remove \"%1\"?").arg(name),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    ChatModeService::instance().delete_mcp_server(name, [this](bool ok, QString err) {
        if (!ok)
            LOG_WARN("ChatAgentPanel", "Delete MCP server failed: " + err);
        refresh_mcp_servers();
    });
}

void ChatAgentPanel::on_refresh_mcp_servers() {
    ChatModeService::instance().refresh_mcp_servers(
        [this](bool ok, QVector<McpServer> servers, int total_tools, QString err) {
            if (!ok) {
                LOG_WARN("ChatAgentPanel", "Refresh MCP failed: " + err);
                return;
            }
            mcp_servers_ = std::move(servers);
            mcp_list_->clear();
            mcp_tools_lbl_->setText(tr("%1 server(s) | %2 tools").arg(mcp_servers_.size()).arg(total_tools));
            if (mcp_servers_.isEmpty()) {
                auto* item = new QListWidgetItem(tr("No servers connected."));
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                mcp_list_->addItem(item);
                return;
            }
            for (const auto& s : mcp_servers_) {
                auto* item = new QListWidgetItem(mcp_list_);
                item->setText(tr("%1\n  %2 | %3 tools").arg(s.name, s.status).arg(s.tools_count));
                item->setData(Qt::UserRole, s.name);
                if (s.status == "connected")
                    item->setForeground(QColor(ui::colors::POSITIVE()));
                else if (s.status == "error")
                    item->setForeground(QColor(ui::colors::NEGATIVE()));
                else
                    item->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
            }
        });
}

// ── Monitor actions ───────────────────────────────────────────────────────────

void ChatAgentPanel::on_add_monitor() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New Monitor"), tr("Name:"), QLineEdit::Normal, {}, &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    // Source-type values are data-source identifiers passed to the service — not translated.
    const QStringList types = {"equity", "macro", "news"};
    const QString source_type = QInputDialog::getItem(this, tr("Source"), tr("Data source:"), types, 0, false, &ok);
    if (!ok)
        return;

    const QString analysis =
        QInputDialog::getText(this, tr("Analysis"), tr("What should the agent analyze?:"), QLineEdit::Normal, {}, &ok);
    if (!ok || analysis.trimmed().isEmpty())
        return;

    const int interval = QInputDialog::getInt(this, tr("Interval"), tr("Check interval (seconds):"), 300, 60, 86400, 60, &ok);
    if (!ok)
        return;

    QJsonObject source_config;
    if (source_type == "equity") {
        const QString sym = QInputDialog::getText(this, tr("Symbol"), tr("Symbol:"), QLineEdit::Normal, {}, &ok);
        if (!ok || sym.trimmed().isEmpty())
            return;
        source_config["symbol"] = sym.trimmed();
    } else if (source_type == "macro") {
        const QString ind = QInputDialog::getText(this, tr("Indicator"), tr("Indicator:"), QLineEdit::Normal, {}, &ok);
        if (!ok || ind.trimmed().isEmpty())
            return;
        source_config["indicator"] = ind.trimmed();
    } else {
        const QString q = QInputDialog::getText(this, tr("Query"), tr("News query:"), QLineEdit::Normal, {}, &ok);
        if (!ok || q.trimmed().isEmpty())
            return;
        source_config["query"] = q.trimmed();
    }

    QJsonObject trigger_config;
    // "any" is the stable default condition value sent to the service — not translated.
    const QString cond = QInputDialog::getText(this, tr("Trigger"), tr("Condition:"), QLineEdit::Normal, "any", &ok);
    if (!ok)
        return;
    trigger_config["condition"] = cond;

    ChatModeService::instance().create_monitor(name.trimmed(), source_type, source_config, trigger_config,
                                               analysis.trimmed(), interval, {},
                                               [this](bool created_ok, AgentMonitor, QString err) {
                                                   if (!created_ok)
                                                       QMessageBox::warning(this, tr("Monitor"), tr("Failed: %1").arg(err));
                                                   refresh_monitors();
                                               });
}

void ChatAgentPanel::on_delete_monitor() {
    auto* item = monitor_list_->currentItem();
    if (!item)
        return;
    if (QMessageBox::question(this, tr("Delete Monitor"), tr("Delete?"), QMessageBox::Yes | QMessageBox::No) !=
        QMessageBox::Yes)
        return;
    ChatModeService::instance().delete_monitor(item->data(Qt::UserRole).toString(), [this](bool ok, QString err) {
        if (!ok)
            LOG_WARN("ChatAgentPanel", "Delete monitor failed: " + err);
        refresh_monitors();
    });
}

void ChatAgentPanel::on_toggle_monitor() {
    auto* item = monitor_list_->currentItem();
    if (!item)
        return;
    const QString id = item->data(Qt::UserRole).toString();
    const QString status = item->data(Qt::UserRole + 1).toString();
    auto cb = [this](bool ok, QString err) {
        if (!ok)
            LOG_WARN("ChatAgentPanel", "Toggle monitor failed: " + err);
        refresh_monitors();
    };
    if (status == "active")
        ChatModeService::instance().pause_monitor(id, cb);
    else
        ChatModeService::instance().resume_monitor(id, cb);
}

// ── Re-translation ──────────────────────────────────────────────────────────

void ChatAgentPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ChatAgentPanel::retranslateUi() {
    // Tab bar
    if (tab_bar_ && tab_bar_->count() >= 5) {
        tab_bar_->setTabText(0, tr("Mem"));
        tab_bar_->setTabText(1, tr("Sched"));
        tab_bar_->setTabText(2, tr("Tasks"));
        tab_bar_->setTabText(3, tr("MCP"));
        tab_bar_->setTabText(4, tr("Monitor"));
    }

    // Section titles + hints
    if (mem_title_)   mem_title_->setText(tr("AGENT MEMORY"));
    if (sched_title_) sched_title_->setText(tr("SCHEDULED QUERIES"));
    if (sched_hint_)  sched_hint_->setText(tr("Cron-based agent queries (e.g. daily 9 AM)."));
    if (tasks_title_) tasks_title_->setText(tr("BACKGROUND TASKS"));
    if (mcp_title_)   mcp_title_->setText(tr("MCP SERVERS"));
    if (mon_title_)   mon_title_->setText(tr("DATA MONITORS"));
    if (mon_hint_)    mon_hint_->setText(tr("Watch sources, trigger agent analysis."));

    // Buttons (text + tooltip)
    if (mem_add_btn_)     { mem_add_btn_->setText(tr("+ Add"));    mem_add_btn_->setToolTip(tr("Add memory entry")); }
    if (mem_del_btn_)     { mem_del_btn_->setText(tr("Delete"));   mem_del_btn_->setToolTip(tr("Delete selected")); }
    if (mem_clear_btn_)   { mem_clear_btn_->setText(tr("Clear"));  mem_clear_btn_->setToolTip(tr("Clear all memory")); }
    if (sched_add_btn_)   { sched_add_btn_->setText(tr("+ Add"));  sched_add_btn_->setToolTip(tr("Create schedule")); }
    if (sched_del_btn_)   { sched_del_btn_->setText(tr("Delete")); sched_del_btn_->setToolTip(tr("Delete selected")); }
    if (sched_toggle_btn_){ sched_toggle_btn_->setText(tr("Pause")); sched_toggle_btn_->setToolTip(tr("Pause/resume")); }
    if (task_refresh_btn_){ task_refresh_btn_->setText(tr("Refresh")); task_refresh_btn_->setToolTip(tr("Refresh task list")); }
    if (task_detail_btn_) { task_detail_btn_->setText(tr("Detail")); task_detail_btn_->setToolTip(tr("View task result")); }
    if (task_feedback_btn_){ task_feedback_btn_->setText(tr("Feedback")); task_feedback_btn_->setToolTip(tr("Send feedback")); }
    if (task_cancel_btn_) { task_cancel_btn_->setText(tr("Cancel")); task_cancel_btn_->setToolTip(tr("Cancel task")); }
    if (mcp_add_btn_)     { mcp_add_btn_->setText(tr("+ Add"));    mcp_add_btn_->setToolTip(tr("Add MCP server")); }
    if (mcp_del_btn_)     { mcp_del_btn_->setText(tr("Delete"));   mcp_del_btn_->setToolTip(tr("Remove selected")); }
    if (mcp_refresh_btn_) { mcp_refresh_btn_->setText(tr("Refresh")); mcp_refresh_btn_->setToolTip(tr("Refresh all")); }
    if (mon_add_btn_)     { mon_add_btn_->setText(tr("+ Add"));    mon_add_btn_->setToolTip(tr("Create monitor")); }
    if (mon_del_btn_)     { mon_del_btn_->setText(tr("Delete"));   mon_del_btn_->setToolTip(tr("Delete selected")); }
    if (mon_toggle_btn_)  { mon_toggle_btn_->setText(tr("Pause")); mon_toggle_btn_->setToolTip(tr("Pause/resume")); }

    // Re-render lists so placeholders / count labels pick up the new language.
    refresh_memory();
    refresh_schedules();
    refresh_tasks();
    refresh_mcp_servers();
    refresh_monitors();
}

} // namespace fincept::chat_mode
