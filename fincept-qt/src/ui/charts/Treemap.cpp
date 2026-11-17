#include "ui/charts/Treemap.h"

#include "ui/theme/Theme.h"

#include <QFontMetricsF>
#include <QHash>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace fincept::ui {

namespace {

/// Worst aspect ratio of a row of `areas` laid along a side of length `side`.
double tm_worst_ratio(double sum, double max_a, double min_a, double side) {
    if (sum <= 0 || min_a <= 0 || side <= 0)
        return std::numeric_limits<double>::max();
    const double s2 = side * side;
    const double sum2 = sum * sum;
    return std::max(s2 * max_a / sum2, sum2 / (s2 * min_a));
}

/// Squarified treemap (Bruls, Huizing & van Wijk): `weights` must be sorted
/// descending and positive. Returns one rect per weight, in the same order.
QVector<QRectF> tm_squarify(const QVector<double>& weights, const QRectF& rect) {
    QVector<QRectF> out(weights.size());
    const double total = std::accumulate(weights.begin(), weights.end(), 0.0);
    if (weights.isEmpty() || total <= 0 || rect.width() <= 0 || rect.height() <= 0)
        return out;
    const double scale = rect.width() * rect.height() / total;
    QRectF r = rect;
    int i = 0;
    const int n = static_cast<int>(weights.size());
    while (i < n) {
        const double side = std::min(r.width(), r.height());
        double sum = weights[i] * scale;
        double max_a = sum;
        double min_a = sum;
        double best = tm_worst_ratio(sum, max_a, min_a, side);
        int j = i + 1;
        while (j < n) {
            const double a = weights[j] * scale;
            const double w = tm_worst_ratio(sum + a, std::max(max_a, a), std::min(min_a, a), side);
            if (w > best)
                break;
            sum += a;
            max_a = std::max(max_a, a);
            min_a = std::min(min_a, a);
            best = w;
            ++j;
        }
        // The last strip takes exactly what is left (no floating-point sliver).
        const bool last_strip = j == n;
        const bool vertical_strip = r.width() >= r.height();
        if (vertical_strip) {
            const double strip_w = last_strip ? r.width() : std::min(r.width(), sum / std::max(r.height(), 1e-9));
            double y = r.top();
            for (int k = i; k < j; ++k) {
                const double a = weights[k] * scale;
                const double h = (k == j - 1) ? r.bottom() - y : a / std::max(strip_w, 1e-9);
                out[k] = QRectF(r.left(), y, strip_w, h);
                y += h;
            }
            r.setLeft(r.left() + strip_w);
        } else {
            const double strip_h = last_strip ? r.height() : std::min(r.height(), sum / std::max(r.width(), 1e-9));
            double x = r.left();
            for (int k = i; k < j; ++k) {
                const double a = weights[k] * scale;
                const double w = (k == j - 1) ? r.right() - x : a / std::max(strip_h, 1e-9);
                out[k] = QRectF(x, r.top(), w, strip_h);
                x += w;
            }
            r.setTop(r.top() + strip_h);
        }
        i = j;
    }
    return out;
}

} // namespace

