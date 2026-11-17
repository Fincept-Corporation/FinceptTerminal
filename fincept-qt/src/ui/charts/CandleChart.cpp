#include "ui/charts/CandleChart.h"

#include "ui/charts/CandleData.h"
#include "ui/charts/ChartAxisTicks.h"
#include "ui/charts/ChartExport.h"
#include "ui/charts/ChartRanges.h"
#include "ui/theme/Theme.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QTimeZone>
#include <QWheelEvent>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace fincept::ui {

namespace {

constexpr double kCcRightMargin = 3.0; ///< empty slots right of the last bar while following
constexpr double kCcMinSlots = 10.0;   ///< zoom-in floor
constexpr double kCcDayMs = 86400000.0;
constexpr double kCcNaN = std::numeric_limits<double>::quiet_NaN();

bool cc_bar_ok(const CandleChart::Bar& b) {
    return std::isfinite(b.time_ms) && std::isfinite(b.open) && std::isfinite(b.high) && std::isfinite(b.low) &&
           std::isfinite(b.close);
}

} // namespace

// ── Construction ─────────────────────────────────────────────────────────────

CandleChart::CandleChart(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this]() {
        invalidate();
        updateGeometry();
    });
}

CandleChart::~CandleChart() = default;

void CandleChart::invalidate() {
    dirty_ = true;
    update();
}

// ── Data ─────────────────────────────────────────────────────────────────────

void CandleChart::set_bars(const QVector<Bar>& bars) {
    bars_.clear();
    bars_.reserve(bars.size());
    for (const auto& b : bars)
        if (cc_bar_ok(b))
            bars_.append(b);
    if (!std::is_sorted(bars_.begin(), bars_.end(), [](const Bar& a, const Bar& b) { return a.time_ms < b.time_ms; }))
        std::stable_sort(bars_.begin(), bars_.end(), [](const Bar& a, const Bar& b) { return a.time_ms < b.time_ms; });
    cross_bar_ = -1;
    reset_view();
}

void CandleChart::set_candles(const QVector<CandleData>& candles, TimeUnit unit) {
    bool secs = unit == TimeUnit::Seconds;
    if (unit == TimeUnit::Auto) {
        long long max_abs = 0;
        for (const auto& c : candles)
            max_abs = std::max<long long>(max_abs, std::llabs(static_cast<long long>(c.timestamp)));
        secs = max_abs < 100000000000LL; // 1e11 s is year 5138; 1e11 ms is 1973
    }
    const double mult = secs ? 1000.0 : 1.0;
    QVector<Bar> out;
    out.reserve(candles.size());
    for (const auto& c : candles)
        out.append({static_cast<double>(c.timestamp) * mult, c.open, c.high, c.low, c.close, c.volume});
    set_bars(out);
}

void CandleChart::update_bar(const Bar& bar) {
    if (!cc_bar_ok(bar))
        return;
    if (bars_.isEmpty() || bar.time_ms > bars_.last().time_ms) {
        bars_.append(bar);
        if (!follow_)
            invalidate();
        else
            set_view(0.0, view_count_, true, true);
        return;
    }
    if (bar.time_ms == bars_.last().time_ms) {
        bars_.last() = bar;
        invalidate();
    }
}

void CandleChart::clear() {
    bars_.clear();
    overlays_.clear();
    annotations_.clear();
    bands_.clear();
    next_palette_slot_ = 0;
    cross_bar_ = -1;
    hover_marker_ = -1;
    reset_view();
}

int CandleChart::bar_count() const {
    return static_cast<int>(bars_.size());
}

const QVector<CandleChart::Bar>& CandleChart::bars() const {
    return bars_;
}

// ── Overlays ─────────────────────────────────────────────────────────────────

int CandleChart::add_overlay_series(const QString& name, const QVector<double>& values, ColorRole role, qreal width,
                                    bool dashed) {
    Overlay o;
    o.id = next_overlay_id_++;
    o.name = name;
    o.values = values;
    o.role = role;
    o.width = width;
    o.dashed = dashed;
    if (role == ColorRole::Auto || role == ColorRole::Palette)
        o.palette_slot = next_palette_slot_++;
    overlays_.append(o);
    invalidate();
    return o.id;
}

void CandleChart::set_overlay_data(int id, const QVector<double>& values) {
    for (auto& o : overlays_) {
        if (o.id == id) {
            o.values = values;
            invalidate();
            return;
        }
    }
}

void CandleChart::set_overlay_visible(int id, bool visible) {
    for (auto& o : overlays_) {
        if (o.id == id) {
            o.visible = visible;
            invalidate();
            return;
        }
    }
}

void CandleChart::remove_overlay(int id) {
    overlays_.removeIf([id](const Overlay& o) { return o.id == id; });
    invalidate();
}

void CandleChart::clear_overlays() {
    overlays_.clear();
    next_palette_slot_ = 0;
    invalidate();
}

// ── Annotations ──────────────────────────────────────────────────────────────

int CandleChart::add_trade_marker(double time_ms, double price, TradeSide side, const QString& label,
                                  const QString& tooltip) {
    Annotation a;
    a.id = next_annotation_id_++;
    a.is_marker = true;
    a.time_ms = time_ms;
    a.price = price;
    a.side = side;
    a.label = label;
    a.tooltip = tooltip;
    annotations_.append(a);
    invalidate();
    return a.id;
}

int CandleChart::add_hline(double price, const QString& label, ColorRole role, bool dashed) {
    Annotation a;
    a.id = next_annotation_id_++;
    a.is_marker = false;
    a.price = price;
    a.role = role;
    a.dashed = dashed;
    a.label = label;
    annotations_.append(a);
    invalidate();
    return a.id;
}

int CandleChart::add_band(double from_ms, double to_ms, ColorRole role, const QString& label) {
    if (!std::isfinite(from_ms) || !std::isfinite(to_ms))
        return -1;
    Band b;
    b.id = next_annotation_id_++;
    b.from_ms = std::min(from_ms, to_ms);
    b.to_ms = std::max(from_ms, to_ms);
    b.role = role;
    b.label = label;
    bands_.append(b);
    invalidate();
    return b.id;
}

void CandleChart::clear_bands() {
    if (bands_.isEmpty())
        return;
    bands_.clear();
    invalidate();
}

int CandleChart::band_count() const {
    return static_cast<int>(bands_.size());
}

