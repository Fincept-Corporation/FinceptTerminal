// TimeSeriesChart — layout and painting.
// Data, view state and interaction live in TimeSeriesChart.cpp; shared state
// in TsChartPrivate.h.
//
// Two layers: rebuild_cache() renders everything static (header, grid, bands,
// series, annotations, axes) into a pixmap at the device pixel ratio;
// paint_overlay() draws the crosshair, tags, dots and tooltip over it on every
// mouse move. Colours and fonts are read from the theme on every rebuild.
#include "ui/charts/ChartAxisTicks.h"
#include "ui/charts/ChartKit.h"
#include "ui/charts/TsChartPrivate.h"
#include "ui/theme/Theme.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QLocale>
#include <QPainter>
#include <QPolygonF>

#include <climits>

namespace fincept::ui {

namespace {

QString tsp_mode_text(ts_detail::HitKind k) {
    switch (k) {
        case ts_detail::HitKind::ModeLog:
            return TimeSeriesChart::tr("LOG", "log scale toggle");
        case ts_detail::HitKind::ModePercent:
            return TimeSeriesChart::tr("%", "percent change toggle");
        case ts_detail::HitKind::ModeRebase:
            return TimeSeriesChart::tr("=100", "rebase to 100 toggle");
        default:
            break;
    }
    return {};
}

bool tsp_is_all_preset(const QString& code) {
    const QString c = code.trimmed().toUpper();
    return c == QLatin1String("ALL") || c == QLatin1String("MAX");
}

/// Turn a polyline into horizontal-then-vertical steps.
QPolygonF tsp_steps(const QPolygonF& seg) {
    QPolygonF out;
    out.reserve(seg.size() * 2);
    for (int k = 0; k < seg.size(); ++k) {
        if (k > 0)
            out << QPointF(seg[k].x(), seg[k - 1].y());
        out << seg[k];
    }
    return out;
}

} // namespace

// ── Formatting ───────────────────────────────────────────────────────────────

QString TimeSeriesChart::format_value(int pane, int axis, double v) const {
    if (!std::isfinite(v))
        return QStringLiteral("—");
    if (pane == 0 && axis == 0 && d->pane0_rebased()) {
        if (d->value_mode == ValueMode::Percent)
            return chartkit::format_signed_percent(v, 2);
        return chartkit::format_number(v, 2);
    }
    if (pane >= 0 && pane < d->panes.size() && d->panes[pane].fmt[axis])
        return d->panes[pane].fmt[axis](v);
    return chartkit::format_auto(v);
}

QString TimeSeriesChart::format_tick_value(int pane, int axis, double v) const {
    const auto& L = d->layout;
    const ts_detail::AxisScale* sc = (pane >= 0 && pane < L.panes.size()) ? &L.panes[pane].axis[axis] : nullptr;
    if (pane == 0 && axis == 0 && d->pane0_rebased()) {
        const int dec = sc ? sc->ticks.decimals : 2;
        if (d->value_mode == ValueMode::Percent)
            return chartkit::format_number(v, dec) + QLocale().percent();
        return chartkit::format_number(v, dec);
    }
    if (pane >= 0 && pane < d->panes.size() && d->panes[pane].fmt[axis])
        return d->panes[pane].fmt[axis](v);
    return chartkit::format_tick(v, sc ? sc->ticks.step : 0.0, sc ? sc->max_abs : std::abs(v));
}

QString TimeSeriesChart::format_time(double x) const {
    if (d->time_fmt)
        return d->time_fmt(x);
    if (d->x_is_time)
        return chart_ticks::time_point_label(x, d->layout.typical_spacing, d->utc);
    return chartkit::format_auto(x);
}

// ── Layout ───────────────────────────────────────────────────────────────────

void TimeSeriesChart::compute_layout() {
    auto& L = d->layout;
    L = ts_detail::Layout{};
    const int W = width();
    const int H = height();
    if (W <= 0 || H <= 0)
        return;

    const QFont af = chartkit::axis_font();
    const QFontMetrics fm(af);
    const int gap = std::max(4, fm.averageCharWidth());
    const int row_h = fm.height() + gap;
    L.header_row_h = row_h;

    // ── Header: right block (mode chips, presets) on row 0, then title and
    //    legend flowing from the left, wrapping onto further rows.
    int right_x = W - gap / 2;
    QVector<ts_detail::HitItem> right_items;
    for (int i = static_cast<int>(d->presets.size()) - 1; i >= 0; --i) {
        const int w = fm.horizontalAdvance(d->presets[i]) + gap * 2;
        right_x -= w;
        right_items.prepend({ts_detail::HitKind::Preset, QRect(right_x, 0, w, row_h), i});
    }
    if (d->show_mode_buttons) {
        if (!d->presets.isEmpty())
            right_x -= gap;
        for (auto k :
             {ts_detail::HitKind::ModeRebase, ts_detail::HitKind::ModePercent, ts_detail::HitKind::ModeLog}) {
            const int w = fm.horizontalAdvance(tsp_mode_text(k)) + gap * 2;
            right_x -= w;
            right_items.prepend({k, QRect(right_x, 0, w, row_h), -1});
        }
    }

    int lx = gap / 2;
    int row = 0;
    int limit = (right_items.isEmpty() ? W - gap / 2 : right_x - gap);
    bool has_header = !right_items.isEmpty();
    if (!d->title.isEmpty()) {
        const QFontMetrics tfm(chartkit::title_font());
        // +2: eliding to exactly the advance width can drop the last glyph.
        const int tw = std::min(tfm.horizontalAdvance(d->title) + 2, std::max(0, limit - lx));
        L.title_rect = QRect(lx, 0, tw, row_h);
        lx += tw + gap * 2;
        has_header = true;
    }
    if (d->show_legend) {
        const int sw = std::max(6, fm.ascent() * 4 / 5);
        for (const auto& s : d->series) {
            // A series on a hidden pane leaves the legend until the pane is shown.
            if (!s.spec.show_in_legend || !d->pane_shown(s.spec.pane))
                continue;
            const QString name = s.spec.name.isEmpty() ? tr("Series %1").arg(s.id) : s.spec.name;
            const int w = sw + gap / 2 + fm.horizontalAdvance(name) + gap;
            if (lx + w > limit && lx > gap / 2) {
                ++row;
                lx = gap / 2;
                limit = W - gap / 2;
            }
            L.hits.append({ts_detail::HitKind::Legend, QRect(lx, row * row_h, w, row_h), s.id});
            lx += w + gap / 2;
            has_header = true;
        }
    }
    L.hits += right_items;
    L.header_h = has_header ? (row + 1) * row_h : 0;

    // ── Vertical extents of the shown panes (hidden / removed ones take no
    //    room; the shown ones re-flow by their ratios)
    L.x_axis_h = fm.height() + gap;
    const int np = static_cast<int>(d->panes.size());
    const int pane_gap = gap;
    const int top = L.header_h + gap / 2;
    const int bottom = H - L.x_axis_h;
    const auto pane_states = ts_detail::pane_states(d->panes);
    const QVector<ts_panes::Slot> pane_slots = ts_panes::layout_slots(pane_states, bottom - top, pane_gap);
    L.first_pane = std::max(0, ts_panes::first_shown(pane_states));

    // ── Visible x range
    resolve_view(L.x0, L.x1);
    if (!(L.x1 > L.x0))
        L.x1 = L.x0 + 1.0;

    // ── Bases for the relative value modes: the first visible value
    if (d->value_mode != ValueMode::Absolute) {
        for (const auto& s : d->series) {
            if (!d->series_shown(s) || !d->is_rebased(s) || s.xs.isEmpty())
                continue;
            int i = std::max(0, ts_detail::asof_index(s.xs, L.x0));
            double base = ts_detail::kNaN;
            for (; i < s.xs.size(); ++i) {
                if (std::isfinite(s.ys[i]) && s.ys[i] != 0.0) {
                    base = s.ys[i];
                    break;
                }
            }
            L.rebase.insert(s.id, base);
        }
    }

    // ── Y scales per pane and axis
    L.panes.resize(np);
    for (int pi = 0; pi < np; ++pi) {
        auto& pl = L.panes[pi];
        pl.shown = pane_slots[pi].shown;
        if (!pl.shown)
            continue; // empty rect, unused axes: nothing draws or hit-tests it
        for (int a = 0; a < 2; ++a) {
            ts_detail::AxisScale sc;
            double lo = std::numeric_limits<double>::infinity();
            double hi = -lo;
            double min_pos = lo;
            bool used = false;
            for (const auto& s : d->series) {
                if (!s.spec.visible || s.spec.pane != pi || d->axis_of(s) != a)
                    continue;
                used = true;
                if (s.xs.isEmpty())
                    continue;
                int i0 = static_cast<int>(std::lower_bound(s.xs.begin(), s.xs.end(), L.x0) - s.xs.begin());
                int i1 = static_cast<int>(std::upper_bound(s.xs.begin(), s.xs.end(), L.x1) - s.xs.begin());
                if (i0 >= i1) {
                    const int k = ts_detail::asof_index(s.xs, L.x0);
                    if (k < 0)
                        continue;
                    i0 = k;
                    i1 = k + 1;
                }
                for (int i = i0; i < i1; ++i) {
                    const double v = d->transform(s, s.ys[i]);
                    if (!std::isfinite(v))
                        continue;
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                    if (v > 0.0)
                        min_pos = std::min(min_pos, v);
                }
                const bool has_base = s.spec.style == Style::Bars || s.spec.style == Style::Baseline ||
                                      (s.spec.style == Style::Area && s.spec.baseline.has_value());
                if (has_base) {
                    const double braw = s.spec.baseline.value_or(0.0);
                    const double bv = d->is_rebased(s) ? d->transform(s, braw) : braw;
                    if (std::isfinite(bv)) {
                        lo = std::min(lo, bv);
                        hi = std::max(hi, bv);
                    }
                }
            }
            if (!used) {
                pl.axis[a] = sc;
                continue;
            }
            sc.used = true;
            sc.log = (pi == 0 && a == 0 && d->log_scale && d->value_mode != ValueMode::Percent);
            const auto& fixed = d->panes[pi].fixed[a];
            if (fixed.has_value()) {
                lo = fixed->first;
                hi = fixed->second;
                if (sc.log && lo <= 0.0)
                    sc.log = false;
            } else {
                if (!(lo <= hi)) {
                    lo = 0.0;
                    hi = 1.0;
                }
                if (sc.log) {
                    if (!(hi > 0.0) || !std::isfinite(min_pos)) {
                        sc.log = false;
                    } else {
                        double ll = std::log10(min_pos);
                        double lh = std::log10(hi);
                        double span = lh - ll;
                        if (span < 1e-6)
                            span = 0.02;
                        ll -= span * 0.06;
                        lh += span * 0.06;
                        lo = std::pow(10.0, ll);
                        hi = std::pow(10.0, lh);
                    }
                }
                if (!sc.log) {
                    const bool lo_zero = lo == 0.0;
                    const bool hi_zero = hi == 0.0;
                    const double span = hi - lo;
                    if (span <= 0.0) {
                        const double pad = std::abs(hi) > 0.0 ? std::abs(hi) * 0.02 : 1.0;
                        lo -= pad;
                        hi += pad;
                    } else {
                        // Keep a zero edge exactly at the frame (volume bars, drawdown).
                        const double pad = span * 0.06;
                        if (!lo_zero)
                            lo -= pad;
                        if (!hi_zero)
                            hi += pad;
                    }
                }
            }
            sc.lo = lo;
            sc.hi = hi;
            sc.max_abs = std::max(std::abs(lo), std::abs(hi));
            const int max_ticks = std::max(2, pane_slots[pi].height / std::max(1, fm.height() * 2));
            sc.ticks = sc.log ? chart_ticks::log_ticks(lo, hi, max_ticks) : chart_ticks::nice_ticks(lo, hi, max_ticks);
            pl.axis[a] = sc;
        }
    }

    // ── Gutters from the widest tick label on each side (and wide enough for
    //    the crosshair / line value tags, which use the bolder tooltip font)
    const QFontMetrics tag_fm(chartkit::tooltip_font(true));
    int lw = 0;
    int rw = 0;
    bool any_l = false;
    bool any_r = false;
    for (int pi = 0; pi < np; ++pi) {
        for (int a = 0; a < 2; ++a) {
            const auto& sc = L.panes[pi].axis[a];
            if (!sc.used)
                continue;
            (a == 0 ? any_l : any_r) = true;
            int& w = a == 0 ? lw : rw;
            for (double v : sc.ticks.values)
                w = std::max(w, fm.horizontalAdvance(format_tick_value(pi, a, v)));
            for (double v : {sc.lo, sc.hi})
                w = std::max(w, tag_fm.horizontalAdvance(format_value(pi, a, v)) - gap);
        }
    }
    L.left_w = std::min(W / 3, any_l ? lw + gap * 2 : gap);
    L.right_w = std::min(W / 3, any_r ? rw + gap * 2 : gap);

    // ── Pane rectangles
    const int x_left = L.left_w;
    const int plot_w = std::max(1, W - L.right_w - x_left);
    bool first_rect = true;
    for (int pi = 0; pi < np; ++pi) {
        if (!pane_slots[pi].shown)
            continue;
        L.panes[pi].rect = QRect(x_left, top + pane_slots[pi].top, plot_w, std::max(1, pane_slots[pi].height));
        L.plot = first_rect ? L.panes[pi].rect : L.plot.united(L.panes[pi].rect);
        first_rect = false;
    }
    if (first_rect) // no shown pane (the setters never allow it): keep a sane plot rect
        L.plot = QRect(x_left, top, plot_w, std::max(1, bottom - top));

    // ── X ticks
    if (d->x_is_time) {
        const auto first = chart_ticks::time_ticks(L.x0, L.x1, std::max(2, plot_w / (fm.averageCharWidth() * 10)), d->utc);
        const int label_w = fm.horizontalAdvance(chart_ticks::time_label_sample(first.step)) + gap * 3;
        L.time_ticks = chart_ticks::time_ticks(L.x0, L.x1, std::max(2, plot_w / std::max(1, label_w)), d->utc);
    } else {
        const int label_w = fm.horizontalAdvance(QStringLiteral("-00000.00")) + gap * 3;
        L.num_ticks = chart_ticks::nice_ticks(L.x0, L.x1, std::max(2, plot_w / std::max(1, label_w)));
    }

    // ── Typical point spacing (tooltip time precision)
    double spacing = 0.0;
    for (const auto& s : d->series) {
        if (!d->series_shown(s) || s.xs.size() < 2)
            continue;
        const double sp = (s.xs.last() - s.xs.first()) / static_cast<double>(s.xs.size() - 1);
        if (sp > 0.0 && (spacing == 0.0 || sp < spacing))
            spacing = sp;
    }
    L.typical_spacing = spacing;
    L.valid = true;
}

// ── Static layer ─────────────────────────────────────────────────────────────

void TimeSeriesChart::rebuild_cache() {
    compute_layout();
    d->cache = chartkit::make_layer(this, d->transparent ? QColor(Qt::transparent) : chartkit::background());
    d->dirty = false;
    auto& L = d->layout;
    L.marker_hits.clear();
    if (d->cache.isNull() || !L.valid)
        return;

    QPainter p(&d->cache);
    paint_header(p);

    bool has_data = false;
    for (const auto& s : d->series)
        if (d->series_shown(s) && !s.xs.isEmpty())
            has_data = true;

    const bool blocking = d->state == State::Empty || d->state == State::Error || !has_data;
    if (!blocking) {
        for (int pi = 0; pi < L.panes.size(); ++pi)
            if (L.panes[pi].shown)
                paint_pane(p, pi);
        paint_x_axis(p);
    } else {
        p.setPen(QPen(chartkit::grid(), 1));
        p.drawLine(L.plot.left(), L.plot.bottom(), L.plot.right(), L.plot.bottom());
    }

    if (blocking) {
        const State shown = d->state == State::Ready ? State::Empty : d->state;
        chartkit::draw_state_message(p, L.plot, shown, d->state_message);
    } else if (d->state == State::Loading) {
        // Refreshing over existing data: a small note instead of blanking the plot.
        const QString note = d->state_message.isEmpty() ? chartkit::ChartKitText::tr("Loading…") : d->state_message;
        const QFontMetrics fm(chartkit::tooltip_font(true));
        const int pad = fm.averageCharWidth();
        const QRectF tag(L.plot.right() - fm.horizontalAdvance(note) - 3 * pad, L.plot.top() + pad,
                         fm.horizontalAdvance(note) + 2 * pad, fm.height() + 2);
        chartkit::draw_axis_tag(p, tag, note, chartkit::raised());
    }
}

void TimeSeriesChart::paint_header(QPainter& p) {
    const auto& L = d->layout;
    if (L.header_h <= 0)
        return;
    const QFont af = chartkit::axis_font();
    const QFontMetrics fm(af);
    const int gap = std::max(4, fm.averageCharWidth());

    if (!d->title.isEmpty() && L.title_rect.width() > 0) {
        const QFont tf = chartkit::title_font();
        p.setFont(tf);
        p.setPen(chartkit::text());
        p.drawText(L.title_rect, Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(tf).elidedText(d->title, Qt::ElideRight, L.title_rect.width()));
    }

    p.setFont(af);
    for (const auto& h : L.hits) {
        switch (h.kind) {
            case ts_detail::HitKind::Legend: {
                const auto* s = d->find(h.id);
                if (!s)
                    break;
                const QColor c = d->color_of(*s);
                const bool vis = s->spec.visible;
                const int sw = std::max(6, fm.ascent() * 4 / 5);
                const QRect sr(h.rect.left(), h.rect.center().y() - sw / 2, sw, sw);
                p.save();
                p.setRenderHint(QPainter::Antialiasing, false);
                const bool filled = s->spec.style == Style::Area || s->spec.style == Style::Bars ||
                                    s->spec.style == Style::Baseline;
                if (!vis) {
                    p.setPen(QPen(chartkit::text_dim(), 1));
                    p.setBrush(Qt::NoBrush);
                    p.drawRect(sr.adjusted(0, 0, -1, -1));
                } else if (filled) {
                    p.fillRect(sr, s->spec.style == Style::Baseline ? chartkit::up() : c);
                    if (s->spec.style == Style::Baseline)
                        p.fillRect(QRect(sr.left(), sr.center().y(), sr.width(), sr.height() / 2), chartkit::down());
                } else {
                    // 2 px for the default 1.5 px line; thicker lines read thicker (max 4).
                    const qreal sw_w = std::clamp<qreal>(s->spec.line_width, 2.0, 4.0);
                    QPen pen(c, sw_w, s->spec.dashed ? Qt::DashLine : Qt::SolidLine);
                    p.setPen(pen);
                    p.drawLine(sr.left(), sr.center().y(), sr.right(), sr.center().y());
                }
                p.restore();
                const QString name = s->spec.name.isEmpty() ? tr("Series %1").arg(s->id) : s->spec.name;
                p.setPen(vis ? chartkit::axis_text() : chartkit::text_dim());
                p.drawText(QRect(sr.right() + gap / 2 + 1, h.rect.top(), h.rect.right() - sr.right(), h.rect.height()),
                           Qt::AlignLeft | Qt::AlignVCenter, name);
                break;
            }
            case ts_detail::HitKind::Preset: {
                const QString code = d->presets.value(h.id);
                const bool active =
                    (d->view_mode == ViewChange::Preset && d->active_preset == code) ||
                    (d->view_mode == ViewChange::Auto && tsp_is_all_preset(code));
                const QRect br = h.rect.adjusted(1, gap / 4, -1, -gap / 4);
                if (active)
                    p.fillRect(br, chartkit::with_alpha(chartkit::accent(), 0.16));
                p.setPen(active ? chartkit::accent() : chartkit::axis_text());
                p.drawText(br, Qt::AlignCenter, code);
                break;
            }
            case ts_detail::HitKind::ModeLog:
            case ts_detail::HitKind::ModeRebase:
            case ts_detail::HitKind::ModePercent: {
                bool active = false;
                if (h.kind == ts_detail::HitKind::ModeLog)
                    active = d->log_scale;
                else if (h.kind == ts_detail::HitKind::ModeRebase)
                    active = d->value_mode == ValueMode::Rebased100;
                else
                    active = d->value_mode == ValueMode::Percent;
                const QRect br = h.rect.adjusted(1, gap / 4, -1, -gap / 4);
                p.save();
                p.setRenderHint(QPainter::Antialiasing, false);
                if (active)
                    p.fillRect(br, chartkit::with_alpha(chartkit::accent(), 0.16));
                p.setPen(QPen(active ? chartkit::accent() : chartkit::grid(), 1));
                p.setBrush(Qt::NoBrush);
                p.drawRect(br.adjusted(0, 0, -1, -1));
                p.restore();
                p.setPen(active ? chartkit::accent() : chartkit::axis_text());
                p.drawText(br, Qt::AlignCenter, tsp_mode_text(h.kind));
                break;
            }
        }
    }
}

void TimeSeriesChart::paint_pane(QPainter& p, int pi) {
    auto& L = d->layout;
    const auto& pl = L.panes[pi];
    const QRect r = pl.rect;
    if (r.width() < 4 || r.height() < 4)
        return;
    const QFont af = chartkit::axis_font();
    const QFontMetrics fm(af);
    const int gap = std::max(4, fm.averageCharWidth());

    // Grid.
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(chartkit::grid(), 1));
    const int grid_axis = pl.axis[0].used ? 0 : (pl.axis[1].used ? 1 : -1);
    if (grid_axis >= 0) {
        const auto& sc = pl.axis[grid_axis];
        for (double v : sc.ticks.values) {
            const double y = ts_detail::py_of(r, sc, v);
            if (!std::isfinite(y) || y < r.top() || y > r.bottom() + 1)
                continue;
            const int yi = std::min(r.bottom(), static_cast<int>(std::lround(y)));
            p.drawLine(r.left(), yi, r.right(), yi);
        }
    }
    const QVector<double>& xt = d->x_is_time ? L.time_ticks.values : L.num_ticks.values;
    for (double x : xt) {
        const int X = static_cast<int>(std::lround(ts_detail::px_of(L, x)));
        if (X >= r.left() && X <= r.right())
            p.drawLine(X, r.top(), X, r.bottom());
    }

