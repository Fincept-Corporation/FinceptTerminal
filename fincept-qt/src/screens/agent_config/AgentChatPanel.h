// src/screens/agent_config/AgentChatPanel.h
#pragma once
#include "services/agents/AgentTypes.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// CHAT view — conversational interface for interacting with agents.
/// Routes queries through AgentService with optional auto-routing.
/// Includes portfolio context bar with quick action buttons.
class AgentChatPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AgentChatPanel(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;

  private:
    void build_ui();
    void setup_connections();
    void build_portfolio_bar(QVBoxLayout* root);

    void send_message();
    void add_user_bubble(const QString& text);
    void add_assistant_bubble(const QString& text, const QString& agent_name = {});
    void add_system_bubble(const QString& text);
    void scroll_to_bottom();
    void clear_chat();

    // UI
    QScrollArea* scroll_area_ = nullptr;
    QWidget* messages_container_ = nullptr;
    QVBoxLayout* messages_layout_ = nullptr;
    QLineEdit* input_edit_ = nullptr;
    QPushButton* send_btn_ = nullptr;
    QPushButton* clear_btn_ = nullptr;
    QPushButton* route_toggle_ = nullptr;
    QLabel* status_label_ = nullptr;

    // Portfolio context
    QWidget* portfolio_bar_ = nullptr;
    QComboBox* portfolio_combo_ = nullptr;
    QPushButton* analyze_btn_ = nullptr;
    QPushButton* rebalance_btn_ = nullptr;
    QPushButton* risk_btn_ = nullptr;

    // State
    bool auto_routing_ = false;
    bool executing_    = false;
    bool data_loaded_  = false;
    QString last_query_; // saved for routed execution
};

} // namespace fincept::screens
