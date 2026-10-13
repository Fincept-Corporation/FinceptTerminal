// src/screens/portfolio/views/PerformanceRiskView.cpp
#include "screens/portfolio/views/PerformanceRiskView.h"

#include "screens/portfolio/PortfolioCsv.h"
#include "ui/theme/Theme.h"

#include <QAreaSeries>
#include <QChart>
#include <QDate>
#include <QDateTimeAxis>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLineSeries>
#include <QMessageBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QValueAxis>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace {
static const QStringList kPeriods = {"1M", "3M", "6M", "1Y", "ALL"};
} // namespace

namespace fincept::screens {

PerformanceRiskView::PerformanceRiskView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PerformanceRiskView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Period selector ───────────────────────────────────────────────────────
    auto* period_bar = new QHBoxLayout;
    period_bar->setContentsMargins(12, 6, 12, 6);

    chart_title_ = new QLabel(tr("NAV PERFORMANCE (FROM SNAPSHOTS)"));
    chart_title_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    period_bar->addWidget(chart_title_);
    period_bar->addStretch();

    for (const auto& p : kPeriods) {
        auto* btn = new QPushButton(p);
        btn->setFixedSize(32, 20);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:none;"
                                   "  font-size:9px; font-weight:700; }"
                                   "QPushButton:checked { color:%2; border-bottom:2px solid %2; }"
                                   "QPushButton:hover { color:%3; }")
                               .arg(ui::colors::TEXT_TERTIARY(), ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));
        if (p == current_period_)
            btn->setChecked(true);
        connect(btn, &QPushButton::clicked, this, [this, period = p]() { set_period(period); });
        period_bar->addWidget(btn);
        period_btns_.append(btn);
    }

    // Flow-adjusted (TWR) series → CSV: date, NAV, net flow, period and
    // cumulative return.
    export_btn_ = new QPushButton(tr("EXPORT CSV"));
    export_btn_->setFixedHeight(20);
    export_btn_->setCursor(Qt::PointingHandCursor);
    export_btn_->setToolTip(tr("Export the time-weighted return series (date, NAV, net flow, period and "
                               "cumulative return) as CSV"));
    export_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                       "  font-size:9px; font-weight:700; padding:0 8px; }"
                                       "QPushButton:hover { color:%3; border-color:%3; }")
                                   .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::AMBER()));
    connect(export_btn_, &QPushButton::clicked, this, &PerformanceRiskView::export_csv);
    period_bar->addSpacing(8);
    period_bar->addWidget(export_btn_);

    layout->addLayout(period_bar);

    // ── Chart ─────────────────────────────────────────────────────────────────
    auto* chart = new QChart;
    chart->setBackgroundBrush(QColor(ui::colors::BG_BASE()));
    chart->setMargins(QMargins(4, 4, 4, 4));
    chart->legend()->setVisible(false);
    chart->setAnimationOptions(QChart::NoAnimation);

    chart_view_ = new QChartView(chart);
    chart_view_->setRenderHint(QPainter::Antialiasing);
    chart_view_->setStyleSheet("border:none; background:transparent;");
    layout->addWidget(chart_view_, 5);

    // Separator
    auto* sep = new QWidget(this);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    layout->addWidget(sep);

    // ── Risk metric cards ──────────────────────────────────────────────────────
    metrics_header_ = new QLabel(tr("  RISK METRICS"));
    metrics_header_->setFixedHeight(24);
    metrics_header_->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700;"
                                           "letter-spacing:1px; background:%2;")
                                       .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE()));
    layout->addWidget(metrics_header_);

    // What the numbers are: TWR window, risk-free rate and its source, and
    // whether vol/beta/VaR are ex-ante (current weights) or realised.
    rf_hint_ = new QLabel;
    rf_hint_->setWordWrap(true);
    rf_hint_->setStyleSheet(
        QString("color:%1; font-size:9px; padding:2px 12px; background:transparent;").arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(rf_hint_);

    auto* cards_layout = new QGridLayout;
    cards_layout->setContentsMargins(12, 8, 12, 8);
    cards_layout->setSpacing(8);

    sharpe_card_ =
        add_metric_card(cards_layout, tr("SHARPE RATIO"), tr("Risk-adjusted return (annualised)"), ui::colors::CYAN);
    sortino_card_ =
        add_metric_card(cards_layout, tr("SORTINO RATIO"), tr("Downside risk-adjusted return"), ui::colors::CYAN);
    beta_card_ =
        add_metric_card(cards_layout, tr("BETA"), tr("Sensitivity vs the benchmark"), ui::colors::WARNING);
    alpha_card_ =
        add_metric_card(cards_layout, tr("ALPHA"), tr("Jensen's alpha vs benchmark (CAPM)"), ui::colors::POSITIVE);
    vol_card_ = add_metric_card(cards_layout, tr("VOLATILITY"), tr("Annualised from daily returns"), ui::colors::AMBER);
    drawdown_card_ =
        add_metric_card(cards_layout, tr("MAX DRAWDOWN"), tr("Peak-to-trough, time-weighted"), ui::colors::NEGATIVE);
    var_card_ =
        add_metric_card(cards_layout, tr("VALUE AT RISK (95%)"), tr("1-day historical VaR"), ui::colors::NEGATIVE);
    cvar_card_ =
        add_metric_card(cards_layout, tr("CONDITIONAL VaR"), tr("Expected shortfall (95%)"), ui::colors::NEGATIVE);

    auto* cards_widget = new QWidget(this);
    cards_widget->setLayout(cards_layout);
    layout->addWidget(cards_widget, 3);
}