    // Bands (shared by every pane; label in the first pane only).
    for (const auto& a : d->annotations) {
        if (a.type != ts_detail::AnnType::Band)
            continue;
        double x0 = std::max<double>(r.left(), ts_detail::px_of(L, a.x));
        double x1 = std::min<double>(r.right() + 1, ts_detail::px_of(L, a.x2));
        if (x1 < r.left() || x0 > r.right() + 1)
            continue;
        if (x1 - x0 < 1.0)
            x1 = x0 + 1.0;
        const QColor c = chartkit::role_color(a.role == ChartColorRole::Auto ? ChartColorRole::Muted : a.role);
        p.fillRect(QRectF(x0, r.top(), x1 - x0, r.height()), chartkit::with_alpha(c, 0.12));
        if (pi == L.first_pane && !a.label.isEmpty() && x1 - x0 > gap * 3) {
            p.setFont(af);
            p.setPen(chartkit::axis_text());
            const QRect lr(static_cast<int>(x0) + gap / 2, r.top() + gap / 4, static_cast<int>(x1 - x0) - gap, fm.height());
            p.drawText(lr, Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(a.label, Qt::ElideRight, lr.width()));
        }
    }

    // Series + annotations, clipped to the pane.
    p.save();
    p.setClipRect(r.adjusted(0, -1, 1, 1));
    for (int i = 0; i < d->series.size(); ++i) {
        const auto& s = d->series[i];
        if (s.spec.visible && s.spec.pane == pi)
            paint_series(p, i, pi);
    }
    paint_annotations(p, pi);
    p.restore();

