#include "screens/chat_mode/ChatModeScreen.h"
#include "screens/chat_mode/ChatAgentPanel.h"
#include "screens/chat_mode/ChatMessagePanel.h"
#include "screens/chat_mode/ChatModeService.h"
#include "screens/chat_mode/ChatSessionPanel.h"
#include "core/logging/Logger.h"

#include <QHBoxLayout>
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
    setStyleSheet("background:#080808;");

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    session_panel_ = new ChatSessionPanel(this);
    message_panel_ = new ChatMessagePanel(this);
    agent_panel_   = new ChatAgentPanel(this);

    hl->addWidget(session_panel_);
    hl->addWidget(message_panel_, 1);
    hl->addWidget(agent_panel_);
}

void ChatModeScreen::wire_signals() {
    auto& svc = ChatModeService::instance();

    // Session panel → screen
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

    // Message panel → screen (sends a message)
    connect(message_panel_, &ChatMessagePanel::send_requested,
            this, &ChatModeScreen::on_send_requested);

    // SSE service → message panel
    connect(&svc, &ChatModeService::stream_session_meta,
            message_panel_, &ChatMessagePanel::on_stream_session_meta);
    connect(&svc, &ChatModeService::stream_text_delta,
            message_panel_, &ChatMessagePanel::on_stream_text_delta);
    connect(&svc, &ChatModeService::stream_tool_end,
            message_panel_, &ChatMessagePanel::on_stream_tool_end);
    connect(&svc, &ChatModeService::stream_finish,
            message_panel_, &ChatMessagePanel::on_stream_finish);
    connect(&svc, &ChatModeService::stream_error,
            message_panel_, &ChatMessagePanel::on_stream_error);
    connect(&svc, &ChatModeService::stream_heartbeat,
            message_panel_, &ChatMessagePanel::on_stream_heartbeat);
    connect(&svc, &ChatModeService::insufficient_credits,
            message_panel_, &ChatMessagePanel::on_insufficient_credits);

    // SSE session-meta → refresh session list so new title appears in sidebar
    // Using `this` as context object is safe for connect() — Qt auto-disconnects on destroy
    connect(&svc, &ChatModeService::stream_session_meta,
            this, [this](const QString& /*id*/, const QString& new_title) {
                if (!new_title.isEmpty())
                    session_panel_->refresh_sessions();
            });

    // SSE finish → refresh session list (message count updated server-side)
    connect(&svc, &ChatModeService::stream_finish,
            this, [this](int) { session_panel_->refresh_sessions(); });
}

// ── Show event — load initial state ──────────────────────────────────────────

void ChatModeScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    LOG_INFO("ChatModeScreen", "Entered chat mode");
    session_panel_->refresh_sessions();

    QPointer<ChatModeScreen> self = this;

    // Load stats for sidebar
    ChatModeService::instance().get_stats(
        [self](bool ok, ChatStats stats, QString) {
            if (!self) return;
            if (ok) self->session_panel_->update_stats(stats);
        });

    // If no active session yet, restore the most recent one
    if (active_session_uuid_.isEmpty()) {
        LOG_INFO("ChatModeScreen", "No active session — loading most recent");
        ChatModeService::instance().list_sessions(
            [self](bool ok, QVector<ChatSession> sessions, QString err) {
                if (!self) return;
                if (!ok) { LOG_WARN("ChatModeScreen", "Failed to list sessions on show: " + err); return; }
                if (sessions.isEmpty()) { LOG_INFO("ChatModeScreen", "No sessions found — user has none yet"); return; }
                LOG_INFO("ChatModeScreen", QString("Auto-selecting session: %1").arg(sessions.first().uuid));
                self->on_session_selected(sessions.first().uuid);
            });
    } else {
        LOG_INFO("ChatModeScreen", QString("Re-entering with active session: %1").arg(active_session_uuid_));
    }
}

// ── Session actions ───────────────────────────────────────────────────────────

void ChatModeScreen::on_session_selected(const QString& uuid) {
    LOG_INFO("ChatModeScreen", QString("Session selected: %1").arg(uuid));
    active_session_uuid_ = uuid;
    session_panel_->set_active_session(uuid);

    // Activate on backend (fire-and-forget, no callback needed)
    ChatModeService::instance().activate_session(uuid, [](bool, QString) {});

    // Single fetch: GET /chat/sessions/{uuid} returns both session meta + messages[]
    QPointer<ChatModeScreen> self = this;
    ChatModeService::instance().get(
        "/chat/sessions/" + uuid,
        [self](bool ok, QJsonDocument doc, QString err) {
            if (!self) return;
            if (!ok) {
                LOG_WARN("ChatModeScreen", "Failed to load session: " + err);
                return;
            }
            const QJsonObject data = doc.object().value("data").toObject();

            // Update title from session object
            const QString title = data.value("session").toObject()
                                      .value("title").toString();
            self->message_panel_->set_session_title(title);

            // Load messages array
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
            if (!ok) {
                LOG_WARN("ChatModeScreen", "Failed to create session: " + err);
                return;
            }
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
            if (!ok) {
                LOG_WARN("ChatModeScreen", "Failed to delete session: " + err);
                return;
            }
            if (self->active_session_uuid_ == uuid)
                self->active_session_uuid_.clear();
            self->session_panel_->refresh_sessions();
            self->message_panel_->clear_messages();

            ChatModeService::instance().list_sessions(
                [self](bool list_ok, QVector<ChatSession> sessions, QString) {
                    if (!self) return;
                    if (!list_ok || sessions.isEmpty()) {
                        self->on_new_session();
                        return;
                    }
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
    LOG_INFO("ChatModeScreen", QString("Send requested [%1]: \"%2\"")
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
            if (!ok) {
                LOG_WARN("ChatModeScreen", "Could not create session for message: " + err);
                return;
            }
            self->active_session_uuid_ = session.uuid;
            self->session_panel_->refresh_sessions();
            self->session_panel_->set_active_session(session.uuid);
            then(session.uuid);
        });
}

} // namespace fincept::chat_mode
