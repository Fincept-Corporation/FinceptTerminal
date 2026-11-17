#pragma once
// RunArtefactsPanel.h — the ARTEFACTS tab of Chat Mode's inspector (WP5).
//
// The selected run's artefacts (ArtefactService) — or every PINNED one, those of
// deleted runs included — as a list (kind, title, step, time, size) above a
// preview (rendered Markdown, a sortable table grid, a chart of a series, an
// image), with Open, Save as…, Copy, Add to report, Send to Notes, Pin and Delete
// (inline confirmation). A designed empty state with its action when there is
// nothing to list. Previews load off the GUI thread and only for the selected
// artefact; a stale load never draws over a newer selection.

#include <QString>
#include <QWidget>

#include <memory>

namespace fincept::chat_mode {

class RunArtefactsPanel : public QWidget {
    Q_OBJECT
  public:
    explicit RunArtefactsPanel(QWidget* parent = nullptr);
    ~RunArtefactsPanel() override;

    /// Show `run_id`'s artefacts (empty: nothing selected).
    void set_run(const QString& run_id);
    QString run_id() const;

  signals:
    /// The empty state's action: start a task.
    void new_run_requested();
    /// "Open Report Builder" / "Open Notes" after a hand-off ("report_builder", "notes").
    void open_screen_requested(const QString& screen_id);

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::chat_mode
