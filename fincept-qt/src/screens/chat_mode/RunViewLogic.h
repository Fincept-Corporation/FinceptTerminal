#pragma once
// RunViewLogic.h — the pure logic behind Chat Mode's run views (WP2): status →
// tone / pill text, the RUNS list's filter, search and order, run links (a run id
// or its 8-character prefix → the run), which actions a
// run allows (and why not), step progress and budget lines, the ACTIVITY
// timeline's event pairing and collapsing, and the composer's prompt with its
// context chips and text attachment inlined, and the "Harness" engine choice.
//
// Qt Core only and header-only on purpose: the Chat Mode widgets share it and
// tests/tst_chat_run_views tests it linking nothing but Qt Core and
// TaskRunTypes.cpp. No widgets, no theme, no singletons in here.

#include "services/agent_tasks/TaskRunTypes.h"
#include "services/llm/ModelRouter.h" // route types only (RouteResult / ModelChoice), no singleton use

#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QVector>

#include <algorithm>
#include <utility>

namespace fincept::chat_mode::run_view {

/// RunRequest::engine and executor id of agent_tasks::HarnessRunExecutor
/// (agent_tasks::kEngineHarness / kExecutorHarness, "harness").
inline constexpr const char* kHarnessEngineId = "harness";

inline QString rv_tr(const char* text, const char* disambiguation = nullptr, int n = -1) {
    return QCoreApplication::translate("fincept::chat_mode::RunView", text, disambiguation, n);
}

// ── Tones ────────────────────────────────────────────────────────────────────

/// The colour role of a state. The widgets map it to ui::colors INFO / SUCCESS /
/// WARNING / DANGER (never POSITIVE / NEGATIVE: those are price direction).
enum class Tone { Neutral, Info, Success, Warning, Danger };

inline Tone run_status_tone(agent_tasks::RunStatus s) {
    using S = agent_tasks::RunStatus;
    switch (s) {
        case S::Planning:
        case S::Running:
            return Tone::Info;
        case S::WaitingInput:
        case S::WaitingApproval:
        case S::BudgetStopped:
        case S::Interrupted:
            return Tone::Warning;
        case S::Completed:
            return Tone::Success;
        case S::Failed:
            return Tone::Danger;
        case S::Queued:
        case S::Paused:
        case S::Cancelled:
            return Tone::Neutral;
    }
    return Tone::Neutral;
}

inline Tone step_status_tone(agent_tasks::StepStatus s) {
    using S = agent_tasks::StepStatus;
    switch (s) {
        case S::Running:
            return Tone::Info;
        case S::Done:
            return Tone::Success;
        case S::Failed:
            return Tone::Danger;
        case S::Pending:
        case S::Skipped:
            return Tone::Neutral;
    }
    return Tone::Neutral;
}

/// Short upper-case text for a status pill (the full label is run_status_label()).
inline QString run_status_pill(agent_tasks::RunStatus s) {
    using S = agent_tasks::RunStatus;
    switch (s) {
        case S::Queued:
            return rv_tr("QUEUED");
        case S::Planning:
            return rv_tr("PLANNING");
        case S::Running:
            return rv_tr("RUNNING");
        case S::WaitingInput:
            return rv_tr("NEEDS INPUT");
        case S::WaitingApproval:
            return rv_tr("NEEDS APPROVAL");
        case S::Paused:
            return rv_tr("PAUSED");
        case S::Completed:
            return rv_tr("COMPLETED");
        case S::Failed:
            return rv_tr("FAILED");
        case S::Cancelled:
            return rv_tr("CANCELLED");
        case S::BudgetStopped:
            return rv_tr("BUDGET STOP");
        case S::Interrupted:
            return rv_tr("INTERRUPTED");
    }
    return rv_tr("FAILED");
}

inline QString step_status_pill(agent_tasks::StepStatus s) {
    return agent_tasks::step_status_label(s).toUpper();
}

// ── Numbers and times ────────────────────────────────────────────────────────

/// "42s", "3m 05s", "2h 07m", "3d 04h".
inline QString format_duration(qint64 ms) {
    const qint64 s = std::max<qint64>(0, ms) / 1000;
    if (s < 60)
        return rv_tr("%1s").arg(s);
    if (s < 3600)
        return rv_tr("%1m %2s").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
    if (s < 86400)
        return rv_tr("%1h %2m").arg(s / 3600).arg((s % 3600) / 60, 2, 10, QLatin1Char('0'));
    return rv_tr("%1d %2h").arg(s / 86400).arg((s % 86400) / 3600, 2, 10, QLatin1Char('0'));
}

/// Credits with up to two decimals, trailing zeros trimmed, thousands grouped
/// (same rule as auth::format_credits, without the auth dependency).
inline QString format_credits(double credits) {
    QString s = QLocale(QLocale::English).toString(credits, 'f', 2);
    if (s.contains(QLatin1Char('.'))) {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(QLatin1Char('.')))
            s.chop(1);
    }
    if (s == QLatin1String("-0"))
        s = QStringLiteral("0");
    return s;
}

