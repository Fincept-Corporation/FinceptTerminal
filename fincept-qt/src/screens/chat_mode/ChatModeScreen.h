#pragma once
#include "screens/chat_mode/ChatModeTypes.h"

#include <QWidget>
#include <functional>

namespace fincept::chat_mode {

class ChatSessionPanel;
class ChatMessagePanel;
class ChatAgentPanel;

/// Full-screen chat mode — replaces the entire terminal UI.
/// Layout: [ChatSessionPanel | ChatMessagePanel | ChatAgentPanel]
class ChatModeScreen : public QWidget {
    Q_OBJECT
  public:
    explicit ChatModeScreen(QWidget* parent = nullptr);

  signals:
    void exit_requested();

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    ChatSessionPanel* session_panel_  = nullptr;
    ChatMessagePanel* message_panel_  = nullptr;
    ChatAgentPanel*   agent_panel_    = nullptr;

    QString active_session_uuid_;

    void build_ui();
    void wire_signals();

    void on_session_selected(const QString& uuid);
    void on_new_session();
    void on_delete_session(const QString& uuid);
    void on_rename_session(const QString& uuid, const QString& title);
    void on_send_requested(const QString& message, StreamMode mode);
    void ensure_active_session(std::function<void(const QString& uuid)> then);
};

} // namespace fincept::chat_mode
