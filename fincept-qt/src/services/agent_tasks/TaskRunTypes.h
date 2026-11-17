#pragma once
// TaskRunTypes.h — the run model shared by Chat Mode's run engine (TaskRunService),
// its executors and every view of a run.
//
// A run is one long-running agentic task: a single cloud reply (CloudTurnExecutor),
// a planned multi-step cloud task (CloudPlanExecutor) or a local agentic task
// (LocalAgenticExecutor). Runs belong to TaskRunService, not to a screen: they keep
// going when the user switches conversations, leaves Chat Mode or locks the window.
//
// Qt Core only (unit-tested without the app). Persisted by TaskRunStore in the
// agent_task_* tables of migration v059; timestamps are epoch milliseconds (UTC).

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace fincept::agent_tasks {

// ── States ───────────────────────────────────────────────────────────────────

/// Queued → Planning → Running ⇄ WaitingInput | WaitingApproval | Paused →
/// Completed | Failed | Cancelled | BudgetStopped, plus Interrupted for a run the
/// app quit caught (WaitingInput and Paused survive a restart as they are).
enum class RunStatus {
    Queued,
    Planning,
    Running,
    WaitingInput,
    WaitingApproval,
    Paused,
    Completed,
    Failed,
    Cancelled,
    BudgetStopped,
    Interrupted,
};

enum class StepStatus { Pending, Running, Done, Failed, Skipped };

/// What one transcript row of a run is (RunEvent::kind).
enum class RunEventKind {
    Plan,            ///< the plan (cloud "planning" summary, or the parsed step list)
    StepStart,       ///< a run step (or a server window, data.sub = "window") started
    StepEnd,         ///< … ended
    Thinking,        ///< model reasoning text
    ToolCall,        ///< a tool was called; paired with its ToolResult by ref_id
    ToolResult,      ///< a tool finished (body = output preview, data.durationMs / success)
    Delegation,      ///< specialists were delegated to (data.phase = "start" | "end")
    Question,        ///< the agent asked the user something (data.resumable, data.mode)
    Answer,          ///< the user's reply to a Question
    Approval,        ///< something needs the user's decision (data = the proposal)
    ApprovalDecided, ///< … and was decided (data = the decision)
    Usage,           ///< credits / tokens (data.credits_charged, credit_balance, tokens_total)
    Note,            ///< informational (history compressed, rate-limit retry, follow-ups…)
    Artefact,        ///< an artefact was added (ref_id = artefact id)
    Error,           ///< something failed (data.code = machine-readable reason)
    Final,           ///< the run's final text
    Status,          ///< a state / connection change (detached, recovering, queued…)
};

// ── Identifiers and defaults ─────────────────────────────────────────────────

/// RunRequest::engine values.
inline constexpr const char* kEngineCloud = "cloud";
inline constexpr const char* kEngineLocal = "local";

/// IRunExecutor::engine_id() values; executor_for() maps (engine, plan_first) here.
inline constexpr const char* kExecutorCloudTurn = "cloud_turn"; ///< one cloud reply = one run (chat)
inline constexpr const char* kExecutorCloudPlan = "cloud_plan"; ///< plan turn + one turn per step + synthesis
inline constexpr const char* kExecutorLocal = "local";          ///< AgentService's local agentic engine

/// RunRequest::origin values.
inline constexpr const char* kOriginChat = "chat";         ///< a message typed in a Chat Mode conversation
inline constexpr const char* kOriginUser = "user";         ///< NEW RUN / templates
inline constexpr const char* kOriginSchedule = "schedule"; ///< fired by TaskRunScheduler (unattended)
inline constexpr const char* kOriginRetry = "retry";       ///< retry_from_step() child run

/// RunRequest::context keys understood by the run core.
inline constexpr const char* kCtxAnswerToPause = "answer_to_pause"; ///< bool: the prompt answers the question the
                                                                   ///< conversation's run paused on (cloud resume)

// Owner decisions (2026-11-16). A RunBudget field of 0 means "unlimited"; the
// NEW RUN dialog pre-fills these.
inline constexpr double kDefaultMaxCredits = 50.0;
inline constexpr int kDefaultMaxSteps = 8;
inline constexpr double kConfirmAboveCredits = 25.0; ///< ask before starting a run estimated above this
inline constexpr double kCloudTurnCredits = 5.0;     ///< nominal price of one cloud agent turn (estimates only)
inline constexpr int kMaxConcurrentRuns = 2;         ///< runs in Planning / Running at once
inline constexpr int kMaxQueuedRuns = 20;            ///< runs waiting for a slot
inline constexpr int kRunPromptMaxChars = 48000;     ///< whole prompt incl. inlined context (32 000-char attachment)
inline constexpr int kChatMessageMaxChars = 20000;   ///< one composer message (the server's resume limit)

// ── Requests ─────────────────────────────────────────────────────────────────

struct RunBudget {
    double max_credits = 0;
    int max_steps = 0;
    int max_minutes = 0;
}; // 0 = unlimited

struct RunRequest {
    QString title, prompt, engine, mode, template_id, session_uuid, origin, parent_run_id, schedule_id;
    int profile_id = 0;
    QJsonObject context;
    RunBudget budget;
    bool plan_first = false;
};

// ── Records ──────────────────────────────────────────────────────────────────

struct RunStep {
    QString run_id;
    int idx = 0;
    QString title;
    QString instruction;
    QString deliverable; ///< what the step must produce (plan JSON "deliverable")
    StepStatus status = StepStatus::Pending;
    QString summary; ///< the step's result (final text of its turn, or a short summary)
    int attempts = 0;
    double credits = 0;
    QString message_uuid; ///< server message that holds the step's answer (cloud), if known
    qint64 started_at_ms = 0;
    qint64 finished_at_ms = 0;
};

struct RunEvent {
    int seq = 0;       ///< 1-based, per run; assigned by TaskRunService
    qint64 ts_ms = 0;  ///< assigned by TaskRunService when 0
    int step = -1;     ///< run step index; -1 = the run's current step (filled in by TaskRunService)
    RunEventKind kind = RunEventKind::Note;
    QString title;
    QString body;
    QJsonObject data;
    bool is_error = false;
    QString ref_id; ///< pairs ToolCall/ToolResult, Delegation start/end; proposal id; artefact id
};

/// RunArtefact::kind values.
inline constexpr const char* kArtefactReport = "report"; ///< Markdown (final report, step deliverable, saved reply)
inline constexpr const char* kArtefactTable = "table";   ///< rows + columns (CSV file, inline JSON when small)
inline constexpr const char* kArtefactSeries = "series"; ///< a table whose first column is the x axis (chart preview)
inline constexpr const char* kArtefactJson = "json";     ///< a tool result kept as JSON
inline constexpr const char* kArtefactText = "text";     ///< plain text
inline constexpr const char* kArtefactImage = "image";   ///< PNG / JPEG
inline constexpr const char* kArtefactFile = "file";     ///< anything else

struct RunArtefact {
    QString id;
    QString run_id;
    int step_idx = -1;
    QString kind;        ///< kArtefact* ("report" | "table" | "series" | "json" | "text" | "image" | "file")
    QString title;
    QString mime;
    QString stored_name; ///< file name in ArtefactService::run_folder(run_id); empty for inline-only
    QJsonValue inline_json; ///< small payloads kept in the database (tables, KPIs); Undefined when none
    qint64 bytes = 0;
    qint64 created_at_ms = 0;
    // ── WP5 (migration v063) ──
    bool pinned = false;  ///< kept when its run is deleted, never removed by the size policy
    /// Provenance: {"origin": "final" | "step" | "table" | "tool" | "reply" | "user", "key": dedupe key,
    /// "tool", "ref_id", "step", "event_seq", "source_title"} — which step / tool produced it.
    QJsonObject source;
    QString run_title; ///< the run's title when it was made (shown once the run is deleted)
};

struct TaskRun {
    QString id;
    QString title;
    QString prompt;
    QString engine; ///< kEngineCloud | kEngineLocal
    bool plan_first = false;
    QString mode;   ///< cloud: "lite" | "deep"
    QString template_id;
    QString session_uuid;   ///< cloud conversation the run talks in
    QString remote_task_id; ///< local engine task id / server task id
    int profile_id = 0;
    QJsonObject context;
    RunStatus status = RunStatus::Queued;
    int current_step = 0;
    QJsonArray plan; ///< plan_json: the step list as planned (see steps_to_plan_json)
    RunBudget budget;
    double credits_spent = 0;
    qint64 tokens_used = 0;
    int turns = 0; ///< agent turns (cloud: one per /chat/agent/stream or resume request)
    QString final_text;
    QString error;
    QString origin;
    QString parent_run_id;
    QString schedule_id;
    qint64 created_at_ms = 0;
    qint64 started_at_ms = 0;
    qint64 finished_at_ms = 0;
    qint64 updated_at_ms = 0;
    // ── WP5/WP6 (migration v063) ──
    /// Time spent working (Planning / Running), ms, excluding pauses and waits —
    /// the elapsed time shown and the clock of the max_minutes budget.
    qint64 active_ms = 0;
    /// When the current working stretch began (epoch ms); 0 while not working.
    qint64 active_since_ms = 0;
    /// Agent turns the server charged but did not report (an answer recovered from
    /// the stored conversation carries no bill): credits_spent is a lower bound.
    int credits_unreported = 0;

    // ── Runtime only (not persisted) ──
    QString activity;            ///< one live status line ("Waiting for the stored answer…")
    QJsonObject waiting_payload; ///< the question / approval while WaitingInput / WaitingApproval
    bool detached = false;       ///< cloud: the user stopped following the stream (the server keeps working)
    bool recovering = false;     ///< cloud: reading the server's stored answer after a lost / stalled stream
    /// Set by resume_run() for an Interrupted run (the terminal closed mid-turn): the
    /// executor first looks for an answer the server stored meanwhile. A paused run
    /// stopped at a step boundary, so nothing can have been stored for it.
    bool resume_after_restart = false;

    QString executor_id() const;
    /// Started by a schedule: nobody watches it, so money actions are refused (paper only).
    bool is_unattended() const;
};

/// RunSchedule::kind values.
inline constexpr const char* kScheduleOnce = "once";         ///< once, at once_at_ms
inline constexpr const char* kScheduleDaily = "daily";       ///< daily_time on the days of weekdays_mask
inline constexpr const char* kScheduleWeekly = "weekly";     ///< daily_time on one weekday (one bit of the mask)
inline constexpr const char* kScheduleInterval = "interval"; ///< every interval_minutes (on the mask's days)

/// Recurring run (agent_task_schedules). TaskRunScheduler owns the semantics.
struct RunSchedule {
    QString id;
    QString name;
    QString template_id;
    QString prompt;
    QString engine = QString::fromLatin1(kEngineCloud);
    QString mode;
    RunRequest request; ///< request_json: everything else the run is started with
    double max_credits = 0;
    QString kind = QStringLiteral("daily"); ///< kSchedule*
    int interval_minutes = 60;
    QString daily_time = QStringLiteral("08:00"); ///< HH:mm in time_zone
    int weekdays_mask = 31;                       ///< bit0 = Monday … bit6 = Sunday
    bool enabled = true;
    bool paused_for_review = false; ///< stopped after a failure / budget stop until the user looks
    qint64 last_run_at_ms = 0;
    qint64 next_run_at_ms = 0;
    qint64 created_at_ms = 0;
    qint64 updated_at_ms = 0;
    // ── WP6 (migration v063) ──
    QString time_zone;          ///< IANA id the times are in; empty = the computer's zone
    qint64 once_at_ms = 0;      ///< kind "once": when (epoch ms)
    int catch_up_minutes = 720; ///< an occurrence missed by less than this runs once at start (0 = never)
    QString last_run_id;        ///< the run the last occurrence started
    QString last_status;        ///< "started" | "skipped_overlap" | "skipped_cap" | "missed" | a run status id
    QString last_error;         ///< why the last occurrence did not run / failed
};

// ── Ids and labels ───────────────────────────────────────────────────────────

QString run_status_id(RunStatus s);
std::optional<RunStatus> run_status_from_id(const QString& id);
/// Short UI label ("Running", "Waiting for input"…), translated.
QString run_status_label(RunStatus s);

QString step_status_id(StepStatus s);
std::optional<StepStatus> step_status_from_id(const QString& id);
QString step_status_label(StepStatus s);

QString event_kind_id(RunEventKind k);
std::optional<RunEventKind> event_kind_from_id(const QString& id);

/// The executor that runs (engine, plan_first): cloud → cloud_turn / cloud_plan, local → local.
/// Empty for an unknown engine.
QString executor_for(const QString& engine, bool plan_first);

bool is_unattended_origin(const QString& origin);

// ── State machine ────────────────────────────────────────────────────────────

/// Completed, Failed, Cancelled, BudgetStopped, Interrupted.
bool is_terminal(RunStatus s);
/// Queued, Planning, Running: the run is moving (or about to).
bool is_active(RunStatus s);
/// Planning, Running: the run occupies one of the kMaxConcurrentRuns slots.
bool holds_slot(RunStatus s);
/// WaitingInput, WaitingApproval: the user has to act.
bool needs_attention(RunStatus s);
/// Whether a run may move from `from` to `to`. Staying in the same state is allowed.
bool can_transition(RunStatus from, RunStatus to);
/// resume_run() is allowed from these (Paused, Interrupted).
bool can_resume(RunStatus s);
/// The state an unfinished run is given when the app starts again: Queued /
/// Planning / Running / WaitingApproval → Interrupted; everything else unchanged.
RunStatus status_after_restart(RunStatus s);

// ── Budget ───────────────────────────────────────────────────────────────────

struct BudgetVerdict {
    bool exceeded = false;
    QString reason; ///< plain-language, for the run's error / transcript
};

/// Is `run` over its budget? `steps_started` counts steps that have run or are
/// running; max_minutes is measured against run_active_ms(run, now_ms).
BudgetVerdict check_budget(const TaskRun& run, int steps_started, qint64 now_ms);

/// Credits a cloud plan run is estimated to cost before it starts: (steps + 2) × 5.
double estimate_cloud_plan_credits(int steps);

/// What a run may cost before it starts (the NEW RUN form, the composer's "Run as
/// task", schedules): the most the budget lets it spend.
struct RunCostEstimate {
    double credits = 0;      ///< upper estimate in Fincept credits (0 for the local engine)
    int steps = 0;           ///< steps the plan can have under the budget (0 for one reply)
    bool unlimited = false;  ///< no credit limit: the estimate assumes the hard step cap
    bool uses_credits = true;
    bool needs_confirmation = false; ///< above kConfirmAboveCredits: ask before starting
};
/// `executor_id` (kExecutor*), `budget` as entered; `hard_max_steps` is the plan
/// cap used when the budget sets none (run_plan_prompts::kPlanHardMaxSteps).
RunCostEstimate estimate_run_cost(const QString& executor_id, const RunBudget& budget, int hard_max_steps);

// ── Working time ─────────────────────────────────────────────────────────────

/// Time the run spent working (Planning / Running) up to `now_ms`: pauses, waits
/// for the user and the time the terminal was closed do not count. Runs recorded
/// before the clock existed fall back to started → finished (or → the moment
/// they stopped moving, never to now while they stand still).
qint64 run_active_ms(const TaskRun& run, qint64 now_ms);
/// Update the working clock for a move of `run` to `to` at `now_ms` (call before
/// changing run.status): leaving Planning / Running banks the stretch, entering
/// it starts one.
void account_active_time(TaskRun& run, RunStatus to, qint64 now_ms);

// ── JSON ─────────────────────────────────────────────────────────────────────

QJsonObject run_request_to_json(const RunRequest& r);
RunRequest run_request_from_json(const QJsonObject& o);

QJsonArray steps_to_plan_json(const QVector<RunStep>& steps);
QVector<RunStep> steps_from_plan_json(const QString& run_id, const QJsonArray& plan);

QJsonObject run_event_to_json(const RunEvent& e);
RunEvent run_event_from_json(const QJsonObject& o);

} // namespace fincept::agent_tasks

Q_DECLARE_METATYPE(fincept::agent_tasks::RunStatus)
Q_DECLARE_METATYPE(fincept::agent_tasks::StepStatus)
Q_DECLARE_METATYPE(fincept::agent_tasks::RunEvent)
Q_DECLARE_METATYPE(fincept::agent_tasks::RunStep)
Q_DECLARE_METATYPE(fincept::agent_tasks::RunArtefact)
