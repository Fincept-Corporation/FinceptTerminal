#include "ui/charts/MonthlyReturnsHeatmap.h"

#include "ui/theme/Theme.h"

#include <QFontMetricsF>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace fincept::ui {

namespace {

/// The `p` percentile (0..1) of |values|, linear interpolation; 0 when empty.
double mr_abs_percentile(QVector<double> values, double p) {
    for (double& v : values)
        v = std::abs(v);
    values.erase(std::remove_if(values.begin(), values.end(), [](double v) { return !std::isfinite(v); }),
                 values.end());
    if (values.isEmpty())
        return 0;
    std::sort(values.begin(), values.end());
    const double pos = qBound(0.0, p, 1.0) * (values.size() - 1);
    const int lo = static_cast<int>(std::floor(pos));
    const int hi = static_cast<int>(std::ceil(pos));
    return values[lo] + (values[hi] - values[lo]) * (pos - lo);
}

} // namespace

MonthlyReturnsHeatmap::MonthlyReturnsHeatmap(QWidget* parent) : PaintedChart(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

MonthlyReturnsHeatmap::MonthlyReturns
MonthlyReturnsHeatmap::monthly_from_equity(const QVector<QPair<QDate, double>>& equity) {
    QVector<QPair<QDate, double>> pts;
    pts.reserve(equity.size());
    for (const auto& pt : equity) {
        if (pt.first.isValid() && std::isfinite(pt.second) && pt.second > 0)
            pts.append(pt);
    }
    std::stable_sort(pts.begin(), pts.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    MonthlyReturns out;
    if (pts.isEmpty())
        return out;
    double base = pts.first().second; // the previous month's closing value
    int cur_key = pts.first().first.year() * 12 + pts.first().first.month() - 1;
    double last = base;
    const auto close_month = [&out](int key, double from, double to) {
        if (from > 0)
            out[key / 12][key % 12 + 1] = (to / from - 1.0) * 100.0;
    };
    for (const auto& pt : pts) {
        const int key = pt.first.year() * 12 + pt.first.month() - 1;
        if (key != cur_key) {
            close_month(cur_key, base, last);
            base = last;
            cur_key = key;
        }
        last = pt.second;
    }
    close_month(cur_key, base, last);
    return out;
}

MonthlyReturnsHeatmap::MonthlyReturns
MonthlyReturnsHeatmap::monthly_from_daily_returns(const QVector<QPair<QDate, double>>& returns_pct) {
    QMap<int, double> growth; // year*12+month-1 → compounded growth factor
    for (const auto& pt : returns_pct) {
        if (!pt.first.isValid() || !std::isfinite(pt.second))
            continue;
        const int key = pt.first.year() * 12 + pt.first.month() - 1;
        growth[key] = growth.value(key, 1.0) * (1.0 + pt.second / 100.0);
    }
    MonthlyReturns out;
    for (auto it = growth.constBegin(); it != growth.constEnd(); ++it)
        out[it.key() / 12][it.key() % 12 + 1] = (it.value() - 1.0) * 100.0;
    return out;
}

double MonthlyReturnsHeatmap::compound_pct(const QMap<int, double>& months) {
    double g = 1.0;
    bool any = false;
    for (double r : months) {
        if (std::isfinite(r)) {
            g *= 1.0 + r / 100.0;
            any = true;
        }
    }
    return any ? (g - 1.0) * 100.0 : std::nan("");
}

void MonthlyReturnsHeatmap::set_monthly_returns(const MonthlyReturns& returns) {
    data_.clear();
    for (auto y = returns.constBegin(); y != returns.constEnd(); ++y) {
        for (auto m = y.value().constBegin(); m != y.value().constEnd(); ++m) {
            if (m.key() >= 1 && m.key() <= 12 && std::isfinite(m.value()))
                data_[y.key()][m.key()] = m.value();
        }
    }
    recompute();
}

void MonthlyReturnsHeatmap::set_equity_curve(const QVector<QPair<QDate, double>>& equity) {
    set_monthly_returns(monthly_from_equity(equity));
}

void MonthlyReturnsHeatmap::set_daily_returns(const QVector<QPair<QDate, double>>& returns_pct) {
    set_monthly_returns(monthly_from_daily_returns(returns_pct));
}

void MonthlyReturnsHeatmap::clear() {
    set_monthly_returns({});
}

void MonthlyReturnsHeatmap::set_clamp_percentile(double p) {
    percentile_ = qBound(0.5, p, 1.0);
    recompute();
}

void MonthlyReturnsHeatmap::set_fixed_clamp(double clamp_pct) {
    fixed_clamp_ = clamp_pct > 0 ? clamp_pct : 0;
    recompute();
}

void MonthlyReturnsHeatmap::set_show_year_total(bool show) {
    show_total_ = show;
    updateGeometry();
    invalidate();
}

void MonthlyReturnsHeatmap::recompute() {
    years_ = data_.keys();
    totals_.clear();
    QVector<double> months;
    QVector<double> totals;
    for (auto y = data_.constBegin(); y != data_.constEnd(); ++y) {
        for (double v : y.value())
            months.append(v);
        const double t = compound_pct(y.value());
        totals_[y.key()] = t;
        totals.append(t);
    }
    clamp_ = fixed_clamp_ > 0 ? fixed_clamp_ : std::max(0.5, mr_abs_percentile(months, percentile_));
    total_clamp_ = std::max(1.0, totals.size() >= 4 ? mr_abs_percentile(totals, percentile_)
                                                    : mr_abs_percentile(totals, 1.0));
    hover_row_ = hover_month_ = -1;
    updateGeometry();
    invalidate();
}

QString MonthlyReturnsHeatmap::default_format(double v) const {
    return chart_style::percent_text(v, 1);
}

QSize MonthlyReturnsHeatmap::sizeHint() const {
    const QFontMetricsF fm(chart_style::font(-3));
    const int rows = qMax(3, static_cast<int>(years_.size()));
    const int cols = show_total_ ? 13 : 12;
    return {qCeil(fm.horizontalAdvance(QStringLiteral("0000")) + 12 + cols * 54.0),
            qCeil((fm.height() + 6) + rows * (fm.height() + 10))};
}

QSize MonthlyReturnsHeatmap::minimumSizeHint() const {
    const QFontMetricsF fm(chart_style::font(-3));
    const int rows = qMax(1, static_cast<int>(years_.size()));
    const int cols = show_total_ ? 13 : 12;
    return {qCeil(fm.horizontalAdvance(QStringLiteral("0000")) + 8 + cols * 22.0),
            qCeil((fm.height() + 4) + rows * (fm.height() + 2))};
}

MonthlyReturnsHeatmap::Geometry MonthlyReturnsHeatmap::geometry(const QRectF& r) const {
    Geometry g;
    const QFontMetricsF fm(chart_style::font(-3));
    const int rows = qMax(1, static_cast<int>(years_.size()));
    g.year_w = fm.horizontalAdvance(QStringLiteral("0000")) + 10;
    g.total_gap = show_total_ ? 6 : 0;
    const int cols = show_total_ ? 13 : 12;
    g.cell_w = std::max<qreal>(1, (r.width() - g.year_w - g.total_gap) / cols);
    const qreal header_h = fm.height() + 6;
    g.cell_h = qBound<qreal>(fm.height() + 2, (r.height() - header_h) / rows, fm.height() * 2 + 10);
    g.left = r.left() + g.year_w;
    g.header = QRectF(g.left, r.top(), g.cell_w * cols + g.total_gap, header_h);
    g.top = r.top() + header_h;
    return g;
}

QRectF MonthlyReturnsHeatmap::cell_rect(const Geometry& g, int row, int month) const {
    const qreal y = g.top + row * g.cell_h;
    if (month == 0)
        return QRectF(g.left + 12 * g.cell_w + g.total_gap, y, g.cell_w, g.cell_h);
    return QRectF(g.left + (month - 1) * g.cell_w, y, g.cell_w, g.cell_h);
}

void MonthlyReturnsHeatmap::paint_chart(QPainter& p, const QRectF& r) {
    const Geometry g = geometry(r);
    const QLocale loc;
    const QFont hf = chart_style::font(-3, QFont::DemiBold);
    const QFontMetricsF hfm(hf);

    // Header: month names, then "Year".
    p.setFont(hf);
    p.setPen(chart_style::axis_text_color());
    for (int m = 1; m <= 12; ++m) {
        const QRectF c(g.left + (m - 1) * g.cell_w, g.header.top(), g.cell_w, g.header.height() - 2);
        QString name = loc.monthName(m, QLocale::ShortFormat);
        if (hfm.horizontalAdvance(name) > c.width() - 2)
            name = loc.monthName(m, QLocale::NarrowFormat);
        p.drawText(c, Qt::AlignCenter, name);
    }
    if (show_total_) {
        const QRectF c(g.left + 12 * g.cell_w + g.total_gap, g.header.top(), g.cell_w, g.header.height() - 2);
        p.drawText(c, Qt::AlignCenter, hfm.elidedText(tr("Year"), Qt::ElideRight, c.width()));
    }

    // Value font: the largest that fits the widest value text.
    QStringList texts;
    for (auto y = data_.constBegin(); y != data_.constEnd(); ++y) {
        for (double v : y.value())
            texts.append(format_value(v));
        if (show_total_)
            texts.append(format_value(totals_.value(y.key())));
    }
    QFont vf;
    bool text_fits = false;
    for (int offset = -3; offset >= -6 && !text_fits; --offset) {
        const QFont f = chart_style::font(offset);
        const QFontMetricsF fm(f);
        qreal widest = 0;
        for (const QString& t : texts)
            widest = std::max(widest, fm.horizontalAdvance(t));
        if (widest + 4 <= g.cell_w && fm.height() <= g.cell_h) {
            vf = f;
            text_fits = true;
        }
    }
    QFont vf_bold = vf;
    vf_bold.setWeight(QFont::Bold);

    for (int row = 0; row < years_.size(); ++row) {
        const int year = years_.at(row);
        p.setFont(hf);
        p.setPen(chart_style::axis_text_color());
        p.drawText(QRectF(r.left(), g.top + row * g.cell_h, g.year_w - 6, g.cell_h), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(year));
        const QMap<int, double> months = data_.value(year);
        for (int m = 1; m <= 12; ++m) {
            const QRectF c = cell_rect(g, row, m).adjusted(1, 1, -1, -1);
            const auto it = months.constFind(m);
            if (it == months.constEnd()) {
                p.fillRect(c, chart_style::empty_cell_color());
                continue;
            }
            const QColor fill = colors::diverging_color(it.value() / clamp_);
            p.fillRect(c, fill);
            if (text_fits) {
                p.setFont(vf);
                p.setPen(colors::contrast_text_on(fill));
                p.drawText(c, Qt::AlignCenter, format_value(it.value()));
            }
        }
        if (show_total_) {
            const QRectF c = cell_rect(g, row, 0).adjusted(1, 1, -1, -1);
            const double t = totals_.value(year);
            const QColor fill = colors::diverging_color(t / total_clamp_);
            p.fillRect(c, fill);
            if (text_fits) {
                p.setFont(vf_bold);
                p.setPen(colors::contrast_text_on(fill));
                p.drawText(c, Qt::AlignCenter, format_value(t));
            }
        }
    }
}

void MonthlyReturnsHeatmap::paint_overlay(QPainter& p, const QRectF& r) {
    if (hover_row_ < 0)
        return;
    const Geometry g = geometry(r);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(chart_style::highlight_color(), 1.5));
    p.drawRect(cell_rect(g, hover_row_, hover_month_).adjusted(1, 1, -1, -1));
}

bool MonthlyReturnsHeatmap::hit(const QPointF& pos, int* row, int* month) const {
    const Geometry g = geometry(content_rect());
    for (int rr = 0; rr < years_.size(); ++rr) {
        const qreal y = g.top + rr * g.cell_h;
        if (pos.y() < y || pos.y() >= y + g.cell_h)
            continue;
        for (int m = show_total_ ? 0 : 1; m <= 12; ++m) {
            if (cell_rect(g, rr, m).contains(pos)) {
                *row = rr;
                *month = m;
                return true;
            }
        }
    }
    return false;
}

void MonthlyReturnsHeatmap::mouseMoveEvent(QMouseEvent* e) {
    if (!chart_visible())
        return;
    int row = -1;
    int month = -1;
    const QPointF pos = e->position();
    if (!hit(pos, &row, &month)) {
        if (hover_row_ >= 0) {
            hover_row_ = hover_month_ = -1;
            update();
        }
        hide_tip();
        return;
    }
    if (row != hover_row_ || month != hover_month_) {
        hover_row_ = row;
        hover_month_ = month;
        update();
    }
    const int year = years_.at(row);
    QString tip;
    if (month == 0) {
        const QMap<int, double>& months = data_.value(year);
        tip = chart_style::tooltip_html(QString::number(year),
                                        {{tr("Year return"), format_value(totals_.value(year))},
                                         {tr("Months"), QString::number(months.size())}});
    } else {
        const QMap<int, double> months = data_.value(year);
        const auto it = months.constFind(month);
        const QString title = QLocale().monthName(month, QLocale::LongFormat) + QLatin1Char(' ') + QString::number(year);
        tip = chart_style::tooltip_html(
            title, {{tr("Return"), it == months.constEnd() ? tr("n/a") : format_value(it.value())}});
    }
    show_tip(pos.toPoint(), tip, cell_rect(geometry(content_rect()), row, month).toAlignedRect());
}

void MonthlyReturnsHeatmap::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && chart_visible()) {
        int row = -1;
        int month = -1;
        if (hit(e->position(), &row, &month))
            emit cell_clicked(years_.at(row), month);
    }
    PaintedChart::mouseReleaseEvent(e);
}

void MonthlyReturnsHeatmap::on_hover_left() {
    hover_row_ = hover_month_ = -1;
}

} // namespace fincept::ui
