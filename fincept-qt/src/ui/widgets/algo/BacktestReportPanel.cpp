// src/ui/widgets/algo/BacktestReportPanel.cpp
#include "ui/widgets/algo/BacktestReportPanel.h"

#include "core/currency/Currency.h"

#include <QChart>
#include <QChartView>
#include <QColor>
#include <QFont>
#include <QGridLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QLegend>
#include <QLineSeries>
#include <QMap>
#include <QMargins>
#include <QPainter>
#include <QPen>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QValueAxis>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::ui::algo {

namespace {
constexpr char kPos[] = "#16A34A";
constexpr char kNeg[] = "#DC2626";

QChartView* make_chart_view(QChart* chart) {
    chart->legend()->hide();
    chart->setBackgroundBrush(QColor("#0E0E0E"));
    chart->setMargins(QMargins(4, 4, 4, 4));
    auto* view = new QChartView(chart);
    view->setRenderHint(QPainter::Antialiasing);
    view->setMinimumHeight(150);
    return view;
}

void style_axis(QValueAxis* ax) {
    ax->setLabelsColor(QColor("#9CA3AF"));
    ax->setGridLineColor(QColor("#1F2937"));
    ax->setLinePenColor(QColor("#374151"));
    ax->setLabelsFont(QFont(QStringLiteral("monospace"), 7));
}
} // namespace

BacktestReportPanel::BacktestReportPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("backtestReportPanel"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    placeholder_ = new QLabel(tr("Run a backtest to see performance, equity curve and trades."), this);
    placeholder_->setObjectName(QStringLiteral("builderChartLabel"));
    placeholder_->setAlignment(Qt::AlignCenter);
    placeholder_->setWordWrap(true);
    placeholder_->setMinimumHeight(120);
    root->addWidget(placeholder_);

    body_ = new QWidget(this);
    auto* body_layout = new QVBoxLayout(body_);
    body_layout->setContentsMargins(0, 0, 0, 0);
    body_layout->setSpacing(8);
    body_layout->addWidget(build_kpis());
    body_layout->addWidget(build_charts());
    body_layout->addWidget(build_heatmap());
    body_layout->addWidget(build_trades(), 1);
    body_->setVisible(false);
    root->addWidget(body_, 1);
}

void BacktestReportPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void BacktestReportPanel::retranslateUi() {
    if (placeholder_)
        placeholder_->setText(tr("Run a backtest to see performance, equity curve and trades."));

    // KPI titles — keyed lookup so each card's fixed header re-translates.
    auto kpi_title = [this](const QString& key, const QString& text) {
        if (auto* l = kpi_title_.value(key, nullptr))
            l->setText(text);
    };
    kpi_title("total_return", tr("TOTAL RETURN"));
    kpi_title("sharpe", tr("SHARPE"));
    kpi_title("sortino", tr("SORTINO"));
    kpi_title("max_dd", tr("MAX DRAWDOWN"));
    kpi_title("calmar", tr("CALMAR"));
    kpi_title("win_rate", tr("WIN RATE"));
    kpi_title("profit_factor", tr("PROFIT FACTOR"));
    kpi_title("trades", tr("TRADES"));
    kpi_title("expectancy", tr("EXPECTANCY"));
    kpi_title("avg_bars", tr("AVG BARS HELD"));

    if (equity_chart_) equity_chart_->setTitle(tr("Equity Curve"));
    if (equity_series_) equity_series_->setName(tr("Strategy"));
    if (benchmark_series_) benchmark_series_->setName(tr("Buy & Hold"));
    if (dd_chart_) dd_chart_->setTitle(tr("Drawdown %"));
    if (heatmap_title_) heatmap_title_->setText(tr("MONTHLY RETURNS %"));

    if (trades_) {
        const QStringList headers = {tr("#"),     tr("Entry"), tr("Exit"), tr("Entry %1").arg(cur::symbol()),
                                     tr("Exit %1").arg(cur::symbol()), tr("Qty"),   tr("P&L"),  tr("P&L %"),
                                     tr("Bars"),   tr("Reason")};
        trades_->setHorizontalHeaderLabels(headers);
    }
}

void BacktestReportPanel::add_kpi(QGridLayout* grid, int row, int col,
                                  const QString& key, const QString& title) {
    auto* card = new QWidget(this);
    card->setObjectName(QStringLiteral("kpiCard"));
    auto* l = new QVBoxLayout(card);
    l->setContentsMargins(8, 6, 8, 6);
    l->setSpacing(2);
    auto* t = new QLabel(title, card);
    t->setObjectName(QStringLiteral("kpiTitle"));
    auto* v = new QLabel(QStringLiteral("--"), card);
    v->setObjectName(QStringLiteral("kpiValue"));
    auto* s = new QLabel(card);
    s->setObjectName(QStringLiteral("kpiSub"));
    l->addWidget(t);
    l->addWidget(v);
    l->addWidget(s);
    grid->addWidget(card, row, col);
    kpi_val_[key] = v;
    kpi_sub_[key] = s;
    kpi_title_[key] = t;
}