void CandleChart::remove_annotation(int id) {
    annotations_.removeIf([id](const Annotation& a) { return a.id == id; });
    bands_.removeIf([id](const Band& b) { return b.id == id; });
    if (hover_marker_ == id)
        hover_marker_ = -1;
    invalidate();
}

void CandleChart::clear_annotations() {
    annotations_.clear();
    hover_marker_ = -1;
    invalidate();
}

// ── View ─────────────────────────────────────────────────────────────────────

double CandleChart::view_start() const {
    if (follow_)
        return static_cast<double>(bars_.size()) + kCcRightMargin - view_count_;
    return view_start_;
}

void CandleChart::set_view(double start, double count, bool follow, bool notify) {
    const auto v = chart_ranges::clamp_view(start, count, follow, static_cast<double>(bars_.size()), kCcMinSlots,
                                            kCcRightMargin);
    view_count_ = v.count;
    if (!follow)
        view_start_ = v.start;
    follow_ = v.follow;
    invalidate();
    if (notify)
        notify_range();
}

void CandleChart::notify_range() {
    if (bars_.isEmpty())
        return;
    const double start = view_start();
    const int n = static_cast<int>(bars_.size());
    const int i0 = std::clamp(static_cast<int>(std::floor(start)), 0, n - 1);
    const int i1 = std::clamp(static_cast<int>(std::ceil(start + view_count_)) - 1, 0, n - 1);
    const double from = bars_[i0].time_ms;
    const double to = bars_[i1].time_ms;
    if (from == last_from_ms_ && to == last_to_ms_)
        return;
    last_from_ms_ = from;
    last_to_ms_ = to;
    emit visible_range_changed(from, to);
}

void CandleChart::set_default_visible_bars(int count) {
    default_count_ = std::max(static_cast<int>(kCcMinSlots), count);
}

void CandleChart::reset_view() {
    const double n = static_cast<double>(bars_.size());
    set_view(0.0, std::min(static_cast<double>(default_count_), n + kCcRightMargin), true, true);
}

bool CandleChart::set_visible_range(double from_ms, double to_ms) {
    const auto span = chart_ranges::view_bars(static_cast<int>(bars_.size()), from_ms, to_ms,
                                              [this](int i) { return bars_[i].time_ms; });
    if (span.empty())
        return false;
    const auto w = chart_ranges::window_for(span, kCcMinSlots);
    set_view(w.start, w.count, false, true);
    return true;
}

void CandleChart::set_show_volume(bool on) {
    show_volume_ = on;
    invalidate();
}

void CandleChart::set_volume_ratio(double ratio) {
    volume_ratio_ = std::clamp(ratio, 0.05, 0.6);
    invalidate();
}

void CandleChart::set_log_scale(bool on) {
    if (log_scale_ == on)
        return;
    log_scale_ = on;
    invalidate();
    emit log_scale_changed(on);
}

bool CandleChart::log_scale() const {
    return log_scale_;
}

void CandleChart::set_show_last_price(bool on) {
    show_last_price_ = on;
    invalidate();
}

void CandleChart::set_title(const QString& title) {
    title_ = title;
    update();
}

void CandleChart::set_price_formatter(ValueFormatter fn) {
    price_fmt_ = std::move(fn);
    invalidate();
}

void CandleChart::set_volume_formatter(ValueFormatter fn) {
    volume_fmt_ = std::move(fn);
    invalidate();
}

void CandleChart::set_time_formatter(TimeFormatter fn) {
    time_fmt_ = std::move(fn);
    update();
}

void CandleChart::set_utc(bool on) {
    utc_ = on;
    invalidate();
}

void CandleChart::set_state(State state, const QString& message) {
    state_ = state;
    state_message_ = message;
    invalidate();
}

CandleChart::State CandleChart::state() const {
    return state_;
}

void CandleChart::set_export_name(const QString& name) {
    export_name_ = name;
}

void CandleChart::set_context_menu_extra(std::function<void(QMenu*)> fn) {
    menu_extra_ = std::move(fn);
}

// ── Mapping / formatting ─────────────────────────────────────────────────────

double CandleChart::bar_x(int i) const {
    return layout_.price.left() + (i - view_start() + 0.5) * layout_.slot_w;
}

int CandleChart::index_at(int px) const {
    if (layout_.slot_w <= 0.0)
        return -1;
    return static_cast<int>(std::floor((px - layout_.price.left()) / layout_.slot_w + view_start()));
}

double CandleChart::py(const QRect& r, const Scale& s, double v) const {
    if (!std::isfinite(v))
        return kCcNaN;
    double t;
    if (s.log) {
        if (v <= 0.0)
            return kCcNaN;
        const double l0 = std::log10(s.lo);
        const double l1 = std::log10(s.hi);
        t = l1 > l0 ? (std::log10(v) - l0) / (l1 - l0) : 0.5;
    } else {
        t = s.hi > s.lo ? (v - s.lo) / (s.hi - s.lo) : 0.5;
    }
    return r.top() + (1.0 - t) * r.height();
}

double CandleChart::value_at(const QRect& r, const Scale& s, double y) const {
    const double t = r.height() > 0 ? 1.0 - (y - r.top()) / r.height() : 0.5;
    if (s.log) {
        const double l0 = std::log10(s.lo);
        const double l1 = std::log10(s.hi);
        return std::pow(10.0, l0 + t * (l1 - l0));
    }
    return s.lo + t * (s.hi - s.lo);
}

QString CandleChart::fmt_price(double v) const {
    if (price_fmt_)
        return price_fmt_(v);
    const double ref = bars_.isEmpty() ? v : bars_.last().close;
    return chartkit::format_number(v, chartkit::auto_decimals(std::abs(ref) >= 1e5 ? 1.0 : ref));
}

QString CandleChart::fmt_volume(double v) const {
    if (volume_fmt_)
        return volume_fmt_(v);
    return std::abs(v) >= 1e3 ? chartkit::format_compact(v, 2) : chartkit::format_number(v, 0);
}

QString CandleChart::fmt_time(double ms) const {
    if (time_fmt_)
        return time_fmt_(ms);
    return chart_ticks::time_point_label(ms, layout_.spacing_ms, utc_);
}

int CandleChart::bar_at_time(double ms) const {
    const auto it = std::upper_bound(bars_.begin(), bars_.end(), ms,
                                     [](double t, const Bar& b) { return t < b.time_ms; });
    return static_cast<int>(it - bars_.begin()) - 1;
}

// ── Layout ───────────────────────────────────────────────────────────────────

