// src/screens/portfolio/PortfolioSectorPanel.cpp
#include "screens/portfolio/PortfolioSectorPanel.h"

#include "ui/theme/Theme.h"

#include <QChart>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPieSeries>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>

namespace fincept::screens {

// 16 sector colors
static const QColor kSectorColors[] = {
    QColor("#0891b2"), // Technology
    QColor("#16a34a"), // Healthcare
    QColor("#d97706"), // Financial Services
    QColor("#ca8a04"), // Energy
    QColor("#9333ea"), // Consumer Cyclical
    QColor("#84cc16"), // Real Estate
    QColor("#0d9488"), // Utilities
    QColor("#f97316"), // Cryptocurrency
    QColor("#60a5fa"), // Communication
    QColor("#f43f5e"), // Consumer Defensive
    QColor("#a78bfa"), // Industrials
    QColor("#fbbf24"), // Basic Materials
    QColor("#2dd4bf"), // Bonds / Fixed Income
    QColor("#fb923c"), // Commodities
    QColor("#818cf8"), // ETF / Index
    QColor("#e879f9"), // Unknown
};

QColor PortfolioSectorPanel::sector_color(int index) {
    return kSectorColors[index % 16];
}

PortfolioSectorPanel::PortfolioSectorPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PortfolioSectorPanel::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Top: Sector Allocation (60%) ──────────────────────────────────────────
    auto* sector_widget = new QWidget(this);
    auto* sector_layout = new QVBoxLayout(sector_widget);
    sector_layout->setContentsMargins(8, 4, 8, 4);
    sector_layout->setSpacing(4);

    auto* sector_header = new QLabel("SECTOR ALLOCATION");
    sector_header->setStyleSheet(
        QString("color:%1; font-size:9px; font-weight:700; letter-spacing:0.5px;").arg(ui::colors::TEXT_SECONDARY()));
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

    legend_widget_ = new QWidget(this);
    legend_widget_->setStyleSheet("background:transparent;");
    donut_row->addWidget(legend_widget_, 1);

    sector_layout->addLayout(donut_row, 1);
    layout->addWidget(sector_widget, 6);

    // Separator
    auto* sep = new QWidget(this);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    layout->addWidget(sep);

    // ── Bottom: Correlation (40%) ─────────────────────────────────────────────
    auto* corr_container = new QWidget(this);
    auto* corr_layout = new QVBoxLayout(corr_container);
    corr_layout->setContentsMargins(8, 4, 8, 4);
    corr_layout->setSpacing(4);

    auto* corr_header = new QHBoxLayout;
    auto* corr_title = new QLabel("CORRELATION MATRIX");
    corr_title->setStyleSheet(
        QString("color:%1; font-size:9px; font-weight:700; letter-spacing:0.5px;").arg(ui::colors::TEXT_SECONDARY()));
    corr_header->addWidget(corr_title);

    auto* corr_note = new QLabel("(P&L return proxy)");
    corr_note->setStyleSheet(QString("color:%1; font-size:8px;").arg(ui::colors::TEXT_TERTIARY()));
    corr_header->addWidget(corr_note);
    corr_header->addStretch();
    corr_layout->addLayout(corr_header);

    corr_widget_ = new QWidget(this);
    corr_widget_->setStyleSheet("background:transparent;");
    corr_layout->addWidget(corr_widget_, 1);

    layout->addWidget(corr_container, 4);
}

void PortfolioSectorPanel::set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings) {
    holdings_ = holdings;
    selected_sector_.clear(); // reset filter on fresh data
    corr_matrix_.clear();     // invalidate stale correlation data
    update_donut();
    update_correlation();
}

void PortfolioSectorPanel::set_correlation(const QHash<QString, double>& matrix) {
    corr_matrix_ = matrix;
    update_correlation();
}

void PortfolioSectorPanel::on_sector_legend_clicked(const QString& sector) {
    if (selected_sector_ == sector)
        selected_sector_.clear(); // toggle off
    else
        selected_sector_ = sector;
    emit sector_selected(selected_sector_);
    update_donut(); // refresh to show highlight
}

