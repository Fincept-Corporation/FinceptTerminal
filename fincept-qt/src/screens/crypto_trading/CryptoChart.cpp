// CryptoChart.cpp — production candlestick chart on top of Qt Charts.
//
// Why Qt Charts and not TradingView Lightweight Charts?
// ------------------------------------------------------
// TradingView's library is JavaScript-only. Embedding it would require
// QtWebEngineWidgets (Chromium runtime), which is currently not in this
// project's Qt module set and would add ~100 MB to the macOS .app and the
// Windows installer. We can revisit if richer charting (drawing tools,
// indicators panel, replay) becomes worth that footprint.
//
// What this widget does on top of stock Qt Charts:
//  • Crosshair on hover (vertical + horizontal lines through the cursor)
//  • OHLC tooltip in the top-left, updated to the candle under the cursor
//  • Right-edge price tag at the cursor's price (snaps to grid)
//  • Bottom-edge time tag at the cursor's time
//  • Always-visible "last price" tag pinned to the right axis at the
//    most recent close — colour-coded green/red by the latest direction
//  • Time-axis label format auto-scales with the timeframe
//    (HH:mm · MMM dd HH:mm · MMM dd · MMM yy)
//  • Generous bottom margin so the time-axis labels stop being clipped
//  • Mouse wheel = zoom price axis; drag = pan
//
// Sizing principles:
//  • No pixel-pinned heights on the header buttons — uses QSS padding so
//    the toolbar scales with the user's font size.
//  • setMinimumHeight(280) to keep the chart usable on narrow splitters.

#include "screens/crypto_trading/CryptoChart.h"

#include "ui/theme/Theme.h"

#include <QCandlestickSeries>
#include <QCandlestickSet>
#include <QChart>
#include <QChartView>
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

namespace fincept::screens::crypto {

constexpr const char* CryptoChart::TF_LABELS[];

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
    if (tf == QLatin1String("4h"))  return 4 * 60 * 60'000;
    if (tf == QLatin1String("1d"))  return 24 * 60 * 60'000;
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

// ── HoverChartView ──────────────────────────────────────────────────────────
// Custom QChartView that forwards mouse-move events to the parent CryptoChart
// so the crosshair / OHLC tooltip can update without subclassing the chart
// itself. Also enables wheel-zoom on the price axis and drag-pan.
class HoverChartView : public QChartView {
  public:
    HoverChartView(QChart* chart, CryptoChart* host)
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
        // candle stream — users expect Time fixed and Price scaled.
        const double factor = (e->angleDelta().y() > 0) ? 0.9 : 1.1;
        if (auto* y = qobject_cast<QValueAxis*>(host_ ? host_->price_axis_ : nullptr)) {
            const double mid = (y->min() + y->max()) / 2.0;
            const double span = (y->max() - y->min()) * factor;
            y->setRange(mid - span / 2.0, mid + span / 2.0);
        }
        e->accept();
    }

  private:
    CryptoChart* host_ = nullptr;
};

// ── CryptoChart ─────────────────────────────────────────────────────────────

CryptoChart::CryptoChart(QWidget* parent) : QWidget(parent) {
    setObjectName("cryptoChart");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Header with TF toggles ─────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setObjectName("cryptoChartHeader");
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(10, 4, 10, 4);
    h_layout->setSpacing(2);

    auto* title = new QLabel(QStringLiteral("CHART"));
    title->setObjectName("cryptoChartTitle");
    h_layout->addWidget(title);
    h_layout->addSpacing(8);

    for (int i = 0; i < 6; ++i) {
        tf_buttons_[i] = new QPushButton(TF_LABELS[i]);
        tf_buttons_[i]->setObjectName("cryptoTfBtn");
        tf_buttons_[i]->setCursor(Qt::PointingHandCursor);
        tf_buttons_[i]->setFocusPolicy(Qt::NoFocus);
        if (i == active_tf_) tf_buttons_[i]->setProperty("active", true);
        connect(tf_buttons_[i], &QPushButton::clicked, this, [this, i]() { set_active_tf(i); });
        h_layout->addWidget(tf_buttons_[i]);
    }
    h_layout->addStretch();
    layout->addWidget(header);

    // ── Chart proper ───────────────────────────────────────────────────────
    chart_ = new QChart;
    chart_->setBackgroundBrush(QBrush(QColor(colors::BG_SURFACE())));
    chart_->setBackgroundPen(QPen(Qt::NoPen));
    chart_->setPlotAreaBackgroundBrush(QBrush(QColor(colors::BG_BASE())));
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->legend()->hide();
    // Generous bottom + right margins so axis labels are NEVER clipped.
    // The previous (4,4,4,4) was the cause of the "bottom time axis isn't
    // visible" symptom — Qt Charts renders the axis labels INSIDE the chart
    // margin, so 4 px wasn't enough to fit a 12 px label.
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

    // ── Last-price line (horizontal) ───────────────────────────────────────
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

    // ── Chart view ─────────────────────────────────────────────────────────
    chart_view_ = new HoverChartView(chart_, this);
    layout->addWidget(chart_view_, 1);

    // ── Scene overlays (crosshair, price/time tags, last-price tag) ────────
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

    // Small dot at the crosshair intersection — gives the user a clear
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

    // ── OHLC tooltip pinned to top-left of the chart widget ────────────────
    ohlc_tooltip_ = new QLabel(chart_view_);
    ohlc_tooltip_->setObjectName("cryptoChartOhlc");
    ohlc_tooltip_->setVisible(false);
    ohlc_tooltip_->setAttribute(Qt::WA_TransparentForMouseEvents);
    ohlc_tooltip_->move(12, 12);

    setMinimumHeight(280);
}

