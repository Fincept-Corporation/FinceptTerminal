// src/screens/portfolio/views/RiskManagementView.cpp
#include "screens/portfolio/views/RiskManagementView.h"

#include "ui/theme/Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace fincept::screens {

// Pre-defined stress test scenarios
struct StressScenario {
    const char* name;
    const char* description;
    double equity_shock; // percentage shock to equities
    double bond_shock;
    double commodity_shock;
};

static const StressScenario kScenarios[] = {
    {"2008 Financial Crisis", "Lehman collapse, credit freeze", -38.5, 5.2, -33.0},
    {"2020 COVID Crash", "Pandemic selloff (Feb-Mar 2020)", -33.9, 1.2, -20.0},
    {"Dot-com Bust", "Tech bubble burst (2000-2002)", -49.1, 8.5, -10.0},
    {"Interest Rate Shock +3%", "Aggressive Fed tightening", -15.0, -12.0, -5.0},
    {"Black Monday 1987", "Single-day market crash", -22.6, 2.0, -8.0},
    {"Inflation Surge", "1970s-style stagflation", -10.0, -15.0, 25.0},
    {"Geopolitical Crisis", "Major conflict escalation", -12.0, 3.0, 15.0},
    {"Currency Crisis", "Emerging market contagion", -18.0, 1.0, 5.0},
};

RiskManagementView::RiskManagementView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void RiskManagementView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tabs_ = new QTabWidget;
    tabs_->setDocumentMode(true);
    tabs_->setStyleSheet(QString("QTabWidget::pane { border:0; background:%1; }"
                                 "QTabBar::tab { background:%2; color:%3; padding:8px 18px; border:0;"
                                 "  border-bottom:2px solid transparent; font-size:10px; font-weight:700;"
                                 "  letter-spacing:1px; }"
                                 "QTabBar::tab:selected { color:%4; border-bottom:2px solid %4; }"
                                 "QTabBar::tab:hover { color:%5; }")
                             .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(),
                                  ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));

    // ── Risk Overview tab ────────────────────────────────────────────────────
    overview_panel_ = new QWidget(this);
    overview_panel_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    tabs_->addTab(overview_panel_, "RISK OVERVIEW");

    // ── Stress Test tab ──────────────────────────────────────────────────────
    auto* stress_w = new QWidget(this);
    auto* stress_layout = new QVBoxLayout(stress_w);
    stress_layout->setContentsMargins(12, 8, 12, 8);

    auto* stress_title = new QLabel("PORTFOLIO STRESS TESTING");
    stress_title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    stress_layout->addWidget(stress_title);

    auto* stress_note = new QLabel("Estimated impact of historical and hypothetical market scenarios");
    stress_note->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY()));
    stress_layout->addWidget(stress_note);

    stress_table_ = new QTableWidget;
    stress_table_->setColumnCount(5);
    stress_table_->setHorizontalHeaderLabels({"SCENARIO", "DESCRIPTION", "EQUITY SHOCK", "PORTFOLIO IMPACT", "LOSS"});
    stress_table_->setSelectionMode(QAbstractItemView::NoSelection);
    stress_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    stress_table_->setShowGrid(false);
    stress_table_->verticalHeader()->setVisible(false);
    stress_table_->horizontalHeader()->setStretchLastSection(true);
    stress_table_->setColumnWidth(0, 180);
    stress_table_->setColumnWidth(1, 240);
    stress_table_->setColumnWidth(2, 100);
    stress_table_->setColumnWidth(3, 120);
    stress_table_->setStyleSheet(QString("QTableWidget { background:%1; color:%2; border:none; font-size:11px; }"
                                         "QTableWidget::item { padding:4px 8px; border-bottom:1px solid %3; }"
                                         "QHeaderView::section { background:%4; color:%5; border:none;"
                                         "  border-bottom:2px solid %6; padding:4px 8px; font-size:9px;"
                                         "  font-weight:700; letter-spacing:0.5px; }")
                                     .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                                          ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER()));
    stress_layout->addWidget(stress_table_, 1);
    tabs_->addTab(stress_w, "STRESS TEST");

    // ── Risk Contribution tab ────────────────────────────────────────────────
    auto* contrib_w = new QWidget(this);
    auto* contrib_layout = new QVBoxLayout(contrib_w);
    contrib_layout->setContentsMargins(12, 8, 12, 8);

    auto* contrib_title = new QLabel("RISK CONTRIBUTION BY HOLDING");
    contrib_title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    contrib_layout->addWidget(contrib_title);

    contrib_table_ = new QTableWidget;
    contrib_table_->setColumnCount(6);
    contrib_table_->setHorizontalHeaderLabels(
        {"SYMBOL", "WEIGHT", "VOL PROXY", "RISK CONTRIB", "VAR CONTRIB", "CONCENTRATION"});
    contrib_table_->setSelectionMode(QAbstractItemView::NoSelection);
    contrib_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    contrib_table_->setShowGrid(false);
    contrib_table_->verticalHeader()->setVisible(false);
    contrib_table_->horizontalHeader()->setStretchLastSection(true);
    contrib_table_->setStyleSheet(QString("QTableWidget { background:%1; color:%2; border:none; font-size:11px; }"
                                          "QTableWidget::item { padding:4px 8px; border-bottom:1px solid %3; }"
                                          "QHeaderView::section { background:%4; color:%5; border:none;"
                                          "  border-bottom:2px solid %6; padding:4px 8px; font-size:9px;"
                                          "  font-weight:700; letter-spacing:0.5px; }")
                                      .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                                           ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER()));
    contrib_layout->addWidget(contrib_table_, 1);
    tabs_->addTab(contrib_w, "RISK CONTRIBUTION");

    layout->addWidget(tabs_);
}

