// src/screens/portfolio/views/QuantStatsView.cpp
#include "screens/portfolio/views/QuantStatsView.h"

#include "core/logging/Logger.h"
#include "services/portfolio/PortfolioAnalyticsService.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/theme/Theme.h"

#include <QAreaSeries>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QDate>
#include <QDateTimeAxis>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineSeries>
#include <QLocale>
#include <QMap>
#include <QPointer>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>
#include <QValueAxis>

#include <algorithm>
#include <cmath>
#include <utility>

using fincept::services::AnalyticsResult;
using fincept::services::PortfolioAnalyticsService;

namespace fincept::screens {

// ── Helpers ───────────────────────────────────────────────────────────────────

static QString pct_str(double v, int dp = 2) {
    return QString("%1%2%").arg(v >= 0 ? "+" : "").arg(QString::number(v, 'f', dp));
}

static QString ratio_str(double v, int dp = 2) {
    return QString::number(v, 'f', dp);
}

static QTableWidgetItem* make_item(const QString& text, Qt::Alignment align, const QColor& color) {
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(align);
    item->setForeground(color);
    return item;
}

// Apply standard dark styling to a QTableWidget.
static void style_table(QTableWidget* t) {
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setShowGrid(false);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setStretchLastSection(true);
    t->setStyleSheet(QString("QTableWidget { background:%1; color:%2; border:none; font-size:11px; }"
                             "QTableWidget::item { padding:4px 8px; border-bottom:1px solid %3; }"
                             "QHeaderView::section { background:%4; color:%5; border:none;"
                             "  border-bottom:2px solid %6; padding:4px 8px; font-size:9px;"
                             "  font-weight:700; letter-spacing:0.5px; }")
                         .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                              ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER()));
}

// One line on a QuantStats time-series chart. Values are fractions (×100 for display).
struct QsLine {
    QsLine(QString n, QJsonArray v, QColor c, bool dash = false, bool fill = false, double s = 100.0)
        : name(std::move(n)), values(std::move(v)), color(c), dashed(dash), fill_to_zero(fill), scale(s) {}
    QString name;
    QJsonArray values;
    QColor color;
    bool dashed = false;
    bool fill_to_zero = false; // shade between the line and 0 (drawdown)
    double scale = 100.0;
};

static qint64 qs_date_ms(const QJsonValue& v) {
    const QDate d = QDate::fromString(v.toString(), Qt::ISODate);
    return d.isValid() ? d.startOfDay().toMSecsSinceEpoch() : 0;
}

// Date-axis line chart for the series the script returns (cumulative, drawdown,
// rolling). Null points (warm-up of a rolling window) are skipped.
static QChartView* qs_line_chart(const QJsonArray& dates, const QVector<QsLine>& lines, const char* y_fmt,
                                 QWidget* parent) {
    auto* chart = new QChart;
    chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart->setBackgroundRoundness(0);
    chart->setMargins(QMargins(4, 4, 4, 4));
    chart->setAnimationOptions(QChart::NoAnimation);
    chart->legend()->setVisible(lines.size() > 1);
    chart->legend()->setLabelColor(QColor(ui::colors::TEXT_SECONDARY()));
    chart->legend()->setAlignment(Qt::AlignTop);

    auto* x_axis = new QDateTimeAxis;
    x_axis->setFormat("MMM yy");
    x_axis->setTickCount(6);
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    x_axis->setLabelsFont(QFont("Consolas", 8));
    auto* y_axis = new QValueAxis;
    y_axis->setLabelFormat(y_fmt);
    y_axis->setTickCount(5);
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    y_axis->setLabelsFont(QFont("Consolas", 8));
    chart->addAxis(x_axis, Qt::AlignBottom);
    chart->addAxis(y_axis, Qt::AlignLeft);

    double lo = 0.0, hi = 0.0;
    for (const auto& ln : lines) {
        auto* s = new QLineSeries;
        s->setName(ln.name);
        QLineSeries* zero = ln.fill_to_zero ? new QLineSeries : nullptr;
        const int n = static_cast<int>(std::min(dates.size(), ln.values.size()));
        for (int i = 0; i < n; ++i) {
            if (!ln.values[i].isDouble())
                continue;
            const qint64 x = qs_date_ms(dates[i]);
            if (x == 0)
                continue;
            const double y = ln.values[i].toDouble() * ln.scale;
            s->append(static_cast<double>(x), y);
            if (zero)
                zero->append(static_cast<double>(x), 0.0);
            lo = std::min(lo, y);
            hi = std::max(hi, y);
        }
        QPen pen(ln.color, ln.dashed ? 1.2 : 1.8);
        if (ln.dashed)
            pen.setStyle(Qt::DashLine);
        if (zero) {
            auto* area = new QAreaSeries(s, zero);
            area->setName(ln.name);
            QColor fill = ln.color;
            fill.setAlpha(60);
            area->setBrush(fill);
            area->setPen(pen);
            chart->addSeries(area);
            area->attachAxis(x_axis);
            area->attachAxis(y_axis);
        } else {
            s->setPen(pen);
            chart->addSeries(s);
            s->attachAxis(x_axis);
            s->attachAxis(y_axis);
        }
    }
    const double pad = std::max((hi - lo) * 0.08, 0.5);
    y_axis->setRange(lo - pad, hi + pad);

    auto* view = new QChartView(chart, parent);
    view->setRenderHint(QPainter::Antialiasing, true);
    view->setStyleSheet(QString("background:%1; border:none;").arg(ui::colors::BG_BASE()));
    return view;
}

// Build a centred placeholder label inside a parent widget. Returns the wrapper
// widget; @p out_label receives the inner QLabel so callers can retranslate it.
static QWidget* make_placeholder(const QString& msg, QLabel** out_label = nullptr, QWidget* parent = nullptr) {
    auto* w = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(24, 32, 24, 32);
    auto* lbl = new QLabel(msg);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setWordWrap(true);
    lbl->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY()));
    lay->addStretch();
    lay->addWidget(lbl);
    lay->addStretch();
    if (out_label)
        *out_label = lbl;
    return w;
}

// ── Constructor ───────────────────────────────────────────────────────────────

QuantStatsView::QuantStatsView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

// ── build_ui ─────────────────────────────────────────────────────────────────