void CandleChart::compute_layout() {
    Layout L;
    const int W = width();
    const int H = height();
    if (W <= 0 || H <= 0) {
        layout_ = L;
        return;
    }
    const QFont af = chartkit::axis_font();
    const QFontMetrics fm(af);
    const QFontMetrics tfm(chartkit::tooltip_font(true));
    const int gap = std::max(4, fm.averageCharWidth());
    const int n = static_cast<int>(bars_.size());

    L.header_h = tfm.height() + gap;
    L.x_axis_h = fm.height() + gap;
    const int plot_h = std::max(10, H - L.header_h - L.x_axis_h);
    bool any_volume = false;
    for (const auto& b : bars_)
        if (b.volume > 0.0) {
            any_volume = true;
            break;
        }
    const bool vol = show_volume_ && any_volume;
    const int vol_h = vol ? static_cast<int>(plot_h * volume_ratio_) : 0;
    const int price_h = std::max(10, plot_h - (vol ? vol_h + gap : 0));

    const double start = view_start();
    if (n > 0) {
        L.i_first = std::clamp(static_cast<int>(std::floor(start)), 0, n - 1);
        L.i_last = std::clamp(static_cast<int>(std::ceil(start + view_count_)) - 1, 0, n - 1);
        if (start + view_count_ <= 0.0 || start >= n)
            L.i_last = L.i_first - 1; // nothing on screen
    }

    // Price scale over the visible bars and overlays.
    double lo = std::numeric_limits<double>::infinity();
    double hi = -lo;
    double min_pos = lo;
    double vmax = 0.0;
    for (int i = L.i_first; i <= L.i_last; ++i) {
        const auto& b = bars_[i];
        lo = std::min(lo, b.low);
        hi = std::max(hi, b.high);
        if (b.low > 0.0)
            min_pos = std::min(min_pos, b.low);
        vmax = std::max(vmax, b.volume);
    }
    for (const auto& o : overlays_) {
        if (!o.visible)
            continue;
        const int last = std::min(L.i_last, static_cast<int>(o.values.size()) - 1);
        for (int i = L.i_first; i <= last; ++i) {
            const double v = o.values[i];
            if (!std::isfinite(v))
                continue;
            lo = std::min(lo, v);
            hi = std::max(hi, v);
            if (v > 0.0)
                min_pos = std::min(min_pos, v);
        }
    }
    Scale ps;
    if (!(lo <= hi)) {
        lo = 0.0;
        hi = 1.0;
    }
    ps.log = log_scale_ && hi > 0.0 && std::isfinite(min_pos);
    if (ps.log) {
        double ll = std::log10(min_pos);
        double lh = std::log10(hi);
        double span = lh - ll;
        if (span < 1e-6)
            span = 0.02;
        ps.lo = std::pow(10.0, ll - span * 0.08);
        ps.hi = std::pow(10.0, lh + span * 0.08);
    } else {
        double span = hi - lo;
        if (span <= 0.0)
            span = std::abs(hi) > 0.0 ? std::abs(hi) * 0.02 : 1.0;
        ps.lo = lo - span * 0.08;
        ps.hi = hi + span * 0.08;
    }
    const int max_ticks = std::max(2, price_h / std::max(1, fm.height() * 2));
    const auto pt = ps.log ? chart_ticks::log_ticks(ps.lo, ps.hi, max_ticks)
                           : chart_ticks::nice_ticks(ps.lo, ps.hi, max_ticks);
    ps.ticks = pt.values;
    ps.step = pt.step;

    Scale vs;
    vs.lo = 0.0;
    vs.hi = vmax > 0.0 ? vmax * 1.1 : 1.0;

    // Right axis wide enough for tick labels and the price tags.
    int aw = 0;
    for (double t : ps.ticks)
        aw = std::max(aw, fm.horizontalAdvance(price_fmt_ ? price_fmt_(t) : chartkit::format_tick(t, ps.step, std::max(std::abs(ps.lo), std::abs(ps.hi)))));
    if (n > 0)
        aw = std::max(aw, tfm.horizontalAdvance(fmt_price(bars_.last().close)));
    if (vol)
        aw = std::max(aw, fm.horizontalAdvance(fmt_volume(vs.hi)));
    L.axis_w = std::min(W / 3, aw + gap * 2);

    const int left = gap / 2;
    const int pw = std::max(1, W - L.axis_w - left);
    L.price = QRect(left, L.header_h, pw, price_h);
    L.volume = vol ? QRect(left, L.price.bottom() + 1 + gap, pw, std::max(1, vol_h)) : QRect();
    L.plot = vol ? L.price.united(L.volume) : L.price;
    L.pscale = ps;
    L.vscale = vs;
    L.slot_w = pw / std::max(1.0, view_count_);
    L.spacing_ms = n >= 2 ? (bars_.last().time_ms - bars_.first().time_ms) / (n - 1) : 0.0;

    // X labels: calendar ticks snapped to the first bar at or after each tick.
    if (L.i_first <= L.i_last) {
        const double t0 = bars_[L.i_first].time_ms;
        const double t1 = bars_[L.i_last].time_ms;
        if (t1 > t0) {
            auto tt = chart_ticks::time_ticks(t0, t1, std::max(2, pw / (fm.averageCharWidth() * 10)), utc_);
            const int lw = fm.horizontalAdvance(chart_ticks::time_label_sample(tt.step)) + gap * 3;
            tt = chart_ticks::time_ticks(t0, t1, std::max(2, pw / std::max(1, lw)), utc_);
            int prev = -1;
            for (double t : tt.values) {
                const auto it = std::lower_bound(bars_.begin() + L.i_first, bars_.begin() + L.i_last + 1, t,
                                                 [](const Bar& b, double v) { return b.time_ms < v; });
                const int i = static_cast<int>(it - bars_.begin());
                if (i > L.i_last || i == prev)
                    continue;
                prev = i;
                L.x_labels.append({i, chart_ticks::time_tick_label(bars_[i].time_ms, tt.step, utc_)});
            }
        } else {
            L.x_labels.append({L.i_first, fmt_time(t0)});
        }
    }
    L.valid = true;
    layout_ = L;
}

// ── Static layer ─────────────────────────────────────────────────────────────

