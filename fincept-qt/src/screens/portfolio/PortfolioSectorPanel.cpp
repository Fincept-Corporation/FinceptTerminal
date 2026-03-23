// src/screens/portfolio/PortfolioSectorPanel.cpp
#include "screens/portfolio/PortfolioSectorPanel.h"
#include "ui/theme/Theme.h"

#include <QChart>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPieSeries>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

// 16 sector colors matching Tauri palette
static const QColor kSectorColors[] = {
    QColor("#0891b2"), // Technology / Cyan
    QColor("#16a34a"), // Healthcare / Green
    QColor("#d97706"), // Financial Services / Orange
    QColor("#ca8a04"), // Energy / Gold
    QColor("#9333ea"), // Consumer Cyclical / Purple
    QColor("#84cc16"), // Real Estate / Lime
    QColor("#0d9488"), // Utilities / Teal
    QColor("#f97316"), // Cryptocurrency / Orange
    QColor("#60a5fa"), // Communication / Light Blue
    QColor("#f43f5e"), // Consumer Defensive / Rose
    QColor("#a78bfa"), // Industrials / Violet
    QColor("#fbbf24"), // Basic Materials / Amber
    QColor("#2dd4bf"), // Bonds / Teal Light
    QColor("#fb923c"), // Commodities / Orange Light
    QColor("#818cf8"), // Other / Indigo
    QColor("#e879f9"), // Unknown / Fuchsia
};

QColor PortfolioSectorPanel::sector_color(int index) {
    return kSectorColors[index % 16];
}

// Simple sector inference from symbol (placeholder — real mapping would use a service)
QString PortfolioSectorPanel::infer_sector(const QString& symbol) {
    static const QHash<QString, QString> known = {
        {"AAPL", "Technology"}, {"MSFT", "Technology"}, {"GOOGL", "Technology"},
        {"AMZN", "Technology"}, {"NVDA", "Technology"}, {"META", "Technology"},
        {"TSLA", "Consumer Cyclical"}, {"JPM", "Financial Services"},
        {"JNJ", "Healthcare"}, {"UNH", "Healthcare"}, {"PFE", "Healthcare"},
        {"XOM", "Energy"}, {"CVX", "Energy"}, {"BRK-B", "Financial Services"},
        {"V", "Financial Services"}, {"MA", "Financial Services"},
        {"WMT", "Consumer Defensive"}, {"PG", "Consumer Defensive"},
        {"DIS", "Communication"}, {"NFLX", "Communication"},
        {"SPY", "US Equity"}, {"QQQ", "US Equity"}, {"BTC-USD", "Cryptocurrency"},
        {"ETH-USD", "Cryptocurrency"},
    };
    auto it = known.find(symbol.toUpper());
    return it != known.end() ? *it : "Other";
}

PortfolioSectorPanel::PortfolioSectorPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PortfolioSectorPanel::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Top: Sector Allocation (60%) ──
    auto* sector_widget = new QWidget;
    auto* sector_layout = new QVBoxLayout(sector_widget);
    sector_layout->setContentsMargins(8, 4, 8, 4);
    sector_layout->setSpacing(4);

    auto* sector_header = new QLabel("SECTOR ALLOCATION");
    sector_header->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:0.5px;")
                                 .arg(ui::colors::TEXT_SECONDARY));
    sector_layout->addWidget(sector_header);

    auto* donut_row = new QHBoxLayout;

    auto* chart = new QChart;
    chart->setBackgroundBrush(Qt::transparent);
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->legend()->setVisible(false);
    chart->setAnimationOptions(QChart::NoAnimation);

    donut_view_ = new QChartView(chart);
    donut_view_->setRenderHint(QPainter::Antialiasing);
    donut_view_->setFixedSize(130, 130);
    donut_view_->setStyleSheet("border:none; background:transparent;");
    donut_row->addWidget(donut_view_);

    // Legend
    legend_widget_ = new QWidget;
    legend_widget_->setStyleSheet("background:transparent;");
    donut_row->addWidget(legend_widget_, 1);

    sector_layout->addLayout(donut_row, 1);
    layout->addWidget(sector_widget, 6); // 60%

    // Separator
    auto* sep = new QWidget;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM));
    layout->addWidget(sep);

    // ── Bottom: Correlation (40%) ──
    auto* corr_container = new QWidget;
    auto* corr_layout = new QVBoxLayout(corr_container);
    corr_layout->setContentsMargins(8, 4, 8, 4);
    corr_layout->setSpacing(4);

    auto* corr_header = new QHBoxLayout;
    auto* corr_title = new QLabel("CORRELATION");
    corr_title->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:0.5px;")
                               .arg(ui::colors::TEXT_SECONDARY));
    corr_header->addWidget(corr_title);

    auto* corr_note = new QLabel("(day change proxy)");
    corr_note->setStyleSheet(QString("color:%1; font-size:8px;").arg(ui::colors::TEXT_TERTIARY));
    corr_header->addWidget(corr_note);
    corr_header->addStretch();
    corr_layout->addLayout(corr_header);

    corr_widget_ = new QWidget;
    corr_widget_->setStyleSheet("background:transparent;");
    corr_layout->addWidget(corr_widget_, 1);

    layout->addWidget(corr_container, 4); // 40%
}

void PortfolioSectorPanel::set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings) {
    holdings_ = holdings;
    update_donut();
    update_correlation();
}

