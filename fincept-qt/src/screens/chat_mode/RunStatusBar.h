#pragma once
// RunStatusBar.h — the strip under Chat Mode (WP2; on ui::ScreenStatusBar since the
// 2026-11-17 revamp): engine · model, the current run's state, steps done / total,
// tokens and credits where the service reports them (spent against the budget),
// elapsed time, last response time, the terminal tool bridge's state
// (TerminalToolBridge::instance()), the MCP servers, the account balance
// (ChatModeService::credit_balance_changed / AuthManager) and the PAPER badge.
// ChatModeScreen places it and tells it the window's current run.

#include <QString>
#include <QWidget>

#include <memory>

namespace fincept::ui {
class ScreenStatusBar;
}

namespace fincept::chat_mode {

class RunStatusBar : public QWidget {
    Q_OBJECT
  public:
    explicit RunStatusBar(QWidget* parent = nullptr);
    ~RunStatusBar() override;

    /// The run this window shows (empty: none).
    void set_run(const QString& run_id);
    QString run_id() const;

    /// What the next message runs on ("Cloud · one reply", "Harness · Claude Code ·
    /// sonnet") — shown while no run is shown.
    void set_next_engine(const QString& engine_label);
    /// Response time of the conversation's last stored answer (ChatMessage::
    /// response_time_ms); < 0 = unknown. A run shown overrides it with its own.
    void set_last_response_ms(qint64 ms);
    /// Remote MCP servers of the cloud agent: connected / total (< 0 = unknown).
    void set_mcp_state(int connected, int total);

    /// The bar itself (transient notices: "Exported to …").
    ui::ScreenStatusBar* bar() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::chat_mode
