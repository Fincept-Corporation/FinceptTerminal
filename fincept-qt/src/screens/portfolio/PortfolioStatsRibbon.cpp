// src/screens/portfolio/PortfolioStatsRibbon.cpp
#include "screens/portfolio/PortfolioStatsRibbon.h"

#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

PortfolioStatsRibbon::PortfolioStatsRibbon(QWidget* parent) : QWidget(parent) {
    setFixedHeight(72);
    setObjectName("portfolioStatsRibbon");
    setStyleSheet(QString("#portfolioStatsRibbon { background:%1; border-bottom:1px solid %2; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── Primary row: 4 hero KPIs (44px) ──────────────────────────────────────
    primary_row_ = new QWidget(this);
    primary_row_->setFixedHeight(44);
    primary_row_->setObjectName("pfStatsPrimary");
    primary_row_->setStyleSheet("#pfStatsPrimary { background:transparent; }");
    auto* primary_layout = new QHBoxLayout(primary_row_);
    primary_layout->setContentsMargins(0, 0, 0, 0);
    primary_layout->setSpacing(0);

    total_value_ = add_hero(primary_layout, "TOTAL VALUE", ui::colors::WARNING);
    total_value_.container->setToolTip("Current market value of all holdings.\n"
                                       "Sum of (quantity × current price) for every position.");

    pnl_ = add_hero(primary_layout, "UNREALIZED P&L", ui::colors::TEXT_PRIMARY);
    pnl_.container->setToolTip("Unrealized profit or loss across all open positions.\n"
                               "Calculated as: market value − total cost basis.\n"
                               "This gain/loss is not realized until you sell.");

    day_change_ = add_hero(primary_layout, "DAY CHANGE", ui::colors::TEXT_PRIMARY);
    day_change_.container->setToolTip("Total portfolio value change today.\n"
                                      "Weighted sum of each holding's intraday change.");

    positions_ = add_hero(primary_layout, "POSITIONS", ui::colors::TEXT_PRIMARY);
    positions_.container->setToolTip("Number of distinct holdings in this portfolio,\n"
                                     "split into gainers (▲) and losers (▼) today.");

    outer->addWidget(primary_row_);

    // Thin divider between tiers
    auto* divider = new QWidget(this);
    divider->setFixedHeight(1);
    divider->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    outer->addWidget(divider);

    // ── Secondary row: risk chips (27px) ─────────────────────────────────────
    secondary_row_ = new QWidget(this);
    secondary_row_->setFixedHeight(27);
    secondary_row_->setObjectName("pfStatsSecondary");
    secondary_row_->setStyleSheet(QString("#pfStatsSecondary { background:%1; }").arg(ui::colors::BG_BASE()));

    auto* scroll = new QScrollArea(secondary_row_);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background:transparent; border:none; }");

    auto* sec_outer = new QHBoxLayout(secondary_row_);
    sec_outer->setContentsMargins(0, 0, 0, 0);
    sec_outer->setSpacing(0);
    sec_outer->addWidget(scroll);

    auto* chips_host = new QWidget(this);
    chips_host->setStyleSheet("background:transparent;");
    auto* chips_layout = new QHBoxLayout(chips_host);
    chips_layout->setContentsMargins(10, 0, 10, 0);
    chips_layout->setSpacing(6);

    cost_basis_ = add_chip(chips_layout, "COST", ui::colors::CYAN);
    cost_basis_.container->setToolTip("Total amount invested — sum of (avg buy price × quantity)\n"
                                      "for all current positions. Used to compute P&L.");

    concentration_ = add_chip(chips_layout, "CONC", ui::colors::AMBER);
    concentration_.container->setToolTip("Concentration risk: combined weight of the top 3 holdings.\n"
                                         "Values above 50% indicate a concentrated portfolio.\n"
                                         "Lower is generally better for diversification.");

    sharpe_ = add_chip(chips_layout, "SHARPE", ui::colors::CYAN);
    sharpe_.container->setToolTip("Sharpe Ratio: risk-adjusted return over a risk-free rate.\n"
                                  "Formula: (mean daily return − risk-free rate) / std dev × √252.\n"
                                  "Above 1.0 = good, above 2.0 = very good, below 0 = worse than risk-free.");

    beta_ = add_chip(chips_layout, "BETA", ui::colors::WARNING);
    beta_.container->setToolTip("Portfolio sensitivity to broad market moves.\n"
                                "Beta = 1.0: moves with the market. >1.0: more volatile.\n"
                                "<1.0: less volatile. Negative: inverse correlation.");

    volatility_ = add_chip(chips_layout, "VOL 30D", ui::colors::AMBER);
    volatility_.container->setToolTip("Annualized portfolio volatility (std deviation of returns).\n"
                                      "Formula: std dev of daily returns × √252.\n"
                                      "Higher values indicate greater price swings.");

    max_drawdown_ = add_chip(chips_layout, "MDD", ui::colors::NEGATIVE);
    max_drawdown_.container->setToolTip("Maximum peak-to-trough decline in portfolio value.\n"
                                        "Measures the worst loss experienced from a high point.\n"
                                        "Lower magnitude = better capital preservation.");

    var95_ = add_chip(chips_layout, "VAR 95%", ui::colors::NEGATIVE);
    var95_.container->setToolTip("Value at Risk at 95% confidence — the maximum expected\n"
                                 "single-day loss 95% of the time based on historical returns.");

    risk_score_ = add_chip(chips_layout, "RISK", ui::colors::NEGATIVE);
    risk_score_.container->setToolTip("Composite risk score from 0 (low) to 100 (high).\n"
                                      "Weighted from: volatility, max drawdown, concentration,\n"
                                      "beta, and VaR. Lower is safer.");

    chips_layout->addStretch(1);
    scroll->setWidget(chips_host);

    outer->addWidget(secondary_row_);
}

// ── Hero cell (primary row) ──────────────────────────────────────────────────

PortfolioStatsRibbon::HeroCell PortfolioStatsRibbon::add_hero(QHBoxLayout* row, const QString& label_text,
                                                              const char* value_color) {
    auto* container = new QWidget(this);
    container->setObjectName("pfHeroCell");
    container->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto* vlayout = new QVBoxLayout(container);
    vlayout->setContentsMargins(14, 4, 14, 4);
    vlayout->setSpacing(1);

    HeroCell cell;
    cell.container = container;

    cell.label = new QLabel(label_text);
    vlayout->addWidget(cell.label);

    cell.value = new QLabel("--");
    vlayout->addWidget(cell.value);

    cell.sub = new QLabel;
    vlayout->addWidget(cell.sub);

    apply_hero_styles(cell, value_color);

    row->addWidget(container, 1);
    return cell;
}

void PortfolioStatsRibbon::apply_hero_styles(HeroCell& c, const char* value_color) {
    const int label_px = ui::fonts::font_px(-3); // ~10–11px
    const int value_px = ui::fonts::font_px(+4); // ~18px
    const int sub_px = ui::fonts::font_px(-4);   // ~9–10px

    if (c.label) {
        c.label->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; letter-spacing:1.2px;"
                                       "  background:transparent;")
                                   .arg(ui::colors::TEXT_TERTIARY())
                                   .arg(label_px));
    }
    if (c.value) {
        c.value->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; letter-spacing:0.2px;"
                                       "  background:transparent;")
                                   .arg(value_color)
                                   .arg(value_px));
    }
    if (c.sub) {
        c.sub->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:500;"
                                     "  background:transparent;")
                                 .arg(ui::colors::TEXT_TERTIARY())
                                 .arg(sub_px));
    }
    if (c.container) {
        c.container->setStyleSheet(QString("QWidget#pfHeroCell { border-right:1px solid %1; background:transparent; }")
                                       .arg(ui::colors::BORDER_DIM()));
    }
}

