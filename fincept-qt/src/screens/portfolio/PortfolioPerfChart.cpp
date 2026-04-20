// src/screens/portfolio/PortfolioPerfChart.cpp
#include "screens/portfolio/PortfolioPerfChart.h"

#include "ui/theme/Theme.h"

#include <QAreaSeries>
#include <QChart>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLineSeries>
#include <QMouseEvent>
#include <QScreen>
#include <QTimeZone>
#include <QVBoxLayout>
#include <QValueAxis>

#include <cmath>
#include <limits>

namespace {
static const QStringList kPeriods = {"1D", "1W", "1M", "3M", "YTD", "1Y", "ALL"};
} // namespace

namespace fincept::screens {

// ── CrosshairChartView ────────────────────────────────────────────────────────

CrosshairChartView::CrosshairChartView(QChart* chart, QWidget* parent) : QChartView(chart, parent) {
    setMouseTracking(true);

    // Vertical crosshair line (hidden by default)
    v_line_ = new QGraphicsLineItem(chart);
    QPen crosshair_pen(QColor(ui::colors::TEXT_TERTIARY()), 1, Qt::DashLine);
    v_line_->setPen(crosshair_pen);
    v_line_->setVisible(false);
    v_line_->setZValue(10);

    // Floating tooltip label (child of this widget, not the scene)
    tooltip_ = new QLabel(this);
    tooltip_->setWindowFlags(Qt::ToolTip);
    tooltip_->setStyleSheet(QString("QLabel { background:%1; color:%2; border:1px solid %3;"
                                    " font-size:10px; font-weight:600; padding:3px 6px; border-radius:3px; }")
                                .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED()));
    tooltip_->hide();
}

void CrosshairChartView::set_series_data(const QVector<QPointF>& pts, const QString& currency) {
    pts_ = pts;
    currency_ = currency;
}

void CrosshairChartView::mouseMoveEvent(QMouseEvent* event) {
    QChartView::mouseMoveEvent(event);
    update_crosshair(event->pos());
}

void CrosshairChartView::leaveEvent(QEvent* event) {
    QChartView::leaveEvent(event);
    hide_crosshair();
}

void CrosshairChartView::update_crosshair(const QPoint& widget_pos) {
    if (pts_.isEmpty() || chart()->axes(Qt::Horizontal).isEmpty()) {
        hide_crosshair();
        return;
    }

    // Map widget pixel → chart value
    const QPointF chart_val = chart()->mapToValue(QPointF(widget_pos));

    // Snap to nearest point by x (ms timestamp)
    int best_idx = 0;
    double best_dist = std::numeric_limits<double>::max();
    for (int i = 0; i < pts_.size(); ++i) {
        const double dist = std::abs(pts_[i].x() - chart_val.x());
        if (dist < best_dist) {
            best_dist = dist;
            best_idx = i;
        }
    }

    const QPointF& snap_pt = pts_[best_idx];

    // Map snapped data point back to scene coordinates
    const QPointF scene_pos = chart()->mapToPosition(snap_pt);

    // Draw vertical line spanning the chart plot area
    const QRectF plot = chart()->plotArea();
    v_line_->setLine(scene_pos.x(), plot.top(), scene_pos.x(), plot.bottom());
    v_line_->setVisible(true);

    // Format tooltip text
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(snap_pt.x()));
    const QString date_str = dt.toString("dd MMM yyyy");
    const QString val_str = QString("%1 %2").arg(currency_, QString::number(snap_pt.y(), 'f', 2));
    tooltip_->setText(date_str + "\n" + val_str);
    tooltip_->adjustSize();

    // Position tooltip: above-right of cursor, clamped to screen bounds
    QPoint global_pos = mapToGlobal(widget_pos) + QPoint(12, -tooltip_->height() - 4);
    const QRect scr = QGuiApplication::primaryScreen()->geometry();
    if (global_pos.x() + tooltip_->width() > scr.right())
        global_pos.rx() -= tooltip_->width() + 24;
    if (global_pos.y() < scr.top())
        global_pos.ry() = mapToGlobal(widget_pos).y() + 12;

    tooltip_->move(global_pos);
    tooltip_->show();
    tooltip_->raise();
}