    // Pane title.
    if (!d->panes[pi].title.isEmpty()) {
        QFont tf = af;
        tf.setWeight(QFont::DemiBold);
        const QFontMetrics tfm(tf);
        const QString title = tfm.elidedText(d->panes[pi].title, Qt::ElideRight, r.width() - gap);
        const QRect tr_rect(r.left() + gap / 2, r.top() + gap / 4, tfm.horizontalAdvance(title) + gap / 2, fm.height());
        // Veil so the title stays readable over the series drawn later.
        p.fillRect(tr_rect.adjusted(-gap / 4, 0, 0, 0), chartkit::with_alpha(chartkit::background(), 0.7));
        p.setFont(tf);
        p.setPen(chartkit::axis_text());
        p.drawText(tr_rect, Qt::AlignLeft | Qt::AlignVCenter, title);
    }

    // Frame.
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(chartkit::axis_line(), 1));
    p.drawLine(r.left(), r.bottom(), r.right(), r.bottom());
    if (pl.axis[0].used)
        p.drawLine(r.left(), r.top(), r.left(), r.bottom());
    if (pl.axis[1].used)
        p.drawLine(r.right(), r.top(), r.right(), r.bottom());

    // Vertical spans taken by horizontal-line value tags (drawn below), per
    // axis, so a tick label that would sit under a tag is skipped.
    const bool rebased_tags = pi == 0 && d->pane0_rebased();
    QVector<QPair<double, double>> tag_spans[2];
    {
        const int tag_h = QFontMetrics(chartkit::tooltip_font(true)).height() + 2;
        for (const auto& ann : d->annotations) {
            if (ann.type != ts_detail::AnnType::HLine || ann.pane != pi)
                continue;
            int tax = static_cast<int>(ann.axis);
            if (!pl.axis[tax].used)
                tax = 1 - tax;
            if (!pl.axis[tax].used || (rebased_tags && tax == 0))
                continue;
            const double ty0 = ts_detail::py_of(r, pl.axis[tax], ann.y);
            if (!std::isfinite(ty0) || ty0 < r.top() || ty0 > r.bottom() + 1)
                continue;
            tag_spans[tax].append({ty0 - tag_h / 2.0, ty0 + tag_h / 2.0});
        }
    }

    // Tick labels.
    p.setFont(af);
    p.setPen(chartkit::axis_text());
    const int fh = fm.height();
    for (int a = 0; a < 2; ++a) {
        const auto& sc = pl.axis[a];
        if (!sc.used)
            continue;
        for (double v : sc.ticks.values) {
            const double y = ts_detail::py_of(r, sc, v);
            if (!std::isfinite(y) || y < r.top() - 0.5 || y > r.bottom() + 1.5)
                continue;
            const int ty = std::clamp(static_cast<int>(std::lround(y)) - fh / 2, r.top() - fh / 2, r.bottom() - fh / 2);
            bool under_tag = false;
            for (const auto& span : tag_spans[a]) {
                if (ty < span.second && ty + fh > span.first) {
                    under_tag = true;
                    break;
                }
            }
            if (under_tag)
                continue;
            const QString text = format_tick_value(pi, a, v);
            if (a == 0)
                p.drawText(QRect(0, ty, r.left() - gap, fh), Qt::AlignRight | Qt::AlignVCenter, text);
            else
                p.drawText(QRect(r.right() + gap, ty, width() - r.right() - gap, fh), Qt::AlignLeft | Qt::AlignVCenter,
                           text);
        }
    }

    // Horizontal-line value tags in the gutter.
    const bool rebased0 = pi == 0 && d->pane0_rebased();
    for (const auto& a : d->annotations) {
        if (a.type != ts_detail::AnnType::HLine || a.pane != pi)
            continue;
        int ax = static_cast<int>(a.axis);
        if (!pl.axis[ax].used)
            ax = 1 - ax;
        if (!pl.axis[ax].used || (rebased0 && ax == 0))
            continue;
        const double y = ts_detail::py_of(r, pl.axis[ax], a.y);
        if (!std::isfinite(y) || y < r.top() || y > r.bottom() + 1)
            continue;
        const QColor c = chartkit::role_color(a.role == ChartColorRole::Auto ? ChartColorRole::Muted : a.role);
        const QString text = format_value(pi, ax, a.y);
        const QFontMetrics tfm(chartkit::tooltip_font(true));
        const int th = tfm.height() + 2;
        const int tw = std::min(tfm.horizontalAdvance(text) + gap, ax == 0 ? r.left() : width() - r.right() - 1);
        const QRectF tag = ax == 0 ? QRectF(r.left() - tw, y - th / 2.0, tw, th)
                                   : QRectF(r.right() + 1, y - th / 2.0, tw, th);
        chartkit::draw_axis_tag(p, tag, text, c);
    }
}

