#pragma once
// SensitivityGrid.h — the data behind ui::SensitivityTable, with no widgets (Qt Core only), so the
// rules can be unit-tested: two input axes, one or more metric matrices, a base-case cell, the
// colour scale and the exported text.
//
//   ui::SensitivityGrid g;
//   g.row_title = "WACC";            g.row_labels = {"8.0%", "9.0%", "10.0%"};
//   g.col_title = "Terminal growth"; g.col_labels = {"2.0%", "2.5%", "3.0%"};
//   g.metrics = {{"Price / share", values /* [row][col], NaN = not computed */}};
//   g.base_row = 1; g.base_col = 1;
//   const double t = sensitivity_detail::tone(g, 0, v);     // colour position, -1..1 or 0..1
//   const QString tsv = sensitivity_detail::to_tsv(g, 0, fmt);
//
// Nothing here computes a sensitivity: callers fill the matrices (one Python call, a C++ worker).

#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace fincept::ui {

struct SensitivityMetric {
    QString name;                     ///< toggle text ("IRR", "MOIC")
    QVector<QVector<double>> values;  ///< [row][col]; NaN or missing = not computed
    std::function<QString(double)> format; ///< empty = the table's default formatter
    /// Colour direction: true = larger values read as better (IRR, price); false = smaller is better.
    bool higher_is_better = true;
    /// Diverging scale centre; NaN = the base cell's value when there is one, else the data midpoint.
    double midpoint = std::numeric_limits<double>::quiet_NaN();
    /// Diverging (around the midpoint) or sequential (min -> max).
    bool diverging = true;
};

struct SensitivityGrid {
    QString row_title;
    QString col_title;
    QStringList row_labels;
    QStringList col_labels;
    QVector<SensitivityMetric> metrics;
    int base_row = -1; ///< -1 = no base case
    int base_col = -1;

    int rows() const { return static_cast<int>(row_labels.size()); }
    int cols() const { return static_cast<int>(col_labels.size()); }
    bool has_base() const { return base_row >= 0 && base_col >= 0 && base_row < rows() && base_col < cols(); }
    bool is_empty() const { return rows() == 0 || cols() == 0 || metrics.isEmpty(); }
};

namespace sensitivity_detail {

inline double value_at(const SensitivityMetric& m, int r, int c) {
    if (r < 0 || r >= m.values.size() || c < 0 || c >= m.values.at(r).size())
        return std::numeric_limits<double>::quiet_NaN();
    return m.values.at(r).at(c);
}

/// Finite min / max of a metric; false when no value is finite.
inline bool finite_range(const SensitivityMetric& m, double* lo, double* hi) {
    double mn = std::numeric_limits<double>::max();
    double mx = std::numeric_limits<double>::lowest();
    for (const auto& row : m.values)
        for (double v : row)
            if (std::isfinite(v)) {
                mn = std::min(mn, v);
                mx = std::max(mx, v);
            }
    if (mn > mx)
        return false;
    *lo = mn;
    *hi = mx;
    return true;
}

/// The diverging centre of metric `index`: its explicit midpoint, else the base cell's value, else
/// the middle of the data range. NaN when nothing is finite.
inline double resolved_midpoint(const SensitivityGrid& g, int index) {
    if (index < 0 || index >= g.metrics.size())
        return std::numeric_limits<double>::quiet_NaN();
    const SensitivityMetric& m = g.metrics.at(index);
    if (std::isfinite(m.midpoint))
        return m.midpoint;
    if (g.has_base()) {
        const double b = value_at(m, g.base_row, g.base_col);
        if (std::isfinite(b))
            return b;
    }
    double lo = 0, hi = 0;
    if (!finite_range(m, &lo, &hi))
        return std::numeric_limits<double>::quiet_NaN();
    return (lo + hi) / 2.0;
}

/// Colour position of `v` under metric `index`: diverging -1 (worst) .. 0 (centre) .. +1 (best);
/// sequential 0 (worst) .. 1 (best). NaN for a value that is not finite. "Best" follows
/// higher_is_better, so a cost grid colours its cheapest cell like an IRR grid's highest.
inline double tone(const SensitivityGrid& g, int index, double v) {
    if (!std::isfinite(v) || index < 0 || index >= g.metrics.size())
        return std::numeric_limits<double>::quiet_NaN();
    const SensitivityMetric& m = g.metrics.at(index);
    double lo = 0, hi = 0;
    if (!finite_range(m, &lo, &hi))
        return std::numeric_limits<double>::quiet_NaN();
    const double sign = m.higher_is_better ? 1.0 : -1.0;
    if (!m.diverging) {
        if (hi <= lo)
            return 0.5;
        const double t = (v - lo) / (hi - lo);
        return std::clamp(sign > 0 ? t : 1.0 - t, 0.0, 1.0);
    }
    const double mid = resolved_midpoint(g, index);
    const double span = std::max(std::fabs(hi - mid), std::fabs(mid - lo));
    if (!(span > 0))
        return 0.0;
    return std::clamp(sign * (v - mid) / span, -1.0, 1.0);
}

inline QString cell_text(const SensitivityGrid& g, int index, int r, int c,
                         const std::function<QString(double)>& fallback) {
    if (index < 0 || index >= g.metrics.size())
        return QString();
    const SensitivityMetric& m = g.metrics.at(index);
    const double v = value_at(m, r, c);
    if (!std::isfinite(v))
        return QStringLiteral("-");
    if (m.format)
        return m.format(v);
    if (fallback)
        return fallback(v);
    return QString::number(v, 'g', 6);
}

/// Header row of an export: "<row title> \ <col title>", then the column labels.
inline QStringList headers(const SensitivityGrid& g) {
    QStringList h;
    h << (g.row_title.isEmpty() && g.col_title.isEmpty()
              ? QString()
              : g.row_title + QStringLiteral(" \\ ") + g.col_title);
    h += g.col_labels;
    return h;
}

/// Every row as text: the row label, then each cell formatted like the table shows it.
inline QVector<QStringList> rows(const SensitivityGrid& g, int index, const std::function<QString(double)>& fallback) {
    QVector<QStringList> out;
    out.reserve(g.rows());
    for (int r = 0; r < g.rows(); ++r) {
        QStringList line;
        line << g.row_labels.at(r);
        for (int c = 0; c < g.cols(); ++c)
            line << cell_text(g, index, r, c, fallback);
        out << line;
    }
    return out;
}

/// Tab-separated text for the clipboard (pastes into a spreadsheet as a grid). A metric name, when
/// there are several, heads the block.
inline QString to_tsv(const SensitivityGrid& g, int index, const std::function<QString(double)>& fallback) {
    QStringList lines;
    if (g.metrics.size() > 1 && index >= 0 && index < g.metrics.size())
        lines << g.metrics.at(index).name;
    lines << headers(g).join(QLatin1Char('\t'));
    for (const QStringList& r : rows(g, index, fallback))
        lines << r.join(QLatin1Char('\t'));
    return lines.join(QLatin1Char('\n'));
}

/// Evenly spaced axis values centred on `base`: steps values (odd steps put `base` in the middle).
inline QVector<double> centred_axis(double base, double step, int steps) {
    QVector<double> v;
    if (steps <= 0)
        return v;
    const int half = (steps - 1) / 2;
    for (int i = 0; i < steps; ++i)
        v << base + (i - half) * step;
    return v;
}

} // namespace sensitivity_detail

} // namespace fincept::ui