/// The time the run has been working (Planning / Running) — pauses, questions to
/// the user, the queue and the time the terminal was closed do not count, so a
/// paused run's clock stands still. 0 before it started.
inline qint64 run_elapsed_ms(const agent_tasks::TaskRun& r, qint64 now_ms) {
    return agent_tasks::run_active_ms(r, now_ms);
}

/// Wall time of one step (running: up to now); -1 when it has no duration to show
/// — a step that has not run, or one a restart put back to pending.
inline qint64 step_elapsed_ms(const agent_tasks::RunStep& s, qint64 now_ms) {
    if (s.started_at_ms <= 0 || s.status == agent_tasks::StepStatus::Pending ||
        s.status == agent_tasks::StepStatus::Skipped)
        return -1;
    const qint64 end = s.finished_at_ms > 0 ? s.finished_at_ms : now_ms;
    return std::max<qint64>(0, end - s.started_at_ms);
}

// ── RUNS list: filter, search, order ─────────────────────────────────────────

enum class RunFilter { All, Active, Attention, Finished };
enum class RunSort { Recent, Status };

/// Active = not finished (queued, planning, running, waiting, paused); Finished =
/// is_terminal(); Attention = needs_attention() (the toolbar badge counts the same).
inline bool run_matches_filter(agent_tasks::RunStatus s, RunFilter f) {
    switch (f) {
        case RunFilter::All:
            return true;
        case RunFilter::Active:
            return !agent_tasks::is_terminal(s);
        case RunFilter::Attention:
            return agent_tasks::needs_attention(s);
        case RunFilter::Finished:
            return agent_tasks::is_terminal(s);
    }
    return true;
}

/// Every whitespace-separated term of `query` occurs (case-insensitively) in the
/// title or the beginning of the prompt. An empty query matches everything.
inline bool run_matches_search(const agent_tasks::TaskRun& r, const QString& query) {
    constexpr qsizetype kSearchPromptChars = 4000; // a 48 000-character prompt is not scanned per keystroke
    const QStringList terms = query.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QStringView prompt = QStringView(r.prompt).left(kSearchPromptChars);
    for (const QString& t : terms) {
        if (!r.title.contains(t, Qt::CaseInsensitive) && !prompt.contains(QStringView(t), Qt::CaseInsensitive))
            return false;
    }
    return true;
}

/// Shortest run-id prefix a link may use: the 8 characters a note saved from a run
/// keeps in its footer ("agent run “Title” (1a2b3c4d)").
inline constexpr int kRunRefMinPrefix = 8;

/// The run a link names (Notes' OPEN CHAT MODE, a deep link): the full run id, or
/// its first kRunRefMinPrefix+ characters; case-insensitive. Empty when no run
/// matches, or when a prefix matches more than one run (never a guess).
inline QString resolve_run_ref(const QStringList& run_ids, const QString& ref) {
    const QString r = ref.trimmed();
    if (r.isEmpty())
        return {};
    if (run_ids.contains(r))
        return r;
    if (r.size() < kRunRefMinPrefix)
        return {};
    QString found;
    for (const QString& id : run_ids) {
        if (!id.startsWith(r, Qt::CaseInsensitive))
            continue;
        if (!found.isEmpty())
            return {}; // ambiguous
        found = id;
    }
    return found;
}

/// Order of states in the "by status" sort: what needs the user first, then what
/// moves, what waits, what stopped, what ended.
inline int run_status_rank(agent_tasks::RunStatus s) {
    using S = agent_tasks::RunStatus;
    switch (s) {
        case S::WaitingInput:
        case S::WaitingApproval:
            return 0;
        case S::Planning:
        case S::Running:
            return 1;
        case S::Queued:
            return 2;
        case S::Paused:
            return 3;
        case S::Interrupted:
            return 4;
        case S::BudgetStopped:
            return 5;
        case S::Failed:
            return 6;
        case S::Cancelled:
            return 7;
        case S::Completed:
            return 8;
    }
    return 9;
}

/// Strict weak order of the RUNS list: runs needing attention first in both
/// sorts, then (Status) by rank, then newest first; the id breaks ties.
inline bool run_sorts_before(const agent_tasks::TaskRun& a, const agent_tasks::TaskRun& b, RunSort sort) {
    const bool aa = agent_tasks::needs_attention(a.status);
    const bool ba = agent_tasks::needs_attention(b.status);
    if (aa != ba)
        return aa;
    if (sort == RunSort::Status) {
        const int ra = run_status_rank(a.status);
        const int rb = run_status_rank(b.status);
        if (ra != rb)
            return ra < rb;
    }
    if (a.created_at_ms != b.created_at_ms)
        return a.created_at_ms > b.created_at_ms;
    return a.id < b.id;
}

