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

    // Benchmark toggle (label updates when set_benchmark_history fires with a
    // non-SPY symbol — e.g. ^GSPTSE for CAD portfolios).
    benchmark_btn_ = new QPushButton(benchmark_symbol_);
    benchmark_btn_->setFixedSize(60, 22);
    benchmark_btn_->setCheckable(true);
    benchmark_btn_->setCursor(Qt::PointingHandCursor);
    benchmark_btn_->setToolTip("Overlay benchmark index (auto-selected by portfolio currency)");
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

    // Indexed-mode toggle: switches the y-axis from currency value to
    // percent-indexed (base 100). Required for fair benchmark comparison
    // when the portfolio and benchmark are in different currencies — without
    // this, comparing CAD NAV against USD SPY closes is meaningless.
    indexed_btn_ = new QPushButton("%");
    indexed_btn_->setFixedSize(28, 22);
    indexed_btn_->setCheckable(true);
    indexed_btn_->setCursor(Qt::PointingHandCursor);
    indexed_btn_->setToolTip("Indexed view: rebase portfolio and benchmark to 100 at the start of\n"
                             "the selected period. Use when comparing different currencies.");
    indexed_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                "  font-size:9px; font-weight:700; border-radius:2px; }"
                "QPushButton:checked { color:%4; background:%2; border:1px solid %2; }"
                "QPushButton:hover:!checked { color:%3; border-color:%3; }")
            .arg(ui::colors::TEXT_TERTIARY(), ui::colors::AMBER(), ui::colors::TEXT_PRIMARY(),
                 ui::colors::BG_BASE()));
    connect(indexed_btn_, &QPushButton::clicked, this, [this]() {
        indexed_mode_ = indexed_btn_->isChecked();
        update_chart();
    });
    header->addSpacing(4);
    header->addWidget(indexed_btn_);

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

    auto* sep3 = new QLabel("|");
    sep3->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::BORDER_MED()));
    info_bar->addWidget(sep3);

    cost_basis_label_ = new QLabel;
    cost_basis_label_->setStyleSheet(
        QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY()));
    cost_basis_label_->setToolTip("Total cost basis — the dashed horizontal line on the chart.");
    info_bar->addWidget(cost_basis_label_);

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
    // Sort once so iso_date_to_ms_utc downstream produces a monotonic series.
    std::sort(snapshots_.begin(), snapshots_.end(),
              [](const auto& a, const auto& b) { return a.snapshot_date < b.snapshot_date; });
    update_period_buttons_enabled();
    update_chart();
}

void PortfolioPerfChart::set_currency(const QString& currency) {
    currency_ = currency;
}

void PortfolioPerfChart::set_spy_history(const QStringList& dates, const QVector<double>& closes) {
    set_benchmark_history(QStringLiteral("SPY"), dates, closes);
}

void PortfolioPerfChart::set_benchmark_history(const QString& symbol, const QStringList& dates,
                                               const QVector<double>& closes) {
    benchmark_symbol_ = symbol.isEmpty() ? QStringLiteral("SPY") : symbol;
    spy_dates_ = dates;   // field name kept for back-compat; holds chosen benchmark
    spy_closes_ = closes;
    if (benchmark_btn_)
        benchmark_btn_->setText(benchmark_symbol_);
    if (show_benchmark_)
        update_chart();
}

void PortfolioPerfChart::set_period(const QString& period) {
    const QString prev = current_period_;
    current_period_ = period;
    for (auto* btn : period_btns_)
        btn->setChecked(btn->text() == period);

    // If the user is asking for a longer window than we have snapshots for,
    // request a backfill. The owner re-feeds set_snapshots when it lands.
    if (prev != period && !snapshots_.isEmpty()) {
        const QDate earliest = QDate::fromString(snapshots_.first().snapshot_date.left(10), Qt::ISODate);
        const QDate today = QDate::currentDate();
        const int needed_days = (period == "5Y" || period == "ALL") ? 1825
                              : (period == "1Y" || period == "YTD") ? 365
                              : (period == "3M") ? 95
                              : (period == "1M") ? 32
                              : 10;
        if (earliest.isValid() && earliest.daysTo(today) < needed_days - 5) {
            const QString backfill_period =
                (period == "5Y" || period == "ALL") ? "5y" : "1y";
            emit backfill_period_requested(backfill_period);
        }
    }
    update_chart();
}