void CryptoChart::set_active_tf(int idx) {
    for (int i = 0; i < 6; ++i) {
        tf_buttons_[i]->setProperty("active", i == idx);
        if (auto* st = tf_buttons_[i]->style()) {
            st->unpolish(tf_buttons_[i]);
            st->polish(tf_buttons_[i]);
        }
    }
    pending_tf_ = TF_LABELS[idx];
    active_tf_ = idx;
    emit timeframe_changed(TF_LABELS[idx]);
}

QString CryptoChart::current_timeframe() const {
    return TF_LABELS[active_tf_];
}

void CryptoChart::set_candles(const QVector<trading::Candle>& candles) {
    candles_ = candles;
    rebuild_chart();
    apply_tf_axis_format();
    update_last_price_marker();

    if (!pending_tf_.isEmpty()) {
        const QString tf = pending_tf_;
        pending_tf_.clear();
        emit timeframe_changed(tf);
    }
}

void CryptoChart::append_candle(const trading::Candle& candle) {
    if (candle.open <= 0.0 || candle.high <= 0.0 || candle.low <= 0.0 || candle.close <= 0.0 || candle.timestamp <= 0)
        return;
    if (candle.low > candle.high)
        return;

    const auto sets = series_->sets();
    const bool is_update = !candles_.isEmpty() && candles_.last().timestamp == candle.timestamp;

    if (is_update) {
        const trading::Candle prev = candles_.last();
        candles_.last() = candle;
        if (!sets.isEmpty()) {
            auto* last_set = sets.last();
            last_set->setOpen(candle.open);
            last_set->setHigh(candle.high);
            last_set->setLow(candle.low);
            last_set->setClose(candle.close);
        }
        if (!bounds_dirty_) {
            if (prev.high >= cached_max_price_ || prev.low <= cached_min_price_) {
                bounds_dirty_ = true;
            } else {
                cached_max_price_ = std::max(cached_max_price_, candle.high);
                cached_min_price_ = std::min(cached_min_price_, candle.low);
            }
        }
    } else {
        candles_.append(candle);
        if (candles_.size() > MAX_VISIBLE * 2) {
            candles_.remove(0, candles_.size() - MAX_VISIBLE);
            rebuild_chart();
            apply_tf_axis_format();
            update_last_price_marker();
            return;
        }
        if (sets.size() >= MAX_VISIBLE) {
            const auto* oldest = sets.first();
            if (!bounds_dirty_ &&
                (oldest->low() <= cached_min_price_ || oldest->high() >= cached_max_price_)) {
                bounds_dirty_ = true;
            }
            series_->remove(sets.first());
        }
        series_->append(new QCandlestickSet(candle.open, candle.high, candle.low, candle.close, candle.timestamp));

        if (!bounds_dirty_) {
            cached_max_price_ = std::max(cached_max_price_, candle.high);
            cached_min_price_ = std::min(cached_min_price_, candle.low);
        }
    }

    const auto& visible_sets = series_->sets();
    if (visible_sets.isEmpty())
        return;

    if (bounds_dirty_)
        recompute_bounds();

    const qint64 min_time = static_cast<qint64>(visible_sets.first()->timestamp());
    const qint64 max_time = static_cast<qint64>(visible_sets.last()->timestamp());
    update_axes(cached_min_price_, cached_max_price_, min_time, max_time);
    apply_tf_axis_format();
    update_last_price_marker();
}

void CryptoChart::recompute_bounds() {
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

void CryptoChart::clear() {
    candles_.clear();
    series_->clear();
    if (last_price_line_) last_price_line_->clear();
    cached_min_price_ = cached_max_price_ = -1;
    bounds_dirty_ = true;
    if (last_tag_bg_) last_tag_bg_->setVisible(false);
    if (last_tag_txt_) last_tag_txt_->setVisible(false);
    on_hover_leave();
}

void CryptoChart::update_axes(double min_price, double max_price, qint64 min_time, qint64 max_time) {
    if (min_price >= max_price)
        return;

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
        // to the right edge of the plot — looks more like a real terminal.
        effective_max = max_time + tf_slot_ms(TF_LABELS[active_tf_]) / 2;
    }

    if (min_time != last_min_time_ || effective_max != last_max_time_) {
        time_axis_->setRange(QDateTime::fromMSecsSinceEpoch(min_time),
                             QDateTime::fromMSecsSinceEpoch(effective_max));
        last_min_time_ = min_time;
        last_max_time_ = effective_max;
    }
}