void RiskManagementView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    update_overview();
    update_stress_test();
    update_contribution();
}

void RiskManagementView::set_metrics(const portfolio::ComputedMetrics& metrics) {
    metrics_ = metrics;
    update_stress_test(); // rescale with real beta
}

void RiskManagementView::update_overview() {
    if (overview_panel_->layout())
        delete overview_panel_->layout();

    auto* layout = new QVBoxLayout(overview_panel_);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    auto* title = new QLabel("PORTFOLIO RISK OVERVIEW");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(title);

    // Compute basic risk metrics
    double total_mv = summary_.total_market_value;

    // Use 30-day realized volatility from ComputedMetrics when available;
    // fall back to single-day cross-sectional proxy otherwise.
    double ann_vol = 0;
    if (metrics_.volatility.has_value()) {
        ann_vol = *metrics_.volatility; // already annualised %
    } else {
        double avg_vol = 0;
        int vol_count = 0;
        for (const auto& h : summary_.holdings) {
            if (std::abs(h.day_change_percent) > 0.001) {
                avg_vol += std::abs(h.day_change_percent);
                ++vol_count;
            }
        }
        if (vol_count > 0)
            avg_vol /= vol_count;
        ann_vol = avg_vol * std::sqrt(252.0);
    }
    const double avg_vol = ann_vol / std::sqrt(252.0); // daily % for VaR formula

    // Concentration
    auto sorted = summary_.holdings;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.weight > b.weight; });
    double conc_top1 = sorted.isEmpty() ? 0 : sorted[0].weight;
    double conc_top3 = 0;
    for (qsizetype i = 0; i < std::min(qsizetype{3}, sorted.size()); ++i)
        conc_top3 += sorted[i].weight;
    double conc_top5 = 0;
    for (qsizetype i = 0; i < std::min(qsizetype{5}, sorted.size()); ++i)
        conc_top5 += sorted[i].weight;

    // VaR/CVaR — parametric normal: CVaR/VaR = phi(1.645)/0.05 ≈ 1.546
    double var95 = total_mv * avg_vol * 1.645 / 100.0;
    double cvar95 = var95 * 1.546;

    // Metric cards grid
    auto* grid = new QGridLayout;
    grid->setSpacing(8);

    auto add_card = [&](int r, int c, const QString& label, const QString& value, const char* color,
                        const QString& sub = {}) {
        auto* card = new QWidget(this);
        card->setStyleSheet(
            QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(12, 8, 12, 8);
        cl->setSpacing(2);

        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(
            QString("color:%1; font-size:8px; font-weight:700; letter-spacing:0.5px;").arg(ui::colors::TEXT_TERTIARY()));
        cl->addWidget(lbl);

        auto* val = new QLabel(value);
        val->setStyleSheet(QString("color:%1; font-size:16px; font-weight:700;").arg(color));
        cl->addWidget(val);

        if (!sub.isEmpty()) {
            auto* s = new QLabel(sub);
            s->setStyleSheet(QString("color:%1; font-size:8px;").arg(ui::colors::TEXT_TERTIARY()));
            cl->addWidget(s);
        }

        grid->addWidget(card, r, c);
    };

    auto fmt = [](double v, int dp = 2) { return QString::number(v, 'f', dp); };

    add_card(0, 0, "PORTFOLIO VALUE", QString("%1 %2").arg(currency_, fmt(total_mv)), ui::colors::WARNING,
             "Total market value");
    add_card(0, 1, "ANNUALIZED VOLATILITY", QString("%1%").arg(fmt(ann_vol, 1)), ui::colors::AMBER,
             "Based on day-change proxy");
    add_card(0, 2, "VALUE AT RISK (95%)", QString("%1 %2").arg(currency_, fmt(var95)), ui::colors::NEGATIVE,
             "1-day parametric");
    add_card(0, 3, "CONDITIONAL VaR", QString("%1 %2").arg(currency_, fmt(cvar95)), ui::colors::NEGATIVE,
             "Expected shortfall");

    add_card(1, 0, "TOP HOLDING CONC.", QString("%1%").arg(fmt(conc_top1, 1)),
             conc_top1 > 30 ? ui::colors::NEGATIVE : ui::colors::POSITIVE, "Largest position");
    add_card(1, 1, "TOP 3 CONCENTRATION", QString("%1%").arg(fmt(conc_top3, 1)),
             conc_top3 > 60 ? ui::colors::NEGATIVE : ui::colors::WARNING, "Sum of top 3");
    add_card(1, 2, "TOP 5 CONCENTRATION", QString("%1%").arg(fmt(conc_top5, 1)), ui::colors::CYAN, "Sum of top 5");
    add_card(1, 3, "DIVERSIFICATION", QString("%1 holdings").arg(summary_.total_positions),
             summary_.total_positions >= 10 ? ui::colors::POSITIVE : ui::colors::WARNING,
             summary_.total_positions >= 10 ? "Well diversified" : "Consider adding more");

    layout->addLayout(grid);
    layout->addStretch();
}

