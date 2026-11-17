#pragma once
// AgentRunPanel.h — the harness-run page of the agents screen's inspector (the
// right half of the old Agent Workbench RUN tab): a header with the harness and
// its version, model, effort, mode, duration, tokens, cost and the exact command
// line (redacted), the transcript (tool cards, live streamed text) and the FILES
// the run changed, with STOP / FOLLOW UP / OPEN FOLDER / VIEW + COPY RESULT /
// EXPORT. The run is picked in the screen's one run list (the old run picker is
// gone); the composer is AgentRunComposer.
//
// Lifecycle: subscribes to CodingAgentService in showEvent and disconnects in
// hideEvent; transcript appends are coalesced (≤ 10 repaints/s) and the elapsed
// ticker only runs while visible and the selected run is running. Never blank:
// with no run selected it opens the latest harness run, and with no runs at all
// it shows what to do first.

#include "services/coding_agents/CodingAgentTypes.h"

#include <QList>
#include <QMetaObject>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTimer;

namespace fincept::screens {

class AgentRunFilesView;
class AgentTranscriptModel;
class AgentTranscriptView;

class AgentRunPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AgentRunPanel(QWidget* parent = nullptr);

    /// Select a run (empty = none).
    void show_run(const QString& run_id);
    QString current_run_id() const { return run_id_; }
    /// EXPORT .MD of the shown run (the screen's IMPORT / EXPORT menu).
    void export_transcript() { on_export(); }

    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

  signals:
    /// A follow-up started a new run (the screen selects it in the run list).
    void run_started(const QString& run_id);
    /// The run shown changed (the screen keeps its list selection in step).
    void run_shown(const QString& run_id);
    /// The empty state's "START FROM A TEMPLATE" (open the composer).
    void new_run_requested();
    /// The empty state's "HARNESSES".
    void show_harnesses_requested();
    /// A short confirmation for the screen's status bar (an export was written).
    void notice(const QString& text);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void build();
    void retranslate();

    void subscribe();
    void unsubscribe();

    void on_run_event(const QString& run_id, const services::coding_agents::AgentEvent& event);
    void on_run_updated(const QString& run_id);
    void on_runs_reset();
    void resync_transcript();
    void flush_pending();
    void refresh_header();
    void refresh_empty_state();
    void set_view_tab(int tab);
    void on_stop();
    void on_follow_up();
    void on_open_folder();
    void on_view_result();
    void on_copy_result();
    void on_export();
    void write_transcript_file(const QString& run_id, const QString& path,
                               const QVector<services::coding_agents::AgentEvent>& events);
    void remember_command(const QVector<services::coding_agents::AgentEvent>& events);

    QLabel* run_title_ = nullptr;
    QLabel* run_pill_ = nullptr;
    QLabel* run_meta_ = nullptr;
    QLabel* cmd_lbl_ = nullptr;
    QLineEdit* cmd_edit_ = nullptr;
    QPushButton* cmd_copy_btn_ = nullptr;
    QPushButton* stop_btn_ = nullptr;
    QPushButton* open_folder_btn_ = nullptr;
    QPushButton* view_result_btn_ = nullptr;
    QPushButton* copy_result_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    QLabel* error_banner_ = nullptr;
    QLabel* note_lbl_ = nullptr;
    QPushButton* transcript_tab_btn_ = nullptr;
    QPushButton* files_tab_btn_ = nullptr;
    QStackedWidget* view_stack_ = nullptr;       // transcript | files
    QStackedWidget* transcript_stack_ = nullptr; // empty state | loading | transcript
    QWidget* empty_page_ = nullptr;
    QLabel* empty_title_ = nullptr;
    QLabel* empty_text_ = nullptr;
    QPushButton* empty_primary_btn_ = nullptr;
    QPushButton* empty_secondary_btn_ = nullptr;
    QLabel* loading_lbl_ = nullptr;
    AgentTranscriptModel* model_ = nullptr;
    AgentTranscriptView* view_ = nullptr;
    AgentRunFilesView* files_view_ = nullptr;
    QLineEdit* followup_edit_ = nullptr;
    QPushButton* followup_btn_ = nullptr;

    QTimer* flush_timer_ = nullptr; // coalesces transcript appends (100 ms)
    QTimer* tick_timer_ = nullptr;  // elapsed-time ticker while the selected run is running

    QString run_id_;
    QString pending_restore_run_; // restored before the run history finished loading
    QString command_fallback_;    // a run from an earlier session: its start event's command line
    QVector<services::coding_agents::AgentEvent> pending_;
    QList<QMetaObject::Connection> conns_;
    int view_tab_ = 0;
    bool stop_clicked_ = false;
    bool loading_ = false;
    bool auto_opened_ = false; // the latest run was opened once for an otherwise empty view
};

} // namespace fincept::screens