void CryptoChart::apply_tf_axis_format() {
    if (!time_axis_) return;
    const qint64 span = std::max<qint64>(0, last_max_time_ - last_min_time_);
    time_axis_->setFormat(time_format_for(span));
}

void CryptoChart::rebuild_chart() {
    series_->clear();
    last_min_price_ = last_max_price_ = -1;
    last_min_time_ = last_max_time_ = -1;
    cached_min_price_ = cached_max_price_ = -1;
    bounds_dirty_ = true;

    if (candles_.isEmpty())
        return;

    const int start = std::max(0, static_cast<int>(candles_.size()) - MAX_VISIBLE);
    double min_price = 1e18, max_price = 0;
    qint64 min_time = INT64_MAX, max_time = 0;

    for (int i = start; i < candles_.size(); ++i) {
        const auto& c = candles_[i];
        if (c.open <= 0.0 || c.high <= 0.0 || c.low <= 0.0 || c.close <= 0.0 || c.timestamp <= 0)
            continue;
        if (c.low > c.high || c.open > c.high || c.close > c.high)
            continue;
        series_->append(new QCandlestickSet(c.open, c.high, c.low, c.close, c.timestamp));
        min_price = std::min(min_price, c.low);
        max_price = std::max(max_price, c.high);
        if (c.timestamp < min_time) min_time = c.timestamp;
        if (c.timestamp > max_time) max_time = c.timestamp;
    }

    if (min_price < max_price) {
        cached_min_price_ = min_price;
        cached_max_price_ = max_price;
        bounds_dirty_ = false;
    }

    update_axes(min_price, max_price, min_time, max_time);
}

void CryptoChart::update_last_price_marker() {
    if (!last_price_line_ || candles_.isEmpty()) {
        if (last_tag_bg_) last_tag_bg_->setVisible(false);
        if (last_tag_txt_) last_tag_txt_->setVisible(false);
        return;
    }
    const auto& last = candles_.last();
    const double price = last.close;
    const bool up = last.close >= last.open;
    last_price_line_->clear();
    last_price_line_->append(QDateTime::fromMSecsSinceEpoch(last_min_time_).toMSecsSinceEpoch(), price);
    last_price_line_->append(QDateTime::fromMSecsSinceEpoch(last_max_time_).toMSecsSinceEpoch(), price);

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

void CryptoChart::on_hover_position(const QPointF& chart_value_pos, const QPoint& view_pos) {
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
    // price" tag — otherwise it overlaps the live cursor price tag whenever
    // the cursor sits at the same y-level as the last close.
    if (last_tag_bg_)  last_tag_bg_->setVisible(false);
    if (last_tag_txt_) last_tag_txt_->setVisible(false);

    constexpr qreal kPadX = 6;
    constexpr qreal kPadY = 3;

    // ── Price tag (right of plot) ──────────────────────────────────────────
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

    // ── Time tag (below plot) ──────────────────────────────────────────────
    // Clamp X so it never extends into the price-axis gutter on the right or
    // off the left edge of the chart.
    {
        const qint64 ms = static_cast<qint64>(chart_value_pos.x());
        const QString fmt = time_format_for(std::max<qint64>(0, last_max_time_ - last_min_time_));
        time_tag_txt_->setText(QDateTime::fromMSecsSinceEpoch(ms).toString(fmt));
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

    // ── OHLC tooltip ───────────────────────────────────────────────────────
    // Find the candle whose timestamp is closest to the cursor.
    const qint64 cursor_ms = static_cast<qint64>(chart_value_pos.x());
    const trading::Candle* hit = nullptr;
    qint64 best = std::numeric_limits<qint64>::max();
    for (const auto& c : candles_) {
        const qint64 d = std::llabs(static_cast<qint64>(c.timestamp) - cursor_ms);
        if (d < best) { best = d; hit = &c; }
    }
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
            .arg(dir_color)                                // %9 close + Δ%
            .arg(QString::number(hit->close, 'f', 2))      // %10
            .arg(chg));                                    // %11
        ohlc_tooltip_->adjustSize();
        ohlc_tooltip_->setVisible(true);
    }
}

void CryptoChart::on_hover_leave() {
    if (xhair_v_)      xhair_v_->setVisible(false);
    if (xhair_h_)      xhair_h_->setVisible(false);
    if (xhair_dot_)    xhair_dot_->setVisible(false);
    if (price_tag_bg_) price_tag_bg_->setVisible(false);
    if (price_tag_txt_)price_tag_txt_->setVisible(false);
    if (time_tag_bg_)  time_tag_bg_->setVisible(false);
    if (time_tag_txt_) time_tag_txt_->setVisible(false);
    if (ohlc_tooltip_) ohlc_tooltip_->setVisible(false);
    // Bring the always-visible last-price tag back now that the cursor's
    // gone — recompute in case anything moved while we were hovering.
    update_last_price_marker();
}

} // namespace fincept::screens::crypto
