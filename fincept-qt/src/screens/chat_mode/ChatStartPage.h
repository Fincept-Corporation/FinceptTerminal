#pragma once
// ChatStartPage.h — what Chat Mode's centre shows with no conversation open / on a
// brand-new profile (2026-11-17 revamp): a designed page, never blank.
//
//   CHAT MODE
//   Long-running finance tasks with an agent: it plans, uses the terminal's tools, asks
//   before it acts, and keeps going while you work elsewhere.
//   [EARNINGS PREP] [PORTFOLIO REVIEW] [SECTOR SCREEN] [BACKTEST IDEA] [MACRO BRIEF] [RISK CHECK]
//        (cards only FILL the composer — nothing is sent)
//   RECENT RUNS  title · state · time  (open)
//   SET UP  (only for what is missing: sign in, a model, a coding agent) -> Settings sections
//
// Cheap to refresh (GUI thread, no I/O): reads TaskRunService and the requirement
// checks. Theme tokens only; sizes from font metrics; re-flows from 1000 px to
// ultrawide (cards wrap).

#include <QString>
#include <QWidget>

#include <memory>

namespace fincept::chat_mode {

class ChatStartPage : public QWidget {
    Q_OBJECT
  public:
    explicit ChatStartPage(QWidget* parent = nullptr);
    ~ChatStartPage() override;

    /// Re-read the recent runs and the requirements (also on every show).
    void refresh();
    /// Keyboard focus to the first example card.
    void focus_first();

  signals:
    /// An example card: its prompt goes into the composer (never sent).
    void example_chosen(const QString& prompt);
    /// A recent run.
    void run_open_requested(const QString& run_id);
    /// "Browse templates" (NEW RUN's gallery).
    void templates_requested();
    /// A SET UP link is about to open a Settings section / the sign-in screen
    /// (ui::open_setup_link, queued): Chat Mode covers the window, so the screen
    /// should leave it now (emit exit_requested()). Emitted just before the link.
    void leave_requested();

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::chat_mode
