#pragma once
// CodingAgentTypes.h — value types for the Agent Workbench: local Claude Code /
// OpenAI Codex CLI runs wired to the terminal's own tools over MCP.
//
// Plain data only (no QObject, no widgets) so the parsers, the repository, the
// service, the MCP tools and the screen can all share them, and so the stream
// parsers stay unit-testable in isolation.

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace fincept::services::coding_agents {

enum class AgentKind { Claude, Codex };
enum class RunMode { Analysis, WorkspaceEdits };
enum class RunStatus { Queued, Running, Succeeded, Failed, Cancelled, Interrupted };
enum class EventKind { Text, Thinking, ToolCall, ToolResult, Command, FileChange, Status, Usage, Error, Final };
enum class ScheduleKind { Interval, Daily };

// ── Stable string ids (persisted in SQLite and exposed over MCP) ─────────────
QString agent_id(AgentKind kind);                    // "claude" | "codex"
std::optional<AgentKind> agent_from_id(const QString& id);
QString agent_display_name(AgentKind kind);          // "Claude Code" | "Codex"
QString mode_id(RunMode mode);                       // "analysis" | "workspace_edits"
RunMode mode_from_id(const QString& id);
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
};

/// One agent run (a single CLI invocation). A follow-up is a new run that
/// resumes the parent's session and points back at it via `parent_run_id`.
struct CodingAgentRun {
    QString id; // uuid
    QString title;
    AgentKind agent = AgentKind::Claude;
    QString prompt;
    QString model; // empty = the CLI's default
    RunMode mode = RunMode::Analysis;
    QString workdir;
    bool workdir_auto = true; // created by the terminal under agent-workspace/
    bool use_fincept_tools = true;
    double max_budget_usd = 0.0; // Claude only; 0 = no cap
    RunStatus status = RunStatus::Queued;
    qint64 created_at_ms = 0;
    qint64 started_at_ms = 0;  // 0 = never started
    qint64 finished_at_ms = 0; // 0 = not finished
    QString session_id;        // Claude session_id / Codex thread_id (needed for follow-ups)
    std::optional<int> exit_code;
    std::optional<double> cost_usd; // Claude reports it; Codex does not
    qint64 input_tokens = 0;
    qint64 output_tokens = 0;
    QString final_text;
    QString error;
    QString parent_run_id;
    QString schedule_id;
    QString origin = QStringLiteral("user"); // user | schedule | mcp | follow_up — audit trail
};

/// What a caller asks for; the service turns it into a CodingAgentRun.
struct RunRequest {
    AgentKind agent = AgentKind::Claude;
    QString prompt;
    QString model;
    RunMode mode = RunMode::Analysis;
    QString workdir; // empty = auto folder under agent-workspace/
    bool use_fincept_tools = true;
    double max_budget_usd = 0.0;
    QString title; // empty = derived from the prompt
    QString schedule_id;
    QString origin = QStringLiteral("user");
};

/// A recurring brief. `interval` fires every N minutes (≥ 15); `daily` fires at
/// HH:mm local time on the weekdays in `weekdays_mask` (bit0 = Monday … bit6 = Sunday).
struct CodingAgentSchedule {
    QString id;
    QString name;
    AgentKind agent = AgentKind::Claude;
    QString prompt;
    QString model;
    RunMode mode = RunMode::Analysis;
    bool use_fincept_tools = true;
    double max_budget_usd = 0.0;
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

/// Detected CLI installation.
struct CliInfo {
    AgentKind kind = AgentKind::Claude;
    QString path;               // resolved executable; empty = not found
    bool via_cmd_shim = false;  // Windows npm .cmd/.bat shim → launched through cmd.exe
    bool path_overridden = false;
    QString version;            // "2.1.296"; empty until probed / unknown
    QString version_line;       // raw first line of `--version`
    bool probing = false;
    bool probed = false;
    QString probe_error;
    bool installed() const { return !path.isEmpty(); }
};

/// First non-empty prompt line, whitespace-collapsed, ≤ kAgentRunTitleMaxChars.
QString run_title_from_prompt(const QString& prompt);

/// Extract "MAJOR.MINOR.PATCH" from a `--version` line ("2.1.296 (Claude Code)",
/// "codex-cli 0.159.2"). Empty when no version-looking token is present.
QString parse_cli_version(const QString& line);

/// True when `version` ("2.1.296") is ≥ major.minor.patch. False for empty/unparseable.
bool cli_version_at_least(const QString& version, int major, int minor, int patch);

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