void TimeSeriesChart::paint_series(QPainter& p, int series_index, int pi) {
    const auto& s = d->series[series_index];
    const auto& L = d->layout;
    const auto& pl = L.panes[pi];
    const QRect r = pl.rect;
    const int ax = d->axis_of(s);
    const auto& sc = pl.axis[ax];
    if (!sc.used || s.xs.isEmpty())
        return;

    const int n = static_cast<int>(s.xs.size());
    // One point beyond each edge so lines run to the frame.
    const int i0 = std::max(0, static_cast<int>(std::lower_bound(s.xs.begin(), s.xs.end(), L.x0) - s.xs.begin()) - 1);
    const int i1 = std::min(n, static_cast<int>(std::upper_bound(s.xs.begin(), s.xs.end(), L.x1) - s.xs.begin()) + 1);
    if (i0 >= i1)
        return;
    const double ppu = r.width() / (L.x1 - L.x0);

    QVector<int> idx;
    idx.reserve(std::min(i1 - i0, r.width() * 4 + 8));
    chartkit::decimate_minmax(s.xs.constData(), s.ys.constData(), i0, i1, L.x0, ppu, idx);

    const QColor color = d->color_of(s);
    const double bottom_y = r.top() + r.height();
    auto map_pt = [&](int i) {
        return QPointF(ts_detail::px_of(L, s.xs[i]), ts_detail::py_of(r, sc, d->transform(s, s.ys[i])));
    };

    // ── Bars
    if (s.spec.style == Style::Bars) {
        const double base_raw = s.spec.baseline.value_or(0.0);
        double by = ts_detail::py_of(r, sc, base_raw);
        if (!std::isfinite(by))
            by = bottom_y;
        by = std::clamp(by, static_cast<double>(r.top()), bottom_y);
        const double spacing_px = n >= 2 ? (s.xs.last() - s.xs.first()) / (n - 1) * ppu : r.width() / 10.0;
        const QColor up = chartkit::up();
        const QColor dn = chartkit::down();
        // Batched per colour (see chartkit::fill_rects for why not fillRect per bar).
        QVector<QRectF> rects_main;
        QVector<QRectF> rects_down;
        auto add_bar = [&](double v, const QRectF& rect) {
            (s.spec.color_by_sign && v < base_raw ? rects_down : rects_main).append(rect);
        };
        if (spacing_px >= 3.0) {
            const double bw = std::max(1.0, spacing_px * 0.7);
            for (int i = i0; i < i1; ++i) {
                const double v = s.ys[i];
                const double y = ts_detail::py_of(r, sc, v);
                if (!std::isfinite(y))
                    continue;
                const double X = ts_detail::px_of(L, s.xs[i]);
                add_bar(v, QRectF(X - bw / 2.0, std::min(y, by), bw, std::max(1.0, std::abs(by - y))));
            }
        } else {
            for (int i : idx) {
                const double v = s.ys[i];
                const double y = ts_detail::py_of(r, sc, v);
                if (!std::isfinite(y))
                    continue;
                const double X = std::floor(ts_detail::px_of(L, s.xs[i]));
                add_bar(v, QRectF(X, std::min(y, by), 1.0, std::max(1.0, std::abs(by - y))));
            }
        }
        chartkit::fill_rects(p, rects_main, chartkit::with_alpha(s.spec.color_by_sign ? up : color, 0.85));
        chartkit::fill_rects(p, rects_down, chartkit::with_alpha(dn, 0.85));
        return;
    }

    // ── Line-based styles: split into segments at gaps
    QVector<QPolygonF> segs;
    QPolygonF cur;
    cur.reserve(idx.size());
    for (int i : idx) {
        const QPointF q = map_pt(i);
        if (!std::isfinite(q.y())) {
            if (!cur.isEmpty())
                segs.append(cur);
            cur.clear();
            continue;
        }
        cur << q;
    }
    if (!cur.isEmpty())
        segs.append(cur);
    if (segs.isEmpty())
        return;
    if (s.spec.style == Style::Step) {
        for (auto& seg : segs)
            seg = tsp_steps(seg);
    }

    const qreal lw = std::max<qreal>(0.5, s.spec.line_width);
    QPen pen(color, lw, s.spec.dashed ? Qt::DashLine : Qt::SolidLine, s.spec.dashed ? Qt::FlatCap : Qt::RoundCap,
             Qt::RoundJoin);
    p.setRenderHint(QPainter::Antialiasing, true);

    auto stroke = [&](const QPen& pn) {
        for (const auto& seg : segs)
            chartkit::draw_series_line(p, seg, pn);
    };
    // Fills use a per-column envelope, never the raw (self-overlapping)
    // zigzag, and no antialiasing: the stroke drawn on top covers the edge,
    // and antialiased filling of dense columns is very slow in Qt's rasterizer.
    auto fill_to = [&](double base_y, const QBrush& brush, chartkit::EnvelopeMode mode) {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(Qt::NoPen);
        p.setBrush(brush);
        for (const auto& seg : segs) {
            if (seg.size() >= 2)
                p.drawPolygon(chartkit::fill_envelope(seg, base_y, mode));
        }
        p.restore();
    };
    auto base_y_of = [&](double raw, double fallback) {
        const double bv = d->is_rebased(s) ? d->transform(s, raw) : raw;
        const double y = ts_detail::py_of(r, sc, bv);
        return std::isfinite(y) ? std::clamp(y, static_cast<double>(r.top()), bottom_y) : fallback;
    };

    switch (s.spec.style) {
        case Style::Line:
        case Style::Step:
        case Style::Bars:
            stroke(pen);
            break;
        case Style::Area: {
            const double base_y = s.spec.baseline.has_value() ? base_y_of(*s.spec.baseline, bottom_y) : bottom_y;
            // Strongest colour away from the base (a drawdown hanging from 0 is
            // darkest at its trough; a price area is darkest near the line).
            const bool hangs_down = base_y < r.center().y();
            QLinearGradient g(QPointF(0, r.top()), QPointF(0, bottom_y));
            g.setColorAt(0.0, chartkit::with_alpha(color, hangs_down ? 0.06 : 0.32));
            g.setColorAt(1.0, chartkit::with_alpha(color, hangs_down ? 0.32 : 0.04));
            fill_to(base_y, g, chartkit::EnvelopeMode::Farthest);
            stroke(pen);
            break;
        }
        case Style::Baseline: {
            const double base_y = base_y_of(s.spec.baseline.value_or(0.0), bottom_y);
            const QColor up = chartkit::up();
            const QColor dn = chartkit::down();

            // Fills: clamped envelopes, so no clipping of a complex polygon.
            // A dense series (spiky envelope, many spans per scanline) gets a
            // flat opaque fill: Qt blends gradients / translucent colours on its
            // thread pool per span batch, which is slow for such shapes.
            bool dense = false;
            for (const auto& seg : segs)
                dense = dense || chartkit::is_dense(seg);
            if (dense) {
                // One rect per pixel column (polygon scan conversion of a spiky
                // envelope is the slow part), filled opaque.
                auto column_rects = [&](chartkit::EnvelopeMode mode) {
                    QVector<QRectF> rects;
                    for (const auto& seg : segs) {
                        if (seg.size() < 2)
                            continue;
                        const QPolygonF env = chartkit::fill_envelope(seg, base_y, mode);
                        for (int k = 0; k + 2 < env.size(); ++k) {
                            const double h = std::abs(env[k].y() - base_y);
                            if (h >= 0.5)
                                rects.append(QRectF(std::floor(env[k].x()), std::min(env[k].y(), base_y), 1.0, h));
                        }
                    }
                    return rects;
                };
                chartkit::fill_rects(p, column_rects(chartkit::EnvelopeMode::Above), chartkit::with_alpha(up, 0.18));
                chartkit::fill_rects(p, column_rects(chartkit::EnvelopeMode::Below), chartkit::with_alpha(dn, 0.18));
            } else {
                QLinearGradient gu(QPointF(0, r.top()), QPointF(0, base_y));
                gu.setColorAt(0.0, chartkit::with_alpha(up, 0.30));
                gu.setColorAt(1.0, chartkit::with_alpha(up, 0.04));
                fill_to(base_y, gu, chartkit::EnvelopeMode::Above);
                QLinearGradient gd(QPointF(0, base_y), QPointF(0, bottom_y));
                gd.setColorAt(0.0, chartkit::with_alpha(dn, 0.04));
                gd.setColorAt(1.0, chartkit::with_alpha(dn, 0.30));
                fill_to(base_y, gd, chartkit::EnvelopeMode::Below);
            }

            for (const auto& seg : segs)
                chartkit::draw_series_line_split(p, seg, pen, base_y, up, dn, QRectF(r));

            p.setRenderHint(QPainter::Antialiasing, false);
            p.setPen(QPen(chartkit::with_alpha(chartkit::text_dim(), 0.8), 1, Qt::DotLine));
            const int yi = static_cast<int>(std::lround(base_y));
            p.drawLine(r.left(), yi, r.right(), yi);
            break;
        }
    }
}