void QuantStatsView::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header bar ────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setFixedHeight(36);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::AMBER()));

    auto* h_lay = new QHBoxLayout(header);
    h_lay->setContentsMargins(12, 0, 12, 0);
    h_lay->setSpacing(8);

    title_lbl_ = new QLabel(tr("QUANTSTATS ANALYSIS"));
    title_lbl_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    h_lay->addWidget(title_lbl_);
    h_lay->addStretch();

    qs_status_ = new QLabel;
    qs_status_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY()));
    h_lay->addWidget(qs_status_);

    qs_run_btn_ = new QPushButton(tr("\u25B6 RUN QUANTSTATS"));
    qs_run_btn_->setFixedHeight(22);
    qs_run_btn_->setCursor(Qt::PointingHandCursor);
    qs_run_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:#000; border:none;"
                "  padding:0 12px; font-size:9px; font-weight:700; }"
                "QPushButton:hover { background:%2; }"
                "QPushButton:disabled { background:%3; color:%4; }")
            .arg(ui::colors::AMBER(), ui::colors::WARNING(), ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY()));
    connect(qs_run_btn_, &QPushButton::clicked, this, &QuantStatsView::run_quantstats);
    h_lay->addWidget(qs_run_btn_);

    root->addWidget(header);

    // ── Tab widget ────────────────────────────────────────────────────────────
    tabs_ = new QTabWidget;
    tabs_->tabBar()->setElideMode(Qt::ElideNone);
    tabs_->tabBar()->setExpanding(false);
    tabs_->tabBar()->setUsesScrollButtons(false);
    tabs_->setDocumentMode(true);
    tabs_->setStyleSheet(QString("QTabWidget::pane { border:0; background:%1; }"
                                 "QTabBar::tab { background:%2; color:%3; padding:6px 14px; border:0;"
                                 "  border-bottom:2px solid transparent; font-size:9px; font-weight:700;"
                                 "  letter-spacing:0.5px; }"
                                 "QTabBar::tab:selected { color:%4; border-bottom:2px solid %4; }"
                                 "QTabBar::tab:hover { color:%5; }")
                             .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(),
                                  ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));

    // ── METRICS tab ───────────────────────────────────────────────────────────
    {
        auto* metrics_w = new QWidget(this);
        auto* ml = new QVBoxLayout(metrics_w);
        ml->setContentsMargins(12, 8, 12, 8);
        ml->setSpacing(4);

        metrics_section_lbl_ = new QLabel(tr("KEY PERFORMANCE INDICATORS"));
        metrics_section_lbl_->setStyleSheet(
            QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
        ml->addWidget(metrics_section_lbl_);

        metrics_table_ = new QTableWidget;
        metrics_table_->setColumnCount(3);
        metrics_table_->setHorizontalHeaderLabels({tr("METRIC"), tr("VALUE"), tr("BENCHMARK")});
        metrics_table_->setColumnWidth(0, 220);
        metrics_table_->setColumnWidth(1, 130);
        style_table(metrics_table_);
        ml->addWidget(metrics_table_, 1);

        metrics_tab_index_ = tabs_->addTab(metrics_w, tr("METRICS"));
    }

    // ── RETURNS tab ───────────────────────────────────────────────────────────
    {
        auto* returns_w = new QWidget(this);
        returns_w->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
        auto* rl = new QVBoxLayout(returns_w);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(0);

        returns_stack_ = new QStackedWidget;
        returns_stack_->addWidget(
            make_placeholder(tr("Run QuantStats Analysis for return distribution"), &returns_placeholder_));
        // Index 1 is added lazily in update_returns()
        rl->addWidget(returns_stack_);

        returns_tab_index_ = tabs_->addTab(returns_w, tr("RETURNS"));
    }

    // ── DRAWDOWN tab ──────────────────────────────────────────────────────────
    {
        auto* dd_w = new QWidget(this);
        dd_w->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
        auto* dl = new QVBoxLayout(dd_w);
        dl->setContentsMargins(0, 0, 0, 0);
        dl->setSpacing(0);

        drawdown_stack_ = new QStackedWidget;
        drawdown_stack_->addWidget(
            make_placeholder(tr("Run QuantStats Analysis for drawdown metrics"), &drawdown_placeholder_));
        dl->addWidget(drawdown_stack_);

        drawdown_tab_index_ = tabs_->addTab(dd_w, tr("DRAWDOWN"));
    }

    // ── ROLLING tab ───────────────────────────────────────────────────────────
    {
        auto* roll_w = new QWidget(this);
        roll_w->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
        auto* rll = new QVBoxLayout(roll_w);
        rll->setContentsMargins(0, 0, 0, 0);
        rll->setSpacing(0);

        rolling_stack_ = new QStackedWidget;
        rolling_stack_->addWidget(
            make_placeholder(tr("Run QuantStats Analysis for rolling metrics"), &rolling_placeholder_));
        rll->addWidget(rolling_stack_);

        rolling_tab_index_ = tabs_->addTab(roll_w, tr("ROLLING"));
    }

    // ── MONTE CARLO tab ───────────────────────────────────────────────────────
    {
        auto* mc_w = new QWidget(this);
        mc_w->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
        auto* mcl = new QVBoxLayout(mc_w);
        mcl->setContentsMargins(16, 12, 16, 12);
        mcl->setSpacing(8);

        mc_title_ = new QLabel(tr("MONTE CARLO SIMULATION"));
        mc_title_->setStyleSheet(
            QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
        mcl->addWidget(mc_title_);

        mc_desc_ = new QLabel(tr("Simulate 1,000 portfolio return paths using GBM to estimate probability\n"
                                 "distributions of future returns, drawdowns, and terminal wealth."));
        mc_desc_->setWordWrap(true);
        mc_desc_->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY()));
        mcl->addWidget(mc_desc_);

        // Button row
        auto* mc_btn_row = new QHBoxLayout;
        mc_btn_row->setSpacing(8);

        mc_run_btn_ = new QPushButton(tr("\u25B6 RUN MONTE CARLO (1000 paths)"));
        mc_run_btn_->setFixedHeight(28);
        mc_run_btn_->setCursor(Qt::PointingHandCursor);
        mc_run_btn_->setStyleSheet(
            QString("QPushButton { background:%1; color:#000; border:none;"
                    "  padding:0 16px; font-size:10px; font-weight:700; }"
                    "QPushButton:hover { background:%2; }"
                    "QPushButton:disabled { background:%3; color:%4; }")
                .arg(ui::colors::AMBER(), ui::colors::WARNING(), ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY()));
        connect(mc_run_btn_, &QPushButton::clicked, this, &QuantStatsView::run_monte_carlo);
        mc_btn_row->addWidget(mc_run_btn_);

        mc_status_ = new QLabel;
        mc_status_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
        mc_btn_row->addWidget(mc_status_);
        mc_btn_row->addStretch();
        mcl->addLayout(mc_btn_row);

        // Stacked area: placeholder vs live content
        mc_stack_ = new QStackedWidget;
        mc_stack_->addWidget(
            make_placeholder(tr("Press RUN MONTE CARLO to simulate 1,000 return paths"), &mc_placeholder_));
        // Index 1 is built lazily in update_monte_carlo_chart()
        mcl->addWidget(mc_stack_, 1);

        // mc_results_ kept for backward compatibility (not used in display logic)
        mc_results_ = new QWidget(this);
        mc_results_->setVisible(false);

        mc_tab_index_ = tabs_->addTab(mc_w, tr("MONTE CARLO"));
    }

    root->addWidget(tabs_, 1);
}

// ── set_data ──────────────────────────────────────────────────────────────────

void QuantStatsView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    const bool portfolio_changed = summary.portfolio.id != summary_.portfolio.id;
    summary_ = summary;
    currency_ = currency;

    // Results belong to the book they were computed for - drop them when the
    // user switches portfolio instead of showing the previous one's statistics
    // until the new run lands.
    if (portfolio_changed) {
        qs_data_ = QJsonObject();
        mc_data_ = QJsonObject();
        for (QStackedWidget* st : {returns_stack_, drawdown_stack_, rolling_stack_, mc_stack_}) {
            if (st)
                st->setCurrentIndex(0);
        }
    }

    // Populate metrics immediately from live summary data
    update_metrics();

    // Kick off the Python fetch automatically - but only when the analysis
    // inputs changed. set_data() now also runs on every portfolio refresh (and
    // on every re-entry to this tab); the 1-year history download + QuantStats
    // run does not depend on live prices, so repeating it each minute only
    // burned a Python slot. The RUN button still forces a fresh run.
    QStringList parts;
    parts.reserve(summary_.holdings.size());
    for (const auto& h : summary_.holdings)
        parts.append(h.symbol + QLatin1Char(':') + QString::number(h.quantity, 'g', 10));
    parts.sort();
    const QString signature = summary_.portfolio.id + QLatin1Char('|') + parts.join(QLatin1Char(','));
    if (signature != qs_signature_ && !qs_running_) {
        qs_signature_ = signature;
        run_quantstats();
    }
}

// ── update_metrics ────────────────────────────────────────────────────────────

