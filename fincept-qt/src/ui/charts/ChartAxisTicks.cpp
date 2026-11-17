#include "ui/charts/ChartAxisTicks.h"

#include <QDate>
#include <QDateTime>
#include <QLocale>
#include <QTime>
#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace fincept::ui::chart_ticks {

namespace {

constexpr double kAxtSecondMs = 1000.0;
constexpr double kAxtMinuteMs = 60.0 * kAxtSecondMs;
constexpr double kAxtHourMs = 60.0 * kAxtMinuteMs;
constexpr double kAxtDayMs = 24.0 * kAxtHourMs;
constexpr double kAxtMonthMs = 30.436875 * kAxtDayMs;
constexpr double kAxtYearMs = 365.2425 * kAxtDayMs;

/// Upper bound on generated ticks — a guard against a degenerate request
/// (huge span with a tiny step) spinning in a loop.
constexpr int kAxtMaxGenerated = 2000;

struct AxtStepDef {
    TimeUnit unit;
    int count;
};

// Candidate steps, finest first. The first one that yields <= max_ticks wins.
constexpr AxtStepDef kAxtSteps[] = {
    {TimeUnit::Second, 1}, {TimeUnit::Second, 5},  {TimeUnit::Second, 15}, {TimeUnit::Second, 30},
    {TimeUnit::Minute, 1}, {TimeUnit::Minute, 5},  {TimeUnit::Minute, 15}, {TimeUnit::Minute, 30},
    {TimeUnit::Hour, 1},   {TimeUnit::Hour, 2},    {TimeUnit::Hour, 3},    {TimeUnit::Hour, 6},
    {TimeUnit::Hour, 12},  {TimeUnit::Day, 1},     {TimeUnit::Day, 2},     {TimeUnit::Week, 1},
    {TimeUnit::Week, 2},   {TimeUnit::Month, 1},   {TimeUnit::Month, 2},   {TimeUnit::Month, 3},
    {TimeUnit::Month, 6},  {TimeUnit::Year, 1},    {TimeUnit::Year, 2},    {TimeUnit::Year, 5},
    {TimeUnit::Year, 10},  {TimeUnit::Year, 25},   {TimeUnit::Year, 50},   {TimeUnit::Year, 100},
};

QTimeZone axt_zone(bool utc) {
    return utc ? QTimeZone(QTimeZone::UTC) : QTimeZone(QTimeZone::LocalTime);
}

QDateTime axt_dt(double ms, bool utc) {
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(std::llround(ms)), axt_zone(utc));
}

double axt_day_start_ms(const QDate& d, bool utc) {
    return static_cast<double>(d.startOfDay(axt_zone(utc)).toMSecsSinceEpoch());
}

double axt_unit_ms(TimeUnit u) {
    switch (u) {
        case TimeUnit::Second:
            return kAxtSecondMs;
        case TimeUnit::Minute:
            return kAxtMinuteMs;
        case TimeUnit::Hour:
            return kAxtHourMs;
        case TimeUnit::Day:
            return kAxtDayMs;
        case TimeUnit::Week:
            return 7.0 * kAxtDayMs;
        case TimeUnit::Month:
            return kAxtMonthMs;
        case TimeUnit::Year:
            return kAxtYearMs;
    }
    return kAxtDayMs;
}

} // namespace

// ── Numeric ──────────────────────────────────────────────────────────────────

double nice_number(double x, bool round) {
    if (!(x > 0.0) || !std::isfinite(x))
        return 1.0;
    const double exp10 = std::floor(std::log10(x));
    const double base = std::pow(10.0, exp10);
    const double f = x / base;
    double nf;
    if (round)
        nf = f < 1.5 ? 1.0 : f < 3.0 ? 2.0 : f < 7.0 ? 5.0 : 10.0;
    else
        nf = f <= 1.0 ? 1.0 : f <= 2.0 ? 2.0 : f <= 5.0 ? 5.0 : 10.0;
    return nf * base;
}

int decimals_for_step(double step) {
    if (!(step > 0.0) || !std::isfinite(step))
        return 0;
    const int d = -static_cast<int>(std::floor(std::log10(step) + 1e-9));
    return std::clamp(d, 0, 10);
}

NumericTicks nice_ticks(double lo, double hi, int max_ticks) {
    NumericTicks out;
    if (!std::isfinite(lo) || !std::isfinite(hi))
        return out;
    if (hi < lo)
        std::swap(lo, hi);
    max_ticks = std::max(2, max_ticks);

    const double span = hi - lo;
    if (span <= 0.0) {
        out.values.append(lo);
        out.step = nice_number(std::abs(lo) > 0.0 ? std::abs(lo) * 0.1 : 1.0, true);
        out.decimals = decimals_for_step(out.step);
        return out;
    }

    // A short axis asks for few ticks, and the next 1-2-5 step up can leave
    // only one inside the range — halve the budget's step until two fit.
    double step = 0.0;
    for (int attempt = 0; attempt < 4; ++attempt) {
        step = nice_number(span / max_ticks, false);
        out.values.clear();
        const double first = std::ceil(lo / step - 1e-9) * step;
        for (int k = 0; k < kAxtMaxGenerated; ++k) {
            double v = first + k * step;
            if (v > hi + step * 1e-9)
                break;
            if (std::abs(v) < step * 1e-9)
                v = 0.0; // avoid "-0" and 1e-17 noise
            out.values.append(v);
        }
        if (out.values.size() >= 2)
            break;
        max_ticks *= 2;
    }
    out.step = step;
    out.decimals = decimals_for_step(step);
    return out;
}