void TimeSeriesChart::paint_annotations(QPainter& p, int pi) {
    auto& L = d->layout;
    const auto& pl = L.panes[pi];
    const QRect r = pl.rect;
    const QFont af = chartkit::axis_font();
    const QFontMetrics fm(af);
    const int gap = std::max(4, fm.averageCharWidth());
    const bool rebased0 = pi == 0 && d->pane0_rebased();

    // Horizontal lines (their value tags are drawn in the gutter by paint_pane).
    for (const auto& a : d->annotations) {
        if (a.type != ts_detail::AnnType::HLine || a.pane != pi)
            continue;
        int ax = static_cast<int>(a.axis);
        if (!pl.axis[ax].used)
            ax = 1 - ax;
        if (!pl.axis[ax].used || (rebased0 && ax == 0))
            continue;
        const double y = ts_detail::py_of(r, pl.axis[ax], a.y);
        if (!std::isfinite(y) || y < r.top() || y > r.bottom() + 1)
            continue;
        const QColor c = chartkit::role_color(a.role == ChartColorRole::Auto ? ChartColorRole::Muted : a.role);
        const int yi = std::min(r.bottom(), static_cast<int>(std::lround(y)));
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(c, 1, Qt::DashLine));
        p.drawLine(r.left(), yi, r.right(), yi);
        if (!a.label.isEmpty()) {
            p.setFont(af);
            p.setPen(c);
            const int ly = yi - fm.height() - 1 < r.top() ? yi + 1 : yi - fm.height() - 1;
            p.drawText(QRect(r.left() + gap, ly, r.width() - gap * 2, fm.height()), Qt::AlignRight | Qt::AlignVCenter,
                       a.label);
        }
    }

    // Markers.
    const qreal size = fm.height() * 0.95;
    int shown = 0;
    for (const auto& a : d->annotations) {
        if (a.type != ts_detail::AnnType::Marker || a.x < L.x0 || a.x > L.x1)
            continue;
        const ts_detail::Series* ms = a.series >= 0 ? d->find(a.series) : nullptr;
        if (d->pane_shown(ms ? ms->spec.pane : a.pane)) // markers on hidden panes do not count
            ++shown;
    }
    const bool draw_labels = shown <= 40;

    for (const auto& a : d->annotations) {
        if (a.type != ts_detail::AnnType::Marker || a.x < L.x0 || a.x > L.x1)
            continue;
        const ts_detail::Series* s = a.series >= 0 ? d->find(a.series) : nullptr;
        const int mpane = s ? s->spec.pane : a.pane;
        if (mpane != pi)
            continue;
        if (s && !s->spec.visible)
            continue;

        double y = ts_detail::kNaN;
        if (s) {
            const auto& sc = pl.axis[d->axis_of(*s)];
            double v = ts_detail::kNaN;
            if (std::isfinite(a.y)) {
                v = d->transform(*s, a.y);
            } else if (!s->xs.isEmpty()) {
                const int k = std::max(0, ts_detail::asof_index(s->xs, a.x));
                v = d->transform(*s, s->ys[k]);
            }
            if (sc.used)
                y = ts_detail::py_of(r, sc, v);
        } else if (std::isfinite(a.y) && !rebased0) {
            const auto& sc = pl.axis[0].used ? pl.axis[0] : pl.axis[1];
            if (sc.used)
                y = ts_detail::py_of(r, sc, a.y);
        }

        const double X = ts_detail::px_of(L, a.x);
        QPointF at;
        if (std::isfinite(y)) {
            at = QPointF(X, y);
        } else if (a.kind == ChartMarkerKind::Buy) {
            at = QPointF(X, r.bottom() - size * 1.1 - 2);
        } else if (a.kind == ChartMarkerKind::Sell) {
            at = QPointF(X, r.top() + size * 1.1 + 2);
        } else {
            at = QPointF(X, r.bottom() - size * 0.6);
        }
        const QColor c = a.role == ChartColorRole::Auto ? chartkit::marker_color(a.kind) : chartkit::role_color(a.role);
        const QRectF hit = chartkit::draw_marker(p, at, a.kind, c, size);
        L.marker_hits.append({hit.toAlignedRect(), a.id});

        if (draw_labels && !a.label.isEmpty()) {
            p.setFont(af);
            p.setPen(c);
            const int tw = fm.horizontalAdvance(a.label);
            QRectF lr;
            if (a.kind == ChartMarkerKind::Buy)
                lr = QRectF(X - tw / 2.0, hit.bottom(), tw, fm.height());
            else if (a.kind == ChartMarkerKind::Sell)
                lr = QRectF(X - tw / 2.0, hit.top() - fm.height(), tw, fm.height());
            else
                lr = QRectF(hit.right() + 1, at.y() - fm.height() / 2.0, tw, fm.height());
            p.drawText(lr, Qt::AlignCenter, a.label);
        }
    }
}

