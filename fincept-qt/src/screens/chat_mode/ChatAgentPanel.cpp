#include "screens/chat_mode/ChatAgentPanel.h"
#include "screens/chat_mode/ChatModeService.h"
#include "core/logging/Logger.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QVBoxLayout>

namespace fincept::chat_mode {

static const char* LIST_SS =
    "QListWidget{background:#0d0d0d;border:1px solid #1a1a1a;color:#e5e5e5;"
    "font-size:12px;font-family:'Consolas',monospace;border-radius:4px;outline:none;}"
    "QListWidget::item{padding:6px 8px;border-bottom:1px solid #111111;}"
    "QListWidget::item:selected{background:#1a1a1a;color:#d97706;}"
    "QListWidget::item:hover{background:#111111;}"
    "QScrollBar:vertical{background:#0d0d0d;width:4px;}"
    "QScrollBar::handle:vertical{background:#2a2a2a;border-radius:2px;}"
    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}";

// ── Constructor ───────────────────────────────────────────────────────────────

ChatAgentPanel::ChatAgentPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    refresh_memory();
    refresh_schedules();
    refresh_tasks();
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void ChatAgentPanel::build_ui() {
    setFixedWidth(280);
    setStyleSheet("background:#0a0a0a;border-left:1px solid #1a1a1a;");

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Tab bar
    tab_bar_ = new QTabBar;
    tab_bar_->setExpanding(true);
    tab_bar_->addTab("Memory");
    tab_bar_->addTab("Schedules");
    tab_bar_->addTab("Tasks");
    tab_bar_->setStyleSheet(
        "QTabBar{background:#0a0a0a;border-bottom:1px solid #1a1a1a;}"
        "QTabBar::tab{background:#0a0a0a;color:#606060;padding:7px 0;"
        "font-size:11px;font-family:'Consolas',monospace;border:none;"
        "border-bottom:2px solid transparent;}"
        "QTabBar::tab:selected{color:#d97706;border-bottom:2px solid #d97706;}"
        "QTabBar::tab:hover{color:#e5e5e5;}");
    connect(tab_bar_, &QTabBar::currentChanged, this, &ChatAgentPanel::on_tab_changed);
    vl->addWidget(tab_bar_);

    // Stacked content
    stack_ = new QStackedWidget;
    stack_->addWidget(build_memory_tab());
    stack_->addWidget(build_schedules_tab());
    stack_->addWidget(build_tasks_tab());
    vl->addWidget(stack_, 1);
}

QPushButton* ChatAgentPanel::make_action_btn(const QString& text, const QString& tooltip) {
    auto* btn = new QPushButton(text);
    btn->setToolTip(tooltip);
    btn->setFixedHeight(24);
    btn->setStyleSheet(
        "QPushButton{background:#111111;color:#808080;border:1px solid #2a2a2a;"
        "border-radius:3px;font-size:11px;font-family:'Consolas',monospace;}"
        "QPushButton:hover{background:#1a1a1a;color:#e5e5e5;border-color:#d97706;}"
        "QPushButton:disabled{color:#333333;border-color:#1a1a1a;}");
    return btn;
}

QWidget* ChatAgentPanel::build_memory_tab() {
    auto* w  = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* title = new QLabel("Agent Memory");
    title->setStyleSheet(
        "color:#606060;font-size:11px;font-weight:700;"
        "font-family:'Consolas',monospace;background:transparent;");
    vl->addWidget(title);

    memory_list_ = new QListWidget;
    memory_list_->setStyleSheet(LIST_SS);
    vl->addWidget(memory_list_, 1);

    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(4);
    mem_add_btn_   = make_action_btn("+ Add",    "Add a memory entry");
    mem_del_btn_   = make_action_btn("Delete",   "Delete selected entry");
    mem_clear_btn_ = make_action_btn("Clear All","Clear all memory");
    mem_del_btn_->setEnabled(false);
    btn_row->addWidget(mem_add_btn_);
    btn_row->addWidget(mem_del_btn_);
    btn_row->addWidget(mem_clear_btn_);
    vl->addLayout(btn_row);

    connect(memory_list_, &QListWidget::itemSelectionChanged, this, [this]() {
        mem_del_btn_->setEnabled(!memory_list_->selectedItems().isEmpty());
    });
    connect(mem_add_btn_,   &QPushButton::clicked, this, &ChatAgentPanel::on_add_memory);
    connect(mem_del_btn_,   &QPushButton::clicked, this, &ChatAgentPanel::on_delete_memory);
    connect(mem_clear_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_clear_all_memory);

    return w;
}

