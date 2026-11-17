#pragma once
// RunActivityView.h — the ACTIVITY tab of Chat Mode's inspector (WP2): every
// event of the run (verbose ones included, collapsed), virtualised: a list
// model over run_view::ActivityLog (tool call + result paired by ref_id) and a
// painting delegate, the same technique as the Agent Workbench's
// AgentTranscriptView. Loads history with TaskRunService::fetch_events() and
// follows run_event live (append-only, never rebuilt per event).

#include <QString>
#include <QWidget>

#include <memory>

namespace fincept::chat_mode {

class RunActivityView : public QWidget {
    Q_OBJECT
  public:
    explicit RunActivityView(QWidget* parent = nullptr);
    ~RunActivityView() override;

    /// Show `run_id`'s activity (empty: clear).
    void set_run(const QString& run_id);
    QString run_id() const;
    void clear();

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::chat_mode