qint64 PortfolioPerfChart::iso_date_to_ms_utc(const QString& iso_date) {
    const QDate d = QDate::fromString(iso_date.left(10), Qt::ISODate);
    if (!d.isValid())
        return QDateTime::currentMSecsSinceEpoch();
    return QDateTime(d, QTime(0, 0), QTimeZone::UTC).toMSecsSinceEpoch();
}

void PortfolioPerfChart::update_period_buttons_enabled() {
    if (snapshots_.isEmpty())
        return;
    const QDate earliest = QDate::fromString(snapshots_.first().snapshot_date.left(10), Qt::ISODate);
    const QDate today = QDate::currentDate();
    const int span_days = earliest.isValid() ? earliest.daysTo(today) : 0;
    for (auto* btn : period_btns_) {
        const QString p = btn->text();
        // Daily snapshots only — 1D needs intraday data we don't capture.
        bool feasible = true;
        if (p == "1D")
            feasible = false; // explicitly unsupported with daily snapshots
        else if (p == "1W")
            feasible = span_days >= 5;
        // Other periods are always allowed: backfill kicks in when clicked.
        btn->setEnabled(feasible);
        btn->setToolTip(feasible ? QString()
                                 : QStringLiteral("Needs intraday data — daily snapshots only."));
    }
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
        if (cost_basis_label_)
            cost_basis_label_->clear();
        chart_view_->set_series_data({}, currency_);
        return;
    }

    const double live_nav = summary_.total_market_value;
    const double cost_basis = summary_.total_cost_basis;

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
    else if (current_period_ == "5Y")
        cutoff = cutoff.addYears(-5);
    else
        cutoff = cutoff.addYears(-10); // ALL

    // ── Filter snapshots (already sorted ascending by set_snapshots) ─────────
    QVector<portfolio::PortfolioSnapshot> filtered;
    filtered.reserve(snapshots_.size());
    for (const auto& s : snapshots_) {
        const QDate d = QDate::fromString(s.snapshot_date.left(10), Qt::ISODate);
        if (d.isValid() && d >= cutoff)
            filtered.append(s);
    }

    // ── Build raw NAV series in absolute currency value ──────────────────────
    QVector<QPointF> nav_pts;       // (ms_utc, NAV)
    nav_pts.reserve(filtered.size() + 1);
    double period_baseline = 0; // first snapshot value within window

    if (filtered.size() >= 2) {
        period_baseline = filtered.first().total_value;
        for (const auto& s : filtered)
            nav_pts.append(QPointF(static_cast<double>(iso_date_to_ms_utc(s.snapshot_date)),
                                   s.total_value));
        // Pin a final point at "now" so the line meets the live NAV.
        const qint64 now_ms = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
        if (nav_pts.last().x() < now_ms)
            nav_pts.append(QPointF(static_cast<double>(now_ms), live_nav));
    } else {
        // No real history yet — render a clean cost→NAV reference segment.
        // (Backfill is auto-triggered on import / first metrics call.)
        period_baseline = cost_basis > 0 ? cost_basis : live_nav;
        const qint64 yest = QDateTime::currentDateTimeUtc().addDays(-1).toMSecsSinceEpoch();
        const qint64 now_ms = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
        nav_pts.append(QPointF(static_cast<double>(yest), period_baseline));
        nav_pts.append(QPointF(static_cast<double>(now_ms), live_nav));
    }

    // ── Period P&L is *always* relative to the period baseline ───────────────
    const double period_pnl = live_nav - period_baseline;
    const double period_pnl_pct = period_baseline > 0 ? (period_pnl / period_baseline) * 100.0 : 0.0;

    // ── Convert series to display space (currency value vs. base-100 indexed) ─
    auto to_display = [&](double v) -> double {
        if (!indexed_mode_)
            return v;
        return period_baseline > 0 ? (v / period_baseline) * 100.0 : 0.0;
    };

    auto* nav_line = new QLineSeries;
    auto* area_upper = new QLineSeries;
    auto* area_lower = new QLineSeries;

    double y_min = std::numeric_limits<double>::max();
    double y_max = std::numeric_limits<double>::lowest();
    const double area_baseline_disp = to_display(period_baseline);

    for (const auto& p : nav_pts) {
        const double y = to_display(p.y());
        nav_line->append(p.x(), y);
        area_upper->append(p.x(), y);
        area_lower->append(p.x(), area_baseline_disp);
        y_min = std::min(y_min, y);
        y_max = std::max(y_max, y);
    }

    // Y-axis must always include the cost-basis reference so the user can see
    // the gap between NAV and cost. Skip in indexed mode where 100 is the line.
    const double cost_disp = to_display(cost_basis);
    if (cost_basis > 0) {
        y_min = std::min(y_min, cost_disp);
        y_max = std::max(y_max, cost_disp);
    }
    y_min = std::min(y_min, area_baseline_disp);
    y_max = std::max(y_max, area_baseline_disp);

    QColor lc = period_pnl >= 0 ? QColor(ui::colors::POSITIVE()) : QColor(ui::colors::NEGATIVE());
    nav_line->setPen(QPen(lc, 2));

    auto* area = new QAreaSeries(area_upper, area_lower);
    QColor fill = lc;
    fill.setAlpha(35);
    area->setBrush(fill);
    area->setPen(Qt::NoPen);

    chart->addSeries(area);
    chart->addSeries(nav_line);

    // ── Cost basis reference line ────────────────────────────────────────────
    // Skipped in indexed mode (where the period baseline IS the reference at 100).
    QLineSeries* cost_line = nullptr;
    if (cost_basis > 0 && !indexed_mode_) {
        cost_line = new QLineSeries;
        cost_line->setName("Cost basis");
        cost_line->append(nav_pts.first().x(), cost_disp);
        cost_line->append(nav_pts.last().x(), cost_disp);
        QPen cp(QColor(ui::colors::TEXT_TERTIARY()), 1, Qt::DashLine);
        cost_line->setPen(cp);
        chart->addSeries(cost_line);
    }

    // ── Axes ──────────────────────────────────────────────────────────────────
    auto* x_axis = new QDateTimeAxis;
    if (current_period_ == "1D" || current_period_ == "1W")
        x_axis->setFormat("MMM dd");
    else if (current_period_ == "ALL" || current_period_ == "5Y" || current_period_ == "1Y")
        x_axis->setFormat("MMM yy");
    else
        x_axis->setFormat("dd MMM");
    x_axis->setTickCount(5);
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    x_axis->setGridLineVisible(false);
    x_axis->setMinorGridLineVisible(false);
    x_axis->setLinePen(QPen(QColor(ui::colors::BORDER_DIM()), 1));
    x_axis->setLabelsFont(QFont(ui::fonts::DATA_FAMILY(), 8));

    auto* y_axis = new QValueAxis;
    const double padding = std::max((y_max - y_min) * 0.08, std::abs(y_max) * 0.01);
    y_axis->setRange(y_min - padding, y_max + padding);
    y_axis->setLabelFormat(indexed_mode_ ? "%.1f" : "%.0f");
    y_axis->setTickCount(5);
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    y_axis->setGridLinePen(QPen(QColor(ui::colors::BORDER_DIM()), 1, Qt::DotLine));
    y_axis->setMinorGridLineVisible(false);
    y_axis->setLinePen(QPen(QColor(ui::colors::BORDER_DIM()), 1));
    y_axis->setLabelsFont(QFont(ui::fonts::DATA_FAMILY(), 8));

    chart->addAxis(x_axis, Qt::AlignBottom);
    chart->addAxis(y_axis, Qt::AlignLeft);
    nav_line->attachAxis(x_axis);
    nav_line->attachAxis(y_axis);
    area->attachAxis(x_axis);
    area->attachAxis(y_axis);
    if (cost_line) {
        cost_line->attachAxis(x_axis);
        cost_line->attachAxis(y_axis);
    }

    chart_view_->set_series_data(nav_line->points(), indexed_mode_ ? QStringLiteral("idx") : currency_);

    // ── Benchmark overlay ─────────────────────────────────────────────────────
    if (show_benchmark_ && nav_line->count() >= 2) {
        if (spy_dates_.isEmpty() || spy_closes_.isEmpty()) {
            nav_label_->setText(nav_label_->text() +
                                QString("  |  %1: loading…").arg(benchmark_symbol_));
        } else {
            const QDate start_date = QDateTime::fromMSecsSinceEpoch(
                static_cast<qint64>(nav_line->at(0).x()), QTimeZone::UTC).date();
            const QDate end_date = QDateTime::fromMSecsSinceEpoch(
                static_cast<qint64>(nav_line->at(nav_line->count() - 1).x()), QTimeZone::UTC).date();

            // First benchmark close on/after period start = base for normalisation.
            double bench_base = 0.0;
            int base_idx = -1;
            for (int i = 0; i < spy_dates_.size(); ++i) {
                const QDate d = QDate::fromString(spy_dates_[i], Qt::ISODate);
                if (d >= start_date) {
                    bench_base = spy_closes_[i];
                    base_idx = i;
                    break;
                }
            }

            if (base_idx >= 0 && bench_base > 1e-6) {
                auto* bench_line = new QLineSeries;
                double b_min = std::numeric_limits<double>::max();
                double b_max = std::numeric_limits<double>::lowest();
                for (int i = base_idx; i < spy_dates_.size(); ++i) {
                    const QDate d = QDate::fromString(spy_dates_[i], Qt::ISODate);
                    if (d > end_date)
                        break;
                    const qint64 ms = iso_date_to_ms_utc(spy_dates_[i]);
                    // In currency mode, scale benchmark to start at the
                    // portfolio's period baseline so they share the y-axis
                    // visually. In indexed mode, base-100 normalisation puts
                    // them on the same dimensionless scale.
                    const double bench_disp = indexed_mode_
                        ? (spy_closes_[i] / bench_base) * 100.0
                        : period_baseline * (spy_closes_[i] / bench_base);
                    bench_line->append(ms, bench_disp);
                    b_min = std::min(b_min, bench_disp);
                    b_max = std::max(b_max, bench_disp);
                }

                if (bench_line->count() >= 2) {
                    QPen bp(QColor(ui::colors::CYAN()), 1, Qt::DashLine);
                    bench_line->setPen(bp);
                    bench_line->setName(benchmark_symbol_);
                    chart->addSeries(bench_line);
                    bench_line->attachAxis(x_axis);
                    bench_line->attachAxis(y_axis);
                    const double new_min = std::min(y_min, b_min);
                    const double new_max = std::max(y_max, b_max);
                    const double new_pad = std::max((new_max - new_min) * 0.08, std::abs(new_max) * 0.01);
                    y_axis->setRange(new_min - new_pad, new_max + new_pad);
                } else {
                    delete bench_line;
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
    period_change_label_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:600;").arg(pnl_color));

    const double total_pnl_pct = summary_.total_unrealized_pnl_percent;
    const char* total_color = total_pnl_pct >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
    total_return_label_->setText(
        QString("TOTAL  %1%2%").arg(total_pnl_pct >= 0 ? "+" : "").arg(QString::number(total_pnl_pct, 'f', 2)));
    total_return_label_->setStyleSheet(
        QString("color:%1; font-size:14px; font-weight:700;").arg(total_color));

    nav_label_->setText(QString("NAV %1 %2").arg(currency_).arg(QString::number(live_nav, 'f', 2)));
    if (cost_basis_label_) {
        if (cost_basis > 0)
            cost_basis_label_->setText(
                QString("COST %1 %2").arg(currency_).arg(QString::number(cost_basis, 'f', 2)));
        else
            cost_basis_label_->clear();
    }
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
