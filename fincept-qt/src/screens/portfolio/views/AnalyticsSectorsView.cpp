// src/screens/portfolio/views/AnalyticsSectorsView.cpp
#include "screens/portfolio/views/AnalyticsSectorsView.h"

#include "ui/theme/Theme.h"

#include <QChart>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPieSeries>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

static const QColor kSectorPalette[] = {
    QColor("#0891b2"), QColor("#16a34a"), QColor("#d97706"), QColor("#ca8a04"), QColor("#9333ea"), QColor("#84cc16"),
    QColor("#0d9488"), QColor("#f97316"), QColor("#60a5fa"), QColor("#f43f5e"), QColor("#a78bfa"), QColor("#fbbf24"),
    QColor("#2dd4bf"), QColor("#fb923c"), QColor("#818cf8"), QColor("#e879f9"),
};

QColor AnalyticsSectorsView::sector_color(int index) {
    return kSectorPalette[index % 16];
}

QString AnalyticsSectorsView::infer_sector(const QString& symbol) {
    static const QHash<QString, QString> known = {
        {"AAPL", "Technology"},
        {"MSFT", "Technology"},
        {"GOOGL", "Technology"},
        {"AMZN", "Technology"},
        {"NVDA", "Technology"},
        {"META", "Technology"},
        {"TSLA", "Consumer Cyclical"},
        {"JPM", "Financial Services"},
        {"JNJ", "Healthcare"},
        {"UNH", "Healthcare"},
        {"PFE", "Healthcare"},
        {"XOM", "Energy"},
        {"CVX", "Energy"},
        {"V", "Financial Services"},
        {"MA", "Financial Services"},
        {"WMT", "Consumer Defensive"},
        {"DIS", "Communication"},
        {"SPY", "US Equity"},
        {"QQQ", "US Equity"},
        {"BTC-USD", "Cryptocurrency"},
        {"ETH-USD", "Cryptocurrency"},
    };
    auto it = known.find(symbol.toUpper());
    return it != known.end() ? *it : "Other";
}

AnalyticsSectorsView::AnalyticsSectorsView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void AnalyticsSectorsView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tabs_ = new QTabWidget;
    tabs_->setDocumentMode(true);
    tabs_->setStyleSheet(QString("QTabWidget::pane { border:0; background:%1; }"
                                 "QTabBar::tab { background:%2; color:%3; padding:8px 18px; border:0;"
                                 "  border-bottom:2px solid transparent; font-size:10px; font-weight:700;"
                                 "  letter-spacing:1px; text-transform:uppercase; }"
                                 "QTabBar::tab:selected { color:%4; border-bottom:2px solid %4; }"
                                 "QTabBar::tab:hover { color:%5; }")
                             .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(),
                                  ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));

    // ── Overview tab ─────────────────────────────────────────────────────────
    auto* overview_w = new QWidget(this);
    auto* overview_layout = new QHBoxLayout(overview_w);
    overview_layout->setContentsMargins(12, 8, 12, 8);
    overview_layout->setSpacing(16);

    auto* chart = new QChart;
    chart->setBackgroundBrush(Qt::transparent);
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->legend()->setVisible(false);
    chart->setAnimationOptions(QChart::NoAnimation);

    sector_chart_ = new QChartView(chart);
    sector_chart_->setRenderHint(QPainter::Antialiasing);
    sector_chart_->setFixedSize(240, 240);
    sector_chart_->setStyleSheet("border:none; background:transparent;");
    overview_layout->addWidget(sector_chart_);

    sector_table_ = new QTableWidget;
    sector_table_->setColumnCount(5);
    sector_table_->setHorizontalHeaderLabels({"SECTOR", "HOLDINGS", "WEIGHT", "P&L", "ALLOCATION"});
    sector_table_->setSelectionMode(QAbstractItemView::NoSelection);
    sector_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sector_table_->setShowGrid(false);
    sector_table_->verticalHeader()->setVisible(false);
    sector_table_->horizontalHeader()->setStretchLastSection(true);
    sector_table_->setStyleSheet(QString("QTableWidget { background:%1; color:%2; border:none; font-size:11px; }"
                                         "QTableWidget::item { padding:4px 8px; border-bottom:1px solid %3; }"
                                         "QHeaderView::section { background:%4; color:%5; border:none;"
                                         "  border-bottom:2px solid %6; padding:4px 8px; font-size:9px;"
                                         "  font-weight:700; letter-spacing:0.5px; }")
                                     .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                                          ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER()));
    overview_layout->addWidget(sector_table_, 1);

    tabs_->addTab(overview_w, "OVERVIEW");

    // ── Analytics tab ────────────────────────────────────────────────────────
    analytics_panel_ = new QWidget(this);
    analytics_panel_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    tabs_->addTab(analytics_panel_, "ANALYTICS");

    // ── Correlation tab ──────────────────────────────────────────────────────
    corr_panel_ = new QWidget(this);
    corr_panel_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    tabs_->addTab(corr_panel_, "CORRELATION");

    layout->addWidget(tabs_);
}

