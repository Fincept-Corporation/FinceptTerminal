#pragma once
// ChartAxisTicks — "nice" axis tick generation for custom-painted charts.
//
// Shared by TimeSeriesChart and CandleChart; any QPainter chart may use it.
//   * numeric ticks follow the 1-2-5 progression (0, 20, 40 … not 0, 17, 34),
//   * log ticks walk decades (1, 2, 5, 10, 20, 50 …),
//   * time ticks are calendar-aware: whole minutes/hours, local midnights,
//     Mondays, month starts and year starts, so labels sit on the boundaries a
//     reader expects.
//
// TIME UNIT: every time value here is epoch MILLISECONDS (a UTC instant, held
// in a double — the unit of QDateTime::toMSecsSinceEpoch()). Calendar
// alignment and labels use local time unless `utc` is true.
//
// Plain functions, no QObject; labels use QLocale() so month names follow the
// application language.

#include <QString>
#include <QVector>

namespace fincept::ui::chart_ticks {

/// Result of a numeric tick computation. `values` only contains ticks inside
/// the requested range. `decimals` is the number of fractional digits that
/// tells adjacent ticks apart (0 for a step of 1, 2 for a step of 0.05, …).
struct NumericTicks {
    QVector<double> values;
    double step = 0.0;
    int decimals = 0;
};

/// A "nice" number close to `x` (> 0): 1, 2 or 5 × 10^k. With `round` the
/// nearest such number, otherwise the smallest one >= x.
double nice_number(double x, bool round);

/// Fractional digits needed to print multiples of `step` distinctly (0..10).
int decimals_for_step(double step);

/// Round ticks covering [lo, hi], at most about `max_ticks` of them.
NumericTicks nice_ticks(double lo, double hi, int max_ticks);

/// Ticks for a log10 axis over [lo, hi] (both must be > 0; falls back to
/// nice_ticks otherwise or when the range is too narrow for decade ticks).
NumericTicks log_ticks(double lo, double hi, int max_ticks);

/// Calendar granularity of a time axis.
enum class TimeUnit { Second, Minute, Hour, Day, Week, Month, Year };

struct TimeStep {
    TimeUnit unit = TimeUnit::Day;
    int count = 1;
    /// Nominal length in ms (months = 30.44 days, years = 365.25 days).
    double approx_ms() const;
};

struct TimeTicks {
    QVector<double> values; ///< epoch ms, ascending, all inside [from, to]
    TimeStep step;
};

/// Calendar-aligned ticks covering [from_ms, to_ms], at most about `max_ticks`.
TimeTicks time_ticks(double from_ms, double to_ms, int max_ticks, bool utc = false);

/// Default axis label for a tick at `ms` on an axis stepping by `step`.
/// Hierarchical: a tick that starts a larger unit shows that unit instead
/// (midnight on an hourly axis shows the date, January on a monthly axis
/// shows the year).
QString time_tick_label(double ms, const TimeStep& step, bool utc = false);

/// Label for a single point in time (crosshair / tooltip). Shows the time of
/// day only when `typical_spacing_ms` is below a day; seconds when below a
/// minute. Pass 0 when unknown (date + time).
QString time_point_label(double ms, double typical_spacing_ms, bool utc = false);

/// A representative widest label for `step`, for measuring how many ticks fit.
QString time_label_sample(const TimeStep& step);

} // namespace fincept::ui::chart_ticks
