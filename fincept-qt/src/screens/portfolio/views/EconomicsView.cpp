// src/screens/portfolio/views/EconomicsView.cpp
#include "screens/portfolio/views/EconomicsView.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

struct MacroIndicator {
    const char* name;
    const char* current;
    const char* previous;
    const char* trend;
    const char* impact;
    const char* color;
};

static const MacroIndicator kIndicators[] = {
    {"GDP Growth (YoY)", "2.8%", "3.1%", "\u25BC", "Moderate", "#ca8a04"},
    {"CPI Inflation (YoY)", "3.2%", "3.5%", "\u25BC", "Easing", "#16a34a"},
    {"Fed Funds Rate", "5.25%", "5.25%", "\u25B6", "Stable", "#0891b2"},
    {"10Y Treasury Yield", "4.35%", "4.25%", "\u25B2", "Rising", "#dc2626"},
    {"Unemployment Rate", "3.9%", "3.8%", "\u25B2", "Slight Up", "#ca8a04"},
    {"PCE Price Index (YoY)", "2.7%", "2.9%", "\u25BC", "Improving", "#16a34a"},
    {"ISM Manufacturing", "47.2", "46.8", "\u25B2", "Contracting", "#dc2626"},
    {"Consumer Confidence", "102.0", "103.5", "\u25BC", "Softening", "#ca8a04"},
    {"Housing Starts (M)", "1.42", "1.38", "\u25B2", "Recovering", "#16a34a"},
    {"Retail Sales (MoM)", "0.7%", "0.3%", "\u25B2", "Strong", "#16a34a"},
    {"Industrial Production", "-0.2%", "0.1%", "\u25BC", "Weak", "#dc2626"},
    {"Trade Balance ($B)", "-68.3", "-65.0", "\u25BC", "Widening", "#dc2626"},
};

EconomicsView::EconomicsView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void EconomicsView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Top: Macro Economic Indicators ───────────────────────────────────────
    auto* indicators_section = new QWidget;
    auto* ind_layout = new QVBoxLayout(indicators_section);
    ind_layout->setContentsMargins(12, 8, 12, 8);
    ind_layout->setSpacing(4);

    auto* ind_title = new QLabel("MACRO ECONOMIC INDICATORS");
    ind_title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    ind_layout->addWidget(ind_title);

    indicators_table_ = new QTableWidget;
    indicators_table_->setColumnCount(5);
    indicators_table_->setHorizontalHeaderLabels({"INDICATOR", "CURRENT", "PREVIOUS", "TREND", "IMPACT"});
    indicators_table_->setSelectionMode(QAbstractItemView::NoSelection);
    indicators_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    indicators_table_->setShowGrid(false);
    indicators_table_->verticalHeader()->setVisible(false);
    indicators_table_->horizontalHeader()->setStretchLastSection(true);
    indicators_table_->setColumnWidth(0, 200);
    indicators_table_->setColumnWidth(1, 90);
    indicators_table_->setColumnWidth(2, 90);
    indicators_table_->setColumnWidth(3, 60);
    indicators_table_->setStyleSheet(QString("QTableWidget { background:%1; color:%2; border:none; font-size:11px; }"
                                             "QTableWidget::item { padding:3px 8px; border-bottom:1px solid %3; }"
                                             "QHeaderView::section { background:%4; color:%5; border:none;"
                                             "  border-bottom:2px solid %6; padding:3px 8px; font-size:9px;"
                                             "  font-weight:700; letter-spacing:0.5px; }")
                                         .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM,
                                              ui::colors::BG_SURFACE, ui::colors::TEXT_SECONDARY, ui::colors::AMBER));
    ind_layout->addWidget(indicators_table_, 1);
    layout->addWidget(indicators_section, 6);

    // Separator
    auto* sep = new QWidget;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM));
    layout->addWidget(sep);

    // ── Bottom: Portfolio Sensitivity ────────────────────────────────────────
    auto* sens_section = new QWidget;
    auto* sens_layout = new QVBoxLayout(sens_section);
    sens_layout->setContentsMargins(12, 8, 12, 8);
    sens_layout->setSpacing(4);

    auto* sens_title = new QLabel("PORTFOLIO FACTOR SENSITIVITY");
    sens_title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    sens_layout->addWidget(sens_title);

    auto* sens_note = new QLabel("Estimated portfolio sensitivity to macro factor changes");
    sens_note->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY));
    sens_layout->addWidget(sens_note);

    sensitivity_table_ = new QTableWidget;
    sensitivity_table_->setColumnCount(4);
    sensitivity_table_->setHorizontalHeaderLabels({"FACTOR", "SENSITIVITY", "DIRECTION", "PORTFOLIO IMPACT"});
    sensitivity_table_->setSelectionMode(QAbstractItemView::NoSelection);
    sensitivity_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sensitivity_table_->setShowGrid(false);
    sensitivity_table_->verticalHeader()->setVisible(false);
    sensitivity_table_->horizontalHeader()->setStretchLastSection(true);
    sensitivity_table_->setStyleSheet(indicators_table_->styleSheet());
    sens_layout->addWidget(sensitivity_table_, 1);
    layout->addWidget(sens_section, 4);
}

