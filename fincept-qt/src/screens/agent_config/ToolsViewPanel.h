// src/screens/agent_config/ToolsViewPanel.h
#pragma once
#include "services/agents/AgentTypes.h"

#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QTextEdit>
#include <QTreeWidget>
#include <QWidget>

namespace fincept::screens {

/// TOOLS view — 3-column terminal layout.
/// Left:   Assign-to context + selected tools list.
/// Center: Categorized tool browser with search.
/// Right:  Tool detail panel (description, params, used-by).
class ToolsViewPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ToolsViewPanel(QWidget* parent = nullptr);

  signals:
    void tools_selection_changed(QStringList selected_tools);
    /// Emitted after tools are successfully saved to an agent/team config.
    /// Carries the target agent/team ID so other panels can reload.
    void tool_assigned(const QString& target_id, const QStringList& tools);

  protected:
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    // ── Build ────────────────────────────────────────────────────────────────
    void build_ui();
    QWidget* build_left_panel();
    QWidget* build_center_panel();
    QWidget* build_right_panel();
    void setup_connections();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    // ── Data ─────────────────────────────────────────────────────────────────
    void populate_tools(const services::AgentToolsInfo& info);
    void filter_tools(const QString& query);
    void load_assign_targets();
    void refresh_used_by(const QString& tool_name);
    void show_tool_detail(const QString& tool_name);

    // ── Selection ────────────────────────────────────────────────────────────
    void add_tool(const QString& tool_name);
    void remove_tool(const QString& tool_name);
    void clear_selection();
    void assign_to_target();
    void update_assigned_dots();

    // ── Helpers ──────────────────────────────────────────────────────────────
    QString current_target_name() const;
    QStringList tools_of_target() const;
    void copy_tool_name(const QString& name);

    // ── LEFT panel widgets ───────────────────────────────────────────────────
    QLabel* assign_to_hdr_ = nullptr;
    QRadioButton* radio_agent_ = nullptr;
    QRadioButton* radio_team_ = nullptr;
    QComboBox* target_combo_ = nullptr;
    QLabel* target_status_ = nullptr;
    QLabel* selected_tools_hdr_ = nullptr;
    QListWidget* selected_list_ = nullptr;
    QLabel* selected_count_ = nullptr;
    QPushButton* remove_btn_ = nullptr;
    QPushButton* clear_btn_ = nullptr;
    QPushButton* assign_btn_ = nullptr;

    // ── CENTER panel widgets ─────────────────────────────────────────────────
    QLabel* available_hdr_ = nullptr;
    QLabel* assigned_hint_ = nullptr;
    QLineEdit* search_edit_ = nullptr;
    QTreeWidget* tool_tree_ = nullptr;
    QLabel* total_count_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QPushButton* copy_btn_ = nullptr;

    // ── RIGHT panel widgets ──────────────────────────────────────────────────
    QLabel* detail_hdr_ = nullptr;
    QLabel* description_hdr_ = nullptr;
    QLabel* parameters_hdr_ = nullptr;
    QLabel* used_by_hdr_ = nullptr;
    QLabel* detail_name_ = nullptr;
    QLabel* detail_category_ = nullptr;
    QTextEdit* detail_desc_ = nullptr;
    QTextEdit* detail_params_ = nullptr;
    QTextEdit* detail_used_by_ = nullptr;

    // ── State ────────────────────────────────────────────────────────────────
    QStringList selected_tools_;
    services::AgentToolsInfo all_tools_;
    QVector<services::AgentInfo> agent_configs_;
    bool data_loaded_ = false;
    QString current_tool_; // tool shown in detail panel
};

} // namespace fincept::screens