// ── Chip cell (secondary row) ────────────────────────────────────────────────

PortfolioStatsRibbon::ChipCell PortfolioStatsRibbon::add_chip(QHBoxLayout* row, const QString& label_text,
                                                              const char* value_color) {
    auto* container = new QWidget(this);
    container->setObjectName("pfChipCell");

    auto* hlayout = new QHBoxLayout(container);
    hlayout->setContentsMargins(8, 2, 10, 2);
    hlayout->setSpacing(5);

    ChipCell cell;
    cell.container = container;

    cell.label = new QLabel(label_text);
    hlayout->addWidget(cell.label);

    cell.value = new QLabel("--");
    hlayout->addWidget(cell.value);

    apply_chip_styles(cell, value_color);

    row->addWidget(container);
    return cell;
}

void PortfolioStatsRibbon::apply_chip_styles(ChipCell& c, const char* value_color) {
    const int label_px = ui::fonts::font_px(-4);
    const int value_px = ui::fonts::font_px(-2);

    if (c.label) {
        c.label->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; letter-spacing:0.8px;"
                                       "  background:transparent;")
                                   .arg(ui::colors::TEXT_TERTIARY())
                                   .arg(label_px));
    }
    if (c.value) {
        c.value->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; letter-spacing:0.2px;"
                                       "  background:transparent;")
                                   .arg(value_color)
                                   .arg(value_px));
    }
    if (c.container) {
        c.container->setStyleSheet(QString("QWidget#pfChipCell { background:%1; border:1px solid %2;"
                                           "  border-radius:3px; }")
                                       .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    }
}

// ── Data setters ─────────────────────────────────────────────────────────────

