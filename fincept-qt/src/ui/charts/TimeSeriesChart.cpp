// TimeSeriesChart — data, view state and interaction.
// Layout and painting live in TsChartPaint.cpp; shared state in TsChartPrivate.h.
#include "ui/charts/TimeSeriesChart.h"

#include "ui/charts/ChartExport.h"
#include "ui/charts/TsChartPrivate.h"
#include "ui/theme/Theme.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QDate>
#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QTimeZone>
#include <QWheelEvent>

#include <numeric>

namespace fincept::ui {

namespace {

/// Drop points with a non-finite x and sort by x (stable) when needed.
void tsc_normalize(QVector<double>& xs, QVector<double>& ys) {
    const int n = static_cast<int>(std::min(xs.size(), ys.size()));
    xs.resize(n);
    ys.resize(n);
    bool all_finite = true;
    for (double x : xs) {
        if (!std::isfinite(x)) {
            all_finite = false;
            break;
        }
    }
    if (!all_finite) {
        int w = 0;
        for (int i = 0; i < n; ++i) {
            if (std::isfinite(xs[i])) {
                xs[w] = xs[i];
                ys[w] = ys[i];
                ++w;
            }
        }
        xs.resize(w);
        ys.resize(w);
    }
    if (std::is_sorted(xs.begin(), xs.end()))
        return;
    QVector<int> order(xs.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&xs](int a, int b) { return xs[a] < xs[b]; });
    QVector<double> sx(xs.size());
    QVector<double> sy(ys.size());
    for (int i = 0; i < order.size(); ++i) {
        sx[i] = xs[order[i]];
        sy[i] = ys[order[i]];
    }
    xs = std::move(sx);
    ys = std::move(sy);
}

/// Smallest average point spacing among the drawn series (0 when unknown).
double tsc_min_spacing(const QVector<ts_detail::Series>& series, const QVector<ts_detail::Pane>& panes) {
    double best = 0.0;
    for (const auto& s : series) {
        if (!s.spec.visible || !ts_detail::pane_is_shown(panes, s.spec.pane) || s.xs.size() < 2)
            continue;
        const double sp = (s.xs.last() - s.xs.first()) / static_cast<double>(s.xs.size() - 1);
        if (sp > 0.0 && (best == 0.0 || sp < best))
            best = sp;
    }
    return best;
}

bool tsc_same(double a, double b) {
    return (std::isnan(a) && std::isnan(b)) || a == b;
}

constexpr double kTscDayMs = 86400000.0;

} // namespace

// ── Construction ─────────────────────────────────────────────────────────────

TimeSeriesChart::TimeSeriesChart(QWidget* parent) : QWidget(parent), d(std::make_unique<Impl>()) {
    d->panes.append(ts_detail::Pane{});
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this]() {
        invalidate();
        updateGeometry();
    });
}

TimeSeriesChart::~TimeSeriesChart() = default;

void TimeSeriesChart::invalidate() {
    d->dirty = true;
    update();
}

// ── Series ───────────────────────────────────────────────────────────────────

int TimeSeriesChart::add_series(const SeriesSpec& spec) {
    ts_detail::Series s;
    s.id = d->next_series_id++;
    s.spec = spec;
    if (!spec.points.isEmpty()) {
        s.xs.reserve(spec.points.size());
        s.ys.reserve(spec.points.size());
        for (const auto& pt : spec.points) {
            s.xs.append(pt.x());
            s.ys.append(pt.y());
        }
    } else {
        s.xs = spec.xs;
        s.ys = spec.ys;
    }
    s.spec.points.clear();
    s.spec.xs.clear();
    s.spec.ys.clear();
    tsc_normalize(s.xs, s.ys);
    s.spec.pane = ts_panes::resolve_pane(ts_detail::pane_states(d->panes), s.spec.pane);
    if (spec.palette_index >= 0)
        s.palette_slot = spec.palette_index;
    else if (spec.color == ColorRole::Auto || spec.color == ColorRole::Palette)
        s.palette_slot = d->next_palette_slot++;
    d->series.append(std::move(s));
    invalidate();
    return d->series.last().id;
}

void TimeSeriesChart::set_series_data(int series, const QVector<QPointF>& points) {
    auto* s = d->find(series);
    if (!s)
        return;
    s->xs.clear();
    s->ys.clear();
    s->xs.reserve(points.size());
    s->ys.reserve(points.size());
    for (const auto& pt : points) {
        s->xs.append(pt.x());
        s->ys.append(pt.y());
    }
    tsc_normalize(s->xs, s->ys);
    invalidate();
}