void CandleChart::rebuild_cache() {
    compute_layout();
    cache_ = chartkit::make_layer(this, chartkit::background());
    dirty_ = false;
    layout_.marker_hits.clear();
    if (cache_.isNull() || !layout_.valid)
        return;
    QPainter p(&cache_);
    const auto& L = layout_;

    const bool blocking = state_ == State::Empty || state_ == State::Error || bars_.isEmpty();
    if (blocking) {
        p.setPen(QPen(chartkit::grid(), 1));
        p.drawLine(L.plot.left(), L.plot.bottom(), L.plot.right(), L.plot.bottom());
        chartkit::draw_state_message(p, L.plot, state_ == State::Ready ? State::Empty : state_, state_message_);
        return;
    }

    paint_bands(p, false); // shading behind the grid and the candles

    // Grid.
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(chartkit::grid(), 1));
    for (double t : L.pscale.ticks) {
        const double y = py(L.price, L.pscale, t);
        if (std::isfinite(y) && y >= L.price.top() && y <= L.price.bottom())
            p.drawLine(L.price.left(), static_cast<int>(std::lround(y)), L.price.right(), static_cast<int>(std::lround(y)));
    }
    for (const auto& xl : L.x_labels) {
        const int X = static_cast<int>(std::lround(bar_x(xl.first)));
        if (X < L.plot.left() || X > L.plot.right())
            continue;
        p.drawLine(X, L.price.top(), X, L.price.bottom());
        if (!L.volume.isEmpty())
            p.drawLine(X, L.volume.top(), X, L.volume.bottom());
    }

    paint_volume(p);
    paint_candles(p);
    paint_overlays(p);
    paint_annotations(p);
    paint_bands(p, true); // band labels over the candles
    paint_axes(p);

    if (state_ == State::Loading) {
        const QString note = state_message_.isEmpty() ? chartkit::ChartKitText::tr("Loading…") : state_message_;
        const QFontMetrics fm(chartkit::tooltip_font(true));
        const int pad = fm.averageCharWidth();
        const int tw = fm.horizontalAdvance(note) + 2 * pad;
        chartkit::draw_axis_tag(p, QRectF(L.price.right() - tw - pad, L.price.top() + pad, tw, fm.height() + 2), note,
                                chartkit::raised());
    }
}

void CandleChart::paint_candles(QPainter& p) {
    const auto& L = layout_;
    if (L.i_first > L.i_last)
        return;
    // Bodies and wicks are collected per colour and filled in two path fills
    // (chartkit::fill_rects) — per-bar fillRect / drawLine is far slower.
    QVector<QRectF> rects_up;
    QVector<QRectF> rects_dn;
    const int n_vis = L.i_last - L.i_first + 1;
    rects_up.reserve(n_vis);
    rects_dn.reserve(n_vis);

    if (L.slot_w >= 1.5) {
        int bw = std::max(1, static_cast<int>(std::floor(L.slot_w * 0.7)));
        if (bw > 2 && bw % 2 == 0)
            --bw; // odd body width keeps the wick centred
        for (int i = L.i_first; i <= L.i_last; ++i) {
            const auto& b = bars_[i];
            const double yo = py(L.price, L.pscale, b.open);
            const double yc = py(L.price, L.pscale, b.close);
            const double yh = py(L.price, L.pscale, b.high);
            const double yl = py(L.price, L.pscale, b.low);
            if (!std::isfinite(yo) || !std::isfinite(yc) || !std::isfinite(yh) || !std::isfinite(yl))
                continue;
            auto& out = b.close >= b.open ? rects_up : rects_dn;
            const int cx = static_cast<int>(std::floor(bar_x(i)));
            out.append(QRectF(cx, std::floor(yh), 1.0, std::max(1.0, std::round(yl - yh))));
            const double top = std::min(yo, yc);
            const double h = std::max(1.0, std::round(std::abs(yo - yc)));
            out.append(QRectF(cx - (bw - 1) / 2, std::floor(top), bw, h));
        }
    } else {
        // Dense: one high-low span per pixel column, coloured by the column's net move.
        int col = INT_MIN;
        double c_hi = 0, c_lo = 0, c_open = 0, c_close = 0;
        auto flush = [&]() {
            if (col == INT_MIN)
                return;
            const double yh = py(L.price, L.pscale, c_hi);
            const double yl = py(L.price, L.pscale, c_lo);
            if (std::isfinite(yh) && std::isfinite(yl))
                (c_close >= c_open ? rects_up : rects_dn).append(QRectF(col, yh, 1.0, std::max(1.0, yl - yh)));
        };
        for (int i = L.i_first; i <= L.i_last; ++i) {
            const auto& b = bars_[i];
            const int x = static_cast<int>(std::floor(bar_x(i)));
            if (x != col) {
                flush();
                col = x;
                c_hi = b.high;
                c_lo = b.low;
                c_open = b.open;
                c_close = b.close;
            } else {
                c_hi = std::max(c_hi, b.high);
                c_lo = std::min(c_lo, b.low);
                c_close = b.close;
            }
        }
        flush();
    }
    p.save();
    p.setClipRect(L.price);
    chartkit::fill_rects(p, rects_up, chartkit::up());
    chartkit::fill_rects(p, rects_dn, chartkit::down());
    p.restore();
}

