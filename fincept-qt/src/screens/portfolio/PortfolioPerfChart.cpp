// src/screens/portfolio/PortfolioPerfChart.cpp
#include "screens/portfolio/PortfolioPerfChart.h"

#include "ui/theme/Theme.h"

#include <QAreaSeries>
#include <QChart>
#include <QDateTimeAxis>
#include <QHBoxLayout>
#include <QLineSeries>
#include <QVBoxLayout>
#include <QValueAxis>

#include <cmath>

namespace {
static const QStringList kPeriods = {"1D", "1W", "1M", "3M", "YTD", "1Y", "ALL"};
} // namespace

namespace fincept::screens {

PortfolioPerfChart::PortfolioPerfChart(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PortfolioPerfChart::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header: PERFORMANCE + period buttons
    auto* header = new QHBoxLayout;
    header->setContentsMargins(8, 4, 8, 4);

    auto* title = new QLabel("PERFORMANCE");
    title->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:700; letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY));
    header->addWidget(title);
    header->addStretch();

    for (const auto& p : kPeriods) {
        auto* btn = new QPushButton(p);
        btn->setFixedSize(28, 18);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:none;"
                                   "  font-size:8px; font-weight:700; }"
                                   "QPushButton:checked { color:%2; border-bottom:1px solid %2; }"
                                   "QPushButton:hover { color:%3; }")
                               .arg(ui::colors::TEXT_TERTIARY, ui::colors::AMBER, ui::colors::TEXT_PRIMARY));

        if (p == current_period_)
            btn->setChecked(true);

        connect(btn, &QPushButton::clicked, this, [this, period = p]() { set_period(period); });

        header->addWidget(btn);
        period_btns_.append(btn);
    }

    layout->addLayout(header);

    // Info bar: period change, total return, NAV
    auto* info_bar = new QHBoxLayout;
    info_bar->setContentsMargins(8, 0, 8, 4);
    info_bar->setSpacing(12);

    period_change_label_ = new QLabel;
    period_change_label_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:600;").arg(ui::colors::TEXT_PRIMARY));
    info_bar->addWidget(period_change_label_);

    auto* sep1 = new QLabel("|");
    sep1->setStyleSheet(QString("color:%1;").arg(ui::colors::BORDER_MED));
    info_bar->addWidget(sep1);

    total_return_label_ = new QLabel;
    total_return_label_->setStyleSheet(
        QString("color:%1; font-size:14px; font-weight:700;").arg(ui::colors::TEXT_PRIMARY));
    info_bar->addWidget(total_return_label_);

    auto* sep2 = new QLabel("|");
    sep2->setStyleSheet(QString("color:%1;").arg(ui::colors::BORDER_MED));
    info_bar->addWidget(sep2);

    nav_label_ = new QLabel;
    nav_label_->setStyleSheet(QString("color:%1; font-size:11px; font-weight:600;").arg(ui::colors::WARNING));
    info_bar->addWidget(nav_label_);

    info_bar->addStretch();
    layout->addLayout(info_bar);

    // Chart view
    auto* chart = new QChart;
    chart->setBackgroundBrush(QColor(ui::colors::BG_BASE));
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->legend()->setVisible(false);
    chart->setAnimationOptions(QChart::NoAnimation);

    chart_view_ = new QChartView(chart);
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

void PortfolioPerfChart::set_period(const QString& period) {
    current_period_ = period;
    for (auto* btn : period_btns_)
        btn->setChecked(btn->text() == period);
    update_chart();
}

