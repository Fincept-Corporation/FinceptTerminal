#pragma once
// CodingAgentTypes.h — value types for the Agent Workbench: local agent-harness
// CLI runs (Claude Code, Codex, Grok, … — see src/services/harness) wired to the
// terminal's own tools over MCP.
//
// Plain data only (no QObject, no widgets) so the parsers, the repository, the
// service, the MCP tools and the screen can all share them, and so the stream
// parsers stay unit-testable in isolation.
//
// A run names its harness by the registry id ("claude", "codex", "grok", …),
// stored verbatim in coding_agent_runs.agent. An id this build has no adapter
// for is kept as it is — shown as "unknown harness (<id>)", never relabelled.

#include "services/harness/HarnessTypes.h"

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace fincept::services::coding_agents {

enum class RunStatus { Queued, Running, Succeeded, Failed, Cancelled, Interrupted };
enum class EventKind { Text, Thinking, ToolCall, ToolResult, Command, FileChange, Status, Usage, Error, Final };
enum class ScheduleKind { Interval, Daily };

using harness::PermissionMode;

// ── Stable string ids (persisted in SQLite and exposed over MCP) ─────────────
/// Trimmed, lower-case, and the historical aliases folded ("claude_code" →
/// "claude", "codex-cli" → "codex"). For ids coming FROM a caller; stored rows
/// are read verbatim.
QString normalize_harness_id(const QString& id);
/// Legacy `mode` column: "analysis" (read-only) | "workspace_edits" (anything that writes).
QString legacy_mode_id(PermissionMode permission);
/// permission_mode column, falling back to the legacy `mode` column when empty
/// (rows written before v061): analysis → ReadOnly, workspace_edits → WorkspaceWrite.
PermissionMode permission_from_columns(const QString& permission_mode, const QString& legacy_mode);
QString status_id(RunStatus status);                 // "queued" … "interrupted"
RunStatus status_from_id(const QString& id);
bool is_terminal_status(RunStatus status);           // succeeded / failed / cancelled / interrupted
QString event_kind_id(EventKind kind);               // "text" | "thinking" | "tool_call" | …
EventKind event_kind_from_id(const QString& id);
QString schedule_kind_id(ScheduleKind kind);         // "interval" | "daily"
ScheduleKind schedule_kind_from_id(const QString& id);

// ── Limits ───────────────────────────────────────────────────────────────────
inline constexpr int kAgentEventBodyMaxBytes = 64 * 1024; // per event body (truncation is marked in place)
inline constexpr int kAgentEventRawMaxBytes = 16 * 1024;  // per raw JSON line kept for audit
inline constexpr int kAgentRunTitleMaxChars = 80;
inline constexpr int kAgentPromptMaxChars = 200000;
inline constexpr int kScheduleMinIntervalMinutes = 15;
inline constexpr qint64 kScheduleCatchUpWindowMs = 12LL * 3600 * 1000; // missed by < 12 h → run once
inline constexpr int kRunTimeLimitMaxSeconds = 4 * 3600;  // the service's hard cap; 0 = use the cap
inline constexpr int kRunMaxTurnsLimit = 1000;

/// One transcript entry. `seq`/`ts_ms` are assigned by the service when the
/// event is recorded; parsers leave them 0.
struct AgentEvent {
    int seq = 0;
    qint64 ts_ms = 0; // epoch ms (UTC)
    EventKind kind = EventKind::Status;
    QString title;
    QString body;     // ≤ kAgentEventBodyMaxBytes, truncation marked in place
    QString raw_json; // ≤ kAgentEventRawMaxBytes, only for events whose body is not the payload itself
    bool is_error = false;
    QString ref_id;   // tool_use_id / item id — pairs a tool call with its result
    /// A streaming fragment (Text or Thinking): `body` is the message so far and
    /// `ref_id` names the message. Shown live, never stored and never counted;
    /// the complete event (partial = false) follows and replaces it.
    bool partial = false;
};

/// One file a run created, changed or removed (parsed from a FileChange event body).
struct FileChangeEntry {
    QString kind; // add | update | delete | move | write (free text from the harness)
    QString path;
};
/// One line of a FileChange event body: "<kind>  <path>".
QString file_change_line(const QString& kind, const QString& path);
/// The entries of a FileChange event body (lines "<kind>  <path>"; a line with a
/// single token is a path with kind "update").
QVector<FileChangeEntry> parse_file_change_body(const QString& body);

/// One agent run (a single CLI invocation). A follow-up is a new run that
/// resumes the parent's session and points back at it via `parent_run_id`.
struct CodingAgentRun {
    QString id; // uuid
    QString title;
    QString harness = QStringLiteral("claude"); // registry id, verbatim from the row
    QString prompt;
    QString model;  // empty = the CLI's default
    QString effort; // empty = the CLI's default
    PermissionMode permission = PermissionMode::ReadOnly;
    QString workdir;
    bool workdir_auto = true; // created by the terminal under agent-workspace/
    bool use_fincept_tools = true;
    double max_budget_usd = 0.0; // harnesses with caps.budget_usd only; 0 = no cap
    int time_limit_s = 0;        // 0 = the service's hard cap (kRunTimeLimitMaxSeconds)
    int max_turns = 0;           // harnesses with caps.max_turns only; 0 = the CLI's default
    RunStatus status = RunStatus::Queued;
    qint64 created_at_ms = 0;
    qint64 started_at_ms = 0;  // 0 = never started
    qint64 finished_at_ms = 0; // 0 = not finished
    QString session_id;        // the harness's session / thread id (needed for follow-ups)
    QString harness_version;   // the CLI version the run was launched with
    std::optional<int> exit_code;
    std::optional<double> cost_usd; // only harnesses that report it
    qint64 input_tokens = 0;
    qint64 output_tokens = 0;
    qint64 cached_tokens = 0;  // input tokens served from the provider's cache (part of input_tokens)
    QString final_text;
    QString error;
    QString parent_run_id;
    QString schedule_id;
    QString origin = QStringLiteral("user"); // user | schedule | mcp | follow_up — audit trail
    /// Why the last attempt failed (harness::failure_id): "auth", "rate_limit",
    /// "not_installed", "timeout", … Not persisted — the error text carries the
    /// same sentence for runs loaded from disk.
    QString failure;
};

/// What a caller asks for; the service turns it into a CodingAgentRun.
struct RunRequest {
    QString harness; // registry id; empty = the user's default harness
    QString prompt;
    QString model;
    QString effort;
    PermissionMode permission = PermissionMode::ReadOnly;
    QString workdir; // empty = auto folder under agent-workspace/
    bool use_fincept_tools = true;
    double max_budget_usd = 0.0;
    int time_limit_s = 0;
    int max_turns = 0;
    QString title; // empty = derived from the prompt
    QString schedule_id;
    QString origin = QStringLiteral("user");
};

/// A recurring brief. `interval` fires every N minutes (≥ 15); `daily` fires at
/// HH:mm local time on the weekdays in `weekdays_mask` (bit0 = Monday … bit6 = Sunday).
struct CodingAgentSchedule {
    QString id;
    QString name;
    QString harness = QStringLiteral("claude"); // registry id, verbatim from the row
    QString prompt;
    QString model;
    QString effort;
    PermissionMode permission = PermissionMode::ReadOnly;
    bool use_fincept_tools = true;
    double max_budget_usd = 0.0;
    int time_limit_s = 0;
    int max_turns = 0;
    QString workdir; // empty = a fresh auto folder per run
    ScheduleKind kind = ScheduleKind::Daily;
    int interval_minutes = 60;
    QString daily_time = QStringLiteral("08:00");
    int weekdays_mask = 0x1F; // Mon–Fri
    bool enabled = true;
    qint64 last_run_at_ms = 0;
    qint64 next_run_at_ms = 0;
    qint64 created_at_ms = 0;
    qint64 updated_at_ms = 0;
};

/// First non-empty prompt line, whitespace-collapsed, ≤ kAgentRunTitleMaxChars.
QString run_title_from_prompt(const QString& prompt);

/// Extract "MAJOR.MINOR.PATCH" from a `--version` line ("2.1.296 (Claude Code)",
/// "codex-cli 0.159.2"). Empty when no version-looking token is present.
QString parse_cli_version(const QString& line);

/// True when `version` ("2.1.296") is ≥ major.minor.patch. False for empty/unparseable.
bool cli_version_at_least(const QString& version, int major, int minor, int patch);

/// -1 / 0 / 1 comparing two "MAJOR.MINOR.PATCH" versions numerically; an
/// unparseable side compares as 0 (equal) so nothing is claimed about it.
int compare_cli_versions(const QString& a, const QString& b);

/// Next fire time strictly after `after_ms` (epoch ms). Daily schedules use the
/// local time zone. Returns 0 when the schedule can never fire (invalid time,
/// empty weekday mask).
qint64 compute_next_run_ms(const CodingAgentSchedule& schedule, qint64 after_ms);

/// Validate a schedule before saving; returns an empty string when valid.
QString validate_schedule(const CodingAgentSchedule& schedule);

/// Truncate `text` to at most `max_bytes` of UTF-8, never splitting a code point,
/// and mark the cut in place ("… [truncated N bytes]"). Unchanged when it fits.
QString clip_utf8(const QString& text, int max_bytes);

} // namespace fincept::services::coding_agents

Q_DECLARE_METATYPE(fincept::services::coding_agents::AgentEvent)
