// src/screens/portfolio/views/RiskManagementView.cpp
#include "screens/portfolio/views/RiskManagementView.h"

#include "screens/portfolio/PortfolioStress.h"
#include "ui/theme/Theme.h"

#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTabBar>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numeric>
#include <optional>

namespace fincept::screens {

namespace {

/// Recursively empties a layout, destroying the widgets it owns.
///
/// `delete panel->layout()` alone does NOT destroy child widgets — they stay
/// parented to the panel, keep their last geometry and keep painting, so every
/// rebuild stacked another full set of metric cards on top of the previous one
/// (a visible artefact and an unbounded leak). Always drain the layout first.
void clear_layout(QLayout* layout) {
    if (!layout)
        return;
    QLayoutItem* item = nullptr;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (auto* w = item->widget()) {
            w->hide();
            w->deleteLater();
        } else if (auto* child = item->layout()) {
            clear_layout(child);
        }
        // `item` IS the nested layout when item->layout() is non-null, so a
        // single delete covers both cases — never delete both.
        delete item;
    }
}

} // namespace

RiskManagementView::RiskManagementView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void RiskManagementView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tabs_ = new QTabWidget;
    tabs_->tabBar()->setElideMode(Qt::ElideNone);
    tabs_->tabBar()->setExpanding(false);
    tabs_->tabBar()->setUsesScrollButtons(false);
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
    overview_tab_index_ = tabs_->addTab(overview_panel_, tr("RISK OVERVIEW"));

    // ── Stress Test tab ──────────────────────────────────────────────────────
    auto* stress_w = new QWidget(this);
    auto* stress_layout = new QVBoxLayout(stress_w);
    stress_layout->setContentsMargins(12, 8, 12, 8);

    stress_title_ = new QLabel(tr("PORTFOLIO STRESS TESTING"));
    stress_title_->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    stress_layout->addWidget(stress_title_);

    stress_note_ = new QLabel(tr("Estimated impact of historical and hypothetical market scenarios"));
    stress_note_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY()));
    stress_layout->addWidget(stress_note_);

    stress_table_ = new QTableWidget;
    stress_table_->setColumnCount(5);
    stress_table_->setHorizontalHeaderLabels(
        {tr("SCENARIO"), tr("DESCRIPTION"), tr("EQUITY SHOCK"), tr("PORTFOLIO IMPACT"), tr("P&L IMPACT")});
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
    stress_tab_index_ = tabs_->addTab(stress_w, tr("STRESS TEST"));

    // ── Risk Contribution tab ────────────────────────────────────────────────
    auto* contrib_w = new QWidget(this);
    auto* contrib_layout = new QVBoxLayout(contrib_w);
    contrib_layout->setContentsMargins(12, 8, 12, 8);

    contrib_title_ = new QLabel(tr("RISK CONTRIBUTION BY HOLDING"));
    contrib_title_->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    contrib_layout->addWidget(contrib_title_);

    contrib_table_ = new QTableWidget;
    contrib_table_->setColumnCount(7);
    contrib_table_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("WEIGHT"), tr("ANN. VOL"), tr("BETA"),
                                               tr("RISK CONTRIB"), tr("≈ VaR CONTRIB"), tr("CONCENTRATION")});
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
                                           ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(),
                                           ui::colors::AMBER()));
    contrib_note_ = new QLabel;
    contrib_note_->setWordWrap(true);
    contrib_note_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY()));
    contrib_layout->addWidget(contrib_note_);
    contrib_layout->addWidget(contrib_table_, 1);
    contrib_tab_index_ = tabs_->addTab(contrib_w, tr("RISK CONTRIBUTION"));

    layout->addWidget(tabs_);
}

void RiskManagementView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    has_data_ = true;
    update_overview();
    update_stress_test();
    update_contribution();
}

void RiskManagementView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void RiskManagementView::retranslateUi() {
    if (tabs_) {
        if (overview_tab_index_ >= 0)
            tabs_->setTabText(overview_tab_index_, tr("RISK OVERVIEW"));
        if (stress_tab_index_ >= 0)
            tabs_->setTabText(stress_tab_index_, tr("STRESS TEST"));
        if (contrib_tab_index_ >= 0)
            tabs_->setTabText(contrib_tab_index_, tr("RISK CONTRIBUTION"));
    }
    if (stress_title_)
        stress_title_->setText(tr("PORTFOLIO STRESS TESTING"));
    if (stress_note_)
        stress_note_->setText(tr("Estimated impact of historical and hypothetical market scenarios"));
    if (contrib_title_)
        contrib_title_->setText(tr("RISK CONTRIBUTION BY HOLDING"));

    if (stress_table_)
        stress_table_->setHorizontalHeaderLabels(
            {tr("SCENARIO"), tr("DESCRIPTION"), tr("EQUITY SHOCK"), tr("PORTFOLIO IMPACT"), tr("P&L IMPACT")});
    if (contrib_table_)
        contrib_table_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("WEIGHT"), tr("ANN. VOL"), tr("BETA"),
                                                   tr("RISK CONTRIB"), tr("≈ VaR CONTRIB"), tr("CONCENTRATION")});

    // re-render dynamic content so tr() row labels (scenario names, ratings) pick up new locale
    if (has_data_) {
        update_overview();
        update_stress_test();
        update_contribution();
    }
}