void TimeSeriesChart::set_series_data(int series, const QVector<double>& xs, const QVector<double>& ys) {
    auto* s = d->find(series);
    if (!s)
        return;
    s->xs = xs;
    s->ys = ys;
    tsc_normalize(s->xs, s->ys);
    invalidate();
}

void TimeSeriesChart::append_point(int series, double x, double y) {
    auto* s = d->find(series);
    if (!s || !std::isfinite(x))
        return;
    if (s->xs.isEmpty() || x > s->xs.last()) {
        s->xs.append(x);
        s->ys.append(y);
    } else if (x == s->xs.last()) {
        s->ys.last() = y;
    } else {
        const auto it = std::lower_bound(s->xs.begin(), s->xs.end(), x);
        const int pos = static_cast<int>(it - s->xs.begin());
        if (it != s->xs.end() && *it == x) {
            s->ys[pos] = y;
        } else {
            s->xs.insert(pos, x);
            s->ys.insert(pos, y);
        }
    }
    invalidate();
}

void TimeSeriesChart::remove_series(int series) {
    d->series.removeIf([series](const ts_detail::Series& s) { return s.id == series; });
    invalidate();
}

void TimeSeriesChart::clear() {
    d->series.clear();
    d->annotations.clear();
    d->next_palette_slot = 0;
    if (d->view_mode == ViewChange::Manual)
        d->view_mode = ViewChange::Auto;
    d->cross_x = ts_detail::kNaN;
    d->hover_marker = -1;
    invalidate();
}

void TimeSeriesChart::set_series_visible(int series, bool visible) {
    auto* s = d->find(series);
    if (!s || s->spec.visible == visible)
        return;
    s->spec.visible = visible;
    invalidate();
    emit series_visibility_changed(series, visible);
}

bool TimeSeriesChart::series_visible(int series) const {
    const auto* s = d->find(series);
    return s && s->spec.visible;
}

void TimeSeriesChart::set_series_name(int series, const QString& name) {
    if (auto* s = d->find(series)) {
        s->spec.name = name;
        invalidate();
    }
}

void TimeSeriesChart::set_series_style(int series, Style style) {
    if (auto* s = d->find(series)) {
        s->spec.style = style;
        invalidate();
    }
}

void TimeSeriesChart::set_series_line_width(int series, qreal px) {
    auto* s = d->find(series);
    const auto w = ts_panes::line_width_px(px);
    if (!s || !w || s->spec.line_width == *w)
        return;
    s->spec.line_width = *w;
    invalidate();
}

qreal TimeSeriesChart::series_line_width(int series) const {
    const auto* s = d->find(series);
    return s ? s->spec.line_width : 0.0;
}

int TimeSeriesChart::find_series(const QString& id) const {
    if (id.isEmpty())
        return -1;
    for (const auto& s : d->series)
        if (s.spec.id == id)
            return s.id;
    return -1;
}

QVector<int> TimeSeriesChart::series_ids() const {
    QVector<int> out;
    out.reserve(d->series.size());
    for (const auto& s : d->series)
        out.append(s.id);
    return out;
}

int TimeSeriesChart::point_count(int series) const {
    const auto* s = d->find(series);
    return s ? static_cast<int>(s->xs.size()) : 0;
}

// ── Panes ────────────────────────────────────────────────────────────────────

int TimeSeriesChart::add_pane(double height_ratio) {
    ts_detail::Pane p;
    p.ratio = height_ratio > 0.0 ? height_ratio : 0.35;
    d->panes.append(p);
    invalidate();
    return static_cast<int>(d->panes.size()) - 1;
}

void TimeSeriesChart::set_pane_ratio(int pane, double height_ratio) {
    if (pane < 0 || pane >= d->panes.size() || !(height_ratio > 0.0))
        return;
    d->panes[pane].ratio = height_ratio;
    invalidate();
}

void TimeSeriesChart::set_pane_title(int pane, const QString& title) {
    if (pane < 0 || pane >= d->panes.size())
        return;
    d->panes[pane].title = title;
    invalidate();
}

int TimeSeriesChart::pane_count() const {
    return static_cast<int>(d->panes.size());
}

