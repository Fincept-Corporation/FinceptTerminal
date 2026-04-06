// EquityChart.cpp — Custom-painted candlestick chart
// QPainter-based: no QCandlestickSeries, no QDateTimeAxis gaps.
#include "screens/equity_trading/EquityChart.h"
#include "screens/equity_trading/EquityTypes.h"
#include "ui/theme/ThemeManager.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::equity {

// ── CandleCanvas ─────────────────────────────────────────────────────────────

CandleCanvas::CandleCanvas(QWidget* parent) : QWidget(parent) {
    setObjectName("eqCandleCanvas");
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed,
            this, [this](const ui::ThemeTokens&) {
                dirty_ = true;
                cache_ = QPixmap(); // invalidate cache
                update();
            });
}

void CandleCanvas::set_candles(const QVector<trading::BrokerCandle>& candles) {
    candles_ = candles;
    dirty_   = true;
    update();
}

void CandleCanvas::clear() {
    candles_.clear();
    dirty_ = true;
    update();
}

void CandleCanvas::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    dirty_ = true;
}

void CandleCanvas::paintEvent(QPaintEvent*) {
    if (dirty_) {
        rebuild_cache();
        dirty_ = false;
    }
    QPainter p(this);
    p.drawPixmap(0, 0, cache_);
}

void CandleCanvas::rebuild_cache() {
    const int W = width();
    const int H = height();
    if (W <= 0 || H <= 0) return;

    cache_ = QPixmap(W, H);
    cache_.fill(BG_SURFACE());

    QPainter p(&cache_);
    p.setRenderHint(QPainter::Antialiasing, false);

    // ── Subset ───────────────────────────────────────────────────────────────
    const int total = candles_.size();
    if (total == 0) {
        // Empty state
        p.setPen(TEXT_TERTIARY());
        p.setFont(QFont("monospace", 10));
        p.drawText(cache_.rect(), Qt::AlignCenter, "No data");
        return;
    }

    const int start = qMax(0, total - MAX_VISIBLE);
    const int count = total - start;

    // ── Price range ──────────────────────────────────────────────────────────
    double lo = 1e18, hi = 0.0;
    for (int i = start; i < total; ++i) {
        lo = std::min(lo, candles_[i].low);
        hi = std::max(hi, candles_[i].high);
    }
    if (lo >= hi) return;

    const double margin = (hi - lo) * 0.05;
    lo -= margin;
    hi += margin;

    // ── Layout ───────────────────────────────────────────────────────────────
    const int plot_w = W - PRICE_AXIS_W;
    const int plot_h = H - TIME_AXIS_H;
    if (plot_w <= 0 || plot_h <= 0) return;

    // Lambda: price → y pixel
    auto py = [&](double price) -> int {
        return static_cast<int>(plot_h - (price - lo) / (hi - lo) * plot_h);
    };

    // ── Grid lines ───────────────────────────────────────────────────────────
    p.setPen(QPen(BORDER_DIM(), 1));
    for (int g = 1; g < 5; ++g) {
        int gy = plot_h * g / 5;
        p.drawLine(0, gy, plot_w, gy);
    }

    // ── Candle geometry ──────────────────────────────────────────────────────
    const double slot_w = static_cast<double>(plot_w) / count;
    const int    body_w = qMax(1, static_cast<int>(slot_w * 0.6));
    const int    half   = body_w / 2;

    for (int i = 0; i < count; ++i) {
        const auto& c   = candles_[start + i];
        const int   cx  = static_cast<int>((i + 0.5) * slot_w); // centre x
        const bool  bull = c.close >= c.open;

        const QColor col = bull ? COLOR_BUY() : COLOR_SELL();

        const int open_y  = py(c.open);
        const int close_y = py(c.close);
        const int high_y  = py(c.high);
        const int low_y   = py(c.low);

        const int body_top = std::min(open_y, close_y);
        const int body_bot = std::max(open_y, close_y);
        const int body_h   = qMax(1, body_bot - body_top);

        // Wick
        p.setPen(QPen(col, 1));
        p.drawLine(cx, high_y, cx, low_y);

        // Body
        p.fillRect(cx - half, body_top, body_w, body_h, col);
    }

    // ── Price axis (right) ────────────────────────────────────────────────────
    p.setPen(BORDER_MED());
    p.drawLine(plot_w, 0, plot_w, plot_h);

    p.setPen(TEXT_SECONDARY());
    QFont lbl_font("monospace", 8);
    p.setFont(lbl_font);
    QFontMetrics fm(lbl_font);

    for (int g = 0; g <= 5; ++g) {
        double price = lo + (hi - lo) * g / 5.0;
        int    gy    = py(price);
        QString txt  = QString::number(price, 'f', 2);
        p.drawText(plot_w + 4, gy + fm.ascent() / 2, txt);
    }

    // ── Time axis (bottom) ────────────────────────────────────────────────────
    p.setPen(BORDER_MED());
    p.drawLine(0, plot_h, plot_w, plot_h);

    p.setPen(TEXT_SECONDARY());
    for (int i = 0; i < count; i += LABEL_STEP) {
        const auto& c  = candles_[start + i];
        QDateTime   dt = QDateTime::fromMSecsSinceEpoch(c.timestamp);
        // Show date if daily/weekly, time otherwise
        QString label;
        if (slot_w * LABEL_STEP > 60) {
            // enough space — pick format based on timeframe span
            qint64 span_ms = candles_.last().timestamp - candles_.first().timestamp;
            label = span_ms > 7LL * 24 * 3600 * 1000
                        ? dt.toString("dd MMM")
                        : dt.toString("HH:mm");
        } else {
            label = dt.toString("HH:mm");
        }
        int lx = static_cast<int>((i + 0.5) * slot_w);
        int tw = fm.horizontalAdvance(label);
        p.drawText(lx - tw / 2, plot_h + TIME_AXIS_H - 3, label);
    }
    // Always draw the last label
    {
        const auto& c  = candles_.last();
        QDateTime   dt = QDateTime::fromMSecsSinceEpoch(c.timestamp);
        qint64 span_ms = candles_.last().timestamp - candles_.first().timestamp;
        QString label = span_ms > 7LL * 24 * 3600 * 1000
                            ? dt.toString("dd MMM")
                            : dt.toString("HH:mm");
        int lx = static_cast<int>((count - 0.5) * slot_w);
        int tw = fm.horizontalAdvance(label);
        p.drawText(lx - tw / 2, plot_h + TIME_AXIS_H - 3, label);
    }
}