QWidget* BacktestReportPanel::build_kpis() {
    auto* w = new QWidget(this);
    auto* grid = new QGridLayout(w);
    grid->setSpacing(8);
    add_kpi(grid, 0, 0, "total_return", tr("TOTAL RETURN"));
    add_kpi(grid, 0, 1, "sharpe", tr("SHARPE"));
    add_kpi(grid, 0, 2, "sortino", tr("SORTINO"));
    add_kpi(grid, 0, 3, "max_dd", tr("MAX DRAWDOWN"));
    add_kpi(grid, 1, 0, "calmar", tr("CALMAR"));
    add_kpi(grid, 1, 1, "win_rate", tr("WIN RATE"));
    add_kpi(grid, 1, 2, "profit_factor", tr("PROFIT FACTOR"));
    add_kpi(grid, 1, 3, "trades", tr("TRADES"));
    add_kpi(grid, 2, 0, "expectancy", tr("EXPECTANCY"));
    add_kpi(grid, 2, 1, "avg_bars", tr("AVG BARS HELD"));
    return w;
}

QWidget* BacktestReportPanel::build_charts() {
    auto* w = new QWidget(this);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(6);

    // Equity curve
    equity_chart_ = new QChart();
    equity_chart_->setTitle(tr("Equity Curve"));
    equity_chart_->setTitleBrush(QColor("#E5E7EB"));
    equity_series_ = new QLineSeries();
    equity_series_->setName(tr("Strategy"));
    equity_series_->setColor(QColor("#3B82F6"));
    equity_chart_->addSeries(equity_series_);
    benchmark_series_ = new QLineSeries();
    benchmark_series_->setName(tr("Buy & Hold"));
    QPen bpen(QColor("#9CA3AF"));
    bpen.setStyle(Qt::DashLine);
    benchmark_series_->setPen(bpen);
    equity_chart_->addSeries(benchmark_series_);
    eq_x_ = new QValueAxis();
    eq_y_ = new QValueAxis();
    style_axis(eq_x_);
    style_axis(eq_y_);
    eq_x_->setLabelFormat("%d");
    eq_y_->setLabelFormat("%.0f");
    equity_chart_->addAxis(eq_x_, Qt::AlignBottom);
    equity_chart_->addAxis(eq_y_, Qt::AlignLeft);
    equity_series_->attachAxis(eq_x_);
    equity_series_->attachAxis(eq_y_);
    benchmark_series_->attachAxis(eq_x_);
    benchmark_series_->attachAxis(eq_y_);
    auto* eq_view = make_chart_view(equity_chart_);
    equity_chart_->legend()->setVisible(true);
    equity_chart_->legend()->setLabelColor(QColor("#9CA3AF"));
    l->addWidget(eq_view);

    // Drawdown / underwater
    dd_chart_ = new QChart();
    dd_chart_->setTitle(tr("Drawdown %"));
    dd_chart_->setTitleBrush(QColor("#E5E7EB"));
    dd_series_ = new QLineSeries();
    dd_series_->setColor(QColor(kNeg));
    dd_chart_->addSeries(dd_series_);
    dd_x_ = new QValueAxis();
    dd_y_ = new QValueAxis();
    style_axis(dd_x_);
    style_axis(dd_y_);
    dd_x_->setLabelFormat("%d");
    dd_y_->setLabelFormat("%.1f");
    dd_chart_->addAxis(dd_x_, Qt::AlignBottom);
    dd_chart_->addAxis(dd_y_, Qt::AlignLeft);
    dd_series_->attachAxis(dd_x_);
    dd_series_->attachAxis(dd_y_);
    l->addWidget(make_chart_view(dd_chart_));

    return w;
}

QWidget* BacktestReportPanel::build_heatmap() {
    auto* w = new QWidget(this);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(4);
    heatmap_title_ = new QLabel(tr("MONTHLY RETURNS %"), w);
    heatmap_title_->setObjectName(QStringLiteral("kpiTitle"));
    l->addWidget(heatmap_title_);
    auto* host = new QWidget(w);
    heatmap_grid_ = new QGridLayout(host);
    heatmap_grid_->setSpacing(2);
    heatmap_grid_->setContentsMargins(0, 0, 0, 0);
    l->addWidget(host);
    return w;
}