void QuantStatsView::update_metrics() {
    metrics_table_->clearContents();

    // ── Build row definitions ─────────────────────────────────────────────────
    struct Row {
        QString section; // non-empty marks a section separator row
        QString name;
        QString value;
        QString benchmark = "--";
        bool is_positive = true; // drives colour of value cell
        bool is_section = false;
    };

    QVector<Row> rows;

    auto add_section = [&](const QString& label) { rows.push_back({label, {}, {}, {}, false, true}); };

    const bool have_qs = !qs_data_.isEmpty();

    // Helper lambdas for reading qs_data_
    auto qs_pct = [&](const QString& group, const QString& key) -> double {
        if (!have_qs)
            return 0.0;
        return qs_data_[group].toObject()[key].toDouble() * 100.0;
    };
    auto qs_val = [&](const QString& group, const QString& key) -> double {
        if (!have_qs)
            return 0.0;
        return qs_data_[group].toObject()[key].toDouble();
    };
    auto qs_int = [&](const QString& group, const QString& key) -> int {
        if (!have_qs)
            return 0;
        return qs_data_[group].toObject()[key].toInt();
    };
    // Same statistics computed by the script for the benchmark index.
    const QJsonObject bench_obj = qs_data_.value("benchmark").toObject();
    auto b_val = [&](const QString& group, const QString& key, bool pct, int dp = 2) -> QString {
        const QJsonValue v = bench_obj.value(group).toObject().value(key);
        if (!v.isDouble())
            return QStringLiteral("--");
        return pct ? pct_str(v.toDouble() * 100.0, dp) : ratio_str(v.toDouble(), dp);
    };

    // ── PERFORMANCE ──────────────────────────────────────────────────────────
    add_section(tr("PERFORMANCE"));

    if (have_qs) {
        double total_ret = qs_pct("performance", "total_return");
        double ann_ret = qs_pct("performance", "annualized_return");
        int t_days = qs_int("performance", "trading_days");
        double best_day = qs_pct("performance", "best_day");
        double worst_day = qs_pct("performance", "worst_day");
        double avg_daily = qs_pct("performance", "avg_daily_return");

        rows.push_back({"", tr("Total Return"), pct_str(total_ret), b_val("performance", "total_return", true), total_ret >= 0});
        rows.push_back({"", tr("Annualized Return"), pct_str(ann_ret), b_val("performance", "annualized_return", true), ann_ret >= 0});
        rows.push_back({"", tr("Trading Days"), QString::number(t_days), "--", true});
        rows.push_back({"", tr("Best Day"), pct_str(best_day), b_val("performance", "best_day", true), true});
        rows.push_back({"", tr("Worst Day"), pct_str(worst_day), b_val("performance", "worst_day", true), worst_day >= 0});
        rows.push_back({"", tr("Avg Daily Return"), pct_str(avg_daily, 4), "--", avg_daily >= 0});
    } else {
        // Pre-run: show live summary values
        double pnl_pct = summary_.total_unrealized_pnl_percent;
        double day_pct = summary_.total_day_change_percent;
        rows.push_back({"", tr("Unrealized P&L %"), pct_str(pnl_pct), "--", pnl_pct >= 0});
        rows.push_back({"", tr("Day Change %"), pct_str(day_pct), "--", day_pct >= 0});
        rows.push_back({"", tr("Total Positions"), QString::number(summary_.total_positions), "--", true});
        rows.push_back({"", tr("Gainers"), QString::number(summary_.gainers), "--", true});
        rows.push_back({"", tr("Losers"), QString::number(summary_.losers), "--", summary_.losers == 0});
    }

    // ── RISK ─────────────────────────────────────────────────────────────────
    add_section(tr("RISK"));

    if (have_qs) {
        double ann_vol = qs_pct("risk", "annualized_volatility");
        double max_dd = qs_pct("risk", "max_drawdown");
        double var95 = qs_pct("risk", "var_95_daily");
        double cvar95 = qs_pct("risk", "cvar_95_daily");
        double down_dev = qs_pct("risk", "downside_deviation");

        rows.push_back({"", tr("Annualized Volatility"), pct_str(ann_vol), b_val("risk", "annualized_volatility", true), ann_vol < 20.0});
        rows.push_back({"", tr("Max Drawdown"), pct_str(max_dd), b_val("risk", "max_drawdown", true), max_dd >= 0});
        rows.push_back({"", tr("VaR 95% (Daily)"), pct_str(var95), b_val("risk", "var_95_daily", true), var95 >= 0});
        rows.push_back({"", tr("CVaR 95% (Daily)"), pct_str(cvar95), b_val("risk", "cvar_95_daily", true), cvar95 >= 0});
        rows.push_back({"", tr("Downside Deviation"), pct_str(down_dev), b_val("risk", "downside_deviation", true), down_dev < 15.0});
    } else {
        rows.push_back({"", tr("Annualized Volatility"), "--", "--", true});
        rows.push_back({"", tr("Max Drawdown"), "--", "--", true});
        rows.push_back({"", tr("VaR 95% (Daily)"), "--", "--", true});
        rows.push_back({"", tr("CVaR 95% (Daily)"), "--", "--", true});
        rows.push_back({"", tr("Downside Deviation"), "--", "--", true});
    }

    // ── RATIOS ────────────────────────────────────────────────────────────────
    add_section(tr("RATIOS"));

    if (have_qs) {
        double sharpe = qs_val("ratios", "sharpe_ratio");
        double sortino = qs_val("ratios", "sortino_ratio");
        double calmar = qs_val("ratios", "calmar_ratio");
        double pf = qs_val("ratios", "profit_factor");

        rows.push_back({"", tr("Sharpe Ratio"), ratio_str(sharpe), b_val("ratios", "sharpe_ratio", false), sharpe >= 0});
        rows.push_back({"", tr("Sortino Ratio"), ratio_str(sortino), b_val("ratios", "sortino_ratio", false), sortino >= 0});
        rows.push_back({"", tr("Calmar Ratio"), ratio_str(calmar), b_val("ratios", "calmar_ratio", false), calmar >= 0});
        rows.push_back({"", tr("Profit Factor"), ratio_str(pf), "--", pf >= 1.0});
    } else {
        rows.push_back({"", tr("Sharpe Ratio"), "--", "--", true});
        rows.push_back({"", tr("Sortino Ratio"), "--", "--", true});
        rows.push_back({"", tr("Calmar Ratio"), "--", "--", true});
        rows.push_back({"", tr("Profit Factor"), "--", "--", true});
    }

    // ── DISTRIBUTION ─────────────────────────────────────────────────────────
    add_section(tr("DISTRIBUTION"));

    if (have_qs) {
        double skew = qs_val("distribution", "skewness");
        double kurt = qs_val("distribution", "kurtosis");
        double win_rate = qs_pct("distribution", "win_rate");
        int win_days = qs_int("distribution", "win_days");
        int los_days = qs_int("distribution", "loss_days");
        double avg_win = qs_pct("distribution", "avg_win");
        double avg_loss = qs_pct("distribution", "avg_loss");

        rows.push_back({"", tr("Skewness"), ratio_str(skew, 3), b_val("distribution", "skewness", false, 3), skew >= 0});
        rows.push_back({"", tr("Excess Kurtosis"), ratio_str(kurt, 3), b_val("distribution", "kurtosis", false, 3), kurt <= 1.0});
        rows.push_back({"", tr("Win Rate"), pct_str(win_rate), "--", win_rate >= 50.0});
        rows.push_back({"", tr("Win Days"), QString::number(win_days), "--", true});
        rows.push_back({"", tr("Loss Days"), QString::number(los_days), "--", los_days == 0});
        rows.push_back({"", tr("Avg Win"), pct_str(avg_win), "--", true});
        rows.push_back({"", tr("Avg Loss"), pct_str(avg_loss), "--", avg_loss >= 0});
    } else {
        rows.push_back({"", tr("Skewness"), "--", "--", true});
        rows.push_back({"", tr("Excess Kurtosis"), "--", "--", true});
        rows.push_back({"", tr("Win Rate"), "--", "--", true});
        rows.push_back({"", tr("Win Days"), "--", "--", true});
        rows.push_back({"", tr("Loss Days"), "--", "--", true});
        rows.push_back({"", tr("Avg Win"), "--", "--", true});
        rows.push_back({"", tr("Avg Loss"), "--", "--", true});

        // Prompt row when pre-run
        rows.push_back({"", tr("Run QuantStats for full metrics \u2192"), "", "--", true});
    }

    // ── Populate table ────────────────────────────────────────────────────────
    // RELATIVE TO BENCHMARK: how the back-tested mix behaves against the index — did it
    // earn its risk? (Appended last so the existing section order stays put.)
    const QJsonObject rel = qs_data_.value("relative").toObject();
    if (have_qs && !rel.isEmpty()) {
        const QString bsym = bench_obj.value("symbol").toString();
        add_section(bsym.isEmpty() ? tr("RELATIVE TO BENCHMARK") : tr("RELATIVE TO %1").arg(bsym));
        auto num = [&rel](const char* key) { return rel.value(QLatin1String(key)); };
        auto add_ratio = [&](const QString& name, const char* key, int dp) {
            const QJsonValue v = num(key);
            if (v.isDouble())
                rows.push_back({"", name, ratio_str(v.toDouble(), dp), "--", true});
        };
        auto add_pct = [&](const QString& name, const char* key, bool good_if_positive) {
            const QJsonValue v = num(key);
            if (v.isDouble())
                rows.push_back(
                    {"", name, pct_str(v.toDouble() * 100.0), "--", good_if_positive ? v.toDouble() >= 0 : true});
        };
        add_ratio(tr("Beta"), "beta", 2);
        add_pct(tr("Alpha (annualized)"), "alpha", true);
        add_ratio(tr("Correlation"), "correlation", 2);
        add_ratio(tr("R-Squared"), "r_squared", 2);
        add_pct(tr("Tracking Error"), "tracking_error", false);
        if (num("information_ratio").isDouble())
            rows.push_back({"", tr("Information Ratio"), ratio_str(num("information_ratio").toDouble(), 2), "--",
                            num("information_ratio").toDouble() >= 0});
        // Capture ratios: above 1 on the way up and below 1 on the way down is the
        // asymmetry an active book wants.
        if (num("up_capture").isDouble())
            rows.push_back({"", tr("Up Capture"), ratio_str(num("up_capture").toDouble(), 2), "--",
                            num("up_capture").toDouble() >= 1.0});
        if (num("down_capture").isDouble())
            rows.push_back({"", tr("Down Capture"), ratio_str(num("down_capture").toDouble(), 2), "--",
                            num("down_capture").toDouble() <= 1.0});
    }

    metrics_table_->setRowCount(rows.size());

    for (int r = 0; r < rows.size(); ++r) {
        metrics_table_->setRowHeight(r, 26);
        const auto& row = rows[r];

        if (row.is_section) {
            // Section separator spanning all columns
            auto* item = new QTableWidgetItem(row.section);
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            item->setForeground(QColor(ui::colors::AMBER()));
            item->setBackground(QColor(ui::colors::BG_RAISED()));
            QFont f = item->font();
            f.setPointSize(8);
            f.setBold(true);
            item->setFont(f);
            metrics_table_->setItem(r, 0, item);
            metrics_table_->setSpan(r, 0, 1, 3);
            continue;
        }

        metrics_table_->setItem(
            r, 0, make_item(row.name, Qt::AlignLeft | Qt::AlignVCenter, QColor(ui::colors::TEXT_SECONDARY())));

        const char* val_color = row.value.isEmpty() ? ui::colors::TEXT_TERTIARY
                                : row.is_positive   ? ui::colors::POSITIVE
                                                    : ui::colors::NEGATIVE();
        // Special: neutral colour for counters and "--"
        if (row.value == "--" || row.name.contains("Days") || row.name.contains("Positions") ||
            row.name == "Trading Days") {
            val_color = ui::colors::TEXT_PRIMARY();
        }

        metrics_table_->setItem(
            r, 1,
            make_item(row.value.isEmpty() ? "--" : row.value, Qt::AlignRight | Qt::AlignVCenter, QColor(val_color)));

        metrics_table_->setItem(
            r, 2, make_item(row.benchmark, Qt::AlignRight | Qt::AlignVCenter, QColor(ui::colors::TEXT_TERTIARY())));
    }
}