PerformanceRiskView::MetricCard PerformanceRiskView::add_metric_card(QLayout* parent_layout, const QString& title,
                                                                     const QString& desc, const char* color) {

    auto* card = new QWidget(this);
    card->setStyleSheet(QString("background:%1; border:1px solid %2; padding:8px;")
                            .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));

    auto* cl = new QVBoxLayout(card);
    cl->setContentsMargins(10, 8, 10, 8);
    cl->setSpacing(2);

    MetricCard mc;

    mc.title = new QLabel(title);
    mc.title->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700;"
                                    "letter-spacing:0.5px; border:none;")
                                .arg(ui::colors::TEXT_TERTIARY()));
    cl->addWidget(mc.title);

    mc.value = new QLabel("--");
    mc.value->setStyleSheet(QString("color:%1; font-size:18px; font-weight:700; border:none;").arg(color));
    cl->addWidget(mc.value);

    mc.desc = new QLabel(desc);
    mc.desc->setStyleSheet(QString("color:%1; font-size:8px; border:none;").arg(ui::colors::TEXT_TERTIARY()));
    cl->addWidget(mc.desc);

    auto* grid = static_cast<QGridLayout*>(parent_layout);
    int count = grid->count();
    grid->addWidget(card, count / 4, count % 4);

    return mc;
}

void PerformanceRiskView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    has_data_ = true;
    update_chart();
    update_metrics();
}

void PerformanceRiskView::set_snapshots(const QVector<portfolio::PortfolioSnapshot>& snapshots) {
    snapshots_ = snapshots;
    has_data_ = true;
    update_chart();
    update_metrics();
}

void PerformanceRiskView::set_metrics(const portfolio::ComputedMetrics& metrics) {
    metrics_ = metrics;
    update_metrics();
}

void PerformanceRiskView::update_rf_hint() {
    if (!rf_hint_)
        return;
    QString text = tr("Sharpe / Sortino / alpha / max drawdown: realised time-weighted returns (buys and sells are "
                      "treated as capital flows, not performance)");
    if (metrics_.perf_observations > 0)
        text += tr(", %1 periods %2 \u2192 %3").arg(metrics_.perf_observations).arg(metrics_.perf_start, metrics_.perf_end);
    if (metrics_.perf_excluded > 0)
        text += tr(" (%1 implausible period(s) excluded \u2014 likely unrecorded deposits/withdrawals)")
                    .arg(metrics_.perf_excluded);
    text += tr(". Risk-free %1% \u2014 %2.")
                .arg(QString::number(metrics_.risk_free_rate * 100.0, 'f', 2), metrics_.risk_free_source);
    text += metrics_.risk_ex_ante
                ? tr(" Volatility / beta / VaR: ex-ante, current weights \u00d7 %1 trailing daily returns.")
                      .arg(metrics_.risk_observations)
                : tr(" Volatility / VaR: realised series (price history not loaded yet).");
    rf_hint_->setText(text);
}