void PortfolioSectorPanel::set_currency(const QString& currency) {
    currency_ = currency;
}

void PortfolioSectorPanel::update_donut() {
    auto* chart = donut_view_->chart();
    chart->removeAllSeries();

    if (holdings_.isEmpty()) return;

    // Group by sector
    QHash<QString, double> sector_weights;
    QHash<QString, double> sector_pnl;
    QHash<QString, int> sector_counts;

    for (const auto& h : holdings_) {
        QString sector = infer_sector(h.symbol);
        sector_weights[sector] += h.weight;
        sector_pnl[sector] += h.unrealized_pnl;
        sector_counts[sector]++;
    }

    auto* series = new QPieSeries;
    series->setHoleSize(0.55);

    // Sort by weight descending
    QVector<QPair<QString, double>> sorted;
    for (auto it = sector_weights.begin(); it != sector_weights.end(); ++it)
        sorted.append({it.key(), it.value()});
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    for (int i = 0; i < sorted.size(); ++i) {
        auto* slice = series->append(sorted[i].first, sorted[i].second);
        slice->setColor(sector_color(i));
        slice->setBorderColor(QColor(ui::colors::BG_BASE));
        slice->setBorderWidth(1);
    }

    chart->addSeries(series);

    // Rebuild legend
    if (legend_widget_->layout())
        delete legend_widget_->layout();

    auto* legend_layout = new QVBoxLayout(legend_widget_);
    legend_layout->setContentsMargins(4, 0, 0, 0);
    legend_layout->setSpacing(2);

    for (int i = 0; i < sorted.size(); ++i) {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* swatch = new QWidget;
        swatch->setFixedSize(8, 8);
        swatch->setStyleSheet(QString("background:%1;").arg(sector_color(i).name()));
        row->addWidget(swatch);

        auto* name = new QLabel(QString("%1 (%2)")
            .arg(sorted[i].first).arg(sector_counts[sorted[i].first]));
        name->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_PRIMARY));
        row->addWidget(name);

        row->addStretch();

        auto* weight = new QLabel(QString("%1%").arg(QString::number(sorted[i].second, 'f', 1)));
        weight->setStyleSheet(QString("color:%1; font-size:9px; font-weight:600;")
                              .arg(ui::colors::TEXT_SECONDARY));
        row->addWidget(weight);

        legend_layout->addLayout(row);
    }
    legend_layout->addStretch();
}

void PortfolioSectorPanel::update_correlation() {
    if (corr_widget_->layout())
        delete corr_widget_->layout();

    if (holdings_.size() < 2) {
        auto* layout = new QVBoxLayout(corr_widget_);
        auto* msg = new QLabel("Need 2+ holdings for correlation");
        msg->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY));
        msg->setAlignment(Qt::AlignCenter);
        layout->addWidget(msg);
        return;
    }

    // Take top 6 by weight
    auto sorted = holdings_;
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.weight > b.weight; });
    int n = static_cast<int>(std::min(qsizetype{6}, sorted.size()));

    auto* grid = new QGridLayout(corr_widget_);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(1);

    // Header row
    auto* empty = new QLabel;
    grid->addWidget(empty, 0, 0);
    for (int i = 0; i < n; ++i) {
        auto* lbl = new QLabel(sorted[i].symbol.left(4));
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(QString("color:%1; font-size:7px; font-weight:700;")
                           .arg(ui::colors::TEXT_SECONDARY));
        grid->addWidget(lbl, 0, i + 1);
    }

    // Matrix cells
    for (int r = 0; r < n; ++r) {
        auto* row_label = new QLabel(sorted[r].symbol.left(4));
        row_label->setStyleSheet(QString("color:%1; font-size:7px; font-weight:700;")
                                 .arg(ui::colors::TEXT_SECONDARY));
        grid->addWidget(row_label, r + 1, 0);

        for (int c = 0; c < n; ++c) {
            double corr;
            if (r == c) {
                corr = 1.0;
            } else {
                // Proxy correlation from day changes
                double a = sorted[r].day_change_percent;
                double b = sorted[c].day_change_percent;
                bool same_sign = (a >= 0) == (b >= 0);
                double mag = std::min(std::abs(a) + std::abs(b), 10.0) / 10.0;
                corr = same_sign ? (mag * 0.8 + 0.1) : -(mag * 0.6 + 0.05);
            }

            auto* cell = new QLabel(QString::number(corr, 'f', 2));
            cell->setAlignment(Qt::AlignCenter);
            cell->setFixedSize(32, 18);

            QColor bg;
            if (r == c)
                bg = QColor(ui::colors::TEXT_PRIMARY);
            else if (corr > 0)
                bg = QColor(22, 163, 74, static_cast<int>(std::abs(corr) * 120));
            else
                bg = QColor(220, 38, 38, static_cast<int>(std::abs(corr) * 120));

            const char* text_color = (r == c) ? "#000000" : ui::colors::TEXT_PRIMARY;
            cell->setStyleSheet(QString("background:rgba(%1,%2,%3,%4); color:%5;"
                                        "font-size:7px; font-weight:600;")
                                .arg(bg.red()).arg(bg.green()).arg(bg.blue())
                                .arg(bg.alpha()).arg(text_color));

            grid->addWidget(cell, r + 1, c + 1);
        }
    }
}

} // namespace fincept::screens