void PortfolioStatsRibbon::set_summary(const portfolio::PortfolioSummary& s) {
    auto fmt = [](double v, int dp = 2) { return QString::number(v, 'f', dp); };
    auto color_tok = [](double v) -> const char* { return v >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE; };

    // Total value
    total_value_.value->setText(fmt(s.total_market_value));
    total_value_.sub->setText(s.portfolio.currency);
    apply_hero_styles(total_value_, ui::colors::WARNING);

    // P&L — value = absolute P&L, sub = percent
    pnl_.value->setText(
        QString("%1%2").arg(s.total_unrealized_pnl >= 0 ? "+" : "").arg(fmt(s.total_unrealized_pnl)));
    pnl_.sub->setText(
        QString("%1%2%").arg(s.total_unrealized_pnl_percent >= 0 ? "+" : "").arg(fmt(s.total_unrealized_pnl_percent)));
    apply_hero_styles(pnl_, color_tok(s.total_unrealized_pnl));

    // Day change
    day_change_.value->setText(
        QString("%1%2").arg(s.total_day_change >= 0 ? "+" : "").arg(fmt(s.total_day_change)));
    day_change_.sub->setText(
        QString("%1%2%").arg(s.total_day_change_percent >= 0 ? "+" : "").arg(fmt(s.total_day_change_percent)));
    apply_hero_styles(day_change_, color_tok(s.total_day_change));

    // Positions
    positions_.value->setText(QString::number(s.total_positions));
    positions_.sub->setText(QString("\u25B2 %1   \u25BC %2").arg(s.gainers).arg(s.losers));
    apply_hero_styles(positions_, ui::colors::TEXT_PRIMARY);

    // Cost basis chip uses summary too
    cost_basis_.value->setText(fmt(s.total_cost_basis));
    apply_chip_styles(cost_basis_, ui::colors::CYAN);
}

void PortfolioStatsRibbon::set_metrics(const portfolio::ComputedMetrics& m) {
    auto fmt_opt = [](std::optional<double> v, int dp = 2) -> QString {
        return v.has_value() ? QString::number(*v, 'f', dp) : "--";
    };

    concentration_.value->setText(m.concentration_top3.has_value()
                                      ? QString("%1%").arg(QString::number(*m.concentration_top3, 'f', 1))
                                      : "--");
    apply_chip_styles(concentration_, ui::colors::AMBER);

    sharpe_.value->setText(fmt_opt(m.sharpe));
    apply_chip_styles(sharpe_, ui::colors::CYAN);

    beta_.value->setText(fmt_opt(m.beta));
    apply_chip_styles(beta_, ui::colors::WARNING);

    volatility_.value->setText(m.volatility.has_value() ? QString("%1%").arg(QString::number(*m.volatility, 'f', 1))
                                                        : "--");
    apply_chip_styles(volatility_, ui::colors::AMBER);

    max_drawdown_.value->setText(m.max_drawdown.has_value()
                                     ? QString("%1%").arg(QString::number(*m.max_drawdown, 'f', 1))
                                     : "--");
    apply_chip_styles(max_drawdown_, ui::colors::NEGATIVE);

    var95_.value->setText(m.var_95.has_value() ? QString::number(*m.var_95, 'f', 2) : "--");
    apply_chip_styles(var95_, ui::colors::NEGATIVE);

    if (m.risk_score.has_value()) {
        double rs = *m.risk_score;
        const char* rs_color = rs < 30 ? ui::colors::POSITIVE : rs < 60 ? ui::colors::WARNING : ui::colors::NEGATIVE;
        risk_score_.value->setText(QString::number(rs, 'f', 0));
        apply_chip_styles(risk_score_, rs_color);
    } else {
        risk_score_.value->setText("--");
        apply_chip_styles(risk_score_, ui::colors::NEGATIVE);
    }
}

void PortfolioStatsRibbon::refresh_theme() {
    setStyleSheet(QString("#portfolioStatsRibbon { background:%1; border-bottom:1px solid %2; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    if (secondary_row_) {
        secondary_row_->setStyleSheet(QString("#pfStatsSecondary { background:%1; }").arg(ui::colors::BG_BASE()));
    }

    apply_hero_styles(total_value_, ui::colors::WARNING);
    apply_hero_styles(pnl_, ui::colors::TEXT_PRIMARY);
    apply_hero_styles(day_change_, ui::colors::TEXT_PRIMARY);
    apply_hero_styles(positions_, ui::colors::TEXT_PRIMARY);

    apply_chip_styles(cost_basis_, ui::colors::CYAN);
    apply_chip_styles(concentration_, ui::colors::AMBER);
    apply_chip_styles(sharpe_, ui::colors::CYAN);
    apply_chip_styles(beta_, ui::colors::WARNING);
    apply_chip_styles(volatility_, ui::colors::AMBER);
    apply_chip_styles(max_drawdown_, ui::colors::NEGATIVE);
    apply_chip_styles(var95_, ui::colors::NEGATIVE);
    apply_chip_styles(risk_score_, ui::colors::NEGATIVE);
}

} // namespace fincept::screens
