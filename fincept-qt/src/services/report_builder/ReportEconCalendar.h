#pragma once
// ReportEconCalendar.h — the Report Builder's "Economic calendar" data source, pure part.
//
// Source: the catalogue connector data_economic_calendar (scripts/economic_calendar.py,
// command `day [date]`) run through ConnectorService: Forex Factory's free public weekly
// export. No API key, no Fincept credit, nothing paid. Limits of the feed, said in every
// block it fills: the CURRENT WEEK only, forecast and previous values (no actuals), times
// in this computer's time zone.
//
// Recipe (report::Binding, source "econ_calendar"):
//   date        the anchor day. "" / "today" = the day of the refresh; "{{date}}" = the report's
//               date (expanded by vars::expand_binding before it reaches request_of; an
//               unresolved variable becomes "" = today); or "2026-11-17" / "17 Nov 2026".
//   period      "day" (the anchor) | "week" (its Monday to Sunday) | "rest_of_week" (the anchor
//               to Sunday). Synonyms: today, this week, week ahead.
//   impact      "high" | "medium" (medium and high) | "low" (low to high, no holidays) | "all"
//   currencies  "USD, EUR" (empty = all)
//   limit       at most this many rows (0 = all)
//
// The connector answers one day per call. The binder makes the FIRST call alone (it downloads
// the weekly file; the script caches it for 15 minutes and every later call reads that copy,
// so a week costs one download), then the other days of the feed's week, and hands every
// reply to shape(). Pure: QtCore + header-only report core; unit-tested with recorded
// connector replies in tests/tst_report_econ_calendar.cpp.

#include "core/report/ReportBinding.h"
#include "core/report/ReportTableCodec.h"
#include "services/data_sources/SourceError.h"
#include "services/data_sources/TabularData.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <cstdlib>

