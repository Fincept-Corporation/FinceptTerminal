#include "SurfaceLineWidget.h"

#include "ui/theme/Theme.h"

#include <QFontMetrics>
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

void SurfaceLineWidget::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        update(); // placeholder text is painter-drawn; repaint picks up new tr()
    QWidget::changeEvent(event);
}

void SurfaceLineWidget::set_curve(const QString& title, const std::vector<float>& x_values,
                                  const std::vector<float>& y_values, const QStringList& x_labels,
                                  const QString& x_axis_title, const QString& y_axis_title, const QColor& line_color) {
    Series s;
    s.name = title;
    s.x_values = x_values;
    s.y_values = y_values;
    s.color = line_color;
    s.slot = line_color.isValid() ? -1 : 0;
    title_ = title;
    x_labels_ = x_labels;
    x_axis_title_ = x_axis_title;
    y_axis_title_ = y_axis_title;
    series_ = {s};
    recompute_bounds();
    update();
}

void SurfaceLineWidget::set_series(const QString& title, const std::vector<Series>& series, const QString& x_axis_title,
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
        p.drawText(rect(), Qt::AlignCenter, tr("No data"));
        return;
    }

    // Margins fit the text: 60 / 30 / 20 / 40 px at the default font size.
    const QFontMetrics fm(p.font());
    const int lh = fm.height();
    int y_label_w = 0;
    for (int i = 0; i <= 4; ++i)
        y_label_w = std::max(y_label_w, fm.horizontalAdvance(QString::number(y_min_ + i / 4.0f * (y_max_ - y_min_), 'f', 2)));
    const int left = std::max(60, y_label_w + lh + 10);
    const int top = std::max(30, lh + 12);
    const int bottom = std::max(40, 2 * lh + 8);
    QRectF plot(left, top, width() - left - 20, height() - top - bottom);

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
        p.drawText(QRectF(0, py - lh / 2.0, left - 5, lh), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(v, 'f', 2));
    }

    // ── X tick labels ───────────────────────────────────────────────────────
    int n_ticks = std::min(8, !x_labels_.isEmpty() ? (int)x_labels_.size() : 6);
    for (int i = 0; i < n_ticks; ++i) {
        float frac = (n_ticks <= 1) ? 0.5f : (float)i / (n_ticks - 1);
        float px = plot.left() + frac * plot.width();
        QString label;
        if (!x_labels_.isEmpty()) {
            int idx = std::min((int)x_labels_.size() - 1, i * ((int)x_labels_.size() - 1) / std::max(1, n_ticks - 1));
            label = x_labels_.at(idx);
        } else {
            float v = x_min_ + frac * (x_max_ - x_min_);
            label = QString::number(v, 'f', 1);
        }
        p.setPen(QColor(colors::TEXT_DIM()));
        const qreal tick_w = std::max(80, fm.horizontalAdvance(label) + 8);
        p.drawText(QRectF(px - tick_w / 2, plot.bottom() + 4, tick_w, lh), Qt::AlignCenter, label);
    }

    // Series colour: its categorical chart slot, or its own colour made legible.
    auto series_color = [](const Series& s) {
        return s.slot >= 0 ? colors::chart_color(s.slot) : colors::legible_color(s.color);
    };

    // ── Curve(s) ────────────────────────────────────────────────────────────
    for (const auto& s : series_) {
        if (s.x_values.empty() || s.y_values.size() != s.x_values.size())
            continue;
        const QColor col = series_color(s);
        QPainterPath path;
        for (size_t i = 0; i < s.x_values.size(); ++i) {
            QPointF pt = data_to_pixel(s.x_values[i], s.y_values[i], plot);
            if (i == 0)
                path.moveTo(pt);
            else
                path.lineTo(pt);
        }
        p.setPen(QPen(col, 2));
        p.drawPath(path);

        // Markers at each point
        p.setBrush(col);
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
    p.drawText(QRectF(0, 4, width(), std::max(22, lh + 6)), Qt::AlignCenter, title_);
    f.setBold(false);
    p.setFont(f);

    p.setPen(QColor(colors::TEXT_SECONDARY()));
    if (!x_axis_title_.isEmpty())
        p.drawText(QRectF(plot.left(), plot.bottom() + 6 + lh, plot.width(), lh), Qt::AlignCenter, x_axis_title_);
    if (!y_axis_title_.isEmpty()) {
        p.save();
        p.translate(lh / 2.0 + 6, plot.center().y());
        p.rotate(-90);
        const qreal title_w = std::max(200.0, plot.height());
        p.drawText(QRectF(-title_w / 2, -lh / 2.0, title_w, lh), Qt::AlignCenter, y_axis_title_);
        p.restore();
    }

    // ── Legend (only if >1 series) ──────────────────────────────────────────
    if (series_.size() > 1) {
        int name_w = 90;
        for (const auto& s : series_)
            name_w = std::max(name_w, fm.horizontalAdvance(s.name) + 4);
        const int row_h = std::max(16, lh);
        int y = (int)plot.top() + 4;
        for (const auto& s : series_) {
            p.setPen(QPen(series_color(s), 2));
            p.drawLine(QPointF(plot.right() - name_w - 20, y + row_h / 2.0),
                       QPointF(plot.right() - name_w - 5, y + row_h / 2.0));
            p.setPen(QColor(colors::TEXT_SECONDARY()));
            p.drawText(QRectF(plot.right() - name_w, y, name_w, row_h), Qt::AlignLeft | Qt::AlignVCenter, s.name);
            y += row_h;
        }
    }
}

} // namespace fincept::surface