// ── update_returns ────────────────────────────────────────────────────────────

void QuantStatsView::update_returns() {
    if (qs_data_.isEmpty())
        return;

    // Remove any previously injected widget at index 1
    if (returns_stack_->count() > 1) {
        QWidget* old = returns_stack_->widget(1);
        returns_stack_->removeWidget(old);
        old->deleteLater();
    }

    auto* content = new QWidget(this);
    content->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(16, 12, 16, 12);
    cl->setSpacing(12);

    // Cumulative return of today's mix against the benchmark, same dates.
    const QJsonObject series = qs_data_["series"].toObject();
    if (!series.isEmpty()) {
        auto* cum_title = new QLabel(tr("CUMULATIVE RETURN — CURRENT WEIGHTS, BACK-TESTED"));
        cum_title->setStyleSheet(
            QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
        cl->addWidget(cum_title);
        QVector<QsLine> lines;
        lines.append({tr("Portfolio"), series["cumulative"].toArray(), QColor(ui::colors::POSITIVE())});
        if (series.contains("benchmark_cumulative")) {
            const QString bsym = qs_data_["benchmark"].toObject()["symbol"].toString();
            lines.append({bsym.isEmpty() ? tr("Benchmark") : bsym, series["benchmark_cumulative"].toArray(),
                          QColor(ui::colors::CYAN()), true});
        }
        auto* cum_chart = qs_line_chart(series["dates"].toArray(), lines, "%.0f%%", content);
        cum_chart->setMinimumHeight(240);
        cl->addWidget(cum_chart);
    }

    auto* title = new QLabel(tr("RETURN DISTRIBUTION"));
    title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    cl->addWidget(title);

    // Win/loss summary cards — 2-column grid
    const QJsonObject dist = qs_data_["distribution"].toObject();
    double win_rate = dist["win_rate"].toDouble() * 100.0;
    int win_days = dist["win_days"].toInt();
    int loss_days = dist["loss_days"].toInt();
    double avg_win = dist["avg_win"].toDouble() * 100.0;
    double avg_loss = dist["avg_loss"].toDouble() * 100.0;
    double skew = dist["skewness"].toDouble();
    double kurt = dist["kurtosis"].toDouble();

    struct Card {
        QString label;
        QString value;
        const char* color;
    };
    QVector<Card> cards = {
        {tr("WIN RATE"), pct_str(win_rate), ui::colors::POSITIVE()},
        {tr("LOSS RATE"), pct_str(100.0 - win_rate), ui::colors::NEGATIVE()},
        {tr("WIN DAYS"), QString::number(win_days), ui::colors::POSITIVE()},
        {tr("LOSS DAYS"), QString::number(loss_days), ui::colors::NEGATIVE()},
        {tr("AVG WIN"), pct_str(avg_win), ui::colors::POSITIVE()},
        {tr("AVG LOSS"), pct_str(avg_loss), ui::colors::NEGATIVE()},
        {tr("SKEWNESS"), ratio_str(skew, 3), ui::colors::TEXT_PRIMARY()},
        {tr("EXCESS KURTOSIS"), ratio_str(kurt, 3), ui::colors::TEXT_PRIMARY()},
    };

    auto* grid_w = new QWidget(this);
    auto* grid = new QGridLayout(grid_w);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(8);

    for (int i = 0; i < cards.size(); ++i) {
        const auto& c = cards[i];
        int row_i = i / 2;
        int col_i = i % 2;

        auto* card_w = new QWidget(this);
        card_w->setStyleSheet(
            QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED()));
        auto* card_l = new QVBoxLayout(card_w);
        card_l->setContentsMargins(12, 10, 12, 10);
        card_l->setSpacing(4);

        auto* lbl = new QLabel(c.label);
        lbl->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:0.5px;")
                               .arg(ui::colors::TEXT_TERTIARY()));
        card_l->addWidget(lbl);

        auto* val_lbl = new QLabel(c.value);
        val_lbl->setStyleSheet(QString("color:%1; font-size:18px; font-weight:700;").arg(c.color));
        card_l->addWidget(val_lbl);

        grid->addWidget(card_w, row_i, col_i);
    }

    cl->addWidget(grid_w);

    // Win/Loss bar chart (simple table-based visual)
    auto* dist_table = new QTableWidget;
    dist_table->setColumnCount(2);
    dist_table->setHorizontalHeaderLabels({tr("METRIC"), tr("VALUE")});
    dist_table->setColumnWidth(0, 200);
    style_table(dist_table);

    const QJsonObject perf = qs_data_["performance"].toObject();
    double best_day = perf["best_day"].toDouble() * 100.0;
    double worst_day = perf["worst_day"].toDouble() * 100.0;
    double avg_daily = perf["avg_daily_return"].toDouble() * 100.0;

    struct TableRow {
        QString name;
        QString val;
        bool positive;
    };
    QVector<TableRow> trows = {
        {tr("Best Day"), pct_str(best_day), true},
        {tr("Worst Day"), pct_str(worst_day), worst_day >= 0},
        {tr("Avg Daily Return"), pct_str(avg_daily, 4), avg_daily >= 0},
        {tr("Profit Factor"), ratio_str(qs_data_["ratios"].toObject()["profit_factor"].toDouble()),
         qs_data_["ratios"].toObject()["profit_factor"].toDouble() >= 1.0},
    };
    dist_table->setRowCount(trows.size());
    for (int i = 0; i < trows.size(); ++i) {
        dist_table->setRowHeight(i, 26);
        dist_table->setItem(
            i, 0, make_item(trows[i].name, Qt::AlignLeft | Qt::AlignVCenter, QColor(ui::colors::TEXT_SECONDARY())));
        dist_table->setItem(i, 1,
                            make_item(trows[i].val, Qt::AlignRight | Qt::AlignVCenter,
                                      QColor(trows[i].positive ? ui::colors::POSITIVE() : ui::colors::NEGATIVE())));
    }
    dist_table->setMinimumHeight(26 * (static_cast<int>(trows.size()) + 1) + 4);
    cl->addWidget(dist_table);

    // Monthly returns grid (years × months + the year compounded) — the classic
    // QuantStats heatmap: seasonality and the bad months at a glance.
    const QJsonObject monthly = qs_data_["monthly"].toObject();
    if (!monthly.isEmpty()) {
        auto* m_title = new QLabel(tr("MONTHLY RETURNS"));
        m_title->setStyleSheet(
            QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
        cl->addWidget(m_title);

        QMap<int, QMap<int, double>> by_year; // year -> month -> return
        for (auto it = monthly.begin(); it != monthly.end(); ++it) {
            if (!it.value().isDouble())
                continue;
            const QStringList ym = it.key().split(QLatin1Char('-'));
            if (ym.size() != 2)
                continue;
            by_year[ym[0].toInt()][ym[1].toInt()] = it.value().toDouble();
        }
        auto* mt = new QTableWidget(static_cast<int>(by_year.size()), 14, content);
        QStringList hdr{tr("YEAR")};
        for (int m = 1; m <= 12; ++m)
            hdr << QLocale().monthName(m, QLocale::ShortFormat).toUpper();
        hdr << tr("YEAR %");
        mt->setHorizontalHeaderLabels(hdr);
        style_table(mt);
        mt->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        int r = 0;
        for (auto yit = by_year.constEnd(); yit != by_year.constBegin();) { // newest year first
            --yit;
            mt->setRowHeight(r, 24);
            mt->setItem(r, 0,
                        make_item(QString::number(yit.key()), Qt::AlignLeft | Qt::AlignVCenter,
                                  QColor(ui::colors::TEXT_SECONDARY())));
            double year_growth = 1.0;
            for (int m = 1; m <= 12; ++m) {
                if (!yit.value().contains(m)) {
                    mt->setItem(r, m, make_item(QString(), Qt::AlignCenter, QColor(ui::colors::TEXT_TERTIARY())));
                    continue;
                }
                const double v = yit.value().value(m);
                year_growth *= 1.0 + v;
                auto* item =
                    make_item(QString::number(v * 100.0, 'f', 1), Qt::AlignCenter, QColor(ui::colors::TEXT_PRIMARY()));
                QColor bg = v >= 0 ? QColor(ui::colors::POSITIVE()) : QColor(ui::colors::NEGATIVE());
                bg.setAlpha(static_cast<int>(std::min(1.0, std::abs(v) / 0.10) * 150) + 20);
                item->setBackground(bg);
                mt->setItem(r, m, item);
            }
            const double yr = year_growth - 1.0;
            mt->setItem(r, 13,
                        make_item(pct_str(yr * 100.0, 1), Qt::AlignCenter,
                                  QColor(yr >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE())));
            ++r;
        }
        mt->setMinimumHeight(24 * (r + 1) + 6);
        cl->addWidget(mt);
    }
    cl->addStretch();

    // The tab now holds charts + tables: scroll rather than squeeze.
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    returns_stack_->addWidget(scroll);
    returns_stack_->setCurrentIndex(1);
}

