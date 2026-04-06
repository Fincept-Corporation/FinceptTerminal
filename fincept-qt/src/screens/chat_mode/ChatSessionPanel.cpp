#include "screens/chat_mode/ChatSessionPanel.h"
#include "screens/chat_mode/ChatModeService.h"
#include "core/logging/Logger.h"

#include <QInputDialog>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace fincept::chat_mode {

ChatSessionPanel::ChatSessionPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    refresh_sessions();
}

void ChatSessionPanel::build_ui() {
    setFixedWidth(240);
    setStyleSheet("background:#0a0a0a;border-right:1px solid #1a1a1a;");

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    // Header row: title + exit button
    auto* header_row = new QHBoxLayout;
    header_row->setSpacing(4);
    auto* title_lbl = new QLabel("CONVERSATIONS");
    title_lbl->setStyleSheet(
        "color:#d97706;font-size:11px;font-weight:700;font-family:'Consolas',monospace;"
        "background:transparent;letter-spacing:1px;");
    header_row->addWidget(title_lbl);
    header_row->addStretch();

    exit_btn_ = new QPushButton("×");
    exit_btn_->setFixedSize(20, 20);
    exit_btn_->setToolTip("Exit Chat Mode");
    exit_btn_->setStyleSheet(
        "QPushButton{background:#1a1a1a;color:#808080;border:1px solid #2a2a2a;"
        "border-radius:3px;font-size:14px;font-family:'Consolas',monospace;}"
        "QPushButton:hover{background:#2a2a2a;color:#e5e5e5;}");
    connect(exit_btn_, &QPushButton::clicked, this, &ChatSessionPanel::exit_chat_mode_requested);
    header_row->addWidget(exit_btn_);
    vl->addLayout(header_row);

    // Search
    search_edit_ = new QLineEdit;
    search_edit_->setPlaceholderText("Search conversations…");
    search_edit_->setStyleSheet(
        "QLineEdit{background:#111111;color:#e5e5e5;border:1px solid #2a2a2a;"
        "border-radius:4px;padding:4px 8px;font-size:12px;font-family:'Consolas',monospace;}"
        "QLineEdit:focus{border:1px solid #d97706;}");
    connect(search_edit_, &QLineEdit::textChanged, this, &ChatSessionPanel::on_search_changed);
    vl->addWidget(search_edit_);

    // Session list
    session_list_ = new QListWidget;
    session_list_->setStyleSheet(
        "QListWidget{background:#0a0a0a;border:none;color:#e5e5e5;"
        "font-size:12px;font-family:'Consolas',monospace;outline:none;}"
        "QListWidget::item{padding:8px 6px;border-bottom:1px solid #111111;"
        "border-radius:3px;}"
        "QListWidget::item:selected{background:#1a1a1a;color:#d97706;}"
        "QListWidget::item:hover{background:#111111;}"
        "QScrollBar:vertical{background:#0a0a0a;width:4px;}"
        "QScrollBar::handle:vertical{background:#2a2a2a;border-radius:2px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");
    session_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(session_list_, &QListWidget::itemClicked,
            this, &ChatSessionPanel::on_item_clicked);
    vl->addWidget(session_list_, 1);

    // Stats label
    stats_lbl_ = new QLabel;
    stats_lbl_->setStyleSheet(
        "color:#404040;font-size:11px;font-family:'Consolas',monospace;background:transparent;");
    stats_lbl_->setAlignment(Qt::AlignCenter);
    vl->addWidget(stats_lbl_);

    // Action buttons
    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(4);

    auto mk_btn = [](const QString& text, const QString& tip) -> QPushButton* {
        auto* btn = new QPushButton(text);
        btn->setToolTip(tip);
        btn->setFixedHeight(26);
        btn->setStyleSheet(
            "QPushButton{background:#111111;color:#808080;border:1px solid #2a2a2a;"
            "border-radius:4px;font-size:12px;font-family:'Consolas',monospace;}"
            "QPushButton:hover{background:#1a1a1a;color:#e5e5e5;border-color:#d97706;}"
            "QPushButton:disabled{color:#333333;border-color:#1a1a1a;}");
        return btn;
    };

    new_btn_    = mk_btn("+  New",  "New conversation");
    delete_btn_ = mk_btn("Delete",  "Delete selected");
    rename_btn_ = mk_btn("Rename",  "Rename selected");

    delete_btn_->setEnabled(false);
    rename_btn_->setEnabled(false);

    btn_row->addWidget(new_btn_);
    btn_row->addWidget(rename_btn_);
    btn_row->addWidget(delete_btn_);
    vl->addLayout(btn_row);

    connect(new_btn_,    &QPushButton::clicked, this, &ChatSessionPanel::on_new_clicked);
    connect(delete_btn_, &QPushButton::clicked, this, &ChatSessionPanel::on_delete_clicked);
    connect(rename_btn_, &QPushButton::clicked, this, &ChatSessionPanel::on_rename_clicked);
}

