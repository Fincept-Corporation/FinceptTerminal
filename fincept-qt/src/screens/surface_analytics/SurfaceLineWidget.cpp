#include "SurfaceLineWidget.h"

#include "ui/theme/Theme.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::surface {

using namespace fincept::ui;

SurfaceLineWidget::SurfaceLineWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(320, 240);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void SurfaceLineWidget::clear() {
    series_.clear();
    x_labels_.clear();
    title_.clear();
    update();
}

void SurfaceLineWidget::set_curve(const QString& title,
                                  const std::vector<float>& x_values,
                                  const std::vector<float>& y_values,
                                  const QStringList& x_labels,
                                  const QString& x_axis_title,
                                  const QString& y_axis_title,
                                  const QColor& line_color) {
    Series s;
    s.name = title;
    s.x_values = x_values;
    s.y_values = y_values;
    s.color = line_color;
    title_ = title;
    x_labels_ = x_labels;
    x_axis_title_ = x_axis_title;
    y_axis_title_ = y_axis_title;
    series_ = {s};
    recompute_bounds();
    update();
}

void SurfaceLineWidget::set_series(const QString& title,
                                   const std::vector<Series>& series,
                                   const QString& x_axis_title,
                                   const QString& y_axis_title) {
    title_ = title;
    series_ = series;
    x_axis_title_ = x_axis_title;
    y_axis_title_ = y_axis_title;
    x_labels_.clear();
    recompute_bounds();
    update();
}

void SurfaceLineWidget::recompute_bounds() {
    x_min_ = std::numeric_limits<float>::infinity();
    x_max_ = -std::numeric_limits<float>::infinity();
    y_min_ = std::numeric_limits<float>::infinity();
    y_max_ = -std::numeric_limits<float>::infinity();
    for (const auto& s : series_) {
        for (float v : s.x_values) {
            x_min_ = std::min(x_min_, v);
            x_max_ = std::max(x_max_, v);
        }
        for (float v : s.y_values) {
            y_min_ = std::min(y_min_, v);
            y_max_ = std::max(y_max_, v);
        }
    }
    if (!std::isfinite(x_min_) || x_min_ == x_max_) {
        x_min_ = 0;
        x_max_ = 1;
    }
    if (!std::isfinite(y_min_) || y_min_ == y_max_) {
        y_min_ = 0;
        y_max_ = 1;
    }
    // Add ~5% headroom on the y-axis so the curve doesn't kiss the frame
    float pad = (y_max_ - y_min_) * 0.05f;
    y_min_ -= pad;
    y_max_ += pad;
}

QPointF SurfaceLineWidget::data_to_pixel(float x, float y, const QRectF& plot) const {
    float fx = (x - x_min_) / (x_max_ - x_min_);
    float fy = (y - y_min_) / (y_max_ - y_min_);
    return QPointF(plot.left() + fx * plot.width(), plot.bottom() - fy * plot.height());
}

void SurfaceLineWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(colors::BG_BASE()));

    if (series_.empty()) {
        p.setPen(QColor(colors::TEXT_DIM()));
        p.drawText(rect(), Qt::AlignCenter, "No data");
        return;
    }

    QRectF plot(60, 30, width() - 80, height() - 70);

    // ── Frame ───────────────────────────────────────────────────────────────
    p.setPen(QPen(QColor(colors::BORDER_DIM()), 1));
    p.drawRect(plot);

    // ── Y gridlines + tick labels (5 ticks) ─────────────────────────────────
    p.setPen(QColor(colors::TEXT_DIM()));
    for (int i = 0; i <= 4; ++i) {
        float frac = i / 4.0f;
        float v = y_min_ + frac * (y_max_ - y_min_);
        float py = plot.bottom() - frac * plot.height();
        QPen grid(QColor(colors::BORDER_DIM()), 1, Qt::DotLine);
        p.setPen(grid);
        p.drawLine(QPointF(plot.left(), py), QPointF(plot.right(), py));
        p.setPen(QColor(colors::TEXT_DIM()));
        p.drawText(QRectF(0, py - 8, 55, 16), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(v, 'f', 2));
    }

    // ── X tick labels ───────────────────────────────────────────────────────
    int n_ticks = std::min(8, !x_labels_.isEmpty() ? (int)x_labels_.size() : 6);
    for (int i = 0; i < n_ticks; ++i) {
        float frac = (n_ticks <= 1) ? 0.5f : (float)i / (n_ticks - 1);
        float px = plot.left() + frac * plot.width();
        QString label;
        if (!x_labels_.isEmpty()) {
            int idx = std::min((int)x_labels_.size() - 1, i * ((int)x_labels_.size() - 1) /
                                                              std::max(1, n_ticks - 1));
            label = x_labels_.at(idx);
        } else {
            float v = x_min_ + frac * (x_max_ - x_min_);
            label = QString::number(v, 'f', 1);
        }
        p.setPen(QColor(colors::TEXT_DIM()));
        p.drawText(QRectF(px - 40, plot.bottom() + 4, 80, 16), Qt::AlignCenter, label);
    }

    // ── Curve(s) ────────────────────────────────────────────────────────────
    for (const auto& s : series_) {
        if (s.x_values.empty() || s.y_values.size() != s.x_values.size())
            continue;
        QPainterPath path;
        for (size_t i = 0; i < s.x_values.size(); ++i) {
            QPointF pt = data_to_pixel(s.x_values[i], s.y_values[i], plot);
            if (i == 0)
                path.moveTo(pt);
            else
                path.lineTo(pt);
        }
        p.setPen(QPen(s.color, 2));
        p.drawPath(path);

        // Markers at each point
        p.setBrush(s.color);
        for (size_t i = 0; i < s.x_values.size(); ++i) {
            QPointF pt = data_to_pixel(s.x_values[i], s.y_values[i], plot);
            p.drawEllipse(pt, 2.5, 2.5);
        }
        p.setBrush(Qt::NoBrush);
    }

    // ── Title + axis labels ─────────────────────────────────────────────────
    p.setPen(QColor(colors::TEXT_PRIMARY()));
    QFont f = p.font();
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRectF(0, 4, width(), 22), Qt::AlignCenter, title_);
    f.setBold(false);
    p.setFont(f);

    p.setPen(QColor(colors::TEXT_SECONDARY()));
    if (!x_axis_title_.isEmpty())
        p.drawText(QRectF(plot.left(), plot.bottom() + 22, plot.width(), 16),
                   Qt::AlignCenter, x_axis_title_);
    if (!y_axis_title_.isEmpty()) {
        p.save();
        p.translate(14, plot.center().y());
        p.rotate(-90);
        p.drawText(QRectF(-100, -8, 200, 16), Qt::AlignCenter, y_axis_title_);
        p.restore();
    }

    // ── Legend (only if >1 series) ──────────────────────────────────────────
    if (series_.size() > 1) {
        int y = (int)plot.top() + 4;
        for (const auto& s : series_) {
            p.setPen(QPen(s.color, 2));
            p.drawLine(QPointF(plot.right() - 110, y + 6), QPointF(plot.right() - 95, y + 6));
            p.setPen(QColor(colors::TEXT_SECONDARY()));
            p.drawText(QRectF(plot.right() - 90, y, 90, 14), Qt::AlignLeft | Qt::AlignVCenter,
                       s.name);
            y += 16;
        }
    }
}

} // namespace fincept::surface