void CandleChart::paint_volume(QPainter& p) {
    const auto& L = layout_;
    if (L.volume.isEmpty() || L.i_first > L.i_last)
        return;
    const QRect r = L.volume;
    const double base = r.top() + r.height();
    QVector<QRectF> rects_up;
    QVector<QRectF> rects_dn;
    if (L.slot_w >= 1.5) {
        int bw = std::max(1, static_cast<int>(std::floor(L.slot_w * 0.7)));
        if (bw > 2 && bw % 2 == 0)
            --bw;
        for (int i = L.i_first; i <= L.i_last; ++i) {
            const auto& b = bars_[i];
            if (!(b.volume > 0.0))
                continue;
            const double y = py(r, L.vscale, b.volume);
            const int cx = static_cast<int>(std::floor(bar_x(i)));
            (b.close >= b.open ? rects_up : rects_dn).append(QRectF(cx - (bw - 1) / 2, y, bw, base - y));
        }
    } else {
        int col = INT_MIN;
        double c_vol = 0, c_open = 0, c_close = 0;
        auto flush = [&]() {
            if (col == INT_MIN || !(c_vol > 0.0))
                return;
            const double y = py(r, L.vscale, c_vol);
            (c_close >= c_open ? rects_up : rects_dn).append(QRectF(col, y, 1.0, base - y));
        };
        for (int i = L.i_first; i <= L.i_last; ++i) {
            const auto& b = bars_[i];
            const int x = static_cast<int>(std::floor(bar_x(i)));
            if (x != col) {
                flush();
                col = x;
                c_vol = b.volume;
                c_open = b.open;
                c_close = b.close;
            } else {
                c_vol = std::max(c_vol, b.volume);
                c_close = b.close;
            }
        }
        flush();
    }
    p.save();
    p.setClipRect(r);
    chartkit::fill_rects(p, rects_up, chartkit::with_alpha(chartkit::up(), 0.45));
    chartkit::fill_rects(p, rects_dn, chartkit::with_alpha(chartkit::down(), 0.45));
    p.restore();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(chartkit::grid(), 1));
    p.drawLine(r.left(), r.top() - 1, r.right(), r.top() - 1);
}
void CandleChart::paint_overlays(QPainter& p) {
    const auto& L = layout_;
    if (overlays_.isEmpty() || L.i_first > L.i_last)
        return;
    p.save();
    p.setClipRect(L.price);
    p.setRenderHint(QPainter::Antialiasing, true);
    const int n = static_cast<int>(bars_.size());
    QVector<double> xs; // bar index as x, for the shared decimator
    QVector<int> idx;
    for (const auto& o : overlays_) {
        if (!o.visible || o.values.isEmpty())
            continue;
        const int i0 = std::max(0, L.i_first - 1);
        const int i1 = std::min({n, static_cast<int>(o.values.size()), L.i_last + 2});
        if (i0 >= i1)
            continue;
        if (xs.size() < i1) {
            xs.resize(i1);
            for (int i = 0; i < i1; ++i)
                xs[i] = i;
        }
        idx.clear();
        chartkit::decimate_minmax(xs.constData(), o.values.constData(), i0, i1, view_start(),
                                  std::max(1e-9, L.slot_w), idx);
        const QColor c = chartkit::role_color(o.role, o.palette_slot);
        QPen pen(c, std::max<qreal>(0.5, o.width), o.dashed ? Qt::DashLine : Qt::SolidLine,
                 o.dashed ? Qt::FlatCap : Qt::RoundCap, Qt::RoundJoin);
        QPolygonF seg;
        auto flush = [&]() {
            if (seg.size() >= 2)
                chartkit::draw_series_line(p, seg, pen);
            seg.clear();
        };
        for (int i : idx) {
            const double y = py(L.price, L.pscale, o.values[i]);
            if (!std::isfinite(y)) {
                flush();
                continue;
            }
            seg << QPointF(bar_x(i), y);
        }
        flush();
    }
    p.restore();
}

void CandleChart::paint_annotations(QPainter& p) {
    auto& L = layout_;
    const QFont af = chartkit::axis_font();
    const QFontMetrics fm(af);
    const int gap = std::max(4, fm.averageCharWidth());
    const QRect r = L.price;

    for (const auto& a : annotations_) {
        if (a.is_marker)
            continue;
        const double y = py(r, L.pscale, a.price);
        if (!std::isfinite(y) || y < r.top() || y > r.bottom() + 1)
            continue;
        const QColor c = chartkit::role_color(a.role == ChartColorRole::Auto ? ChartColorRole::Accent : a.role);
        const int yi = std::min(r.bottom(), static_cast<int>(std::lround(y)));
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(c, 1, a.dashed ? Qt::DashLine : Qt::SolidLine));
        p.drawLine(r.left(), yi, r.right(), yi);
        if (!a.label.isEmpty()) {
            p.setFont(af);
            const int ly = yi - fm.height() - 1 < r.top() ? yi + 1 : yi - fm.height() - 1;
            p.drawText(QRect(r.left() + gap, ly, r.width() - gap * 2, fm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       a.label);
        }
        const QFontMetrics tfm(chartkit::tooltip_font(true));
        const QString text = fmt_price(a.price);
        chartkit::draw_axis_tag(p, QRectF(r.right() + 1, yi - (tfm.height() + 2) / 2.0, L.axis_w - 1, tfm.height() + 2),
                                text, c);
    }

    if (L.i_first > L.i_last)
        return;
    const qreal size = fm.height() * 0.95;
    p.save();
    p.setClipRect(r.adjusted(0, -1, 1, 1));
    for (const auto& a : annotations_) {
        if (!a.is_marker)
            continue;
        const int i = bar_at_time(a.time_ms);
        if (i < L.i_first || i > L.i_last)
            continue;
        const double y = py(r, L.pscale, a.price);
        if (!std::isfinite(y))
            continue;
        const double X = bar_x(i);
        const ChartMarkerKind kind = a.side == TradeSide::Buy ? ChartMarkerKind::Buy : ChartMarkerKind::Sell;
        const QColor c = chartkit::marker_color(kind);
        const QRectF hit = chartkit::draw_marker(p, QPointF(X, y), kind, c, size);
        L.marker_hits.append({hit.toAlignedRect(), a.id});
        if (!a.label.isEmpty()) {
            p.setFont(af);
            p.setPen(c);
            const int tw = fm.horizontalAdvance(a.label);
            const QRectF lr = kind == ChartMarkerKind::Buy ? QRectF(X - tw / 2.0, hit.bottom(), tw, fm.height())
                                                           : QRectF(X - tw / 2.0, hit.top() - fm.height(), tw, fm.height());
            p.drawText(lr, Qt::AlignCenter, a.label);
        }
    }
    p.restore();
}

