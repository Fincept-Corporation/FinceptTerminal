#include "screens/chat_mode/ChatModeScreen.h"
#include "core/session/ScreenStateManager.h"
#include "screens/chat_mode/ChatAgentPanel.h"
#include "screens/chat_mode/ChatMessagePanel.h"
#include "screens/chat_mode/ChatModeService.h"
#include "screens/chat_mode/ChatSessionPanel.h"
#include "screens/chat_mode/TerminalToolBridge.h"
#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHideEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QShowEvent>

namespace fincept::chat_mode {

ChatModeScreen::ChatModeScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    wire_signals();
}

void ChatModeScreen::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    session_panel_ = new ChatSessionPanel(this);
    message_panel_ = new ChatMessagePanel(this);
    agent_panel_   = new ChatAgentPanel(this);
    tool_bridge_   = new TerminalToolBridge(this);

    hl->addWidget(session_panel_);
    hl->addWidget(message_panel_, 1);
    hl->addWidget(agent_panel_);
}

void ChatModeScreen::wire_signals() {
    auto& svc = ChatModeService::instance();

    // Session panel -> screen
    connect(session_panel_, &ChatSessionPanel::session_selected,
            this, &ChatModeScreen::on_session_selected);
    connect(session_panel_, &ChatSessionPanel::new_session_requested,
            this, &ChatModeScreen::on_new_session);
    connect(session_panel_, &ChatSessionPanel::delete_session_requested,
            this, &ChatModeScreen::on_delete_session);
    connect(session_panel_, &ChatSessionPanel::rename_session_requested,
            this, &ChatModeScreen::on_rename_session);
    connect(session_panel_, &ChatSessionPanel::exit_chat_mode_requested,
            this, &ChatModeScreen::exit_requested);

    // Message panel -> screen
    connect(message_panel_, &ChatMessagePanel::send_requested,
            this, &ChatModeScreen::on_send_requested);

    // SSE service -> message panel
    connect(&svc, &ChatModeService::stream_session_meta,
            message_panel_, &ChatMessagePanel::on_stream_session_meta);
    connect(&svc, &ChatModeService::stream_text_delta,
            message_panel_, &ChatMessagePanel::on_stream_text_delta);
    connect(&svc, &ChatModeService::stream_tool_end,
            message_panel_, &ChatMessagePanel::on_stream_tool_end);
    connect(&svc, &ChatModeService::stream_step_start,
            message_panel_, &ChatMessagePanel::on_stream_step_start);
    connect(&svc, &ChatModeService::stream_step_finish,
            message_panel_, &ChatMessagePanel::on_stream_step_finish);
    connect(&svc, &ChatModeService::stream_thinking,
            message_panel_, &ChatMessagePanel::on_stream_thinking);
    connect(&svc, &ChatModeService::stream_finish,
            message_panel_, &ChatMessagePanel::on_stream_finish);
    connect(&svc, &ChatModeService::stream_error,
            message_panel_, &ChatMessagePanel::on_stream_error);
    connect(&svc, &ChatModeService::stream_heartbeat,
            message_panel_, &ChatMessagePanel::on_stream_heartbeat);
    connect(&svc, &ChatModeService::insufficient_credits,
            message_panel_, &ChatMessagePanel::on_insufficient_credits);

    // SSE session-meta -> refresh sidebar
    connect(&svc, &ChatModeService::stream_session_meta,
            this, [this](const QString&, const QString& new_title) {
                if (!new_title.isEmpty())
                    session_panel_->refresh_sessions();
            });

    // SSE finish -> refresh sidebar + credits (delayed to let backend settle)
    connect(&svc, &ChatModeService::stream_finish,
            this, [this](int) {
                session_panel_->refresh_sessions();
                QPointer<ChatModeScreen> s = this;
                QTimer::singleShot(1500, this, [s]() {
                    if (!s) return;
                    ChatModeService::instance().get_credits(
                        [s](bool ok, int credits, QString) {
                            if (s && ok) s->message_panel_->set_credits(credits);
                        });
                });
            });

    // Tool bridge -> message panel (shows tool count in header)
    connect(tool_bridge_, &TerminalToolBridge::tools_registered,
            message_panel_, &ChatMessagePanel::on_tools_registered);
}

// ── Show/Hide — lifecycle ────────────────────────────────────────────────────

void ChatModeScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    LOG_INFO("ChatModeScreen", "Entered chat mode");
    session_panel_->refresh_sessions();

    // Start tool bridge
    tool_bridge_->start();

    QPointer<ChatModeScreen> self = this;

    // Load credits
    ChatModeService::instance().get_credits(
        [self](bool ok, int credits, QString) {
            if (self && ok) self->message_panel_->set_credits(credits);
        });

    // Load stats
    ChatModeService::instance().get_stats(
        [self](bool ok, ChatStats stats, QString) {
            if (!self) return;
            if (ok) self->session_panel_->update_stats(stats);
        });

    // Restore most recent session
    if (active_session_uuid_.isEmpty()) {
        LOG_INFO("ChatModeScreen", "No active session -- loading most recent");
        ChatModeService::instance().list_sessions(
            [self](bool ok, QVector<ChatSession> sessions, QString err) {
                if (!self) return;
                if (!ok) { LOG_WARN("ChatModeScreen", "List sessions failed: " + err); return; }
                if (sessions.isEmpty()) return;
                self->on_session_selected(sessions.first().uuid);
            });
    }
}

void ChatModeScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    tool_bridge_->stop();
    LOG_INFO("ChatModeScreen", "Exited chat mode");
}

// ── Session actions ───────────────────────────────────────────────────────────

void ChatModeScreen::on_session_selected(const QString& uuid) {
    LOG_INFO("ChatModeScreen", QString("Session selected: %1").arg(uuid));
    active_session_uuid_ = uuid;
    session_panel_->set_active_session(uuid);
    fincept::ScreenStateManager::instance().notify_changed(this);

    ChatModeService::instance().activate_session(uuid, [](bool, QString) {});

    QPointer<ChatModeScreen> self = this;
    ChatModeService::instance().get(
        "/chat/sessions/" + uuid,
        [self](bool ok, QJsonDocument doc, QString err) {
            if (!self) return;
            if (!ok) { LOG_WARN("ChatModeScreen", "Load session failed: " + err); return; }
            const QJsonObject data = doc.object().value("data").toObject();

            const QString title = data.value("session").toObject()
                                      .value("title").toString();
            self->message_panel_->set_session_title(title);

            const QJsonArray msgs_arr = data.value("messages").toArray();
            QVector<ChatMessage> messages;
            messages.reserve(msgs_arr.size());
            for (const auto& v : msgs_arr)
                messages.append(ChatMessage::from_json(v.toObject()));
            self->message_panel_->load_messages(messages);
        });
}

void ChatModeScreen::on_new_session() {
    QPointer<ChatModeScreen> self = this;
    ChatModeService::instance().create_session(
        "New Conversation",
        [self](bool ok, ChatSession session, QString err) {
            if (!self) return;
            if (!ok) { LOG_WARN("ChatModeScreen", "Create session failed: " + err); return; }
            self->message_panel_->clear_messages();
            self->session_panel_->refresh_sessions();
            self->on_session_selected(session.uuid);
        });
}

void ChatModeScreen::on_delete_session(const QString& uuid) {
    QPointer<ChatModeScreen> self = this;
    ChatModeService::instance().delete_session(
        uuid, [self, uuid](bool ok, QString err) {
            if (!self) return;
            if (!ok) { LOG_WARN("ChatModeScreen", "Delete failed: " + err); return; }
            if (self->active_session_uuid_ == uuid)
                self->active_session_uuid_.clear();
            self->session_panel_->refresh_sessions();
            self->message_panel_->clear_messages();

            ChatModeService::instance().list_sessions(
                [self](bool list_ok, QVector<ChatSession> sessions, QString) {
                    if (!self) return;
                    if (!list_ok || sessions.isEmpty()) { self->on_new_session(); return; }
                    self->on_session_selected(sessions.first().uuid);
                });
        });
}

void ChatModeScreen::on_rename_session(const QString& uuid, const QString& title) {
    QPointer<ChatModeScreen> self = this;
    ChatModeService::instance().rename_session(
        uuid, title, [self](bool ok, QString err) {
            if (!self) return;
            if (!ok) LOG_WARN("ChatModeScreen", "Rename failed: " + err);
            self->session_panel_->refresh_sessions();
        });
}

// ── Send message ──────────────────────────────────────────────────────────────

void ChatModeScreen::on_send_requested(const QString& message, StreamMode mode) {
    LOG_INFO("ChatModeScreen", QString("Send [%1]: \"%2\"")
                 .arg(mode == StreamMode::Deep ? "deep" : "lite")
                 .arg(message.left(60)));
    ensure_active_session([this, message, mode](const QString& uuid) {
        ChatModeService::instance().stream_message(message, uuid, mode);
    });
}

void ChatModeScreen::ensure_active_session(std::function<void(const QString&)> then) {
    if (!active_session_uuid_.isEmpty()) {
        then(active_session_uuid_);
        return;
    }
    QPointer<ChatModeScreen> self = this;
    ChatModeService::instance().create_session(
        "New Conversation",
        [self, then = std::move(then)](bool ok, ChatSession session, QString err) {
            if (!self) return;
            if (!ok) { LOG_WARN("ChatModeScreen", "Auto-create session failed: " + err); return; }
            self->active_session_uuid_ = session.uuid;
            self->session_panel_->refresh_sessions();
            self->session_panel_->set_active_session(session.uuid);
            then(session.uuid);
        });
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap ChatModeScreen::save_state() const {
    return {{"session_uuid", active_session_uuid_}};
}

void ChatModeScreen::restore_state(const QVariantMap& state) {
    const QString uuid = state.value("session_uuid").toString();
    if (!uuid.isEmpty() && uuid != active_session_uuid_)
        on_session_selected(uuid);
}

} // namespace fincept::chat_mode