void CrosshairChartView::hide_crosshair() {
    v_line_->setVisible(false);
    tooltip_->hide();
}

// ── PortfolioPerfChart ────────────────────────────────────────────────────────

PortfolioPerfChart::PortfolioPerfChart(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PortfolioPerfChart::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header: PERFORMANCE + period buttons
    auto* header = new QHBoxLayout;
    header->setContentsMargins(10, 6, 10, 4);

    auto* title = new QLabel("PERFORMANCE");
    title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1.5px;").arg(ui::colors::TEXT_SECONDARY()));
    header->addWidget(title);
    header->addStretch();

    for (const auto& p : kPeriods) {
        auto* btn = new QPushButton(p);
        btn->setFixedSize(32, 22);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid transparent;"
                                   "  font-size:9px; font-weight:700; border-radius:2px; }"
                                   "QPushButton:checked { color:%4; background:%2;"
                                   "  border:1px solid %2; }"
                                   "QPushButton:hover:!checked { color:%3; border-color:%5; }")
                               .arg(ui::colors::TEXT_TERTIARY(), ui::colors::AMBER(), ui::colors::TEXT_PRIMARY(),
                                    ui::colors::BG_BASE(), ui::colors::BORDER_MED()));

        if (p == current_period_)
            btn->setChecked(true);

        connect(btn, &QPushButton::clicked, this, [this, period = p]() { set_period(period); });

        header->addWidget(btn);
        period_btns_.append(btn);
    }

    // Benchmark toggle
    benchmark_btn_ = new QPushButton("SPY");
    benchmark_btn_->setFixedSize(34, 22);
    benchmark_btn_->setCheckable(true);
    benchmark_btn_->setCursor(Qt::PointingHandCursor);
    benchmark_btn_->setToolTip("Overlay SPY benchmark (normalised to portfolio start value)");
    benchmark_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                "  font-size:9px; font-weight:700; border-radius:2px; }"
                "QPushButton:checked { color:%4; background:%2; border:1px solid %2; }"
                "QPushButton:hover:!checked { color:%3; border-color:%3; }")
            .arg(ui::colors::CYAN(), ui::colors::CYAN(), ui::colors::TEXT_PRIMARY(), ui::colors::BG_BASE()));
    connect(benchmark_btn_, &QPushButton::clicked, this, [this]() {
        show_benchmark_ = benchmark_btn_->isChecked();
        update_chart();
    });
    header->addSpacing(6);
    header->addWidget(benchmark_btn_);

    layout->addLayout(header);

    // Info bar: period change, total return, NAV
    auto* info_bar = new QHBoxLayout;
    info_bar->setContentsMargins(10, 2, 10, 6);
    info_bar->setSpacing(14);

    period_change_label_ = new QLabel;
    period_change_label_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:600;").arg(ui::colors::TEXT_PRIMARY()));
    info_bar->addWidget(period_change_label_);

    auto* sep1 = new QLabel("|");
    sep1->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::BORDER_MED()));
    info_bar->addWidget(sep1);

    total_return_label_ = new QLabel;
    total_return_label_->setStyleSheet(
        QString("color:%1; font-size:14px; font-weight:700;").arg(ui::colors::TEXT_PRIMARY()));
    info_bar->addWidget(total_return_label_);

    auto* sep2 = new QLabel("|");
    sep2->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::BORDER_MED()));
    info_bar->addWidget(sep2);

    nav_label_ = new QLabel;
    nav_label_->setStyleSheet(QString("color:%1; font-size:11px; font-weight:600;").arg(ui::colors::WARNING()));
    info_bar->addWidget(nav_label_);

    info_bar->addStretch();
    layout->addLayout(info_bar);

    // Chart view
    auto* chart = new QChart;
    chart->setBackgroundBrush(QColor(ui::colors::BG_BASE()));
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->legend()->setVisible(false);
    chart->setAnimationOptions(QChart::NoAnimation);

    chart_view_ = new CrosshairChartView(chart, this);
    chart_view_->setRenderHint(QPainter::Antialiasing);
    chart_view_->setStyleSheet("border:none; background:transparent;");
    layout->addWidget(chart_view_, 1);
}

void PortfolioPerfChart::set_summary(const portfolio::PortfolioSummary& summary) {
    summary_ = summary;
    update_chart();
}

