// EquityChart.cpp — Qt Charts candlestick chart with overlays.
//
// Ported from QPainter-based rendering to Qt Charts, matching the CryptoChart
// implementation pattern. This gives EquityChart the same overlay
// infrastructure (indicators, crosshair, hover tooltips).
//
// What this widget does on top of stock Qt Charts:
//  * Crosshair on hover (vertical + horizontal lines through the cursor)
//  * OHLC tooltip in the top-left, updated to the candle under the cursor
//  * Right-edge price tag at the cursor's price (snaps to grid)
//  * Bottom-edge time tag at the cursor's time
//  * Always-visible "last price" tag pinned to the right axis at the
//    most recent close -- colour-coded green/red by the latest direction
//  * Time-axis label format auto-scales with the timeframe
//    (HH:mm / MMM dd HH:mm / MMM dd / MMM yy)
//  * Generous bottom margin so the time-axis labels stop being clipped
//  * Mouse wheel = zoom price axis; drag = pan

#include "screens/equity_trading/EquityChart.h"

#include "ui/charts/CandleData.h"
#include "ui/charts/ChartOverlayManager.h"
#include "ui/charts/IndicatorPicker.h"
#include "ui/charts/layers/EmaLayer.h"
#include "ui/charts/layers/VwapLayer.h"
#include "ui/charts/layers/BollingerLayer.h"
#include "ui/charts/layers/SupportResistanceLayer.h"
#include "ui/charts/layers/PivotLayer.h"
#include "ui/charts/HorizontalLineLayer.h"

#include "ui/theme/Theme.h"

#include <QCandlestickSeries>
#include <QCandlestickSet>
#include <QChart>
#include <QChartView>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QEnterEvent>
#include <QFont>
#include <QGraphicsLayout>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineSeries>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QStyle>
#include <QToolTip>
#include <QValueAxis>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

using namespace fincept::ui;

