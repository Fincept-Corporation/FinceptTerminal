// src/screens/portfolio/PortfolioStatsRibbon.cpp
#include "screens/portfolio/PortfolioStatsRibbon.h"

#include "screens/portfolio/PortfolioFormat.h"
#include "services/portfolio/PortfolioPerformance.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

namespace {

constexpr const char* kRibbonWindowKey = "portfolio.ribbon.window";

const portfolio::PerfWindow kRibbonWindows[] = {portfolio::PerfWindow::MTD, portfolio::PerfWindow::QTD,
                                                portfolio::PerfWindow::YTD, portfolio::PerfWindow::Y1,
                                                portfolio::PerfWindow::Y3,  portfolio::PerfWindow::Y5,
                                                portfolio::PerfWindow::All};

QString ribbon_window_text(portfolio::PerfWindow w) {
    switch (w) {
        case portfolio::PerfWindow::MTD:
            return PortfolioStatsRibbon::tr("Month to date");
        case portfolio::PerfWindow::QTD:
            return PortfolioStatsRibbon::tr("Quarter to date");
        case portfolio::PerfWindow::YTD:
            return PortfolioStatsRibbon::tr("Year to date");
        case portfolio::PerfWindow::Y1:
            return PortfolioStatsRibbon::tr("1 year");
        case portfolio::PerfWindow::Y3:
            return PortfolioStatsRibbon::tr("3 years");
        case portfolio::PerfWindow::Y5:
            return PortfolioStatsRibbon::tr("5 years");
        case portfolio::PerfWindow::All:
            break;
    }
    return PortfolioStatsRibbon::tr("Since inception");
}

QVector<double> ribbon_nav_spark(const QVector<portfolio::TwrPoint>& series) {
    QVector<double> v;
    v.reserve(series.size());
    for (const auto& p : series)
        v.append(p.nav > 0 ? p.nav : portfolio::kAbsent);
    return v;
}

ui::StatTile::Tone ribbon_tone(double v) {
    if (!std::isfinite(v) || v == 0)
        return ui::StatTile::Tone::Neutral;
    return v > 0 ? ui::StatTile::Tone::Positive : ui::StatTile::Tone::Negative;
}

QString ribbon_css() {
    return QString("#portfolioStatsRibbon { background:%1; border-bottom:1px solid %2; }"
                   "#portfolioStatsRibbon QLabel { color:%3; font-size:%4px; font-weight:700;"
                   "  letter-spacing:1px; background:transparent; }"
                   "#portfolioStatsRibbon QComboBox { background:%5; color:%6; border:1px solid %2;"
                   "  padding:1px 6px; font-size:%7px; font-weight:700; }")
        .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_TERTIARY(), ui::fonts::px(-4),
             ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::fonts::px(-3));
}

} // namespace

PortfolioStatsRibbon::PortfolioStatsRibbon(QWidget* parent) : QWidget(parent) {
    setObjectName("portfolioStatsRibbon");
    {
        auto r = SettingsRepository::instance().get(kRibbonWindowKey);
        if (r.is_ok() && !r.value().isEmpty())
            window_ = portfolio::perf_window_from_key(r.value());
    }
    setStyleSheet(ribbon_css()); // before the children exist: each is styled once, at birth
    build_ui();
    refresh_theme();
    // Currency / number format changed: re-format the figures already held.
    ui::formatting::on_format_changed(this, [this]() {
        render_summary();
        render_metrics();
    });
}