NumericTicks log_ticks(double lo, double hi, int max_ticks) {
    if (!(lo > 0.0) || !(hi > lo) || !std::isfinite(hi))
        return nice_ticks(lo, hi, max_ticks);
    max_ticks = std::max(2, max_ticks);

    const double l0 = std::log10(lo);
    const double l1 = std::log10(hi);
    const double decades = l1 - l0;
    if (decades < 0.5) // narrow range: linear ticks read better and are still correct positions
        return nice_ticks(lo, hi, max_ticks);

    const int d0 = static_cast<int>(std::floor(l0));
    const int d1 = static_cast<int>(std::ceil(l1));

    static const QVector<double> kAxtM1235 = {1.0, 2.0, 3.0, 5.0};
    static const QVector<double> kAxtM125 = {1.0, 2.0, 5.0};
    static const QVector<double> kAxtM1 = {1.0};

    auto build = [&](const QVector<double>& mantissas, int decade_step) {
        QVector<double> vals;
        for (int d = d0; d <= d1 && vals.size() < kAxtMaxGenerated; d += decade_step) {
            const double p10 = std::pow(10.0, d);
            for (double m : mantissas) {
                const double v = m * p10;
                if (v >= lo * (1.0 - 1e-9) && v <= hi * (1.0 + 1e-9))
                    vals.append(v);
            }
        }
        return vals;
    };

    QVector<double> vals;
    if (decades <= 1.5)
        vals = build(kAxtM1235, 1);
    if (vals.isEmpty() || vals.size() > max_ticks + max_ticks / 2)
        vals = build(kAxtM125, 1);
    if (vals.size() > max_ticks + max_ticks / 2)
        vals = build(kAxtM1, std::max(1, static_cast<int>(std::ceil(decades / max_ticks))));
    if (vals.size() < 2)
        return nice_ticks(lo, hi, max_ticks);

    NumericTicks out;
    out.values = vals;
    out.step = 0.0; // not uniform
    out.decimals = decimals_for_step(vals.first());
    return out;
}

// ── Time ─────────────────────────────────────────────────────────────────────

double TimeStep::approx_ms() const {
    return axt_unit_ms(unit) * std::max(1, count);
}

TimeTicks time_ticks(double from_ms, double to_ms, int max_ticks, bool utc) {
    TimeTicks out;
    if (!std::isfinite(from_ms) || !std::isfinite(to_ms) || !(to_ms > from_ms))
        return out;
    max_ticks = std::max(2, max_ticks);
    const double span = to_ms - from_ms;

    TimeStep step{TimeUnit::Year, 1};
    bool found = false;
    for (const auto& def : kAxtSteps) {
        const TimeStep cand{def.unit, def.count};
        if (span / cand.approx_ms() <= max_ticks) {
            step = cand;
            found = true;
            break;
        }
    }
    if (!found) {
        const double years = span / kAxtYearMs / max_ticks;
        step = TimeStep{TimeUnit::Year, std::max(1, static_cast<int>(std::ceil(nice_number(years, false))))};
    }
    out.step = step;

    const int n = std::max(1, step.count);
    switch (step.unit) {
        case TimeUnit::Second:
        case TimeUnit::Minute:
        case TimeUnit::Hour: {
            const double step_ms = axt_unit_ms(step.unit) * n;
            // Align to the local clock (whole hours in local time, not UTC).
            const double off = utc ? 0.0 : axt_dt(from_ms, false).offsetFromUtc() * 1000.0;
            const double first = std::ceil((from_ms + off) / step_ms) * step_ms - off;
            for (int k = 0; k < kAxtMaxGenerated; ++k) {
                const double v = first + k * step_ms;
                if (v > to_ms)
                    break;
                out.values.append(v);
            }
            break;
        }
        case TimeUnit::Day: {
            QDate d = axt_dt(from_ms, utc).date();
            for (int k = 0; k < kAxtMaxGenerated * n; ++k) {
                const double v = axt_day_start_ms(d, utc);
                if (v > to_ms)
                    break;
                if (v >= from_ms && (n == 1 || d.toJulianDay() % n == 0))
                    out.values.append(v);
                d = d.addDays(1);
            }
            break;
        }
        case TimeUnit::Week: {
            QDate d = axt_dt(from_ms, utc).date();
            d = d.addDays((8 - d.dayOfWeek()) % 7); // next Monday (or today)
            for (int k = 0; k < kAxtMaxGenerated * n; ++k) {
                const double v = axt_day_start_ms(d, utc);
                if (v > to_ms)
                    break;
                if (v >= from_ms && (n == 1 || (d.toJulianDay() / 7) % n == 0))
                    out.values.append(v);
                d = d.addDays(7);
            }
            break;
        }
        case TimeUnit::Month: {
            const QDate start = axt_dt(from_ms, utc).date();
            QDate d(start.year(), start.month(), 1);
            for (int k = 0; k < kAxtMaxGenerated * n; ++k) {
                const int idx = d.year() * 12 + (d.month() - 1);
                if (idx % n == 0) {
                    const double v = axt_day_start_ms(d, utc);
                    if (v > to_ms)
                        break;
                    if (v >= from_ms)
                        out.values.append(v);
                }
                d = d.addMonths(1);
            }
            break;
        }
        case TimeUnit::Year: {
            int y = axt_dt(from_ms, utc).date().year();
            y -= ((y % n) + n) % n;
            for (int k = 0; k < kAxtMaxGenerated; ++k, y += n) {
                const QDate d(y, 1, 1);
                if (!d.isValid())
                    break;
                const double v = axt_day_start_ms(d, utc);
                if (v > to_ms)
                    break;
                if (v >= from_ms)
                    out.values.append(v);
            }
            break;
        }
    }
    return out;
}