void RiskManagementView::set_metrics(const portfolio::ComputedMetrics& metrics) {
    metrics_ = metrics;
    // The overview cards (volatility, VaR, CVaR) read metrics_ too. Metrics land
    // asynchronously after the summary, so without this the cards kept showing the
    // day-change proxy until the next set_data().
    if (has_data_)
        update_overview();
    update_stress_test(); // rescale with real beta
}

void RiskManagementView::set_analytics(const portfolio::MarketAnalytics& analytics) {
    analytics_ = analytics;
    if (has_data_) {
        update_overview();    // parametric VaR card
        update_stress_test(); // per-holding betas
        update_contribution();
    }
}

void RiskManagementView::update_overview() {
    if (auto* old = overview_panel_->layout()) {
        clear_layout(old);
        delete old;
    }

    auto* layout = new QVBoxLayout(overview_panel_);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    auto* title = new QLabel(tr("PORTFOLIO RISK OVERVIEW"));
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(title);

    // Compute basic risk metrics
    double total_mv = summary_.total_market_value;

    // Volatility / VaR / CVaR come only from PortfolioService (ex-ante at
    // current weights, or the realised TWR series). The old fallback here
    // annualised the average of the day's absolute price moves, a number with
    // no statistical meaning, and presented it as volatility and VaR.
    const std::optional<double> ann_vol = metrics_.volatility;
    const std::optional<double> var95 = metrics_.var_95;
    const std::optional<double> cvar95 = metrics_.cvar_95;
    const QString risk_basis = metrics_.risk_ex_ante
                                   ? tr("Ex-ante: current weights, %1 daily obs.").arg(metrics_.risk_observations)
                                   : tr("Realised: %1 TWR periods").arg(metrics_.perf_observations);
    const QString na = tr("N/A");

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
        lbl->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; letter-spacing:0.5px;")
                               .arg(ui::colors::TEXT_TERTIARY()));
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

    add_card(0, 0, tr("PORTFOLIO VALUE"), QString("%1 %2").arg(currency_, fmt(total_mv)), ui::colors::WARNING,
             tr("Total market value"));
    // Be honest about which of the two sources produced the number — the card
    // previously always claimed "day-change proxy" even when the real 30-day
    // realized volatility from ComputedMetrics was used.
    const bool have_risk = ann_vol.has_value();
    add_card(0, 1, tr("ANNUALIZED VOLATILITY"), have_risk ? QString("%1%").arg(fmt(*ann_vol, 1)) : na,
             have_risk ? ui::colors::AMBER : ui::colors::TEXT_TERTIARY,
             have_risk ? risk_basis : tr("Waiting for price history"));
    add_card(0, 2, tr("VALUE AT RISK (95%)"), var95 ? QString("%1 %2").arg(currency_, fmt(*var95)) : na,
             var95 ? ui::colors::NEGATIVE : ui::colors::TEXT_TERTIARY, tr("1-day historical, loss"));
    add_card(0, 3, tr("CONDITIONAL VaR"), cvar95 ? QString("%1 %2").arg(currency_, fmt(*cvar95)) : na,
             cvar95 ? ui::colors::NEGATIVE : ui::colors::TEXT_TERTIARY, tr("1-day expected shortfall beyond VaR"));

    add_card(1, 0, tr("TOP HOLDING CONC."), QString("%1%").arg(fmt(conc_top1, 1)),
             conc_top1 > 30 ? ui::colors::NEGATIVE : ui::colors::POSITIVE, tr("Largest position"));
    add_card(1, 1, tr("TOP 3 CONCENTRATION"), QString("%1%").arg(fmt(conc_top3, 1)),
             conc_top3 > 60 ? ui::colors::NEGATIVE : ui::colors::WARNING, tr("Sum of top 3"));
    add_card(1, 2, tr("TOP 5 CONCENTRATION"), QString("%1%").arg(fmt(conc_top5, 1)), ui::colors::CYAN,
             tr("Sum of top 5"));
    // Diversification by WEIGHT, not by count: 1/Σw² is how many equal-sized positions
    // the book behaves like (ten holdings with one at 80% ≈ 1.5). The card used to grade
    // the position count alone, so that book read "Well diversified".
    const double eff_n = metrics_.effective_n.value_or(0.0);
    const bool have_eff = metrics_.effective_n.has_value();
    add_card(1, 3, tr("EFFECTIVE POSITIONS"), have_eff ? fmt(eff_n, 1) : na,
             !have_eff    ? ui::colors::TEXT_TERTIARY
             : eff_n >= 8 ? ui::colors::POSITIVE
             : eff_n >= 4 ? ui::colors::WARNING
                          : ui::colors::NEGATIVE,
             tr("1/Σw² of %n holding(s)", "", summary_.total_positions));

    // Market sensitivity, correlation benefit and tail shape.
    const bool have_beta = metrics_.beta.has_value();
    add_card(2, 0, tr("BETA"), have_beta ? fmt(*metrics_.beta) : na,
             !have_beta                       ? ui::colors::TEXT_TERTIARY
             : std::abs(*metrics_.beta) > 1.3 ? ui::colors::NEGATIVE
                                              : ui::colors::CYAN,
             tr("vs %1").arg(metrics_.benchmark_symbol.isEmpty() ? tr("benchmark") : metrics_.benchmark_symbol));
    const bool have_dr = metrics_.diversification_ratio.has_value();
    add_card(2, 1, tr("DIVERSIFICATION RATIO"), have_dr ? fmt(*metrics_.diversification_ratio) : na,
             !have_dr                                  ? ui::colors::TEXT_TERTIARY
             : *metrics_.diversification_ratio >= 1.25 ? ui::colors::POSITIVE
                                                       : ui::colors::WARNING,
             tr("Σ wᵢσᵢ / σₚ — above 1 = correlation cuts risk"));
    add_card(2, 2, tr("10-DAY VaR (95%)"),
             metrics_.var_95_10d ? QString("%1 %2").arg(currency_, fmt(*metrics_.var_95_10d)) : na,
             metrics_.var_95_10d ? ui::colors::NEGATIVE : ui::colors::TEXT_TERTIARY,
             tr("√10 × 1-day — assumes i.i.d. returns"));
    const bool same_book = analytics_.portfolio_id == summary_.portfolio.id;
    const bool have_pvar = same_book && analytics_.param_var_95_1d.has_value() && total_mv > 0;
    const double pvar = have_pvar ? *analytics_.param_var_95_1d * total_mv : 0.0;
    QString pvar_sub = tr("Normal-distribution 1-day VaR");
    if (have_pvar && var95 && pvar > 0 && *var95 > pvar * 1.2)
        pvar_sub = tr("Historical VaR is %1x this — fat tails").arg(fmt(*var95 / pvar, 1));
    add_card(2, 3, tr("PARAMETRIC VaR (95%)"), have_pvar ? QString("%1 %2").arg(currency_, fmt(pvar)) : na,
             have_pvar ? ui::colors::WARNING : ui::colors::TEXT_TERTIARY, pvar_sub);

    layout->addLayout(grid);
    layout->addStretch();
}