void PortfolioPerfChart::set_snapshots(const QVector<portfolio::PortfolioSnapshot>& snapshots) {
    snapshots_ = snapshots;
    update_chart();
}

void PortfolioPerfChart::set_currency(const QString& currency) {
    currency_ = currency;
}

void PortfolioPerfChart::set_spy_history(const QStringList& dates, const QVector<double>& closes) {
    spy_dates_ = dates;
    spy_closes_ = closes;
    if (show_benchmark_)
        update_chart(); // refresh only if overlay is visible
}

void PortfolioPerfChart::set_period(const QString& period) {
    current_period_ = period;
    for (auto* btn : period_btns_)
        btn->setChecked(btn->text() == period);
    update_chart();
}

QColor PortfolioPerfChart::chart_color() const {
    return summary_.total_unrealized_pnl >= 0 ? QColor(ui::colors::POSITIVE()) : QColor(ui::colors::NEGATIVE());
}

void PortfolioPerfChart::update_chart() {
    auto* chart = chart_view_->chart();
    chart->removeAllSeries();

    const auto old_axes = chart->axes();
    for (auto* axis : old_axes) {
        chart->removeAxis(axis);
        delete axis;
    }

    if (summary_.holdings.isEmpty()) {
        period_change_label_->setText("No data");
        total_return_label_->clear();
        nav_label_->clear();
        chart_view_->set_series_data({}, currency_);
        return;
    }

    // ── Determine cutoff date from selected period ────────────────────────────
    QDate cutoff = QDate::currentDate();
    if (current_period_ == "1D")
        cutoff = cutoff.addDays(-1);
    else if (current_period_ == "1W")
        cutoff = cutoff.addDays(-7);
    else if (current_period_ == "1M")
        cutoff = cutoff.addMonths(-1);
    else if (current_period_ == "3M")
        cutoff = cutoff.addMonths(-3);
    else if (current_period_ == "YTD")
        cutoff = QDate(QDate::currentDate().year(), 1, 1);
    else if (current_period_ == "1Y")
        cutoff = cutoff.addYears(-1);
    else
        cutoff = cutoff.addYears(-10); // ALL

    // ── Filter snapshots by period ────────────────────────────────────────────
    QVector<portfolio::PortfolioSnapshot> filtered;
    for (const auto& s : snapshots_) {
        QDate d = QDate::fromString(s.snapshot_date.left(10), Qt::ISODate);
        if (d.isValid() && d >= cutoff)
            filtered.append(s);
    }

    // Sort ascending by date
    std::sort(filtered.begin(), filtered.end(),
              [](const auto& a, const auto& b) { return a.snapshot_date < b.snapshot_date; });

    // ── Build chart series ────────────────────────────────────────────────────
    auto* line = new QLineSeries;
    auto* area_upper = new QLineSeries;
    auto* area_lower = new QLineSeries;

    double first_val = 0;
    double last_val = summary_.total_market_value;
    double min_val = last_val;
    double max_val = last_val;

    if (filtered.size() >= 2) {
        // Real snapshot data
        first_val = filtered.first().total_value;
        for (const auto& s : filtered) {
            QDateTime dt = QDateTime::fromString(s.snapshot_date.left(10), Qt::ISODate);
            if (!dt.isValid())
                dt = QDateTime::currentDateTime();
            const qint64 ms = dt.toMSecsSinceEpoch();
            line->append(ms, s.total_value);
            area_upper->append(ms, s.total_value);
            area_lower->append(ms, first_val);
            min_val = std::min(min_val, s.total_value);
            max_val = std::max(max_val, s.total_value);
        }
        // Append current live value at now
        const qint64 now_ms = QDateTime::currentDateTime().toMSecsSinceEpoch();
        line->append(now_ms, last_val);
        area_upper->append(now_ms, last_val);
        area_lower->append(now_ms, first_val);
        min_val = std::min(min_val, last_val);
        max_val = std::max(max_val, last_val);
    } else {
        // Insufficient real data — show a single cost→NAV segment and a note.
        // Do NOT synthesise fake data points.
        const double cost = summary_.total_cost_basis > 0 ? summary_.total_cost_basis : last_val;
        first_val = cost;
        const qint64 start_ms = QDateTime::currentDateTime().addDays(-1).toMSecsSinceEpoch();
        const qint64 now_ms = QDateTime::currentDateTime().toMSecsSinceEpoch();
        line->append(start_ms, cost);
        line->append(now_ms, last_val);
        area_upper->append(start_ms, cost);
        area_upper->append(now_ms, last_val);
        area_lower->append(start_ms, cost);
        area_lower->append(now_ms, cost);
        min_val = std::min(cost, last_val);
        max_val = std::max(cost, last_val);

        // Overlay a note inside the chart area
        period_change_label_->setText(QString("%1  — (no history yet, showing NAV only)").arg(current_period_));
        period_change_label_->setStyleSheet(
            QString("color:%1; font-size:11px; font-weight:500;").arg(ui::colors::TEXT_TERTIARY()));
    }

    // ── Period P&L from the series endpoints ─────────────────────────────────
    double period_pnl = last_val - first_val;
    double period_pnl_pct = (first_val > 0) ? (period_pnl / first_val) * 100.0 : 0;

    QColor lc = period_pnl >= 0 ? QColor(ui::colors::POSITIVE()) : QColor(ui::colors::NEGATIVE());
    QPen pen(lc, 2);
    line->setPen(pen);

    auto* area = new QAreaSeries(area_upper, area_lower);
    QColor fill = lc;
    fill.setAlpha(35);
    area->setBrush(fill);
    area->setPen(Qt::NoPen);

    chart->addSeries(area);
    chart->addSeries(line);

    // ── Axes ──────────────────────────────────────────────────────────────────
    auto* x_axis = new QDateTimeAxis;
    if (current_period_ == "1D" || current_period_ == "1W")
        x_axis->setFormat("MMM dd");
    else if (current_period_ == "ALL" || current_period_ == "1Y")
        x_axis->setFormat("MMM yy");
    else
        x_axis->setFormat("dd MMM");
    x_axis->setTickCount(5);
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    x_axis->setGridLineVisible(false);
    x_axis->setMinorGridLineVisible(false);
    x_axis->setLinePen(QPen(QColor(ui::colors::BORDER_DIM()), 1));
    x_axis->setLabelsFont(QFont(ui::fonts::DATA_FAMILY(), 8));

    double padding = std::max((max_val - min_val) * 0.08, max_val * 0.01);
    auto* y_axis = new QValueAxis;
    y_axis->setRange(min_val - padding, max_val + padding);
    y_axis->setLabelFormat("%.0f");
    y_axis->setTickCount(5);
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    {
        QPen grid_pen(QColor(ui::colors::BORDER_DIM()), 1, Qt::DotLine);
        y_axis->setGridLinePen(grid_pen);
    }
    y_axis->setMinorGridLineVisible(false);
    y_axis->setLinePen(QPen(QColor(ui::colors::BORDER_DIM()), 1));
    y_axis->setLabelsFont(QFont(ui::fonts::DATA_FAMILY(), 8));

    chart->addAxis(x_axis, Qt::AlignBottom);
    chart->addAxis(y_axis, Qt::AlignLeft);
    line->attachAxis(x_axis);
    line->attachAxis(y_axis);
    area->attachAxis(x_axis);
    area->attachAxis(y_axis);

    // ── Feed crosshair with series points ────────────────────────────────────
    chart_view_->set_series_data(line->points(), currency_);

    // ── Benchmark overlay (real SPY prices normalised to first_val) ──────────
    if (show_benchmark_ && line->count() >= 2) {
        if (spy_dates_.isEmpty() || spy_closes_.isEmpty()) {
            // Real data not yet available — show a note; do NOT draw fake curve
            nav_label_->setText(nav_label_->text() + QString("  |  SPY: loading…"));
        } else {
            // Filter SPY history to the same date window as the portfolio line
            const QPointF& p0 = line->at(0);
            const QPointF& pN = line->at(line->count() - 1);
            const QDate start_date = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(p0.x())).date();
            const QDate end_date = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(pN.x())).date();

            // Find first SPY close on or after start_date to use as base
            double spy_base = 0.0;
            int base_idx = -1;
            for (int i = 0; i < spy_dates_.size(); ++i) {
                const QDate d = QDate::fromString(spy_dates_[i], Qt::ISODate);
                if (d >= start_date) {
                    spy_base = spy_closes_[i];
                    base_idx = i;
                    break;
                }
            }

            if (base_idx >= 0 && spy_base > 1e-6) {
                auto* spy_line = new QLineSeries;
                double spy_min = first_val, spy_max = first_val;

                for (int i = base_idx; i < spy_dates_.size(); ++i) {
                    const QDate d = QDate::fromString(spy_dates_[i], Qt::ISODate);
                    if (d > end_date)
                        break;
                    const qint64 ms = QDateTime(d, QTime(), QTimeZone::UTC).toMSecsSinceEpoch();
                    const double val = first_val * (spy_closes_[i] / spy_base);
                    spy_line->append(ms, val);
                    spy_min = std::min(spy_min, val);
                    spy_max = std::max(spy_max, val);
                }

                if (spy_line->count() >= 2) {
                    QPen spy_pen(QColor(ui::colors::CYAN()), 1, Qt::DashLine);
                    spy_line->setPen(spy_pen);
                    spy_line->setName("SPY");
                    chart->addSeries(spy_line);
                    spy_line->attachAxis(x_axis);
                    spy_line->attachAxis(y_axis);

                    // Extend y-axis if SPY goes outside portfolio range
                    const double new_min = std::min(min_val, spy_min);
                    const double new_max = std::max(max_val, spy_max);
                    const double new_pad = std::max((new_max - new_min) * 0.08, new_max * 0.01);
                    y_axis->setRange(new_min - new_pad, new_max + new_pad);
                } else {
                    delete spy_line;
                }
            }
        }
    }

    // ── Info labels ───────────────────────────────────────────────────────────
    const char* pnl_color = period_pnl_pct >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;

    period_change_label_->setText(QString("%1  %2%3%")
                                      .arg(current_period_)
                                      .arg(period_pnl_pct >= 0 ? "+" : "")
                                      .arg(QString::number(period_pnl_pct, 'f', 2)));
    period_change_label_->setStyleSheet(QString("color:%1; font-size:11px; font-weight:600;").arg(pnl_color));

    double total_pnl_pct = summary_.total_unrealized_pnl_percent;
    const char* total_color = total_pnl_pct >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
    total_return_label_->setText(
        QString("TOTAL  %1%2%").arg(total_pnl_pct >= 0 ? "+" : "").arg(QString::number(total_pnl_pct, 'f', 2)));
    total_return_label_->setStyleSheet(QString("color:%1; font-size:14px; font-weight:700;").arg(total_color));

    nav_label_->setText(QString("NAV %1 %2").arg(currency_).arg(QString::number(last_val, 'f', 2)));
}

