#pragma once
// ReportSchedule.h — scheduled regeneration of a report: when it is due, and the
// missed-run rule. Pure, Qt Core only (unit-tested in tests/tst_report_builder_core.cpp).
//
// A schedule belongs to one library report (reports.id) on THIS computer: refresh every
// data block, then export to a chosen folder in the chosen formats. It runs only while the
// terminal is open (the scheduler is in-process — no system task is installed):
//   daily  at HH:mm (local time)
//   weekly on a weekday at HH:mm
//   on start: once each time the terminal starts (at most once every 12 hours)
// Missed-run rule: a scheduled time that passed while the terminal was closed is made up
// ONCE, when the terminal next runs — only the most recent missed time, never a backlog.
// Runs never overlap (the scheduler queues them).
//
// Stored per computer (folder paths are local) in <profile data>/report_schedules.json,
// with the last results; schedules are NOT synced with the report.

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QVector>

#include <algorithm>

namespace fincept::report::schedule {

enum class Frequency { Daily, Weekly, OnStart };

inline QString frequency_id(Frequency f) {
    switch (f) {
        case Frequency::Daily:
            return QStringLiteral("daily");
        case Frequency::Weekly:
            return QStringLiteral("weekly");
        case Frequency::OnStart:
            return QStringLiteral("on_start");
    }
    return QStringLiteral("daily");
}

inline Frequency frequency_from_id(const QString& id) {
    if (id == QLatin1String("weekly"))
        return Frequency::Weekly;
    if (id == QLatin1String("on_start"))
        return Frequency::OnStart;
    return Frequency::Daily;
}

/// Export formats a schedule may produce.
inline const QStringList& known_formats() {
    static const QStringList k = {QStringLiteral("pdf"), QStringLiteral("odt"), QStringLiteral("md"),
                                  QStringLiteral("html")};
    return k;
}

inline constexpr int kOnStartMinGapHours = 12;

struct Schedule {
    qint64 report_id = 0;
    QString title; ///< the report's title when the schedule was saved (display only)
    bool enabled = true;
    Frequency frequency = Frequency::Daily;
    QTime time = QTime(8, 0);
    int weekday = 1; ///< 1 = Monday … 7 = Sunday (weekly)
    QString folder;
    QStringList formats = {QStringLiteral("pdf")};
    bool refresh_data = true;
    QDateTime last_run; ///< when it last ran (UTC); invalid = never