void RiskManagementView::update_stress_test() {
    namespace ps = portfolio_stress;
    const double total_mv = summary_.total_market_value;
    // Per-holding betas from the analytics run, only when they describe this book.
    const bool have_betas = !analytics_.holdings.isEmpty() && analytics_.portfolio_id == summary_.portfolio.id;
    const QHash<QString, portfolio::HoldingRisk>* risk = have_betas ? &analytics_.holdings : nullptr;

    const int n = static_cast<int>(std::size(ps::kScenarios));
    stress_table_->setRowCount(n);
    int equity_lines = 0;
    int beta_lines = 0;

    for (int r = 0; r < n; ++r) {
        const auto& s = ps::kScenarios[r];
        stress_table_->setRowHeight(r, 32);

        auto set_cell = [&](int col, const QString& text, const char* color = nullptr,
                            Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(align);
            if (color)
                item->setForeground(QColor(color));
            stress_table_->setItem(r, col, item);
        };

        // Each equity line moves by ITS OWN beta × the market shock (the old code scaled
        // the whole equity slice by the portfolio beta, which already includes the bond
        // and commodity lines, and clamped a hedge's negative beta to zero).
        const ps::Result res = ps::apply(s, summary_.holdings, risk, total_mv);
        const double impact_pct = res.impact_pct;
        equity_lines = res.equity_lines;
        beta_lines = res.beta_lines;

        // Signed currency delta. The column used to hard-code a leading "-"
        // even for scenarios the portfolio profits from (e.g. Inflation Surge
        // on a commodity-heavy book), reporting a gain as a loss.
        const double delta = res.pnl;

        set_cell(0, ps::scenario_name(s), ui::colors::TEXT_PRIMARY);
        set_cell(1, ps::scenario_description(s), ui::colors::TEXT_SECONDARY);
        set_cell(2, QString("%1%").arg(QString::number(s.equity_shock, 'f', 1)),
                 s.equity_shock < 0 ? ui::colors::NEGATIVE : ui::colors::POSITIVE, Qt::AlignRight | Qt::AlignVCenter);
        set_cell(3, QString("%1%2%").arg(impact_pct < 0 ? "" : "+").arg(QString::number(impact_pct, 'f', 1)),
                 impact_pct < 0 ? ui::colors::NEGATIVE : ui::colors::POSITIVE, Qt::AlignRight | Qt::AlignVCenter);
        set_cell(4,
                 QString("%1%2 %3")
                     .arg(delta < 0 ? "-" : "+")
                     .arg(currency_)
                     .arg(QString::number(std::abs(delta), 'f', 0)),
                 delta < 0 ? ui::colors::NEGATIVE : ui::colors::POSITIVE, Qt::AlignRight | Qt::AlignVCenter);
    }

    if (stress_note_) {
        stress_note_->setText(
            beta_lines > 0
                ? tr("Equity lines move by their own beta × the scenario's market shock (%1 of %2 equity holdings have "
                     "an estimated beta; the rest move with the market). Bonds, commodities and crypto take "
                     "asset-class shocks.")
                      .arg(beta_lines)
                      .arg(equity_lines)
                : tr("Asset-class shocks applied to current weights; per-holding betas refine this once the price "
                     "history has loaded."));
    }
}

