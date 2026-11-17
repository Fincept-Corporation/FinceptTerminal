#include "ui/charts/MatrixHeatmap.h"

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

/// Average-linkage agglomerative clustering of a square matrix; returns the
/// leaf order. Adjacent clusters are joined end-to-end in whichever of the four
/// orientations puts the closest leaves next to each other.
QVector<int> mh_cluster_order(const QVector<QVector<double>>& m, int n) {
    const auto cell = [&m](int i, int j) {
        if (i < m.size() && j < m.at(i).size())
            return m.at(i).at(j);
        return std::numeric_limits<double>::quiet_NaN();
    };
    bool correlation = true;
    for (int i = 0; i < n && correlation; ++i) {
        for (int j = 0; j < n; ++j) {
            const double v = cell(i, j);
            if (std::isfinite(v) && (v < -1.0 - 1e-9 || v > 1.0 + 1e-9)) {
                correlation = false;
                break;
            }
        }
    }

    QVector<QVector<double>> d(n, QVector<double>(n, 0.0));
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            double dist = 0;
            if (correlation) {
                const double a = cell(i, j);
                const double b = cell(j, i);
                double v = 0;
                int k = 0;
                if (std::isfinite(a)) {
                    v += a;
                    ++k;
                }
                if (std::isfinite(b)) {
                    v += b;
                    ++k;
                }
                dist = 1.0 - (k > 0 ? v / k : 0.0);
            } else {
                double sum = 0;
                for (int c = 0; c < n; ++c) {
                    const double a = cell(i, c);
                    const double b = cell(j, c);
                    if (std::isfinite(a) && std::isfinite(b))
                        sum += (a - b) * (a - b);
                }
                dist = std::sqrt(sum);
            }
            d[i][j] = d[j][i] = dist;
        }
    }

    QVector<QVector<int>> leaves(n);
    QVector<double> size(n, 1.0);
    QVector<int> active(n);
    for (int i = 0; i < n; ++i) {
        leaves[i] = {i};
        active[i] = i;
    }
    QVector<QVector<double>> cd = d; // cluster distances, indexed by cluster id

    while (active.size() > 1) {
        int ba = -1;
        int bb = -1;
        double best = std::numeric_limits<double>::max();
        for (int x = 0; x < active.size(); ++x) {
            for (int y = x + 1; y < active.size(); ++y) {
                const double v = cd[active[x]][active[y]];
                if (v < best) {
                    best = v;
                    ba = x;
                    bb = y;
                }
            }
        }
        const int a = active[ba];
        const int b = active[bb];
        // Orientation that minimises the distance across the join.
        QVector<int> la = leaves[a];
        QVector<int> lb = leaves[b];
        const double o1 = d[la.last()][lb.first()];
        const double o2 = d[la.last()][lb.last()];
        const double o3 = d[la.first()][lb.first()];
        const double o4 = d[la.first()][lb.last()];
        const double mn = std::min({o1, o2, o3, o4});
        if (mn == o2) {
            std::reverse(lb.begin(), lb.end());
        } else if (mn == o3) {
            std::reverse(la.begin(), la.end());
        } else if (mn == o4) {
            std::reverse(la.begin(), la.end());
            std::reverse(lb.begin(), lb.end());
        }
        leaves[a] = la + lb;
        for (int c : active) {
            if (c == a || c == b)
                continue;
            const double v = (size[a] * cd[a][c] + size[b] * cd[b][c]) / (size[a] + size[b]);
            cd[a][c] = cd[c][a] = v;
        }
        size[a] += size[b];
        active.removeAt(bb);
    }
    return active.isEmpty() ? QVector<int>{} : leaves[active.first()];
}

QVector<int> mh_identity(int n) {
    QVector<int> v(qMax(0, n));
    for (int i = 0; i < v.size(); ++i)
        v[i] = i;
    return v;
}

} // namespace