void PerformanceRiskView::export_csv() {
    if (metrics_.twr_series.isEmpty()) {
        QMessageBox::information(this, tr("Export CSV"), tr("No performance history to export yet."));
        return;
    }
    QVector<QStringList> rows;
    rows.reserve(metrics_.twr_series.size());
    for (const auto& p : metrics_.twr_series) {
        rows.append({p.date, portfolio_csv::num(p.nav, 2), portfolio_csv::num(p.net_flow, 2),
                     p.valid ? portfolio_csv::num(p.period_return * 100.0, 4) : QString(),
                     portfolio_csv::num(p.cumulative * 100.0, 4), p.valid ? QStringLiteral("1") : QStringLiteral("0")});
    }
    const QString name = QStringLiteral("%1_performance_%2.csv")
                             .arg(summary_.portfolio.name.isEmpty() ? QStringLiteral("portfolio") : summary_.portfolio.name,
                                  QDate::currentDate().toString(Qt::ISODate));
    portfolio_csv::save(this, name,
                        {"date", "nav_" + currency_, "net_flow_" + currency_, "period_return_pct",
                         "cumulative_twr_pct", "included"},
                        rows);
}

void PerformanceRiskView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PerformanceRiskView::retranslateUi() {
    if (export_btn_)
        export_btn_->setText(tr("EXPORT CSV"));
    if (chart_title_)
        chart_title_->setText(tr("NAV PERFORMANCE (FROM SNAPSHOTS)"));
    if (metrics_header_)
        metrics_header_->setText(tr("  RISK METRICS"));

    // Period button labels are unit/time symbols (1M, 3M, 1Y, ALL) — left untranslated.

    if (sharpe_card_.title)
        sharpe_card_.title->setText(tr("SHARPE RATIO"));
    if (sharpe_card_.desc)
        sharpe_card_.desc->setText(tr("Risk-adjusted return (annualised)"));
    if (sortino_card_.title)
        sortino_card_.title->setText(tr("SORTINO RATIO"));
    if (sortino_card_.desc)
        sortino_card_.desc->setText(tr("Downside risk-adjusted return"));
    if (beta_card_.title)
        beta_card_.title->setText(tr("BETA"));
    if (beta_card_.desc)
        beta_card_.desc->setText(tr("Sensitivity vs the benchmark"));
    if (alpha_card_.title)
        alpha_card_.title->setText(tr("ALPHA"));
    if (alpha_card_.desc)
        alpha_card_.desc->setText(tr("Jensen's alpha vs benchmark (CAPM)"));
    if (vol_card_.title)
        vol_card_.title->setText(tr("VOLATILITY"));
    if (vol_card_.desc)
        vol_card_.desc->setText(tr("Annualised from daily returns"));
    if (drawdown_card_.title)
        drawdown_card_.title->setText(tr("MAX DRAWDOWN"));
    if (drawdown_card_.desc)
        drawdown_card_.desc->setText(tr("Peak-to-trough, time-weighted"));
    if (var_card_.title)
        var_card_.title->setText(tr("VALUE AT RISK (95%)"));
    if (var_card_.desc)
        var_card_.desc->setText(tr("1-day historical VaR"));
    if (cvar_card_.title)
        cvar_card_.title->setText(tr("CONDITIONAL VaR"));
    if (cvar_card_.desc)
        cvar_card_.desc->setText(tr("Expected shortfall (95%)"));

    // No tr() literals in chart/metric values themselves — only formatted numbers + currency.
}

void PerformanceRiskView::set_period(const QString& period) {
    current_period_ = period;
    for (auto* btn : period_btns_)
        btn->setChecked(btn->text() == period);
    update_chart();
}