void RiskManagementView::update_stress_test() {
    double total_mv = summary_.total_market_value;

    // ── Compute actual sector weights from holdings ───────────────────────────
    // Classify each holding into equity / bond / commodity / crypto / other
    // based on symbol patterns — mirrors PortfolioSectorPanel::infer_sector
    double equity_wt = 0, bond_wt = 0, commodity_wt = 0, crypto_wt = 0;
    for (const auto& h : summary_.holdings) {
        const QString s = h.symbol.toUpper();
        bool is_crypto = s.endsWith("-USD") || s.endsWith("-USDT");
        bool is_bond = s == "TLT" || s == "AGG" || s == "BND" || s == "IEF" || s == "SHY" || s == "LQD" || s == "HYG" ||
                       s == "EMB" || s.contains("BOND") || s.contains("TREAS");
        bool is_comm = s == "GLD" || s == "IAU" || s == "SLV" || s == "USO" || s.contains("OIL") || s.contains("GOLD");

        if (is_crypto)
            crypto_wt += h.weight;
        else if (is_bond)
            bond_wt += h.weight;
        else if (is_comm)
            commodity_wt += h.weight;
        else
            equity_wt += h.weight;
    }
    // Normalise to fractions
    equity_wt /= 100.0;
    bond_wt /= 100.0;
    commodity_wt /= 100.0;
    crypto_wt /= 100.0;

    int n = sizeof(kScenarios) / sizeof(kScenarios[0]);
    stress_table_->setRowCount(n);

    for (int r = 0; r < n; ++r) {
        const auto& s = kScenarios[r];
        stress_table_->setRowHeight(r, 32);

        auto set_cell = [&](int col, const QString& text, const char* color = nullptr,
                            Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(align);
            if (color)
                item->setForeground(QColor(color));
            stress_table_->setItem(r, col, item);
        };

        // Weighted impact using actual portfolio composition
        double impact_pct = s.equity_shock * equity_wt + s.bond_shock * bond_wt + s.commodity_shock * commodity_wt +
                            s.equity_shock * 1.5 * crypto_wt; // crypto amplified

        // Scale by portfolio beta vs SPY (if available from OLS).
        // Beta > 1 means portfolio amplifies market moves; beta < 1 dampens.
        // SPY equity_shock is the base market scenario, so multiply equity
        // component by beta. Bond/commodity/crypto retain their own sensitivities.
        if (metrics_.beta.has_value()) {
            const double beta = std::max(0.0, *metrics_.beta); // clamp negative beta to 0
            const double equity_impact = s.equity_shock * equity_wt * beta;
            const double other_impact =
                s.bond_shock * bond_wt + s.commodity_shock * commodity_wt + s.equity_shock * 1.5 * crypto_wt;
            impact_pct = equity_impact + other_impact;
        }

        double loss = total_mv * std::abs(impact_pct) / 100.0;

        set_cell(0, s.name, ui::colors::TEXT_PRIMARY);
        set_cell(1, s.description, ui::colors::TEXT_SECONDARY);
        set_cell(2, QString("%1%").arg(QString::number(s.equity_shock, 'f', 1)),
                 s.equity_shock < 0 ? ui::colors::NEGATIVE : ui::colors::POSITIVE, Qt::AlignRight | Qt::AlignVCenter);
        set_cell(3, QString("%1%2%").arg(impact_pct < 0 ? "" : "+").arg(QString::number(impact_pct, 'f', 1)),
                 impact_pct < 0 ? ui::colors::NEGATIVE : ui::colors::POSITIVE, Qt::AlignRight | Qt::AlignVCenter);
        set_cell(4, QString("-%1 %2").arg(currency_, QString::number(loss, 'f', 0)), ui::colors::NEGATIVE,
                 Qt::AlignRight | Qt::AlignVCenter);
    }
}