bool TimeSeriesChart::set_pane_visible(int pane, bool visible) {
    const auto states = ts_detail::pane_states(d->panes);
    if (!ts_panes::is_alive(states, pane))
        return false;
    if (d->panes[pane].visible == visible)
        return true;
    if (!visible && !ts_panes::can_hide(states, pane))
        return false;
    d->panes[pane].visible = visible;
    drop_pointer_state();
    invalidate();
    emit pane_visibility_changed(pane, visible);
    return true;
}

bool TimeSeriesChart::pane_visible(int pane) const {
    return d->pane_shown(pane);
}

bool TimeSeriesChart::has_pane(int pane) const {
    return ts_panes::is_alive(ts_detail::pane_states(d->panes), pane);
}

bool TimeSeriesChart::remove_pane(int pane) {
    if (!ts_panes::can_remove(ts_detail::pane_states(d->panes), pane))
        return false;
    QVector<int> gone;
    for (const auto& s : d->series)
        if (s.spec.pane == pane)
            gone.append(s.id);
    d->series.removeIf([pane](const ts_detail::Series& s) { return s.spec.pane == pane; });
    // Its horizontal lines and markers go with it; a marker attached to a
    // series that lives on another pane stays with that series; bands span
    // every pane and stay.
    d->annotations.removeIf([this, pane, &gone](const ts_detail::Annotation& a) {
        if (a.type == ts_detail::AnnType::Band)
            return false;
        if (a.type == ts_detail::AnnType::Marker && a.series >= 0) {
            if (gone.contains(a.series))
                return true;
            if (d->find(a.series))
                return false;
        }
        return a.pane == pane;
    });
    auto& p = d->panes[pane];
    p.removed = true;
    p.visible = false;
    p.title.clear();
    p.fmt[0] = nullptr;
    p.fmt[1] = nullptr;
    p.fixed[0].reset();
    p.fixed[1].reset();
    drop_pointer_state();
    invalidate();
    return true;
}

void TimeSeriesChart::drop_pointer_state() {
    // Pane geometry changes: hit rects and the crosshair's pane are stale until
    // the next layout; the next mouse move rebuilds them.
    d->hover_hit = -1;
    d->hover_marker = -1;
    d->cross_pane = -1;
    if (std::isfinite(d->cross_x)) {
        d->cross_x = ts_detail::kNaN;
        emit crosshair_moved(d->cross_x);
    }
}

void TimeSeriesChart::set_y_range(int pane, Axis axis, double lo, double hi) {
    if (pane < 0 || pane >= d->panes.size())
        return;
    auto& slot = d->panes[pane].fixed[static_cast<int>(axis)];
    if (std::isfinite(lo) && std::isfinite(hi) && hi > lo)
        slot = std::make_pair(lo, hi);
    else
        slot.reset();
    invalidate();
}

// ── View ─────────────────────────────────────────────────────────────────────

bool TimeSeriesChart::data_extent(double& lo, double& hi) const {
    lo = std::numeric_limits<double>::infinity();
    hi = -lo;
    // Drawn series (visible, on a shown pane) set the extent; with none drawn,
    // every series does (so the axis still has a sensible range).
    bool any_visible = false;
    for (const auto& s : d->series)
        if (d->series_shown(s) && !s.xs.isEmpty())
            any_visible = true;
    for (const auto& s : d->series) {
        if (s.xs.isEmpty() || (any_visible && !d->series_shown(s)))
            continue;
        double a = s.xs.first();
        double b = s.xs.last();
        if (s.spec.style == Style::Bars && s.xs.size() >= 2) {
            const double half = (b - a) / static_cast<double>(s.xs.size() - 1) / 2.0; // keep the edge bars whole
            a -= half;
            b += half;
        }
        lo = std::min(lo, a);
        hi = std::max(hi, b);
    }
    if (!(lo <= hi))
        return false;
    if (hi - lo <= 0.0) {
        const double pad = d->x_is_time ? kTscDayMs / 2.0 : std::max(std::abs(lo) * 0.01, 0.5);
        lo -= pad;
        hi += pad;
    }
    return true;
}

