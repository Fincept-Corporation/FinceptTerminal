// ScheduleLogic.cpp — occurrences, missed-run / overlap / credit-cap rules and
// relative dates of scheduled runs. Qt Core only (tests/tst_run_scheduling).

#include "services/agent_tasks/ScheduleLogic.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QLocale>
#include <QTime>

#include <algorithm>

namespace fincept::agent_tasks::schedule_logic {

namespace schedule_logic_detail {

QString sl_tr(const char* text) {
    return QCoreApplication::translate("fincept::agent_tasks::Schedules", text);
}

bool sl_day_allowed(int mask, const QDate& d) {
    return d.isValid() && (mask & (1 << (d.dayOfWeek() - 1))) != 0;
}

QString sl_days(int mask) {
    mask &= 0x7f;
    if (mask == 0x7f)
        return sl_tr("every day");
    if (mask == 0x1f)
        return sl_tr("weekdays");
    if (mask == 0x60)
        return sl_tr("weekends");
    QStringList names;
    const QLocale loc;
    for (int i = 0; i < 7; ++i)
        if (mask & (1 << i))
            names << loc.dayName(i + 1, QLocale::ShortFormat);
    return names.join(QStringLiteral(", "));
}

QString sl_two(int n) {
    return QStringLiteral("%1").arg(n, 2, 10, QLatin1Char('0'));
}

} // namespace schedule_logic_detail

using namespace schedule_logic_detail;

QTimeZone schedule_zone(const RunSchedule& s) {
    if (!s.time_zone.trimmed().isEmpty()) {
        const QTimeZone tz(s.time_zone.trimmed().toUtf8());
        if (tz.isValid())
            return tz;
    }
    return QTimeZone::systemTimeZone();
}

QString zone_label(const QTimeZone& zone, qint64 at_ms) {
    const QDateTime at = QDateTime::fromMSecsSinceEpoch(at_ms, QTimeZone::UTC);
    const int off = zone.offsetFromUtc(at) / 60;
    const QString sign = off < 0 ? QStringLiteral("-") : QStringLiteral("+");
    const int a = std::abs(off);
    QString id = QString::fromUtf8(zone.id());
    if (id.isEmpty())
        id = sl_tr("local time");
    return QStringLiteral("%1 (UTC%2%3:%4)").arg(id, sign, sl_two(a / 60), sl_two(a % 60));
}

int parse_daily_time(const QString& hhmm) {
    const QTime t = QTime::fromString(hhmm.trimmed(), QStringLiteral("HH:mm"));
    if (!t.isValid())
        return -1;
    return t.hour() * 60 + t.minute();
}

qint64 next_occurrence_ms(const RunSchedule& s, qint64 after_ms) {
    const QString kind = s.kind;
    if (kind == QLatin1String(kScheduleOnce))
        return s.once_at_ms > after_ms ? s.once_at_ms : 0;

    const QTimeZone tz = schedule_zone(s);
    const int mask = s.weekdays_mask & 0x7f;
    if (mask == 0)
        return 0;

    if (kind == QLatin1String(kScheduleInterval)) {
        const qint64 step = qint64(std::clamp(s.interval_minutes, kMinIntervalMinutes, kMaxIntervalMinutes)) * 60 * 1000;
        const qint64 anchor = s.last_run_at_ms > 0 ? s.last_run_at_ms : (s.created_at_ms > 0 ? s.created_at_ms : after_ms);
        qint64 next = anchor;
        if (next <= after_ms)
            next = anchor + ((after_ms - anchor) / step + 1) * step;
        for (int guard = 0; guard < 4000; ++guard, next += step) {
            const QDate day = QDateTime::fromMSecsSinceEpoch(next, tz).date();
            if (sl_day_allowed(mask, day))
                return next;
        }
        return 0;
    }

    // daily / weekly: a wall-clock time on the mask's weekdays.
    const int minutes = parse_daily_time(s.daily_time);
    if (minutes < 0)
        return 0;
    const QTime at(minutes / 60, minutes % 60);
    QDate day = QDateTime::fromMSecsSinceEpoch(after_ms, tz).date().addDays(-1); // a gap shift can move a time past midnight
    for (int i = 0; i < 10; ++i, day = day.addDays(1)) {
        if (!sl_day_allowed(mask, day))
            continue;
        const QDateTime cand(day, at, tz, QDateTime::TransitionResolution::RelativeToBefore);
        if (!cand.isValid())
            continue;
        const qint64 ms = cand.toMSecsSinceEpoch();
        if (ms > after_ms)
            return ms;
    }
    return 0;
}

DueDecision decide_due(const RunSchedule& s, qint64 now_ms) {
    DueDecision d;
    d.next_ms = s.next_run_at_ms;
    if (!s.enabled || s.paused_for_review)
        return d;
    if (s.next_run_at_ms <= 0) {
        d.next_ms = next_occurrence_ms(s, now_ms);
        return d;
    }
    if (s.next_run_at_ms > now_ms)
        return d;
    // Several occurrences may have passed (the terminal was closed): the latest counts.
    qint64 latest = s.next_run_at_ms;
    for (int guard = 0; guard < 20000; ++guard) {
        const qint64 n = next_occurrence_ms(s, latest);
        if (n <= 0 || n > now_ms)
            break;
        latest = n;
    }
    const qint64 late = now_ms - latest;
    const qint64 window = qint64(std::clamp(s.catch_up_minutes, 0, kMaxCatchUpMinutes)) * 60 * 1000;
    if (late <= kOnTimeSlackMs) {
        d.fire = true;
    } else if (late <= window) {
        d.fire = true;
        d.caught_up = true;
    } else {
        d.missed = true;
        d.missed_at_ms = latest;
    }
    d.next_ms = next_occurrence_ms(s, now_ms);
    return d;
}

QString overlap_reason(const std::optional<RunStatus>& previous_status) {
    if (!previous_status || is_terminal(*previous_status))
        return {};
    return sl_tr("Skipped: the previous run of this schedule is still %1.").arg(run_status_label(*previous_status).toLower());
}

bool engine_needs_fincept_login(const QString& engine) {
    // "harness": agent_tasks::kEngineHarness (HarnessRunExecutor.h).
    return engine != QLatin1String(kEngineLocal) && engine != QLatin1String("harness");
}

bool engine_spends_credits(const QString& engine) {
    return engine_needs_fincept_login(engine); // the same set today: the Fincept cloud engines
}

QString start_blocker(const QString& engine, bool signed_in) {
    if (engine_needs_fincept_login(engine) && !signed_in)
        return sl_tr("Waiting for you to sign in to Fincept: the cloud agent needs your account.");
    return {};
}

CapDecision apply_daily_cap(double cap, double committed_today, double wanted, double minimum) {
    CapDecision c;
    c.budget = wanted;
    if (cap <= 0)
        return c;
    const double remaining = cap - std::max(0.0, committed_today);
    if (remaining + 1e-9 < std::max(0.0, minimum)) {
        c.allowed = false;
        c.budget = 0;
        c.reason = sl_tr("Skipped: scheduled runs may spend %1 credits a day and %2 are already spent or reserved today.")
                       .arg(QLocale().toString(cap, 'f', 0), QLocale().toString(std::max(0.0, committed_today), 'f', 0));
        return c;
    }
    const double budget = wanted <= 0 ? remaining : std::min(wanted, remaining);
    if (wanted <= 0 || budget + 1e-9 < wanted)
        c.reason = sl_tr("Credit budget limited to %1 by the daily cap of scheduled runs.")
                       .arg(QLocale().toString(budget, 'f', 0));
    c.budget = budget;
    return c;
}

QStringList validate_schedule(const RunSchedule& s, qint64 now_ms) {
    QStringList errors;
    if (s.name.trimmed().isEmpty())
        errors << sl_tr("Give the schedule a name.");
    if (s.prompt.trimmed().isEmpty() && s.request.prompt.trimmed().isEmpty() && s.template_id.isEmpty())
        errors << sl_tr("Choose a template or write the task.");
    if (!s.time_zone.trimmed().isEmpty() && !QTimeZone(s.time_zone.trimmed().toUtf8()).isValid())
        errors << sl_tr("The time zone %1 is not known on this computer.").arg(s.time_zone);
    const QString k = s.kind;
    if (k == QLatin1String(kScheduleOnce)) {
        if (s.once_at_ms <= now_ms)
            errors << sl_tr("Pick a date and time in the future.");
    } else if (k == QLatin1String(kScheduleDaily) || k == QLatin1String(kScheduleWeekly)) {
        if (parse_daily_time(s.daily_time) < 0)
            errors << sl_tr("Enter the time as HH:mm, for example 08:30.");
        const int mask = s.weekdays_mask & 0x7f;
        if (mask == 0)
            errors << sl_tr("Pick at least one day of the week.");
        else if (k == QLatin1String(kScheduleWeekly) && (mask & (mask - 1)) != 0)
            errors << sl_tr("A weekly schedule runs on one day: pick exactly one.");
    } else if (k == QLatin1String(kScheduleInterval)) {
        if (s.interval_minutes < kMinIntervalMinutes)
            errors << sl_tr("Runs can repeat at most every hour.");
        else if (s.interval_minutes > kMaxIntervalMinutes)
            errors << sl_tr("Repeat at least once a week, or use a weekly schedule.");
        if ((s.weekdays_mask & 0x7f) == 0)
            errors << sl_tr("Pick at least one day of the week.");
    } else {
        errors << sl_tr("Choose how often the schedule runs.");
    }
    if (s.max_credits < 0)
        errors << sl_tr("The credit budget cannot be negative.");
    return errors;
}

QString describe_recurrence(const RunSchedule& s) {
    const QString k = s.kind;
    if (k == QLatin1String(kScheduleOnce)) {
        if (s.once_at_ms <= 0)
            return sl_tr("Once (no time set)");
        const QDateTime at = QDateTime::fromMSecsSinceEpoch(s.once_at_ms, schedule_zone(s));
        return sl_tr("Once on %1").arg(QLocale().toString(at, QLocale::ShortFormat));
    }
    if (k == QLatin1String(kScheduleInterval)) {
        const int h = std::max(1, s.interval_minutes / 60);
        const QString every = h == 1 ? sl_tr("Every hour") : sl_tr("Every %1 hours").arg(h);
        return (s.weekdays_mask & 0x7f) == 0x7f ? every : sl_tr("%1 (%2)").arg(every, sl_days(s.weekdays_mask));
    }
    const QString days = sl_days(s.weekdays_mask);
    const QString when = s.daily_time.trimmed();
    if (k == QLatin1String(kScheduleWeekly))
        return sl_tr("Every %1 at %2").arg(days, when);
    QString d = days;
    if (!d.isEmpty())
        d[0] = d[0].toUpper();
    return sl_tr("%1 at %2").arg(d, when);
}

// ── Relative date ranges ─────────────────────────────────────────────────────

QStringList date_range_codes() {
    return {QStringLiteral("1d"), QStringLiteral("7d"), QStringLiteral("30d"), QStringLiteral("90d"),
            QStringLiteral("ytd"), QStringLiteral("1y"), QStringLiteral("custom")};
}

QString date_range_label(const QString& code) {
    if (code == QLatin1String("1d"))
        return sl_tr("Last day");
    if (code == QLatin1String("7d"))
        return sl_tr("Last 7 days");
    if (code == QLatin1String("30d"))
        return sl_tr("Last 30 days");
    if (code == QLatin1String("90d"))
        return sl_tr("Last quarter (90 days)");
    if (code == QLatin1String("ytd"))
        return sl_tr("Year to date");
    if (code == QLatin1String("1y"))
        return sl_tr("Last 12 months");
    return sl_tr("Custom dates");
}

QJsonObject expand_relative_dates(const QJsonObject& context, const QDate& today) {
    QJsonObject out = context;
    const QString code = context.value(QStringLiteral("date_range")).toString();
    QDate from;
    QDate to = today;
    if (code == QLatin1String("1d"))
        from = today.addDays(-1);
    else if (code == QLatin1String("7d"))
        from = today.addDays(-7);
    else if (code == QLatin1String("30d"))
        from = today.addDays(-30);
    else if (code == QLatin1String("90d"))
        from = today.addDays(-90);
    else if (code == QLatin1String("ytd"))
        from = QDate(today.year(), 1, 1);
    else if (code == QLatin1String("1y"))
        from = today.addYears(-1);
    if (from.isValid()) {
        out[QStringLiteral("date_from")] = from.toString(Qt::ISODate);
        out[QStringLiteral("date_to")] = to.toString(Qt::ISODate);
    }
    const QString f = out.value(QStringLiteral("date_from")).toString();
    const QString t = out.value(QStringLiteral("date_to")).toString();
    if (!f.isEmpty() && !t.isEmpty()) {
        const QString label = code.isEmpty() || code == QLatin1String("custom") ? sl_tr("the period") : date_range_label(code).toLower();
        out[QStringLiteral("period")] = sl_tr("%1 (%2 to %3)").arg(label, f, t);
    }
    return out;
}

} // namespace fincept::agent_tasks::schedule_logic