void TimeSeriesChart::paint_x_axis(QPainter& p) {
    const auto& L = d->layout;
    const QFont af = chartkit::axis_font();
    const QFontMetrics fm(af);
    const int gap = std::max(4, fm.averageCharWidth());
    const int y0 = L.plot.bottom() + 1;
    const int W = width();
    const QVector<double>& ticks = d->x_is_time ? L.time_ticks.values : L.num_ticks.values;
    const double xmax = std::max(std::abs(L.x0), std::abs(L.x1));

    p.setFont(af);
    p.setRenderHint(QPainter::Antialiasing, false);
    int last_right = INT_MIN / 2;
    for (double v : ticks) {
        const int X = static_cast<int>(std::lround(ts_detail::px_of(L, v)));
        if (X < L.plot.left() || X > L.plot.right())
            continue;
        QString text;
        if (d->tick_fmt)
            text = d->tick_fmt(v);
        else if (d->x_is_time)
            text = chart_ticks::time_tick_label(v, L.time_ticks.step, d->utc);
        else
            text = chartkit::format_tick(v, L.num_ticks.step, xmax);
        const int tw = fm.horizontalAdvance(text);
        const int lx = std::clamp(X - tw / 2, 0, std::max(0, W - tw));
        p.setPen(QPen(chartkit::axis_line(), 1));
        p.drawLine(X, y0, X, y0 + gap / 2);
        if (lx < last_right + gap)
            continue;
        p.setPen(chartkit::axis_text());
        p.drawText(lx, y0 + gap / 2 + fm.ascent(), text);
        last_right = lx + tw;
    }
}

