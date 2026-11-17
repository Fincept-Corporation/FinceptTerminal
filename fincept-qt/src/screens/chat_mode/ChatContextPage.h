#pragma once
// ChatContextPage.h — the CONTEXT page of Chat Mode's inspector (2026-11-17 revamp):
// exactly what the run can see and use.
//
//   ▌CONTEXT   3                         [+ SYMBOL] [+ FILE] [+ MORE]
//     AAPL ×   Portfolio: Growth ×   Attached: notes.txt · 4,210 chars ×
//     SENT WITH THIS RUN  Symbols: AAPL · Portfolio: Growth
//   ▌TOOLS                                                   [REFRESH]
//     Terminal tools  12 ready      MCP servers  2 of 3 connected · 14 tools
//     ● research-mcp · connected · 9 tools …
//   ▌FILES     4
//     TABLE  Peer comparison  12 KB …   (click: the FILES page)
//   ▌BUDGET & LIMITS
//     12.5 / 50 credits ▬▬▬▬▬▭▭▭▭▭   3 / 8 steps   4m 10s / 30 min   PAPER ONLY …
//   ▌RUN STATS
//     state · engine · turns · tokens · credits · elapsed · started · finished · steps
//
// The chips are the composer's ComposerContextBar (detach / attach go through its
// public API). The MCP servers are read once on the first show and on REFRESH only;
// the stats tick once a second only while the run works and the page is visible.
// Nothing here does I/O on the GUI thread. Theme tokens only; sizes from font metrics.

#include <QString>
#include <QWidget>

#include <memory>

namespace fincept::chat_mode {

class ComposerContextBar;

class ChatContextPage : public QWidget {
    Q_OBJECT
  public:
    explicit ChatContextPage(QWidget* parent = nullptr);
    ~ChatContextPage() override;

    /// The run the page describes (empty: none — the next message's context,
    /// tools and budget still show).
    void set_run(const QString& run_id);
    QString run_id() const;
    /// The composer's context bar (not owned).
    void set_context_bar(ComposerContextBar* bar);
    /// Read the cloud agent's MCP servers again (the REFRESH button).
    void refresh_mcp();
    /// Keyboard focus to the first control.
    void focus_first();

  signals:
    /// A file row was clicked: show the FILES page (on that artefact).
    void files_requested(const QString& artefact_id);
    /// The MCP servers were read: connected / total (-1, -1 when they could not be).
    void mcp_state_changed(int connected, int total);

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::chat_mode
