#pragma once
// DateFormat — the app's single date / time formatter.
//
// Every function converts the instant to FormatSettings' display zone first
// (Local, UTC, the exchange's zone, or a fixed IANA zone) and formats with the
// number locale's month names and field order — never hard-coded English.
// Invalid input gives placeholder() ("—").
//
//   namespace fmt = fincept::ui::formatting;
//   fmt::date(ts)                          -> "Nov 16, 2026" (en_US) / "16. Nov. 2026" (de)
//   fmt::time(ts)                          -> "14:30" (24h) / "2:30 PM" (12h)
//   fmt::date_time(ts, fmt::DateStyle::Short, false, nyse_tz)
//   fmt::relative(ts)                      -> "5m ago"
//   fmt::axis_label(ts, visible_span_secs) -> "14:30" / "Nov 16" / "Nov 2026" / "2026"
//
// Daily bars: a bar stamped 2026-11-16T00:00Z is the 16th everywhere, but in
// any zone west of UTC that instant falls on the 15th. Never pass such stamps
// to date(); use bar_date() / date_from_utc_midnight() / axis_label_daily(),
// which read the UTC calendar date without converting.

#include "ui/formatting/FormatSettings.h"

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QTimeZone>

namespace fincept::ui::formatting {

enum class DateStyle {
    Short,  ///< the locale's short date: "11/16/26", "16.11.26"
    Medium, ///< abbreviated month, no weekday: "Nov 16, 2026", "16. Nov. 2026"
    Long,   ///< the locale's long date: "Monday, November 16, 2026"
};

/// The zone timestamps are shown in (FormatSettings::display_zone()).
QTimeZone display_zone(const QTimeZone& exchange_tz = QTimeZone());
/// `dt` converted to the display zone.
QDateTime to_display_zone(const QDateTime& dt, const QTimeZone& exchange_tz = QTimeZone());

QString date(const QDateTime& dt, DateStyle style = DateStyle::Medium, const QTimeZone& exchange_tz = QTimeZone());
QString date(qint64 epoch_secs, DateStyle style = DateStyle::Medium, const QTimeZone& exchange_tz = QTimeZone());

/// Time of day per FormatSettings' hour cycle: "14:30", "2:30 PM", "14:30:05".
QString time(const QDateTime& dt, bool with_seconds = false, const QTimeZone& exchange_tz = QTimeZone());
QString time(qint64 epoch_secs, bool with_seconds = false, const QTimeZone& exchange_tz = QTimeZone());

/// date() + " " + time().
QString date_time(const QDateTime& dt, DateStyle style = DateStyle::Medium, bool with_seconds = false,
                  const QTimeZone& exchange_tz = QTimeZone());
QString date_time(qint64 epoch_secs, DateStyle style = DateStyle::Medium, bool with_seconds = false,
                  const QTimeZone& exchange_tz = QTimeZone());

/// The display zone's abbreviation at `dt`: "EST", "UTC", "IST", "GMT+5:30".
QString zone_label(const QDateTime& dt, const QTimeZone& exchange_tz = QTimeZone());

/// "just now", "5m ago", "3h ago", "2d ago", "in 5m"; a week or more away
/// falls back to date(). Translated (plural-aware).
QString relative(const QDateTime& dt);
QString relative(const QDateTime& dt, const QDateTime& now);
QString relative(qint64 epoch_secs);
QString relative(qint64 epoch_secs, qint64 now_epoch_secs);

/// A chart-axis label whose granularity follows the visible span:
///   <= 3 min  "14:30:05"     <= 2 days  "14:30"     <= 10 days "Nov 16 14:30"
///   <= 180 d  "Nov 16"       <= 4 years "Nov 2026"  longer     "2026"
QString axis_label(const QDateTime& ts, qint64 span_secs, const QTimeZone& exchange_tz = QTimeZone());
QString axis_label(qint64 epoch_secs, qint64 span_secs, const QTimeZone& exchange_tz = QTimeZone());
/// axis_label() for daily bars stamped at UTC midnight: the UTC calendar date,
/// never shifted by the display zone. Day granularity at most.
QString axis_label_daily(qint64 epoch_secs, qint64 span_secs);

/// The UTC calendar date of a daily-bar timestamp (no zone conversion).
QDate date_from_utc_midnight(qint64 epoch_secs);
QDate date_from_utc_midnight(const QDateTime& dt);
/// A calendar date (no time, no zone): settlement, ex-dividend, expiry dates.
QString calendar_date(const QDate& d, DateStyle style = DateStyle::Medium);
/// calendar_date(date_from_utc_midnight(epoch_secs), style).
QString bar_date(qint64 epoch_secs, DateStyle style = DateStyle::Medium);

/// The QLocale format strings behind the functions above, for widgets that
/// take a pattern (QDateEdit::setDisplayFormat). Format with the same locale:
/// FormatSettings::instance().locale().toString(value, pattern).
QString date_pattern(DateStyle style = DateStyle::Medium);
QString time_pattern(bool with_seconds = false);
QString axis_pattern(qint64 span_secs);

} // namespace fincept::ui::formatting