// ── Dynamic layer ────────────────────────────────────────────────────────────

void TimeSeriesChart::paintEvent(QPaintEvent*) {
    if (d->dirty || !chartkit::layer_valid(d->cache, this))
        rebuild_cache();
    QPainter p(this);
    if (!d->cache.isNull())
        p.drawPixmap(0, 0, d->cache);
    paint_overlay(p);
}

void TimeSeriesChart::paint_overlay(QPainter& p) {
    const auto& L = d->layout;
    if (!L.valid)
        return;
    const QFont af = chartkit::axis_font();
    const QFontMetrics fm(af);
    const int gap = std::max(4, fm.averageCharWidth());

    if (d->mouse_in && d->hover_hit >= 0 && d->hover_hit < L.hits.size())
        p.fillRect(L.hits[d->hover_hit].rect, colors::overlay_color(0.07));

    const ts_detail::Annotation* hovered = nullptr;
    if (d->mouse_in && d->hover_marker >= 0) {
        for (const auto& a : d->annotations)
            if (a.id == d->hover_marker)
                hovered = &a;
        for (const auto& mh : L.marker_hits) {
            if (mh.id == d->hover_marker) {
                p.save();
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setPen(QPen(chartkit::text(), 1));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(QRectF(mh.rect).adjusted(-1, -1, 1, 1));
                p.restore();
            }
        }
    }

    const bool blocked = d->state == State::Error || d->state == State::Empty;
    double cx = ts_detail::kNaN;
    bool own = false;
    if (!blocked && !d->dragging) {
        if (d->mouse_in && std::isfinite(d->cross_x)) {
            cx = d->cross_x;
            own = true;
        } else if (std::isfinite(d->linked_x)) {
            cx = d->linked_x;
        }
    }

    QVector<chartkit::TooltipRow> rows;
    if (std::isfinite(cx) && cx >= L.x0 && cx <= L.x1) {
        const double X = ts_detail::px_of(L, cx);
        const int Xi = static_cast<int>(std::lround(X));
        p.save();
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(chartkit::crosshair(), 1, Qt::DashLine));
        for (const auto& pl : L.panes)
            if (pl.shown)
                p.drawLine(Xi, pl.rect.top(), Xi, pl.rect.bottom());

        if (own && d->cross_pane >= 0 && d->cross_pane < L.panes.size() && L.panes[d->cross_pane].shown) {
            const auto& pl = L.panes[d->cross_pane];
            const QRect r = pl.rect;
            const int my = std::clamp(d->mouse.y(), r.top(), r.bottom());
            p.drawLine(r.left(), my, r.right(), my);

            const QFontMetrics tfm(chartkit::tooltip_font(true));
            const int th = tfm.height() + 2;
            const int ax = pl.axis[0].used ? 0 : (pl.axis[1].used ? 1 : -1);
            if (ax >= 0) {
                const QString text = format_value(d->cross_pane, ax, ts_detail::v_of(r, pl.axis[ax], my));
                const int tw = tfm.horizontalAdvance(text) + gap;
                const QRectF tag = ax == 0 ? QRectF(std::max(0, r.left() - tw), my - th / 2.0, std::min(tw, r.left()), th)
                                           : QRectF(r.right() + 1, my - th / 2.0, tw, th);
                chartkit::draw_axis_tag(p, tag, text, chartkit::crosshair());
            }
            const QString xt = format_time(cx);
            const int tw = tfm.horizontalAdvance(xt) + gap;
            const double tx = std::clamp(X - tw / 2.0, 0.0, std::max(0.0, static_cast<double>(width() - tw)));
            chartkit::draw_axis_tag(p, QRectF(tx, L.plot.bottom() + 1, tw, th), xt, chartkit::crosshair());
        }

        // Dots and tooltip rows (as-of values).
        p.setRenderHint(QPainter::Antialiasing, true);
        const QColor ring = chartkit::background();
        for (const auto& s : d->series) {
            if (!d->series_shown(s) || !s.spec.show_in_tooltip || s.xs.isEmpty())
                continue;
            const int k = ts_detail::asof_index(s.xs, cx);
            if (k < 0)
                continue;
            const double v = d->transform(s, s.ys[k]);
            const int pane = s.spec.pane;
            if (pane < 0 || pane >= L.panes.size())
                continue;
            const auto& pl = L.panes[pane];
            const int ax = d->axis_of(s);
            const QColor c = d->color_of(s);
            if (std::isfinite(v) && pl.axis[ax].used && s.xs[k] >= L.x0) {
                const double y = ts_detail::py_of(pl.rect, pl.axis[ax], v);
                if (std::isfinite(y) && y >= pl.rect.top() && y <= pl.rect.bottom() + 1) {
                    p.setPen(QPen(ring, 1));
                    p.setBrush(s.spec.style == Style::Baseline
                                   ? (v >= s.spec.baseline.value_or(0.0) ? chartkit::up() : chartkit::down())
                                   : c);
                    p.drawEllipse(QPointF(ts_detail::px_of(L, s.xs[k]), y), 3.5, 3.5);
                }
            }
            if (!own)
                continue;
            chartkit::TooltipRow row;
            row.swatch = c;
            row.name = s.spec.name.isEmpty() ? tr("Series %1").arg(s.id) : s.spec.name;
            if (!d->is_rebased(s) && s.spec.formatter && std::isfinite(v))
                row.value = s.spec.formatter(v);
            else
                row.value = format_value(pane, ax, v);
            const bool signed_style =
                s.spec.style == Style::Baseline || (s.spec.style == Style::Bars && s.spec.color_by_sign);
            if (std::isfinite(v) && (signed_style || (d->is_rebased(s) && d->value_mode == ValueMode::Percent))) {
                const double ref = d->is_rebased(s) ? 0.0 : s.spec.baseline.value_or(0.0);
                row.value_color = v > ref ? chartkit::up() : v < ref ? chartkit::down() : QColor();
            }
            rows.append(row);
        }
        p.restore();
    }

    if (hovered) {
        QVector<chartkit::TooltipRow> mrows;
        for (const auto& line : hovered->tooltip.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
            mrows.append({QColor(), line, QString(), QColor()});
        const QString title = hovered->label.isEmpty() ? QString() : hovered->label;
        chartkit::draw_tooltip(p, rect(), d->mouse, title, mrows, format_time(hovered->x));
    } else if (own && !rows.isEmpty()) {
        chartkit::draw_tooltip(p, rect(), d->mouse, format_time(cx), rows);
    }
}

} // namespace fincept::ui