void CandleChart::paint_bands(QPainter& p, bool labels) {
    const auto& L = layout_;
    if (bands_.isEmpty() || L.i_first > L.i_last || L.slot_w <= 0.0)
        return;
    const int n = static_cast<int>(bars_.size());
    const auto time_at = [this](int i) { return bars_[i].time_ms; };
    const QFontMetrics afm(chartkit::axis_font());
    const QFontMetrics tfm(chartkit::tooltip_font(true)); // draw_axis_tag's font
    const int gap = std::max(4, afm.averageCharWidth());
    // Label tags sit under the overlay legend (paint_overlay_layer draws it top-left of the price pane).
    int legend_lines = 0;
    for (const auto& o : overlays_)
        if (o.visible)
            ++legend_lines;
    const int tag_h = tfm.height() + 2;
    const int tag_y = L.price.top() + gap / 2 + legend_lines * afm.height() + (legend_lines > 0 ? gap / 2 : 0);
    const double start = view_start();

    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    for (const auto& b : bands_) {
        const auto span = chart_ranges::band_bars(n, b.from_ms, b.to_ms, time_at);
        if (span.empty() || span.last < L.i_first || span.first > L.i_last)
            continue;
        double x0 = L.price.left() + (span.first - start) * L.slot_w;
        double x1 = L.price.left() + (span.last + 1 - start) * L.slot_w;
        if (x1 - x0 < 2.0) { // dense zoom: keep a one-bar band visible
            const double mid = (x0 + x1) / 2.0;
            x0 = mid - 1.0;
            x1 = mid + 1.0;
        }
        const QColor c = chartkit::role_color(b.role == ColorRole::Auto ? ColorRole::Accent : b.role);
        if (!labels) {
            const int e0 = static_cast<int>(std::floor(x0));
            const int e1 = static_cast<int>(std::ceil(x1)) - 1;
            for (const QRect& pane : {L.price, L.volume}) {
                if (pane.isEmpty())
                    continue;
                const QRectF area = QRectF(x0, pane.top(), x1 - x0, pane.height()).intersected(QRectF(pane));
                if (area.isEmpty())
                    continue;
                p.fillRect(area, chartkit::with_alpha(c, 0.14));
                // 1 px edges: the band reads by shape, not only by its tint.
                p.setPen(QPen(chartkit::with_alpha(c, 0.6), 1));
                if (e0 >= pane.left() && e0 <= pane.right())
                    p.drawLine(e0, pane.top(), e0, pane.bottom());
                if (e1 > e0 && e1 >= pane.left() && e1 <= pane.right())
                    p.drawLine(e1, pane.top(), e1, pane.bottom());
            }
            continue;
        }
        if (b.label.isEmpty() || tag_y + tag_h > L.price.bottom())
            continue;
        const double vx0 = std::max(x0, static_cast<double>(L.price.left()));
        const double vx1 = std::min(x1, static_cast<double>(L.price.right() + 1));
        const int pad = tfm.averageCharWidth();
        const int max_w = std::min(L.price.width(), std::max(static_cast<int>(vx1 - vx0), pad * 12));
        const QString shown = tfm.elidedText(b.label, Qt::ElideRight, std::max(0, max_w - 2 * pad));
        if (shown.isEmpty())
            continue;
        const int tw = tfm.horizontalAdvance(shown) + 2 * pad;
        const double tx = std::clamp((vx0 + vx1) / 2.0 - tw / 2.0, static_cast<double>(L.price.left()),
                                     static_cast<double>(std::max(L.price.left(), L.price.right() + 1 - tw)));
        chartkit::draw_axis_tag(p, QRectF(tx, tag_y, tw, tag_h), shown, c);
    }
    p.restore();
}

