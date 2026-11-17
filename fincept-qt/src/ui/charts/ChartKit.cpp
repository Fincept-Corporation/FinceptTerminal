#include "ui/charts/ChartKit.h"

#include "ui/charts/ChartAxisTicks.h"
#include "ui/theme/Theme.h"

#include <QFontMetrics>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QWidget>
#include <QtMath>

#include <algorithm>
#include <climits>
#include <cmath>
#include <limits>

namespace fincept::ui::chartkit {

namespace {

} // namespace

// ── Colours ──────────────────────────────────────────────────────────────────

QColor role_color(ChartColorRole role, int palette_index, const QColor& custom) {
    switch (role) {
        case ChartColorRole::Auto:
        case ChartColorRole::Palette:
            return colors::chart_color(palette_index);
        case ChartColorRole::Accent:
            return accent();
        case ChartColorRole::Positive:
            return up();
        case ChartColorRole::Negative:
            return down();
        case ChartColorRole::Warning:
            return warning();
        case ChartColorRole::Info:
            return QColor(colors::INFO());
        case ChartColorRole::Cyan:
            return QColor(colors::CYAN());
        case ChartColorRole::Muted:
            return QColor(colors::TEXT_TERTIARY());
        case ChartColorRole::Text:
            return text();
        case ChartColorRole::Custom:
            return custom.isValid() ? colors::legible_color(custom) : colors::chart_color(palette_index);
    }
    return colors::chart_color(palette_index);
}

QColor background() {
    return QColor(colors::BG_BASE());
}
QColor surface() {
    return QColor(colors::BG_SURFACE());
}
QColor raised() {
    return QColor(colors::BG_RAISED());
}
QColor grid() {
    return QColor(colors::BORDER_DIM());
}
QColor axis_line() {
    return QColor(colors::BORDER_MED());
}
QColor axis_text() {
    return QColor(colors::TEXT_SECONDARY());
}
QColor text() {
    return QColor(colors::TEXT_PRIMARY());
}
QColor text_dim() {
    return QColor(colors::TEXT_TERTIARY());
}
QColor crosshair() {
    return QColor(colors::TEXT_TERTIARY());
}
QColor up() {
    return QColor(colors::POSITIVE());
}
QColor down() {
    return QColor(colors::NEGATIVE());
}
QColor accent() {
    return QColor(colors::AMBER());
}
QColor warning() {
    return QColor(colors::WARNING());
}

QColor with_alpha(QColor c, double alpha) {
    c.setAlphaF(static_cast<float>(std::clamp(alpha, 0.0, 1.0)));
    return c;
}

QColor text_on(const QColor& bg) {
    return colors::contrast_text_on(bg);
}

// ── Fonts ────────────────────────────────────────────────────────────────────

QFont axis_font() {
    return fonts::font(-3);
}

QFont tooltip_font(bool bold) {
    return fonts::font(-2, bold ? QFont::DemiBold : QFont::Normal);
}

QFont title_font() {
    return fonts::font(-2, QFont::DemiBold);
}

// ── Number formatting ────────────────────────────────────────────────────────

QString format_number(double v, int decimals) {
    if (!std::isfinite(v))
        return QStringLiteral("—");
    decimals = std::clamp(decimals, 0, 12);
    if (std::abs(v) < 0.5 * std::pow(10.0, -decimals))
        v = 0.0; // never print "-0.00"
    return QLocale().toString(v, 'f', decimals);
}

int auto_decimals(double v) {
    const double a = std::abs(v);
    if (!std::isfinite(a) || a == 0.0)
        return 2;
    if (a >= 1e5)
        return 0;
    if (a >= 1.0)
        return 2;
    if (a >= 0.01)
        return 4;
    return 6;
}

QString format_compact(double v, int decimals) {
    if (!std::isfinite(v))
        return QStringLiteral("—");
    const double a = std::abs(v);
    if (a >= 1e12)
        return format_number(v / 1e12, decimals) + ChartKitText::tr("T", "trillions suffix");
    if (a >= 1e9)
        return format_number(v / 1e9, decimals) + ChartKitText::tr("B", "billions suffix");
    if (a >= 1e6)
        return format_number(v / 1e6, decimals) + ChartKitText::tr("M", "millions suffix");
    if (a >= 1e3)
        return format_number(v / 1e3, decimals) + ChartKitText::tr("K", "thousands suffix");
    return format_number(v, decimals);
}

QString format_auto(double v) {
    if (std::isfinite(v) && std::abs(v) >= 1e7)
        return format_compact(v, 2);
    return format_number(v, auto_decimals(v));
}

QString format_signed_percent(double pct, int decimals) {
    if (!std::isfinite(pct))
        return QStringLiteral("—");
    const QLocale loc;
    QString s = format_number(pct, decimals);
    if (pct > 0.0 && std::abs(pct) >= 0.5 * std::pow(10.0, -decimals))
        s.prepend(loc.positiveSign());
    return s + loc.percent();
}

QString format_tick(double v, double step, double max_abs) {
    if (!std::isfinite(v))
        return QStringLiteral("—");
    if (max_abs >= 1e6) {
        double scale = 1e6;
        QString suffix = ChartKitText::tr("M", "millions suffix");
        if (max_abs >= 1e12) {
            scale = 1e12;
            suffix = ChartKitText::tr("T", "trillions suffix");
        } else if (max_abs >= 1e9) {
            scale = 1e9;
            suffix = ChartKitText::tr("B", "billions suffix");
        }
        const int d = step > 0.0 ? chart_ticks::decimals_for_step(step / scale)
                                 : (v != 0.0 ? chart_ticks::decimals_for_step(std::abs(v) / scale) : 0);
        return format_number(v / scale, d) + suffix;
    }
    const int d =
        step > 0.0 ? chart_ticks::decimals_for_step(step) : (v != 0.0 ? chart_ticks::decimals_for_step(std::abs(v)) : 0);
    return format_number(v, d);
}

// ── Decimation ───────────────────────────────────────────────────────────────

void decimate_minmax(const double* xs, const double* ys, int i0, int i1, double x0, double px_per_unit,
                     QVector<int>& out) {
    if (i1 <= i0 || !xs || !ys)
        return;
    long long cur_col = LLONG_MIN;
    int first = -1, last = -1, mn = -1, mx = -1;
    auto flush = [&]() {
        if (first < 0)
            return;
        int buf[4] = {first, mn, mx, last};
        std::sort(buf, buf + 4);
        int prev = -1;
        for (int k : buf) {
            if (k != prev)
                out.append(k);
            prev = k;
        }
        first = -1;
    };
    bool in_gap = false;
    for (int i = i0; i < i1; ++i) {
        const double y = ys[i];
        if (!std::isfinite(y)) {
            flush();
            if (!in_gap)
                out.append(i);
            in_gap = true;
            continue;
        }
        in_gap = false;
        const double cf = std::clamp(std::floor((xs[i] - x0) * px_per_unit), -1e15, 1e15);
        const long long col = static_cast<long long>(cf);
        if (first < 0 || col != cur_col) {
            flush();
            cur_col = col;
            first = last = mn = mx = i;
        } else {
            last = i;
            if (y < ys[mn])
                mn = i;
            if (y > ys[mx])
                mx = i;
        }
    }
    flush();
}

// ── Fast stroking / filling ──────────────────────────────────────────────────

void stroke_polyline(QPainter& p, const QPolygonF& poly) {
    const int n = static_cast<int>(poly.size());
    if (n == 0)
        return;
    if (n == 1) {
        p.drawPoint(poly.first());
        return;
    }
    // Dashed pens restart their pattern at every call, so they get long chunks.
    const int chunk = p.pen().style() == Qt::SolidLine ? 48 : 512;
    if (n <= chunk) {
        p.drawPolyline(poly);
        return;
    }
    for (int start = 0; start < n - 1; start += chunk - 1)
        p.drawPolyline(poly.constData() + start, std::min(chunk, n - start));
}

QColor opaque_over_background(const QColor& c) {
    if (c.alpha() == 255)
        return c;
    const QColor bg = background();
    const double a = c.alphaF();
    return QColor::fromRgbF(static_cast<float>(c.redF() * a + bg.redF() * (1.0 - a)),
                            static_cast<float>(c.greenF() * a + bg.greenF() * (1.0 - a)),
                            static_cast<float>(c.blueF() * a + bg.blueF() * (1.0 - a)));
}

void fill_rects(QPainter& p, const QVector<QRectF>& rects, const QColor& color) {
    if (rects.isEmpty())
        return;
    const QColor solid = opaque_over_background(color);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    for (const QRectF& r : rects)
        p.fillRect(r, solid);
    p.restore();
}

bool is_dense(const QPolygonF& line) {
    if (line.size() < 8)
        return false;
    const double span = std::abs(line.last().x() - line.first().x()) + 1.0;
    return line.size() > span * 1.5;
}

namespace {

/// Dense-line renderer: one non-antialiased rect per pixel column covering
/// the column's min..max y, extended to the previous column's last y so the
/// columns join. Columns further apart are joined with a stroked segment.
/// With a finite `base_y` the part of each rect above it uses `above`, the
/// part below `below`.
void ck_dense_columns(QPainter& p, const QPolygonF& line, const QPen& pen, double base_y, const QColor& above,
                      const QColor& below) {
    const qreal w = std::max<qreal>(1.0, pen.widthF());
    const qreal half = w / 2.0;
    const bool split = std::isfinite(base_y);

    QVector<QRectF> rects_above;
    QVector<QRectF> rects_below;
    rects_above.reserve(line.size() / 2 + 1);
    auto fill_span = [&](double x, double top, double bot) {
        const QRectF r(x + 0.5 - half, top - half, w, (bot - top) + w);
        if (!split) {
            rects_above.append(r);
            return;
        }
        if (r.top() < base_y)
            rects_above.append(QRectF(r.left(), r.top(), r.width(), std::min(r.bottom(), base_y) - r.top()));
        if (r.bottom() > base_y)
            rects_below.append(
                QRectF(r.left(), std::max(r.top(), base_y), r.width(), r.bottom() - std::max(r.top(), base_y)));
    };

    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    long long col = LLONG_MIN;
    double lo = 0.0, hi = 0.0, last_y = 0.0;
    QPointF last_pt;
    bool have = false;
    for (const QPointF& q : line) {
        const long long c = static_cast<long long>(std::floor(q.x()));
        if (!have || c != col) {
            if (have)
                fill_span(static_cast<double>(col), lo, hi);
            lo = hi = q.y();
            if (have && c == col + 1) {
                lo = std::min(lo, last_y); // join to the neighbouring column
                hi = std::max(hi, last_y);
            } else if (have) {
                QPen seg_pen = pen;
                seg_pen.setColor(split && q.y() > base_y ? below : above);
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setPen(seg_pen);
                p.drawLine(last_pt, q);
                p.setRenderHint(QPainter::Antialiasing, false);
            }
            col = c;
            have = true;
        } else {
            lo = std::min(lo, q.y());
            hi = std::max(hi, q.y());
        }
        last_y = q.y();
        last_pt = q;
    }
    if (have)
        fill_span(static_cast<double>(col), lo, hi);
    fill_rects(p, rects_above, above);
    fill_rects(p, rects_below, below);
    p.restore();
}

} // namespace

void draw_series_line(QPainter& p, const QPolygonF& line, const QPen& pen) {
    if (line.isEmpty())
        return;
    if (pen.style() == Qt::SolidLine && is_dense(line)) {
        ck_dense_columns(p, line, pen, std::numeric_limits<double>::quiet_NaN(), pen.color(), pen.color());
        return;
    }
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    stroke_polyline(p, line);
}

void draw_series_line_split(QPainter& p, const QPolygonF& line, const QPen& pen, double base_y, const QColor& above,
                            const QColor& below, const QRectF& bounds) {
    if (line.isEmpty())
        return;
    if (pen.style() == Qt::SolidLine && is_dense(line)) {
        ck_dense_columns(p, line, pen, base_y, above, below);
        return;
    }
    const QRectF top(bounds.left() - 1, bounds.top() - 1, bounds.width() + 2, base_y - bounds.top() + 1);
    const QRectF bottom(bounds.left() - 1, base_y, bounds.width() + 2, bounds.bottom() - base_y + 2);
    QPen pa = pen;
    pa.setColor(above);
    QPen pb = pen;
    pb.setColor(below);
    p.save();
    p.setClipRect(top, Qt::IntersectClip);
    p.setPen(pa);
    p.setBrush(Qt::NoBrush);
    stroke_polyline(p, line);
    p.restore();
    p.save();
    p.setClipRect(bottom, Qt::IntersectClip);
    p.setPen(pb);
    p.setBrush(Qt::NoBrush);
    stroke_polyline(p, line);
    p.restore();
}

QPolygonF fill_envelope(const QPolygonF& line, double base_y, EnvelopeMode mode) {
    QPolygonF out;
    if (line.size() < 2)
        return out;

    // Exact crossing points, so a sparse line's fill meets the base where the
    // line does (needed for the clamped Above / Below modes).
    QPolygonF pts;
    pts.reserve(line.size() + line.size() / 8 + 2);
    for (int i = 0; i < line.size(); ++i) {
        if (i > 0) {
            const QPointF a = line[i - 1];
            const QPointF b = line[i];
            const double da = a.y() - base_y;
            const double db = b.y() - base_y;
            if ((da < 0.0 && db > 0.0) || (da > 0.0 && db < 0.0)) {
                const double t = da / (da - db);
                pts << QPointF(a.x() + t * (b.x() - a.x()), base_y);
            }
        }
        pts << line[i];
    }

    auto better = [mode, base_y](const QPointF& cand, const QPointF& cur) {
        switch (mode) {
            case EnvelopeMode::Farthest:
                return std::abs(cand.y() - base_y) > std::abs(cur.y() - base_y);
            case EnvelopeMode::Above:
                return cand.y() < cur.y();
            case EnvelopeMode::Below:
                return cand.y() > cur.y();
        }
        return false;
    };
    auto clamp_y = [mode, base_y](QPointF q) {
        if (mode == EnvelopeMode::Above)
            q.setY(std::min(q.y(), base_y));
        else if (mode == EnvelopeMode::Below)
            q.setY(std::max(q.y(), base_y));
        return q;
    };

    out.reserve(pts.size() + 2);
    long long col = LLONG_MIN;
    QPointF best;
    bool have = false;
    for (const QPointF& q : pts) {
        const long long c = static_cast<long long>(std::floor(q.x()));
        if (!have || c != col) {
            if (have)
                out << clamp_y(best);
            col = c;
            best = q;
            have = true;
        } else if (better(q, best)) {
            best = q;
        }
    }
    if (have)
        out << clamp_y(best);
    // Span exactly the line's extent.
    out.first().setX(pts.first().x());
    out.last().setX(pts.last().x());
    out << QPointF(out.last().x(), base_y) << QPointF(out.first().x(), base_y);
    return out;
}

// ── Layers ───────────────────────────────────────────────────────────────────

bool layer_valid(const QPixmap& layer, const QWidget* w) {
    if (layer.isNull() || !w)
        return false;
    const qreal dpr = w->devicePixelRatioF();
    if (!qFuzzyCompare(layer.devicePixelRatio(), dpr))
        return false;
    return layer.width() == qCeil(w->width() * dpr) && layer.height() == qCeil(w->height() * dpr);
}

QPixmap make_layer(const QWidget* w, const QColor& fill) {
    const qreal dpr = w ? w->devicePixelRatioF() : 1.0;
    const int pw = w ? qCeil(w->width() * dpr) : 0;
    const int ph = w ? qCeil(w->height() * dpr) : 0;
    if (pw <= 0 || ph <= 0)
        return {};
    QPixmap pm(pw, ph);
    pm.setDevicePixelRatio(dpr);
    pm.fill(fill);
    return pm;
}

// ── Painting primitives ──────────────────────────────────────────────────────

namespace {

struct CkTipMetrics {
    int pad = 0;
    int line_h = 0;
    int swatch = 0;
    int gap = 0;
};

CkTipMetrics ck_tip_metrics(const QFontMetrics& fm) {
    CkTipMetrics m;
    m.pad = fm.height() / 3 + 2;
    m.line_h = fm.height() + 2;
    m.swatch = std::max(4, fm.ascent() * 2 / 3);
    m.gap = std::max(4, fm.averageCharWidth());
    return m;
}

} // namespace

QSize tooltip_size(const QString& title, const QVector<TooltipRow>& rows, const QString& footer) {
    const QFont f = tooltip_font();
    const QFont fb = tooltip_font(true);
    const QFontMetrics fm(f);
    const QFontMetrics fmb(fb);
    const CkTipMetrics m = ck_tip_metrics(fm);

    int w = title.isEmpty() ? 0 : fmb.horizontalAdvance(title);
    for (const auto& r : rows) {
        int rw = 0;
        if (r.swatch.isValid())
            rw += m.swatch + m.gap;
        rw += fm.horizontalAdvance(r.name);
        if (!r.value.isEmpty())
            rw += (r.name.isEmpty() ? 0 : m.gap * 2) + fmb.horizontalAdvance(r.value);
        w = std::max(w, rw);
    }
    if (!footer.isEmpty())
        w = std::max(w, fm.horizontalAdvance(footer));
    int h = 0;
    if (!title.isEmpty())
        h += m.line_h;
    h += static_cast<int>(rows.size()) * m.line_h;
    if (!footer.isEmpty())
        h += m.line_h;
    return {w + 2 * m.pad, h + 2 * m.pad - 2};
}

QRect draw_tooltip(QPainter& p, const QRect& bounds, const QPoint& anchor, const QString& title,
                   const QVector<TooltipRow>& rows, const QString& footer) {
    const QSize sz = tooltip_size(title, rows, footer);
    const QFont f = tooltip_font();
    const QFont fb = tooltip_font(true);
    const QFontMetrics fm(f);
    const CkTipMetrics m = ck_tip_metrics(fm);
    const int off = fm.height();

    int x = anchor.x() + off;
    int y = anchor.y() + off;
    if (x + sz.width() > bounds.right())
        x = anchor.x() - off - sz.width();
    if (y + sz.height() > bounds.bottom())
        y = anchor.y() - off - sz.height();
    x = std::clamp(x, bounds.left(), std::max(bounds.left(), bounds.right() - sz.width()));
    y = std::clamp(y, bounds.top(), std::max(bounds.top(), bounds.bottom() - sz.height()));
    const QRect box(QPoint(x, y), sz);

    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(box, raised());
    p.setPen(QPen(axis_line(), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(box.adjusted(0, 0, -1, -1));

    int cy = box.top() + m.pad;
    const int left = box.left() + m.pad;
    const int right = box.right() - m.pad;
    if (!title.isEmpty()) {
        p.setFont(fb);
        p.setPen(text());
        p.drawText(QRect(left, cy, right - left + 1, m.line_h), Qt::AlignLeft | Qt::AlignVCenter, title);
        cy += m.line_h;
    }
    for (const auto& r : rows) {
        int tx = left;
        if (r.swatch.isValid()) {
            const int sy = cy + (m.line_h - m.swatch) / 2;
            p.fillRect(QRect(tx, sy, m.swatch, m.swatch), r.swatch);
            tx += m.swatch + m.gap;
        }
        p.setFont(f);
        p.setPen(axis_text());
        p.drawText(QRect(tx, cy, right - tx + 1, m.line_h), Qt::AlignLeft | Qt::AlignVCenter, r.name);
        if (!r.value.isEmpty()) {
            p.setFont(fb);
            p.setPen(r.value_color.isValid() ? r.value_color : text());
            p.drawText(QRect(tx, cy, right - tx + 1, m.line_h), Qt::AlignRight | Qt::AlignVCenter, r.value);
        }
        cy += m.line_h;
    }
    if (!footer.isEmpty()) {
        p.setFont(f);
        p.setPen(text_dim());
        p.drawText(QRect(left, cy, right - left + 1, m.line_h), Qt::AlignLeft | Qt::AlignVCenter, footer);
    }
    p.restore();
    return box;
}

void draw_axis_tag(QPainter& p, const QRectF& r, const QString& text_str, const QColor& bg) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(r, bg);
    p.setFont(tooltip_font(true));
    p.setPen(text_on(bg));
    p.drawText(r, Qt::AlignCenter, text_str);
    p.restore();
}

void draw_state_message(QPainter& p, const QRect& r, ChartState state, const QString& message) {
    if (state == ChartState::Ready || r.width() < 8 || r.height() < 8)
        return;
    QString headline;
    QColor color = text_dim();
    switch (state) {
        case ChartState::Loading:
            headline = message.isEmpty() ? ChartKitText::tr("Loading…") : message;
            break;
        case ChartState::Empty:
            headline = message.isEmpty() ? ChartKitText::tr("No data") : message;
            break;
        case ChartState::Error:
            headline = ChartKitText::tr("Could not load data");
            color = QColor(colors::DANGER());
            break;
        case ChartState::Ready:
            return;
    }
    const QString detail = state == ChartState::Error ? message : QString();

    const QFont fb = tooltip_font(true);
    const QFont f = tooltip_font();
    const QFontMetrics fmb(fb);
    const QFontMetrics fm(f);
    const int max_w = std::max(40, r.width() - 2 * fm.height());
    const QRect head_br = fmb.boundingRect(QRect(0, 0, max_w, 10000), Qt::AlignCenter | Qt::TextWordWrap, headline);
    const QRect det_br = detail.isEmpty()
                             ? QRect()
                             : fm.boundingRect(QRect(0, 0, max_w, 10000), Qt::AlignCenter | Qt::TextWordWrap, detail);
    const int total_h = head_br.height() + (detail.isEmpty() ? 0 : det_br.height() + fm.height() / 3);
    int y = r.top() + (r.height() - total_h) / 2;

    p.save();
    p.setFont(fb);
    p.setPen(color);
    p.drawText(QRect(r.left() + (r.width() - max_w) / 2, y, max_w, head_br.height()), Qt::AlignCenter | Qt::TextWordWrap,
               headline);
    if (!detail.isEmpty()) {
        y += head_br.height() + fm.height() / 3;
        p.setFont(f);
        p.setPen(axis_text());
        p.drawText(QRect(r.left() + (r.width() - max_w) / 2, y, max_w, det_br.height()),
                   Qt::AlignCenter | Qt::TextWordWrap, detail);
    }
    p.restore();
}

QColor marker_color(ChartMarkerKind kind) {
    switch (kind) {
        case ChartMarkerKind::Buy:
            return up();
        case ChartMarkerKind::Sell:
            return down();
        case ChartMarkerKind::Earnings:
            return accent();
        case ChartMarkerKind::Dividend:
            return QColor(colors::CYAN());
        case ChartMarkerKind::Split:
            return warning();
        case ChartMarkerKind::News:
            return QColor(colors::INFO());
        case ChartMarkerKind::Custom:
            return accent();
    }
    return accent();
}

QString marker_letter(ChartMarkerKind kind) {
    switch (kind) {
        case ChartMarkerKind::Earnings:
            return ChartKitText::tr("E", "earnings marker");
        case ChartMarkerKind::Dividend:
            return ChartKitText::tr("D", "dividend marker");
        case ChartMarkerKind::Split:
            return ChartKitText::tr("S", "split marker");
        case ChartMarkerKind::News:
            return ChartKitText::tr("N", "news marker");
        case ChartMarkerKind::Buy:
        case ChartMarkerKind::Sell:
        case ChartMarkerKind::Custom:
            break;
    }
    return {};
}

QRectF draw_marker(QPainter& p, const QPointF& at, ChartMarkerKind kind, const QColor& color, qreal size) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor outline = background();
    QRectF hit;
    const qreal half = size * 0.5;
    const qreal gap = std::max<qreal>(2.0, size * 0.2);
    switch (kind) {
        case ChartMarkerKind::Buy: {
            const qreal top = at.y() + gap;
            QPolygonF tri;
            tri << QPointF(at.x(), top) << QPointF(at.x() + half, top + size * 0.85)
                << QPointF(at.x() - half, top + size * 0.85);
            p.setPen(QPen(outline, 1));
            p.setBrush(color);
            p.drawPolygon(tri);
            hit = tri.boundingRect();
            break;
        }
        case ChartMarkerKind::Sell: {
            const qreal bottom = at.y() - gap;
            QPolygonF tri;
            tri << QPointF(at.x(), bottom) << QPointF(at.x() + half, bottom - size * 0.85)
                << QPointF(at.x() - half, bottom - size * 0.85);
            p.setPen(QPen(outline, 1));
            p.setBrush(color);
            p.drawPolygon(tri);
            hit = tri.boundingRect();
            break;
        }
        case ChartMarkerKind::Custom: {
            QPolygonF dia;
            dia << QPointF(at.x(), at.y() - half) << QPointF(at.x() + half, at.y()) << QPointF(at.x(), at.y() + half)
                << QPointF(at.x() - half, at.y());
            p.setPen(QPen(outline, 1));
            p.setBrush(color);
            p.drawPolygon(dia);
            hit = dia.boundingRect();
            break;
        }
        case ChartMarkerKind::Earnings:
        case ChartMarkerKind::Dividend:
        case ChartMarkerKind::Split:
        case ChartMarkerKind::News: {
            const QRectF circle(at.x() - half, at.y() - half, size, size);
            p.setPen(QPen(outline, 1));
            p.setBrush(color);
            p.drawEllipse(circle);
            QFont f = axis_font();
            f.setWeight(QFont::Bold);
            // The letter scales with the badge, which itself follows the font size.
            f.setPixelSize(std::max(ThemeManager::kFloorFontPx, static_cast<int>(size * 0.62)));
            p.setFont(f);
            p.setPen(text_on(color));
            p.drawText(circle, Qt::AlignCenter, marker_letter(kind));
            hit = circle;
            break;
        }
    }
    p.restore();
    return hit.adjusted(-2, -2, 2, 2);
}

} // namespace fincept::ui::chartkit