Treemap::Treemap(QWidget* parent) : PaintedChart(parent) {
    setMinimumSize(minimumSizeHint());
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void Treemap::set_items(const QVector<TreemapItem>& items) {
    items_ = items;
    hover_cell_ = -1;
    mark_dirty();
}

void Treemap::clear() {
    set_items({});
}

void Treemap::set_grouped(bool grouped) {
    if (grouped_ == grouped)
        return;
    grouped_ = grouped;
    mark_dirty();
}

void Treemap::set_color_clamp(double clamp) {
    clamp_ = clamp > 0 ? clamp : 3.0;
    invalidate();
}

void Treemap::set_show_legend(bool show) {
    if (show_legend_ == show)
        return;
    show_legend_ = show;
    mark_dirty();
}

void Treemap::set_weight_formatter(Formatter formatter) {
    weight_formatter_ = std::move(formatter);
}

void Treemap::set_tooltip_provider(std::function<QString(const TreemapItem&)> provider) {
    tooltip_provider_ = std::move(provider);
}

QString Treemap::default_format(double v) const {
    return chart_style::percent_text(v, 2);
}

bool Treemap::has_data() const {
    return std::any_of(items_.begin(), items_.end(), [](const TreemapItem& it) { return it.weight > 0; });
}

void Treemap::mark_dirty() {
    layout_valid_ = false;
    invalidate();
}

QColor Treemap::fill_for(double value) const {
    if (std::isnan(value))
        return QColor(colors::CHART_NEUTRAL());
    return colors::diverging_color(value / clamp_);
}

void Treemap::ensure_layout(const QRectF& r) {
    if (layout_valid_ && layout_for_ == r)
        return;
    layout_for_ = r;
    layout_valid_ = true;
    cells_.clear();
    groups_.clear();
    legend_rect_ = QRectF();

    QRectF plot = r;
    if (show_legend_ && r.height() > 140 && r.width() > 200) {
        const qreal lh = chart_style::legend_height();
        const qreal lw = std::min<qreal>(260.0, r.width() * 0.45);
        legend_rect_ = QRectF(r.right() - lw, r.bottom() - lh, lw, lh);
        plot.setBottom(legend_rect_.top() - 6);
    }

    // Group the drawable items (insertion order of first appearance).
    QVector<QString> order;
    QHash<QString, QVector<int>> members;
    for (int i = 0; i < items_.size(); ++i) {
        const TreemapItem& it = items_.at(i);
        if (!(it.weight > 0) || !std::isfinite(it.weight))
            continue;
        const QString key = grouped_ ? (it.group.isEmpty() ? tr("Other") : it.group) : QString();
        if (!members.contains(key))
            order.append(key);
        members[key].append(i);
    }
    if (order.isEmpty())
        return;

    const auto by_weight_desc = [this](int a, int b) { return items_.at(a).weight > items_.at(b).weight; };

    struct Pending {
        QString name;
        QVector<int> idx;
        double weight = 0;
    };
    QVector<Pending> pend;
    for (const QString& key : order) {
        Pending pg;
        pg.name = key;
        pg.idx = members.value(key);
        std::sort(pg.idx.begin(), pg.idx.end(), by_weight_desc);
        for (int i : pg.idx)
            pg.weight += items_.at(i).weight;
        pend.append(pg);
    }
    std::sort(pend.begin(), pend.end(), [](const Pending& a, const Pending& b) { return a.weight > b.weight; });

    QVector<double> gw;
    for (const Pending& pg : pend)
        gw.append(pg.weight);
    const QVector<QRectF> group_rects = grouped_ ? tm_squarify(gw, plot) : QVector<QRectF>{plot};

    const QFontMetricsF header_fm(chart_style::font(-3, QFont::Bold));
    const qreal header_h = std::ceil(header_fm.height() + 4);

    for (int g = 0; g < pend.size() && g < group_rects.size(); ++g) {
        const Pending& pg = pend.at(g);
        GroupBox box;
        box.name = pg.name;
        box.rect = grouped_ ? group_rects.at(g).adjusted(1, 1, -1, -1) : group_rects.at(g);
        box.weight = pg.weight;
        double wv = 0;
        double wsum = 0;
        for (int i : pg.idx) {
            const double v = items_.at(i).value;
            if (std::isfinite(v)) {
                wv += v * items_.at(i).weight;
                wsum += items_.at(i).weight;
            }
        }
        box.value = wsum > 0 ? wv / wsum : std::nan("");

        QRectF inner = box.rect;
        if (grouped_ && box.rect.height() > header_h * 2.5 && box.rect.width() > 48) {
            box.header = QRectF(box.rect.left(), box.rect.top(), box.rect.width(), header_h);
            inner.setTop(box.header.bottom());
        }
        QVector<double> w;
        w.reserve(pg.idx.size());
        for (int i : pg.idx)
            w.append(items_.at(i).weight);
        const QVector<QRectF> rects = tm_squarify(w, inner);
        for (int k = 0; k < pg.idx.size() && k < rects.size(); ++k)
            cells_.append(Cell{pg.idx.at(k), rects.at(k)});
        groups_.append(box);
    }
}

void Treemap::paint_chart(QPainter& p, const QRectF& r) {
    ensure_layout(r);

    // Cells: 1 px surface gap between neighbours (the parent background shows through).
    for (const Cell& c : cells_) {
        const TreemapItem& it = items_.at(c.item);
        const QRectF cell = c.rect.adjusted(0.5, 0.5, -0.5, -0.5);
        if (cell.width() < 1 || cell.height() < 1)
            continue;
        const QColor fill = fill_for(it.value);
        p.fillRect(cell, fill);

        const qreal side = std::min(cell.width(), cell.height());
        const int max_offset = side > 160 ? 8 : (side > 110 ? 4 : (side > 70 ? 0 : (side > 44 ? -2 : -4)));
        QStringList lines{it.label, std::isnan(it.value) ? QString() : format_value(it.value)};
        if (!it.sublabel.isEmpty() && side > 70)
            lines.append(it.sublabel);
        chart_style::draw_cell_text(p, cell, lines, colors::contrast_text_on(fill), max_offset, -6);
    }

    // Group headers.
    if (grouped_) {
        const QFont hf = chart_style::font(-3, QFont::Bold);
        const QFontMetricsF fm(hf);
        for (const GroupBox& g : groups_) {
            if (g.header.isEmpty())
                continue;
            p.fillRect(g.header.adjusted(0.5, 0.5, -0.5, 0), QColor(colors::BG_RAISED()));
            p.setFont(hf);
            const QRectF tr_rect = g.header.adjusted(6, 0, -6, 0);
            QString value_text;
            if (std::isfinite(g.value))
                value_text = format_value(g.value);
            const qreal vw = value_text.isEmpty() ? 0 : fm.horizontalAdvance(value_text) + 8;
            p.setPen(QColor(colors::TEXT_PRIMARY()));
            p.drawText(tr_rect.adjusted(0, 0, -vw, 0), Qt::AlignLeft | Qt::AlignVCenter,
                       fm.elidedText(g.name.toUpper(), Qt::ElideRight, std::max<qreal>(0, tr_rect.width() - vw)));
            if (!value_text.isEmpty() && tr_rect.width() > vw + 40) {
                p.setPen(QColor(change_color(g.value)));
                p.drawText(tr_rect, Qt::AlignRight | Qt::AlignVCenter, value_text);
            }
        }
    }

    if (!legend_rect_.isEmpty()) {
        chart_style::draw_scale_legend(p, legend_rect_, true, -clamp_, clamp_,
                                       [this](double v) { return format_value(v); });
    }
}

void Treemap::paint_overlay(QPainter& p, const QRectF&) {
    if (hover_cell_ < 0 || hover_cell_ >= cells_.size())
        return;
    const QRectF rc = cells_.at(hover_cell_).rect.adjusted(1, 1, -1, -1);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(chart_style::highlight_color(), 2));
    p.drawRect(rc);
}