QWidget* ChatAgentPanel::build_schedules_tab() {
    auto* w  = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* title = new QLabel("Scheduled Queries");
    title->setStyleSheet(
        "color:#606060;font-size:11px;font-weight:700;"
        "font-family:'Consolas',monospace;background:transparent;");
    vl->addWidget(title);

    auto* hint = new QLabel("Run agent queries on a schedule (e.g. daily at 9 AM).");
    hint->setWordWrap(true);
    hint->setStyleSheet(
        "color:#404040;font-size:10px;font-family:'Consolas',monospace;background:transparent;");
    vl->addWidget(hint);

    sched_list_ = new QListWidget;
    sched_list_->setStyleSheet(LIST_SS);
    vl->addWidget(sched_list_, 1);

    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(4);
    sched_add_btn_    = make_action_btn("+ Add",  "Create a schedule");
    sched_del_btn_    = make_action_btn("Delete", "Delete selected");
    sched_toggle_btn_ = make_action_btn("Pause",  "Pause / resume selected");
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
    connect(sched_add_btn_,    &QPushButton::clicked, this, &ChatAgentPanel::on_add_schedule);
    connect(sched_del_btn_,    &QPushButton::clicked, this, &ChatAgentPanel::on_delete_schedule);
    connect(sched_toggle_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_toggle_schedule);

    return w;
}

QWidget* ChatAgentPanel::build_tasks_tab() {
    auto* w  = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* title = new QLabel("Background Tasks");
    title->setStyleSheet(
        "color:#606060;font-size:11px;font-weight:700;"
        "font-family:'Consolas',monospace;background:transparent;");
    vl->addWidget(title);

    task_status_lbl_ = new QLabel("Long-running agent queries.");
    task_status_lbl_->setWordWrap(true);
    task_status_lbl_->setStyleSheet(
        "color:#404040;font-size:10px;font-family:'Consolas',monospace;background:transparent;");
    vl->addWidget(task_status_lbl_);

    task_list_ = new QListWidget;
    task_list_->setStyleSheet(LIST_SS);
    vl->addWidget(task_list_, 1);

    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(4);
    task_refresh_btn_ = make_action_btn("Refresh", "Refresh task list");
    task_cancel_btn_  = make_action_btn("Cancel",  "Cancel selected task");
    task_cancel_btn_->setEnabled(false);
    btn_row->addWidget(task_refresh_btn_);
    btn_row->addWidget(task_cancel_btn_);
    btn_row->addStretch();
    vl->addLayout(btn_row);

    connect(task_list_, &QListWidget::itemSelectionChanged, this, [this]() {
        task_cancel_btn_->setEnabled(!task_list_->selectedItems().isEmpty());
    });
    connect(task_refresh_btn_, &QPushButton::clicked, this, &ChatAgentPanel::on_refresh_tasks);
    connect(task_cancel_btn_,  &QPushButton::clicked, this, &ChatAgentPanel::on_cancel_task);

    return w;
}

// ── Tab switch ────────────────────────────────────────────────────────────────

void ChatAgentPanel::on_tab_changed(int index) {
    stack_->setCurrentIndex(index);
}

// ── Refresh methods ───────────────────────────────────────────────────────────