void CandleChart::paint_axes(QPainter& p) {
    const auto& L = layout_;
    const QFont af = chartkit::axis_font();
    const QFontMetrics fm(af);
    const int gap = std::max(4, fm.averageCharWidth());
    const int W = width();

    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(chartkit::axis_line(), 1));
    p.drawLine(L.price.right() + 1, L.price.top(), L.price.right() + 1, L.plot.bottom());
    p.drawLine(L.plot.left(), L.plot.bottom() + 1, L.price.right() + 1, L.plot.bottom() + 1);

    // Price labels.
    p.setFont(af);
    p.setPen(chartkit::axis_text());
    const double max_abs = std::max(std::abs(L.pscale.lo), std::abs(L.pscale.hi));
    for (double t : L.pscale.ticks) {
        const double y = py(L.price, L.pscale, t);
        if (!std::isfinite(y) || y < L.price.top() || y > L.price.bottom())
            continue;
        const QString text = price_fmt_ ? price_fmt_(t) : chartkit::format_tick(t, L.pscale.step, max_abs);
        p.drawText(QRect(L.price.right() + gap, static_cast<int>(std::lround(y)) - fm.height() / 2, L.axis_w - gap,
                         fm.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, text);
    }
    if (!L.volume.isEmpty()) {
        p.drawText(QRect(L.volume.right() + gap, L.volume.top(), L.axis_w - gap, fm.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, fmt_volume(L.vscale.hi / 1.1));
    }

    // Time labels.
    int last_right = INT_MIN / 2;
    const int y0 = L.plot.bottom() + 1;
    for (const auto& xl : L.x_labels) {
        const int X = static_cast<int>(std::lround(bar_x(xl.first)));
        if (X < L.plot.left() || X > L.plot.right())
            continue;
        const int tw = fm.horizontalAdvance(xl.second);
        const int lx = std::clamp(X - tw / 2, 0, std::max(0, W - tw));
        p.setPen(QPen(chartkit::axis_line(), 1));
        p.drawLine(X, y0, X, y0 + gap / 2);
        if (lx < last_right + gap)
            continue;
        p.setPen(chartkit::axis_text());
        p.drawText(lx, y0 + gap / 2 + fm.ascent(), xl.second);
        last_right = lx + tw;
    }

    // Last price line + tag.
    if (show_last_price_ && !bars_.isEmpty()) {
        const auto& b = bars_.last();
        const double y = py(L.price, L.pscale, b.close);
        if (std::isfinite(y) && y >= L.price.top() && y <= L.price.bottom() + 1) {
            const QColor c = b.close >= b.open ? chartkit::up() : chartkit::down();
            const int yi = std::min(L.price.bottom(), static_cast<int>(std::lround(y)));
            p.setPen(QPen(chartkit::with_alpha(c, 0.7), 1, Qt::DotLine));
            p.drawLine(L.price.left(), yi, L.price.right(), yi);
            const QFontMetrics tfm(chartkit::tooltip_font(true));
            chartkit::draw_axis_tag(
                p, QRectF(L.price.right() + 1, yi - (tfm.height() + 2) / 2.0, L.axis_w - 1, tfm.height() + 2),
                fmt_price(b.close), c);
        }
    }
}

// ── Dynamic layer ────────────────────────────────────────────────────────────

void CandleChart::paintEvent(QPaintEvent*) {
    if (dirty_ || !chartkit::layer_valid(cache_, this))
        rebuild_cache();
    QPainter p(this);
    if (!cache_.isNull())
        p.drawPixmap(0, 0, cache_);
    paint_overlay_layer(p);
}

void CandleChart::paint_header(QPainter& p, int bar) {
    const auto& L = layout_;
    const QFont tf = chartkit::tooltip_font(true);
    const QFont nf = chartkit::tooltip_font(false);
    const QFontMetrics tfm(tf);
    const QFontMetrics nfm(nf);
    const int gap = std::max(4, nfm.averageCharWidth());
    int x = gap / 2;
    const int base = (L.header_h + tfm.ascent() - tfm.descent()) / 2;
    const int right_limit = width() - gap;

    auto put = [&](const QString& s, const QFont& f, const QColor& c) {
        if (x >= right_limit)
            return;
        p.setFont(f);
        p.setPen(c);
        const QFontMetrics m(f);
        const QString shown = m.elidedText(s, Qt::ElideRight, right_limit - x);
        p.drawText(x, base, shown);
        x += m.horizontalAdvance(shown);
    };

    if (!title_.isEmpty()) {
        put(title_, tf, chartkit::text());
        x += gap * 2;
    }
    if (bar < 0 || bar >= bars_.size())
        return;
    const auto& b = bars_[bar];
    const QColor vc = b.close >= b.open ? chartkit::up() : chartkit::down();
    auto field = [&](const QString& label, const QString& value, const QColor& c) {
        put(label + QLatin1Char(' '), nf, chartkit::axis_text());
        put(value, tf, c);
        x += gap;
    };
    field(tr("O", "open"), fmt_price(b.open), vc);
    field(tr("H", "high"), fmt_price(b.high), vc);
    field(tr("L", "low"), fmt_price(b.low), vc);
    field(tr("C", "close"), fmt_price(b.close), vc);
    if (b.volume > 0.0)
        field(tr("V", "volume"), fmt_volume(b.volume), chartkit::text());
    if (bar > 0 && bars_[bar - 1].close != 0.0) {
        const double prev = bars_[bar - 1].close;
        const double chg = (b.close - prev) / std::abs(prev) * 100.0;
        put(chartkit::format_signed_percent(chg, 2), tf, chg >= 0.0 ? chartkit::up() : chartkit::down());
        x += gap;
    }
    if (bar != bars_.size() - 1 || mouse_in_)
        put(QStringLiteral("  ") + fmt_time(b.time_ms), nf, chartkit::text_dim());
}

void CandleChart::paint_overlay_layer(QPainter& p) {
    const auto& L = layout_;
    if (!L.valid)
        return;
    const bool blocked = state_ == State::Empty || state_ == State::Error || bars_.isEmpty();
    const int shown_bar = (mouse_in_ && cross_bar_ >= 0) ? cross_bar_ : static_cast<int>(bars_.size()) - 1;
    paint_header(p, blocked ? -1 : shown_bar);
    if (blocked)
        return;

    const QFont af = chartkit::axis_font();
    const QFontMetrics fm(af);
    const int gap = std::max(4, fm.averageCharWidth());

    // Overlay legend with values at the shown bar (top-left of the price pane).
    {
        int y = L.price.top() + gap / 2;
        p.setFont(af);
        for (const auto& o : overlays_) {
            if (!o.visible)
                continue;
            const QColor c = chartkit::role_color(o.role, o.palette_slot);
            const double v = (shown_bar >= 0 && shown_bar < o.values.size()) ? o.values[shown_bar] : kCcNaN;
            const QString text = o.name + QStringLiteral("  ") + (std::isfinite(v) ? fmt_price(v) : QStringLiteral("—"));
            p.setPen(c);
            p.drawText(QRect(L.price.left() + gap, y, L.price.width() - gap * 2, fm.height()),
                       Qt::AlignLeft | Qt::AlignVCenter, text);
            y += fm.height();
        }
    }

    // Hovered trade marker.
    const Annotation* hovered = nullptr;
    if (mouse_in_ && hover_marker_ >= 0) {
        for (const auto& a : annotations_)
            if (a.id == hover_marker_)
                hovered = &a;
        for (const auto& mh : L.marker_hits) {
            if (mh.id == hover_marker_) {
                p.save();
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setPen(QPen(chartkit::text(), 1));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(QRectF(mh.rect).adjusted(-1, -1, 1, 1));
                p.restore();
            }
        }
    }

    // Crosshair.
    if (mouse_in_ && !dragging_ && cross_bar_ >= 0 && cross_bar_ < bars_.size()) {
        const int X = static_cast<int>(std::floor(bar_x(cross_bar_)));
        p.save();
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(chartkit::crosshair(), 1, Qt::DashLine));
        p.drawLine(X, L.price.top(), X, L.price.bottom());
        if (!L.volume.isEmpty())
            p.drawLine(X, L.volume.top(), X, L.volume.bottom());
        const QFontMetrics tfm(chartkit::tooltip_font(true));
        const int th = tfm.height() + 2;
        const QRect* pane = L.price.contains(mouse_) ? &L.price : (L.volume.contains(mouse_) ? &L.volume : nullptr);
        if (pane) {
            const int my = mouse_.y();
            p.drawLine(pane->left(), my, pane->right(), my);
            const QString text = pane == &L.price ? fmt_price(value_at(L.price, L.pscale, my))
                                                  : fmt_volume(value_at(L.volume, L.vscale, my));
            chartkit::draw_axis_tag(p, QRectF(pane->right() + 1, my - th / 2.0, L.axis_w - 1, th), text,
                                    chartkit::crosshair());
        }
        const QString xt = fmt_time(bars_[cross_bar_].time_ms);
        const int tw = tfm.horizontalAdvance(xt) + gap;
        const double tx = std::clamp(X - tw / 2.0, 0.0, std::max(0.0, static_cast<double>(width() - tw)));
        chartkit::draw_axis_tag(p, QRectF(tx, L.plot.bottom() + 2, tw, th), xt, chartkit::crosshair());
        p.restore();
    }

    if (hovered && !hovered->tooltip.isEmpty()) {
        QVector<chartkit::TooltipRow> rows;
        for (const auto& line : hovered->tooltip.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
            rows.append({QColor(), line, QString(), QColor()});
        chartkit::draw_tooltip(p, rect(), mouse_, hovered->label, rows, fmt_time(hovered->time_ms));
    }
}

// ── Interaction ──────────────────────────────────────────────────────────────

void CandleChart::mouseMoveEvent(QMouseEvent* e) {
    const QPoint pos = e->position().toPoint();
    mouse_ = pos;
    mouse_in_ = true;
    if (dragging_) {
        if (!drag_moved_ && (pos - press_pos_).manhattanLength() > 3)
            drag_moved_ = true;
        if (drag_moved_ && layout_.slot_w > 0.0) {
            const double delta = -(pos.x() - press_pos_.x()) / layout_.slot_w;
            set_view(press_start_ + delta, view_count_, false, true);
            setCursor(Qt::ClosedHandCursor);
        }
        update();
        return;
    }

    int hm = -1;
    for (int i = static_cast<int>(layout_.marker_hits.size()) - 1; i >= 0; --i) {
        if (layout_.marker_hits[i].rect.contains(pos)) {
            hm = layout_.marker_hits[i].id;
            break;
        }
    }
    hover_marker_ = hm;
    if (hm >= 0)
        setCursor(Qt::PointingHandCursor);
    else
        unsetCursor();

    const int old = cross_bar_;
    cross_bar_ = -1;
    if (layout_.valid && layout_.plot.contains(pos) && !bars_.isEmpty() && layout_.i_first <= layout_.i_last)
        cross_bar_ = std::clamp(index_at(pos.x()), layout_.i_first, layout_.i_last);
    if (cross_bar_ != old)
        emit crosshair_moved(cross_bar_);
    update();
}

void CandleChart::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && layout_.valid && layout_.plot.contains(e->position().toPoint())) {
        dragging_ = true;
        drag_moved_ = false;
        press_pos_ = e->position().toPoint();
        press_start_ = view_start();
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void CandleChart::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && dragging_) {
        const bool clicked = !drag_moved_;
        dragging_ = false;
        drag_moved_ = false;
        unsetCursor();
        if (clicked && hover_marker_ >= 0)
            emit trade_marker_clicked(hover_marker_);
        update();
        e->accept();
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void CandleChart::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && layout_.valid && layout_.plot.contains(e->position().toPoint())) {
        reset_view();
        e->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(e);
}