bool TimeSeriesChart::preset_range(const QString& code, double lo, double hi, double& from, double& to) const {
    const QString c = code.trimmed().toUpper();
    to = hi;
    if (c == QLatin1String("ALL") || c == QLatin1String("MAX")) {
        from = lo;
        return true;
    }
    if (!d->x_is_time)
        return false;
    const QTimeZone tz = d->utc ? QTimeZone(QTimeZone::UTC) : QTimeZone(QTimeZone::LocalTime);
    const QDateTime end = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(std::llround(hi)), tz);
    QDateTime start;
    if (c == QLatin1String("YTD")) {
        start = QDate(end.date().year(), 1, 1).startOfDay(tz);
    } else {
        static const QRegularExpression kTscPresetRe(QStringLiteral("^(\\d+)\\s*([HDWMY])$"));
        const auto m = kTscPresetRe.match(c);
        if (!m.hasMatch())
            return false;
        const int n = m.captured(1).toInt();
        const QChar unit = m.captured(2).at(0);
        if (unit == QLatin1Char('H'))
            start = end.addSecs(-3600LL * n);
        else if (unit == QLatin1Char('D'))
            start = end.addDays(-n);
        else if (unit == QLatin1Char('W'))
            start = end.addDays(-7LL * n);
        else if (unit == QLatin1Char('M'))
            start = end.addMonths(-n);
        else
            start = end.addYears(-n);
    }
    from = static_cast<double>(start.toMSecsSinceEpoch());
    if (from < lo || from >= to)
        from = lo;
    return true;
}

void TimeSeriesChart::resolve_view(double& from, double& to) const {
    double lo, hi;
    if (!data_extent(lo, hi)) {
        from = 0.0;
        to = 1.0;
        return;
    }
    switch (d->view_mode) {
        case ViewChange::Auto:
            from = lo;
            to = hi;
            return;
        case ViewChange::Preset:
            if (!preset_range(d->active_preset, lo, hi, from, to)) {
                from = lo;
                to = hi;
            }
            return;
        case ViewChange::Manual:
            break;
    }
    from = d->view_from;
    to = d->view_to;
    const double span = to - from;
    if (!(span > 0.0) || span >= hi - lo) {
        from = lo;
        to = hi;
        return;
    }
    if (from < lo) {
        from = lo;
        to = lo + span;
    } else if (to > hi) {
        to = hi;
        from = hi - span;
    }
}

void TimeSeriesChart::apply_view(ViewChange mode, double from, double to, const QString& preset) {
    double of, ot;
    resolve_view(of, ot);
    d->view_mode = mode;
    d->view_from = from;
    d->view_to = to;
    d->active_preset = preset;
    invalidate();
    double nf, nt;
    resolve_view(nf, nt);
    if (nf != of || nt != ot)
        emit range_changed(nf, nt);
}

void TimeSeriesChart::set_x_range(double from, double to) {
    if (!std::isfinite(from) || !std::isfinite(to))
        return;
    if (to < from)
        std::swap(from, to);
    if (to == from)
        return;
    apply_view(ViewChange::Manual, from, to, {});
}

void TimeSeriesChart::reset_x_range() {
    apply_view(ViewChange::Auto, 0.0, 0.0, {});
}

double TimeSeriesChart::x_from() const {
    double f, t;
    resolve_view(f, t);
    return f;
}

double TimeSeriesChart::x_to() const {
    double f, t;
    resolve_view(f, t);
    return t;
}

void TimeSeriesChart::set_range_presets(const QStringList& presets) {
    d->presets = presets;
    invalidate();
}

QStringList TimeSeriesChart::range_presets() const {
    return d->presets;
}

void TimeSeriesChart::set_active_preset(const QString& preset) {
    if (preset.isEmpty())
        apply_view(ViewChange::Auto, 0.0, 0.0, {});
    else
        apply_view(ViewChange::Preset, 0.0, 0.0, preset);
}

QString TimeSeriesChart::active_preset() const {
    return d->view_mode == ViewChange::Preset ? d->active_preset : QString();
}

void TimeSeriesChart::set_value_mode(ValueMode mode) {
    if (d->value_mode == mode)
        return;
    d->value_mode = mode;
    invalidate();
    emit value_mode_changed(mode);
}

TimeSeriesChart::ValueMode TimeSeriesChart::value_mode() const {
    return d->value_mode;
}

void TimeSeriesChart::set_log_scale(bool on) {
    if (d->log_scale == on)
        return;
    d->log_scale = on;
    invalidate();
    emit log_scale_changed(on);
}

bool TimeSeriesChart::log_scale() const {
    return d->log_scale;
}

