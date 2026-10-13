// src/screens/portfolio/PortfolioStatsRibbon.cpp
#include "screens/portfolio/PortfolioStatsRibbon.h"

#include "ui/theme/Theme.h"

#include <QEvent>
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
    row->addWidget(build_hero(value_cell_, tr("PORTFOLIO VALUE"), 24), 28);
    row->addWidget(make_v_separator(this));

    // ── Cell 2: UNREALIZED P&L — 20px, signed color ──────────────────────────
    row->addWidget(build_hero(pnl_cell_, tr("UNREALIZED P&L"), 20), 20);
    row->addWidget(make_v_separator(this));

    // ── Cell 3: TODAY — 20px, signed color ───────────────────────────────────
    row->addWidget(build_hero(day_cell_, tr("TODAY"), 20), 16);
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
    c.sub->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:600; background:transparent;").arg(ui::colors::TEXT_TERTIARY()));
    v->addWidget(c.sub);

    return c.container;
}

void PortfolioStatsRibbon::apply_hero_value_color(HeroCell& c, const char* color, int value_px) {
    // Skip the restyle when the colour has not flipped. This runs on every
    // poll and each setStyleSheet is a CSS reparse + repolish of the label.
    const QString color_str = QString::fromLatin1(color);
    if (c.value->property("colorTok").toString() == color_str)
        return;
    c.value->setProperty("colorTok", color_str);
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

    risk_header_ = new QLabel(tr("RISK & POSITIONING"));
    risk_header_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1.2px; background:transparent;")
            .arg(ui::colors::TEXT_TERTIARY()));
    outer->addWidget(risk_header_);

    auto* g = new QGridLayout;
    g->setContentsMargins(0, 0, 0, 0);
    g->setHorizontalSpacing(20);
    g->setVerticalSpacing(0);

    // Layout: label TEXT_TERTIARY 10px on left, value 13px / 700 on right.
    // 2 columns × 3 rows. Left column: SHARPE / BETA / MDD. Right: CONC / VOL30 / RISK.
    sharpe_ = add_grid_chip(g, 0, 0, tr("SHARPE"));
    conc_ = add_grid_chip(g, 0, 1, tr("CONC"));
    beta_ = add_grid_chip(g, 1, 0, tr("BETA"));
    vol_ = add_grid_chip(g, 1, 1, tr("VOL"));
    mdd_ = add_grid_chip(g, 2, 0, tr("MDD"));
    risk_ = add_grid_chip(g, 2, 1, tr("RISK"));

    apply_chip_tooltips();

    outer->addLayout(g, 1);

    return risk_cell_;
}

void PortfolioStatsRibbon::apply_chip_tooltips() {
    // Six four-letter labels with no explanation of window, benchmark or
    // scale. Say what each number actually is.
    auto set = [](GridChip& c, const QString& text) {
        if (c.label)
            c.label->setToolTip(text);
        if (c.value) {
            c.value->setToolTip(text);
            c.value->setAccessibleDescription(text);
        }
    };
    // Say exactly what each figure is: window, basis, benchmark, risk-free.
    const auto& m = last_metrics_;
    const QString bench = m.benchmark_symbol.isEmpty() ? tr("the benchmark") : m.benchmark_symbol;
    const QString window = m.perf_observations > 0 ? tr("%1 periods, %2 \u2192 %3")
                                                         .arg(m.perf_observations)
                                                         .arg(m.perf_start, m.perf_end)
                                                   : tr("no history yet");
    const QString rf = tr("risk-free %1% (%2)")
                           .arg(QString::number(m.risk_free_rate * 100.0, 'f', 2), m.risk_free_source);
    set(sharpe_, tr("Sharpe ratio — annualised excess return per unit of volatility, from the realised "
                    "time-weighted return series (buys/sells treated as capital flows): %1; %2. "
                    "Needs 20+ periods.")
                     .arg(window, rf));
    set(conc_, tr("Concentration — combined weight of your three largest positions."));
    set(beta_, m.risk_ex_ante ? tr("Beta vs %1 — current weights regressed on %2 trailing daily returns "
                                   "(ex-ante). 1.0 moves with the market.")
                                    .arg(bench)
                                    .arg(m.risk_observations)
                              : tr("Beta vs %1 — regression of the realised return series (%2). "
                                   "1.0 moves with the market.")
                                    .arg(bench, window));
    set(vol_, m.risk_ex_ante ? tr("Volatility — annualised standard deviation of daily returns at CURRENT "
                                  "weights over %1 trailing trading days (ex-ante).")
                                   .arg(m.risk_observations)
                             : tr("Volatility — annualised standard deviation of the realised time-weighted "
                                  "returns (%1).")
                                   .arg(window));
    set(mdd_, tr("Maximum drawdown — worst peak-to-trough fall of the time-weighted return index (%1). "
                 "Deposits and withdrawals do not count as drawdowns.")
                  .arg(window));
    set(risk_, tr("Composite risk score, 0–100, from volatility, concentration, drawdown and beta. "
                  "Lower is calmer; above 60 is aggressive. Blank until volatility is known."));
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
    last_summary_ = s;
    auto fmt = [](double v, int dp = 2) {
        // Add thousands separator for readability on large NAVs.
        return QLocale().toString(v, 'f', dp);
    };
    auto color_tok = [](double v) -> const char* { return v >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE; };

    // ── Cell 1: PORTFOLIO VALUE ──────────────────────────────────────────────
    // Value: amber (brand value, neutral semantics). Sub: position count.
    value_cell_.value->setText(QString("%1  %2").arg(fmt(s.total_market_value), s.portfolio.currency));
    apply_hero_value_color(value_cell_, ui::colors::AMBER(), 24);
    value_cell_.sub->setText(tr("▲ %1 positions").arg(s.total_positions));

    // ── Cell 2: UNREALIZED P&L ───────────────────────────────────────────────
    pnl_cell_.value->setText(
        QString("%1%2").arg(s.total_unrealized_pnl >= 0 ? "+" : "").arg(fmt(s.total_unrealized_pnl)));
    apply_hero_value_color(pnl_cell_, color_tok(s.total_unrealized_pnl), 20);
    QString pnl_sub = QString("%1%2%   ▲ %3   ▼ %4")
                          .arg(s.total_unrealized_pnl_percent >= 0 ? "+" : "")
                          .arg(QString::number(s.total_unrealized_pnl_percent, 'f', 2))
                          .arg(s.gainers)
                          .arg(s.losers);
    // Realised P&L and dividend income live in the ledger, not in the open
    // positions — show them next to the unrealised figure instead of nowhere.
    if (std::abs(s.total_realized_pnl) > 0.005 || s.total_dividends > 0.005)
        pnl_sub += tr("   R %1%2   D +%3")
                       .arg(s.total_realized_pnl >= 0 ? "+" : "")
                       .arg(fmt(s.total_realized_pnl, 0))
                       .arg(fmt(s.total_dividends, 0));
    pnl_cell_.sub->setText(pnl_sub);
    const QString pnl_tip = tr("Unrealized P&L: market value of open positions minus their average cost.\n"
                               "Realized P&L (closed lots, average-cost method): %1 %2\n"
                               "Dividends received: %1 %3\n"
                               "Total return incl. realized and dividends: %1 %4")
                                .arg(s.portfolio.currency, fmt(s.total_realized_pnl), fmt(s.total_dividends),
                                     fmt(s.total_unrealized_pnl + s.total_realized_pnl + s.total_dividends));
    pnl_cell_.value->setToolTip(pnl_tip);
    pnl_cell_.sub->setToolTip(pnl_tip);
    pnl_cell_.sub->setStyleSheet(QString("color:%1; font-size:11px; font-weight:600; background:transparent;")
                                     .arg(color_tok(s.total_unrealized_pnl)));

    // ── Cell 3: TODAY ────────────────────────────────────────────────────────
    day_cell_.value->setText(QString("%1%2").arg(s.total_day_change >= 0 ? "+" : "").arg(fmt(s.total_day_change)));
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
    } else {
        conc_.value->setText(QStringLiteral("--")); // sold out: don't keep the old figure
    }
}