QString time_tick_label(double ms, const TimeStep& step, bool utc) {
    const QDateTime dt = axt_dt(ms, utc);
    const QLocale loc;
    const QTime t = dt.time();
    const QDate d = dt.date();
    switch (step.unit) {
        case TimeUnit::Second:
            if (t.hour() == 0 && t.minute() == 0 && t.second() == 0)
                return loc.toString(d, QStringLiteral("d MMM"));
            return loc.toString(t, QStringLiteral("HH:mm:ss"));
        case TimeUnit::Minute:
        case TimeUnit::Hour:
            if (t.hour() == 0 && t.minute() == 0)
                return loc.toString(d, QStringLiteral("d MMM"));
            return loc.toString(t, QStringLiteral("HH:mm"));
        case TimeUnit::Day:
        case TimeUnit::Week:
            if (d.month() == 1 && d.day() == 1)
                return loc.toString(d, QStringLiteral("yyyy"));
            return loc.toString(d, QStringLiteral("d MMM"));
        case TimeUnit::Month:
            if (d.month() == 1)
                return loc.toString(d, QStringLiteral("yyyy"));
            return loc.toString(d, QStringLiteral("MMM"));
        case TimeUnit::Year:
            return loc.toString(d, QStringLiteral("yyyy"));
    }
    return loc.toString(d, QStringLiteral("d MMM yyyy"));
}

QString time_point_label(double ms, double typical_spacing_ms, bool utc) {
    const QDateTime dt = axt_dt(ms, utc);
    const QLocale loc;
    if (typical_spacing_ms <= 0.0) {
        if (dt.time() == QTime(0, 0))
            return loc.toString(dt.date(), QStringLiteral("d MMM yyyy"));
        return loc.toString(dt, QStringLiteral("d MMM yyyy HH:mm"));
    }
    if (typical_spacing_ms < kAxtMinuteMs)
        return loc.toString(dt, QStringLiteral("d MMM yyyy HH:mm:ss"));
    if (typical_spacing_ms < 0.9 * kAxtDayMs)
        return loc.toString(dt, QStringLiteral("d MMM yyyy HH:mm"));
    if (typical_spacing_ms >= 27.0 * kAxtDayMs)
        return loc.toString(dt.date(), QStringLiteral("MMM yyyy"));
    return loc.toString(dt.date(), QStringLiteral("ddd d MMM yyyy"));
}

QString time_label_sample(const TimeStep& step) {
    const QLocale loc;
    const QDate wide(2000, 9, 28); // two-digit day, a long month abbreviation
    switch (step.unit) {
        case TimeUnit::Second:
            return loc.toString(QTime(23, 59, 59), QStringLiteral("HH:mm:ss"));
        case TimeUnit::Minute:
        case TimeUnit::Hour: {
            const QString a = loc.toString(QTime(23, 59), QStringLiteral("HH:mm"));
            const QString b = loc.toString(wide, QStringLiteral("d MMM"));
            return a.size() >= b.size() ? a : b;
        }
        case TimeUnit::Day:
        case TimeUnit::Week:
            return loc.toString(wide, QStringLiteral("d MMM"));
        case TimeUnit::Month: {
            const QString a = loc.toString(wide, QStringLiteral("MMM"));
            const QString b = loc.toString(wide, QStringLiteral("yyyy"));
            return a.size() >= b.size() ? a : b;
        }
        case TimeUnit::Year:
            return loc.toString(wide, QStringLiteral("yyyy"));
    }
    return loc.toString(wide, QStringLiteral("d MMM yyyy"));
}

} // namespace fincept::ui::chart_ticks
