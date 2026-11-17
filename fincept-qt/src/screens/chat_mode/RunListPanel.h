#pragma once
// RunListPanel.h — the RUNS section of Chat Mode's left rail (WP2; regrouped in the
// 2026-11-17 revamp).
//
// Lists TaskRunService's runs in groups — PINNED, NEEDS YOU, TODAY, YESTERDAY, THIS
// WEEK, OLDER (or one group per state in the "By status" order) — each row a state
// mark, the title and its time, then the state pill, a one-line snippet, the engine
// chip, step n/m and credits. A state filter (ALL / ACTIVE / NEEDS YOU / FINISHED), an
// engine filter and a text search; pin / unpin in the context menu (QSettings
// "chat_mode/pinned_runs"); NEW RUN and SCHEDULES. Consumes TaskRunService signals
// only (run_added / run_updated / run_finished / runs_reset; a run that find_run()
// no longer knows was deleted). ChatModeScreen places it and reacts to its signals.

#include <QString>
#include <QWidget>

#include <memory>

namespace fincept::chat_mode {

class RunListPanel : public QWidget {
    Q_OBJECT
  public:
    explicit RunListPanel(QWidget* parent = nullptr);
    ~RunListPanel() override;

    /// Highlight `run_id` (empty clears the selection). Emits nothing.
    void select_run(const QString& run_id);
    /// select_run() for a run opened from elsewhere (a link): when the filter or
    /// the search hides it, both are cleared first so it shows. Emits nothing.
    void reveal_run(const QString& run_id);
    /// A plain (not an error) line above the list, e.g. "That run no longer
    /// exists"; it goes like an error line does, after a few seconds.
    void show_notice(const QString& text);
    QString selected_run() const;
    /// Keyboard focus into the list (the current row) / the search field.
    void focus_list();
    void focus_search();

  signals:
    /// The user picked a run (click / keyboard): show it in the inspector.
    void run_selected(const QString& run_id);
    /// The user opened a run (double click / Enter): show its conversation.
    void run_activated(const QString& run_id);
    void new_run_requested();
    void schedules_requested();

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::chat_mode