namespace fincept::screens::equity {

constexpr const char* EquityChart::TF_LABELS[];

namespace {

// Maps a timeframe label to its slot duration in milliseconds. Used to
// extend the time axis by one slot when there is only one candle (otherwise
// QCandlestickSeries renders bars as hairlines) and to pick the time-axis
// label format.
qint64 tf_slot_ms(const QString& tf) {
    if (tf == QLatin1String("1m"))  return 60'000;
    if (tf == QLatin1String("5m"))  return 5 * 60'000;
    if (tf == QLatin1String("15m")) return 15 * 60'000;
    if (tf == QLatin1String("1h"))  return 60 * 60'000;
    if (tf == QLatin1String("1d"))  return 24 * 60 * 60'000;
    if (tf == QLatin1String("1w"))  return 7 * 24 * 60 * 60'000;
    return 60'000;
}

// Choose a time-axis label format that fits the chart's time span without
// overlapping ticks. Driven by total span, not just the timeframe, so a
// 120-bar 1d chart shows months/years rather than redundant day labels.
QString time_format_for(qint64 span_ms) {
    constexpr qint64 kHour = 60LL * 60 * 1000;
    constexpr qint64 kDay = 24 * kHour;
    if (span_ms <= 6 * kHour)        return QStringLiteral("HH:mm");
    if (span_ms <= 3 * kDay)         return QStringLiteral("MMM dd HH:mm");
    if (span_ms <= 90 * kDay)        return QStringLiteral("MMM dd");
    return QStringLiteral("MMM yy");
}

QFont scene_font() {
    QFont f;
    f.setFamily(QStringLiteral("Consolas"));
    f.setPointSize(9);
    f.setWeight(QFont::DemiBold);
    return f;
}

} // namespace

// -- HoverEquityChartView ----------------------------------------------------
// Custom QChartView that forwards mouse-move events to the parent EquityChart
// so the crosshair / OHLC tooltip can update without subclassing the chart
// itself. Also enables wheel-zoom on the price axis and drag-pan.
class HoverEquityChartView : public QChartView {
  public:
    HoverEquityChartView(QChart* chart, EquityChart* host)
        : QChartView(chart), host_(host) {
        setMouseTracking(true);
        setRenderHint(QPainter::Antialiasing, true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setRubberBand(QChartView::NoRubberBand);
        setDragMode(QGraphicsView::NoDrag);
    }

  protected:
    void mouseMoveEvent(QMouseEvent* e) override {
        if (host_ && chart()) {
            const QPointF chart_pos = chart()->mapToValue(e->pos());
            host_->on_hover_position(chart_pos, e->pos());
        }
        QChartView::mouseMoveEvent(e);
    }
    void leaveEvent(QEvent* e) override {
        if (host_) host_->on_hover_leave();
        QChartView::leaveEvent(e);
    }
    void wheelEvent(QWheelEvent* e) override {
        if (!chart()) {
            QChartView::wheelEvent(e);
            return;
        }
        // Zoom price axis only. Wheel-panning the time axis collides with the
        // candle stream -- users expect Time fixed and Price scaled.
        const double factor = (e->angleDelta().y() > 0) ? 0.9 : 1.1;
        if (auto* y = qobject_cast<QValueAxis*>(host_ ? host_->price_axis_ : nullptr)) {
            const double mid = (y->min() + y->max()) / 2.0;
            const double span = (y->max() - y->min()) * factor;
            y->setRange(mid - span / 2.0, mid + span / 2.0);
        }
        e->accept();
    }
    void contextMenuEvent(QContextMenuEvent* e) override {
        if (host_ && chart()) {
            const QPointF v = chart()->mapToValue(e->pos());
            host_->show_trade_menu(v.y(), e->globalPos());
            e->accept();
            return;
        }
        QChartView::contextMenuEvent(e);
    }

  private:
    EquityChart* host_ = nullptr;
};

// -- EquityChart -------------------------------------------------------------

EquityChart::EquityChart(QWidget* parent) : QWidget(parent) {
    setObjectName("eqChart");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // -- Header with TF toggles ----------------------------------------------
    auto* header = new QWidget(this);
    header->setObjectName("eqChartHeader");
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(10, 4, 10, 4);
    h_layout->setSpacing(2);

    title_label_ = new QLabel(tr("CHART"));
    title_label_->setObjectName("eqChartTitle");
    h_layout->addWidget(title_label_);
    h_layout->addSpacing(8);

    for (int i = 0; i < 6; ++i) {
        tf_buttons_[i] = new QPushButton(TF_LABELS[i]);
        tf_buttons_[i]->setObjectName("eqTfBtn");
        tf_buttons_[i]->setCursor(Qt::PointingHandCursor);
        tf_buttons_[i]->setFocusPolicy(Qt::NoFocus);
        if (i == active_tf_) tf_buttons_[i]->setProperty("active", true);
        connect(tf_buttons_[i], &QPushButton::clicked, this, [this, i]() { set_active_tf(i); });
        h_layout->addWidget(tf_buttons_[i]);
    }
    h_layout->addStretch();
    layout->addWidget(header);

    // -- Chart proper --------------------------------------------------------
    chart_ = new QChart;
    chart_->setBackgroundBrush(QBrush(QColor(colors::BG_SURFACE())));
    chart_->setBackgroundPen(QPen(Qt::NoPen));
    chart_->setPlotAreaBackgroundBrush(QBrush(QColor(colors::BG_BASE())));
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->legend()->hide();
    // Generous bottom + right margins so axis labels are NEVER clipped.
    // Right margin = 72 px gives the price-axis labels and the right-edge
    // hover/last-price tags room for "99999.99" without bleeding into the
    // plot. Bottom = 36 px fits "MMM dd HH:mm" comfortably.
    chart_->setMargins(QMargins(6, 6, 72, 36));
    if (auto* l = chart_->layout())
        l->setContentsMargins(0, 0, 0, 0);

    series_ = new QCandlestickSeries;
    series_->setIncreasingColor(QColor(colors::POSITIVE()));
    series_->setDecreasingColor(QColor(colors::NEGATIVE()));
    series_->setBodyWidth(0.78);
    series_->setPen(QPen(Qt::NoPen)); // no outline on body
    series_->setCapsVisible(false);
    chart_->addSeries(series_);

    time_axis_ = new QDateTimeAxis;
    time_axis_->setFormat(QStringLiteral("HH:mm"));
    time_axis_->setLabelsColor(QColor(colors::TEXT_SECONDARY()));
    time_axis_->setLabelsFont(scene_font());
    time_axis_->setGridLineColor(QColor(colors::BORDER_DIM()));
    time_axis_->setMinorGridLineColor(QColor(colors::BG_RAISED()));
    time_axis_->setLinePenColor(QColor(colors::BORDER_DIM()));
    time_axis_->setTickCount(6);
    chart_->addAxis(time_axis_, Qt::AlignBottom);
    series_->attachAxis(time_axis_);

    price_axis_ = new QValueAxis;
    price_axis_->setLabelsColor(QColor(colors::TEXT_SECONDARY()));
    price_axis_->setLabelsFont(scene_font());
    price_axis_->setGridLineColor(QColor(colors::BORDER_DIM()));
    price_axis_->setMinorGridLineColor(QColor(colors::BG_RAISED()));
    price_axis_->setLinePenColor(QColor(colors::BORDER_DIM()));
    price_axis_->setLabelFormat(QStringLiteral("%.2f"));
    price_axis_->setTickCount(6);
    chart_->addAxis(price_axis_, Qt::AlignRight);
    series_->attachAxis(price_axis_);

    // -- Last-price line (horizontal) ----------------------------------------
    last_price_line_ = new QLineSeries;
    last_price_line_->setUseOpenGL(false);
    QPen last_pen;
    last_pen.setColor(QColor(colors::AMBER()));
    last_pen.setStyle(Qt::DashLine);
    last_pen.setWidthF(1.0);
    last_price_line_->setPen(last_pen);
    chart_->addSeries(last_price_line_);
    last_price_line_->attachAxis(time_axis_);
    last_price_line_->attachAxis(price_axis_);

    // -- Chart view ----------------------------------------------------------
    chart_view_ = new HoverEquityChartView(chart_, this);
    layout->addWidget(chart_view_, 1);

    // -- Scene overlays (crosshair, price/time tags, last-price tag) ---------
    auto* scene = chart_view_->scene();
    QPen pen_grid;
    pen_grid.setColor(QColor(colors::BORDER_BRIGHT()));
    pen_grid.setStyle(Qt::DashLine);
    pen_grid.setWidthF(0.8);

    xhair_v_ = scene->addLine(QLineF(), pen_grid);
    xhair_h_ = scene->addLine(QLineF(), pen_grid);
    xhair_v_->setZValue(20);
    xhair_h_->setZValue(20);
    xhair_v_->setVisible(false);
    xhair_h_->setVisible(false);

    // Small dot at the crosshair intersection -- gives the user a clear
    // anchor point and disambiguates which exact price/time the tags refer
    // to when the cursor is moving fast.
    QPen dot_pen;
    dot_pen.setColor(QColor(colors::AMBER()));
    dot_pen.setWidthF(1.0);
    xhair_dot_ = scene->addEllipse(QRectF(-3, -3, 6, 6), dot_pen,
                                    QBrush(QColor(colors::BG_BASE())));
    xhair_dot_->setZValue(23);
    xhair_dot_->setVisible(false);

    auto build_tag = [&](QGraphicsRectItem*& bg, QGraphicsSimpleTextItem*& txt,
                         const QColor& fill, const QColor& text_color) {
        bg = scene->addRect(QRectF(), QPen(Qt::NoPen), QBrush(fill));
        bg->setZValue(21);
        bg->setVisible(false);
        txt = scene->addSimpleText(QStringLiteral(""));
        txt->setBrush(QBrush(text_color));
        txt->setFont(scene_font());
        txt->setZValue(22);
        txt->setVisible(false);
    };
    build_tag(price_tag_bg_, price_tag_txt_, QColor(colors::BG_RAISED()), QColor(colors::TEXT_PRIMARY()));
    build_tag(time_tag_bg_,  time_tag_txt_,  QColor(colors::BG_RAISED()), QColor(colors::TEXT_PRIMARY()));
    build_tag(last_tag_bg_,  last_tag_txt_,  QColor(colors::AMBER()),     QColor(colors::BG_BASE()));

    // -- OHLC tooltip pinned to top-left of the chart widget -----------------
    ohlc_tooltip_ = new QLabel(chart_view_);
    ohlc_tooltip_->setObjectName("eqChartOhlc");
    ohlc_tooltip_->setVisible(false);
    ohlc_tooltip_->setAttribute(Qt::WA_TransparentForMouseEvents);
    ohlc_tooltip_->move(12, 12);

    setMinimumHeight(280);

    // --- Overlay engine ---
    overlay_mgr_ = new fincept::ui::ChartOverlayManager(this);
    overlay_mgr_->set_chart(chart_view_->scene(), chart_);

    connect(chart_, &QChart::plotAreaChanged, this, [this](const QRectF&) {
        overlay_mgr_->reposition_all();
    });

    indicator_picker_ = new fincept::ui::IndicatorPicker(overlay_mgr_, this);
    layout->insertWidget(layout->indexOf(chart_view_), indicator_picker_);

    connect(indicator_picker_, &fincept::ui::IndicatorPicker::indicator_requested,
            this, [this](const QString& id) {
        using namespace fincept::ui;
        OverlayLayer* layer = nullptr;
        if (id.startsWith("ema_")) {
            int period = id.mid(4).toInt();
            if (period <= 0) period = 21;
            const QColor colors[] = {QColor("#d97706"), QColor("#16a34a"), QColor("#2563eb"), QColor("#dc2626")};
            int idx = (period == 9) ? 0 : (period == 21) ? 1 : (period == 50) ? 2 : 3;
            layer = new EmaLayer(period, colors[idx]);
        } else if (id == "vwap") {
            layer = new VwapLayer(true);
        } else if (id.startsWith("bb_")) {
            layer = new BollingerLayer();
        } else if (id == "sr_auto") {
            layer = new SupportResistanceLayer();
        } else if (id == "pivot_std") {
            layer = new PivotLayer();
        }
        if (layer)
            overlay_mgr_->add_layer(layer);
    });

    connect(indicator_picker_, &fincept::ui::IndicatorPicker::indicator_removed,
            this, [this](const QString& id) {
        overlay_mgr_->remove_layer(id);
    });
}

void EquityChart::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquityChart::retranslateUi() {
    if (title_label_) title_label_->setText(tr("CHART"));
    // Timeframe button labels (1m/5m/…) are fixed codes, not translatable.
}

void EquityChart::set_active_tf(int idx) {
    for (int i = 0; i < 6; ++i) {
        tf_buttons_[i]->setProperty("active", i == idx);
        if (auto* st = tf_buttons_[i]->style()) {
            st->unpolish(tf_buttons_[i]);
            st->polish(tf_buttons_[i]);
        }
    }
    active_tf_ = idx;
    emit timeframe_changed(TF_LABELS[idx]);
}

QString EquityChart::current_timeframe() const {
    return TF_LABELS[active_tf_];
}

void EquityChart::set_candles(const QVector<trading::BrokerCandle>& candles) {
    candles_ = candles;
    rebuild_chart();

    // Overlay layers must use synthetic timestamps so their QLineSeries
    // align with the chart's gap-free X axis.
    QVector<fincept::ui::CandleData> synth_candles;
    synth_candles.reserve(display_count_);
    for (int i = display_start_; i < display_start_ + display_count_ && i < candles_.size(); ++i) {
        const auto& c = candles_[i];
        synth_candles.append({synth_ts_for(i), c.open, c.high, c.low, c.close, c.volume});
    }
    overlay_mgr_->set_candles(std::move(synth_candles));

    apply_tf_axis_format();
    update_last_price_marker();
}

void EquityChart::update_last_candle(const trading::BrokerCandle& candle) {
    if (candle.timestamp <= 0 || candle.open <= 0.0 || candle.high <= 0.0 ||
        candle.low <= 0.0 || candle.close <= 0.0)
        return;

    if (candles_.isEmpty() || candle.timestamp > candles_.last().timestamp) {
        // Roll-forward: a new forming bar. Reuse the bulk path (rebuild +
        // overlays + axes) -- it runs at most once per bar interval.
        QVector<trading::BrokerCandle> next = candles_;
        next.append(candle);
        set_candles(next);
        return;
    }
    if (candle.timestamp != candles_.last().timestamp)
        return; // tick for an older bar -- the next history reload reconciles

    // Per-tick hot path: replace the forming bar in place.
    candles_.last() = candle;

    const auto sets = series_->sets();
    QCandlestickSet* last_set = sets.isEmpty() ? nullptr : sets.last();
    if (last_set &&
        static_cast<int64_t>(last_set->timestamp()) == synth_ts_for(candles_.size() - 1)) {
        last_set->setOpen(candle.open);
        last_set->setHigh(candle.high);
        last_set->setLow(candle.low);
        last_set->setClose(candle.close);
    } else {
        // The last candle has no candlestick set (it was invalid when the
        // history loaded) -- rebuild once so the forming bar is rendered.
        rebuild_chart();
    }

    // Grow the price axis when the tick pierces the current bounds. Passing
    // the sets' own raw synthetic range leaves the time axis untouched.
    if (bounds_dirty_)
        recompute_bounds();
    if (cached_min_price_ > 0.0 &&
        (candle.low < cached_min_price_ || candle.high > cached_max_price_)) {
        cached_min_price_ = std::min(cached_min_price_, candle.low);
        cached_max_price_ = std::max(cached_max_price_, candle.high);
        const auto cur = series_->sets();
        if (!cur.isEmpty())
            update_axes(cached_min_price_, cached_max_price_,
                        static_cast<qint64>(cur.first()->timestamp()),
                        static_cast<qint64>(cur.last()->timestamp()));
    }

    update_last_price_marker();
}

void EquityChart::recompute_bounds() {
    const auto& sets = series_->sets();
    if (sets.isEmpty()) {
        cached_min_price_ = -1;
        cached_max_price_ = -1;
        bounds_dirty_ = false;
        return;
    }
    double mn = 1e18, mx = 0;
    for (const auto* s : sets) {
        mn = std::min(mn, s->low());
        mx = std::max(mx, s->high());
    }
    cached_min_price_ = mn;
    cached_max_price_ = mx;
    bounds_dirty_ = false;
}

void EquityChart::clear() {
    candles_.clear();
    series_->clear();
    if (last_price_line_) last_price_line_->clear();
    cached_min_price_ = cached_max_price_ = -1;
    bounds_dirty_ = true;
    if (last_tag_bg_) last_tag_bg_->setVisible(false);
    if (last_tag_txt_) last_tag_txt_->setVisible(false);
    on_hover_leave();
}

void EquityChart::set_position_line(double price, const QColor& color, const QString& label) {
    if (!overlay_mgr_ || price <= 0.0)
        return;
    if (!pos_line_) {
        pos_line_ = new fincept::ui::HorizontalLineLayer(QStringLiteral("__position"),
                                                         QStringLiteral("Position"));
        overlay_mgr_->add_layer(pos_line_);
    }
    fincept::ui::HorizontalLevel lvl;
    lvl.price = price;
    lvl.label = label;
    lvl.color = color;
    lvl.style = Qt::DashLine;
    pos_line_->set_levels({lvl});
    overlay_mgr_->reposition_all();
}

void EquityChart::clear_position_line() {
    if (pos_line_)
        pos_line_->set_levels({});
    if (overlay_mgr_)
        overlay_mgr_->reposition_all();
}

void EquityChart::update_axes(double min_price, double max_price, qint64 min_time, qint64 max_time) {
    if (min_price >= max_price)
        return;

    // Expand price range to include overlay levels (pivots, S/R)
    double overlay_min = 0, overlay_max = 0;
    if (overlay_mgr_->overlay_price_range(overlay_min, overlay_max)) {
        min_price = std::min(min_price, overlay_min);
        max_price = std::max(max_price, overlay_max);
    }

    const double padding = (max_price - min_price) * 0.06;
    const double p_min = min_price - padding;
    const double p_max = max_price + padding;

    if (p_min != last_min_price_ || p_max != last_max_price_) {
        price_axis_->setRange(p_min, p_max);
        last_min_price_ = p_min;
        last_max_price_ = p_max;
    }

    qint64 effective_max = max_time;
    if (min_time >= max_time) {
        effective_max = min_time + tf_slot_ms(TF_LABELS[active_tf_]);
    } else {
        // Add half-a-slot of right padding so the latest candle isn't pinned
        // to the right edge of the plot -- looks more like a real terminal.
        effective_max = max_time + tf_slot_ms(TF_LABELS[active_tf_]) / 2;
    }

    if (min_time != last_min_time_ || effective_max != last_max_time_) {
        time_axis_->setRange(QDateTime::fromMSecsSinceEpoch(min_time),
                             QDateTime::fromMSecsSinceEpoch(effective_max));
        last_min_time_ = min_time;
        last_max_time_ = effective_max;
    }

    overlay_mgr_->reposition_all();
}

void EquityChart::apply_tf_axis_format() {
    if (!time_axis_ || candles_.isEmpty()) return;
    // Use real time span (not synthetic) to pick the right label format.
    const int64_t real_first = candles_[display_start_].timestamp;
    const int64_t real_last = candles_.last().timestamp;
    const qint64 real_span = std::max<qint64>(0, real_last - real_first);
    time_axis_->setFormat(time_format_for(real_span));
}

int64_t EquityChart::synth_ts_for(int candle_index) const {
    return synth_base_ts_ + static_cast<int64_t>(candle_index - display_start_) * synth_slot_ms_;
}

int EquityChart::candle_index_from_synth(int64_t synth_ts) const {
    if (synth_slot_ms_ <= 0) return display_start_;
    int idx = display_start_ + static_cast<int>((synth_ts - synth_base_ts_) / synth_slot_ms_);
    return std::clamp(idx, display_start_, display_start_ + display_count_ - 1);
}

const trading::BrokerCandle* EquityChart::candle_near_synth(int64_t synth_ts) const {
    const int idx = candle_index_from_synth(synth_ts);
    if (idx >= 0 && idx < candles_.size())
        return &candles_[idx];
    return nullptr;
}

void EquityChart::rebuild_chart() {
    series_->clear();
    last_min_price_ = last_max_price_ = -1;
    last_min_time_ = last_max_time_ = -1;
    cached_min_price_ = cached_max_price_ = -1;
    bounds_dirty_ = true;

    if (candles_.isEmpty())
        return;

    const int start = std::max(0, static_cast<int>(candles_.size()) - MAX_VISIBLE);
    display_start_ = start;
    display_count_ = 0;

    synth_slot_ms_ = tf_slot_ms(TF_LABELS[active_tf_]);
    synth_base_ts_ = candles_[start].timestamp;

    double min_price = 1e18, max_price = 0;
    qint64 min_synth = INT64_MAX, max_synth = 0;

    for (int i = start; i < candles_.size(); ++i) {
        const auto& c = candles_[i];
        if (c.open <= 0.0 || c.high <= 0.0 || c.low <= 0.0 || c.close <= 0.0 || c.timestamp <= 0)
            continue;
        if (c.low > c.high || c.open > c.high || c.close > c.high)
            continue;

        const int64_t synth = synth_ts_for(i);
        series_->append(new QCandlestickSet(c.open, c.high, c.low, c.close, synth));
        ++display_count_;

        min_price = std::min(min_price, c.low);
        max_price = std::max(max_price, c.high);
        if (synth < min_synth) min_synth = synth;
        if (synth > max_synth) max_synth = synth;
    }

    if (min_price < max_price) {
        cached_min_price_ = min_price;
        cached_max_price_ = max_price;
        bounds_dirty_ = false;
    }

    update_axes(min_price, max_price, min_synth, max_synth);
}

void EquityChart::update_last_price_marker() {
    if (!last_price_line_ || candles_.isEmpty()) {
        if (last_tag_bg_) last_tag_bg_->setVisible(false);
        if (last_tag_txt_) last_tag_txt_->setVisible(false);
        return;
    }
    const auto& last = candles_.last();
    const double price = last.close;
    const bool up = last.close >= last.open;
    last_price_line_->clear();
    last_price_line_->append(static_cast<qreal>(last_min_time_), price);
    last_price_line_->append(static_cast<qreal>(last_max_time_), price);

    QPen pen = last_price_line_->pen();
    pen.setColor(QColor(up ? colors::POSITIVE() : colors::NEGATIVE()));
    last_price_line_->setPen(pen);

    // Update the always-visible "last price" tag on the right axis.
    if (last_tag_bg_ && last_tag_txt_ && chart_) {
        const QPointF anchor = chart_->mapToPosition(QPointF(last_max_time_, price), series_);
        const QString text = QString::number(price, 'f', 2);
        last_tag_txt_->setText(text);
        const QRectF tb = last_tag_txt_->boundingRect();
        const qreal pad_x = 6, pad_y = 2;
        const qreal x = chart_->plotArea().right() + 2;
        const qreal y = anchor.y() - tb.height() / 2 - pad_y;
        last_tag_bg_->setRect(QRectF(x, y, tb.width() + 2 * pad_x, tb.height() + 2 * pad_y));
        last_tag_bg_->setBrush(QBrush(QColor(up ? colors::POSITIVE() : colors::NEGATIVE())));
        last_tag_txt_->setPos(x + pad_x, y + pad_y);
        last_tag_bg_->setVisible(true);
        last_tag_txt_->setVisible(true);
    }
}

void EquityChart::on_hover_position(const QPointF& chart_value_pos, const QPoint& view_pos) {
    if (!chart_ || candles_.isEmpty()) return;

    const QRectF plot = chart_->plotArea();
    if (!plot.contains(view_pos)) {
        on_hover_leave();
        return;
    }

    // Crosshair lines: stop a half-pixel before the plot edge so they don't
    // bleed into the axis labels at the bottom-right corner.
    const qreal cx = view_pos.x();
    const qreal cy = view_pos.y();
    xhair_v_->setLine(cx, plot.top(), cx, plot.bottom());
    xhair_h_->setLine(plot.left(), cy, plot.right(), cy);
    xhair_v_->setVisible(true);
    xhair_h_->setVisible(true);

    // Intersection dot, centred on the cursor.
    if (xhair_dot_) {
        xhair_dot_->setRect(cx - 3, cy - 3, 6, 6);
        xhair_dot_->setVisible(true);
    }

    // While the cursor is over the chart, hide the always-visible "last
    // price" tag -- otherwise it overlaps the live cursor price tag whenever
    // the cursor sits at the same y-level as the last close.
    if (last_tag_bg_)  last_tag_bg_->setVisible(false);
    if (last_tag_txt_) last_tag_txt_->setVisible(false);

    constexpr qreal kPadX = 6;
    constexpr qreal kPadY = 3;

    // -- Price tag (right of plot) -------------------------------------------
    // Clamp Y so the tag never extends past the plot edges into the time tag.
    {
        const double price = chart_value_pos.y();
        price_tag_txt_->setText(QString::number(price, 'f', 2));
        const QRectF tb = price_tag_txt_->boundingRect();
        const qreal w = tb.width() + 2 * kPadX;
        const qreal h = tb.height() + 2 * kPadY;
        const qreal x = plot.right() + 4;
        qreal y = cy - h / 2.0;
        y = std::max(plot.top(), std::min(y, plot.bottom() - h));
        price_tag_bg_->setRect(QRectF(x, y, w, h));
        price_tag_txt_->setPos(x + kPadX, y + kPadY);
        price_tag_bg_->setVisible(true);
        price_tag_txt_->setVisible(true);
    }

    // -- Time tag (below plot) -----------------------------------------------
    // Map synthetic timestamp back to real timestamp for display.
    {
        const qint64 synth_ms = static_cast<qint64>(chart_value_pos.x());
        const auto* nearest = candle_near_synth(synth_ms);
        const qint64 real_ms = nearest ? nearest->timestamp : synth_ms;
        const int64_t real_first = (display_start_ < candles_.size()) ? candles_[display_start_].timestamp : 0;
        const int64_t real_last = candles_.isEmpty() ? 0 : candles_.last().timestamp;
        const QString fmt = time_format_for(std::max<qint64>(0, real_last - real_first));
        time_tag_txt_->setText(QDateTime::fromMSecsSinceEpoch(real_ms).toString(fmt));
        const QRectF tb = time_tag_txt_->boundingRect();
        const qreal w = tb.width() + 2 * kPadX;
        const qreal h = tb.height() + 2 * kPadY;
        qreal x = cx - w / 2.0;
        x = std::max(plot.left(), std::min(x, plot.right() - w));
        const qreal y = plot.bottom() + 4;
        time_tag_bg_->setRect(QRectF(x, y, w, h));
        time_tag_txt_->setPos(x + kPadX, y + kPadY);
        time_tag_bg_->setVisible(true);
        time_tag_txt_->setVisible(true);
    }

    // -- OHLC tooltip --------------------------------------------------------
    // Map synthetic timestamp to candle index for direct lookup.
    const qint64 synth_cursor = static_cast<qint64>(chart_value_pos.x());
    const trading::BrokerCandle* hit = candle_near_synth(synth_cursor);
    if (hit) {
        const bool up = hit->close >= hit->open;
        const double pct = (hit->close - hit->open) / std::max(1e-12, hit->open) * 100.0;
        const QString chg = QString("%1%2%")
                                .arg(pct >= 0 ? QStringLiteral("+") : QString())
                                .arg(QString::number(pct, 'f', 2));
        const QString time_str =
            QDateTime::fromMSecsSinceEpoch(hit->timestamp).toString(QStringLiteral("MMM dd  HH:mm"));
        const QString dir_color = up ? QString::fromLatin1(colors::POSITIVE())
                                     : QString::fromLatin1(colors::NEGATIVE());
        const QString dim   = QString::fromLatin1(colors::TEXT_TERTIARY());
        const QString fg    = QString::fromLatin1(colors::TEXT_PRIMARY());
        const QString green = QString::fromLatin1(colors::POSITIVE());
        const QString red   = QString::fromLatin1(colors::NEGATIVE());

        // Vertical layout: header row + four label/value rows + change row.
        // Numeric values use a fixed-width <td> so different price magnitudes
        // (e.g. 79655.40 vs 9.50) don't push the values into different
        // columns from row to row. font-family:Consolas keeps digits aligned.
        const QString tpl = QStringLiteral(
            "<div style='font-family:Consolas,Courier New,monospace;'>"
            "<div style='color:%1;font-size:10px;font-weight:700;letter-spacing:0.8px;"
            "  margin-bottom:6px;'>%2</div>"
            "<table cellspacing='0' cellpadding='0' style='border-spacing:0;font-size:12px;'>"
            "<tr>"
            "  <td style='color:%1;font-weight:700;padding-right:10px;'>O</td>"
            "  <td style='color:%3;text-align:right;'>%4</td>"
            "</tr><tr>"
            "  <td style='color:%1;font-weight:700;padding-right:10px;padding-top:1px;'>H</td>"
            "  <td style='color:%5;text-align:right;padding-top:1px;'>%6</td>"
            "</tr><tr>"
            "  <td style='color:%1;font-weight:700;padding-right:10px;padding-top:1px;'>L</td>"
            "  <td style='color:%7;text-align:right;padding-top:1px;'>%8</td>"
            "</tr><tr>"
            "  <td style='color:%1;font-weight:700;padding-right:10px;padding-top:1px;'>C</td>"
            "  <td style='color:%9;text-align:right;font-weight:700;padding-top:1px;'>%10</td>"
            "</tr><tr>"
            "  <td colspan='2' style='padding-top:6px;color:%9;font-weight:700;text-align:right;"
            "    border-top:1px solid #1a1a1a;'>%11</td>"
            "</tr>"
            "</table>"
            "</div>");
        ohlc_tooltip_->setText(tpl
            .arg(dim)                                      // %1 dim/label colour
            .arg(time_str)                                 // %2 timestamp
            .arg(fg)                                       // %3 open colour
            .arg(QString::number(hit->open,  'f', 2))      // %4
            .arg(green)                                    // %5 high colour
            .arg(QString::number(hit->high,  'f', 2))      // %6
            .arg(red)                                      // %7 low colour
            .arg(QString::number(hit->low,   'f', 2))      // %8
            .arg(dir_color)                                // %9 close + delta%
            .arg(QString::number(hit->close, 'f', 2))      // %10
            .arg(chg));                                    // %11
        ohlc_tooltip_->adjustSize();
        ohlc_tooltip_->setVisible(true);
    }
}

void EquityChart::on_hover_leave() {
    if (xhair_v_)      xhair_v_->setVisible(false);
    if (xhair_h_)      xhair_h_->setVisible(false);
    if (xhair_dot_)    xhair_dot_->setVisible(false);
    if (price_tag_bg_) price_tag_bg_->setVisible(false);
    if (price_tag_txt_)price_tag_txt_->setVisible(false);
    if (time_tag_bg_)  time_tag_bg_->setVisible(false);
    if (time_tag_txt_) time_tag_txt_->setVisible(false);
    if (ohlc_tooltip_) ohlc_tooltip_->setVisible(false);
    // Bring the always-visible last-price tag back now that the cursor's
    // gone -- recompute in case anything moved while we were hovering.
    update_last_price_marker();
}

void EquityChart::show_trade_menu(double price, const QPoint& global_pos) {
    const QString pstr = price > 0 ? QString::number(price, 'f', 2) : QString();
    QMenu menu(this);
    QAction* buy_act = menu.addAction(price > 0 ? tr("Buy @ %1").arg(pstr) : tr("Buy"));
    QAction* sell_act = menu.addAction(price > 0 ? tr("Sell @ %1").arg(pstr) : tr("Sell"));
    menu.addSeparator();
    QAction* wl_act = menu.addAction(tr("Add to watchlist"));

    QAction* chosen = menu.exec(global_pos);
    if (chosen == buy_act)
        emit buy_requested(price);
    else if (chosen == sell_act)
        emit sell_requested(price);
    else if (chosen == wl_act)
        emit add_to_watchlist_requested();
}

} // namespace fincept::screens::equity