void PerformanceRiskView::update_chart() {
    auto* chart = chart_view_->chart();
    chart->removeAllSeries();
    const auto old_axes = chart->axes();
    for (auto* axis : old_axes) {
        chart->removeAxis(axis);
        delete axis;
    }

    if (summary_.holdings.isEmpty())
        return;

    // Filter snapshots by selected period
    QDate cutoff = QDate::currentDate();
    if (current_period_ == "1M")
        cutoff = cutoff.addMonths(-1);
    else if (current_period_ == "3M")
        cutoff = cutoff.addMonths(-3);
    else if (current_period_ == "6M")
        cutoff = cutoff.addMonths(-6);
    else if (current_period_ == "1Y")
        cutoff = cutoff.addYears(-1);
    else
        cutoff = cutoff.addYears(-10);

    QVector<portfolio::PortfolioSnapshot> filtered;
    for (const auto& s : snapshots_) {
        QDate d = QDate::fromString(s.snapshot_date.left(10), Qt::ISODate);
        if (d.isValid() && d >= cutoff)
            filtered.append(s);
    }
    std::sort(filtered.begin(), filtered.end(),
              [](const auto& a, const auto& b) { return a.snapshot_date < b.snapshot_date; });

    auto* line = new QLineSeries;
    auto* upper = new QLineSeries;
    auto* lower = new QLineSeries;

    double first_val = summary_.total_cost_basis;
    double last_val = summary_.total_market_value;
    double min_val = last_val, max_val = last_val;

    if (filtered.size() >= 2) {
        first_val = filtered.first().total_value;
        for (const auto& s : filtered) {
            QDateTime dt = QDateTime::fromString(s.snapshot_date.left(10), Qt::ISODate);
            if (!dt.isValid())
                dt = QDateTime::currentDateTime();
            qint64 ms = dt.toMSecsSinceEpoch();
            line->append(ms, s.total_value);
            upper->append(ms, s.total_value);
            lower->append(ms, first_val);
            min_val = std::min(min_val, s.total_value);
            max_val = std::max(max_val, s.total_value);
        }
        qint64 now_ms = QDateTime::currentDateTime().toMSecsSinceEpoch();
        line->append(now_ms, last_val);
        upper->append(now_ms, last_val);
        lower->append(now_ms, first_val);
        min_val = std::min(min_val, last_val);
        max_val = std::max(max_val, last_val);
    } else {
        // Not enough history in this window. The old fallback drew a straight
        // line from cost basis "30 days ago" to today's value — a made-up date
        // and a made-up path. Say so instead.
        delete line;
        delete upper;
        delete lower;
        chart->setTitle(tr("Not enough NAV history for %1 yet \u2014 history is rebuilt from your transactions "
                           "in the background.")
                            .arg(current_period_));
        chart->setTitleBrush(QColor(ui::colors::TEXT_TERTIARY()));
        return;
    }
    chart->setTitle(QString());

    bool up = last_val >= first_val;
    QColor lc = up ? QColor(ui::colors::POSITIVE()) : QColor(ui::colors::NEGATIVE());
    line->setPen(QPen(lc, 2));

    auto* area = new QAreaSeries(upper, lower);
    QColor fill = lc;
    fill.setAlpha(28);
    area->setBrush(fill);
    area->setPen(Qt::NoPen);

    chart->addSeries(area);
    chart->addSeries(line);

    auto* x_axis = new QDateTimeAxis;
    x_axis->setFormat(current_period_ == "ALL" || current_period_ == "1Y" ? "MMM yy" : "dd MMM");
    x_axis->setTickCount(5);
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    x_axis->setLinePen(QPen(QColor(ui::colors::BORDER_DIM())));
    x_axis->setLabelsFont(QFont("monospace", 7));

    double pad = std::max((max_val - min_val) * 0.08, max_val * 0.01);
    auto* y_axis = new QValueAxis;
    y_axis->setRange(min_val - pad, max_val + pad);
    y_axis->setLabelFormat("%.0f");
    y_axis->setTickCount(4);
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    y_axis->setLinePen(QPen(QColor(ui::colors::BORDER_DIM())));
    y_axis->setLabelsFont(QFont("monospace", 7));

    chart->addAxis(x_axis, Qt::AlignBottom);
    chart->addAxis(y_axis, Qt::AlignLeft);
    line->attachAxis(x_axis);
    line->attachAxis(y_axis);
    area->attachAxis(x_axis);
    area->attachAxis(y_axis);
}