// ── Actions ──────────────────────────────────────────────────────────────────

struct ActionState {
    bool enabled = false;
    QString reason; ///< why it is disabled (tooltip); empty when enabled
};

struct RunActions {
    ActionState pause, resume, cancel, retry, open, remove;
};

/// What the user may do with `r` now. `executor_can_pause`: the run's executor
/// says it can pause it (IRunExecutor::can_pause).
inline RunActions run_actions(const agent_tasks::TaskRun& r, bool executor_can_pause) {
    using S = agent_tasks::RunStatus;
    RunActions a;
    const bool ended = agent_tasks::is_terminal(r.status);

    if (r.status == S::Queued) {
        a.pause.enabled = true; // held back before it starts
    } else if (agent_tasks::holds_slot(r.status)) {
        a.pause.enabled = executor_can_pause;
        if (!executor_can_pause)
            a.pause.reason = r.executor_id() == QLatin1String(agent_tasks::kExecutorCloudTurn)
                                 ? rv_tr("A single reply cannot pause half-way. Cancel stops following it.")
                             : r.engine == QLatin1String(kHarnessEngineId)
                                 ? rv_tr("A harness run (Claude Code, Codex) cannot pause. Cancel stops it.")
                                 : rv_tr("This engine cannot pause the run right now.");
    } else if (r.status == S::Paused) {
        a.pause.reason = rv_tr("The run is already paused.");
    } else if (ended) {
        a.pause.reason = rv_tr("The run has ended.");
    } else {
        a.pause.reason = rv_tr("The run is waiting for you: answer it or cancel it.");
    }

    a.resume.enabled = agent_tasks::can_resume(r.status);
    if (!a.resume.enabled)
        a.resume.reason = rv_tr("Only a paused or interrupted run can be resumed.");

    a.cancel.enabled = !ended;
    if (!a.cancel.enabled)
        a.cancel.reason = rv_tr("The run has already ended.");

    a.retry.enabled = !agent_tasks::is_active(r.status);
    if (!a.retry.enabled)
        a.retry.reason = rv_tr("Pause or cancel the run before retrying it.");

    a.open.enabled = !r.session_uuid.isEmpty();
    if (!a.open.enabled)
        a.open.reason = rv_tr("The run has no conversation yet.");

    a.remove.enabled = ended;
    if (!a.remove.enabled)
        a.remove.reason = rv_tr("Only an ended run can be deleted. Cancel it first.");
    return a;
}

// ── Text ─────────────────────────────────────────────────────────────────────

/// Markdown source as a plain preview for the narrow run panels: heading marks,
/// bold marks, code ticks and table rules are dropped and table rows become
/// "a · b" (the transcript shows the rendered reply).
inline QString markdown_preview(const QString& markdown) {
    QStringList out;
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    out.reserve(lines.size());
    for (const QString& raw : lines) {
        QString t = raw.trimmed();
        if (t.contains(QLatin1Char('-'))) {
            bool rule = true; // "|---|---|" or "---"
            for (const QChar c : std::as_const(t)) {
                if (c != QLatin1Char('|') && c != QLatin1Char('-') && c != QLatin1Char(':') && !c.isSpace()) {
                    rule = false;
                    break;
                }
            }
            if (rule)
                continue;
        }
        int marks = 0;
        while (marks < t.size() && t.at(marks) == QLatin1Char('#'))
            ++marks;
        if (marks > 0 && marks <= 6 && (marks == t.size() || t.at(marks).isSpace()))
            t = t.mid(marks).trimmed();
        t.remove(QStringLiteral("**"));
        t.remove(QLatin1Char('`'));
        if (t.size() > 1 && t.startsWith(QLatin1Char('|'))) {
            t = t.mid(1);
            if (t.endsWith(QLatin1Char('|')))
                t.chop(1);
            QStringList cells = t.split(QLatin1Char('|'));
            for (QString& cell : cells)
                cell = cell.trimmed();
            t = cells.join(QStringLiteral(" · "));
        }
        out << t;
    }
    return out.join(QLatin1Char('\n'));
}

// ── Steps and budget ─────────────────────────────────────────────────────────

struct StepProgress {
    int total = 0;
    int done = 0;     ///< Done or Skipped
    int failed = 0;
    int current = -1; ///< index of the running step, -1 when none runs
};

inline StepProgress step_progress(const QVector<agent_tasks::RunStep>& steps) {
    StepProgress p;
    for (const agent_tasks::RunStep& s : steps) {
        ++p.total;
        if (s.status == agent_tasks::StepStatus::Done || s.status == agent_tasks::StepStatus::Skipped)
            ++p.done;
        else if (s.status == agent_tasks::StepStatus::Failed)
            ++p.failed;
        else if (s.status == agent_tasks::StepStatus::Running && p.current < 0)
            p.current = s.idx;
    }
    return p;
}