void TimeSeriesChart::zoom_at(double anchor, double factor) {
    double lo, hi;
    if (!data_extent(lo, hi) || !(factor > 0.0))
        return;
    double f, t;
    resolve_view(f, t);
    const double span = t - f;
    const double full = hi - lo;
    if (!(span > 0.0) || !(full > 0.0))
        return;
    const double min_span = std::max(full * 1e-4, tsc_min_spacing(d->series, d->panes) * 3.0);
    const double new_span = std::clamp(span * factor, std::min(min_span, full), full);
    if (new_span >= full * 0.9999) {
        apply_view(ViewChange::Auto, 0.0, 0.0, {});
        return;
    }
    const double rel = std::clamp((anchor - f) / span, 0.0, 1.0);
    const double nf = anchor - rel * new_span;
    apply_view(ViewChange::Manual, nf, nf + new_span, {});
}

// ── Appearance / behaviour ───────────────────────────────────────────────────

void TimeSeriesChart::set_title(const QString& title) {
    d->title = title;
    invalidate();
}

void TimeSeriesChart::set_show_legend(bool on) {
    d->show_legend = on;
    invalidate();
}

void TimeSeriesChart::set_show_mode_buttons(bool on) {
    d->show_mode_buttons = on;
    invalidate();
}

void TimeSeriesChart::set_crosshair_enabled(bool on) {
    d->crosshair_enabled = on;
    if (!on)
        d->cross_x = ts_detail::kNaN;
    update();
}

void TimeSeriesChart::set_zoom_enabled(bool on) {
    d->zoom_enabled = on;
}

void TimeSeriesChart::set_x_is_time(bool on) {
    d->x_is_time = on;
    invalidate();
}

void TimeSeriesChart::set_utc(bool on) {
    d->utc = on;
    invalidate();
}

void TimeSeriesChart::set_transparent_background(bool on) {
    d->transparent = on;
    setAttribute(Qt::WA_OpaquePaintEvent, !on);
    invalidate();
}

void TimeSeriesChart::set_value_formatter(Axis axis, ValueFormatter fn, int pane) {
    if (pane < 0 || pane >= d->panes.size())
        return;
    d->panes[pane].fmt[static_cast<int>(axis)] = std::move(fn);
    invalidate();
}

void TimeSeriesChart::set_time_formatter(TimeFormatter fn) {
    d->time_fmt = std::move(fn);
    update();
}

void TimeSeriesChart::set_tick_time_formatter(TimeFormatter fn) {
    d->tick_fmt = std::move(fn);
    invalidate();
}

void TimeSeriesChart::set_state(State state, const QString& message) {
    d->state = state;
    d->state_message = message;
    invalidate();
}

TimeSeriesChart::State TimeSeriesChart::state() const {
    return d->state;
}

void TimeSeriesChart::set_export_name(const QString& name) {
    d->export_name = name;
}

void TimeSeriesChart::set_context_menu_extra(std::function<void(QMenu*)> fn) {
    d->menu_extra = std::move(fn);
}

void TimeSeriesChart::set_linked_crosshair(double x) {
    if (tsc_same(d->linked_x, x))
        return;
    d->linked_x = x;
    update();
}

// ── Annotations ──────────────────────────────────────────────────────────────

int TimeSeriesChart::add_marker(const MarkerSpec& spec) {
    ts_detail::Annotation a;
    a.id = d->next_annotation_id++;
    a.type = ts_detail::AnnType::Marker;
    a.x = spec.x;
    a.series = spec.series;
    a.y = spec.price;
    a.kind = spec.kind;
    a.label = spec.label;
    a.tooltip = spec.tooltip;
    a.role = spec.color;
    a.pane = ts_panes::resolve_pane(ts_detail::pane_states(d->panes), spec.pane);
    d->annotations.append(a);
    invalidate();
    return a.id;
}

int TimeSeriesChart::add_marker(double x, int series, MarkerKind kind, const QString& label, const QString& tooltip) {
    MarkerSpec m;
    m.x = x;
    m.series = series;
    m.kind = kind;
    m.label = label;
    m.tooltip = tooltip;
    if (const auto* s = d->find(series))
        m.pane = s->spec.pane;
    return add_marker(m);
}

int TimeSeriesChart::add_price_marker(double x, double price, MarkerKind kind, const QString& label,
                                      const QString& tooltip) {
    MarkerSpec m;
    m.x = x;
    m.price = price;
    m.kind = kind;
    m.label = label;
    m.tooltip = tooltip;
    return add_marker(m);
}