void BacktestReportPanel::populate_heatmap(const QJsonObject& payload) {
    while (QLayoutItem* it = heatmap_grid_->takeAt(0)) {
        if (it->widget())
            it->widget()->deleteLater();
        delete it;
    }

    QMap<int, QMap<int, double>> by_year; // year -> month(1-12) -> return%
    for (const auto& v : payload.value("monthly_returns").toArray()) {
        const QString ym = v.toObject().value("month").toString(); // "yyyy-MM"
        const int y = ym.left(4).toInt();
        const int m = ym.mid(5, 2).toInt();
        if (y > 0 && m >= 1 && m <= 12)
            by_year[y][m] = v.toObject().value("return").toDouble();
    }

    static const char* mlabels[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int m = 0; m < 12; ++m) {
        auto* h = new QLabel(QString::fromLatin1(mlabels[m]));
        h->setAlignment(Qt::AlignCenter);
        h->setStyleSheet(QStringLiteral("color:#9CA3AF;font-size:8px;"));
        heatmap_grid_->addWidget(h, 0, m + 1);
    }

    int row = 1;
    for (auto y = by_year.constBegin(); y != by_year.constEnd(); ++y, ++row) {
        auto* yl = new QLabel(QString::number(y.key()));
        yl->setStyleSheet(QStringLiteral("color:#9CA3AF;font-size:8px;"));
        heatmap_grid_->addWidget(yl, row, 0);
        for (int m = 1; m <= 12; ++m) {
            auto* cell = new QLabel();
            cell->setAlignment(Qt::AlignCenter);
            cell->setFixedHeight(18);
            cell->setMinimumWidth(32);
            if (y.value().contains(m)) {
                const double r = y.value().value(m);
                cell->setText(QString::number(r, 'f', 1));
                QColor c(r >= 0 ? "#16A34A" : "#DC2626");
                c.setAlphaF(std::min(0.85, 0.15 + std::abs(r) / 20.0));
                cell->setStyleSheet(
                    QStringLiteral("background:%1;color:#E5E7EB;font-size:8px;border-radius:2px;")
                        .arg(c.name(QColor::HexArgb)));
            } else {
                cell->setStyleSheet(QStringLiteral("background:#161616;border-radius:2px;"));
            }
            heatmap_grid_->addWidget(cell, row, m);
        }
    }
}

QWidget* BacktestReportPanel::build_trades() {
    trades_ = new QTableWidget(this);
    trades_->setObjectName(QStringLiteral("backtestTradeTable"));
    const QStringList headers = {tr("#"),     tr("Entry"), tr("Exit"), tr("Entry %1").arg(cur::symbol()),
                                 tr("Exit %1").arg(cur::symbol()), tr("Qty"),   tr("P&L"),  tr("P&L %"),
                                 tr("Bars"),   tr("Reason")};
    trades_->setColumnCount(headers.size());
    trades_->setHorizontalHeaderLabels(headers);
    trades_->verticalHeader()->setVisible(false);
    trades_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    trades_->setSelectionBehavior(QAbstractItemView::SelectRows);
    trades_->horizontalHeader()->setStretchLastSection(true);
    trades_->setMinimumHeight(160);
    return trades_;
}