/// A run that is one cloud reply (no plan): the inspector shows a compact
/// "single reply" state instead of a one-row step list.
inline bool is_single_reply(const agent_tasks::TaskRun& r, const QVector<agent_tasks::RunStep>& steps) {
    if (r.executor_id() == QLatin1String(agent_tasks::kExecutorCloudTurn))
        return true;
    return !r.plan_first && steps.size() <= 1 && r.engine == QLatin1String(agent_tasks::kEngineCloud);
}

/// "step 2/5" (the running step, else the steps done); empty for a single reply
/// or while there is no plan yet.
inline QString step_progress_text(const agent_tasks::TaskRun& r, const QVector<agent_tasks::RunStep>& steps) {
    if (is_single_reply(r, steps) || steps.isEmpty())
        return {};
    const StepProgress p = step_progress(steps);
    const int shown = p.current >= 0 ? p.current + 1 : p.done + p.failed;
    return rv_tr("step %1/%2").arg(shown).arg(p.total);
}

/// Steps that have run or are running (TaskRunService counts the step budget so).
inline int steps_started(const QVector<agent_tasks::RunStep>& steps) {
    int n = 0;
    for (const agent_tasks::RunStep& s : steps)
        if (s.status != agent_tasks::StepStatus::Pending && s.status != agent_tasks::StepStatus::Skipped)
            ++n;
    return n;
}

/// Runs of `r`'s engine spend Fincept credits: the cloud engines do; "local" (the
/// user's own model) and "harness" (billed to the user's own API key) do not.
inline bool run_uses_credits(const agent_tasks::TaskRun& r) {
    return r.engine != QLatin1String(agent_tasks::kEngineLocal) && r.engine != QLatin1String(kHarnessEngineId);
}

/// `base` ("12.5 credits") with the turns the server charged but never billed to
/// the stream (answers recovered from the stored conversation): "12.5 credits + 1
/// turn not reported"; with nothing reported at all, "credits not reported".
inline QString credits_text(const agent_tasks::TaskRun& r, const QString& base) {
    if (r.credits_unreported <= 0 || !run_uses_credits(r))
        return base;
    if (r.credits_spent <= 0 && r.budget.max_credits <= 0)
        return rv_tr("credits not reported");
    return rv_tr("%1 + %n turn(s) not reported", nullptr, r.credits_unreported).arg(base);
}

/// What the start button of a budget editor says and whether it must ask first
/// (the composer's "Run as task" and the NEW RUN form share it).
struct BudgetConfirmation {
    bool ask = false;  ///< above kConfirmAboveCredits: an inline "start anyway?" step first
    QString summary;   ///< "up to 8 steps · est. up to 50 credits"
    QString question;  ///< the confirmation text (empty when !ask)
};

inline BudgetConfirmation budget_confirmation(const QString& executor_id, const agent_tasks::RunBudget& budget,
                                              int hard_max_steps, double balance = -1) {
    BudgetConfirmation c;
    const agent_tasks::RunCostEstimate e = agent_tasks::estimate_run_cost(executor_id, budget, hard_max_steps);
    if (!e.uses_credits) {
        c.summary = executor_id == QLatin1String(kHarnessEngineId)
                        ? rv_tr("up to %1 steps · no Fincept credits").arg(e.steps)
                        : rv_tr("up to %1 steps · your own model, no credits").arg(e.steps);
        if (budget.max_minutes > 0)
            c.summary += rv_tr(" · %1 min").arg(budget.max_minutes);
        return c;
    }
    if (executor_id == QLatin1String(agent_tasks::kExecutorCloudTurn))
        c.summary = rv_tr("one reply · about %1 credits").arg(format_credits(e.credits));
    else
        c.summary = rv_tr("up to %1 steps · est. up to %2 credits").arg(e.steps).arg(format_credits(e.credits));
    if (budget.max_minutes > 0)
        c.summary += rv_tr(" · %1 min").arg(budget.max_minutes);
    c.ask = e.needs_confirmation;
    if (c.ask) {
        c.question = e.unlimited
                         ? rv_tr("This run has no credit limit and may cost about %1 credits or more.")
                               .arg(format_credits(e.credits))
                         : rv_tr("This run may cost up to %1 credits.").arg(format_credits(e.credits));
        if (balance >= 0)
            c.question += QLatin1Char(' ') + rv_tr("Your balance is %1 credits.").arg(format_credits(balance));
        c.question += QLatin1Char(' ') + rv_tr("Start it?");
    }
    return c;
}

// ── The "Harness" engine choice ──────────────────────────────────────────────