void PortfolioStatsRibbon::set_metrics(const portfolio::ComputedMetrics& m) {
    last_metrics_ = m;
    apply_chip_tooltips();
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
    else
        conc_.value->setText(QStringLiteral("--"));

    // MDD always negative — semantic NEGATIVE color.
    mdd_.value->setText(fmt_pct_opt(m.max_drawdown));
    mdd_.value->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; background:transparent;").arg(ui::colors::NEGATIVE()));

    // RISK score: threshold-driven semantic color.
    if (m.risk_score.has_value()) {
        const double rs = *m.risk_score;
        const char* rs_color = rs < 30 ? ui::colors::POSITIVE : rs < 60 ? ui::colors::WARNING : ui::colors::NEGATIVE;
        risk_.value->setText(QString::number(rs, 'f', 0));
        risk_.value->setStyleSheet(
            QString("color:%1; font-size:13px; font-weight:700; background:transparent;").arg(rs_color));
    } else {
        risk_.value->setText("--");
        risk_.value->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700; background:transparent;")
                                       .arg(ui::colors::TEXT_PRIMARY()));
    }
}

void PortfolioStatsRibbon::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PortfolioStatsRibbon::retranslateUi() {
    if (value_cell_.label)
        value_cell_.label->setText(tr("PORTFOLIO VALUE"));
    if (pnl_cell_.label)
        pnl_cell_.label->setText(tr("UNREALIZED P&L"));
    if (day_cell_.label)
        day_cell_.label->setText(tr("TODAY"));
    if (risk_header_)
        risk_header_->setText(tr("RISK & POSITIONING"));

    if (sharpe_.label)
        sharpe_.label->setText(tr("SHARPE"));
    if (conc_.label)
        conc_.label->setText(tr("CONC"));
    if (beta_.label)
        beta_.label->setText(tr("BETA"));
    if (vol_.label)
        vol_.label->setText(tr("VOL"));
    if (mdd_.label)
        mdd_.label->setText(tr("MDD"));
    if (risk_.label)
        risk_.label->setText(tr("RISK"));

    apply_chip_tooltips();

    if (last_summary_.has_value()) {
        // Re-render the "▲ %1 positions" sub-line under PORTFOLIO VALUE.
        if (value_cell_.sub)
            value_cell_.sub->setText(tr("▲ %1 positions").arg(last_summary_->total_positions));
    }
}

void PortfolioStatsRibbon::refresh_theme() {
    setStyleSheet(QString("#portfolioStatsRibbon { background:%1; border-bottom:1px solid %2; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    // Drop the "already this colour" memo in apply_hero_value_color(), or the
    // hero values would keep the previous theme's palette.
    for (HeroCell* c : {&value_cell_, &pnl_cell_, &day_cell_}) {
        if (c->value)
            c->value->setProperty("colorTok", QString());
    }
    // Hero label/sub colors and chip label colors are static per-style; the
    // value colors get re-applied on the next set_summary/set_metrics call,
    // which always fires after a theme switch via PortfolioScreen's
    // refresh_theme path.
}

} // namespace fincept::screens