void CandleChart::wheelEvent(QWheelEvent* e) {
    const QPoint pos = e->position().toPoint();
    if (!layout_.valid || !layout_.plot.contains(pos) || bars_.isEmpty()) {
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
    const double new_count = view_count_ * factor;
    if (follow_) {
        set_view(0.0, new_count, true, true);
    } else {
        const double start = view_start();
        const double rel = std::clamp((pos.x() - layout_.price.left()) / std::max(1.0, double(layout_.price.width())), 0.0, 1.0);
        const double anchor = start + rel * view_count_;
        set_view(anchor - rel * new_count, new_count, false, true);
    }
    e->accept();
}

void CandleChart::leaveEvent(QEvent* e) {
    mouse_in_ = false;
    hover_marker_ = -1;
    if (cross_bar_ != -1) {
        cross_bar_ = -1;
        emit crosshair_moved(-1);
    }
    unsetCursor();
    update();
    QWidget::leaveEvent(e);
}

void CandleChart::build_context_menu(QMenu* menu) {
    ChartExport::MenuSpec spec;
    spec.suggested_name = !export_name_.isEmpty() ? export_name_ : !title_.isEmpty() ? title_ : QStringLiteral("candles");
    spec.image = [this]() { return render_image(); };
    spec.csv = [this]() {
        ChartExport::Table t;
        csv_table(t.headers, t.rows);
        return t;
    };
    spec.extra = [this](QMenu* m) {
        QAction* log = m->addAction(tr("Log Scale"));
        log->setCheckable(true);
        log->setChecked(log_scale_);
        connect(log, &QAction::toggled, this, [this](bool on) { set_log_scale(on); });
        QAction* vol = m->addAction(tr("Show Volume"));
        vol->setCheckable(true);
        vol->setChecked(show_volume_);
        connect(vol, &QAction::toggled, this, [this](bool on) { set_show_volume(on); });
        m->addSeparator();
        QAction* reset = m->addAction(tr("Reset View"));
        connect(reset, &QAction::triggered, this, [this]() { reset_view(); });
        if (menu_extra_) {
            m->addSeparator();
            menu_extra_(m);
        }
    };
    ChartExport::populate_menu(menu, this, spec);
}

void CandleChart::contextMenuEvent(QContextMenuEvent* e) {
    QMenu menu(this);
    build_context_menu(&menu);
    menu.exec(e->globalPos());
    e->accept();
}

void CandleChart::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    dirty_ = true;
}

void CandleChart::changeEvent(QEvent* e) {
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

QSize CandleChart::sizeHint() const {
    const QFontMetrics fm(chartkit::axis_font());
    return {fm.averageCharWidth() * 80, fm.height() * 24};
}

QSize CandleChart::minimumSizeHint() const {
    const QFontMetrics fm(chartkit::axis_font());
    return {fm.averageCharWidth() * 24, fm.height() * 8};
}

// ── Export ───────────────────────────────────────────────────────────────────

QPixmap CandleChart::render_image() {
    if (dirty_ || !chartkit::layer_valid(cache_, this))
        rebuild_cache();
    QPixmap out = cache_.isNull() ? QPixmap() : cache_.copy();
    if (!out.isNull()) {
        QPainter p(&out);
        paint_header(p, static_cast<int>(bars_.size()) - 1);
    }
    return out;
}

void CandleChart::csv_table(QStringList& headers, QVector<QStringList>& rows) const {
    headers = {tr("Time"), tr("Open"), tr("High"), tr("Low"), tr("Close"), tr("Volume")};
    for (const auto& o : overlays_)
        headers << o.name;
    rows.clear();
    rows.reserve(bars_.size());
    const QTimeZone tz = utc_ ? QTimeZone(QTimeZone::UTC) : QTimeZone(QTimeZone::LocalTime);
    const double spacing =
        bars_.size() >= 2 ? (bars_.last().time_ms - bars_.first().time_ms) / static_cast<double>(bars_.size() - 1) : kCcDayMs;
    const QString fmt =
        spacing < 0.9 * kCcDayMs ? QStringLiteral("yyyy-MM-dd HH:mm:ss") : QStringLiteral("yyyy-MM-dd");
    for (int i = 0; i < bars_.size(); ++i) {
        const auto& b = bars_[i];
        QStringList row;
        row << QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(std::llround(b.time_ms)), tz).toString(fmt)
            << QString::number(b.open, 'g', 12) << QString::number(b.high, 'g', 12) << QString::number(b.low, 'g', 12)
            << QString::number(b.close, 'g', 12) << QString::number(b.volume, 'g', 12);
        for (const auto& o : overlays_) {
            const double v = i < o.values.size() ? o.values[i] : kCcNaN;
            row << (std::isfinite(v) ? QString::number(v, 'g', 12) : QString());
        }
        rows.append(row);
    }
}

} // namespace fincept::ui