void ChatSessionPanel::refresh_sessions() {
    ChatModeService::instance().list_sessions(
        [this](bool ok, QVector<ChatSession> sessions, QString err) {
            if (!ok) {
                LOG_WARN("ChatSessionPanel", "Failed to load sessions: " + err);
                return;
            }
            sessions_ = std::move(sessions);
            populate_list(sessions_);
        });
}

void ChatSessionPanel::populate_list(const QVector<ChatSession>& sessions) {
    session_list_->clear();
    for (const auto& s : sessions) {
        auto* item = new QListWidgetItem(session_list_);
        // Show title + message count
        const QString display = s.title.isEmpty() ? "(Untitled)" : s.title;
        item->setText(display + QString("\n%1 msg").arg(s.message_count));
        item->setData(Qt::UserRole, s.uuid);
        item->setData(Qt::UserRole + 1, s.title);
        if (s.uuid == active_uuid_) {
            item->setSelected(true);
            session_list_->setCurrentItem(item);
        }
    }
    // Enable/disable action buttons
    const bool has_sel = !active_uuid_.isEmpty();
    delete_btn_->setEnabled(has_sel);
    rename_btn_->setEnabled(has_sel);
}

void ChatSessionPanel::apply_filter(const QString& text) {
    if (text.trimmed().isEmpty()) {
        populate_list(sessions_);
        return;
    }
    QVector<ChatSession> filtered;
    for (const auto& s : sessions_) {
        if (s.title.contains(text, Qt::CaseInsensitive))
            filtered.append(s);
    }
    populate_list(filtered);
}

void ChatSessionPanel::set_active_session(const QString& uuid) {
    active_uuid_ = uuid;
    // Update selection in list
    for (int i = 0; i < session_list_->count(); ++i) {
        auto* item = session_list_->item(i);
        const bool match = item->data(Qt::UserRole).toString() == uuid;
        item->setSelected(match);
        if (match) session_list_->setCurrentItem(item);
    }
    delete_btn_->setEnabled(!uuid.isEmpty());
    rename_btn_->setEnabled(!uuid.isEmpty());
}

void ChatSessionPanel::update_stats(const ChatStats& stats) {
    stats_lbl_->setText(
        QString("%1 sessions · %2 messages")
            .arg(stats.total_sessions)
            .arg(stats.total_messages));
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void ChatSessionPanel::on_item_clicked(QListWidgetItem* item) {
    if (!item) return;
    const QString uuid = item->data(Qt::UserRole).toString();
    active_uuid_ = uuid;
    delete_btn_->setEnabled(true);
    rename_btn_->setEnabled(true);
    emit session_selected(uuid);
}

void ChatSessionPanel::on_new_clicked() {
    emit new_session_requested();
}

void ChatSessionPanel::on_delete_clicked() {
    if (active_uuid_.isEmpty()) return;

    // Find title for confirmation
    QString title;
    for (const auto& s : sessions_) {
        if (s.uuid == active_uuid_) { title = s.title; break; }
    }
    const QString confirm_name = title.isEmpty() ? "(Untitled)" : title;

    if (QMessageBox::question(this, "Delete Conversation",
            QString("Delete \"%1\"? This cannot be undone.").arg(confirm_name),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    emit delete_session_requested(active_uuid_);
}

void ChatSessionPanel::on_rename_clicked() {
    if (active_uuid_.isEmpty()) return;

    QString current_title;
    for (const auto& s : sessions_) {
        if (s.uuid == active_uuid_) { current_title = s.title; break; }
    }

    bool ok = false;
    const QString new_title = QInputDialog::getText(
        this, "Rename Conversation", "New title:", QLineEdit::Normal, current_title, &ok);
    if (ok && !new_title.trimmed().isEmpty())
        emit rename_session_requested(active_uuid_, new_title.trimmed());
}

void ChatSessionPanel::on_search_changed(const QString& text) {
    apply_filter(text);
}

} // namespace fincept::chat_mode
