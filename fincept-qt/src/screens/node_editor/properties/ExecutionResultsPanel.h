#pragma once

#include "screens/node_editor/NodeEditorTypes.h"

#include <QEvent>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::workflow {

/// Bottom drawer: the run log. One row per node — RUNNING while in flight, then
/// OK / FAILED / SKIPPED with its duration and output — plus the run's mode banner
/// and totals.
class ExecutionResultsPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ExecutionResultsPanel(QWidget* parent = nullptr);

    void clear();
    void set_started(const QString& workflow_id);
    /// One-line banner at the top of the log (execution mode, simulated live nodes...).
    void set_mode_banner(const QString& text, bool live);
    /// A node was launched — show it as RUNNING until its result arrives.
    void add_node_running(const QString& node_id, const QString& node_label);
    /// `node_label` is the node's display name; when empty the card falls back to a short id.
    void add_node_result(const NodeExecutionResult& result, const QString& node_label = {});
    void set_finished(const WorkflowExecutionResult& result);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void set_collapsed(bool collapsed);
    void retranslateUi();
    void refresh_status_label(); ///< Re-apply status text for the current run state.
    void refresh_node_counter(); ///< Re-apply the node/error counter text.
    void insert_card(QWidget* card, int pos = -1);
    void scroll_to_bottom();

    // Tracks what the status label should currently show so retranslateUi
    // can re-render it in the new language.
    enum class RunState { Idle, Running, Done, Stopped, Failed };
    RunState run_state_ = RunState::Idle;
    int last_total_ms_ = 0;
    QString last_failure_;

    QLabel* status_label_ = nullptr;
    QLabel* node_counter_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* mode_banner_ = nullptr;
    QPushButton* collapse_btn_ = nullptr;
    QPushButton* clear_btn_ = nullptr;
    QPushButton* copy_btn_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    QWidget* results_container_ = nullptr;
    QVBoxLayout* results_layout_ = nullptr;
    QHash<QString, QWidget*> running_cards_; // node id -> its RUNNING placeholder
    bool collapsed_ = false;
    int node_count_ = 0;
    int error_count_ = 0;
    int skipped_count_ = 0;
    QString copy_buffer_;
};

} // namespace fincept::workflow