    QJsonObject to_json() const {
        QJsonObject o;
        o.insert(QStringLiteral("report_id"), static_cast<double>(report_id));
        o.insert(QStringLiteral("title"), title);
        o.insert(QStringLiteral("enabled"), enabled);
        o.insert(QStringLiteral("frequency"), frequency_id(frequency));
        o.insert(QStringLiteral("time"), time.toString(QStringLiteral("HH:mm")));
        o.insert(QStringLiteral("weekday"), weekday);
        o.insert(QStringLiteral("folder"), folder);
        o.insert(QStringLiteral("formats"), QJsonArray::fromStringList(formats));
        o.insert(QStringLiteral("refresh_data"), refresh_data);
        if (last_run.isValid())
            o.insert(QStringLiteral("last_run"), last_run.toUTC().toString(Qt::ISODate));
        return o;
    }
    static Schedule from_json(const QJsonObject& o) {
        Schedule s;
        s.report_id = static_cast<qint64>(o.value(QStringLiteral("report_id")).toDouble());
        s.title = o.value(QStringLiteral("title")).toString();
        s.enabled = o.value(QStringLiteral("enabled")).toBool(true);
        s.frequency = frequency_from_id(o.value(QStringLiteral("frequency")).toString());
        const QTime t = QTime::fromString(o.value(QStringLiteral("time")).toString(), QStringLiteral("HH:mm"));
        s.time = t.isValid() ? t : QTime(8, 0);
        s.weekday = std::clamp(o.value(QStringLiteral("weekday")).toInt(1), 1, 7);
        s.folder = o.value(QStringLiteral("folder")).toString();
        s.formats.clear();
        for (const QJsonValue& v : o.value(QStringLiteral("formats")).toArray())
            if (known_formats().contains(v.toString()) && !s.formats.contains(v.toString()))
                s.formats << v.toString();
        if (s.formats.isEmpty())
            s.formats << QStringLiteral("pdf");
        s.refresh_data = o.value(QStringLiteral("refresh_data")).toBool(true);
        s.last_run = QDateTime::fromString(o.value(QStringLiteral("last_run")).toString(), Qt::ISODate);
        return s;
    }
};

/// The most recent scheduled moment at or before `now` (local time). Invalid for on-start.
inline QDateTime last_slot(const Schedule& s, const QDateTime& now) {
    const QDateTime local = now.toLocalTime();
    if (s.frequency == Frequency::OnStart)
        return {};
    QDate d = local.date();
    if (s.frequency == Frequency::Daily) {
        QDateTime slot(d, s.time);
        if (slot > local)
            slot = QDateTime(d.addDays(-1), s.time);
        return slot;
    }
    // Weekly: step back to the weekday.
    int back = (d.dayOfWeek() - s.weekday + 7) % 7;
    QDateTime slot(d.addDays(-back), s.time);
    if (slot > local)
        slot = QDateTime(d.addDays(-back - 7), s.time);
    return slot;
}

/// The next scheduled moment after `now` (local time). Invalid for on-start.
inline QDateTime next_slot(const Schedule& s, const QDateTime& now) {
    const QDateTime last = last_slot(s, now);
    if (!last.isValid())
        return {};
    return s.frequency == Frequency::Daily ? QDateTime(last.date().addDays(1), s.time)
                                           : QDateTime(last.date().addDays(7), s.time);
}

enum class Decision {
    NotDue,
    Due,    ///< its time came while the terminal was running
    Missed, ///< its time passed while the terminal was closed: made up once now
    OnStart ///< the on-start run of this session
};

/// Should `s` run now? `session_start` = when this terminal session started;
/// `ran_this_session` = an on-start schedule already ran in this session.
inline Decision decide(const Schedule& s, const QDateTime& now, const QDateTime& session_start,
                       bool ran_this_session = false) {
    if (!s.enabled || s.report_id <= 0)
        return Decision::NotDue;
    if (s.frequency == Frequency::OnStart) {
        if (ran_this_session)
            return Decision::NotDue;
        if (s.last_run.isValid() && s.last_run.secsTo(now) < kOnStartMinGapHours * 3600)
            return Decision::NotDue;
        return Decision::OnStart;
    }
    const QDateTime slot = last_slot(s, now);
    if (!slot.isValid())
        return Decision::NotDue;
    if (s.last_run.isValid() && s.last_run >= slot)
        return Decision::NotDue; // already ran for this slot
    return slot >= session_start ? Decision::Due : Decision::Missed;
}

/// One run's result, listed in the schedule editor.
struct RunRecord {
    qint64 report_id = 0;
    QString title;
    QString trigger; ///< scheduled | missed | start | manual
    QDateTime started;
    QDateTime finished;
    bool ok = false;
    QString error;
    QStringList files;    ///< written files (absolute paths)
    QStringList warnings; ///< e.g. blocks that could not be refreshed, unresolved variables