void AnalyticsSectorsView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    update_overview();
    update_analytics();
    update_correlation();
}

QVector<AnalyticsSectorsView::SectorInfo> AnalyticsSectorsView::compute_sectors() const {
    QHash<QString, SectorInfo> map;
    for (const auto& h : summary_.holdings) {
        QString sec = infer_sector(h.symbol);
        auto& info = map[sec];
        info.name = sec;
        info.weight += h.weight;
        info.pnl += h.unrealized_pnl;
        info.count++;
        info.holdings.append(h);
    }

    QVector<SectorInfo> result;
    result.reserve(map.size());
    for (auto it = map.begin(); it != map.end(); ++it)
        result.append(it.value());

    std::sort(result.begin(), result.end(),
              [](const SectorInfo& a, const SectorInfo& b) { return a.weight > b.weight; });
    return result;
}

void AnalyticsSectorsView::update_overview() {
    auto sectors = compute_sectors();

    // Update donut chart
    auto* chart = sector_chart_->chart();
    chart->removeAllSeries();

    auto* series = new QPieSeries;
    series->setHoleSize(0.55);

    for (int i = 0; i < sectors.size(); ++i) {
        auto* slice = series->append(sectors[i].name, sectors[i].weight);
        slice->setColor(sector_color(i));
        slice->setBorderColor(QColor(ui::colors::BG_BASE()));
        slice->setBorderWidth(1);
    }
    chart->addSeries(series);

    // Update table
    sector_table_->setRowCount(sectors.size());
    for (int r = 0; r < sectors.size(); ++r) {
        const auto& s = sectors[r];
        sector_table_->setRowHeight(r, 30);

        auto set_cell = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            sector_table_->setItem(r, col, item);
        };

        set_cell(0, QString("\u25CF %1").arg(s.name));
        // Color the swatch in the name
        auto* name_item = sector_table_->item(r, 0);
        if (name_item)
            name_item->setForeground(sector_color(r));

        set_cell(1, QString::number(s.count), ui::colors::TEXT_SECONDARY);
        set_cell(2, QString("%1%").arg(QString::number(s.weight, 'f', 1)), ui::colors::AMBER);

        const char* pnl_color = s.pnl >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        set_cell(3, QString("%1%2").arg(s.pnl >= 0 ? "+" : "").arg(QString::number(s.pnl, 'f', 2)), pnl_color);

        // Allocation bar (text representation)
        int bar_len = static_cast<int>(s.weight / 5.0);
        set_cell(4, QString(bar_len, QChar(0x2588)) + QString(" %1%").arg(QString::number(s.weight, 'f', 1)),
                 ui::colors::CYAN);
    }
}

void AnalyticsSectorsView::update_analytics() {
    if (analytics_panel_->layout())
        delete analytics_panel_->layout();

    auto* layout = new QVBoxLayout(analytics_panel_);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(8);

    auto* title = new QLabel("SECTOR PERFORMANCE ANALYSIS");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(title);

    auto sectors = compute_sectors();
    for (int i = 0; i < sectors.size(); ++i) {
        const auto& s = sectors[i];
        auto* row = new QHBoxLayout;
        row->setSpacing(8);

        auto* swatch = new QWidget(this);
        swatch->setFixedSize(12, 12);
        swatch->setStyleSheet(QString("background:%1;").arg(sector_color(i).name()));
        row->addWidget(swatch);

        auto* name = new QLabel(s.name);
        name->setFixedWidth(150);
        name->setStyleSheet(QString("color:%1; font-size:11px; font-weight:600;").arg(ui::colors::TEXT_PRIMARY()));
        row->addWidget(name);

        // Weight bar
        auto* bar_container = new QWidget(this);
        bar_container->setFixedHeight(14);
        auto* bar_layout = new QHBoxLayout(bar_container);
        bar_layout->setContentsMargins(0, 0, 0, 0);
        bar_layout->setSpacing(0);

        auto* bar = new QWidget(this);
        int bar_width = static_cast<int>(s.weight * 3.0);
        bar->setFixedSize(std::max(bar_width, 2), 12);
        bar->setStyleSheet(QString("background:%1;").arg(sector_color(i).name()));
        bar_layout->addWidget(bar);
        bar_layout->addStretch();

        row->addWidget(bar_container, 1);

        auto* weight_lbl = new QLabel(QString("%1%").arg(QString::number(s.weight, 'f', 1)));
        weight_lbl->setFixedWidth(50);
        weight_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        weight_lbl->setStyleSheet(
            QString("color:%1; font-size:10px; font-weight:600;").arg(ui::colors::TEXT_SECONDARY()));
        row->addWidget(weight_lbl);

        const char* pnl_color = s.pnl >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        auto* pnl_lbl = new QLabel(QString("%1%2").arg(s.pnl >= 0 ? "+" : "").arg(QString::number(s.pnl, 'f', 2)));
        pnl_lbl->setFixedWidth(80);
        pnl_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        pnl_lbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:600;").arg(pnl_color));
        row->addWidget(pnl_lbl);

        layout->addLayout(row);
    }

    layout->addStretch();
}

