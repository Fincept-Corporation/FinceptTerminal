#include "screens/chat_mode/ChatSessionPanel.h"
#include "screens/chat_mode/ChatModeService.h"
#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace fincept::chat_mode {

ChatSessionPanel::ChatSessionPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    refresh_sessions();
}

void ChatSessionPanel::build_ui() {
    setFixedWidth(240);
    setStyleSheet(QString("background:%1;border-right:1px solid %2;")
        .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    // Header row
    auto* header_row = new QHBoxLayout;
    header_row->setSpacing(4);
    auto* title_lbl = new QLabel("CONVERSATIONS");
    title_lbl->setStyleSheet(
        QString("color:%1;font-size:11px;font-weight:700;"
                "font-family:'Consolas','Courier New',monospace;"
                "background:transparent;letter-spacing:1px;")
            .arg(ui::colors::AMBER));
    header_row->addWidget(title_lbl);
    header_row->addStretch();

    exit_btn_ = new QPushButton("TERMINAL");
    exit_btn_->setFixedHeight(20);
    exit_btn_->setCursor(Qt::PointingHandCursor);
    exit_btn_->setToolTip("Switch to Terminal Mode (F9)");
    exit_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    exit_btn_->setStyleSheet(QString(
        "QPushButton{background:transparent;color:%1;border:1px solid %2;"
        "padding:0 8px;font-weight:700;font-size:10px;"
        "font-family:'Consolas','Courier New',monospace;}"
        "QPushButton:hover{background:%2;color:%3;border-color:%1;}")
        .arg(ui::colors::AMBER, ui::colors::AMBER_DIM, ui::colors::TEXT_PRIMARY));
    connect(exit_btn_, &QPushButton::clicked, this, &ChatSessionPanel::exit_chat_mode_requested);
    header_row->addWidget(exit_btn_);
    vl->addLayout(header_row);

    // Search
    search_edit_ = new QLineEdit;
    search_edit_->setPlaceholderText("Search...");
    search_edit_->setFixedHeight(28);
    search_edit_->setStyleSheet(QString(
        "QLineEdit{background:%1;color:%2;border:1px solid %3;"
        "border-radius:0px;padding:4px 8px;font-size:12px;"
        "font-family:'Consolas','Courier New',monospace;}"
        "QLineEdit:focus{border:1px solid %4;}")
        .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY,
             ui::colors::BORDER_DIM, ui::colors::BORDER_BRIGHT));
    connect(search_edit_, &QLineEdit::textChanged, this, &ChatSessionPanel::on_search_changed);
    vl->addWidget(search_edit_);

    // Debounce timer for server-side search
    search_timer_ = new QTimer(this);
    search_timer_->setSingleShot(true);
    search_timer_->setInterval(400);
    connect(search_timer_, &QTimer::timeout, this, &ChatSessionPanel::on_search_server);

    // Session list
    session_list_ = new QListWidget;
    session_list_->setStyleSheet(QString(
        "QListWidget{background:%1;border:none;color:%2;"
        "font-size:12px;font-family:'Consolas','Courier New',monospace;outline:none;}"
        "QListWidget::item{padding:6px 6px;border-bottom:1px solid %3;}"
        "QListWidget::item:selected{background:%3;color:%4;"
        "border-left:2px solid %4;}"
        "QListWidget::item:hover:!selected{background:%5;}"
        "QScrollBar:vertical{background:transparent;width:4px;}"
        "QScrollBar::handle:vertical{background:%6;border-radius:0px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
        .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY, ui::colors::BG_RAISED,
             ui::colors::AMBER, ui::colors::BG_HOVER, ui::colors::BORDER_MED));
    session_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(session_list_, &QListWidget::itemClicked,
            this, &ChatSessionPanel::on_item_clicked);
    vl->addWidget(session_list_, 1);

    // Stats label
    stats_lbl_ = new QLabel;
    stats_lbl_->setStyleSheet(
        QString("color:%1;font-size:11px;font-family:'Consolas','Courier New',monospace;"
                "background:transparent;")
            .arg(ui::colors::TEXT_DIM));
    stats_lbl_->setAlignment(Qt::AlignCenter);
    vl->addWidget(stats_lbl_);

    // Action buttons
    auto mk_btn = [](const QString& text, const QString& tip) -> QPushButton* {
        auto* btn = new QPushButton(text);
        btn->setToolTip(tip);
        btn->setFixedHeight(24);
        btn->setStyleSheet(QString(
            "QPushButton{background:%1;color:%2;border:1px solid %3;"
            "border-radius:0px;font-size:11px;"
            "font-family:'Consolas','Courier New',monospace;}"
            "QPushButton:hover{background:%4;color:%5;border-color:%6;}"
            "QPushButton:disabled{color:%7;border-color:%1;}")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_SECONDARY, ui::colors::BORDER_DIM,
                 ui::colors::BG_HOVER, ui::colors::TEXT_PRIMARY, ui::colors::AMBER,
                 ui::colors::BORDER_BRIGHT));
        return btn;
    };

    // Row 1: New + Rename
    auto* row1 = new QHBoxLayout;
    row1->setSpacing(4);
    new_btn_    = mk_btn("+ New",   "New conversation");
    rename_btn_ = mk_btn("Rename",  "Rename selected");
    rename_btn_->setEnabled(false);
    row1->addWidget(new_btn_);
    row1->addWidget(rename_btn_);
    vl->addLayout(row1);

    // Row 2: Delete + Export
    auto* row2 = new QHBoxLayout;
    row2->setSpacing(4);
    delete_btn_ = mk_btn("Delete",  "Delete selected");
    export_btn_ = mk_btn("Export",  "Export conversations");
    delete_btn_->setEnabled(false);
    row2->addWidget(delete_btn_);
    row2->addWidget(export_btn_);
    vl->addLayout(row2);

    connect(new_btn_,    &QPushButton::clicked, this, &ChatSessionPanel::on_new_clicked);
    connect(delete_btn_, &QPushButton::clicked, this, &ChatSessionPanel::on_delete_clicked);
    connect(rename_btn_, &QPushButton::clicked, this, &ChatSessionPanel::on_rename_clicked);
    connect(export_btn_, &QPushButton::clicked, this, &ChatSessionPanel::on_export_clicked);
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
        const QString display = s.title.isEmpty() ? "(Untitled)" : s.title;
        item->setText(display + QString("\n%1 msg").arg(s.message_count));
        item->setData(Qt::UserRole, s.uuid);
        item->setData(Qt::UserRole + 1, s.title);
        if (s.uuid == active_uuid_) {
            item->setSelected(true);
            session_list_->setCurrentItem(item);
        }
    }
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
        QString("%1 sessions | %2 messages")
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

    QString title;
    for (const auto& s : sessions_) {
        if (s.uuid == active_uuid_) { title = s.title; break; }
    }
    const QString confirm_name = title.isEmpty() ? "(Untitled)" : title;

    if (QMessageBox::question(this, "Delete Conversation",
            QString("Delete \"%1\"?").arg(confirm_name),
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

void ChatSessionPanel::on_export_clicked() {
    // Export all sessions (or selected)
    QStringList uuids;
    for (const auto& s : sessions_)
        uuids.append(s.uuid);

    if (uuids.isEmpty()) {
        QMessageBox::information(this, "Export", "No conversations to export.");
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, "Export Conversations", "fincept_chat_export.json",
        "JSON (*.json)");
    if (path.isEmpty()) return;

    export_btn_->setEnabled(false);
    export_btn_->setText("...");

    ChatModeService::instance().export_sessions(
        uuids,
        [this, path](bool ok, QJsonArray data, QString err) {
            export_btn_->setEnabled(true);
            export_btn_->setText("Export");
            if (!ok) {
                QMessageBox::warning(this, "Export Failed", err);
                return;
            }
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QMessageBox::warning(this, "Export Failed", "Could not write file.");
                return;
            }
            file.write(QJsonDocument(data).toJson(QJsonDocument::Indented));
            file.close();
            QMessageBox::information(this, "Export",
                QString("Exported %1 conversations.").arg(data.size()));
        });
}

void ChatSessionPanel::on_search_changed(const QString& text) {
    // Local filter immediately
    apply_filter(text);
    // Debounce server-side search for better results
    if (text.trimmed().length() >= 2)
        search_timer_->start();
    else
        search_timer_->stop();
}

void ChatSessionPanel::on_search_server() {
    const QString query = search_edit_->text().trimmed();
    if (query.length() < 2) return;

    ChatModeService::instance().search_messages(
        query,
        [this](bool ok, QVector<ChatMessage> results, QString) {
            if (!ok || results.isEmpty()) return;
            // Highlight sessions that have matching messages
            for (int i = 0; i < session_list_->count(); ++i) {
                auto* item = session_list_->item(i);
                bool has_match = false;
                for (const auto& r : results) {
                    // Check if this message's content matches any session
                    if (item->text().contains(r.content.left(20), Qt::CaseInsensitive)) {
                        has_match = true;
                        break;
                    }
                }
                if (has_match && !item->isSelected()) {
                    item->setForeground(QColor(ui::colors::AMBER()));
                }
            }
        });
}

} // namespace fincept::chat_mode