void RiskManagementView::update_contribution() {
    auto sorted = summary_.holdings;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.weight > b.weight; });

    contrib_table_->setRowCount(sorted.size());
    const bool have = !analytics_.holdings.isEmpty() && analytics_.portfolio_id == summary_.portfolio.id;
    if (contrib_note_) {
        contrib_note_->setText(
            have ? tr("Ex-ante, current weights x trailing daily returns to %1 (%2 obs.). RISK CONTRIB = share of "
                      "portfolio variance (Euler decomposition, sums to 100%); BETA vs %3.")
                       .arg(analytics_.as_of)
                       .arg(analytics_.observations)
                       .arg(analytics_.benchmark)
                 : tr("Per-holding risk appears once the price-history download completes."));
    }

    for (int r = 0; r < sorted.size(); ++r) {
        const auto& h = sorted[r];
        contrib_table_->setRowHeight(r, 28);

        auto set_cell = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            contrib_table_->setItem(r, col, item);
        };

        set_cell(0, h.symbol, ui::colors::CYAN);
        set_cell(1, QString("%1%").arg(QString::number(h.weight, 'f', 1)));

        const auto it = analytics_.holdings.constFind(h.symbol);
        const bool row_have = have && it != analytics_.holdings.constEnd();
        if (row_have && it->ann_vol)
            set_cell(2, QString("%1%").arg(QString::number(*it->ann_vol * 100.0, 'f', 1)),
                     *it->ann_vol > 0.40 ? ui::colors::NEGATIVE : ui::colors::TEXT_PRIMARY);
        else
            set_cell(2, QStringLiteral("--"), ui::colors::TEXT_TERTIARY);
        if (row_have && it->beta)
            set_cell(3, QString::number(*it->beta, 'f', 2));
        else
            set_cell(3, QStringLiteral("--"), ui::colors::TEXT_TERTIARY);
        if (row_have && it->risk_contribution) {
            const double rc = *it->risk_contribution * 100.0;
            // Risk share well above capital share = the line punches above its weight.
            set_cell(4, QString("%1%").arg(QString::number(rc, 'f', 1)),
                     rc > h.weight + 5.0 ? ui::colors::NEGATIVE : ui::colors::TEXT_PRIMARY);
            // Share of the 1-day VaR in currency (Euler: exact for parametric VaR,
            // a close approximation for the historical figure).
            if (metrics_.var_95) {
                set_cell(
                    5,
                    QString("%1 %2").arg(currency_, QString::number(*it->risk_contribution * *metrics_.var_95, 'f', 0)),
                    ui::colors::TEXT_SECONDARY);
            } else {
                set_cell(5, QStringLiteral("--"), ui::colors::TEXT_TERTIARY);
            }
        } else {
            set_cell(4, row_have ? QStringLiteral("--") : tr("no data"), ui::colors::TEXT_TERTIARY);
            set_cell(5, QStringLiteral("--"), ui::colors::TEXT_TERTIARY);
        }

        const char* conc_color = h.weight > 20   ? ui::colors::NEGATIVE
                                 : h.weight > 10 ? ui::colors::WARNING
                                                 : ui::colors::POSITIVE;
        set_cell(6, h.weight > 20 ? tr("HIGH") : h.weight > 10 ? tr("MEDIUM") : tr("LOW"), conc_color);
    }
}

} // namespace fincept::screens