MatrixHeatmap::MatrixHeatmap(QWidget* parent) : PaintedChart(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void MatrixHeatmap::set_matrix(const QStringList& row_labels, const QStringList& col_labels,
                               const QVector<QVector<double>>& values) {
    row_labels_ = row_labels;
    col_labels_ = col_labels;
    values_ = values;
    // Rows without a label still get a row; labels without data get an empty row.
    while (values_.size() < row_labels_.size())
        values_.append(QVector<double>());
    while (row_labels_.size() < values_.size())
        row_labels_.append(QString());
    hover_row_ = hover_col_ = -1;
    compute_order();
    resolve_scale();
    layout_valid_ = false;
    updateGeometry();
    invalidate();
}

void MatrixHeatmap::clear() {
    set_matrix({}, {}, {});
}

void MatrixHeatmap::set_diverging_scale(double min, double max, double center) {
    scale_ = Scale::Diverging;
    req_min_ = min;
    req_max_ = max;
    req_center_ = center;
    resolve_scale();
    layout_valid_ = false;
    invalidate();
}

void MatrixHeatmap::set_sequential_scale(double min, double max) {
    scale_ = Scale::Sequential;
    req_min_ = min;
    req_max_ = max;
    req_center_ = std::numeric_limits<double>::quiet_NaN();
    resolve_scale();
    layout_valid_ = false;
    invalidate();
}

void MatrixHeatmap::set_show_values(bool show) {
    show_values_ = show;
    invalidate();
}

void MatrixHeatmap::set_cluster(bool cluster) {
    if (cluster_ == cluster)
        return;
    cluster_ = cluster;
    compute_order();
    invalidate();
}

void MatrixHeatmap::set_show_legend(bool show) {
    show_legend_ = show;
    layout_valid_ = false;
    updateGeometry();
    invalidate();
}

void MatrixHeatmap::set_min_cell_size(int px) {
    min_cell_ = qMax(1, px);
    updateGeometry();
}

QString MatrixHeatmap::default_format(double v) const {
    if (!std::isfinite(v))
        return QStringLiteral("-");
    return QLocale().toString(v, 'f', std::abs(v) >= 100 ? 0 : 2);
}

double MatrixHeatmap::value_at(int row, int col) const {
    if (row < 0 || row >= values_.size() || col < 0 || col >= values_.at(row).size())
        return std::numeric_limits<double>::quiet_NaN();
    return values_.at(row).at(col);
}

void MatrixHeatmap::compute_order() {
    const int nr = static_cast<int>(values_.size());
    const int nc = static_cast<int>(col_labels_.size());
    if (cluster_ && nr == nc && nr > 2) {
        row_order_ = mh_cluster_order(values_, nr);
        col_order_ = row_order_;
    } else {
        row_order_ = mh_identity(nr);
        col_order_ = mh_identity(nc);
    }
}

void MatrixHeatmap::resolve_scale() {
    double dmin = std::numeric_limits<double>::max();
    double dmax = std::numeric_limits<double>::lowest();
    for (const auto& row : values_) {
        for (double v : row) {
            if (std::isfinite(v)) {
                dmin = std::min(dmin, v);
                dmax = std::max(dmax, v);
            }
        }
    }
    const bool any = dmin <= dmax;
    if (!any) {
        dmin = 0;
        dmax = 1;
    }
    if (scale_ == Scale::Sequential) {
        lo_ = std::isfinite(req_min_) ? req_min_ : dmin;
        hi_ = std::isfinite(req_max_) ? req_max_ : dmax;
        if (hi_ <= lo_)
            hi_ = lo_ + 1.0;
        center_ = (lo_ + hi_) / 2.0;
        return;
    }
    const double m = std::max(std::abs(dmin), std::abs(dmax));
    lo_ = std::isfinite(req_min_) ? req_min_ : -m;
    hi_ = std::isfinite(req_max_) ? req_max_ : m;
    if (hi_ <= lo_) {
        lo_ = -1;
        hi_ = 1;
    }
    if (std::isfinite(req_center_))
        center_ = qBound(lo_, req_center_, hi_);
    else
        center_ = (lo_ < 0 && hi_ > 0) ? 0.0 : (lo_ + hi_) / 2.0;
}

double MatrixHeatmap::t_for(double v) const {
    if (scale_ == Scale::Sequential)
        return qBound(0.0, (v - lo_) / (hi_ - lo_), 1.0);
    if (v >= center_)
        return hi_ > center_ ? qBound(0.0, (v - center_) / (hi_ - center_), 1.0) : 0.0;
    return center_ > lo_ ? -qBound(0.0, (center_ - v) / (center_ - lo_), 1.0) : 0.0;
}

QColor MatrixHeatmap::color_for(double v) const {
    if (!std::isfinite(v))
        return chart_style::empty_cell_color();
    return scale_ == Scale::Sequential ? colors::sequential_color(t_for(v)) : colors::diverging_color(t_for(v));
}

QSizeF MatrixHeatmap::label_extent() const {
    const QFontMetricsF fm(chart_style::font(-3));
    qreal rw = 0;
    for (const QString& s : row_labels_)
        rw = std::max(rw, fm.horizontalAdvance(s));
    qreal cw = 0;
    for (const QString& s : col_labels_)
        cw = std::max(cw, fm.horizontalAdvance(s));
    return {rw, cw};
}

QSize MatrixHeatmap::sizeHint() const {
    const QSizeF ext = label_extent();
    const QFontMetricsF fm(chart_style::font(-3));
    const int nr = static_cast<int>(values_.size());
    const int nc = static_cast<int>(col_labels_.size());
    const qreal lh = show_legend_ ? chart_style::legend_height() + 8 : 0;
    const qreal w = std::min<qreal>(ext.width() + 8, 180) + std::max(nc, 4) * 36.0;
    const qreal h = fm.height() + 6 + std::max(nr, 4) * 24.0 + lh;
    return {qCeil(w), qCeil(h)};
}

QSize MatrixHeatmap::minimumSizeHint() const {
    const QSizeF ext = label_extent();
    const QFontMetricsF fm(chart_style::font(-3));
    const int nr = static_cast<int>(values_.size());
    const int nc = static_cast<int>(col_labels_.size());
    const qreal lh = show_legend_ ? chart_style::legend_height() + 8 : 0;
    const qreal col_hdr = (nc * min_cell_ >= nc * (ext.height() + 4)) ? fm.height() + 6 : std::min<qreal>(ext.height(), 120) + 8;
    const qreal w = std::min<qreal>(ext.width() + 8, 180) + nc * min_cell_;
    const qreal h = col_hdr + nr * min_cell_ + lh;
    return {qCeil(std::max<qreal>(w, 80)), qCeil(std::max<qreal>(h, 60))};
}

void MatrixHeatmap::resizeEvent(QResizeEvent* e) {
    layout_valid_ = false;
    PaintedChart::resizeEvent(e);
}

void MatrixHeatmap::ensure_layout(const QRectF& r) {
    if (layout_valid_ && layout_for_ == r)
        return;
    layout_for_ = r;
    layout_valid_ = true;
    layout_ = Layout{};
    const int nr = static_cast<int>(values_.size());
    const int nc = static_cast<int>(col_labels_.size());
    if (nr == 0 || nc == 0)
        return;

    const QFontMetricsF fm(chart_style::font(-3));
    const QSizeF ext = label_extent();
    const qreal legend_h = (show_legend_ && r.height() > 100) ? chart_style::legend_height() + 8 : 0;
    const qreal row_hdr_w = std::min({ext.width() + 8, r.width() * 0.3, qreal(180)});
    const qreal aw = std::max<qreal>(1, r.width() - row_hdr_w);
    const qreal cw0 = aw / nc;
    const bool rotate = ext.height() + 6 > cw0;
    const qreal col_hdr_h = rotate ? std::min<qreal>(ext.height(), 120) + 8 : fm.height() + 6;
    const qreal ah = std::max<qreal>(1, r.height() - col_hdr_h - legend_h);
    qreal cw = aw / nc;
    qreal ch = ah / nr;
    if (nr == nc) {
        const qreal c = std::min(cw, ch);
        cw = ch = c;
    } else {
        ch = std::min(ch, std::max<qreal>(cw, fm.height() + 8) * 1.2);
    }

    layout_.cell_w = cw;
    layout_.cell_h = ch;
    layout_.rotate_cols = rotate;
    layout_.grid = QRectF(r.left() + row_hdr_w, r.top() + col_hdr_h, cw * nc, ch * nr);
    layout_.row_hdr = QRectF(r.left(), layout_.grid.top(), row_hdr_w - 6, layout_.grid.height());
    layout_.col_hdr = QRectF(layout_.grid.left(), r.top(), layout_.grid.width(), col_hdr_h - 4);
    if (legend_h > 0) {
        layout_.legend = QRectF(layout_.grid.left(), layout_.grid.bottom() + 8,
                                std::min<qreal>(std::max<qreal>(layout_.grid.width(), 120), 260),
                                chart_style::legend_height());
    }
    layout_.row_label_step = std::max(1, qCeil(fm.height() / std::max<qreal>(ch, 0.5)));
    layout_.col_label_step =
        std::max(1, qCeil((rotate ? fm.height() : ext.height() + 6) / std::max<qreal>(cw, 0.5)));
}

void MatrixHeatmap::paint_chart(QPainter& p, const QRectF& r) {
    ensure_layout(r);
    const Layout& L = layout_;
    const int nr = static_cast<int>(row_order_.size());
    const int nc = static_cast<int>(col_order_.size());
    if (nr == 0 || nc == 0)
        return;

    // In-cell text: the largest font at which every value fits its cell.
    QFont value_font;
    bool values_fit = false;
    if (show_values_ && L.cell_w >= 18 && L.cell_h >= 10) {
        QStringList texts;
        texts.reserve(nr * nc);
        for (int i = 0; i < nr; ++i) {
            for (int j = 0; j < nc; ++j) {
                const double v = value_at(row_order_[i], col_order_[j]);
                if (std::isfinite(v))
                    texts.append(format_value(v));
            }
        }
        texts.removeDuplicates();
        for (int offset = -3; offset >= -6 && !values_fit; --offset) {
            const QFont f = chart_style::font(offset);
            const QFontMetricsF fm(f);
            qreal widest = 0;
            for (const QString& t : texts)
                widest = std::max(widest, fm.horizontalAdvance(t));
            if (widest + 4 <= L.cell_w && fm.height() + 1 <= L.cell_h) {
                value_font = f;
                values_fit = true;
            }
        }
    }

    const qreal gap = L.cell_w >= 6 && L.cell_h >= 6 ? 0.5 : 0.0;
    p.setPen(Qt::NoPen);
    for (int i = 0; i < nr; ++i) {
        for (int j = 0; j < nc; ++j) {
            const double v = value_at(row_order_[i], col_order_[j]);
            const QRectF cell(L.grid.left() + j * L.cell_w, L.grid.top() + i * L.cell_h, L.cell_w, L.cell_h);
            const QColor fill = color_for(v);
            p.fillRect(cell.adjusted(gap, gap, -gap, -gap), fill);
            if (values_fit && std::isfinite(v)) {
                p.setFont(value_font);
                p.setPen(colors::contrast_text_on(fill));
                p.drawText(cell, Qt::AlignCenter, format_value(v));
                p.setPen(Qt::NoPen);
            }
        }
    }

    // Row labels (right-aligned), thinned when rows are shorter than the text.
    const QFont lf = chart_style::font(-3);
    const QFontMetricsF lfm(lf);
    p.setFont(lf);
    p.setPen(chart_style::axis_text_color());
    for (int i = 0; i < nr; i += L.row_label_step) {
        const QString text = lfm.elidedText(row_labels_.value(row_order_[i]), Qt::ElideRight, L.row_hdr.width());
        const qreal cy = L.grid.top() + (i + 0.5) * L.cell_h;
        p.drawText(QRectF(L.row_hdr.left(), cy - lfm.height() / 2, L.row_hdr.width(), lfm.height()),
                   Qt::AlignRight | Qt::AlignVCenter, text);
    }
    // Column labels: horizontal when they fit, else rotated to read bottom-up.
    for (int j = 0; j < nc; j += L.col_label_step) {
        const QString label = col_labels_.value(col_order_[j]);
        const qreal cx = L.grid.left() + (j + 0.5) * L.cell_w;
        if (L.rotate_cols) {
            const QString text = lfm.elidedText(label, Qt::ElideRight, L.col_hdr.height());
            p.save();
            p.translate(cx, L.col_hdr.bottom());
            p.rotate(-90);
            p.drawText(QRectF(0, -lfm.height() / 2, L.col_hdr.height(), lfm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       text);
            p.restore();
        } else {
            const QString text = lfm.elidedText(label, Qt::ElideRight, L.cell_w * L.col_label_step);
            p.drawText(QRectF(cx - L.cell_w * L.col_label_step / 2.0, L.col_hdr.top(), L.cell_w * L.col_label_step,
                              L.col_hdr.height()),
                       Qt::AlignHCenter | Qt::AlignBottom, text);
        }
    }

    if (!L.legend.isEmpty()) {
        chart_style::draw_scale_legend(p, L.legend, scale_ == Scale::Diverging, lo_, hi_,
                                       [this](double v) { return format_value(v); });
    }
}

void MatrixHeatmap::paint_overlay(QPainter& p, const QRectF&) {
    if (hover_row_ < 0 || hover_col_ < 0 || !layout_valid_)
        return;
    const Layout& L = layout_;
    const QRectF row_band(L.grid.left(), L.grid.top() + hover_row_ * L.cell_h, L.grid.width(), L.cell_h);
    const QRectF col_band(L.grid.left() + hover_col_ * L.cell_w, L.grid.top(), L.cell_w, L.grid.height());
    p.setPen(Qt::NoPen);
    p.setBrush(colors::overlay_color(0.10));
    p.drawRect(row_band);
    p.drawRect(col_band.adjusted(0, 0, 0, 0));
    // Accent ticks beside the hovered row's and column's labels.
    const QColor accent(colors::AMBER());
    p.setBrush(accent);
    p.drawRect(QRectF(L.row_hdr.right() + 2, row_band.top(), 2, row_band.height()));
    p.drawRect(QRectF(col_band.left(), L.col_hdr.bottom() + 1, col_band.width(), 2));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(chart_style::highlight_color(), 1.5));
    p.drawRect(QRectF(col_band.left(), row_band.top(), L.cell_w, L.cell_h));
}

bool MatrixHeatmap::cell_at(const QPointF& pos, int* disp_row, int* disp_col) const {
    const Layout& L = layout_;
    if (!L.grid.contains(pos) || L.cell_w <= 0 || L.cell_h <= 0)
        return false;
    const int c = static_cast<int>((pos.x() - L.grid.left()) / L.cell_w);
    const int r = static_cast<int>((pos.y() - L.grid.top()) / L.cell_h);
    if (r < 0 || r >= row_order_.size() || c < 0 || c >= col_order_.size())
        return false;
    *disp_row = r;
    *disp_col = c;
    return true;
}

void MatrixHeatmap::mouseMoveEvent(QMouseEvent* e) {
    if (!chart_visible())
        return;
    ensure_layout(content_rect());
    int r = -1;
    int c = -1;
    const QPointF pos = e->position();
    if (!cell_at(pos, &r, &c)) {
        if (hover_row_ >= 0) {
            hover_row_ = hover_col_ = -1;
            emit cell_hovered(-1, -1);
            update();
        }
        hide_tip();
        return;
    }
    if (r != hover_row_ || c != hover_col_) {
        hover_row_ = r;
        hover_col_ = c;
        emit cell_hovered(row_order_[r], col_order_[c]);
        update();
    }
    const int orow = row_order_[r];
    const int ocol = col_order_[c];
    const double v = value_at(orow, ocol);
    const QString title = row_labels_.value(orow) + QStringLiteral(" ") + QChar(0x00D7) + QStringLiteral(" ") +
                          col_labels_.value(ocol);
    const QRectF cell(layout_.grid.left() + c * layout_.cell_w, layout_.grid.top() + r * layout_.cell_h,
                      layout_.cell_w, layout_.cell_h);
    show_tip(pos.toPoint(),
             chart_style::tooltip_html(title, {{tr("Value"), std::isfinite(v) ? format_value(v) : tr("n/a")}}),
             cell.toAlignedRect());
}

void MatrixHeatmap::mousePressEvent(QMouseEvent* e) {
    press_row_ = press_col_ = -1;
    if (e->button() == Qt::LeftButton && chart_visible()) {
        ensure_layout(content_rect());
        cell_at(e->position(), &press_row_, &press_col_);
    }
    PaintedChart::mousePressEvent(e);
}

void MatrixHeatmap::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && chart_visible()) {
        int r = -1;
        int c = -1;
        if (cell_at(e->position(), &r, &c) && r == press_row_ && c == press_col_)
            emit cell_clicked(row_order_[r], col_order_[c]);
    }
    press_row_ = press_col_ = -1;
    PaintedChart::mouseReleaseEvent(e);
}

void MatrixHeatmap::on_hover_left() {
    if (hover_row_ >= 0)
        emit cell_hovered(-1, -1);
    hover_row_ = hover_col_ = -1;
}

} // namespace fincept::ui
