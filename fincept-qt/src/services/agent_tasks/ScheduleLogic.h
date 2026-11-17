#pragma once
// ScheduleLogic.h — the pure rules of scheduled runs (WP6): when the next
// occurrence of a schedule is (once / daily on chosen weekdays / weekly / every
// N hours, in an explicit time zone, across DST changes), what to do with an
// occurrence the terminal missed, when an occurrence must be skipped (its
// previous run still going, the day's credit cap spent), relative date ranges
// for template parameters, and plain descriptions of a schedule.
//
// Qt Core only (unit-tested by tests/tst_run_scheduling). TaskRunScheduler
// owns the timer, the persistence and the runs.

#include "services/agent_tasks/TaskRunTypes.h"

#include <QDate>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QTimeZone>

#include <optional>

namespace fincept::agent_tasks::schedule_logic {

inline constexpr int kMinIntervalMinutes = 60;          ///< "every N hours": at least hourly
inline constexpr int kMaxIntervalMinutes = 7 * 24 * 60; ///< … and at least weekly
inline constexpr int kMaxCatchUpMinutes = 24 * 60;      ///< a missed occurrence older than a day never runs
inline constexpr qint64 kOnTimeSlackMs = 2LL * 60 * 1000; ///< a timer firing this late still counts as on time
inline constexpr double kDefaultDailyCreditCap = 100.0; ///< credits all scheduled runs of a day may spend

/// The zone a schedule's times are in (its IANA id; the computer's zone when empty or unknown).
QTimeZone schedule_zone(const RunSchedule& s);
/// "Europe/London (UTC+01:00)" — the zone as shown next to every time.
QString zone_label(const QTimeZone& zone, qint64 at_ms);

/// "HH:mm" → minutes after midnight; -1 when malformed.
int parse_daily_time(const QString& hhmm);

/// The first occurrence of `s` strictly after `after_ms` (epoch ms); 0 when it has
/// none (a once-schedule already past, an empty weekday mask, a malformed time).
/// Daily / weekly times are wall-clock times in the schedule's zone: on the day
/// clocks spring forward a time inside the gap runs at the same distance after
/// the gap (02:30 → 03:30); on the day they fall back it runs once, at its first
/// occurrence. Intervals are absolute (every N hours of real time), counted from
/// the last run (or from the creation), on the mask's weekdays.
/// Does not look at `enabled` / `paused_for_review`.
qint64 next_occurrence_ms(const RunSchedule& s, qint64 after_ms);

/// What to do when the scheduler looks at a schedule at `now_ms`.
struct DueDecision {
    bool fire = false;        ///< start a run now
    bool caught_up = false;   ///< …for an occurrence the terminal missed (within catch_up_minutes)
    bool missed = false;      ///< an occurrence was missed by more than the window: not run
    qint64 missed_at_ms = 0;  ///< the (latest) missed occurrence
    qint64 next_ms = 0;       ///< the next occurrence after this decision
};
/// `s.next_run_at_ms` is the occurrence that was due. Not yet due → no change.
/// Due on time (or late by up to kOnTimeSlackMs) → fire. Missed by less than the
/// catch-up window → fire once (however many occurrences were missed). Older →
/// skip to the next occurrence after now. Disabled / paused / no occurrence → nothing.
DueDecision decide_due(const RunSchedule& s, qint64 now_ms);

/// Why the occurrence may not start although it is due; empty when it may.
/// `previous_status`: the status of the schedule's last run (nullopt when it has
/// none or it no longer exists). A run still going (not finished) blocks it.
QString overlap_reason(const std::optional<RunStatus>& previous_status);

// ── Per engine ───────────────────────────────────────────────────────────────

/// A run of `engine` needs a Fincept sign-in to start: the cloud agent works on
/// Fincept's servers; "local" (the user's own model) and "harness" (Claude Code /
/// Codex on the user's own API key) do not. Unknown engines count as cloud.
bool engine_needs_fincept_login(const QString& engine);
/// Runs of `engine` spend Fincept credits and count toward the daily cap: the
/// cloud engines only.
bool engine_spends_credits(const QString& engine);
/// Why a due occurrence of a schedule with `engine` must wait now; empty when it
/// may start. `signed_in`: a Fincept session exists. (Scheduled runs are paper
/// only on every engine: TerminalToolBridge / LocalAgenticExecutor for unattended
/// runs, HarnessRunExecutor for all of its runs.)
QString start_blocker(const QString& engine, bool signed_in);

/// The day's credit cap across scheduled runs.
struct CapDecision {
    bool allowed = true;
    double budget = 0;  ///< the run's credit budget after the cap (<= what it asked for)
    QString reason;     ///< why not, or why the budget was lowered
};
/// `cap` <= 0 = no cap. `committed_today`: credits the day's scheduled runs spent
/// or still may spend. `wanted`: the run's own credit budget (0 = unlimited);
/// `minimum`: the least a useful run needs (one turn, or a one-step plan).
CapDecision apply_daily_cap(double cap, double committed_today, double wanted, double minimum);

/// The schedule's problems as plain sentences; empty when it can be saved.
QStringList validate_schedule(const RunSchedule& s, qint64 now_ms);

/// "Weekdays at 08:00", "Every day at 18:30", "Mondays at 07:00", "Every 4 hours
/// (weekdays)", "Once on 20 Nov 2026 09:00" — in the schedule's own zone.
QString describe_recurrence(const RunSchedule& s);

// ── Relative date ranges (template parameters) ───────────────────────────────

/// date_range codes a template parameter stores: "1d" (yesterday / today), "7d",
/// "30d", "90d", "ytd", "1y" or "custom" (date_from / date_to as given).
QStringList date_range_codes();
QString date_range_label(const QString& code);
/// `context` with date_from / date_to (yyyy-MM-dd) and period (a readable phrase)
/// filled from its date_range code relative to `today` — a schedule asks for
/// "the last 7 days" of the day it runs, not of the day it was saved.
QJsonObject expand_relative_dates(const QJsonObject& context, const QDate& today);

} // namespace fincept::agent_tasks::schedule_logic