/// What the composer's engine menu and the NEW RUN / schedule form show for the
/// "harness" engine (agent_tasks::HarnessRunExecutor: Claude Code or Codex on the
/// user's own API key, chosen by ModelRouter's route for "chat_mode_harness").
struct HarnessEngineOption {
    bool listed = false;          ///< the harness executor is in this build: the choice is shown
    bool enabled = false;         ///< a harness may run Chat Mode tasks now: the choice can be picked
    ai_chat::ModelChoice choice;  ///< the harness that would run it (enabled)
    QString label;                ///< "Claude Code · sonnet · high" (enabled)
    QString state;                ///< short reason (disabled): "Workbench only", "not set up"
    QString reason;               ///< the whole reason (disabled), shown as the tooltip
};

/// `has_executor`: TaskRunService::has_executor("harness"); `route`: ModelRouter's
/// route for kFeatureChatModeHarness. Enabled when the route holds a harness (the
/// first one runs the task, as HarnessRunExecutor picks it); otherwise disabled
/// with the router's reasons for the harnesses it passed over ("Claude Code:
/// Workbench only — sign in with an API key").
inline HarnessEngineOption harness_engine_option(bool has_executor, const ai_chat::RouteResult& route) {
    HarnessEngineOption o;
    o.listed = has_executor;
    if (!has_executor)
        return o;
    for (const ai_chat::ModelChoice& c : route.chain) {
        if (c.is_harness()) {
            o.enabled = true;
            o.choice = c;
            o.label = c.label.isEmpty() ? c.harness_id : c.label;
            return o;
        }
    }
    QStringList parts;
    for (const ai_chat::SkippedChoice& s : route.skipped) {
        if (s.harness_id.isEmpty())
            continue;
        parts << s.what + QStringLiteral(": ") + s.reason;
        if (o.state.isEmpty()) {
            const qsizetype dash = s.reason.indexOf(QStringLiteral(" — "));
            o.state = dash > 0 ? s.reason.left(dash) : rv_tr("not allowed for tasks yet");
        }
    }
    if (parts.isEmpty()) {
        o.state = rv_tr("not set up");
        o.reason = rv_tr("No agent harness (Claude Code, Codex) may run Chat Mode tasks yet. Connect one with an API "
                         "key in the Agent Workbench, then allow it for Chat Mode tasks in Settings › LLM "
                         "Configuration › Terminal AI.");
    } else {
        o.reason = parts.join(QLatin1Char('\n'));
    }
    return o;
}

struct BudgetLine {
    QString credits; ///< "12.5 / 50 credits" or "12.5 credits"
    QString steps;   ///< "3 / 8 steps", "3 steps"; empty for a single reply
    QString time;    ///< "4m 10s / 30 min"; empty without a time limit
    bool limited = false;          ///< any limit set
    bool stopped = false;          ///< the run ended at its budget
    double credits_fraction = -1;  ///< spent / max (0..1+), -1 without a credit limit
};

inline BudgetLine budget_line(const agent_tasks::TaskRun& r, const QVector<agent_tasks::RunStep>& steps, qint64 now_ms) {
    BudgetLine b;
    const agent_tasks::RunBudget& max = r.budget;
    b.limited = max.max_credits > 0 || max.max_steps > 0 || max.max_minutes > 0;
    b.stopped = r.status == agent_tasks::RunStatus::BudgetStopped;
    if (max.max_credits > 0) {
        b.credits = rv_tr("%1 / %2 credits").arg(format_credits(r.credits_spent), format_credits(max.max_credits));
        b.credits_fraction = r.credits_spent / max.max_credits;
    } else {
        b.credits = rv_tr("%1 credits").arg(format_credits(r.credits_spent));
    }
    b.credits = credits_text(r, b.credits);
    if (!is_single_reply(r, steps)) {
        const int started = steps_started(steps);
        b.steps = max.max_steps > 0 ? rv_tr("%1 / %2 steps").arg(started).arg(max.max_steps)
                                    : rv_tr("%n step(s)", nullptr, started);
    }
    if (max.max_minutes > 0)
        b.time = rv_tr("%1 / %2 min").arg(format_duration(run_elapsed_ms(r, now_ms))).arg(max.max_minutes);
    return b;
}

// ── ACTIVITY: pairing and collapsing ─────────────────────────────────────────

/// One row of the activity timeline. A tool call and its result (same ref_id)
/// share one row, as do a delegation's start and end; a question is marked
/// answered and an approval decided by the events that settle them.
struct ActivityRow {
    agent_tasks::RunEvent ev;  ///< the row's event (a call / delegation start, or any single event)
    bool has_end = false;      ///< the paired end arrived (tool result / delegation end)
    agent_tasks::RunEvent end; ///< …that end event
    qint64 duration_ms = -1;   ///< tool / delegation / step duration when known
    bool verbose = false;      ///< a detail row (data.verbose): shown collapsed to its title
    bool settled = false;      ///< a question that was answered / an approval that was decided
    bool expanded = false;     ///< UI state: shows its whole text
};