void PortfolioStatsRibbon::build_ui() {
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(8, 6, 8, 6);
    row->setSpacing(8);

    auto* period_box = new QWidget(this);
    auto* pv = new QVBoxLayout(period_box);
    pv->setContentsMargins(0, 0, 0, 0);
    pv->setSpacing(2);
    period_label_ = new QLabel(tr("PERIOD"), period_box);
    pv->addWidget(period_label_);
    period_cb_ = new QComboBox(period_box);
    for (const auto w : kRibbonWindows)
        period_cb_->addItem(portfolio::perf_window_key(w), static_cast<int>(w));
    for (int i = 0; i < period_cb_->count(); ++i)
        if (period_cb_->itemData(i).toInt() == static_cast<int>(window_))
            period_cb_->setCurrentIndex(i);
    period_cb_->setAccessibleName(tr("Performance period of the return and risk figures"));
    connect(period_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        const auto w = static_cast<portfolio::PerfWindow>(period_cb_->itemData(idx).toInt());
        if (w == window_)
            return;
        window_ = w;
        metrics_.reset();
        SettingsRepository::instance().set(kRibbonWindowKey, portfolio::perf_window_key(w), "portfolio");
        set_risk_loading();
        emit window_changed(w);
    });
    pv->addWidget(period_cb_);
    pv->addStretch(1);
    row->addWidget(period_box, 0, Qt::AlignTop);

    strip_ = new ui::KpiStrip(this);
    strip_->set_spacing(6);
    strip_->set_min_tile_width(ui::metrics::width(132));
    nav_ = strip_->add_tile("nav", tr("NAV"));
    day_ = strip_->add_tile("day", tr("TODAY"));
    ret_ = strip_->add_tile("ret", tr("RETURN"));
    upnl_ = strip_->add_tile("upnl", tr("UNREALISED P&L"));
    real_ = strip_->add_tile("real", tr("REALISED + INCOME"));
    cash_ = strip_->add_tile("cash", tr("CASH"));
    vol_ = strip_->add_tile("vol", tr("VOLATILITY"));
    sharpe_ = strip_->add_tile("sharpe", tr("SHARPE"));
    beta_ = strip_->add_tile("beta", tr("BETA"));
    mdd_ = strip_->add_tile("mdd", tr("MAX DRAWDOWN"));
    for (auto* t : {nav_, day_, ret_, upnl_, real_, cash_, vol_, sharpe_, beta_, mdd_}) {
        t->set_compact(true);
        t->set_state(ui::StatTile::State::Loading);
    }
    vol_->set_delta_inverted(true);
    mdd_->set_delta_inverted(true);
    row->addWidget(strip_, 1);
}

QString PortfolioStatsRibbon::window_label() const {
    return portfolio::perf_window_key(window_);
}

void PortfolioStatsRibbon::set_risk_loading() {
    for (auto* t : {ret_, vol_, sharpe_, beta_, mdd_})
        t->set_state(ui::StatTile::State::Loading, tr("Computing %1 figures…").arg(ribbon_window_text(window_)));
    ret_->set_label(tr("RETURN %1").arg(window_label()));
}

void PortfolioStatsRibbon::set_summary(const portfolio::PortfolioSummary& s) {
    if (summary_ && summary_->portfolio.id != s.portfolio.id)
        metrics_.reset(); // a different book: the old window figures do not describe it
    summary_ = s;
    render_summary();
    if (!metrics_)
        set_risk_loading();
}

void PortfolioStatsRibbon::set_metrics(const portfolio::ComputedMetrics& m) {
    if (portfolio::perf_window_from_key(m.window) != window_)
        return;
    if (summary_ && !m.portfolio_id.isEmpty() && m.portfolio_id != summary_->portfolio.id)
        return;
    metrics_ = m;
    render_metrics();
}

