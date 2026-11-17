#include "ui/formatting/DateFormat.h"

#include "ui/formatting/NumberFormat.h"

#include <QCoreApplication>
#include <QLocale>
#include <QRegularExpression>

#include <algorithm>

namespace fincept::ui::formatting {

namespace datefmt_detail {

/// Date patterns derived from a locale's own short / long formats, cached per
/// thread and locale.
struct DatefmtPatterns {
    QString short_date;
    QString medium_date;
    QString long_date;
    QString day_month;  ///< "MMM d" / "d MMM" / "M月d日"
    QString month_year; ///< "MMM yyyy" / "yyyy年M月"
};

/// True when `token` appears in `fmt` outside quoted literals.
bool datefmt_has_token(const QString& fmt, QChar token) {
    bool quoted = false;
    for (const QChar c : fmt) {
        if (c == QLatin1Char('\''))
            quoted = !quoted;
        else if (!quoted && c == token)
            return true;
    }
    return false;
}

QString datefmt_trim_separators(QString f) {
    static const QRegularExpression edges(QStringLiteral("^[\\s,\\x{060C}]+|[\\s,\\x{060C}]+$"));
    f.remove(edges);
    return f;
}

/// Long format minus the weekday, with abbreviated month names:
/// "dddd, MMMM d, yyyy" -> "MMM d, yyyy"; "yyyy年M月d日dddd" -> "yyyy年M月d日".
QString datefmt_medium_from_long(QString f) {
    static const QRegularExpression weekday(QStringLiteral("[\\s,\\x{060C}]*(?<!d)d{3,4}(?!d)[\\s,\\x{060C}]*"));
    static const QRegularExpression full_month(QStringLiteral("(?<!M)M{4}(?!M)"));
    f.replace(weekday, QStringLiteral(" "));
    f.replace(full_month, QStringLiteral("MMM"));
    f = datefmt_trim_separators(f.trimmed());
    if (!datefmt_has_token(f, QLatin1Char('y')) || !datefmt_has_token(f, QLatin1Char('M')) ||
        !datefmt_has_token(f, QLatin1Char('d')))
        return QStringLiteral("d MMM yyyy");
    return f;
}

/// Medium format minus the year: "MMM d, yyyy" -> "MMM d"; "yyyy年M月d日" -> "M月d日".
QString datefmt_day_month_from_medium(QString f) {
    static const QRegularExpression leading_year(QStringLiteral("^y+[^dM']*"));
    static const QRegularExpression trailing_year(
        QStringLiteral("(?:[\\s,.\\x{060C}]*'[^']*')*[\\s,.\\x{060C}]*y+[^dM]*$"));
    if (f.startsWith(QLatin1Char('y')))
        f.remove(leading_year);
    else
        f.remove(trailing_year);
    f = datefmt_trim_separators(f.trimmed());
    if (datefmt_has_token(f, QLatin1Char('y')) || !datefmt_has_token(f, QLatin1Char('M')) ||
        !datefmt_has_token(f, QLatin1Char('d')))
        return QStringLiteral("d MMM");
    return f;
}

/// Medium format minus the day: "MMM d, yyyy" -> "MMM yyyy"; "yyyy年M月d日" -> "yyyy年M月".
QString datefmt_month_year_from_medium(QString f) {
    // A day token ("d", "dd") with its trailing "." or CJK day suffix, an optional quoted
    // literal ("d 'de' MMM"), and separators. Never a "d" inside quotes.
    static const QRegularExpression day(
        QStringLiteral("(?<![dM'])d{1,2}(?![d'])[.\\x{65E5}\\x{C77C}]?(?:\\s*'[^']*')?[\\s,]*"));
    f.remove(day);
    f = datefmt_trim_separators(f.trimmed());
    if (datefmt_has_token(f, QLatin1Char('d')) || !datefmt_has_token(f, QLatin1Char('M')) ||
        !datefmt_has_token(f, QLatin1Char('y')))
        return QStringLiteral("MMM yyyy");
    return f;
}

DatefmtPatterns datefmt_patterns(const QLocale& loc) {
    thread_local QString cached_key;
    thread_local DatefmtPatterns cached;
    const QString key = loc.bcp47Name();
    if (!key.isEmpty() && key == cached_key)
        return cached;
    DatefmtPatterns p;
    p.short_date = loc.dateFormat(QLocale::ShortFormat);
    p.long_date = loc.dateFormat(QLocale::LongFormat);
    p.medium_date = datefmt_medium_from_long(p.long_date);
    p.day_month = datefmt_day_month_from_medium(p.medium_date);
    p.month_year = datefmt_month_year_from_medium(p.medium_date);
    cached_key = key;
    cached = p;
    return p;
}

QString datefmt_date_pattern(DateStyle style, const QLocale& loc) {
    const DatefmtPatterns p = datefmt_patterns(loc);
    switch (style) {
        case DateStyle::Short:
            return p.short_date;
        case DateStyle::Long:
            return p.long_date;
        case DateStyle::Medium:
            break;
    }
    return p.medium_date;
}

QString datefmt_time_pattern(HourCycle cycle, bool with_seconds, const QLocale& loc) {
    switch (cycle) {
        case HourCycle::H12:
            return with_seconds ? QStringLiteral("h:mm:ss AP") : QStringLiteral("h:mm AP");
        case HourCycle::Locale: {
            QString f = loc.timeFormat(QLocale::ShortFormat);
            static const QRegularExpression zone(QStringLiteral("\\s*\\(?t+\\)?"));
            f.remove(zone);
            const bool has_secs = f.contains(QLatin1String("ss"));
            if (with_seconds && !has_secs) {
                const int m = f.indexOf(QLatin1String("mm"));
                if (m >= 0)
                    f.insert(m + 2, QLatin1String(":ss"));
            } else if (!with_seconds && has_secs) {
                f.remove(QLatin1String(":ss"));
            }
            return f.trimmed();
        }
        case HourCycle::H24:
            break;
    }
    return with_seconds ? QStringLiteral("HH:mm:ss") : QStringLiteral("HH:mm");
}

QString datefmt_axis_pattern(qint64 span_secs, HourCycle cycle, const QLocale& loc) {
    const qint64 span = span_secs < 0 ? -span_secs : span_secs;
    constexpr qint64 kDay = 86400;
    if (span <= 180)
        return datefmt_time_pattern(cycle, true, loc);
    if (span <= 2 * kDay)
        return datefmt_time_pattern(cycle, false, loc);
    const DatefmtPatterns p = datefmt_patterns(loc);
    if (span <= 10 * kDay)
        return p.day_month + QLatin1Char(' ') + datefmt_time_pattern(cycle, false, loc);
    if (span <= 180 * kDay)
        return p.day_month;
    if (span <= 4 * 366 * kDay)
        return p.month_year;
    return QStringLiteral("yyyy");
}

QString datefmt_daily_axis_pattern(qint64 span_secs, const QLocale& loc) {
    const qint64 span = span_secs < 0 ? -span_secs : span_secs;
    constexpr qint64 kDay = 86400;
    const DatefmtPatterns p = datefmt_patterns(loc);
    if (span <= 180 * kDay)
        return p.day_month;
    if (span <= 4 * 366 * kDay)
        return p.month_year;
    return QStringLiteral("yyyy");
}

QDateTime datefmt_from_epoch(qint64 epoch_secs) {
    return QDateTime::fromSecsSinceEpoch(epoch_secs, QTimeZone(QTimeZone::UTC));
}

} // namespace datefmt_detail

// ── Zones ───────────────────────────────────────────────────────────────────

QTimeZone display_zone(const QTimeZone& exchange_tz) {
    return FormatSettings::instance().display_zone(exchange_tz);
}

QDateTime to_display_zone(const QDateTime& dt, const QTimeZone& exchange_tz) {
    if (!dt.isValid())
        return dt;
    return dt.toTimeZone(display_zone(exchange_tz));
}

QString zone_label(const QDateTime& dt, const QTimeZone& exchange_tz) {
    if (!dt.isValid())
        return placeholder();
    return to_display_zone(dt, exchange_tz).timeZoneAbbreviation();
}

// ── Dates and times ─────────────────────────────────────────────────────────

QString date(const QDateTime& dt, DateStyle style, const QTimeZone& exchange_tz) {
    if (!dt.isValid())
        return placeholder();
    const QLocale loc = FormatSettings::instance().locale();
    return loc.toString(to_display_zone(dt, exchange_tz).date(), datefmt_detail::datefmt_date_pattern(style, loc));
}

QString date(qint64 epoch_secs, DateStyle style, const QTimeZone& exchange_tz) {
    return date(datefmt_detail::datefmt_from_epoch(epoch_secs), style, exchange_tz);
}

QString time(const QDateTime& dt, bool with_seconds, const QTimeZone& exchange_tz) {
    if (!dt.isValid())
        return placeholder();
    const FormatSettings::Snapshot s = FormatSettings::instance().snapshot();
    return s.locale.toString(to_display_zone(dt, exchange_tz).time(),
                             datefmt_detail::datefmt_time_pattern(s.hour_cycle, with_seconds, s.locale));
}

QString time(qint64 epoch_secs, bool with_seconds, const QTimeZone& exchange_tz) {
    return time(datefmt_detail::datefmt_from_epoch(epoch_secs), with_seconds, exchange_tz);
}

QString date_time(const QDateTime& dt, DateStyle style, bool with_seconds, const QTimeZone& exchange_tz) {
    if (!dt.isValid())
        return placeholder();
    const FormatSettings::Snapshot s = FormatSettings::instance().snapshot();
    const QDateTime local = to_display_zone(dt, exchange_tz);
    return s.locale.toString(local.date(), datefmt_detail::datefmt_date_pattern(style, s.locale)) + QLatin1Char(' ') +
           s.locale.toString(local.time(), datefmt_detail::datefmt_time_pattern(s.hour_cycle, with_seconds, s.locale));
}

QString date_time(qint64 epoch_secs, DateStyle style, bool with_seconds, const QTimeZone& exchange_tz) {
    return date_time(datefmt_detail::datefmt_from_epoch(epoch_secs), style, with_seconds, exchange_tz);
}

// ── Relative ────────────────────────────────────────────────────────────────

QString relative(const QDateTime& dt) {
    return relative(dt, QDateTime::currentDateTimeUtc());
}

QString relative(const QDateTime& dt, const QDateTime& now) {
    if (!dt.isValid() || !now.isValid())
        return placeholder();
    return relative(dt.toSecsSinceEpoch(), now.toSecsSinceEpoch());
}

QString relative(qint64 epoch_secs) {
    return relative(epoch_secs, QDateTime::currentSecsSinceEpoch());
}

QString relative(qint64 epoch_secs, qint64 now_epoch_secs) {
    const qint64 diff = now_epoch_secs - epoch_secs;
    const bool future = diff < 0;
    const qint64 a = future ? -diff : diff;
    if (a < 45)
        return QCoreApplication::translate("fincept::ui::formatting::DateFormat", "just now");
    if (a < 3600) {
        const int n = int(std::max<qint64>(1, a / 60));
        return future ? QCoreApplication::translate("fincept::ui::formatting::DateFormat", "in %nm",
                                                    "relative time, minutes ahead", n)
                      : QCoreApplication::translate("fincept::ui::formatting::DateFormat", "%nm ago",
                                                    "relative time, minutes ago", n);
    }
    if (a < 86400) {
        const int n = int(a / 3600);
        return future ? QCoreApplication::translate("fincept::ui::formatting::DateFormat", "in %nh",
                                                    "relative time, hours ahead", n)
                      : QCoreApplication::translate("fincept::ui::formatting::DateFormat", "%nh ago",
                                                    "relative time, hours ago", n);
    }
    if (a < 7 * 86400) {
        const int n = int(a / 86400);
        return future ? QCoreApplication::translate("fincept::ui::formatting::DateFormat", "in %nd",
                                                    "relative time, days ahead", n)
                      : QCoreApplication::translate("fincept::ui::formatting::DateFormat", "%nd ago",
                                                    "relative time, days ago", n);
    }
    return date(epoch_secs, DateStyle::Medium);
}

// ── Axis labels ─────────────────────────────────────────────────────────────

QString axis_label(const QDateTime& ts, qint64 span_secs, const QTimeZone& exchange_tz) {
    if (!ts.isValid())
        return placeholder();
    const FormatSettings::Snapshot s = FormatSettings::instance().snapshot();
    return s.locale.toString(to_display_zone(ts, exchange_tz),
                             datefmt_detail::datefmt_axis_pattern(span_secs, s.hour_cycle, s.locale));
}

QString axis_label(qint64 epoch_secs, qint64 span_secs, const QTimeZone& exchange_tz) {
    return axis_label(datefmt_detail::datefmt_from_epoch(epoch_secs), span_secs, exchange_tz);
}

QString axis_label_daily(qint64 epoch_secs, qint64 span_secs) {
    const QLocale loc = FormatSettings::instance().locale();
    return loc.toString(date_from_utc_midnight(epoch_secs), datefmt_detail::datefmt_daily_axis_pattern(span_secs, loc));
}

// ── Calendar dates ──────────────────────────────────────────────────────────

QDate date_from_utc_midnight(qint64 epoch_secs) {
    return datefmt_detail::datefmt_from_epoch(epoch_secs).date();
}

QDate date_from_utc_midnight(const QDateTime& dt) {
    return dt.isValid() ? dt.toUTC().date() : QDate();
}

QString calendar_date(const QDate& d, DateStyle style) {
    if (!d.isValid())
        return placeholder();
    const QLocale loc = FormatSettings::instance().locale();
    return loc.toString(d, datefmt_detail::datefmt_date_pattern(style, loc));
}

QString bar_date(qint64 epoch_secs, DateStyle style) {
    return calendar_date(date_from_utc_midnight(epoch_secs), style);
}

// ── Patterns ────────────────────────────────────────────────────────────────

QString date_pattern(DateStyle style) {
    return datefmt_detail::datefmt_date_pattern(style, FormatSettings::instance().locale());
}

QString time_pattern(bool with_seconds) {
    const FormatSettings::Snapshot s = FormatSettings::instance().snapshot();
    return datefmt_detail::datefmt_time_pattern(s.hour_cycle, with_seconds, s.locale);
}

QString axis_pattern(qint64 span_secs) {
    const FormatSettings::Snapshot s = FormatSettings::instance().snapshot();
    return datefmt_detail::datefmt_axis_pattern(span_secs, s.hour_cycle, s.locale);
}

} // namespace fincept::ui::formatting