int TimeSeriesChart::add_hline(double y, const QString& label, ColorRole role, int pane, Axis axis) {
    ts_detail::Annotation a;
    a.id = d->next_annotation_id++;
    a.type = ts_detail::AnnType::HLine;
    a.y = y;
    a.label = label;
    a.role = role;
    a.pane = ts_panes::resolve_pane(ts_detail::pane_states(d->panes), pane);
    a.axis = axis;
    d->annotations.append(a);
    invalidate();
    return a.id;
}

int TimeSeriesChart::add_band(double x_from, double x_to, ColorRole role, const QString& label) {
    ts_detail::Annotation a;
    a.id = d->next_annotation_id++;
    a.type = ts_detail::AnnType::Band;
    a.x = std::min(x_from, x_to);
    a.x2 = std::max(x_from, x_to);
    a.role = role;
    a.label = label;
    a.pane = -1;
    d->annotations.append(a);
    invalidate();
    return a.id;
}

void TimeSeriesChart::remove_annotation(int id) {
    d->annotations.removeIf([id](const ts_detail::Annotation& a) { return a.id == id; });
    if (d->hover_marker == id)
        d->hover_marker = -1;
    invalidate();
}

void TimeSeriesChart::clear_annotations() {
    d->annotations.clear();
    d->hover_marker = -1;
    invalidate();
}

// ── Export ───────────────────────────────────────────────────────────────────

QPixmap TimeSeriesChart::render_image() {
    if (d->dirty || !chartkit::layer_valid(d->cache, this))
        rebuild_cache();
    QPixmap out = chartkit::make_layer(this, chartkit::background());
    if (out.isNull())
        return out;
    QPainter p(&out);
    p.drawPixmap(0, 0, d->cache);
    return out;
}

void TimeSeriesChart::csv_table(QStringList& headers, QVector<QStringList>& rows) const {
    headers.clear();
    rows.clear();
    QVector<const ts_detail::Series*> cols;
    for (const auto& s : d->series)
        if (d->series_shown(s) && !s.xs.isEmpty())
            cols.append(&s);
    if (cols.isEmpty())
        for (const auto& s : d->series)
            if (!s.xs.isEmpty())
                cols.append(&s);
    if (cols.isEmpty())
        return;

    headers << (d->x_is_time ? tr("Time") : tr("X"));
    QVector<double> all;
    double spacing = 0.0;
    for (const auto* s : cols) {
        headers << (s->spec.name.isEmpty() ? tr("Series %1").arg(s->id) : s->spec.name);
        all += s->xs;
        if (s->xs.size() >= 2) {
            const double sp = (s->xs.last() - s->xs.first()) / static_cast<double>(s->xs.size() - 1);
            if (spacing == 0.0 || sp < spacing)
                spacing = sp;
        }
    }
    std::sort(all.begin(), all.end());
    all.erase(std::unique(all.begin(), all.end()), all.end());

    const QTimeZone tz = d->utc ? QTimeZone(QTimeZone::UTC) : QTimeZone(QTimeZone::LocalTime);
    const QString time_fmt = (spacing > 0.0 && spacing < 0.9 * kTscDayMs) ? QStringLiteral("yyyy-MM-dd HH:mm:ss")
                                                                         : QStringLiteral("yyyy-MM-dd");
    QVector<int> ptr(cols.size(), 0);
    rows.reserve(all.size());
    for (double x : all) {
        QStringList row;
        row.reserve(cols.size() + 1);
        if (d->x_is_time)
            row << QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(std::llround(x)), tz).toString(time_fmt);
        else
            row << QString::number(x, 'g', 15);
        for (int c = 0; c < cols.size(); ++c) {
            const auto& xs = cols[c]->xs;
            int& i = ptr[c];
            while (i < xs.size() && xs[i] < x)
                ++i;
            if (i < xs.size() && xs[i] == x && std::isfinite(cols[c]->ys[i]))
                row << QString::number(cols[c]->ys[i], 'g', 12);
            else
                row << QString();
        }
        rows.append(row);
    }
}

// ── Interaction ──────────────────────────────────────────────────────────────

void TimeSeriesChart::update_hover(const QPoint& pos) {
    const auto& L = d->layout;
    int hh = -1;
    for (int i = 0; i < L.hits.size(); ++i) {
        if (L.hits[i].rect.contains(pos)) {
            hh = i;
            break;
        }
    }
    int hm = -1;
    if (hh < 0) {
        for (int i = static_cast<int>(L.marker_hits.size()) - 1; i >= 0; --i) {
            if (L.marker_hits[i].rect.contains(pos)) {
                hm = L.marker_hits[i].id;
                break;
            }
        }
    }
    d->hover_hit = hh;
    d->hover_marker = hm;
    if (d->dragging)
        setCursor(Qt::ClosedHandCursor);
    else if (hh >= 0 || hm >= 0)
        setCursor(Qt::PointingHandCursor);
    else
        unsetCursor();
}

