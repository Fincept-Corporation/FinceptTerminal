#pragma once
// ToolApprovalDialog.h — per-call user approval for tool calls made by EXTERNAL
// agents (Claude Code, Codex, Cursor… connected through McpHttpServer).
//
// Live-trading tools and tools whose auth level is ExplicitConfirm never run on
// an agent's say-so alone: the MCP worker thread that is handling the call asks
// the user here and waits for the verdict.
//
// Threading contract
//   * request_approval() is called from a WORKER thread (McpHttpServer's tool
//     pool). It posts the request to the GUI thread and blocks the calling
//     worker — never the GUI thread — on a QWaitCondition until the user
//     decides or the deadline passes (default 50 s, deliberately below the 60 s
//     tool timeout Codex applies). Calling it on the GUI thread returns
//     Unavailable instead of deadlocking.
//   * Everything else (the queue, the dialog) lives on the GUI thread.
//
// Behaviour
//   * One dialog at a time; concurrent requests queue FIFO. A request's
//     deadline is absolute from the moment it was made, so time spent waiting
//     behind another dialog counts against it.
//   * Non-modal and kept above the terminal's windows, shown WITHOUT taking
//     keyboard focus (the user is typically typing in another app when an agent
//     asks), with APPROVE disarmed for the first second and DENY as the default
//     button — a stray Enter can only ever deny.
//   * The dialog says WHAT KIND of action it is (ApprovalPolicy.h: order with
//     LIVE / PAPER stated truthfully, money, spend with the cost when known,
//     delete, account, code, file, generic), who asks (label, client id, run)
//     and the exact control and values.
//   * A timed grant ("allow … for 15 minutes") is offered only for spend,
//     delete, file and generic actions, scoped to the same client + run + kind +
//     screen, and dropped when the run ends. Orders, money moves, account and
//     code actions are approved one at a time, always.
//   * When the terminal has no visible window (headless self-tests, a window
//     that never opened) the request is denied immediately.

#include "mcp/server/ApprovalPolicy.h"

#include <QDialog>
#include <QJsonObject>
#include <QString>

class QCheckBox;
class QEvent;
class QHideEvent;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QShowEvent;
class QTimer;

namespace fincept::mcp {

// ToolApprovalRequest and ToolApprovalVerdict: mcp/server/ApprovalPolicy.h.

class ToolApprovalDialog : public QDialog {
    Q_OBJECT
  public:
    /// Default approval window. Must stay below the 60 s per-tool timeout Codex
    /// CLI applies, so a timed-out approval is reported as such rather than as a
    /// client-side transport timeout.
    static constexpr int kDefaultTimeoutMs = 50000;
    static constexpr int kGrantMinutes = approval::kGrantMinutes;
    static constexpr int kArgPreviewMaxChars = 1536;
    static constexpr int kArmDelayMs = 1000;

    /// WORKER THREAD ONLY. Asks the user and blocks the calling thread until a
    /// verdict, the deadline (`timeout_ms` from now), or Unavailable. An active
    /// timed grant for exactly this request's client + run + kind + screen
    /// approves immediately without a dialog (never for order / money / account /
    /// code). Empty client_id / run_id are taken from ScopedCallClient on the
    /// calling thread.
    static ToolApprovalVerdict request_approval(const ToolApprovalRequest& request,
                                                int timeout_ms = kDefaultTimeoutMs);

    /// True while an unexpired timed grant covers this request. Thread-safe.
    static bool has_active_grant(const ToolApprovalRequest& request);

    /// Forget every session grant. Thread-safe.
    static void clear_grants();
    /// Forget the grants made during one run (the run ended). Thread-safe.
    static void clear_grants_for_run(const QString& run_id);

    /// GUI THREAD. Deny every queued request and the one on screen (used when the
    /// MCP server stops, so no worker keeps waiting on a dialog nobody needs).
    static void deny_all_pending();

    // ── Auto mode (Settings › AI Agents (MCP)) ──────────────────────────────
    // When on, request_approval() answers Approved at once for EVERY caller —
    // external MCP clients, AI Chat and the agent bridge — without showing a
    // dialog: orders, money moves, spends, deletes, sign-out and sensitive
    // scripts all run on the agent's say-so. Each auto-approval is logged (WARN)
    // and live-trading ones raise a desktop notification. Hard rules are NOT
    // approvals and still hold: a PAPER workflow run stays paper, Blocked
    // scripts stay refused, secret fields are never read or typed.
    // Off by default; persisted as `mcp/auto_approve`.

    /// True when auto mode is on. Thread-safe (atomic, seeded from settings once).
    static bool auto_approve();
    /// Turn auto mode on/off and persist it. Call from the GUI thread.
    static void set_auto_approve(bool on);
    /// Read the persisted value into the cache. GUI thread; called at startup so
    /// worker threads never touch QSettings.
    static void load_auto_approve_setting();

    ToolApprovalDialog(const ToolApprovalRequest& request, qint64 deadline_ms, QWidget* parent = nullptr);
    ~ToolApprovalDialog() override;

    /// The "allow for 15 minutes" checkbox state at the time of the verdict.
    bool remember_for_session() const;

    /// Resolve the dialog programmatically (shutdown / server stop). Emits
    /// verdict_reached once, like a user decision would.
    void force_verdict(fincept::mcp::ToolApprovalVerdict verdict);

  signals:
    /// Emitted exactly once per dialog.
    void verdict_reached(fincept::mcp::ToolApprovalVerdict verdict);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
    /// Esc and the window close button both land here: treated as DENY.
    void reject() override;

  private:
    void build_ui();
    void apply_theme();
    void retranslate_ui();
    void on_tick();
    void finish(ToolApprovalVerdict verdict);
    void update_countdown();
    QString status_text() const;
    static QString argument_preview(const QJsonObject& args);

    ToolApprovalRequest request_;
    approval::Presentation presentation_;
    qint64 deadline_ms_ = 0;
    qint64 shown_at_ms_ = 0;
    bool finished_ = false;
    bool grant_allowed_ = false;

    QLabel* title_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* lede_label_ = nullptr;
    QLabel* client_caption_ = nullptr;
    QLabel* client_value_ = nullptr;
    QLabel* tool_caption_ = nullptr;
    QLabel* tool_value_ = nullptr;
    QLabel* desc_caption_ = nullptr;
    QLabel* desc_value_ = nullptr;
    QLabel* args_caption_ = nullptr;
    QPlainTextEdit* args_view_ = nullptr;
    QCheckBox* remember_box_ = nullptr;
    QLabel* countdown_label_ = nullptr;
    QPushButton* deny_button_ = nullptr;
    QPushButton* approve_button_ = nullptr;
    QTimer* tick_timer_ = nullptr;
};

} // namespace fincept::mcp
