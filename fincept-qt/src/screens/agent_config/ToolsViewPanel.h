// src/screens/agent_config/ToolsViewPanel.h
#pragma once
#include "services/agents/AgentTypes.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTreeWidget>
#include <QWidget>

namespace fincept::screens {

/// TOOLS view — browse and select available agent tools.
/// Left panel: selected tools list. Center panel: categorized tool browser.
class ToolsViewPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ToolsViewPanel(QWidget* parent = nullptr);

  signals:
    /// Emitted when the selected tools list changes.
    void tools_selection_changed(QStringList selected_tools);

  protected:
    void showEvent(QShowEvent* event) override;

  private:
    void build_ui();
    QWidget* build_selected_panel();
    QWidget* build_browser_panel();
    void setup_connections();

    void populate_tools(const services::AgentToolsInfo& info);
    void add_tool(const QString& tool_name);
    void remove_tool(const QString& tool_name);
    void filter_tools(const QString& query);
    void copy_tool_name(const QString& tool_name);

    // Left panel
    QListWidget* selected_list_ = nullptr;
    QLabel* selected_count_ = nullptr;

    // Browser panel
    QTreeWidget* tool_tree_ = nullptr;
    QLineEdit* search_edit_ = nullptr;
    QLabel* total_count_ = nullptr;
    QPushButton* copy_btn_ = nullptr;

    // State
    QStringList selected_tools_;
    services::AgentToolsInfo all_tools_;
    bool data_loaded_ = false;
};

} // namespace fincept::screens
