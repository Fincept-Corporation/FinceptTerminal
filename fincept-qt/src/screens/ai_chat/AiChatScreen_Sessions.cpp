// src/screens/ai_chat/AiChatScreen_Sessions.cpp
//
// Session list / sidebar interactions: load_sessions, create_new_session,
// load_messages, on_new/select/rename/delete/attach_file, sidebar collapse,
// show_welcome.
//
// Part of the partial-class split of AiChatScreen.cpp.

#include "screens/ai_chat/AiChatScreen.h"

#include "screens/ai_chat/ChatBubbleFactory.h"
#include "services/llm/LlmService.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "mcp/McpService.h"
#include "storage/repositories/ChatRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPalette>
#include <QPointer>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QtConcurrent/QtConcurrent>

#include <cmath>
#include <memory>

namespace fincept::screens {

namespace fnt = fincept::ui::fonts;
namespace col = fincept::ui::colors;

// File-local logging tag. Renamed to avoid a unity-build collision with the
// anonymous-namespace `TAG` in AiChatScreen.cpp — two `namespace {}` blocks
// in the same TU produce two distinct anonymous namespaces, so an unqualified
// `TAG` reference becomes ambiguous when the unity bucket merges both.
static constexpr const char* TAG_SESSIONS = "AiChatScreen";

static QString generate_session_title() {
    static const QStringList prefixes = {"Amber", "Apex", "Atlas", "Echo", "Flux", "Nova", "Slate", "Vector"};
    static const QStringList nouns = {"Brief", "Drift", "Focus", "Ledger", "Macro", "Pulse", "Signal", "Tape"};
    const int pi = QRandomGenerator::global()->bounded(prefixes.size());
    const int ni = QRandomGenerator::global()->bounded(nouns.size());
    const QString sfx =
        QString::number(QRandomGenerator::global()->bounded(0x10000), 16).rightJustified(4, '0').toUpper();
    return QString("%1 %2 %3").arg(prefixes[pi], nouns[ni], sfx);
}

static QString display_session_title(const ChatSession& s) {
    if (!s.title.trimmed().isEmpty() && s.title.trimmed().toLower() != "chat")
        return s.title;
    return QString("Session %1").arg(s.id.left(4).toUpper());
}

static QString display_session_meta(const ChatSession& s) {
    QDateTime dt = QDateTime::fromString(s.updated_at, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(s.updated_at, "yyyy-MM-dd HH:mm:ss");
    const QString stamp = dt.isValid() ? dt.toString("MMM d · hh:mm") : "";
    return s.message_count > 0
               ? QString("%1 msg%2%3").arg(s.message_count).arg(stamp.isEmpty() ? "" : "  ·  ").arg(stamp)
               : stamp;
}

void AiChatScreen::on_toggle_sidebar() {
    apply_sidebar_collapsed(!sidebar_collapsed_, /*animate=*/true);
    ScreenStateManager::instance().notify_changed(this);
}

void AiChatScreen::apply_sidebar_collapsed(bool collapsed, bool animate) {
    sidebar_collapsed_ = collapsed;
    if (!sidebar_)
        return;

    const int target = collapsed ? 0 : kSidebarExpandedWidth;

    // Lazily create the animation so the helper works even before the first
    // user toggle (e.g. when restore_state runs before the screen is shown).
    if (!sidebar_anim_) {
        sidebar_anim_ = new QPropertyAnimation(sidebar_, "maximumWidth", this);
        sidebar_anim_->setDuration(180);
        sidebar_anim_->setEasingCurve(QEasingCurve::OutCubic);
    }

    sidebar_anim_->stop();
    if (animate) {
        sidebar_anim_->setStartValue(sidebar_->maximumWidth());
        sidebar_anim_->setEndValue(target);
        sidebar_anim_->start();
    } else {
        sidebar_->setMaximumWidth(target);
    }

    if (sidebar_toggle_btn_) {
        sidebar_toggle_btn_->setText(collapsed ? "›" : "‹");
        sidebar_toggle_btn_->setToolTip(collapsed ? "Expand sidebar  (Ctrl+B)" : "Collapse sidebar  (Ctrl+B)");
    }
}

void AiChatScreen::on_search_changed(const QString& text) {
    const QString filter = text.trimmed().toLower();
    for (int i = 0; i < session_list_->count(); ++i) {
        auto* item = session_list_->item(i);
        item->setHidden(!filter.isEmpty() && !item->text().toLower().contains(filter));
    }
    ScreenStateManager::instance().notify_changed(this);
}

// ── Typing indicator slot ─────────────────────────────────────────────────────


void AiChatScreen::load_sessions() {
    session_list_->blockSignals(true);
    session_list_->clear();
    auto result = ChatRepository::instance().list_sessions();
    if (result.is_ok()) {
        for (const auto& s : result.value()) {
            const QString meta = display_session_meta(s);
            const QString title = display_session_title(s);
            const QString label = title + (meta.isEmpty() ? "" : "\n" + meta);
            auto* item = new QListWidgetItem(label);
            item->setData(Qt::UserRole, s.id);
            item->setToolTip(title + (meta.isEmpty() ? "" : "\n" + meta));
            item->setSizeHint(QSize(0, 52));
            session_list_->addItem(item);
        }
    }
    session_list_->blockSignals(false);

    if (!active_session_id_.isEmpty()) {
        for (int i = 0; i < session_list_->count(); ++i) {
            if (session_list_->item(i)->data(Qt::UserRole).toString() == active_session_id_) {
                session_list_->setCurrentRow(i);
                return;
            }
        }
    }
    if (session_list_->count() > 0)
        session_list_->setCurrentRow(0);
    else
        create_new_session();
}

void AiChatScreen::create_new_session() {
    if (streaming_)
        return;
    const QString title = generate_session_title();
    auto result = ChatRepository::instance().create_session(title, ai_chat::LlmService::instance().active_provider(),
                                                            ai_chat::LlmService::instance().active_model());
    if (result.is_err()) {
        LOG_ERROR(TAG_SESSIONS, "create_new_session failed: " + QString::fromStdString(result.error()));
        return;
    }
    active_session_id_ = result.value().id;
    active_session_title_ = result.value().title;
    history_.clear();
    total_tokens_ = 0;
    total_messages_ = 0;
    clear_messages();
    load_sessions();
    hdr_session_lbl_->setText(active_session_title_);
    delete_btn_->setEnabled(true);
    rename_btn_->setEnabled(true);
    update_stats();
    show_welcome(true);
}

void AiChatScreen::load_messages(const QString& session_id) {
    clear_messages();
    history_.clear();
    auto result = ChatRepository::instance().get_messages(session_id);
    if (result.is_err())
        return;
    const auto& msgs = result.value();
    total_messages_ = static_cast<int>(msgs.size());
    total_tokens_ = 0;
    for (const auto& msg : msgs) {
        add_message_bubble(msg.role, msg.content, msg.timestamp);
        if (msg.role != "system")
            history_.push_back({msg.role, msg.content});
        total_tokens_ += msg.tokens_used;
    }
    show_welcome(msgs.empty());
    scroll_to_bottom();
    update_stats();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void AiChatScreen::on_new_session() {
    create_new_session();
}

void AiChatScreen::on_session_selected(int row) {
    if (streaming_ || row < 0 || row >= session_list_->count())
        return;
    auto* item = session_list_->item(row);
    active_session_id_ = item->data(Qt::UserRole).toString();
    active_session_title_ = item->text().split('\n').first();
    delete_btn_->setEnabled(true);
    rename_btn_->setEnabled(true);
    hdr_session_lbl_->setText(active_session_title_);
    load_messages(active_session_id_);
    ScreenStateManager::instance().notify_changed(this);
}

void AiChatScreen::on_rename_session() {
    if (active_session_id_.isEmpty())
        return;
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, "Rename Session", "Session name:", QLineEdit::Normal, active_session_title_, &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    ChatRepository::instance().update_session_title(active_session_id_, name.trimmed());
    active_session_title_ = name.trimmed();
    hdr_session_lbl_->setText(active_session_title_);
    load_sessions();
}

void AiChatScreen::on_delete_session() {
    if (streaming_ || active_session_id_.isEmpty())
        return;
    ChatRepository::instance().delete_session(active_session_id_);
    active_session_id_.clear();
    active_session_title_.clear();
    history_.clear();
    clear_messages();
    delete_btn_->setEnabled(false);
    rename_btn_->setEnabled(false);
    load_sessions();
}

void AiChatScreen::on_attach_file() {
    // Let user pick from File Manager index or browse disk
    QStringList paths = QFileDialog::getOpenFileNames(
        this, "Attach File to Message", QString(),
        "All Files (*);;Text Files (*.txt *.md *.csv *.json);;Notebooks (*.ipynb);;PDF (*.pdf)");
    if (paths.isEmpty())
        return;

    attached_file_path_ = paths.first(); // single attach for now
    QFileInfo fi(attached_file_path_);
    if (attach_badge_) {
        attach_badge_->setText("⊕ " + fi.fileName());
        attach_badge_->setVisible(true);
    }
    if (attach_btn_)
        attach_btn_->setProperty("active", true);
    ScreenStateManager::instance().notify_changed(this);
}


void AiChatScreen::show_welcome(bool show) {
    if (welcome_panel_)
        welcome_panel_->setVisible(show);
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

} // namespace fincept::screens
