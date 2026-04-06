// src/screens/agent_config/AgentChatPanel.h
#pragma once
#include "services/agents/AgentTypes.h"

#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Rich chat panel — conversational interface for interacting with configured agents.
/// Matches the AiChatScreen design: proper bubbles, typing indicator, welcome panel,
/// LLM config status in header, QTextEdit multi-line input with Shift+Enter support.
class AgentChatPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AgentChatPanel(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void build_ui();
    void setup_connections();

    // Portfolio context helpers
    QString build_portfolio_context() const;   // returns enriched context string
    void    refresh_portfolios();

    // Message area
    void add_user_bubble(const QString& text);
    void add_assistant_bubble(const QString& text, const QString& agent_name = {});
    void add_system_bubble(const QString& text);
    QTextEdit* add_streaming_bubble(const QString& agent_name = {}); // returns live QTextEdit*
    void scroll_to_bottom();
    void clear_chat();

    // State helpers
    void set_executing(bool on);
    void show_welcome(bool on);
    void show_typing(bool on);
    void update_llm_status();

    void send_message();

    // ── UI ─────────────────────────────────────────────────────────────────────
    // Header
    QLabel*      hdr_model_lbl_  = nullptr;   // active model pill
    QLabel*      hdr_status_lbl_ = nullptr;   // Ready / Streaming…
    QLabel*      hdr_agent_lbl_  = nullptr;   // selected agent badge
    QComboBox*   agent_selector_ = nullptr;
    QPushButton* route_toggle_   = nullptr;
    QPushButton* clear_btn_      = nullptr;

    // Messages
    QScrollArea*  scroll_area_       = nullptr;
    QWidget*      messages_container_ = nullptr;
    QVBoxLayout*  messages_layout_   = nullptr;
    QWidget*      welcome_panel_     = nullptr;
    QWidget*      typing_indicator_  = nullptr;
    QLabel*       typing_dots_lbl_   = nullptr;

    // Portfolio context bar
    QComboBox*   portfolio_combo_  = nullptr;
    QPushButton* analyze_btn_      = nullptr;
    QPushButton* rebalance_btn_    = nullptr;
    QPushButton* risk_btn_         = nullptr;

    // Input
    QTextEdit*   input_edit_  = nullptr;
    QPushButton* send_btn_    = nullptr;

    // Status bar
    QLabel* status_label_ = nullptr;

    // ── State ──────────────────────────────────────────────────────────────────
    bool    auto_routing_      = false;
    bool    executing_         = false;
    bool    data_loaded_       = false;
    int     typing_step_       = 0;
    QString last_query_;
    QString pending_request_id_;
    QString streaming_text_;
    QPointer<QTextEdit> streaming_bubble_widget_;

    QTimer* typing_timer_ = nullptr;
};

} // namespace fincept::screens