namespace fincept::services::econcal {

inline constexpr char kConnector[] = "data_economic_calendar";
inline constexpr char kCommand[] = "day";
inline constexpr int kMaxLimit = 300;
/// A requested day further than this from today cannot be in the feed (current week only):
/// refused before any script runs.
inline constexpr int kReachDays = 7;

inline QString ec_tr(const char* text) {
    return QCoreApplication::translate("fincept::services::ReportDataBinder", text);
}

inline const QString& ec_dash() {
    static const QString d = QStringLiteral("—");
    return d;
}

enum class Period { Day, Week, RestOfWeek };

/// The recipe, read and checked. `days` are the calls to make (in order).
struct Request {
    bool ok = false;
    QString error; ///< !ok: a plain sentence for the block's failed state
    QDate anchor;
    Period period = Period::Week;
    int min_impact = 2; ///< 3 high only, 2 medium and high, 1 low to high, 0 everything (holidays too)
    QStringList currencies;
    int limit = 0;
    QVector<QDate> days;
};

// ── Reading the recipe ───────────────────────────────────────────────────────

/// "", "today", ISO (a date-time keeps its date), "17 Nov 2026", "Nov 17, 2026", "17.11.2026".
/// Invalid when unreadable.
inline QDate parse_day(const QString& text, const QDate& today) {
    const QString t = text.trimmed();
    if (t.isEmpty() || t.compare(QLatin1String("today"), Qt::CaseInsensitive) == 0)
        return today;
    if (t.size() >= 10) {
        const QDate iso = QDate::fromString(t.left(10), Qt::ISODate);
        if (iso.isValid() && (t.size() == 10 || t.at(10) == QLatin1Char('T') || t.at(10) == QLatin1Char(' ')))
            return iso;
    }
    for (const char* f : {"d MMM yyyy", "d MMMM yyyy", "MMM d, yyyy", "MMMM d, yyyy", "ddd d MMM yyyy", "dd.MM.yyyy"}) {
        const QDate d = QDate::fromString(t, QString::fromLatin1(f));
        if (d.isValid())
            return d;
    }
    return {};
}

inline bool parse_period(const QString& text, Period* out) {
    QString t = text.trimmed().toLower();
    t.replace(QLatin1Char(' '), QLatin1Char('_'));
    t.replace(QLatin1Char('-'), QLatin1Char('_'));
    if (t.isEmpty() || t == QLatin1String("week") || t == QLatin1String("this_week")) {
        *out = Period::Week;
        return true;
    }
    if (t == QLatin1String("day") || t == QLatin1String("today") || t == QLatin1String("the_day")) {
        *out = Period::Day;
        return true;
    }
    if (t == QLatin1String("rest_of_week") || t == QLatin1String("week_ahead") || t == QLatin1String("ahead")) {
        *out = Period::RestOfWeek;
        return true;
    }
    return false;
}

/// "high" 3, "medium" (default) 2, "low" 1, "all" 0; -1 when unknown.
inline int parse_impact(const QString& text) {
    const QString t = text.trimmed().toLower();
    if (t.isEmpty() || t == QLatin1String("medium"))
        return 2;
    if (t == QLatin1String("high"))
        return 3;
    if (t == QLatin1String("low"))
        return 1;
    if (t == QLatin1String("all"))
        return 0;
    return -1;
}

/// "usd, eur;gbp" -> {"USD","EUR","GBP"}. False (with the bad item in *bad) on anything that
/// is not a 2-4 letter code ("ALL" is Forex Factory's code for global events).
inline bool parse_currencies(const QString& text, QStringList* out, QString* bad) {
    out->clear();
    QString t = text;
    t.replace(QLatin1Char(';'), QLatin1Char(','));
    t.replace(QLatin1Char(' '), QLatin1Char(','));
    for (const QString& raw : t.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString c = raw.trimmed().toUpper();
        if (c.isEmpty())
            continue;
        bool letters = c.size() >= 2 && c.size() <= 4;
        for (const QChar ch : c)
            letters = letters && ch >= QLatin1Char('A') && ch <= QLatin1Char('Z');
        if (!letters) {
            *bad = raw.trimmed();
            return false;
        }
        if (!out->contains(c))
            *out << c;
    }
    return true;
}

/// The anchor, its week (Monday to Sunday) or the anchor to Sunday.
inline QVector<QDate> days_for(const QDate& anchor, Period p) {
    QVector<QDate> out;
    if (!anchor.isValid())
        return out;
    if (p == Period::Day) {
        out << anchor;
        return out;
    }
    const QDate monday = anchor.addDays(1 - anchor.dayOfWeek());
    const QDate sunday = monday.addDays(6);
    for (QDate d = p == Period::Week ? monday : anchor; d <= sunday; d = d.addDays(1))
        out << d;
    return out;
}

/// "Tue 17 Nov 2026" / "week of 16 Nov 2026" / "17 Nov – 22 Nov 2026".
inline QString range_label(const Request& r) {
    if (r.days.isEmpty())
        return {};
    if (r.period == Period::Day)
        return r.anchor.toString(QStringLiteral("ddd d MMM yyyy"));
    if (r.period == Period::Week)
        return ec_tr("week of %1").arg(r.days.first().toString(QStringLiteral("d MMM yyyy")));
    return QStringLiteral("%1 – %2").arg(r.days.first().toString(QStringLiteral("d MMM")),
                                          r.days.last().toString(QStringLiteral("d MMM yyyy")));
}

/// "on Tue 17 Nov 2026" / "in the week of 16 Nov 2026" / "from 17 Nov to 22 Nov 2026".
inline QString range_phrase(const Request& r) {
    if (r.days.isEmpty())
        return {};
    if (r.period == Period::Day)
        return ec_tr("on %1").arg(r.anchor.toString(QStringLiteral("ddd d MMM yyyy")));
    if (r.period == Period::Week)
        return ec_tr("in the week of %1").arg(r.days.first().toString(QStringLiteral("d MMM yyyy")));
    return ec_tr("from %1 to %2")
        .arg(r.days.first().toString(QStringLiteral("d MMM")), r.days.last().toString(QStringLiteral("d MMM yyyy")));
}

/// The recipe of a block (variables already expanded) against `today`.
inline Request request_of(const report::Binding& b, const QDate& today) {
    Request r;
    const QString date_text = b.param("date");
    r.anchor = parse_day(date_text, today);
    if (!r.anchor.isValid()) {
        r.error = ec_tr("“%1” is not a date the economic calendar understands — use 2026-11-17 or 17 Nov 2026, "
                        "or leave it empty for today")
                      .arg(date_text.trimmed());
        return r;
    }
    if (!parse_period(b.param("period"), &r.period)) {
        r.error = ec_tr("unknown period “%1” — use day, week or rest_of_week").arg(b.param("period").trimmed());
        return r;
    }
    r.min_impact = parse_impact(b.param("impact"));
    if (r.min_impact < 0) {
        r.error = ec_tr("unknown impact level “%1” — use high, medium, low or all").arg(b.param("impact").trimmed());
        return r;
    }
    QString bad;
    if (!parse_currencies(b.param("currencies"), &r.currencies, &bad)) {
        r.error = ec_tr("“%1” is not a currency code — use codes such as USD, EUR, JPY").arg(bad);
        return r;
    }
    r.limit = std::clamp(b.int_param("limit", 0), 0, kMaxLimit);
    r.days = days_for(r.anchor, r.period);
    bool reachable = false;
    for (const QDate& d : r.days)
        reachable = reachable || std::abs(d.daysTo(today)) <= kReachDays;
    if (!reachable) {
        r.error = ec_tr("the free economic calendar (Forex Factory) lists the current week only; this block asks "
                        "for %1 — set its date to a day of this week, or leave it empty for today")
                      .arg(range_label(r));
        return r;
    }
    r.ok = true;
    return r;
}

// ── One connector reply per day ──────────────────────────────────────────────

enum class DayStatus { Ok, Empty, Failed, Cancelled };

struct DayResult {
    QDate day;
    DayStatus status = DayStatus::Failed;
    datasources::ErrorCode code = datasources::ErrorCode::None;
    QString error;            ///< Failed: the connector's plain message
    QStringList columns;      ///< Ok: the events table
    QVector<QJsonArray> rows; ///< Ok: cells in column order
    QDate week_start;         ///< Ok: the days the feed covers (its first and last event day)
    QDate week_end;
    QString timezone;         ///< Ok: "UTC+05:30" (the times' zone, this computer's)
    QDateTime as_of;          ///< Ok: when the feed copy was taken (run time minus its cache age)
};

/// What one ConnectorService reply (or a parsed envelope) says about `day`.
inline DayResult day_from_connector(const QDate& day, bool success, const datasources::SourceError& failure,
                                    const datasources::TabularResult& tabular) {
    namespace ds = datasources;
    DayResult d;
    d.day = day;
    if (!success) {
        d.code = failure.code;
        d.error = failure.message.trimmed();
        d.status = failure.cancelled                       ? DayStatus::Cancelled
                   : failure.code == ds::ErrorCode::NoData ? DayStatus::Empty
                                                           : DayStatus::Failed;
        return d;
    }
    if (tabular.tables.isEmpty() || tabular.tables.first().rows.isEmpty()) {
        d.status = DayStatus::Empty;
        return d;
    }
    const ds::DataTable& t = tabular.tables.first();
    d.columns = t.columns;
    d.rows = t.rows;
    const QJsonObject extra = tabular.meta.value(QStringLiteral("extra")).toObject();
    d.week_start = QDate::fromString(extra.value(QStringLiteral("week_start")).toString(), Qt::ISODate);
    d.week_end = QDate::fromString(extra.value(QStringLiteral("week_end")).toString(), Qt::ISODate);
    d.timezone = extra.value(QStringLiteral("timezone")).toString();
    d.as_of = tabular.as_of.isValid() ? tabular.as_of : QDateTime::currentDateTimeUtc();
    const double age = extra.value(QStringLiteral("feed_age_seconds")).toDouble(0.0);
    if (age > 0)
        d.as_of = d.as_of.addSecs(-static_cast<qint64>(age));
    d.status = DayStatus::Ok;
    return d;
}

/// A failure every other day would repeat (feed down, rate limited, Python missing).
inline bool is_systemic(const DayResult& d) {
    using ds_code = datasources::ErrorCode;
    return d.status == DayStatus::Failed &&
           (d.code == ds_code::UpstreamDown || d.code == ds_code::RateLimited || d.code == ds_code::NotSupported ||
            d.code == ds_code::MissingKey || d.code == ds_code::AuthFailed);
}

/// The day to fetch first: today when asked for (the day most surely inside the feed's week).
inline QDate first_day(const Request& r, const QDate& today) {
    if (r.days.isEmpty())
        return {};
    return r.days.contains(today) ? today : r.days.first();
}

/// The calls still to make after the first reply; *stop = none (the first failure would repeat,
/// or the run was cancelled). Days outside the feed's week (known from an Ok reply) are skipped.
inline QVector<QDate> remaining_days(const Request& r, const DayResult& first, bool* stop) {
    QVector<QDate> out;
    *stop = is_systemic(first) || first.status == DayStatus::Cancelled;
    if (*stop)
        return out;
    const bool bounded = first.status == DayStatus::Ok && first.week_start.isValid() && first.week_end.isValid();
    for (const QDate& d : r.days) {
        if (d == first.day)
            continue;
        if (bounded && (d < first.week_start || d > first.week_end))
            continue;
        out << d;
    }
    return out;
}

// ── Events and the block ─────────────────────────────────────────────────────

struct Event {
    QDate day;
    QString time; ///< "14:30", this computer's time
    QString currency;
    int impact = 0; ///< 3 high, 2 medium, 1 low, 0 holiday / non-economic
    QString impact_label;
    QString title;
    QString forecast;
    QString previous;
};

inline int impact_from_label(const QString& label) {
    const QString t = label.trimmed().toLower();
    return t == QLatin1String("high") ? 3 : t == QLatin1String("medium") ? 2 : t == QLatin1String("low") ? 1 : 0;
}

inline QString impact_text(const Event& e) {
    if (!e.impact_label.isEmpty())
        return e.impact_label;
    return e.impact >= 3 ? ec_tr("High") : e.impact == 2 ? ec_tr("Medium") : e.impact == 1 ? ec_tr("Low") : ec_tr("Holiday");
}

inline QVector<Event> events_of(const DayResult& d) {
    QVector<Event> out;
    if (d.status != DayStatus::Ok)
        return out;
    auto col = [&d](const char* name) { return static_cast<int>(d.columns.indexOf(QString::fromLatin1(name))); };
    const int c_time = col("time"), c_ccy = col("currency"), c_imp = col("impact"), c_label = col("impact_label"),
              c_event = col("event"), c_fc = col("forecast"), c_prev = col("previous");
    auto text = [](const QJsonArray& row, int c) -> QString {
        if (c < 0 || c >= row.size())
            return {};
        const QJsonValue v = row.at(c);
        if (v.isString())
            return v.toString().trimmed();
        if (v.isDouble())
            return QString::number(v.toDouble(), 'g', 12);
        return {};
    };
    for (const QJsonArray& row : d.rows) {
        Event e;
        e.day = d.day;
        e.title = text(row, c_event);
        if (e.title.isEmpty())
            continue;
        e.time = text(row, c_time);
        e.currency = text(row, c_ccy).toUpper();
        e.impact_label = text(row, c_label);
        e.impact = (c_imp >= 0 && c_imp < row.size() && row.at(c_imp).isDouble())
                       ? static_cast<int>(row.at(c_imp).toDouble())
                       : impact_from_label(e.impact_label);
        e.forecast = text(row, c_fc);
        e.previous = text(row, c_prev);
        out << e;
    }
    return out;
}

/// What the binder writes into the block (BindFetchResult fields).
struct Shaped {
    bool ok = false;
    QString error;
    QMap<QString, QString> patch;
    bool has_content = false;
    QString content;
    QString title;
    QString source_label;
    QDateTime as_of;
};

/// "no high-impact economic events for USD in the week of 16 Nov 2026".
inline QString no_events_message(const Request& r, bool filtered) {
    QString impact;
    if (filtered && r.min_impact >= 3)
        impact = ec_tr("high-impact ");
    else if (filtered && r.min_impact == 2)
        impact = ec_tr("medium- or high-impact ");
    const QString ccy =
        filtered && !r.currencies.isEmpty() ? ec_tr(" for %1").arg(r.currencies.join(QStringLiteral(", "))) : QString();
    return ec_tr("no %1economic events%2 %3").arg(impact, ccy, range_phrase(r));
}

/// The block from every day's reply: a table ("csv" / "rows" / "cols" / "note") or, for a
/// "list" block, one line per event. Failures are plain sentences (the binder's failed state
/// keeps the last snapshot): no events at all, none left after the filters, or every day failed.
inline Shaped shape(const Request& r, const QVector<DayResult>& replies, const QString& block_type) {
    Shaped s;
    s.source_label = QStringLiteral("Forex Factory");
    QDate week_start, week_end;
    QString tz;
    for (const DayResult& d : replies) {
        if (d.status != DayStatus::Ok)
            continue;
        if (!week_start.isValid() && d.week_start.isValid() && d.week_end.isValid()) {
            week_start = d.week_start;
            week_end = d.week_end;
        }
        if (tz.isEmpty())
            tz = d.timezone;
        if (!s.as_of.isValid() || d.as_of > s.as_of)
            s.as_of = d.as_of;
    }
    int ok_days = 0;
    int empty_days = 0;
    QStringList missing;
    QString first_error;
    for (const DayResult& d : replies) {
        switch (d.status) {
        case DayStatus::Ok:
            ++ok_days;
            break;
        case DayStatus::Empty:
            ++empty_days;
            break;
        case DayStatus::Cancelled:
            break;
        case DayStatus::Failed:
            // A day outside the week the feed covers (a weekend after its last event) is not missing.
            if (week_start.isValid() && (d.day < week_start || d.day > week_end))
                break;
            if (first_error.isEmpty())
                first_error = d.error.left(200);
            missing << d.day.toString(QStringLiteral("ddd d MMM"));
            break;
        }
    }
    const QString not_loaded =
        missing.isEmpty() ? QString()
                          : ec_tr("not loaded: %1 (%2)")
                                .arg(missing.join(QStringLiteral(", ")),
                                     first_error.isEmpty() ? ec_tr("no reason given") : first_error);
    if (ok_days == 0) {
        if (empty_days == 0) {
            s.error = first_error.isEmpty() ? ec_tr("the economic calendar returned nothing") : first_error;
            return s;
        }
        s.error = no_events_message(r, false);
        if (!not_loaded.isEmpty())
            s.error += QStringLiteral("; ") + not_loaded;
        return s;
    }

    QVector<Event> kept;
    for (const DayResult& d : replies)
        for (const Event& e : events_of(d))
            if (e.impact >= r.min_impact && (r.currencies.isEmpty() || r.currencies.contains(e.currency)))
                kept << e;
    std::stable_sort(kept.begin(), kept.end(), [](const Event& a, const Event& b) {
        return a.day != b.day ? a.day < b.day : a.time < b.time;
    });
    const int total = static_cast<int>(kept.size());
    if (kept.isEmpty()) {
        s.error = no_events_message(r, true);
        if (!not_loaded.isEmpty())
            s.error += QStringLiteral("; ") + not_loaded;
        return s;
    }
    if (r.limit > 0 && kept.size() > r.limit)
        kept.resize(r.limit);

    s.ok = true;
    s.title = ec_tr("Economic calendar — %1").arg(range_label(r));
    if (!s.as_of.isValid())
        s.as_of = QDateTime::currentDateTimeUtc();
    const bool show_day = r.period != Period::Day;
    auto value = [](const QString& v) { return v.isEmpty() ? ec_dash() : v; };

    if (block_type == QLatin1String("list")) {
        QStringList lines;
        for (const Event& e : kept) {
            QString line = show_day ? e.day.toString(QStringLiteral("ddd d MMM")) + QLatin1Char(' ') : QString();
            line += e.time + QStringLiteral(" — ") + e.currency + QStringLiteral(", ") + impact_text(e) +
                    QStringLiteral(": ") + e.title;
            if (!e.forecast.isEmpty() || !e.previous.isEmpty())
                line += QStringLiteral(" (") +
                        ec_tr("forecast %1, previous %2").arg(value(e.forecast), value(e.previous)) +
                        QLatin1Char(')');
            lines << line;
        }
        s.has_content = true;
        s.content = lines.join(QLatin1Char('\n'));
        return s;
    }

    QVector<QStringList> grid;
    QStringList head;
    if (show_day)
        head << ec_tr("Day");
    head << ec_tr("Time") << ec_tr("Currency") << ec_tr("Impact") << ec_tr("Event") << ec_tr("Forecast")
         << ec_tr("Previous");
    grid.append(head);
    for (const Event& e : kept) {
        QStringList row;
        if (show_day)
            row << e.day.toString(QStringLiteral("ddd d MMM"));
        row << value(e.time) << value(e.currency) << impact_text(e) << e.title << value(e.forecast)
            << value(e.previous);
        grid.append(row);
    }
    QStringList note;
    if (!tz.isEmpty())
        note << ec_tr("Times in this computer's time zone (%1).").arg(tz);
    note << ec_tr("Forecast and previous values only — the free feed has no actual figures.");
    if (r.min_impact >= 3)
        note << ec_tr("High-impact events only.");
    else if (r.min_impact == 2)
        note << ec_tr("Medium- and high-impact events.");
    else if (r.min_impact == 1)
        note << ec_tr("Low- to high-impact events (holidays left out).");
    if (!r.currencies.isEmpty())
        note << ec_tr("Currencies: %1.").arg(r.currencies.join(QStringLiteral(", ")));
    if (total > kept.size())
        note << ec_tr("Showing the first %1 of %2 events.").arg(kept.size()).arg(total);
    if (!not_loaded.isEmpty()) {
        QString n = not_loaded;
        n[0] = n[0].toUpper();
        note << n + QLatin1Char('.');
    }
    s.patch.insert(QStringLiteral("csv"), report::table::encode(grid));
    s.patch.insert(QStringLiteral("rows"), QString::number(grid.size()));
    s.patch.insert(QStringLiteral("cols"), QString::number(head.size()));
    s.patch.insert(QStringLiteral("note"), note.join(QLatin1Char(' ')));
    return s;
}

} // namespace fincept::services::econcal