int Treemap::cell_at(const QPointF& pos) const {
    for (int i = 0; i < cells_.size(); ++i) {
        if (cells_.at(i).rect.contains(pos))
            return i;
    }
    return -1;
}

int Treemap::header_at(const QPointF& pos) const {
    for (int i = 0; i < groups_.size(); ++i) {
        if (!groups_.at(i).header.isEmpty() && groups_.at(i).header.contains(pos))
            return i;
    }
    return -1;
}

void Treemap::mouseMoveEvent(QMouseEvent* e) {
    if (!chart_visible())
        return;
    ensure_layout(content_rect());
    const QPointF pos = e->position();
    const int hit = cell_at(pos);
    if (hit != hover_cell_) {
        hover_cell_ = hit;
        update();
    }
    if (hit < 0) {
        const int h = header_at(pos);
        if (h >= 0) {
            const GroupBox& g = groups_.at(h);
            show_tip(pos.toPoint(),
                     chart_style::tooltip_html(g.name, {{tr("Change"), std::isfinite(g.value) ? format_value(g.value)
                                                                                             : QStringLiteral("-")},
                                                         {tr("Weight"), weight_formatter_
                                                                            ? weight_formatter_(g.weight)
                                                                            : chart_style::compact_number(g.weight)}}),
                     g.header.toAlignedRect());
        } else {
            hide_tip();
        }
        setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        return;
    }
    setCursor(Qt::PointingHandCursor);
    const TreemapItem& it = items_.at(cells_.at(hit).item);
    QString tip;
    if (tooltip_provider_) {
        tip = tooltip_provider_(it);
    } else {
        QVector<QPair<QString, QString>> rows;
        if (!it.sublabel.isEmpty())
            rows.append({tr("Name"), it.sublabel});
        if (!it.group.isEmpty())
            rows.append({tr("Group"), it.group});
        rows.append({tr("Change"), std::isnan(it.value) ? QStringLiteral("-") : format_value(it.value)});
        rows.append({tr("Weight"), weight_formatter_ ? weight_formatter_(it.weight)
                                                     : chart_style::compact_number(it.weight)});
        tip = chart_style::tooltip_html(it.label, rows);
    }
    show_tip(pos.toPoint(), tip, cells_.at(hit).rect.toAlignedRect());
}

void Treemap::mousePressEvent(QMouseEvent* e) {
    press_cell_ = -1;
    press_header_ = -1;
    if (e->button() == Qt::LeftButton && chart_visible()) {
        ensure_layout(content_rect());
        press_cell_ = cell_at(e->position());
        if (press_cell_ < 0)
            press_header_ = header_at(e->position());
    }
    PaintedChart::mousePressEvent(e);
}

void Treemap::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && chart_visible()) {
        const int cell = cell_at(e->position());
        if (cell >= 0 && cell == press_cell_)
            emit item_clicked(items_.at(cells_.at(cell).item).id);
        const int header = header_at(e->position());
        if (cell < 0 && header >= 0 && header == press_header_)
            emit group_clicked(groups_.at(header).name);
    }
    press_cell_ = -1;
    press_header_ = -1;
    PaintedChart::mouseReleaseEvent(e);
}

void Treemap::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && chart_visible()) {
        const int cell = cell_at(e->position());
        if (cell >= 0)
            emit item_double_clicked(items_.at(cells_.at(cell).item).id);
    }
    PaintedChart::mouseDoubleClickEvent(e);
}

void Treemap::on_hover_left() {
    hover_cell_ = -1;
    unsetCursor();
}

} // namespace fincept::ui