void ChatAgentPanel::refresh_memory() {
    ChatModeService::instance().list_memory(
        [this](bool ok, QVector<AgentMemory> memories, QString err) {
            memory_list_->clear();
            if (!ok) {
                auto* item = new QListWidgetItem(memory_list_);
                item->setText("Agent memory not available on this plan.");
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                LOG_WARN("ChatAgentPanel", "Memory load failed: " + err);
                return;
            }
            memories_ = std::move(memories);
            if (memories_.isEmpty()) {
                auto* item = new QListWidgetItem(memory_list_);
                item->setText("No memory entries yet.");
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                return;
            }
            for (const auto& m : memories_) {
                auto* item = new QListWidgetItem(memory_list_);
                item->setText(QString("[%1] %2\n%3").arg(m.memory_type, m.key, m.value));
                item->setData(Qt::UserRole, m.key);
            }
        });
}

void ChatAgentPanel::refresh_schedules() {
    ChatModeService::instance().list_schedules(
        [this](bool ok, QVector<AgentSchedule> schedules, QString err) {
            sched_list_->clear();
            if (!ok) {
                auto* item = new QListWidgetItem(sched_list_);
                item->setText("Schedules not available on this plan.");
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                LOG_WARN("ChatAgentPanel", "Schedules load failed: " + err);
                return;
            }
            schedules_ = std::move(schedules);
            if (schedules_.isEmpty()) {
                auto* item = new QListWidgetItem(sched_list_);
                item->setText("No schedules yet.");
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                return;
            }
            for (const auto& s : schedules_) {
                auto* item = new QListWidgetItem(sched_list_);
                const QString status_icon = (s.status == "active") ? "▶" : "⏸";
                item->setText(QString("%1 %2\n%3").arg(status_icon, s.query, s.cron_expression));
                item->setData(Qt::UserRole,     s.schedule_id);
                item->setData(Qt::UserRole + 1, s.status);
            }
        });
}

void ChatAgentPanel::refresh_tasks() {
    ChatModeService::instance().list_tasks(
        [this](bool ok, QVector<AgentTask> tasks, QString err) {
            task_list_->clear();
            if (!ok) {
                auto* item = new QListWidgetItem(task_list_);
                item->setText("Background tasks not available on this plan.");
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                LOG_WARN("ChatAgentPanel", "Tasks load failed: " + err);
                return;
            }
            tasks_ = std::move(tasks);
            if (tasks_.isEmpty()) {
                auto* item = new QListWidgetItem(task_list_);
                item->setText("No background tasks yet.");
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                return;
            }
            for (const auto& t : tasks_) {
                auto* item = new QListWidgetItem(task_list_);
                const QString icon =
                    t.status == "complete"  ? "✓" :
                    t.status == "error"     ? "✗" :
                    t.status == "cancelled" ? "⊘" :
                    t.status == "running"   ? "⟳" : "·";
                item->setText(QString("%1 %2\n%3").arg(icon, t.query.left(40), t.status));
                item->setData(Qt::UserRole, t.task_id);
            }
        });
}

// ── Memory actions ────────────────────────────────────────────────────────────

void ChatAgentPanel::on_add_memory() {
    bool ok = false;
    const QString key = QInputDialog::getText(
        this, "Add Memory", "Key (e.g. risk_tolerance):", QLineEdit::Normal, {}, &ok);
    if (!ok || key.trimmed().isEmpty()) return;

    const QString value = QInputDialog::getText(
        this, "Add Memory", QString("Value for \"%1\":").arg(key), QLineEdit::Normal, {}, &ok);
    if (!ok || value.trimmed().isEmpty()) return;

    // Type selection
    const QStringList types = {"fact", "preference", "entity"};
    const QString type = QInputDialog::getItem(
        this, "Memory Type", "Type:", types, 0, false, &ok);
    if (!ok) return;

    ChatModeService::instance().save_memory(
        key.trimmed(), value.trimmed(), type,
        [this](bool saved_ok, QString err) {
            if (!saved_ok) {
                LOG_WARN("ChatAgentPanel", "Save memory failed: " + err);
                return;
            }
            refresh_memory();
        });
}