void PortfolioStatsRibbon::render_summary() {
    if (!summary_)
        return;
    const auto& s = *summary_;
    const QString ccy = pfmt::base_ccy(s);
    using State = ui::StatTile::State;
    using Tone = ui::StatTile::Tone;

    // ── NAV ──────────────────────────────────────────────────────────────────
    const double navv = s.total_market_value;
    nav_->set_state(State::Normal);
    // "≈" when a conversion used a stale FX rate (the rate's age is in the tooltip).
    nav_->set_value(pfmt::approx_prefix(s) +
                    (std::abs(navv) >= 1e6 ? ui::formatting::money(navv, ccy, ui::formatting::Compact::None, 0)
                                           : pfmt::money(navv, ccy)));
    const QString stale_fx = pfmt::fx_stale_line(s);
    if (!s.complete) {
        nav_->set_tone(Tone::Warning);
        nav_->set_sub_label(tr("INCOMPLETE · %1/%2 valued").arg(s.valued_positions).arg(s.total_positions));
        nav_->set_tooltip_text(pfmt::incomplete_line(s) + (stale_fx.isEmpty() ? QString() : QLatin1Char('\n') + stale_fx));
    } else {
        nav_->set_tone(Tone::Neutral);
        nav_->set_sub_label(s.as_of.isEmpty() ? tr("%n position(s)", "", s.total_positions)
                                              : tr("%n position(s) · %1", "", s.total_positions).arg(pfmt::date(s.as_of)));
        nav_->set_tooltip_text(tr("Net asset value in %1%2. Prices as of %3.")
                                   .arg(ccy, s.cash_in_nav ? tr(", cash included") : QString(), pfmt::date(s.as_of)) +
                               (stale_fx.isEmpty() ? QString() : QLatin1Char('\n') + stale_fx));
    }

    // ── TODAY ────────────────────────────────────────────────────────────────
    day_->set_state(State::Normal);
    if (s.valued_positions == 0) {
        day_->set_value(pfmt::dash());
        day_->clear_delta();
        day_->set_tone(Tone::Neutral);
    } else {
        // The % goes to the sub-label: as a delta beside the value it took the row's
        // width first and the value itself was elided ("+$13…") in a 10-tile ribbon.
        day_->set_value(pfmt::smoney(s.total_day_change, ccy));
        day_->clear_delta();
        day_->set_sub_label(pfmt::pct(s.total_day_change_percent));
        day_->set_tone(ribbon_tone(s.total_day_change));
    }
    day_->set_tooltip_text(tr("Change since the previous close of each valued holding, in %1.").arg(ccy));

    // ── UNREALISED P&L ──────────────────────────────────────────────────────
    upnl_->set_state(State::Normal);
    if (s.valued_positions == 0) {
        // Nothing open (or nothing priced yet): no P&L to split — "FX split unavailable"
        // on an empty book read like a fault.
        upnl_->set_value(pfmt::dash());
        upnl_->clear_delta();
        upnl_->set_tone(Tone::Neutral);
        upnl_->set_sub_label(s.total_positions == 0 ? tr("no open positions") : tr("no position priced yet"));
        upnl_->set_tooltip_text(tr("Unrealised P&L of open positions in %1.").arg(ccy));
    } else {
        upnl_->set_value(pfmt::smoney(s.total_unrealized_pnl, ccy));
        upnl_->clear_delta(); // the % leads the sub-label (a delta elided the value)
        upnl_->set_tone(ribbon_tone(s.total_unrealized_pnl));
    }
    const QString upnl_pct = pfmt::pct(s.total_unrealized_pnl_percent);
    if (s.valued_positions == 0) {
        // sub label / tooltip set above
    } else if (!pfmt::fx_split_shown(s)) {
        // Split off in Settings, or nothing held in a foreign currency: one P&L figure.
        upnl_->set_sub_label(upnl_pct);
        upnl_->set_tooltip_text(tr("Unrealised P&L of open positions in %1.").arg(ccy));
    } else if (std::isfinite(s.total_fx_pnl) && std::isfinite(s.total_price_pnl)) {
        upnl_->set_sub_label(tr("%1 · Price %2 · FX %3")
                                 .arg(upnl_pct, pfmt::smoney_c(s.total_price_pnl, ccy),
                                      pfmt::smoney_c(s.total_fx_pnl, ccy)));
        upnl_->set_tooltip_text(tr("Unrealised P&L of open positions in %1, split into the move of the price in its "
                                   "own currency and the move of that currency against %1 since purchase.")
                                    .arg(ccy));
    } else {
        upnl_->set_sub_label(tr("%1 · FX split unavailable").arg(upnl_pct));
        upnl_->set_tooltip_text(tr("Some positions have no trade-date FX rate (cost converted at today's rate), so the "
                                   "P&L cannot be split into price and currency effects. Record the FX rate on the "
                                   "BUY rows to enable it."));
    }

    // ── REALISED + INCOME ───────────────────────────────────────────────────
    const double realised_income = s.total_realized_pnl + s.total_dividends + s.total_interest;
    real_->set_state(State::Normal);
    real_->set_value(pfmt::smoney(realised_income, ccy));
    real_->set_tone(ribbon_tone(realised_income));
    real_->set_sub_label(tr("Realised %1 · Income %2")
                             .arg(pfmt::smoney_c(s.total_realized_pnl, ccy),
                                  pfmt::money_c(s.total_dividends + s.total_interest, ccy)));
    real_->set_tooltip_text(tr("Realised P&L (%1 lot relief): short-term %2, long-term %3.\n"
                               "Dividends (net of withholding): %4. Interest: %5.\n"
                               "Fees paid: %6. Taxes paid: %7.")
                                .arg(s.portfolio.lot_method, pfmt::smoney(s.realized_short_term, ccy),
                                     pfmt::smoney(s.realized_long_term, ccy), pfmt::money(s.total_dividends, ccy),
                                     pfmt::money(s.total_interest, ccy), pfmt::money(s.total_fees, ccy),
                                     pfmt::money(s.total_taxes, ccy)));

    // ── CASH ─────────────────────────────────────────────────────────────────
    cash_->set_state(State::Normal);
    if (!s.portfolio.track_cash && !(std::isfinite(s.cash_value) && s.cash_value > 0)) {
        // Untracked: the ledger still nets purchases against sales / income, but without
        // recorded deposits that figure (a large negative for any bought book) is not a
        // cash balance — "-$31,096.75" in a warning colour read as an overdraft. A
        // positive balance (deposits recorded) is still shown below, "not in NAV".
        cash_->set_value(pfmt::dash());
        cash_->set_sub_label(tr("not tracked"));
        cash_->set_tone(Tone::Neutral);
        cash_->set_tooltip_text(tr("Cash is not tracked for this portfolio. Turn on cash tracking in Portfolio "
                                   "Settings to record deposits, withdrawals and trade settlement."));
    } else {
        cash_->set_value(pfmt::money(s.cash_value, ccy));
        cash_->set_sub_label(s.cash_in_nav ? tr("in NAV") : tr("not in NAV"));
        QStringList lines;
        for (auto it = s.cash_balances.constBegin(); it != s.cash_balances.constEnd(); ++it)
            lines << pfmt::money(it.value(), it.key());
        cash_->set_tooltip_text(lines.isEmpty() ? tr("No cash balances.")
                                                : tr("Cash balances:\n%1").arg(lines.join(QLatin1Char('\n'))));
        cash_->set_tone(std::isfinite(s.cash_value) && s.cash_value < 0 ? Tone::Warning : Tone::Neutral);
    }
}