void TimeSeriesChart::update_crosshair(const QPoint& pos) {
    const double old = d->cross_x;
    d->cross_x = ts_detail::kNaN;
    d->cross_pane = -1;
    const auto& L = d->layout;
    const bool can_show = d->crosshair_enabled && L.valid && d->hover_hit < 0 && d->state != State::Error &&
                          d->state != State::Empty;
    if (can_show) {
        for (int i = 0; i < L.panes.size(); ++i) {
            if (L.panes[i].shown && L.panes[i].rect.contains(pos)) {
                d->cross_pane = i;
                break;
            }
        }
    }
    if (d->cross_pane >= 0) {
        const double x = ts_detail::x_of(L, pos.x());
        double best = ts_detail::kNaN;
        double best_d = std::numeric_limits<double>::infinity();
        for (const auto& s : d->series) {
            if (!d->series_shown(s) || !s.spec.show_in_tooltip || s.xs.isEmpty())
                continue;
            const auto it = std::lower_bound(s.xs.begin(), s.xs.end(), x);
            const int i = static_cast<int>(it - s.xs.begin());
            for (int c : {i - 1, i}) {
                if (c < 0 || c >= s.xs.size())
                    continue;
                const double dist = std::abs(s.xs[c] - x);
                if (dist < best_d) {
                    best_d = dist;
                    best = s.xs[c];
                }
            }
        }
        d->cross_x = std::isfinite(best) ? best : x;
    }
    if (!tsc_same(old, d->cross_x))
        emit crosshair_moved(d->cross_x);
}

void TimeSeriesChart::click_header_item(int hit_index) {
    if (hit_index < 0 || hit_index >= d->layout.hits.size())
        return;
    const ts_detail::HitItem h = d->layout.hits[hit_index];
    switch (h.kind) {
        case ts_detail::HitKind::Legend:
            set_series_visible(h.id, !series_visible(h.id));
            break;
        case ts_detail::HitKind::Preset:
            if (h.id >= 0 && h.id < d->presets.size()) {
                const QString name = d->presets[h.id];
                set_active_preset(name);
                emit range_preset_selected(name);
            }
            break;
        case ts_detail::HitKind::ModeLog:
            set_log_scale(!d->log_scale);
            break;
        case ts_detail::HitKind::ModeRebase:
            set_value_mode(d->value_mode == ValueMode::Rebased100 ? ValueMode::Absolute : ValueMode::Rebased100);
            break;
        case ts_detail::HitKind::ModePercent:
            set_value_mode(d->value_mode == ValueMode::Percent ? ValueMode::Absolute : ValueMode::Percent);
            break;
    }
}

void TimeSeriesChart::mouseMoveEvent(QMouseEvent* e) {
    const QPoint pos = e->position().toPoint();
    d->mouse = pos;
    d->mouse_in = true;
    if (d->dragging) {
        if (!d->drag_moved && (pos - d->press_pos).manhattanLength() > 3)
            d->drag_moved = true;
        if (d->drag_moved && d->zoom_enabled && d->layout.valid && d->layout.plot.width() > 0) {
            const double span = d->press_to - d->press_from;
            const double dx = -(pos.x() - d->press_pos.x()) * span / d->layout.plot.width();
            double lo, hi;
            if (data_extent(lo, hi) && span < hi - lo)
                apply_view(ViewChange::Manual, d->press_from + dx, d->press_to + dx, {});
        }
        d->cross_x = ts_detail::kNaN;
        update();
        return;
    }
    update_hover(pos);
    update_crosshair(pos);
    update();
}

void TimeSeriesChart::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(e);
        return;
    }
    const QPoint pos = e->position().toPoint();
    update_hover(pos);
    if (d->hover_hit >= 0) {
        click_header_item(d->hover_hit);
        e->accept();
        return;
    }
    if (d->layout.valid && d->layout.plot.contains(pos)) {
        d->press_pos = pos;
        d->drag_moved = false;
        d->dragging = d->zoom_enabled || d->hover_marker >= 0;
        resolve_view(d->press_from, d->press_to);
        if (d->dragging && d->hover_marker < 0)
            setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void TimeSeriesChart::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && d->dragging) {
        const bool clicked = !d->drag_moved;
        d->dragging = false;
        d->drag_moved = false;
        const QPoint pos = e->position().toPoint();
        update_hover(pos);
        if (clicked && d->hover_marker >= 0)
            emit marker_clicked(d->hover_marker);
        update_crosshair(pos);
        update();
        e->accept();
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void TimeSeriesChart::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && d->layout.valid && d->layout.plot.contains(e->position().toPoint()) &&
        d->hover_marker < 0) {
        reset_x_range();
        e->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(e);
}