/// Whether the row's outcome is a failure (a failed tool, an error, a failed step).
inline bool activity_row_failed(const ActivityRow& r) {
    if (r.has_end)
        return r.end.is_error || !r.end.data.value(QStringLiteral("success")).toBool(!r.end.is_error);
    return r.ev.is_error;
}

/// The colour role of an activity row: open tools / steps / plans are Info, what
/// waits for the user Warning, failures Danger, finished work Success.
inline Tone activity_row_tone(const ActivityRow& r) {
    using K = agent_tasks::RunEventKind;
    switch (r.ev.kind) {
        case K::Plan:
        case K::StepStart:
            return Tone::Info;
        case K::StepEnd: {
            if (r.ev.is_error)
                return Tone::Danger;
            const auto st = agent_tasks::step_status_from_id(r.ev.data.value(QStringLiteral("status")).toString());
            return st ? step_status_tone(*st) : Tone::Success;
        }
        case K::ToolCall:
        case K::Delegation:
            if (!r.has_end)
                return Tone::Info;
            return activity_row_failed(r) ? Tone::Danger : Tone::Success;
        case K::ToolResult:
            return r.ev.is_error || !r.ev.data.value(QStringLiteral("success")).toBool(!r.ev.is_error) ? Tone::Danger
                                                                                                       : Tone::Success;
        case K::Question:
        case K::Approval:
            return r.settled ? Tone::Neutral : Tone::Warning;
        case K::ApprovalDecided: {
            const QString s = r.ev.data.value(QStringLiteral("status")).toString().toLower();
            if (s == QLatin1String("executed") || s == QLatin1String("approved"))
                return Tone::Success;
            if (s == QLatin1String("failed") || s == QLatin1String("error") || r.ev.is_error)
                return Tone::Danger;
            return Tone::Neutral;
        }
        case K::Artefact:
        case K::Final:
            return Tone::Success;
        case K::Error:
            return Tone::Danger;
        case K::Thinking:
        case K::Answer:
        case K::Usage:
        case K::Note:
        case K::Status:
            break;
    }
    return r.ev.is_error ? Tone::Danger : Tone::Neutral;
}

/// Ingests a run's events in seq order and keeps the rows. Append-only: an event
/// either inserts one row at the end or updates an earlier one (its pair), and
/// `Change` says which — the view never rebuilds for a live event.
class ActivityLog {
  public:
    struct Change {
        int inserted = -1; ///< index of a new row at the end, -1 none
        int updated = -1;  ///< index of an earlier row that changed, -1 none
        bool ignored = false; ///< already seen (seq <= last_seq())
    };

    void clear() {
        rows_.clear();
        open_.clear();
        open_question_ = -1;
        last_seq_ = 0;
    }

    void reset(const QVector<agent_tasks::RunEvent>& events) {
        clear();
        rows_.reserve(events.size());
        for (const agent_tasks::RunEvent& e : events)
            append(e);
    }

    Change append(const agent_tasks::RunEvent& ev) {
        using K = agent_tasks::RunEventKind;
        Change c;
        if (ev.seq > 0 && ev.seq <= last_seq_) {
            c.ignored = true; // history and live overlap
            return c;
        }
        if (ev.seq > 0)
            last_seq_ = ev.seq;

        // Ends that fold into their start row.
        if (ev.kind == K::ToolResult && !ev.ref_id.isEmpty()) {
            const int i = take_open(QStringLiteral("tool:") + ev.ref_id);
            if (i >= 0) {
                close_pair(i, ev);
                c.updated = i;
                return c;
            }
        }
        if (ev.kind == K::Delegation && is_delegation_end(ev) && !ev.ref_id.isEmpty()) {
            const int i = take_open(QStringLiteral("delegation:") + ev.ref_id);
            if (i >= 0) {
                close_pair(i, ev);
                c.updated = i;
                return c;
            }
        }

        ActivityRow row;
        row.ev = ev;
        row.verbose = ev.data.value(QStringLiteral("verbose")).toBool();
        const int idx = static_cast<int>(rows_.size());
        switch (ev.kind) {
            case K::ToolCall:
                if (!ev.ref_id.isEmpty())
                    open_.insert(QStringLiteral("tool:") + ev.ref_id, idx);
                break;
            case K::Delegation:
                if (!is_delegation_end(ev) && !ev.ref_id.isEmpty())
                    open_.insert(QStringLiteral("delegation:") + ev.ref_id, idx);
                break;
            case K::Approval:
                if (!ev.ref_id.isEmpty())
                    open_.insert(QStringLiteral("approval:") + ev.ref_id, idx);
                break;
            case K::ApprovalDecided: {
                const int i = ev.ref_id.isEmpty() ? -1 : take_open(QStringLiteral("approval:") + ev.ref_id);
                if (i >= 0) {
                    rows_[i].settled = true;
                    c.updated = i;
                }
                break;
            }
            case K::Question:
                open_question_ = idx;
                break;
            case K::Answer:
                if (open_question_ >= 0 && open_question_ < rows_.size()) {
                    rows_[open_question_].settled = true;
                    c.updated = open_question_;
                }
                open_question_ = -1;
                break;
            case K::StepStart:
                open_.insert(step_key(ev), idx);
                break;
            case K::StepEnd: {
                const int i = take_open(step_key(ev));
                if (i >= 0 && ev.ts_ms > 0 && rows_[i].ev.ts_ms > 0)
                    row.duration_ms = std::max<qint64>(0, ev.ts_ms - rows_[i].ev.ts_ms);
                break;
            }
            default:
                break;
        }
        rows_.append(row);
        c.inserted = idx;
        return c;
    }

