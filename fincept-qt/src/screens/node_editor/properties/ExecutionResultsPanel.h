#pragma once

#include "screens/node_editor/NodeEditorTypes.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::workflow {

/// Bottom drawer showing execution results per node.
class ExecutionResultsPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ExecutionResultsPanel(QWidget* parent = nullptr);

    void clear();
    void set_started(const QString& workflow_id);
    void add_node_result(const NodeExecutionResult& result);
    void set_finished(const WorkflowExecutionResult& result);

  private:
    void build_ui();

    QLabel* status_label_ = nullptr;
    QVBoxLayout* results_layout_ = nullptr;
    QWidget* results_container_ = nullptr;
};

} // namespace fincept::workflow