void ChatAgentPanel::on_delete_memory() {
    auto* item = memory_list_->currentItem();
    if (!item) return;
    const QString key = item->data(Qt::UserRole).toString();
    if (QMessageBox::question(this, "Delete Memory",
            QString("Delete memory entry \"%1\"?").arg(key),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    ChatModeService::instance().delete_memory(
        key, [this](bool ok, QString err) {
            if (!ok) LOG_WARN("ChatAgentPanel", "Delete memory failed: " + err);
            refresh_memory();
        });
}

void ChatAgentPanel::on_clear_all_memory() {
    if (QMessageBox::question(this, "Clear All Memory",
            "Delete ALL agent memory entries? This cannot be undone.",
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    ChatModeService::instance().clear_all_memory(
        [this](bool ok, QString err) {
            if (!ok) LOG_WARN("ChatAgentPanel", "Clear memory failed: " + err);
            refresh_memory();
        });
}

// ── Schedule actions ──────────────────────────────────────────────────────────

void ChatAgentPanel::on_add_schedule() {
    bool ok = false;
    const QString query = QInputDialog::getText(
        this, "New Schedule", "Query to run:", QLineEdit::Normal, {}, &ok);
    if (!ok || query.trimmed().isEmpty()) return;

    const QString cron = QInputDialog::getText(
        this, "New Schedule",
        "Cron expression (e.g. \"0 9 * * 1-5\" = weekdays at 9 AM):",
        QLineEdit::Normal, "0 9 * * 1-5", &ok);
    if (!ok || cron.trimmed().isEmpty()) return;

    ChatModeService::instance().create_schedule(
        query.trimmed(), cron.trimmed(), {},
        [this](bool created_ok, AgentSchedule, QString err) {
            if (!created_ok) {
                LOG_WARN("ChatAgentPanel", "Create schedule failed: " + err);
                return;
            }
            refresh_schedules();
        });
}

void ChatAgentPanel::on_delete_schedule() {
    auto* item = sched_list_->currentItem();
    if (!item) return;
    const QString id = item->data(Qt::UserRole).toString();
    ChatModeService::instance().delete_schedule(
        id, [this](bool ok, QString err) {
            if (!ok) LOG_WARN("ChatAgentPanel", "Delete schedule failed: " + err);
            refresh_schedules();
        });
}

void ChatAgentPanel::on_toggle_schedule() {
    auto* item = sched_list_->currentItem();
    if (!item) return;
    const QString id     = item->data(Qt::UserRole).toString();
    const QString status = item->data(Qt::UserRole + 1).toString();

    if (status == "active") {
        ChatModeService::instance().pause_schedule(
            id, [this](bool ok, QString err) {
                if (!ok) LOG_WARN("ChatAgentPanel", "Pause schedule failed: " + err);
                refresh_schedules();
            });
    } else {
        ChatModeService::instance().resume_schedule(
            id, [this](bool ok, QString err) {
                if (!ok) LOG_WARN("ChatAgentPanel", "Resume schedule failed: " + err);
                refresh_schedules();
            });
    }
}

// ── Task actions ──────────────────────────────────────────────────────────────

void ChatAgentPanel::on_refresh_tasks() {
    refresh_tasks();
}

void ChatAgentPanel::on_cancel_task() {
    auto* item = task_list_->currentItem();
    if (!item) return;
    const QString id = item->data(Qt::UserRole).toString();
    ChatModeService::instance().cancel_task(
        id, [this](bool ok, QString err) {
            if (!ok) LOG_WARN("ChatAgentPanel", "Cancel task failed: " + err);
            refresh_tasks();
        });
}

} // namespace fincept::chat_mode