void PortfolioSectorPanel::set_currency(const QString& currency) {
    currency_ = currency;
}

void PortfolioSectorPanel::update_donut() {
    auto* chart = donut_view_->chart();
    chart->removeAllSeries();

    if (holdings_.isEmpty())
        return;

    // Group by inferred sector using actual portfolio weights
    QHash<QString, double> sector_weights;
    QHash<QString, double> sector_pnl;
    QHash<QString, int> sector_counts;

    for (const auto& h : holdings_) {
        QString sector = h.sector.isEmpty() ? QStringLiteral("Unclassified") : h.sector;
        sector_weights[sector] += h.weight;
        sector_pnl[sector] += h.unrealized_pnl;
        sector_counts[sector]++;
    }

    auto* series = new QPieSeries;
    series->setHoleSize(0.55);

    QVector<QPair<QString, double>> sorted;
    for (auto it = sector_weights.begin(); it != sector_weights.end(); ++it)
        sorted.append({it.key(), it.value()});
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    for (int i = 0; i < sorted.size(); ++i) {
        const QString& sec = sorted[i].first;
        auto* slice = series->append(sec, sorted[i].second);
        slice->setColor(sector_color(i));
        const bool active = (sec == selected_sector_);
        slice->setBorderColor(active ? QColor(ui::colors::AMBER()) : QColor(ui::colors::BG_BASE()));
        slice->setBorderWidth(active ? 2 : 1);
        if (active)
            slice->setExploded(true);
    }

    // Wire slice clicks
    connect(series, &QPieSeries::clicked, this, [this](QPieSlice* slice) { on_sector_legend_clicked(slice->label()); });

    chart->addSeries(series);

    // Rebuild legend
    if (auto* old = legend_widget_->layout()) {
        QLayoutItem* item;
        while ((item = old->takeAt(0)) != nullptr) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
        delete old;
    }

    auto* legend_layout = new QVBoxLayout(legend_widget_);
    legend_layout->setContentsMargins(4, 0, 0, 0);
    legend_layout->setSpacing(2);

    for (int i = 0; i < sorted.size(); ++i) {
        const QString& sec = sorted[i].first;
        const bool active = (sec == selected_sector_);

        // Wrap the row in a clickable QWidget for hit-testing
        auto* row_widget = new QWidget(this);
        row_widget->setCursor(Qt::PointingHandCursor);
        row_widget->setStyleSheet(active ? QString("background:%1; border-radius:2px;").arg(ui::colors::BG_HOVER())
                                         : "background:transparent;");

        auto* row = new QHBoxLayout(row_widget);
        row->setContentsMargins(2, 1, 2, 1);
        row->setSpacing(4);

        auto* swatch = new QWidget(this);
        swatch->setFixedSize(8, 8);
        swatch->setStyleSheet(QString("background:%1; border-radius:1px;").arg(sector_color(i).name()));
        row->addWidget(swatch);

        auto* name = new QLabel(QString("%1 (%2)").arg(sec).arg(sector_counts[sec]));
        name->setStyleSheet(active ? QString("color:%1; font-size:9px; font-weight:700;").arg(ui::colors::AMBER())
                                   : QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_PRIMARY()));
        row->addWidget(name);
        row->addStretch();

        double pnl = sector_pnl[sec];
        auto* pnl_lbl = new QLabel(QString("%1%2").arg(pnl >= 0 ? "+" : "").arg(QString::number(pnl, 'f', 0)));
        const char* pc = pnl >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        pnl_lbl->setStyleSheet(QString("color:%1; font-size:8px;").arg(pc));
        row->addWidget(pnl_lbl);

        auto* weight = new QLabel(QString(" %1%").arg(QString::number(sorted[i].second, 'f', 1)));
        weight->setStyleSheet(QString("color:%1; font-size:9px; font-weight:600;").arg(ui::colors::TEXT_SECONDARY()));
        row->addWidget(weight);

        // Install click via event filter on row_widget
        row_widget->installEventFilter(this);
        row_widget->setProperty("sector_name", sec);

        legend_layout->addWidget(row_widget);
    }
    legend_layout->addStretch();
}