void TimeSeriesChart::wheelEvent(QWheelEvent* e) {
    const QPoint pos = e->position().toPoint();
    if (!d->zoom_enabled || !d->layout.valid || !d->layout.plot.contains(pos)) {
        e->ignore();
        return;
    }
    const QPoint ad = e->angleDelta();
    const int delta = ad.y() != 0 ? ad.y() : ad.x();
    if (delta == 0) {
        e->ignore();
        return;
    }
    const double factor = std::pow(0.85, delta / 120.0);
    zoom_at(ts_detail::x_of(d->layout, pos.x()), factor);
    e->accept();
}

void TimeSeriesChart::leaveEvent(QEvent* e) {
    d->mouse_in = false;
    d->hover_hit = -1;
    d->hover_marker = -1;
    if (!d->dragging) {
        const bool had = std::isfinite(d->cross_x);
        d->cross_x = ts_detail::kNaN;
        d->cross_pane = -1;
        if (had)
            emit crosshair_moved(d->cross_x);
    }
    unsetCursor();
    update();
    QWidget::leaveEvent(e);
}

void TimeSeriesChart::build_context_menu(QMenu* menu) {
    ChartExport::MenuSpec spec;
    spec.suggested_name = !d->export_name.isEmpty() ? d->export_name
                          : !d->title.isEmpty()     ? d->title
                                                    : QStringLiteral("chart");
    spec.image = [this]() { return render_image(); };
    spec.csv = [this]() {
        ChartExport::Table t;
        csv_table(t.headers, t.rows);
        return t;
    };
    spec.extra = [this](QMenu* m) {
        QAction* log = m->addAction(tr("Log Scale"));
        log->setCheckable(true);
        log->setChecked(d->log_scale);
        connect(log, &QAction::toggled, this, [this](bool on) { set_log_scale(on); });

        QAction* rebase = m->addAction(tr("Rebase to 100"));
        rebase->setCheckable(true);
        rebase->setChecked(d->value_mode == ValueMode::Rebased100);
        connect(rebase, &QAction::toggled, this,
                [this](bool on) { set_value_mode(on ? ValueMode::Rebased100 : ValueMode::Absolute); });

        QAction* pct = m->addAction(tr("Percent Change"));
        pct->setCheckable(true);
        pct->setChecked(d->value_mode == ValueMode::Percent);
        connect(pct, &QAction::toggled, this,
                [this](bool on) { set_value_mode(on ? ValueMode::Percent : ValueMode::Absolute); });

        m->addSeparator();
        QAction* reset = m->addAction(tr("Reset Zoom"));
        reset->setEnabled(d->view_mode == ViewChange::Manual);
        connect(reset, &QAction::triggered, this, [this]() { reset_x_range(); });

        if (d->menu_extra) {
            m->addSeparator();
            d->menu_extra(m);
        }
    };
    ChartExport::populate_menu(menu, this, spec);
}

void TimeSeriesChart::contextMenuEvent(QContextMenuEvent* e) {
    QMenu menu(this);
    build_context_menu(&menu);
    menu.exec(e->globalPos());
    e->accept();
}

void TimeSeriesChart::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    d->dirty = true;
}

void TimeSeriesChart::changeEvent(QEvent* e) {
    switch (e->type()) {
        case QEvent::LanguageChange:
        case QEvent::FontChange:
        case QEvent::EnabledChange:
            invalidate();
            break;
        default:
            break;
    }
    QWidget::changeEvent(e);
}

QSize TimeSeriesChart::sizeHint() const {
    const QFontMetrics fm(chartkit::axis_font());
    return {fm.averageCharWidth() * 72, fm.height() * 20};
}

QSize TimeSeriesChart::minimumSizeHint() const {
    const QFontMetrics fm(chartkit::axis_font());
    return {fm.averageCharWidth() * 24, fm.height() * 7};
}

} // namespace fincept::ui