QColor PortfolioPerfChart::chart_color() const {
    return summary_.total_unrealized_pnl >= 0 ? QColor(ui::colors::POSITIVE) : QColor(ui::colors::NEGATIVE);
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
        return;
    }

    // ── Determine cutoff date from selected period ────────────────────────────
    QDate cutoff = QDate::currentDate();
    if (current_period_ == "1D")       cutoff = cutoff.addDays(-1);
    else if (current_period_ == "1W")  cutoff = cutoff.addDays(-7);
    else if (current_period_ == "1M")  cutoff = cutoff.addMonths(-1);
    else if (current_period_ == "3M")  cutoff = cutoff.addMonths(-3);
    else if (current_period_ == "YTD") cutoff = QDate(QDate::currentDate().year(), 1, 1);
    else if (current_period_ == "1Y")  cutoff = cutoff.addYears(-1);
    else                               cutoff = cutoff.addYears(-10); // ALL

    // ── Filter snapshots by period ────────────────────────────────────────────
    QVector<portfolio::PortfolioSnapshot> filtered;
    for (const auto& s : snapshots_) {
        QDate d = QDate::fromString(s.snapshot_date.left(10), Qt::ISODate);
        if (d.isValid() && d >= cutoff)
            filtered.append(s);
    }

    // Sort ascending by date
    std::sort(filtered.begin(), filtered.end(), [](const auto& a, const auto& b) {
        return a.snapshot_date < b.snapshot_date;
    });

    // ── Build chart series ────────────────────────────────────────────────────
    auto* line       = new QLineSeries;
    auto* area_upper = new QLineSeries;
    auto* area_lower = new QLineSeries;

    double first_val = 0;
    double last_val  = summary_.total_market_value;
    double min_val   = last_val;
    double max_val   = last_val;

    if (filtered.size() >= 2) {
        // Real snapshot data
        first_val = filtered.first().total_value;
        for (const auto& s : filtered) {
            QDateTime dt = QDateTime::fromString(s.snapshot_date.left(10), Qt::ISODate);
            if (!dt.isValid())
                dt = QDateTime::currentDateTime();
            qint64 ms = dt.toMSecsSinceEpoch();
            line->append(ms, s.total_value);
            area_upper->append(ms, s.total_value);
            area_lower->append(ms, first_val); // baseline = starting value
            min_val = std::min(min_val, s.total_value);
            max_val = std::max(max_val, s.total_value);
        }
        // Append current live value at now
        qint64 now_ms = QDateTime::currentDateTime().toMSecsSinceEpoch();
        line->append(now_ms, last_val);
        area_upper->append(now_ms, last_val);
        area_lower->append(now_ms, first_val);
        min_val = std::min(min_val, last_val);
        max_val = std::max(max_val, last_val);
    } else {
        // Fallback: synthesise from cost → current over period days
        double cost    = summary_.total_cost_basis;
        first_val      = cost;
        int days       = cutoff.daysTo(QDate::currentDate());
        if (days < 2) days = 30;
        int pts = std::min(days, 60);
        QDateTime now  = QDateTime::currentDateTime();
        for (int i = 0; i < pts; ++i) {
            double t   = static_cast<double>(i) / (pts - 1);
            double val = cost + (last_val - cost) * t;
            double noise = (i % 5 == 0) ? -0.004 : (i % 5 == 2) ? 0.003 : 0;
            val *= (1.0 + noise);
            qint64 ms  = now.addDays(-(pts - 1 - i)).toMSecsSinceEpoch();
            line->append(ms, val);
            area_upper->append(ms, val);
            area_lower->append(ms, cost);
            min_val = std::min(min_val, val);
            max_val = std::max(max_val, val);
        }
    }

    // ── Period P&L from the series endpoints ─────────────────────────────────
    double period_pnl     = last_val - first_val;
    double period_pnl_pct = (first_val > 0) ? (period_pnl / first_val) * 100.0 : 0;

    QColor lc = period_pnl >= 0 ? QColor(ui::colors::POSITIVE) : QColor(ui::colors::NEGATIVE);
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
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    x_axis->setLinePen(QPen(QColor(ui::colors::BORDER_DIM)));
    x_axis->setLabelsFont(QFont("monospace", 7));

    double padding = std::max((max_val - min_val) * 0.08, max_val * 0.01);
    auto* y_axis = new QValueAxis;
    y_axis->setRange(min_val - padding, max_val + padding);
    y_axis->setLabelFormat("%.0f");
    y_axis->setTickCount(4);
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    y_axis->setLinePen(QPen(QColor(ui::colors::BORDER_DIM)));
    y_axis->setLabelsFont(QFont("monospace", 7));

    chart->addAxis(x_axis, Qt::AlignBottom);
    chart->addAxis(y_axis, Qt::AlignLeft);
    line->attachAxis(x_axis);
    line->attachAxis(y_axis);
    area->attachAxis(x_axis);
    area->attachAxis(y_axis);

    // ── Info labels ───────────────────────────────────────────────────────────
    const char* pnl_color = period_pnl_pct >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;

    period_change_label_->setText(
        QString("%1  %2%3%")
            .arg(current_period_)
            .arg(period_pnl_pct >= 0 ? "+" : "")
            .arg(QString::number(period_pnl_pct, 'f', 2)));
    period_change_label_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:600;").arg(pnl_color));

    double total_pnl_pct = summary_.total_unrealized_pnl_percent;
    const char* total_color = total_pnl_pct >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
    total_return_label_->setText(
        QString("TOTAL  %1%2%")
            .arg(total_pnl_pct >= 0 ? "+" : "")
            .arg(QString::number(total_pnl_pct, 'f', 2)));
    total_return_label_->setStyleSheet(
        QString("color:%1; font-size:14px; font-weight:700;").arg(total_color));

    nav_label_->setText(
        QString("NAV %1 %2").arg(currency_).arg(QString::number(last_val, 'f', 2)));
}

} // namespace fincept::screens