void PerformanceRiskView::update_metrics() {
    update_rf_hint();
    auto fmt = [](double v, int dp = 2) { return QString::number(v, 'f', dp); };

    // All headline metrics are sourced from the service's ComputedMetrics
    // (PortfolioService::compute_metrics), which derives them from real daily
    // snapshots and a real OLS regression against the currency benchmark. The
    // view no longer recomputes anything from assumed market returns. When a
    // metric is unset (insufficient history / no benchmark alignment), the card
    // shows "N/A" rather than a fabricated value.
    const QString na = tr("N/A");
    const QString dim_style =
        QString("color:%1; font-size:18px; font-weight:700; border:none;").arg(ui::colors::TEXT_TERTIARY());

    auto set_na = [&](MetricCard& card) {
        if (!card.value)
            return;
        card.value->setText(na);
        card.value->setStyleSheet(dim_style);
    };
    auto set_colored = [&](MetricCard& card, const QString& text, const char* color) {
        if (!card.value)
            return;
        card.value->setText(text);
        card.value->setStyleSheet(QString("color:%1; font-size:18px; font-weight:700; border:none;").arg(color));
    };

    // ── Sharpe ──────────────────────────────────────────────────────────────
    if (metrics_.sharpe) {
        const double v = *metrics_.sharpe;
        const char* c = v >= 1.0 ? ui::colors::POSITIVE : v >= 0 ? ui::colors::WARNING : ui::colors::NEGATIVE;
        set_colored(sharpe_card_, fmt(v), c);
    } else {
        set_na(sharpe_card_);
    }

    // ── Sortino ─────────────────────────────────────────────────────────────
    if (metrics_.sortino) {
        const double v = *metrics_.sortino;
        const char* c = v >= 1.0 ? ui::colors::POSITIVE : v >= 0 ? ui::colors::WARNING : ui::colors::NEGATIVE;
        set_colored(sortino_card_, fmt(v), c);
    } else {
        set_na(sortino_card_);
    }

    // Descriptions carry the actual benchmark and basis.
    if (beta_card_.desc)
        beta_card_.desc->setText(metrics_.risk_ex_ante ? tr("vs %1, ex-ante (current weights)").arg(metrics_.benchmark_symbol)
                                                       : tr("vs %1, realised").arg(metrics_.benchmark_symbol));
    if (alpha_card_.desc)
        alpha_card_.desc->setText(tr("Jensen's alpha vs %1, annualised").arg(metrics_.benchmark_symbol));
    if (vol_card_.desc)
        vol_card_.desc->setText(metrics_.risk_ex_ante ? tr("Annualised, ex-ante") : tr("Annualised, realised TWR"));

    // ── Beta (real OLS vs benchmark) ──────────────────────────────────────────
    if (metrics_.beta) {
        const double v = *metrics_.beta;
        const char* c = std::abs(v - 1.0) < 0.2   ? ui::colors::POSITIVE
                        : std::abs(v - 1.0) < 0.5 ? ui::colors::WARNING
                                                  : ui::colors::NEGATIVE;
        set_colored(beta_card_, fmt(v), c);
    } else {
        set_na(beta_card_);
    }

    // ── Alpha (real Jensen's alpha vs benchmark) ──────────────────────────────
    if (metrics_.alpha) {
        const double v = *metrics_.alpha;
        const char* c = v >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        set_colored(alpha_card_, QString("%1%2%").arg(v >= 0 ? "+" : "").arg(fmt(v, 1)), c);
    } else {
        set_na(alpha_card_);
    }

    // ── Volatility (annualised %) ─────────────────────────────────────────────
    if (metrics_.volatility)
        set_colored(vol_card_, QString("%1%").arg(fmt(*metrics_.volatility, 1)), ui::colors::TEXT_PRIMARY);
    else
        set_na(vol_card_);

    // ── Max Drawdown (% , negative) ───────────────────────────────────────────
    if (metrics_.max_drawdown)
        set_colored(drawdown_card_, QString("%1%").arg(fmt(*metrics_.max_drawdown, 1)), ui::colors::NEGATIVE);
    else
        set_na(drawdown_card_);

    // ── VaR / CVaR 95% (historical, in currency) ──────────────────────────────
    if (metrics_.var_95)
        set_colored(var_card_, QString("%1 %2").arg(currency_, fmt(*metrics_.var_95)), ui::colors::TEXT_PRIMARY);
    else
        set_na(var_card_);

    if (metrics_.cvar_95)
        set_colored(cvar_card_, QString("%1 %2").arg(currency_, fmt(*metrics_.cvar_95)), ui::colors::TEXT_PRIMARY);
    else
        set_na(cvar_card_);
}

} // namespace fincept::screens