void PortfolioSectorPanel::update_correlation() {
    if (auto* old = corr_widget_->layout()) {
        QLayoutItem* item;
        while ((item = old->takeAt(0)) != nullptr) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
        delete old;
    }

    if (holdings_.size() < 2) {
        auto* layout = new QVBoxLayout(corr_widget_);
        auto* msg = new QLabel("Need 2+ holdings for correlation");
        msg->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY()));
        msg->setAlignment(Qt::AlignCenter);
        layout->addWidget(msg);
        return;
    }

    // Top 6 by weight
    auto sorted = holdings_;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.weight > b.weight; });
    int n = static_cast<int>(std::min(qsizetype{6}, sorted.size()));

    // Use real Pearson matrix if available; otherwise show "--" pending fetch.
    const bool has_real_corr = !corr_matrix_.isEmpty();

    auto corr_pair = [&](int i, int j) -> std::optional<double> {
        if (i == j)
            return 1.0;
        if (has_real_corr) {
            const QString key_ab = sorted[i].symbol + "|" + sorted[j].symbol;
            const QString key_ba = sorted[j].symbol + "|" + sorted[i].symbol;
            if (corr_matrix_.contains(key_ab))
                return corr_matrix_[key_ab];
            if (corr_matrix_.contains(key_ba))
                return corr_matrix_[key_ba];
            return std::nullopt; // symbol not in matrix (fetch may be partial)
        }
        // No real data yet — return nullopt so cell shows "…"
        return std::nullopt;
    };

    auto* grid = new QGridLayout(corr_widget_);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(1);

    // Header row
    grid->addWidget(new QLabel, 0, 0);
    for (int i = 0; i < n; ++i) {
        auto* lbl = new QLabel(sorted[i].symbol.left(4));
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(QString("color:%1; font-size:7px; font-weight:700;").arg(ui::colors::TEXT_SECONDARY()));
        grid->addWidget(lbl, 0, i + 1);
    }

    // Matrix
    for (int r = 0; r < n; ++r) {
        auto* row_label = new QLabel(sorted[r].symbol.left(4));
        row_label->setStyleSheet(QString("color:%1; font-size:7px; font-weight:700;").arg(ui::colors::TEXT_SECONDARY()));
        grid->addWidget(row_label, r + 1, 0);

        for (int c = 0; c < n; ++c) {
            const auto opt_corr = corr_pair(r, c);
            const bool is_diag = (r == c);
            const double corr = opt_corr.value_or(0.0);
            const QString label = is_diag ? "1.00" : opt_corr ? QString::number(corr, 'f', 2) : "\u2026"; // "…" pending

            auto* cell = new QLabel(label);
            cell->setAlignment(Qt::AlignCenter);
            cell->setFixedSize(32, 18);

            QColor bg;
            if (is_diag) {
                bg = QColor(ui::colors::TEXT_PRIMARY());
            } else if (!opt_corr) {
                bg = QColor(ui::colors::BG_RAISED());
            } else if (corr > 0) {
                bg = QColor(22, 163, 74, static_cast<int>(std::abs(corr) * 140 + 20));
            } else {
                bg = QColor(220, 38, 38, static_cast<int>(std::abs(corr) * 140 + 20));
            }

            const char* text_color = is_diag ? ui::colors::BG_BASE : ui::colors::TEXT_PRIMARY;
            cell->setStyleSheet(QString("background:rgba(%1,%2,%3,%4); color:%5;"
                                        "font-size:7px; font-weight:600;")
                                    .arg(bg.red())
                                    .arg(bg.green())
                                    .arg(bg.blue())
                                    .arg(bg.alpha())
                                    .arg(text_color));

            grid->addWidget(cell, r + 1, c + 1);
        }
    }
}

bool PortfolioSectorPanel::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        const QString sec = obj->property("sector_name").toString();
        if (!sec.isEmpty()) {
            on_sector_legend_clicked(sec);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

} // namespace fincept::screens