    int size() const { return static_cast<int>(rows_.size()); }
    bool isEmpty() const { return rows_.isEmpty(); }
    const ActivityRow& at(int i) const { return rows_.at(i); }
    ActivityRow& row(int i) { return rows_[i]; }
    const QVector<ActivityRow>& rows() const { return rows_; }
    int last_seq() const { return last_seq_; }

    /// Plain text of one row (copy): time, kind, title, body and — for a pair —
    /// the result with its duration.
    static QString row_text(const ActivityRow& r) {
        const QString when =
            r.ev.ts_ms > 0 ? QDateTime::fromMSecsSinceEpoch(r.ev.ts_ms).toString(QStringLiteral("HH:mm:ss")) : QString();
        QString out = QStringLiteral("[%1] %2  %3").arg(when, agent_tasks::event_kind_id(r.ev.kind).toUpper(), r.ev.title);
        if (!r.ev.body.isEmpty())
            out += QLatin1Char('\n') + r.ev.body;
        if (r.has_end) {
            QString head = activity_row_failed(r) ? rv_tr("Failed") : rv_tr("Finished");
            if (r.duration_ms >= 0)
                head += QStringLiteral(" (%1)").arg(format_duration_precise(r.duration_ms));
            out += QStringLiteral("\n→ ") + head;
            if (!r.end.title.isEmpty() && r.end.title != r.ev.title)
                out += QStringLiteral(": ") + r.end.title;
            if (!r.end.body.isEmpty())
                out += QLatin1Char('\n') + r.end.body;
        }
        return out;
    }

    /// "850 ms", "1.2 s", "14 s", then format_duration().
    static QString format_duration_precise(qint64 ms) {
        if (ms < 0)
            return {};
        if (ms < 1000)
            return rv_tr("%1 ms").arg(ms);
        if (ms < 10000)
            return rv_tr("%1 s").arg(QString::number(ms / 1000.0, 'f', 1));
        if (ms < 60000)
            return rv_tr("%1 s").arg(ms / 1000);
        return format_duration(ms);
    }

  private:
    static bool is_delegation_end(const agent_tasks::RunEvent& ev) {
        return ev.data.value(QStringLiteral("phase")).toString() == QLatin1String("end");
    }

    static QString step_key(const agent_tasks::RunEvent& ev) {
        if (ev.data.value(QStringLiteral("sub")).toString() == QLatin1String("window"))
            return QStringLiteral("window:%1").arg(ev.data.value(QStringLiteral("window")).toInt());
        return QStringLiteral("step:%1").arg(ev.step);
    }

    int take_open(const QString& key) {
        const auto it = open_.find(key);
        if (it == open_.end())
            return -1;
        const int i = it.value();
        open_.erase(it);
        return (i >= 0 && i < rows_.size()) ? i : -1;
    }

    void close_pair(int i, const agent_tasks::RunEvent& end) {
        ActivityRow& r = rows_[i];
        r.has_end = true;
        r.end = end;
        const QJsonValue d = end.data.value(QStringLiteral("durationMs"));
        if (d.isDouble() && d.toDouble() >= 0)
            r.duration_ms = static_cast<qint64>(d.toDouble());
        else if (end.ts_ms > 0 && r.ev.ts_ms > 0)
            r.duration_ms = std::max<qint64>(0, end.ts_ms - r.ev.ts_ms);
    }

    QVector<ActivityRow> rows_;
    QHash<QString, int> open_; ///< pairing key -> row index of the open start
    int open_question_ = -1;
    int last_seq_ = 0;
};

// ── Composer prompt ──────────────────────────────────────────────────────────

/// What the composer's context chips hold.
struct ComposerContextData {
    QStringList symbols;
    QString portfolio_id;
    QString portfolio_name;
    int profile_id = 0;
    QString profile_name;
    QString attachment_name;
    QString attachment_text; ///< inlined into the prompt; never part of RunRequest::context
};