void EconomicsView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    update_indicators();
    update_sensitivity();
}

void EconomicsView::update_indicators() {
    int n = sizeof(kIndicators) / sizeof(kIndicators[0]);
    indicators_table_->setRowCount(n);

    for (int r = 0; r < n; ++r) {
        const auto& ind = kIndicators[r];
        indicators_table_->setRowHeight(r, 26);

        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignCenter));
            if (color)
                item->setForeground(QColor(color));
            indicators_table_->setItem(r, col, item);
        };

        set(0, ind.name, ui::colors::TEXT_PRIMARY);
        set(1, ind.current, ui::colors::CYAN);
        set(2, ind.previous, ui::colors::TEXT_SECONDARY);
        set(3, ind.trend, ind.color);
        set(4, ind.impact, ind.color);
    }
}

void EconomicsView::update_sensitivity() {
    struct Factor {
        const char* name;
        double sensitivity;
        const char* direction;
    };

    Factor factors[] = {
        {"Interest Rates (+1%)", -0.12, "Negative"},      {"GDP Growth (+1%)", 0.08, "Positive"},
        {"Inflation (+1%)", -0.05, "Negative"},           {"USD Strength (+1%)", -0.03, "Negative"},
        {"Oil Price (+10%)", -0.02, "Slightly Negative"}, {"Unemployment (+1%)", -0.06, "Negative"},
        {"Consumer Spending (+1%)", 0.04, "Positive"},    {"Credit Spreads (+50bps)", -0.08, "Negative"},
    };

    int n = sizeof(factors) / sizeof(factors[0]);
    sensitivity_table_->setRowCount(n);

    double mv = summary_.total_market_value;

    for (int r = 0; r < n; ++r) {
        const auto& f = factors[r];
        sensitivity_table_->setRowHeight(r, 26);

        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignCenter));
            if (color)
                item->setForeground(QColor(color));
            sensitivity_table_->setItem(r, col, item);
        };

        const char* sens_color = f.sensitivity >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        double impact = mv * f.sensitivity;

        set(0, f.name, ui::colors::TEXT_PRIMARY);
        set(1, QString("%1%2%").arg(f.sensitivity >= 0 ? "+" : "").arg(QString::number(f.sensitivity * 100.0, 'f', 1)),
            sens_color);
        set(2, f.direction, sens_color);
        set(3,
            QString("%1%2 %3")
                .arg(impact >= 0 ? "+" : "")
                .arg(currency_)
                .arg(QString::number(std::abs(impact), 'f', 0)),
            sens_color);
    }
}

} // namespace fincept::screens
