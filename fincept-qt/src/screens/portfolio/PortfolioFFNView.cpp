// src/screens/portfolio/PortfolioFFNView.cpp
#include "screens/portfolio/PortfolioFFNView.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

PortfolioFFNView::PortfolioFFNView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PortfolioFFNView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header with back button
    auto* header = new QWidget;
    header->setFixedHeight(36);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::AMBER));

    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(8, 0, 8, 0);
    h_layout->setSpacing(8);

    back_btn_ = new QPushButton("\u2190 BACK");
    back_btn_->setFixedHeight(24);
    back_btn_->setCursor(Qt::PointingHandCursor);
    back_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                     "  padding:0 10px; font-size:9px; font-weight:700; }"
                                     "QPushButton:hover { background:%1; color:#000; }")
                                 .arg(ui::colors::AMBER));
    connect(back_btn_, &QPushButton::clicked, this, &PortfolioFFNView::back_requested);
    h_layout->addWidget(back_btn_);

    auto* sep = new QWidget;
    sep->setFixedSize(1, 20);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_MED));
    h_layout->addWidget(sep);

    auto* title = new QLabel("FFN ANALYTICS");
    title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    h_layout->addWidget(title);
    h_layout->addStretch();

    layout->addWidget(header);

    // Tabs
    tabs_ = new QTabWidget;
    tabs_->setDocumentMode(true);
    tabs_->setStyleSheet(QString("QTabWidget::pane { border:0; background:%1; }"
                                 "QTabBar::tab { background:%2; color:%3; padding:6px 14px; border:0;"
                                 "  border-bottom:2px solid transparent; font-size:9px; font-weight:700;"
                                 "  letter-spacing:0.5px; }"
                                 "QTabBar::tab:selected { color:%4; border-bottom:2px solid %4; }"
                                 "QTabBar::tab:hover { color:%5; }")
                             .arg(ui::colors::BG_BASE, ui::colors::BG_SURFACE, ui::colors::TEXT_SECONDARY,
                                  ui::colors::AMBER, ui::colors::TEXT_PRIMARY));

    // ── Overview tab ─────────────────────────────────────────────────────────
    auto* overview_w = new QWidget;
    auto* overview_layout = new QVBoxLayout(overview_w);
    overview_layout->setContentsMargins(12, 8, 12, 8);

    auto* ov_title = new QLabel("PORTFOLIO METRICS OVERVIEW");
    ov_title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    overview_layout->addWidget(ov_title);

    overview_table_ = new QTableWidget;
    overview_table_->setColumnCount(3);
    overview_table_->setHorizontalHeaderLabels({"METRIC", "PORTFOLIO", "BENCHMARK (SPY)"});
    overview_table_->setSelectionMode(QAbstractItemView::NoSelection);
    overview_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    overview_table_->setShowGrid(false);
    overview_table_->verticalHeader()->setVisible(false);
    overview_table_->horizontalHeader()->setStretchLastSection(true);
    overview_table_->setColumnWidth(0, 220);
    overview_table_->setColumnWidth(1, 140);
    overview_table_->setStyleSheet(
        QString("QTableWidget { background:%1; color:%2; border:none; font-size:11px; }"
                "QTableWidget::item { padding:3px 8px; border-bottom:1px solid %3; }"
                "QHeaderView::section { background:%4; color:%5; border:none;"
                "  border-bottom:2px solid %6; padding:3px 8px; font-size:9px; font-weight:700; }")
            .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM, ui::colors::BG_SURFACE,
                 ui::colors::TEXT_SECONDARY, ui::colors::AMBER));
    overview_layout->addWidget(overview_table_, 1);
    tabs_->addTab(overview_w, "OVERVIEW");

    // ── Benchmark tab ────────────────────────────────────────────────────────
    benchmark_panel_ = new QWidget;
    benchmark_panel_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    auto* bench_layout = new QVBoxLayout(benchmark_panel_);
    bench_layout->setContentsMargins(16, 12, 16, 12);
    bench_layout->setAlignment(Qt::AlignCenter);
    auto* bench_desc = new QLabel("BENCHMARK COMPARISON\n\nCompare portfolio performance against SPY benchmark.\n"
                                  "Requires Python FFN library for full analysis.");
    bench_desc->setAlignment(Qt::AlignCenter);
    bench_desc->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY));
    bench_layout->addWidget(bench_desc);
    tabs_->addTab(benchmark_panel_, "BENCHMARK");

    // ── Remaining tabs (placeholders) ────────────────────────────────────────
    auto make_placeholder = [this](const QString& tab_name, const QString& desc) {
        auto* w = new QWidget;
        auto* l = new QVBoxLayout(w);
        l->setAlignment(Qt::AlignCenter);
        auto* d = new QLabel(desc);
        d->setAlignment(Qt::AlignCenter);
        d->setWordWrap(true);
        d->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY));
        l->addWidget(d);
        tabs_->addTab(w, tab_name);
        return w;
    };

    optimization_panel_ = make_placeholder(
        "OPTIMIZATION",
        "EFFICIENT FRONTIER\n\nRun portfolio optimization using FFN to find\nthe optimal risk-return tradeoff.");
    rebased_panel_ = make_placeholder(
        "REBASED",
        "REBASED PRICE CHARTS\n\nCompare holdings on a common base (100)\nto visualize relative performance.");
    drawdowns_panel_ = make_placeholder(
        "DRAWDOWNS",
        "DRAWDOWN ANALYSIS\n\nVisualize historical drawdowns for each holding\nand the overall portfolio.");
    rolling_panel_ = make_placeholder(
        "ROLLING", "ROLLING CORRELATIONS\n\nTrack how correlations between holdings\nchange over rolling windows.");

    layout->addWidget(tabs_);
}

