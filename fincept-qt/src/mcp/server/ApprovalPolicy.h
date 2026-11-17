#pragma once
// ApprovalPolicy.h — WHAT an agent asks the user to approve, and the rules for
// it, without any widget: the request, its kind, the dialog's heading and plain
// sentence per kind, which kinds may be covered by a timed grant, and the
// session grant store. ToolApprovalDialog shows it; the unit tests drive it
// directly (Qt Core only).
//
// Kinds (the UI engine's, src/mcp/core/UiRisk.h; aliases trade → order,
// destructive → delete, anything unknown → generic):
//
//   kind      heading                               timed grant
//   order     Place / change an order (LIVE/PAPER)   never — every order on its own
//   money     Move money or change balances          never
//   account   Change account or security settings    never
//   code      Run code on this computer              never
//   spend     Use credits or model quota             15 min, same client + run + kind + screen
//   delete    Delete data                            15 min, same client + run + kind + screen
//   file      Write a file                           15 min, same client + run + kind + screen
//   generic   Agent action needs your approval       15 min, same client + run + kind + screen
//
// A grant is never "don't ask again": it lives in memory for kGrantMinutes, is
// never shared across kinds, clients, runs or screens, and is dropped when its
// run ends (McpHttpServer::end_run) or the token is revoked.

#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QString>

namespace fincept::mcp {

/// What the screen shows about the trading mode of an order.
enum class ApprovalTradingMode {
    Unknown, ///< the screen does not say (the dialog says so, and treats it as real money)
    Live,
    Paper
};

/// What the user is asked to approve.
struct ToolApprovalRequest {
    QString client;      ///< the caller's label ("claude-code", "Claude Code · terminal AI", "Fincept agent: …")
    QString tool;        ///< what is being approved ("ui_act: click Place order (…)", "python: …")
    QString description; ///< the action in words (the control, where, what it does)
    QJsonObject args;    ///< the values — shown as a bounded preview, never logged
    bool destructive = false;
    bool live_trading = false; ///< legacy flag: an order / money action (kind wins when set)

    QString kind; ///< order, money, spend, delete, account, code, file, generic ("" → from the flags)
    ApprovalTradingMode trading_mode = ApprovalTradingMode::Unknown; ///< order: what the screen shows
    QString client_id; ///< stable caller id (ScopedCallClient: "claude_code", "codex", "terminal_agent" …)
    QString run_id;    ///< the agent run, when known
    QString screen;    ///< where it happens (a UI action's panel / dialog); grants are scoped to it
    QString cost;      ///< spend: what it costs, when known ("≈ 40 credits")
};

enum class ToolApprovalVerdict {
    Approved,
    Denied,     ///< the user pressed DENY / Esc / closed the dialog
    TimedOut,   ///< nobody answered before the deadline
    Unavailable ///< no visible terminal window to ask in, or called on the GUI thread
};

namespace approval {

inline constexpr int kGrantMinutes = 15;

/// The request's kind, normalised: an alias or unknown kind mapped, "" derived
/// from the legacy flags (live_trading → order, else generic).
QString kind_of(const ToolApprovalRequest& request);

/// spend, delete, file and generic may be covered by a timed grant; order,
/// money, account and code never.
bool timed_grant_allowed(const QString& kind);

/// The dialog's text for a request.
struct Presentation {
    QString kind;        ///< normalised
    QString heading;     ///< "Place / change an order"
    QString sentence;    ///< plain words: what approving does (LIVE / PAPER stated truthfully)
    QString status;      ///< the header chip ("LIVE · REAL MONEY", "PAPER · SIMULATED", "DELETES DATA" …)
    QString client;      ///< "Claude Code · terminal AI — claude_code · run run-42"
    QString grant_label; ///< the checkbox text
    QString grant_note;  ///< why no grant is offered / what the grant covers
    bool danger = false; ///< real money (or possibly): red approve button and chip
    bool grant_allowed = false;
};
Presentation present(const ToolApprovalRequest& request);

/// LIVE / PAPER from what a screen shows ("LIVE BUY LIMIT 0.5 BTC", "Confirm
/// Live Order", a "PAPER" mode label). Both or neither → Unknown.
ApprovalTradingMode trading_mode_from_text(const QString& text);

/// Short words for logs: "live", "paper", "unknown".
QString trading_mode_word(ApprovalTradingMode mode);

/// The grant key of a request: client (stable id, else label) + run + kind +
/// scope (screen, else the tool). Empty when the kind allows no timed grant.
QString grant_key(const ToolApprovalRequest& request);

/// In-memory, session-only timed grants. Thread-safe.
class GrantBook {
  public:
    /// Record a grant for this request's (client, run, kind, screen) until
    /// now + minutes. A no-op for kinds that allow none.
    void add(const ToolApprovalRequest& request, qint64 now_ms, int minutes = kGrantMinutes);
    /// True while an unexpired grant covers exactly this request's key.
    bool covers(const ToolApprovalRequest& request, qint64 now_ms);
    void clear();
    /// Drop every grant made during this run.
    void clear_run(const QString& run_id);
    int size() const;

  private:
    struct Grant {
        qint64 expires_ms = 0;
        QString run_id;
    };
    mutable QMutex mutex_;
    QHash<QString, Grant> grants_;
};

/// The terminal's grants (ToolApprovalDialog).
GrantBook& session_grants();

} // namespace approval
} // namespace fincept::mcp
