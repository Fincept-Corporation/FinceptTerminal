#pragma once

#include "screens/node_editor/NodeEditorTypes.h"

#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::workflow {

/// Bottom drawer showing per-node execution results with collapsible UI.
class ExecutionResultsPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ExecutionResultsPanel(QWidget* parent = nullptr);

    void clear();
    void set_started(const QString& workflow_id);
    void add_node_result(const NodeExecutionResult& result);
    void set_finished(const WorkflowExecutionResult& result);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void set_collapsed(bool collapsed);
    void retranslateUi();
    void refresh_status_label(); ///< Re-apply status text for the current run state.
    void refresh_node_counter(); ///< Re-apply the node/error counter text.

    // Tracks what the status label should currently show so retranslateUi
    // can re-render it in the new language.
    enum class RunState { Idle, Running, Done, Stopped, Failed };
    RunState run_state_ = RunState::Idle;
    int last_total_ms_ = 0;
    QString last_failure_;

    QLabel* status_label_ = nullptr;
    QLabel* node_counter_ = nullptr;
    QLabel* title_label_ = nullptr;
    QPushButton* collapse_btn_ = nullptr;
    QPushButton* clear_btn_ = nullptr;
    QPushButton* copy_btn_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    QWidget* results_container_ = nullptr;
    QVBoxLayout* results_layout_ = nullptr;
    bool collapsed_ = false;
    int node_count_ = 0;
    int error_count_ = 0;
    QString copy_buffer_;
};

} // namespace fincept::workflow
