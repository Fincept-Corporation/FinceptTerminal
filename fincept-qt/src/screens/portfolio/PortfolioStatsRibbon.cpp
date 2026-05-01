// src/screens/portfolio/PortfolioStatsRibbon.cpp
#include "screens/portfolio/PortfolioStatsRibbon.h"

#include "ui/theme/Theme.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLocale>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

QWidget* make_v_separator(QWidget* parent) {
    auto* sep = new QWidget(parent);
    sep->setFixedWidth(1);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    return sep;
}

} // namespace

PortfolioStatsRibbon::PortfolioStatsRibbon(QWidget* parent) : QWidget(parent) {
    setFixedHeight(72);
    setObjectName("portfolioStatsRibbon");
    setStyleSheet(QString("#portfolioStatsRibbon { background:%1; border-bottom:1px solid %2; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);

    // ── Cell 1: PORTFOLIO VALUE — biggest number on the row (24px hero) ──────
    row->addWidget(build_hero(value_cell_, "PORTFOLIO VALUE", 24), 28);
    row->addWidget(make_v_separator(this));

    // ── Cell 2: UNREALIZED P&L — 20px, signed color ──────────────────────────
    row->addWidget(build_hero(pnl_cell_, "UNREALIZED P&L", 20), 20);
    row->addWidget(make_v_separator(this));

    // ── Cell 3: TODAY — 20px, signed color ───────────────────────────────────
    row->addWidget(build_hero(day_cell_, "TODAY", 20), 16);
    row->addWidget(make_v_separator(this));

    // ── Cell 4: RISK & POSITIONING — 2×3 chip grid ───────────────────────────
    row->addWidget(build_risk_grid(), 36);
}

QWidget* PortfolioStatsRibbon::build_hero(HeroCell& c, const QString& label_text, int value_px) {
    c.container = new QWidget(this);
    c.container->setStyleSheet("background:transparent;");

    auto* v = new QVBoxLayout(c.container);
    v->setContentsMargins(14, 8, 14, 8);
    v->setSpacing(2);

    c.label = new QLabel(label_text);
    c.label->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1.2px; background:transparent;")
            .arg(ui::colors::TEXT_TERTIARY()));
    v->addWidget(c.label);

    c.value = new QLabel("--");
    c.value->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;"
                                   "  letter-spacing:0.2px; background:transparent;")
                               .arg(ui::colors::TEXT_PRIMARY())
                               .arg(value_px));
    v->addWidget(c.value);

    c.sub = new QLabel;
    c.sub->setStyleSheet(QString("color:%1; font-size:11px; font-weight:600; background:transparent;")
                             .arg(ui::colors::TEXT_TERTIARY()));
    v->addWidget(c.sub);

    return c.container;
}

void PortfolioStatsRibbon::apply_hero_value_color(HeroCell& c, const char* color, int value_px) {
    c.value->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;"
                                   "  letter-spacing:0.2px; background:transparent;")
                               .arg(color)
                               .arg(value_px));
}

QWidget* PortfolioStatsRibbon::build_risk_grid() {
    risk_cell_ = new QWidget(this);
    risk_cell_->setStyleSheet("background:transparent;");

    auto* outer = new QVBoxLayout(risk_cell_);
    outer->setContentsMargins(14, 8, 14, 8);
    outer->setSpacing(2);

    auto* header = new QLabel("RISK & POSITIONING");
    header->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1.2px; background:transparent;")
            .arg(ui::colors::TEXT_TERTIARY()));
    outer->addWidget(header);

    auto* g = new QGridLayout;
    g->setContentsMargins(0, 0, 0, 0);
    g->setHorizontalSpacing(20);
    g->setVerticalSpacing(0);

    // Layout: label TEXT_TERTIARY 10px on left, value 13px / 700 on right.
    // 2 columns × 3 rows. Left column: SHARPE / BETA / MDD. Right: CONC / VOL30 / RISK.
    sharpe_ = add_grid_chip(g, 0, 0, "SHARPE");
    conc_   = add_grid_chip(g, 0, 1, "CONC");
    beta_   = add_grid_chip(g, 1, 0, "BETA");
    vol_    = add_grid_chip(g, 1, 1, "VOL 30D");
    mdd_    = add_grid_chip(g, 2, 0, "MDD");
    risk_   = add_grid_chip(g, 2, 1, "RISK");

    outer->addLayout(g, 1);

    return risk_cell_;
}

PortfolioStatsRibbon::GridChip PortfolioStatsRibbon::add_grid_chip(QGridLayout* g, int row, int col,
                                                                    const QString& label_text) {
    GridChip c;

    // Each chip occupies 2 grid sub-columns within its parent cell so we can
    // align label-left + value-right. We use a small inline QHBoxLayout per
    // chip and place that layout's holder widget at (row, col) of the outer
    // 2-col grid.
    auto* holder = new QWidget(risk_cell_);
    holder->setStyleSheet("background:transparent;");
    auto* h = new QHBoxLayout(holder);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);

    c.label = new QLabel(label_text);
    c.label->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; letter-spacing:0.5px;"
                                   "  background:transparent;")
                               .arg(ui::colors::TEXT_TERTIARY()));
    h->addWidget(c.label);

    c.value = new QLabel("--");
    c.value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    c.value->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700;"
                                   "  background:transparent;")
                               .arg(ui::colors::TEXT_PRIMARY()));
    h->addWidget(c.value, 1);

    g->addWidget(holder, row, col);
    return c;
}

