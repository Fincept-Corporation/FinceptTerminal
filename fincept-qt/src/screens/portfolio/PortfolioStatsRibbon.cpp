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
    setFixedHeight(52);
    setObjectName("portfolioStatsRibbon");
    setStyleSheet(QString("#portfolioStatsRibbon { background:%1; border-bottom:1px solid %2; }")
                      .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Scroll area prevents horizontal overflow
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background:transparent; border:none; }");

    auto* inner = new QWidget;
    inner->setStyleSheet("background:transparent;");
    cells_layout_ = new QHBoxLayout(inner);
    cells_layout_->setContentsMargins(0, 0, 0, 0);
    cells_layout_->setSpacing(0);

    total_value_ = add_cell("TOTAL VALUE", ui::colors::WARNING);
    total_value_.container->setToolTip(
        "Current market value of all holdings.\n"
        "Sum of (quantity × current price) for every position.");

    pnl_ = add_cell("UNREALIZED P&L", ui::colors::TEXT_PRIMARY);
    pnl_.container->setToolTip(
        "Unrealized profit or loss across all open positions.\n"
        "Calculated as: market value − total cost basis.\n"
        "This gain/loss is not realized until you sell.");

    day_change_ = add_cell("DAY CHANGE", ui::colors::TEXT_PRIMARY);
    day_change_.container->setToolTip(
        "Total portfolio value change today as a percentage.\n"
        "Weighted sum of each holding's intraday % change.");

    cost_basis_ = add_cell("COST BASIS", ui::colors::CYAN);
    cost_basis_.container->setToolTip(
        "Total amount invested — sum of (avg buy price × quantity)\n"
        "for all current positions. Used to compute P&L.");

    positions_ = add_cell("POSITIONS", ui::colors::TEXT_PRIMARY);
    positions_.container->setToolTip(
        "Number of distinct holdings in this portfolio,\n"
        "split into gainers (↑) and losers (↓) today.");

    concentration_ = add_cell("CONC. TOP3", ui::colors::AMBER);
    concentration_.container->setToolTip(
        "Concentration risk: combined weight of the top 3 holdings.\n"
        "Values above 50% indicate a concentrated portfolio.\n"
        "Lower is generally better for diversification.");

    sharpe_ = add_cell("SHARPE", ui::colors::CYAN);
    sharpe_.container->setToolTip(
        "Sharpe Ratio: risk-adjusted return over a risk-free rate.\n"
        "Formula: (mean daily return − risk-free rate) / std dev of returns × √252.\n"
        "Above 1.0 = good, above 2.0 = very good, below 0 = worse than risk-free.");

    beta_ = add_cell("BETA", ui::colors::WARNING);
    beta_.container->setToolTip(
        "Beta measures portfolio sensitivity to broad market moves.\n"
        "Beta = 1.0: moves with the market. >1.0: more volatile.\n"
        "<1.0: less volatile. Negative beta: inverse correlation.");

    volatility_ = add_cell("VOLATILITY", ui::colors::AMBER);
    volatility_.container->setToolTip(
        "Annualized portfolio volatility (standard deviation of returns).\n"
        "Formula: std dev of daily returns × √252.\n"
        "Higher values indicate greater price swings.");

    max_drawdown_ = add_cell("MAX DRAWDOWN", ui::colors::NEGATIVE);
    max_drawdown_.container->setToolTip(
        "Maximum peak-to-trough decline in portfolio value.\n"
        "Measures the worst loss experienced from a high point.\n"
        "Lower magnitude = better capital preservation.");

    risk_score_ = add_cell("RISK SCORE", ui::colors::NEGATIVE);
    risk_score_.container->setToolTip(
        "Composite risk score from 0 (low risk) to 100 (high risk).\n"
        "Weighted from: volatility, max drawdown, concentration,\n"
        "beta, and VaR. Lower is safer.");

    var95_ = add_cell("VAR (95%)", ui::colors::NEGATIVE);
    var95_.container->setToolTip(
        "Value at Risk at 95% confidence — the maximum expected\n"
        "single-day loss 95% of the time based on historical returns.\n"
        "E.g., VaR = -2.5% means you should not lose more than\n"
        "2.5% on 95% of trading days.");

    scroll->setWidget(inner);
    outer->addWidget(scroll);
}

PortfolioStatsRibbon::MetricCell PortfolioStatsRibbon::add_cell(const QString& label_text, const char* value_color) {

    auto* container = new QWidget;
    container->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto* vlayout = new QVBoxLayout(container);
    vlayout->setContentsMargins(6, 3, 6, 3);
    vlayout->setSpacing(0);

    MetricCell cell;
    cell.container = container;

    cell.label = new QLabel(label_text);
    cell.label->setStyleSheet(
        QString("color:%1; font-size:8px; font-weight:700; letter-spacing:1px;").arg(ui::colors::TEXT_TERTIARY));
    vlayout->addWidget(cell.label);

    cell.value = new QLabel("--");
    cell.value->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;").arg(value_color).arg(ui::fonts::font_px(0)));
    vlayout->addWidget(cell.value);

    cell.sub = new QLabel;
    cell.sub->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY));
    vlayout->addWidget(cell.sub);

    // Right border separator
    container->setStyleSheet(QString("border-right:1px solid %1;").arg(ui::colors::BORDER_DIM));

    cells_layout_->addWidget(container, 1);

    return cell;
}