void PortfolioStatsRibbon::render_metrics() {
    if (!metrics_)
        return;
    const auto& m = *metrics_;
    using State = ui::StatTile::State;
    using Tone = ui::StatTile::Tone;
    const QString bench = m.benchmark_symbol.isEmpty() ? tr("benchmark") : m.benchmark_symbol;
    const QString span = m.perf_start.isEmpty()
                             ? tr("no history yet")
                             : tr("%1 → %2").arg(pfmt::date(m.perf_start), pfmt::date(m.perf_end));

    // ── RETURN (window) vs benchmark ────────────────────────────────────────
    ret_->set_label(tr("RETURN %1").arg(window_label()));
    if (m.total_return) {
        ret_->set_state(State::Normal);
        ret_->set_value(pfmt::pct(*m.total_return));
        ret_->set_tone(ribbon_tone(*m.total_return));
        // Excess return leads the sub-label: as a delta ("▲ +3.62 pp vs SPY") it took the
        // whole value row and the return itself was not shown at all.
        ret_->clear_delta();
        QStringList sub;
        if (m.excess_return)
            sub << tr("%1 vs %2").arg(pfmt::pp(*m.excess_return), bench);
        else if (m.benchmark_return)
            sub << tr("%1 %2").arg(bench, pfmt::pct(*m.benchmark_return));
        if (m.mwr)
            sub << tr("MWR %1 p.a.").arg(pfmt::pct(*m.mwr));
        else if (m.mwr_period)
            sub << tr("MWR %1").arg(pfmt::pct(*m.mwr_period));
        ret_->set_sub_label(sub.join(QStringLiteral(" · ")));
        QString tip = tr("Time-weighted return over %1 (%2): deposits, withdrawals and trades are treated as flows, "
                         "not as performance.")
                          .arg(ribbon_window_text(window_), span);
        if (m.annualized_return)
            tip += QLatin1Char('\n') + tr("Annualised: %1.").arg(pfmt::pct(*m.annualized_return));
        if (m.benchmark_return)
            tip += QLatin1Char('\n') + tr("%1 over the same period: %2.").arg(bench, pfmt::pct(*m.benchmark_return));
        if (m.mwr || m.mwr_period)
            tip += QLatin1Char('\n') + tr("Money-weighted return (IRR of your own flows): %1.")
                                           .arg(m.mwr ? tr("%1 p.a.").arg(pfmt::pct(*m.mwr)) : pfmt::pct(*m.mwr_period));
        if (!m.notes.isEmpty())
            tip += QStringLiteral("\n\n") + m.notes.join(QLatin1Char('\n'));
        ret_->set_tooltip_text(tip);
    } else {
        ret_->set_state(State::Normal);
        ret_->set_value(pfmt::dash());
        ret_->clear_delta();
        ret_->set_tone(Tone::Neutral);
        ret_->set_sub_label(tr("not enough history"));
        ret_->set_tooltip_text(m.notes.isEmpty() ? tr("No NAV history covers %1 yet.").arg(ribbon_window_text(window_))
                                                 : m.notes.join(QLatin1Char('\n')));
    }

    // NAV sparkline follows the window.
    const QVector<double> spark = ribbon_nav_spark(m.twr_series);
    if (spark.size() >= 2)
        nav_->set_sparkline(spark);
    else
        nav_->clear_sparkline();

    // ── VOLATILITY ───────────────────────────────────────────────────────────
    vol_->set_state(State::Normal);
    vol_->set_value(pfmt::opt_pct(m.volatility, 1, false));
    vol_->set_sub_label(!m.volatility ? tr("needs %1+ periods").arg(portfolio::kPerfMinObservations)
                        : m.volatility_is_ex_ante ? tr("ex-ante")
                                                  : tr("realised"));
    vol_->set_tooltip_text(m.volatility_is_ex_ante
                               ? tr("Annualised volatility of the CURRENT weights over %1 trailing daily returns "
                                    "(ex-ante) — the realised history is too short.")
                                     .arg(m.risk_observations)
                               : tr("Annualised volatility of the realised time-weighted returns, %1.").arg(span));

    // ── SHARPE ───────────────────────────────────────────────────────────────
    sharpe_->set_state(State::Normal);
    sharpe_->set_value(pfmt::opt_num(m.sharpe));
    sharpe_->set_tone(m.sharpe ? ribbon_tone(*m.sharpe) : Tone::Neutral);
    sharpe_->set_sub_label(m.risk_free_known ? tr("rf %1").arg(pfmt::fpct(m.risk_free_rate, 2, false))
                                             : tr("rf unknown · 0 %"));
    sharpe_->set_tooltip_text(
        tr("Sharpe ratio over %1: annualised excess return per unit of volatility.\nRisk-free rate: %2 (%3).")
            .arg(span, pfmt::fpct(m.risk_free_rate, 2, false),
                 m.risk_free_known ? m.risk_free_source
                                   : tr("no rate known for this currency — 0 % used; set an override in Settings")));
    if (!m.risk_free_known)
        sharpe_->set_tone(Tone::Warning);

    // ── BETA ─────────────────────────────────────────────────────────────────
    beta_->set_state(State::Normal);
    beta_->set_value(pfmt::opt_num(m.beta));
    beta_->set_sub_label(tr("vs %1").arg(bench));
    beta_->set_tooltip_text(m.volatility_is_ex_ante
                                ? tr("Beta of the current weights vs %1 (ex-ante, trailing daily returns).").arg(bench)
                                : tr("Beta of the realised returns vs %1, %2.").arg(bench, span));

    // ── MAX DRAWDOWN ─────────────────────────────────────────────────────────
    mdd_->set_state(State::Normal);
    mdd_->set_value(pfmt::opt_pct(m.max_drawdown, 1));
    mdd_->set_tone(m.max_drawdown && *m.max_drawdown < 0 ? Tone::Negative : Tone::Neutral);
    mdd_->set_sub_label(m.current_drawdown ? tr("now %1").arg(pfmt::pct(*m.current_drawdown, 1)) : QString());
    mdd_->set_tooltip_text(tr("Worst peak-to-trough fall of the time-weighted return index, %1. Deposits and "
                              "withdrawals are not drawdowns.")
                               .arg(span));
}

void PortfolioStatsRibbon::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PortfolioStatsRibbon::retranslateUi() {
    if (period_label_)
        period_label_->setText(tr("PERIOD"));
    nav_->set_label(tr("NAV"));
    day_->set_label(tr("TODAY"));
    upnl_->set_label(tr("UNREALISED P&L"));
    real_->set_label(tr("REALISED + INCOME"));
    cash_->set_label(tr("CASH"));
    vol_->set_label(tr("VOLATILITY"));
    sharpe_->set_label(tr("SHARPE"));
    beta_->set_label(tr("BETA"));
    mdd_->set_label(tr("MAX DRAWDOWN"));
    ret_->set_label(tr("RETURN %1").arg(window_label()));
    render_summary();
    render_metrics();
}

void PortfolioStatsRibbon::refresh_theme() {
    // Applied before build_ui() by the constructor; the same text again (the constructor's
    // own call, then PortfolioScreen's) re-styled the whole ribbon twice for nothing.
    const QString css = ribbon_css();
    if (styleSheet() != css)
        setStyleSheet(css);
    // Formatting depends on the locale / font; re-render (tiles repaint themselves).
    render_summary();
    render_metrics();
}

} // namespace fincept::screens