// ── EquityChart ───────────────────────────────────────────────────────────────

EquityChart::EquityChart(QWidget* parent) : QWidget(parent) {
    setObjectName("eqChart");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header with timeframe buttons
    auto* header = new QWidget;
    header->setObjectName("eqChartHeader");
    header->setFixedHeight(28);
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(8, 0, 8, 0);
    h_layout->setSpacing(2);

    auto* title = new QLabel("CHART");
    title->setObjectName("eqChartTitle");
    h_layout->addWidget(title);
    h_layout->addStretch();

    for (int i = 0; i < 6; ++i) {
        tf_buttons_[i] = new QPushButton(TF_LABELS[i]);
        tf_buttons_[i]->setObjectName("eqTfBtn");
        tf_buttons_[i]->setFixedHeight(20);
        tf_buttons_[i]->setCursor(Qt::PointingHandCursor);
        if (i == active_tf_)
            tf_buttons_[i]->setProperty("active", true);
        connect(tf_buttons_[i], &QPushButton::clicked, this, [this, i]() { set_active_tf(i); });
        h_layout->addWidget(tf_buttons_[i]);
    }
    layout->addWidget(header);

    canvas_ = new CandleCanvas(this);
    layout->addWidget(canvas_, 1);
}

void EquityChart::set_candles(const QVector<trading::BrokerCandle>& candles) {
    canvas_->set_candles(candles);
}

void EquityChart::clear() {
    canvas_->clear();
}

QString EquityChart::current_timeframe() const {
    return TF_LABELS[active_tf_];
}

void EquityChart::set_active_tf(int idx) {
    if (idx == active_tf_) return;
    for (int i = 0; i < 6; ++i) {
        tf_buttons_[i]->setProperty("active", i == idx);
        tf_buttons_[i]->style()->unpolish(tf_buttons_[i]);
        tf_buttons_[i]->style()->polish(tf_buttons_[i]);
    }
    active_tf_ = idx;
    emit timeframe_changed(TF_LABELS[idx]);
}

} // namespace fincept::screens::equity