struct ComposedPrompt {
    QString text;
    bool attachment_truncated = false;
    int attachment_chars_sent = 0;
};

/// `user_text` followed by the context lines and the attachment, as sent to the
/// agent. The attachment gets at most `max_attachment_chars` and never more than
/// what keeps the whole prompt within `max_total_chars` (TaskRunService refuses
/// longer prompts); a cut attachment ends with a note saying how much is missing.
/// Without context the text is returned unchanged.
inline ComposedPrompt compose_run_prompt(const QString& user_text, const ComposerContextData& c,
                                         int max_attachment_chars = 32000,
                                         int max_total_chars = agent_tasks::kRunPromptMaxChars) {
    ComposedPrompt out;
    QStringList lines;
    if (!c.symbols.isEmpty())
        lines << rv_tr("Symbols: %1").arg(c.symbols.join(QStringLiteral(", ")));
    if (!c.portfolio_name.isEmpty() || !c.portfolio_id.isEmpty())
        lines << rv_tr("Portfolio: %1").arg(c.portfolio_name.isEmpty() ? c.portfolio_id : c.portfolio_name);
    if (!c.profile_name.isEmpty())
        lines << rv_tr("Investment profile: %1").arg(c.profile_name);
    if (lines.isEmpty() && c.attachment_text.isEmpty()) {
        out.text = user_text;
        return out;
    }
    QString text = user_text.trimmed();
    if (!lines.isEmpty())
        text += QStringLiteral("\n\n") + rv_tr("Context from the terminal:") + QStringLiteral("\n- ") +
                lines.join(QStringLiteral("\n- "));
    if (c.attachment_text.isEmpty()) {
        out.text = text;
        return out;
    }
    const QString name = c.attachment_name.trimmed().isEmpty() ? rv_tr("attachment") : c.attachment_name.trimmed();
    const QString open = QStringLiteral("\n\n") + rv_tr("Attached text \"%1\":").arg(name) + QStringLiteral("\n<<<\n");
    const QString close = QStringLiteral("\n>>>");
    const int attachment_size = static_cast<int>(c.attachment_text.size());
    const int room = max_total_chars - static_cast<int>(text.size() + open.size() + close.size());
    int cap = std::min(std::max(0, max_attachment_chars), std::max(0, room));
    QString body;
    if (attachment_size <= cap) {
        body = c.attachment_text;
        out.attachment_chars_sent = attachment_size;
    } else {
        constexpr int kNoteReserve = 96; // the cut note below, with its number
        cap = std::max(0, cap - kNoteReserve);
        if (cap > 0 && c.attachment_text.at(cap - 1).isHighSurrogate())
            --cap; // never split a surrogate pair
        body = c.attachment_text.left(cap) + QLatin1Char('\n') +
               rv_tr("[… %1 more characters of the attachment were not included]")
                   .arg(QLocale(QLocale::English).toString(attachment_size - cap));
        out.attachment_truncated = true;
        out.attachment_chars_sent = cap;
    }
    out.text = text + open + body + close;
    return out;
}

/// A stored user message taken apart again: what the user typed and the context
/// block compose_run_prompt() appended (empty when there is none).
struct SplitPrompt {
    QString text;
    QString context;
};

/// Splits a message built by compose_run_prompt() back into the typed text and
/// the appended context (context lines and / or an attachment). A message
/// without that block — or composed in another interface language — comes back
/// whole in `text`.
inline SplitPrompt split_composed_prompt(const QString& stored) {
    SplitPrompt out;
    out.text = stored;
    const QString ctx_mark = QStringLiteral("\n\n") + rv_tr("Context from the terminal:") + QStringLiteral("\n- ");
    QString att_head = rv_tr("Attached text \"%1\":");
    const int arg_at = static_cast<int>(att_head.indexOf(QStringLiteral("%1")));
    if (arg_at > 0)
        att_head = att_head.left(arg_at);
    const QString att_mark = QStringLiteral("\n\n") + att_head;
    int at = static_cast<int>(stored.indexOf(ctx_mark));
    // An attachment block is recognised by its closing mark too.
    if (stored.endsWith(QStringLiteral("\n>>>"))) {
        const int att = static_cast<int>(stored.indexOf(att_mark));
        if (att >= 0 && stored.indexOf(QStringLiteral("\n<<<\n"), att) > att && (at < 0 || att < at))
            at = att;
    }
    if (at <= 0)
        return out;
    out.text = stored.left(at).trimmed();
    out.context = stored.mid(at).trimmed();
    if (out.text.isEmpty()) { // nothing typed: show the message as it is
        out.text = stored;
        out.context.clear();
    }
    return out;
}

} // namespace fincept::chat_mode::run_view
