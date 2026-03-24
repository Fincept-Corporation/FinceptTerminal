// src/screens/portfolio/PortfolioStatsRibbon.cpp
#include "screens/portfolio/PortfolioStatsRibbon.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

PortfolioStatsRibbon::PortfolioStatsRibbon(QWidget* parent) : QWidget(parent) {
    setFixedHeight(52);
    setObjectName("portfolioStatsRibbon");
    setStyleSheet(QString("#portfolioStatsRibbon { background:%1; border-bottom:1px solid %2; }")
                      .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    total_value_ = add_cell("TOTAL VALUE", ui::colors::WARNING);
    pnl_ = add_cell("UNREALIZED P&L", ui::colors::TEXT_PRIMARY);
    day_change_ = add_cell("DAY CHANGE", ui::colors::TEXT_PRIMARY);
    cost_basis_ = add_cell("COST BASIS", ui::colors::CYAN);
    positions_ = add_cell("POSITIONS", ui::colors::TEXT_PRIMARY);
    concentration_ = add_cell("CONC. TOP3", ui::colors::AMBER);
    sharpe_ = add_cell("SHARPE", ui::colors::CYAN);
    beta_ = add_cell("BETA", ui::colors::WARNING);
    volatility_ = add_cell("VOLATILITY", ui::colors::AMBER);
    max_drawdown_ = add_cell("MAX DRAWDOWN", ui::colors::NEGATIVE);
    risk_score_ = add_cell("RISK SCORE", ui::colors::NEGATIVE);
    var95_ = add_cell("VAR (95%)", ui::colors::NEGATIVE);
}

PortfolioStatsRibbon::MetricCell PortfolioStatsRibbon::add_cell(const QString& label_text, const char* value_color) {

    auto* container = new QWidget;
    auto* vlayout = new QVBoxLayout(container);
    vlayout->setContentsMargins(8, 4, 8, 4);
    vlayout->setSpacing(0);

    MetricCell cell;

    cell.label = new QLabel(label_text);
    cell.label->setStyleSheet(
        QString("color:%1; font-size:8px; font-weight:600; letter-spacing:0.5px;").arg(ui::colors::TEXT_TERTIARY));
    vlayout->addWidget(cell.label);

    cell.value = new QLabel("--");
    cell.value->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700;").arg(value_color));
    vlayout->addWidget(cell.value);

    cell.sub = new QLabel;
    cell.sub->setStyleSheet(QString("color:%1; font-size:8px;").arg(ui::colors::TEXT_TERTIARY));
    vlayout->addWidget(cell.sub);

    // Right border separator
    container->setStyleSheet(QString("border-right:1px solid %1;").arg(ui::colors::BORDER_DIM));

    static_cast<QHBoxLayout*>(layout())->addWidget(container, 1);

    return cell;
}

void PortfolioStatsRibbon::set_summary(const portfolio::PortfolioSummary& s) {
    auto fmt = [](double v, int dp = 2) { return QString::number(v, 'f', dp); };
    auto color = [](double v) -> const char* { return v >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE; };
    auto set_val_color = [](MetricCell& c, const QString& text, const char* col) {
        c.value->setText(text);
        c.value->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700;").arg(col));
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
        risk_score_.value->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700;").arg(rs_color));
    } else {
        risk_score_.value->setText("--");
    }

    var95_.value->setText(m.var_95.has_value() ? QString::number(*m.var_95, 'f', 2) : "--");
    var95_.sub->setText("1-Day");
}

} // namespace fincept::screens
