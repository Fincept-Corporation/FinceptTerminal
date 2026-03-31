#pragma once

#include "screens/node_editor/NodeEditorTypes.h"

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

  private:
    void build_ui();
    void set_collapsed(bool collapsed);

    QLabel*      status_label_      = nullptr;
    QLabel*      node_counter_      = nullptr;
    QPushButton* collapse_btn_      = nullptr;
    QPushButton* clear_btn_         = nullptr;
    QPushButton* copy_btn_          = nullptr;
    QScrollArea* scroll_area_       = nullptr;
    QWidget*     results_container_ = nullptr;
    QVBoxLayout* results_layout_    = nullptr;
    bool         collapsed_         = false;
    int          node_count_        = 0;
    int          error_count_       = 0;
    QString      copy_buffer_;
};

} // namespace fincept::workflow