// ── update_drawdown ───────────────────────────────────────────────────────────

void QuantStatsView::update_drawdown() {
    if (qs_data_.isEmpty())
        return;

    if (drawdown_stack_->count() > 1) {
        QWidget* old = drawdown_stack_->widget(1);
        drawdown_stack_->removeWidget(old);
        old->deleteLater();
    }

    auto* content = new QWidget(this);
    content->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(16, 12, 16, 12);
    cl->setSpacing(12);

    auto* title = new QLabel(tr("DRAWDOWN & RISK METRICS"));
    title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    cl->addWidget(title);

    // Prominent Max Drawdown display
    const QJsonObject risk = qs_data_["risk"].toObject();
    double max_dd = risk["max_drawdown"].toDouble() * 100.0;

    auto* hero_w = new QWidget(this);
    hero_w->setStyleSheet(
        QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::NEGATIVE()));
    auto* hero_l = new QVBoxLayout(hero_w);
    hero_l->setContentsMargins(16, 12, 16, 12);
    hero_l->setSpacing(2);

    auto* hero_label = new QLabel(tr("MAX DRAWDOWN"));
    hero_label->setStyleSheet(
        QString("color:%1; font-size:9px; font-weight:700; letter-spacing:1px;").arg(ui::colors::TEXT_TERTIARY()));
    hero_l->addWidget(hero_label);

    auto* hero_value = new QLabel(pct_str(max_dd));
    hero_value->setStyleSheet(QString("color:%1; font-size:28px; font-weight:700;")
                                  .arg(max_dd < 0 ? ui::colors::NEGATIVE() : ui::colors::POSITIVE()));
    hero_l->addWidget(hero_value);
    cl->addWidget(hero_w);

    // Underwater curve: how far below its running peak the mix was, every day.
    const QJsonObject series = qs_data_["series"].toObject();
    if (!series.isEmpty()) {
        auto* uw = qs_line_chart(
            series["dates"].toArray(),
            {QsLine(tr("Drawdown"), series["drawdown"].toArray(), QColor(ui::colors::NEGATIVE()), false, true)},
            "%.0f%%", content);
        uw->setMinimumHeight(200);
        cl->addWidget(uw);
    }

    // The worst episodes: when they started, bottomed and recovered — depth alone says
    // nothing about how long the money was under water.
    const QJsonArray episodes = qs_data_["drawdowns"].toArray();
    if (!episodes.isEmpty()) {
        auto* ep_title = new QLabel(tr("WORST DRAWDOWN PERIODS"));
        ep_title->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; letter-spacing:0.5px;")
                                    .arg(ui::colors::TEXT_SECONDARY()));
        cl->addWidget(ep_title);
        auto* ep = new QTableWidget(static_cast<int>(episodes.size()), 5, content);
        ep->setHorizontalHeaderLabels({tr("PEAK"), tr("TROUGH"), tr("RECOVERED"), tr("DEPTH"), tr("DAYS")});
        style_table(ep);
        ep->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        for (int i = 0; i < episodes.size(); ++i) {
            const QJsonObject e = episodes[i].toObject();
            ep->setRowHeight(i, 24);
            const auto left = Qt::AlignLeft | Qt::AlignVCenter;
            const auto right = Qt::AlignRight | Qt::AlignVCenter;
            ep->setItem(i, 0, make_item(e["start"].toString(), left, QColor(ui::colors::TEXT_SECONDARY())));
            ep->setItem(i, 1, make_item(e["trough"].toString(), left, QColor(ui::colors::TEXT_SECONDARY())));
            const bool open = e["end"].isNull();
            ep->setItem(i, 2,
                        make_item(open ? tr("not yet") : e["end"].toString(), left,
                                  QColor(open ? ui::colors::WARNING() : ui::colors::TEXT_SECONDARY())));
            ep->setItem(i, 3, make_item(pct_str(e["depth"].toDouble() * 100.0), right, QColor(ui::colors::NEGATIVE())));
            ep->setItem(i, 4, make_item(QString::number(e["days"].toInt()), right, QColor(ui::colors::TEXT_PRIMARY())));
        }
        ep->setMinimumHeight(24 * (static_cast<int>(episodes.size()) + 1) + 6);
        cl->addWidget(ep);
    }

    // Drawdown metrics table
    auto* dd_table = new QTableWidget;
    dd_table->setColumnCount(2);
    dd_table->setHorizontalHeaderLabels({tr("RISK METRIC"), tr("VALUE")});
    dd_table->setColumnWidth(0, 220);
    style_table(dd_table);

    struct DdRow {
        QString name;
        QString val;
        bool positive;
    };
    double var95 = risk["var_95_daily"].toDouble() * 100.0;
    double cvar95 = risk["cvar_95_daily"].toDouble() * 100.0;
    double ann_vol = risk["annualized_volatility"].toDouble() * 100.0;
    double down_dev = risk["downside_deviation"].toDouble() * 100.0;

    QVector<DdRow> drows = {
        {tr("Max Drawdown"), pct_str(max_dd), max_dd >= 0},
        {tr("Annualized Volatility"), pct_str(ann_vol), ann_vol < 20.0},
        {tr("VaR 95% (Daily)"), pct_str(var95), var95 >= 0},
        {tr("CVaR 95% (Daily)"), pct_str(cvar95), cvar95 >= 0},
        {tr("Downside Deviation"), pct_str(down_dev), down_dev < 15.0},
    };
    dd_table->setRowCount(drows.size());
    for (int i = 0; i < drows.size(); ++i) {
        dd_table->setRowHeight(i, 26);
        dd_table->setItem(
            i, 0, make_item(drows[i].name, Qt::AlignLeft | Qt::AlignVCenter, QColor(ui::colors::TEXT_SECONDARY())));
        dd_table->setItem(i, 1,
                          make_item(drows[i].val, Qt::AlignRight | Qt::AlignVCenter,
                                    QColor(drows[i].positive ? ui::colors::POSITIVE : ui::colors::NEGATIVE())));
    }
    dd_table->setMinimumHeight(26 * (static_cast<int>(drows.size()) + 1) + 4);
    cl->addWidget(dd_table);
    cl->addStretch();

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    drawdown_stack_->addWidget(scroll);
    drawdown_stack_->setCurrentIndex(1);
}

