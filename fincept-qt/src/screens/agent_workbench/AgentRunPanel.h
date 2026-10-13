#pragma once
// AgentRunPanel.h — RUN tab of the Agent Workbench: the run composer (left) and
// the live, virtualized transcript of the selected run (right) with STOP /
// FOLLOW UP / OPEN FOLDER / VIEW + COPY RESULT / EXPORT.
//
// Lifecycle: subscribes to CodingAgentService in showEvent and disconnects in
// hideEvent; transcript appends are coalesced (≤ 10 repaints/s) and the elapsed
// ticker only runs while visible and the selected run is running.

#include "services/coding_agents/CodingAgentTypes.h"

#include <QList>
#include <QMetaObject>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTimer;

namespace fincept::screens {

class AgentPlaceholderHighlighter;
class AgentTranscriptModel;
class AgentTranscriptView;

class AgentRunPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AgentRunPanel(QWidget* parent = nullptr);

    /// Select a run for the transcript pane (empty = none).
    void show_run(const QString& run_id);
    QString current_run_id() const { return run_id_; }
    /// Put a prompt into the composer (e.g. from a schedule) without running it.
    void set_composer_prompt(const QString& prompt);

    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

  signals:
    void run_started(const QString& run_id);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    QWidget* build_composer();
    QWidget* build_transcript_pane();
    void retranslate();

    void subscribe();
    void unsubscribe();

    // Composer
    services::coding_agents::AgentKind selected_agent() const;
    void on_agent_changed();
    void refresh_cli_status();
    void refresh_hint();
    void on_template_activated(int index);
    void select_first_placeholder();
    void on_browse_folder();
    void on_run();

    // Transcript
    void refresh_run_picker();
    void update_picker_item(const QString& run_id);
    void on_picker_activated(int index);
    void on_run_event(const QString& run_id, const services::coding_agents::AgentEvent& event);
    void on_run_updated(const QString& run_id);
    void on_runs_reset();
    void resync_transcript();
    void flush_pending();
    void refresh_header();
    void on_stop();
    void on_follow_up();
    void on_open_folder();
    void on_view_result();
    void on_copy_result();
    void on_export();
    void write_transcript_file(const QString& run_id, const QString& path,
                               const QVector<services::coding_agents::AgentEvent>& events);

    // ── Composer widgets ─────────────────────────────────────────────────────
    QLabel* composer_title_ = nullptr;
    QLabel* agent_lbl_ = nullptr;
    QPushButton* claude_btn_ = nullptr;
    QPushButton* codex_btn_ = nullptr;
    QLabel* agent_status_ = nullptr;
    QLabel* model_lbl_ = nullptr;
    QLineEdit* model_edit_ = nullptr;
    QLabel* mode_lbl_ = nullptr;
    QComboBox* mode_combo_ = nullptr;
    QCheckBox* tools_check_ = nullptr;
    QLabel* budget_lbl_ = nullptr;
    QDoubleSpinBox* budget_spin_ = nullptr;
    QLabel* budget_hint_ = nullptr;
    QLabel* folder_lbl_ = nullptr;
    QLineEdit* folder_edit_ = nullptr;
    QPushButton* folder_browse_btn_ = nullptr;
    QLabel* template_lbl_ = nullptr;
    QComboBox* template_combo_ = nullptr;
    QLabel* prompt_lbl_ = nullptr;
    QPlainTextEdit* prompt_edit_ = nullptr;
    AgentPlaceholderHighlighter* highlighter_ = nullptr;
    QLabel* run_hint_ = nullptr;
    QPushButton* run_btn_ = nullptr;

    // ── Transcript widgets ───────────────────────────────────────────────────
    QLabel* picker_lbl_ = nullptr;
    QComboBox* run_picker_ = nullptr;
    QLabel* run_title_ = nullptr;
    QLabel* run_pill_ = nullptr;
    QLabel* run_meta_ = nullptr;
    QPushButton* stop_btn_ = nullptr;
    QPushButton* open_folder_btn_ = nullptr;
    QPushButton* view_result_btn_ = nullptr;
    QPushButton* copy_result_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    QLabel* error_banner_ = nullptr;
    QLabel* note_lbl_ = nullptr;
    QStackedWidget* transcript_stack_ = nullptr;
    QLabel* placeholder_lbl_ = nullptr;
    AgentTranscriptModel* model_ = nullptr;
    AgentTranscriptView* view_ = nullptr;
    QLineEdit* followup_edit_ = nullptr;
    QPushButton* followup_btn_ = nullptr;

    QTimer* flush_timer_ = nullptr; // coalesces transcript appends (100 ms)
    QTimer* tick_timer_ = nullptr;  // elapsed-time ticker while the selected run is running

    QString run_id_;
    QString pending_restore_run_; // restored before the run history finished loading
    QVector<services::coding_agents::AgentEvent> pending_;
    QList<QMetaObject::Connection> conns_;
    bool stop_clicked_ = false;
    bool loading_ = false;
};

} // namespace fincept::screens