void PortfolioStatsRibbon::set_summary(const portfolio::PortfolioSummary& s) {
    auto fmt = [](double v, int dp = 2) { return QString::number(v, 'f', dp); };
    auto color = [](double v) -> const char* { return v >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE; };
    auto set_val_color = [](MetricCell& c, const QString& text, const char* col) {
        c.value->setText(text);
        c.value->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;").arg(col).arg(ui::fonts::font_px(0)));
    };

    set_val_color(total_value_, fmt(s.total_market_value), ui::colors::WARNING);
    total_value_.sub->setText(s.portfolio.currency);

    set_val_color(pnl_, QString("%1%2").arg(s.total_unrealized_pnl >= 0 ? "+" : "").arg(fmt(s.total_unrealized_pnl)),
                  color(s.total_unrealized_pnl));
    pnl_.sub->setText(
        QString("%1%2%").arg(s.total_unrealized_pnl_percent >= 0 ? "+" : "").arg(fmt(s.total_unrealized_pnl_percent)));

    set_val_color(day_change_, QString("%1%2").arg(s.total_day_change >= 0 ? "+" : "").arg(fmt(s.total_day_change)),
                  color(s.total_day_change));
    day_change_.sub->setText(
        QString("%1%2%").arg(s.total_day_change_percent >= 0 ? "+" : "").arg(fmt(s.total_day_change_percent)));

    set_val_color(cost_basis_, fmt(s.total_cost_basis), ui::colors::CYAN);

    set_val_color(positions_, QString::number(s.total_positions), ui::colors::TEXT_PRIMARY);
    positions_.sub->setText(QString("W%1 / L%2").arg(s.gainers).arg(s.losers));
}

void PortfolioStatsRibbon::set_metrics(const portfolio::ComputedMetrics& m) {
    auto fmt_opt = [](std::optional<double> v, int dp = 2) -> QString {
        return v.has_value() ? QString::number(*v, 'f', dp) : "--";
    };

    concentration_.value->setText(
        m.concentration_top3.has_value() ? QString("%1%").arg(QString::number(*m.concentration_top3, 'f', 1)) : "--");

    sharpe_.value->setText(fmt_opt(m.sharpe));

    beta_.value->setText(fmt_opt(m.beta));

    volatility_.value->setText(m.volatility.has_value() ? QString("%1%").arg(QString::number(*m.volatility, 'f', 1))
                                                        : "--");
    volatility_.sub->setText("30D Ann.");

    max_drawdown_.value->setText(
        m.max_drawdown.has_value() ? QString("%1%").arg(QString::number(*m.max_drawdown, 'f', 1)) : "--");

    if (m.risk_score.has_value()) {
        double rs = *m.risk_score;
        const char* rs_color = rs < 30 ? ui::colors::POSITIVE : rs < 60 ? ui::colors::WARNING : ui::colors::NEGATIVE;
        risk_score_.value->setText(QString::number(rs, 'f', 0));
        risk_score_.value->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;").arg(rs_color).arg(ui::fonts::font_px(0)));
    } else {
        risk_score_.value->setText("--");
    }

    var95_.value->setText(m.var_95.has_value() ? QString::number(*m.var_95, 'f', 2) : "--");
    var95_.sub->setText("1-Day");
}

void PortfolioStatsRibbon::refresh_theme() {
    setStyleSheet(QString("#portfolioStatsRibbon { background:%1; border-bottom:1px solid %2; }")
                      .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    const QString lsz = QString::number(ui::fonts::font_px(-4));
    const QString vsz = QString::number(ui::fonts::font_px(0));
    const QString ssz = QString::number(ui::fonts::font_px(-3));

    auto refresh_cell = [&](MetricCell& c) {
        if (c.label)
            c.label->setStyleSheet(
                QString("color:%1; font-size:" + lsz + "px; font-weight:700; letter-spacing:1px;").arg(ui::colors::TEXT_TERTIARY));
        if (c.sub)
            c.sub->setStyleSheet(QString("color:%1; font-size:" + ssz + "px;").arg(ui::colors::TEXT_TERTIARY));
        if (c.container)
            c.container->setStyleSheet(QString("border-right:1px solid %1;").arg(ui::colors::BORDER_DIM));
    };

    refresh_cell(total_value_);
    refresh_cell(pnl_);
    refresh_cell(day_change_);
    refresh_cell(cost_basis_);
    refresh_cell(positions_);
    refresh_cell(concentration_);
    refresh_cell(sharpe_);
    refresh_cell(beta_);
    refresh_cell(volatility_);
    refresh_cell(max_drawdown_);
    refresh_cell(risk_score_);
    refresh_cell(var95_);
}

} // namespace fincept::screens