// ── update_rolling ────────────────────────────────────────────────────────────

void QuantStatsView::update_rolling() {
    if (qs_data_.isEmpty())
        return;

    if (rolling_stack_->count() > 1) {
        QWidget* old = rolling_stack_->widget(1);
        rolling_stack_->removeWidget(old);
        old->deleteLater();
    }

    auto* content = new QWidget(this);
    content->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(16, 12, 16, 12);
    cl->setSpacing(12);

    // Rolling statistics over a 63-day window: whether the risk-adjusted return and
    // the volatility are stable, or the full-period figure hides a regime change.
    // (This tab used to repeat the RATIOS rows of the METRICS tab under a ROLLING title.)
    const QJsonObject rolling = qs_data_["rolling"].toObject();
    const int window = rolling["window"].toInt(63);
    auto* title = new QLabel(tr("ROLLING %1-DAY METRICS — CURRENT WEIGHTS, BACK-TESTED").arg(window));
    title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    cl->addWidget(title);

    if (rolling.isEmpty()) {
        auto* msg = new QLabel(tr("Not enough daily history for a %1-day rolling window yet.").arg(window));
        msg->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY()));
        cl->addWidget(msg);
    } else {
        const QJsonArray dates = rolling["dates"].toArray();
        auto add_chart = [&](const QString& caption, const QVector<QsLine>& lines, const char* fmt) {
            auto* cap = new QLabel(caption);
            cap->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; letter-spacing:0.5px;")
                                   .arg(ui::colors::TEXT_SECONDARY()));
            cl->addWidget(cap);
            auto* view = qs_line_chart(dates, lines, fmt, content);
            view->setMinimumHeight(190);
            cl->addWidget(view);
        };
        add_chart(tr("ROLLING SHARPE RATIO (annualised)"),
                  {QsLine(tr("Sharpe"), rolling["sharpe"].toArray(), QColor(ui::colors::CYAN()), false, false, 1.0)},
                  "%.1f");
        add_chart(tr("ROLLING VOLATILITY (annualised)"),
                  {QsLine(tr("Volatility"), rolling["volatility"].toArray(), QColor(ui::colors::AMBER()))}, "%.0f%%");
        if (rolling.contains("beta")) {
            const QString bsym = qs_data_["benchmark"].toObject()["symbol"].toString();
            add_chart(tr("ROLLING BETA vs %1").arg(bsym.isEmpty() ? tr("benchmark") : bsym),
                      {QsLine(tr("Beta"), rolling["beta"].toArray(), QColor(ui::colors::WARNING()), false, false, 1.0)},
                      "%.2f");
        }
    }
    cl->addStretch();

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    rolling_stack_->addWidget(scroll);
    rolling_stack_->setCurrentIndex(1);
}

// ── update_monte_carlo_chart ──────────────────────────────────────────────────