void RiskManagementView::update_contribution() {
    auto sorted = summary_.holdings;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.weight > b.weight; });

    contrib_table_->setRowCount(sorted.size());

    // Total portfolio vol proxy
    double total_vol = 0;
    for (const auto& h : sorted)
        total_vol += std::abs(h.day_change_percent) * h.weight / 100.0;

    for (int r = 0; r < sorted.size(); ++r) {
        const auto& h = sorted[r];
        contrib_table_->setRowHeight(r, 28);

        auto set_cell = [&](int col, const QString& text, const char* color = nullptr,
                            Qt::Alignment align = Qt::AlignRight | Qt::AlignVCenter) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : align);
            if (color)
                item->setForeground(QColor(color));
            contrib_table_->setItem(r, col, item);
        };

        double vol_proxy = std::abs(h.day_change_percent);
        double risk_contrib = vol_proxy * h.weight / 100.0;
        double risk_pct = total_vol > 0 ? (risk_contrib / total_vol) * 100.0 : 0;
        double var_contrib = summary_.total_market_value * vol_proxy * 1.645 / 100.0 * h.weight / 100.0;

        set_cell(0, h.symbol, ui::colors::CYAN);
        set_cell(1, QString("%1%").arg(QString::number(h.weight, 'f', 1)));
        set_cell(2, QString("%1%").arg(QString::number(vol_proxy, 'f', 2)),
                 vol_proxy > 3 ? ui::colors::NEGATIVE : ui::colors::TEXT_PRIMARY);
        set_cell(3, QString("%1%").arg(QString::number(risk_pct, 'f', 1)),
                 risk_pct > 20 ? ui::colors::NEGATIVE : ui::colors::TEXT_PRIMARY);
        set_cell(4, QString("%1 %2").arg(currency_, QString::number(var_contrib, 'f', 2)));

        // Concentration rating
        const char* conc_color = h.weight > 20   ? ui::colors::NEGATIVE
                                 : h.weight > 10 ? ui::colors::WARNING
                                                 : ui::colors::POSITIVE;
        QString conc_text = h.weight > 20 ? "HIGH" : h.weight > 10 ? "MEDIUM" : "LOW";
        set_cell(5, conc_text, conc_color);
    }
}

} // namespace fincept::screens