void BacktestReportPanel::set_result(const QJsonObject& payload) {
    placeholder_->setVisible(false);
    body_->setVisible(true);

    auto d = [&](const char* k) { return payload.value(QLatin1String(k)).toDouble(); };

    const double tr_pct = d("total_return");
    kpi_val_["total_return"]->setText(QStringLiteral("%1%2%")
                                          .arg(tr_pct >= 0 ? "+" : "").arg(tr_pct, 0, 'f', 2));
    kpi_val_["total_return"]->setStyleSheet(QStringLiteral("color:%1;").arg(tr_pct >= 0 ? kPos : kNeg));
    kpi_sub_["total_return"]->setText(tr("Final %1%2").arg(cur::symbol()).arg(d("final_value"), 0, 'f', 0));

    const double sharpe = d("sharpe_ratio");
    kpi_val_["sharpe"]->setText(QString::number(sharpe, 'f', 2));
    kpi_sub_["sharpe"]->setText(sharpe >= 1.0 ? tr("Excellent") : sharpe >= 0.5 ? tr("Good") : tr("Weak"));

    kpi_val_["sortino"]->setText(QString::number(d("sortino"), 'f', 2));
    kpi_sub_["sortino"]->setText(tr("Downside-adj."));

    const double mdd = d("max_drawdown");
    kpi_val_["max_dd"]->setText(QStringLiteral("-%1%").arg(qAbs(mdd), 0, 'f', 2));
    kpi_sub_["max_dd"]->setText(tr("Peak-to-trough"));

    kpi_val_["calmar"]->setText(QString::number(d("calmar"), 'f', 2));
    kpi_sub_["calmar"]->setText(tr("Return / MaxDD"));

    const double wr = d("win_rate");
    kpi_val_["win_rate"]->setText(QStringLiteral("%1%").arg(wr, 0, 'f', 1));
    kpi_sub_["win_rate"]->setText(wr >= 50.0 ? tr("Above average") : tr("Below average"));

    const double pf = d("profit_factor");
    kpi_val_["profit_factor"]->setText(QString::number(pf, 'f', 2));
    kpi_sub_["profit_factor"]->setText(pf >= 1.5 ? tr("Strong") : pf >= 1.0 ? tr("Profitable") : tr("Losing"));

    kpi_val_["trades"]->setText(QString::number(payload.value("total_trades").toInt()));
    kpi_sub_["trades"]->setText(tr("%1W / %2L")
                                    .arg(payload.value("winning_trades").toInt())
                                    .arg(payload.value("losing_trades").toInt()));

    const double exp = d("expectancy");
    kpi_val_["expectancy"]->setText(QStringLiteral("%1%2").arg(exp >= 0 ? "+" : "").arg(exp, 0, 'f', 2));
    kpi_sub_["expectancy"]->setText(tr("Avg P&L / trade"));

    kpi_val_["avg_bars"]->setText(QString::number(d("avg_bars_held"), 'f', 1));
    kpi_sub_["avg_bars"]->setText(tr("Holding period"));

    // ── Equity curve + benchmark + drawdown ─────────────────────────────────
    equity_series_->clear();
    benchmark_series_->clear();
    dd_series_->clear();
    const QJsonArray eq = payload.value("equity_curve").toArray();
    const QJsonArray bench = payload.value("benchmark_curve").toArray();
    double ymin = std::numeric_limits<double>::max();
    double ymax = std::numeric_limits<double>::lowest();
    double peak = 0.0, dd_min = 0.0;
    for (int i = 0; i < eq.size(); ++i) {
        const double v = eq[i].toDouble();
        equity_series_->append(i, v);
        ymin = std::min(ymin, v);
        ymax = std::max(ymax, v);
        if (i < bench.size()) {
            const double b = bench[i].toDouble();
            benchmark_series_->append(i, b);
            ymin = std::min(ymin, b);
            ymax = std::max(ymax, b);
        }
        peak = std::max(peak, v);
        const double dd = peak > 0.0 ? (v - peak) / peak * 100.0 : 0.0;
        dd_series_->append(i, dd);
        dd_min = std::min(dd_min, dd);
    }
    const int last = std::max<int>(1, static_cast<int>(eq.size()) - 1);
    eq_x_->setRange(0, last);
    dd_x_->setRange(0, last);
    if (eq.isEmpty()) {
        eq_y_->setRange(0, 1);
        dd_y_->setRange(-1, 0);
    } else {
        const double pad = std::max(1.0, (ymax - ymin) * 0.05);
        eq_y_->setRange(ymin - pad, ymax + pad);
        dd_y_->setRange(dd_min * 1.1 - 0.1, 0.5);
    }

    // ── Trade table ─────────────────────────────────────────────────────────
    const QJsonArray trades = payload.value("trades").toArray();
    trades_->setRowCount(trades.size());
    for (int i = 0; i < trades.size(); ++i) {
        const QJsonObject t = trades[i].toObject();
        const double pnl = t.value("pnl").toDouble();
        const double pnl_pct = t.value("pnl_pct").toDouble();
        auto cell = [&](int col, const QString& text, bool color_pnl = false) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col >= 3 ? Qt::AlignRight | Qt::AlignVCenter
                                            : Qt::AlignLeft | Qt::AlignVCenter);
            if (color_pnl)
                item->setForeground(QColor(pnl >= 0 ? kPos : kNeg));
            trades_->setItem(i, col, item);
        };
        cell(0, QString::number(i + 1));
        cell(1, QString::number(t.value("entry_bar").toInt()));
        cell(2, QString::number(t.value("exit_bar").toInt()));
        cell(3, QString::number(t.value("entry_price").toDouble(), 'f', 2));
        cell(4, QString::number(t.value("exit_price").toDouble(), 'f', 2));
        cell(5, QString::number(t.value("shares").toDouble(), 'f', 0));
        cell(6, QStringLiteral("%1%2").arg(pnl >= 0 ? "+" : "").arg(pnl, 0, 'f', 2), true);
        cell(7, QStringLiteral("%1%2%").arg(pnl_pct >= 0 ? "+" : "").arg(pnl_pct, 0, 'f', 2), true);
        cell(8, QString::number(t.value("bars_held").toInt()));
        cell(9, t.value("reason").toString());
    }

    populate_heatmap(payload);
}

void BacktestReportPanel::clear() {
    placeholder_->setVisible(true);
    body_->setVisible(false);
}

} // namespace fincept::ui::algo