void QuantStatsView::update_monte_carlo_chart() {
    if (mc_data_.isEmpty())
        return;

    // Remove previously built live widget if any
    if (mc_stack_->count() > 1) {
        QWidget* old = mc_stack_->widget(1);
        mc_stack_->removeWidget(old);
        old->deleteLater();
        mc_chart_ = nullptr;
    }

    auto* content = new QWidget(this);
    content->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);

    // ── Summary stats strip ───────────────────────────────────────────────────
    auto* stats_bar = new QWidget(this);
    stats_bar->setFixedHeight(56);
    stats_bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED()));
    auto* sb_lay = new QHBoxLayout(stats_bar);
    sb_lay->setContentsMargins(16, 4, 16, 4);
    sb_lay->setSpacing(24);

    double median_ret = mc_data_["median_return"].toDouble();
    double pct5 = mc_data_["percentile_5"].toDouble();
    double pct95 = mc_data_["percentile_95"].toDouble();
    double prob_loss = mc_data_["prob_loss"].toDouble();
    double exp_max_dd = mc_data_["expected_max_dd"].toDouble();

    struct StatItem {
        QString label;
        QString value;
        const char* color;
    };
    QVector<StatItem> stats = {
        {tr("MEDIAN RETURN"), pct_str(median_ret), median_ret >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()},
        {tr("5TH PERCENTILE"), pct_str(pct5), pct5 >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()},
        {tr("95TH PERCENTILE"), pct_str(pct95), ui::colors::POSITIVE()},
        {tr("PROB OF LOSS"), pct_str(prob_loss), ui::colors::WARNING()},
        {tr("EXP MAX DRAWDOWN"), pct_str(exp_max_dd), ui::colors::NEGATIVE()},
    };
    // Tail in money: the average of the worst 5% of outcomes, on today's NAV.
    if (mc_data_.contains("cvar_5")) {
        const double cvar5 = mc_data_["cvar_5"].toDouble();
        stats.append({tr("WORST 5% AVG"), pct_str(cvar5), ui::colors::NEGATIVE()});
        if (summary_.total_market_value > 0)
            stats.append({tr("≈ LOSS AT RISK"),
                          QString("%1 %2").arg(currency_,
                                               QLocale().toString(summary_.total_market_value * cvar5 / 100.0, 'f', 0)),
                          ui::colors::NEGATIVE()});
    }

    for (const auto& s : stats) {
        auto* col = new QWidget(this);
        auto* col_l = new QVBoxLayout(col);
        col_l->setContentsMargins(0, 0, 0, 0);
        col_l->setSpacing(2);
        auto* lbl = new QLabel(s.label);
        lbl->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; letter-spacing:0.5px;")
                               .arg(ui::colors::TEXT_TERTIARY()));
        col_l->addWidget(lbl);
        auto* val = new QLabel(s.value);
        val->setStyleSheet(QString("color:%1; font-size:14px; font-weight:700;").arg(s.color));
        col_l->addWidget(val);
        sb_lay->addWidget(col);
    }
    sb_lay->addStretch();
    cl->addWidget(stats_bar);

    // ── Monte Carlo paths chart ───────────────────────────────────────────────
    auto* chart = new QChart;
    chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart->setBackgroundRoundness(0);
    chart->setMargins(QMargins(8, 8, 8, 8));
    chart->legend()->hide();
    chart->setTitle("");

    // Parse paths array
    const QJsonArray paths_arr = mc_data_["paths"].toArray();
    const int n_paths = paths_arr.size();

    // Compute median path for highlighting
    const int n_steps = (n_paths > 0) ? paths_arr[0].toArray().size() : 0;
    QVector<double> median_path(n_steps, 0.0);
    if (n_paths > 0 && n_steps > 0) {
        for (int t = 0; t < n_steps; ++t) {
            QVector<double> vals;
            vals.reserve(n_paths);
            for (int p = 0; p < n_paths; ++p) {
                vals.append(paths_arr[p].toArray()[t].toDouble());
            }
            std::sort(vals.begin(), vals.end());
            median_path[t] = vals[vals.size() / 2];
        }
    }

    // Dim cyan for background paths
    QColor path_color(ui::colors::CYAN());
    path_color.setAlpha(40);
    QPen path_pen(path_color);
    path_pen.setWidth(1);

    for (int p = 0; p < n_paths; ++p) {
        const QJsonArray path_data = paths_arr[p].toArray();
        auto* series = new QLineSeries;
        series->setPen(path_pen);
        const int pts = path_data.size();
        for (int t = 0; t < pts; ++t) {
            series->append(static_cast<qreal>(t + 1), path_data[t].toDouble());
        }
        chart->addSeries(series);
    }

    // Percentile fan over ALL simulations (the script computes it per step); the median
    // used to come from the 50 paths drawn, which wobbles from run to run.
    const QJsonObject bands = mc_data_["percentile_paths"].toObject();
    if (!bands.isEmpty()) {
        const QJsonArray p50 = bands["p50"].toArray();
        if (p50.size() == n_steps) {
            for (int t = 0; t < n_steps; ++t)
                median_path[t] = p50[t].toDouble();
        }
        struct Band {
            const char* key;
            Qt::PenStyle style;
            QColor color;
        };
        const Band kBands[] = {
            {"p5", Qt::DashLine, QColor(ui::colors::NEGATIVE())},
            {"p25", Qt::DotLine, QColor(ui::colors::AMBER())},
            {"p75", Qt::DotLine, QColor(ui::colors::AMBER())},
            {"p95", Qt::DashLine, QColor(ui::colors::POSITIVE())},
        };
        for (const auto& b : kBands) {
            const QJsonArray vals = bands[QLatin1String(b.key)].toArray();
            if (vals.isEmpty())
                continue;
            auto* s = new QLineSeries;
            QPen pen(b.color);
            pen.setWidthF(1.5);
            pen.setStyle(b.style);
            s->setPen(pen);
            for (int t = 0; t < vals.size(); ++t)
                s->append(static_cast<qreal>(t + 1), vals[t].toDouble());
            chart->addSeries(s);
        }
    }

    // Median path — bright cyan
    if (n_steps > 0) {
        auto* median_series = new QLineSeries;
        QPen med_pen{QColor(ui::colors::CYAN())};
        med_pen.setWidth(2);
        median_series->setPen(med_pen);
        for (int t = 0; t < n_steps; ++t) {
            median_series->append(static_cast<qreal>(t + 1), median_path[t]);
        }
        chart->addSeries(median_series);
    }

    // Zero line
    auto* zero_series = new QLineSeries;
    QPen zero_pen{QColor(ui::colors::BORDER_BRIGHT())};
    zero_pen.setStyle(Qt::DashLine);
    zero_pen.setWidth(1);
    zero_series->setPen(zero_pen);
    zero_series->append(1, 0.0);
    zero_series->append(n_steps > 0 ? n_steps : 252, 0.0);
    chart->addSeries(zero_series);

    // Axes
    auto* x_axis = new QValueAxis;
    x_axis->setRange(1, n_steps > 0 ? n_steps : 252);
    x_axis->setTitleText(tr("Trading Days"));
    x_axis->setTitleBrush(QBrush(QColor(ui::colors::TEXT_SECONDARY())));
    x_axis->setLabelsBrush(QBrush(QColor(ui::colors::TEXT_TERTIARY())));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    x_axis->setLinePenColor(QColor(ui::colors::BORDER_MED()));
    x_axis->setLabelsFont(QFont("Consolas", 8));

    auto* y_axis = new QValueAxis;
    y_axis->setTitleText(tr("Cumulative Return (%)"));
    y_axis->setTitleBrush(QBrush(QColor(ui::colors::TEXT_SECONDARY())));
    y_axis->setLabelsBrush(QBrush(QColor(ui::colors::TEXT_TERTIARY())));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    y_axis->setLinePenColor(QColor(ui::colors::BORDER_MED()));
    y_axis->setLabelsFont(QFont("Consolas", 8));

    chart->addAxis(x_axis, Qt::AlignBottom);
    chart->addAxis(y_axis, Qt::AlignLeft);

    const auto& all_series = chart->series();
    for (auto* s : all_series) {
        s->attachAxis(x_axis);
        s->attachAxis(y_axis);
    }

    auto* chart_view = new QChartView(chart);
    chart_view->setRenderHint(QPainter::Antialiasing, false);
    chart_view->setStyleSheet(QString("background:%1; border:none;").arg(ui::colors::BG_BASE()));
    mc_chart_ = chart_view;

    cl->addWidget(chart_view, 1);

    // Caption
    int n_shown = mc_data_["num_paths_shown"].toInt();
    QString caption_text = tr("Showing %1 of %2 simulated paths over %3 trading days (GBM). Bright line = median; "
                              "dashed = 5th / 95th percentile, dotted = 25th / 75th, over all paths.")
                               .arg(n_shown)
                               .arg(mc_data_["num_simulations"].toInt(1000))
                               .arg(mc_data_["horizon_days"].toInt(252));
    if (mc_data_.contains("annual_drift"))
        caption_text += tr(" Calibrated to the mix's last 2 years: drift %1%/yr, volatility %2%/yr — a "
                           "projection of the past, not a forecast.")
                            .arg(QString::number(mc_data_["annual_drift"].toDouble(), 'f', 1),
                                 QString::number(mc_data_["annual_volatility"].toDouble(), 'f', 1));
    auto* caption = new QLabel(caption_text);
    caption->setWordWrap(true);
    caption->setStyleSheet(QString("color:%1; font-size:9px; padding:4px 16px;").arg(ui::colors::TEXT_TERTIARY()));
    cl->addWidget(caption);

    mc_stack_->addWidget(content);
    mc_stack_->setCurrentIndex(1);
}