    QJsonObject to_json() const {
        QJsonObject o;
        o.insert(QStringLiteral("report_id"), static_cast<double>(report_id));
        o.insert(QStringLiteral("title"), title);
        o.insert(QStringLiteral("trigger"), trigger);
        o.insert(QStringLiteral("started"), started.toUTC().toString(Qt::ISODate));
        o.insert(QStringLiteral("finished"), finished.toUTC().toString(Qt::ISODate));
        o.insert(QStringLiteral("ok"), ok);
        if (!error.isEmpty())
            o.insert(QStringLiteral("error"), error);
        o.insert(QStringLiteral("files"), QJsonArray::fromStringList(files));
        if (!warnings.isEmpty())
            o.insert(QStringLiteral("warnings"), QJsonArray::fromStringList(warnings));
        return o;
    }
    static RunRecord from_json(const QJsonObject& o) {
        RunRecord r;
        r.report_id = static_cast<qint64>(o.value(QStringLiteral("report_id")).toDouble());
        r.title = o.value(QStringLiteral("title")).toString();
        r.trigger = o.value(QStringLiteral("trigger")).toString();
        r.started = QDateTime::fromString(o.value(QStringLiteral("started")).toString(), Qt::ISODate);
        r.finished = QDateTime::fromString(o.value(QStringLiteral("finished")).toString(), Qt::ISODate);
        r.ok = o.value(QStringLiteral("ok")).toBool();
        r.error = o.value(QStringLiteral("error")).toString();
        for (const QJsonValue& v : o.value(QStringLiteral("files")).toArray())
            r.files << v.toString();
        for (const QJsonValue& v : o.value(QStringLiteral("warnings")).toArray())
            r.warnings << v.toString();
        return r;
    }
};

/// A file name from a report title: letters, digits, space, '-', '_' and '.' kept, the
/// rest dropped; never empty; at most 80 characters.
inline QString safe_file_stem(const QString& title) {
    QString out;
    for (QChar ch : title.trimmed()) {
        if (ch.isLetterOrNumber() || ch == QLatin1Char(' ') || ch == QLatin1Char('-') || ch == QLatin1Char('_') ||
            ch == QLatin1Char('.'))
            out += ch;
        else if (ch == QChar(0x2014) || ch == QChar(0x2013) || ch == QLatin1Char('/'))
            out += QLatin1Char('-');
    }
    out = out.simplified();
    // No leading / trailing dots or dashes ("..", "-..", "name." are poor file names).
    while (!out.isEmpty() && (out.front() == QLatin1Char('.') || out.front() == QLatin1Char('-') || out.front() == QLatin1Char(' ')))
        out.remove(0, 1);
    while (!out.isEmpty() && (out.back() == QLatin1Char('.') || out.back() == QLatin1Char('-') || out.back() == QLatin1Char(' ')))
        out.chop(1);
    bool meaningful = false;
    for (QChar ch : out)
        meaningful = meaningful || ch.isLetterOrNumber();
    if (!meaningful)
        out = QStringLiteral("Report");
    return out.left(80).trimmed();
}

/// "<folder>/<title> 2026-11-17.<ext>" — one file per day and format (a second run on the
/// same day replaces it).
inline QString output_path(const QString& folder, const QString& title, const QDate& day, const QString& format) {
    QString f = folder;
    while (f.endsWith(QLatin1Char('/')) || f.endsWith(QLatin1Char('\\')))
        f.chop(1);
    return f + QLatin1Char('/') + safe_file_stem(title) + QLatin1Char(' ') + day.toString(QStringLiteral("yyyy-MM-dd")) +
           QLatin1Char('.') + format;
}

/// One-line human description: "Daily at 08:00", "Mondays at 07:30", "When the terminal starts".
inline QString describe(const Schedule& s) {
    if (s.frequency == Frequency::OnStart)
        return QStringLiteral("When the terminal starts");
    const QString t = s.time.toString(QStringLiteral("HH:mm"));
    if (s.frequency == Frequency::Daily)
        return QStringLiteral("Daily at %1").arg(t);
    static const char* const kDays[] = {"Mondays", "Tuesdays", "Wednesdays", "Thursdays", "Fridays", "Saturdays", "Sundays"};
    return QStringLiteral("%1 at %2").arg(QLatin1String(kDays[std::clamp(s.weekday, 1, 7) - 1]), t);
}

} // namespace fincept::report::schedule