// ── Data setters ─────────────────────────────────────────────────────────────

void PortfolioStatsRibbon::set_summary(const portfolio::PortfolioSummary& s) {
    auto fmt = [](double v, int dp = 2) {
        // Add thousands separator for readability on large NAVs.
        return QLocale().toString(v, 'f', dp);
    };
    auto color_tok = [](double v) -> const char* { return v >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE; };

    // ── Cell 1: PORTFOLIO VALUE ──────────────────────────────────────────────
    // Value: amber (brand value, neutral semantics). Sub: position count.
    value_cell_.value->setText(QString("%1  %2").arg(fmt(s.total_market_value), s.portfolio.currency));
    apply_hero_value_color(value_cell_, ui::colors::AMBER(), 24);
    value_cell_.sub->setText(QString("▲ %1 positions").arg(s.total_positions));

    // ── Cell 2: UNREALIZED P&L ───────────────────────────────────────────────
    pnl_cell_.value->setText(
        QString("%1%2").arg(s.total_unrealized_pnl >= 0 ? "+" : "").arg(fmt(s.total_unrealized_pnl)));
    apply_hero_value_color(pnl_cell_, color_tok(s.total_unrealized_pnl), 20);
    pnl_cell_.sub->setText(
        QString("%1%2%   ▲ %3   ▼ %4")
            .arg(s.total_unrealized_pnl_percent >= 0 ? "+" : "")
            .arg(QString::number(s.total_unrealized_pnl_percent, 'f', 2))
            .arg(s.gainers)
            .arg(s.losers));
    pnl_cell_.sub->setStyleSheet(QString("color:%1; font-size:11px; font-weight:600; background:transparent;")
                                     .arg(color_tok(s.total_unrealized_pnl)));

    // ── Cell 3: TODAY ────────────────────────────────────────────────────────
    day_cell_.value->setText(
        QString("%1%2").arg(s.total_day_change >= 0 ? "+" : "").arg(fmt(s.total_day_change)));
    apply_hero_value_color(day_cell_, color_tok(s.total_day_change), 20);
    day_cell_.sub->setText(QString("%1%2%")
                               .arg(s.total_day_change_percent >= 0 ? "+" : "")
                               .arg(QString::number(s.total_day_change_percent, 'f', 2)));
    day_cell_.sub->setStyleSheet(QString("color:%1; font-size:11px; font-weight:600; background:transparent;")
                                     .arg(color_tok(s.total_day_change)));

    // CONC chip uses summary as a fallback while ComputedMetrics is loading;
    // ComputedMetrics overrides it below if/when it arrives.
    if (!s.holdings.isEmpty()) {
        // simple top-3 fallback (full calc lives in ComputedMetrics.concentration_top3)
        QVector<double> ws;
        for (const auto& h : s.holdings)
            ws.append(h.weight);
        std::sort(ws.begin(), ws.end(), std::greater<double>());
        double top3 = 0.0;
        for (int i = 0; i < std::min<int>(3, ws.size()); ++i)
            top3 += ws[i];
        conc_.value->setText(QString("%1%").arg(QString::number(top3, 'f', 1)));
    }
}

void PortfolioStatsRibbon::set_metrics(const portfolio::ComputedMetrics& m) {
    auto fmt_opt = [](std::optional<double> v, int dp = 2) -> QString {
        return v.has_value() ? QString::number(*v, 'f', dp) : "--";
    };
    auto fmt_pct_opt = [](std::optional<double> v, int dp = 1) -> QString {
        return v.has_value() ? QString("%1%").arg(QString::number(*v, 'f', dp)) : "--";
    };

    sharpe_.value->setText(fmt_opt(m.sharpe));
    beta_.value->setText(fmt_opt(m.beta));
    vol_.value->setText(fmt_pct_opt(m.volatility));

    if (m.concentration_top3.has_value())
        conc_.value->setText(fmt_pct_opt(m.concentration_top3));

    // MDD always negative — semantic NEGATIVE color.
    mdd_.value->setText(fmt_pct_opt(m.max_drawdown));
    mdd_.value->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700; background:transparent;")
                                  .arg(ui::colors::NEGATIVE()));

    // RISK score: threshold-driven semantic color.
    if (m.risk_score.has_value()) {
        const double rs = *m.risk_score;
        const char* rs_color = rs < 30 ? ui::colors::POSITIVE
                              : rs < 60 ? ui::colors::WARNING
                                        : ui::colors::NEGATIVE;
        risk_.value->setText(QString::number(rs, 'f', 0));
        risk_.value->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700; background:transparent;")
                                       .arg(rs_color));
    } else {
        risk_.value->setText("--");
        risk_.value->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700; background:transparent;")
                                       .arg(ui::colors::TEXT_PRIMARY()));
    }
}

void PortfolioStatsRibbon::refresh_theme() {
    setStyleSheet(QString("#portfolioStatsRibbon { background:%1; border-bottom:1px solid %2; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    // Hero label/sub colors and chip label colors are static per-style; the
    // value colors get re-applied on the next set_summary/set_metrics call,
    // which always fires after a theme switch via PortfolioScreen's
    // refresh_theme path.
}

} // namespace fincept::screens