// ── make_chart_view ───────────────────────────────────────────────────────────

QChartView* QuantStatsView::make_chart_view(const QString& title) {
    auto* chart = new QChart;
    chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart->setBackgroundRoundness(0);
    chart->setMargins(QMargins(8, 8, 8, 8));
    chart->legend()->hide();
    if (!title.isEmpty()) {
        chart->setTitle(title);
        chart->setTitleBrush(QBrush(QColor(ui::colors::AMBER())));
    }
    auto* view = new QChartView(chart, this);
    view->setRenderHint(QPainter::Antialiasing, false);
    view->setStyleSheet(QString("background:%1; border:none;").arg(ui::colors::BG_BASE()));
    return view;
}

// ── run_quantstats ────────────────────────────────────────────────────────────

void QuantStatsView::run_quantstats() {
    if (qs_running_ || summary_.holdings.isEmpty())
        return;
    qs_running_ = true;
    qs_run_btn_->setEnabled(false);
    qs_status_->setText(tr("Fetching 1-year price history..."));
    qs_status_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::AMBER()));

    QStringList symbols;
    QList<double> weights;
    for (const auto& h : summary_.holdings) {
        symbols.append(h.symbol);
        weights.append(h.weight / 100.0);
    }

    // Live risk-free rate and the portfolio currency's benchmark, so the
    // BENCHMARK column is filled and Sharpe/Sortino use the same rf as the
    // rest of the tab (the script used to hard-code 4%).
    auto& psvc = services::PortfolioService::instance();
    const QString bench = services::PortfolioService::default_benchmark_for_currency(summary_.portfolio.currency);

    QPointer<QuantStatsView> self = this;
    PortfolioAnalyticsService::instance().run_quantstats(symbols, weights, psvc.risk_free_rate(), bench,
                                                         [self](const AnalyticsResult& r) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, r]() {
                if (!self)
                    return;
                self->qs_running_ = false;
                self->qs_run_btn_->setEnabled(true);

                if (!r.success) {
                    self->qs_status_->setText(tr("QuantStats: %1").arg(r.error));
                    self->qs_status_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::NEGATIVE()));
                    return;
                }

                self->qs_data_ = r.data;
                self->qs_status_->setText(tr("Complete"));
                self->qs_status_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::POSITIVE()));
                self->update_metrics();
                self->update_returns();
                self->update_drawdown();
                self->update_rolling();
            },
            Qt::QueuedConnection);
    });
}

// ── run_monte_carlo ───────────────────────────────────────────────────────────

void QuantStatsView::run_monte_carlo() {
    if (mc_running_ || summary_.holdings.isEmpty())
        return;
    mc_running_ = true;
    mc_run_btn_->setEnabled(false);
    mc_status_->setText(tr("Running 1000 simulation paths..."));
    mc_status_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::AMBER()));

    QStringList symbols;
    QList<double> weights;
    for (const auto& h : summary_.holdings) {
        symbols.append(h.symbol);
        weights.append(h.weight / 100.0);
    }

    QPointer<QuantStatsView> self = this;
    PortfolioAnalyticsService::instance().run_monte_carlo(
        symbols, weights, /*num_simulations=*/1000, [self](const AnalyticsResult& r) {
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, r]() {
                    if (!self)
                        return;
                    self->mc_running_ = false;
                    self->mc_run_btn_->setEnabled(true);

                    if (!r.success) {
                        self->mc_status_->setText(tr("Monte Carlo: %1").arg(r.error));
                        self->mc_status_->setStyleSheet(
                            QString("color:%1; font-size:10px;").arg(ui::colors::NEGATIVE()));
                        return;
                    }

                    self->mc_data_ = r.data;
                    self->mc_status_->setText(
                        tr("Complete — %1 paths simulated").arg(r.data["num_paths_shown"].toInt() > 0 ? "1000" : "0"));
                    self->mc_status_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::POSITIVE()));
                    self->update_monte_carlo_chart();
                },
                Qt::QueuedConnection);
        });
}

void QuantStatsView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void QuantStatsView::retranslateUi() {
    if (tabs_) {
        if (metrics_tab_index_ >= 0)
            tabs_->setTabText(metrics_tab_index_, tr("METRICS"));
        if (returns_tab_index_ >= 0)
            tabs_->setTabText(returns_tab_index_, tr("RETURNS"));
        if (drawdown_tab_index_ >= 0)
            tabs_->setTabText(drawdown_tab_index_, tr("DRAWDOWN"));
        if (rolling_tab_index_ >= 0)
            tabs_->setTabText(rolling_tab_index_, tr("ROLLING"));
        if (mc_tab_index_ >= 0)
            tabs_->setTabText(mc_tab_index_, tr("MONTE CARLO"));
    }
    if (title_lbl_)
        title_lbl_->setText(tr("QUANTSTATS ANALYSIS"));
    if (qs_run_btn_)
        qs_run_btn_->setText(tr("▶ RUN QUANTSTATS"));
    if (metrics_section_lbl_)
        metrics_section_lbl_->setText(tr("KEY PERFORMANCE INDICATORS"));
    if (mc_title_)
        mc_title_->setText(tr("MONTE CARLO SIMULATION"));
    if (mc_desc_)
        mc_desc_->setText(tr("Simulate 1,000 portfolio return paths using GBM to estimate probability\n"
                             "distributions of future returns, drawdowns, and terminal wealth."));
    if (mc_run_btn_)
        mc_run_btn_->setText(tr("▶ RUN MONTE CARLO (1000 paths)"));

    if (returns_placeholder_)
        returns_placeholder_->setText(tr("Run QuantStats Analysis for return distribution"));
    if (drawdown_placeholder_)
        drawdown_placeholder_->setText(tr("Run QuantStats Analysis for drawdown metrics"));
    if (rolling_placeholder_)
        rolling_placeholder_->setText(tr("Run QuantStats Analysis for rolling metrics"));
    if (mc_placeholder_)
        mc_placeholder_->setText(tr("Press RUN MONTE CARLO to simulate 1,000 return paths"));

    if (metrics_table_)
        metrics_table_->setHorizontalHeaderLabels({tr("METRIC"), tr("VALUE"), tr("BENCHMARK")});

    // re-render tabs whose stacks already hold live content. The update_* methods
    // pluck source-key tr() strings on each call, so re-running them picks up the
    // new locale for section labels, row names, and embedded sub-table headers.
    update_metrics();
    if (returns_stack_ && returns_stack_->count() > 1)
        update_returns();
    if (drawdown_stack_ && drawdown_stack_->count() > 1)
        update_drawdown();
    if (rolling_stack_ && rolling_stack_->count() > 1)
        update_rolling();
    if (mc_stack_ && mc_stack_->count() > 1)
        update_monte_carlo_chart();
}

} // namespace fincept::screens