void AnalyticsSectorsView::update_correlation() {
    if (corr_panel_->layout())
        delete corr_panel_->layout();

    auto* layout = new QVBoxLayout(corr_panel_);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(8);

    auto* title = new QLabel("FULL CORRELATION MATRIX");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(title);

    auto* note = new QLabel("Based on day-change proxy (requires historical data for precise calculation)");
    note->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(note);

    if (summary_.holdings.size() < 2) {
        auto* msg = new QLabel("Need 2+ holdings for correlation analysis");
        msg->setAlignment(Qt::AlignCenter);
        msg->setStyleSheet(QString("color:%1; font-size:12px; padding:40px;").arg(ui::colors::TEXT_TERTIARY()));
        layout->addWidget(msg);
        return;
    }

    // Sort by weight, take top 10
    auto sorted = summary_.holdings;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.weight > b.weight; });
    int n = static_cast<int>(std::min(qsizetype{10}, sorted.size()));

    auto* grid = new QGridLayout;
    grid->setSpacing(2);

    // Header
    auto* empty = new QLabel;
    grid->addWidget(empty, 0, 0);
    for (int i = 0; i < n; ++i) {
        auto* lbl = new QLabel(sorted[i].symbol.left(5));
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setFixedHeight(22);
        lbl->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700;").arg(ui::colors::TEXT_SECONDARY()));
        grid->addWidget(lbl, 0, i + 1);
    }

    for (int r = 0; r < n; ++r) {
        auto* row_lbl = new QLabel(sorted[r].symbol.left(5));
        row_lbl->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700;").arg(ui::colors::TEXT_SECONDARY()));
        grid->addWidget(row_lbl, r + 1, 0);

        for (int c = 0; c < n; ++c) {
            double corr;
            if (r == c) {
                corr = 1.0;
            } else {
                double a = sorted[r].day_change_percent;
                double b = sorted[c].day_change_percent;
                bool same = (a >= 0) == (b >= 0);
                double mag = std::min(std::abs(a) + std::abs(b), 10.0) / 10.0;
                corr = same ? (mag * 0.8 + 0.1) : -(mag * 0.6 + 0.05);
            }

            auto* cell = new QLabel(QString::number(corr, 'f', 2));
            cell->setAlignment(Qt::AlignCenter);
            cell->setFixedSize(42, 22);

            QColor bg;
            if (r == c)
                bg = QColor(ui::colors::TEXT_PRIMARY());
            else if (corr > 0)
                bg = QColor(22, 163, 74, static_cast<int>(std::abs(corr) * 140));
            else
                bg = QColor(220, 38, 38, static_cast<int>(std::abs(corr) * 140));

            const char* tc = (r == c) ? ui::colors::BG_BASE : ui::colors::TEXT_PRIMARY;
            cell->setStyleSheet(QString("background:rgba(%1,%2,%3,%4); color:%5;"
                                        "font-size:8px; font-weight:600;")
                                    .arg(bg.red())
                                    .arg(bg.green())
                                    .arg(bg.blue())
                                    .arg(bg.alpha())
                                    .arg(tc));

            grid->addWidget(cell, r + 1, c + 1);
        }
    }

    layout->addLayout(grid);
    layout->addStretch();
}

} // namespace fincept::screens