void PortfolioPerfChart::refresh_theme() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));

    const QString bsz = QString::number(ui::fonts::font_px(-3));
    // Re-style period buttons
    for (auto* btn : period_btns_) {
        btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid transparent;"
                                   "  font-size:" +
                                   bsz +
                                   "px; font-weight:700; border-radius:2px; }"
                                   "QPushButton:checked { color:%4; background:%2;"
                                   "  border:1px solid %2; }"
                                   "QPushButton:hover:!checked { color:%3; border-color:%5; }")
                               .arg(ui::colors::TEXT_TERTIARY(), ui::colors::AMBER(), ui::colors::TEXT_PRIMARY(),
                                    ui::colors::BG_BASE(), ui::colors::BORDER_MED()));
    }
    if (benchmark_btn_) {
        benchmark_btn_->setStyleSheet(
            QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                    "  font-size:" +
                    bsz +
                    "px; font-weight:700; border-radius:2px; }"
                    "QPushButton:checked { color:%4; background:%2; border:1px solid %2; }"
                    "QPushButton:hover:!checked { color:%3; border-color:%3; }")
                .arg(ui::colors::CYAN(), ui::colors::CYAN(), ui::colors::TEXT_PRIMARY(), ui::colors::BG_BASE()));
    }

    // Chart background
    if (chart_view_ && chart_view_->chart())
        chart_view_->chart()->setBackgroundBrush(QColor(ui::colors::BG_BASE()));

    // Rebuild chart with current theme colors (axes, line colors, etc.)
    update_chart();
}

} // namespace fincept::screens