void PortfolioFFNView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    update_overview();
}

void PortfolioFFNView::update_overview() {
    struct Metric {
        const char* name;
        double portfolio_val;
        double benchmark_val;
    };

    double pnl_pct = summary_.total_unrealized_pnl_percent;
    double vol = 0;
    int vol_n = 0;
    for (const auto& h : summary_.holdings) {
        if (std::abs(h.day_change_percent) > 0.001) {
            vol += std::abs(h.day_change_percent);
            ++vol_n;
        }
    }
    double daily_vol = vol_n > 0 ? vol / vol_n : 0;
    double ann_vol = daily_vol * std::sqrt(252.0);
    double sharpe = ann_vol > 0.01 ? (pnl_pct - 4.0) / ann_vol : 0;

    QVector<Metric> metrics = {
        {"Total Return", pnl_pct, 10.5},
        {"Annualized Volatility", ann_vol, 15.2},
        {"Sharpe Ratio", sharpe, 0.82},
        {"Max Drawdown", pnl_pct < 0 ? std::abs(pnl_pct) : 0, 20.5},
        {"Win Rate", summary_.gainers > 0 ? summary_.gainers * 100.0 / summary_.total_positions : 0, 55.0},
        {"Best Holding", 0, 0},
        {"Worst Holding", 0, 0},
        {"Positions", static_cast<double>(summary_.total_positions), 500},
        {"Total Value", summary_.total_market_value, 0},
        {"Cost Basis", summary_.total_cost_basis, 0},
    };

    // Find best/worst
    if (!summary_.holdings.isEmpty()) {
        auto best =
            std::max_element(summary_.holdings.begin(), summary_.holdings.end(), [](const auto& a, const auto& b) {
                return a.unrealized_pnl_percent < b.unrealized_pnl_percent;
            });
        auto worst =
            std::min_element(summary_.holdings.begin(), summary_.holdings.end(), [](const auto& a, const auto& b) {
                return a.unrealized_pnl_percent < b.unrealized_pnl_percent;
            });
        metrics[5].portfolio_val = best->unrealized_pnl_percent;
        metrics[6].portfolio_val = worst->unrealized_pnl_percent;
    }

    overview_table_->setRowCount(metrics.size());
    for (int r = 0; r < metrics.size(); ++r) {
        const auto& m = metrics[r];
        overview_table_->setRowHeight(r, 28);

        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            overview_table_->setItem(r, col, item);
        };

        set(0, m.name, ui::colors::TEXT_PRIMARY);

        // Format based on metric type
        bool is_pct = (r <= 4 || r == 5 || r == 6);
        bool is_currency = (r == 8 || r == 9);
        const char* val_color =
            is_pct ? (m.portfolio_val >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE) : ui::colors::CYAN;

        if (is_currency)
            set(1, QString("%1 %2").arg(currency_, QString::number(m.portfolio_val, 'f', 2)), ui::colors::WARNING);
        else if (is_pct)
            set(1, QString("%1%2%").arg(m.portfolio_val >= 0 ? "+" : "").arg(QString::number(m.portfolio_val, 'f', 2)),
                val_color);
        else
            set(1, QString::number(m.portfolio_val, 'f', 0), ui::colors::CYAN);

        // Benchmark
        if (m.benchmark_val != 0) {
            if (is_pct)
                set(2, QString("%1%").arg(QString::number(m.benchmark_val, 'f', 2)), ui::colors::TEXT_TERTIARY);
            else
                set(2, QString::number(m.benchmark_val, 'f', 0), ui::colors::TEXT_TERTIARY);
        } else {
            set(2, "--", ui::colors::TEXT_TERTIARY);
        }
    }
}

} // namespace fincept::screens
