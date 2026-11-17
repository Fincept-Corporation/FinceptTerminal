// src/screens/portfolio/views/RiskManagementView.cpp
#include "screens/portfolio/views/RiskManagementView.h"

#include "screens/portfolio/PortfolioFormat.h"
#include "screens/portfolio/views/RiskWhatIfPanel.h"
#include "screens/portfolio/views/StressTestPanel.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/charts/BarChart.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

constexpr int kRmvValueRole = Qt::UserRole + 31;

QTableWidgetItem* rmv_num(const QString& text, double v, const QColor& fg = {}) {
    auto* it = new ui::NumericTableWidgetItem(text, std::isfinite(v) ? v : -1e300);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    it->setData(kRmvValueRole, v);
    if (fg.isValid())
        it->setForeground(fg);
    return it;
}

QTableWidget* rmv_table(const QStringList& headers, QWidget* parent) {
    auto* t = new QTableWidget(0, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->setVisible(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setShowGrid(false);
    t->setSortingEnabled(true);
    t->setWordWrap(false);
    t->horizontalHeader()->setStretchLastSection(true);
    ui::metrics::follow_row_height(t, 24);
    return t;
}

double rmv_opt(const std::optional<double>& v) {
    return v ? *v : portfolio::kAbsent;
}

} // namespace

RiskManagementView::RiskManagementView(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::PortfolioService::instance();
    connect(&svc, &services::PortfolioService::market_analytics_loaded, this,
            [this](const QString& pid, bool ok, const QString& error) {
                if (!has_data_ || pid != summary_.portfolio.id)
                    return;
                if (!ok) {
                    error_ = error;
                    requested_ = false;
                    render_notice();
                    whatif_->set_analytics(analytics_); // NOW cells leave "…" (the notice offers RETRY)
                    return;
                }
                error_.clear();
                const auto a = services::PortfolioService::instance().last_market_analytics(pid);
                const bool mine = matches_options(a);
                if (mine)
                    requested_ = false;
                // A run with other options (the screen's default refresh) only fills an empty view.
                if (mine || analytics_.portfolio_id.isEmpty()) {
                    analytics_ = a;
                    render();
                } else {
                    render_notice();
                }
            });
    // Table item colours and charts are data: re-render them once an appearance change has
    // settled (never inside theme_changed). The stylesheet itself is kept in step by the
    // theme manager's re-style pass (and the notice bar styles itself).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        if (!has_data_ || summary_.holdings.isEmpty())
            return;
        render_overview();
        render_contribution();
    });
    // Currency / number format changed: re-format the run already held (the STRESS and
    // WHAT-IF panels re-render themselves).
    ui::formatting::on_format_changed(this, [this]() {
        if (!has_data_ || summary_.holdings.isEmpty())
            return;
        render_notice();
        render_overview();
        render_contribution();
    });
}

void RiskManagementView::build_ui() {
    apply_styles(); // one selector stylesheet, before any child exists
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    body_ = new QStackedWidget(this);
    outer->addWidget(body_);
    auto* main = new QWidget(body_);
    auto* root = new QVBoxLayout(main);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* bar = new QWidget(main);
    bar->setObjectName("rmvBar");
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(10, 6, 10, 6);
    h->setSpacing(6);
    h->addWidget(new QLabel(tr("VaR confidence:"), bar));
    conf_cb_ = new QComboBox(bar);
    for (double c : {0.95, 0.99, 0.975, 0.90})
        conf_cb_->addItem(pfmt::upct(c * 100.0, c == 0.975 ? 1 : 0), c);
    h->addWidget(conf_cb_);
    h->addWidget(new QLabel(tr("Horizon:"), bar));
    horizon_cb_ = new QComboBox(bar);
    for (int d : {1, 5, 10, 20})
        horizon_cb_->addItem(tr("%n trading day(s)", "", d), d);
    h->addWidget(horizon_cb_);
    h->addWidget(new QLabel(tr("Monte Carlo:"), bar));
    paths_cb_ = new QComboBox(bar);
    paths_cb_->addItem(tr("10,000 paths"), 10000);
    paths_cb_->addItem(tr("50,000 paths"), 50000);
    paths_cb_->addItem(tr("Off"), 0);
    h->addWidget(paths_cb_);
    recalc_btn_ = new QPushButton(tr("RECALCULATE"), bar);
    recalc_btn_->setToolTip(tr("Download fresh prices and recompute with these settings"));
    h->addWidget(recalc_btn_);
    h->addStretch(1);
    root->addWidget(bar);
    connect(recalc_btn_, &QPushButton::clicked, this, [this]() { recalculate(true); });
    // A run downloads prices and simulates: only on RECALCULATE, not on every combo change.
    for (auto* cb : {conf_cb_, horizon_cb_, paths_cb_})
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
            whatif_->set_options(options());
            render_notice();
        });

    notice_ = new pfmt::PortfolioNoticeBar(main);
    connect(notice_->retry_button(), &QPushButton::clicked, this, [this]() { recalculate(true); });
    root->addWidget(notice_);

    tabs_ = new QTabWidget(main);

    // OVERVIEW
    auto* ov = new QWidget(tabs_);
    auto* ovl = new QVBoxLayout(ov);
    ovl->setContentsMargins(10, 8, 10, 8);
    kpis_ = new ui::KpiStrip(ov);
    kpis_->set_min_tile_width(ui::metrics::width(140));
    for (const auto& [k, l] :
         {std::pair{"vol", tr("VOLATILITY")}, std::pair{"beta", tr("BETA")}, std::pair{"te", tr("TRACKING ERROR")},
          std::pair{"corr", tr("BENCHMARK CORR.")}, std::pair{"div", tr("DIVERSIFICATION")},
          std::pair{"fx", tr("FX SHARE OF RISK")}, std::pair{"cov", tr("COVERAGE")}, std::pair{"obs", tr("OBSERVATIONS")}}) {
        auto* t = kpis_->add_tile(QString::fromLatin1(k), l);
        t->set_compact(true);
        t->set_state(ui::StatTile::State::Loading);
    }
    ovl->addWidget(kpis_);
    auto* vsplit = new QSplitter(Qt::Horizontal, ov);
    var_table_ = rmv_table({tr("CONFIDENCE"), tr("HORIZON"), tr("HISTORICAL"), tr("CVaR (HIST.)"), tr("PARAMETRIC"),
                            tr("MONTE CARLO"), tr("CVaR (MC)")},
                           vsplit);
    vsplit->addWidget(var_table_);
    var_chart_ = new ui::BarChart(vsplit);
    var_chart_->set_show_legend(true);
    vsplit->addWidget(var_chart_);
    vsplit->setStretchFactor(0, 3);
    vsplit->setStretchFactor(1, 2);
    ovl->addWidget(vsplit, 1);
    var_note_ = new QLabel(ov);
    var_note_->setObjectName("rmvNote");
    var_note_->setWordWrap(true);
    ovl->addWidget(var_note_);
    tabs_->addTab(ov, tr("OVERVIEW"));

    // CONTRIBUTION
    auto* cp = new QSplitter(Qt::Vertical, tabs_);
    auto* ctop = new QSplitter(Qt::Horizontal, cp);
    contrib_chart_ = new ui::BarChart(ctop);
    contrib_chart_->set_orientation(Qt::Horizontal);
    contrib_chart_->set_show_legend(true);
    contrib_chart_->set_value_formatter([](double v) { return pfmt::upct(v, 1); });
    ctop->addWidget(contrib_chart_);
    fx_table_ = rmv_table({tr("CURRENCY"), tr("WEIGHT"), tr("FX VOL"), tr("POSITIONS")}, ctop);
    ctop->addWidget(fx_table_);
    ctop->setStretchFactor(0, 3);
    ctop->setStretchFactor(1, 1);
    cp->addWidget(ctop);
    auto* cbottom = new QWidget(cp);
    auto* cbl = new QVBoxLayout(cbottom);
    cbl->setContentsMargins(0, 0, 0, 0);
    contrib_table_ = rmv_table({tr("SYMBOL"), tr("WEIGHT"), tr("VOL"), tr("BETA"), tr("RISK SHARE"),
                                tr("MARGINAL VaR"), tr("COMPONENT VaR"), tr("COMPONENT VaR (%1)").arg(QString()),
                                tr("FX VOL"), tr("OBS")},
                               cbottom);
    for (int c : {1, 4}) {
        auto* d = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Bar, contrib_table_);
        d->set_value_role(kRmvValueRole);
        contrib_table_->setItemDelegateForColumn(c, d);
    }
    cbl->addWidget(contrib_table_, 1);
    contrib_note_ = new QLabel(cbottom);
    contrib_note_->setObjectName("rmvNote");
    contrib_note_->setWordWrap(true);
    contrib_note_->setContentsMargins(10, 4, 10, 4);
    cbl->addWidget(contrib_note_);
    cp->addWidget(cbottom);
    tabs_->addTab(cp, tr("CONTRIBUTION"));

    stress_ = new StressTestPanel(tabs_);
    tabs_->addTab(stress_, tr("STRESS"));
    whatif_ = new RiskWhatIfPanel(tabs_);
    whatif_->set_options(options());
    connect(whatif_, &RiskWhatIfPanel::add_position_requested, this, &RiskManagementView::add_position_requested);
    tabs_->addTab(whatif_, tr("WHAT-IF"));

    root->addWidget(tabs_, 1);
    body_->addWidget(main);

    // An empty book has no risk to measure: say so (with the action that fixes it)
    // instead of a "downloading…" notice that never ends.
    empty_ = ui::EmptyState::create(
        ui::EmptyState::no_data_spec(tr("Add a position first"),
                                     tr("Volatility, VaR, risk contributions, stress tests and what-if trades are "
                                        "measured on the holdings of the book. Buy a holding to start."),
                                     tr("BUY")),
        body_);
    connect(empty_, &ui::EmptyState::primary_clicked, this, &RiskManagementView::add_position_requested);
    body_->addWidget(empty_);
}

void RiskManagementView::apply_styles() {
    setStyleSheet(QString("#rmvBar { background:%1; border-bottom:1px solid %2; }"
                          "#rmvBar QLabel, #rmvNote { color:%3; font-size:%4px; }"
                          "QComboBox { background:%5; color:%6; border:1px solid %2; padding:2px 6px; font-size:%4px; }"
                          "#rmvBar QPushButton { background:transparent; color:%7; border:1px solid %7; padding:3px 10px;"
                          "  font-size:%4px; font-weight:700; }"
                          "QTableWidget { background:%8; color:%6; border:none; font-size:%9px; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY(),
                           ui::fonts::px(-3), ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::AMBER(),
                           ui::colors::BG_BASE(), ui::fonts::px(-2)));
}

portfolio::RiskOptions RiskManagementView::options() const {
    portfolio::RiskOptions o;
    const double c = conf_cb_->currentData().toDouble();
    o.confidences = {c};
    for (double extra : {0.95, 0.99})
        if (std::abs(extra - c) > 1e-9)
            o.confidences.append(extra);
    o.horizon_days = horizon_cb_->currentData().toInt();
    o.mc_paths = paths_cb_->currentData().toInt();
    return o;
}

bool RiskManagementView::matches_options(const portfolio::MarketAnalytics& a) const {
    if (a.portfolio_id.isEmpty())
        return false;
    const auto o = options();
    return a.var_horizon_days == o.horizon_days && std::abs(a.var_confidence - o.confidences.first()) < 1e-6 &&
           (o.mc_paths == 0 || a.mc_paths > 0);
}

void RiskManagementView::recalculate(bool force) {
    if (!has_data_ || summary_.holdings.isEmpty())
        return;
    requested_ = true;
    error_.clear();
    render_notice();
    services::PortfolioService::instance().fetch_market_analytics(summary_, force, options());
}

void RiskManagementView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    const bool new_book = summary.portfolio.id != summary_.portfolio.id;
    summary_ = summary;
    currency_ = currency.isEmpty() ? pfmt::base_ccy(summary) : currency;
    has_data_ = true;
    if (new_book) {
        analytics_ = {};
        error_.clear();
        const auto last = services::PortfolioService::instance().last_market_analytics(summary.portfolio.id);
        if (!last.portfolio_id.isEmpty())
            analytics_ = last;
    }
    body_->setCurrentIndex(summary_.holdings.isEmpty() ? 1 : 0);
    stress_->set_data(summary);
    whatif_->set_data(summary);
    render();
}

void RiskManagementView::set_metrics(const portfolio::ComputedMetrics& metrics) {
    if (!metrics.portfolio_id.isEmpty() && metrics.portfolio_id != summary_.portfolio.id)
        return;
    metrics_ = metrics;
}

void RiskManagementView::show_stress_tab() {
    if (tabs_ && stress_)
        tabs_->setCurrentWidget(stress_);
}

void RiskManagementView::set_analytics(const portfolio::MarketAnalytics& analytics) {
    if (analytics.portfolio_id.isEmpty() || (has_data_ && analytics.portfolio_id != summary_.portfolio.id))
        return;
    // A default-options run arriving while the user looks at other settings does not
    // replace their result (it is still used until theirs lands).
    if (!matches_options(analytics) && !analytics_.portfolio_id.isEmpty() && matches_options(analytics_))
        return;
    analytics_ = analytics;
    stress_->set_analytics(analytics);
    whatif_->set_analytics(analytics);
    if (has_data_)
        render();
}

void RiskManagementView::render() {
    render_notice();
    render_overview();
    render_contribution();
    stress_->set_analytics(analytics_);
    // WHAT-IF's NOW column comes from the same run (no script of its own).
    whatif_->set_analytics(analytics_);
}

void RiskManagementView::render_notice() {
    if (summary_.holdings.isEmpty()) {
        notice_->clear(); // the empty-book page says it; nothing is downloading
        recalc_btn_->setEnabled(false);
        return;
    }
    QStringList lines;
    auto kind = pfmt::PortfolioNoticeBar::Kind::Hidden;
    bool retry = false;
    const bool have = !analytics_.portfolio_id.isEmpty();
    if (!error_.isEmpty()) {
        kind = pfmt::PortfolioNoticeBar::Kind::Error;
        retry = true;
        lines << (have ? tr("The latest risk run failed — showing the previous result. %1").arg(error_)
                       : tr("Price history for the risk figures could not be downloaded: %1").arg(error_));
    } else if (requested_ || !have) {
        kind = pfmt::PortfolioNoticeBar::Kind::Loading;
        lines << tr("Downloading daily prices and computing risk (VaR, contributions, Monte Carlo)…");
    }
    if (have && !matches_options(analytics_) && !requested_)
        lines << tr("Showing the result for %1 / %2 day(s) — press RECALCULATE for the selected settings.")
                     .arg(pfmt::upct(analytics_.var_confidence * 100.0, 1))
                     .arg(analytics_.var_horizon_days);
    if (!summary_.complete) {
        if (kind == pfmt::PortfolioNoticeBar::Kind::Hidden)
            kind = pfmt::PortfolioNoticeBar::Kind::Warning;
        lines << pfmt::incomplete_line(summary_);
    }
    if (!summary_.fx_stale.isEmpty()) {
        if (kind == pfmt::PortfolioNoticeBar::Kind::Hidden)
            kind = pfmt::PortfolioNoticeBar::Kind::Warning;
        lines << pfmt::fx_stale_line(summary_);
    }
    if (have && !analytics_.missing.isEmpty()) {
        if (kind == pfmt::PortfolioNoticeBar::Kind::Hidden)
            kind = pfmt::PortfolioNoticeBar::Kind::Warning;
        lines << tr("No price history (left out, coverage %1): %2")
                     .arg(pfmt::fpct(analytics_.coverage, 0, false), analytics_.missing.join(QStringLiteral(", ")));
    }
    if (have && !analytics_.warnings.isEmpty())
        lines << analytics_.warnings;
    if (kind == pfmt::PortfolioNoticeBar::Kind::Hidden && !lines.isEmpty())
        kind = pfmt::PortfolioNoticeBar::Kind::Info;
    notice_->set_notice(kind, lines.join(QLatin1Char('\n')), retry);
    recalc_btn_->setEnabled(!requested_);
}

void RiskManagementView::render_overview() {
    using State = ui::StatTile::State;
    const bool have = !analytics_.portfolio_id.isEmpty();
    const QString ccy = currency_;
    const double nav = pfmt::holdings_value(summary_);
    auto tile = [this](const char* k) { return kpis_->tile(QString::fromLatin1(k)); };
    auto set = [&](const char* k, const QString& v, const QString& sub, const QString& tip) {
        auto* t = tile(k);
        t->set_state(have ? State::Normal : (error_.isEmpty() ? State::Loading : State::Error), error_);
        t->set_value(v);
        t->set_sub_label(sub);
        t->set_tooltip_text(tip);
    };
    const QString bench = analytics_.benchmark.isEmpty() ? tr("benchmark") : analytics_.benchmark;
    set("vol", pfmt::opt_fpct(analytics_.ann_vol, 1, false), tr("annualised, ex-ante"),
        tr("Volatility of the CURRENT weights over the trailing daily returns, in %1 (currency moves included).").arg(ccy));
    set("beta", pfmt::opt_num(analytics_.beta), tr("vs %1").arg(bench), tr("Ex-ante beta of the current weights."));
    set("te", pfmt::opt_fpct(analytics_.tracking_error, 1, false), tr("vs %1").arg(bench),
        tr("Annualised standard deviation of the return relative to the benchmark."));
    set("corr", pfmt::opt_num(analytics_.benchmark_corr), tr("daily returns"),
        tr("Correlation of the book's daily returns with the benchmark's."));
    set("div", pfmt::opt_num(analytics_.diversification_ratio), tr("Σ wσ / σp"),
        tr("Diversification ratio: above 1 means correlation between holdings cancels part of their risk."));
    set("fx", pfmt::opt_fpct(analytics_.fx_vol_contribution, 0, false), tr("of variance from FX"),
        tr("Share of the book's variance that disappears without currency moves against %1.").arg(ccy));
    set("cov", have ? pfmt::fpct(analytics_.coverage, 0, false) : pfmt::dash(), tr("of weight with history"),
        tr("Share of the book's weight that had price history for the run."));
    set("obs", have ? QString::number(analytics_.observations) : pfmt::dash(),
        have ? tr("to %1").arg(pfmt::date(analytics_.as_of, ui::formatting::DateStyle::Short)) : QString(),
        tr("Daily observations behind the ex-ante figures (%1 per year).").arg(QString::number(analytics_.periods_per_year, 'f', 0)));

    // VaR table + bars.
    var_table_->setSortingEnabled(false);
    var_table_->setRowCount(static_cast<int>(analytics_.var.size()));
    QStringList cats = {tr("Historical"), tr("CVaR (hist.)"), tr("Parametric"), tr("Monte Carlo"), tr("CVaR (MC)")};
    QVector<ui::BarSeries> series;
    int ci = 0;
    for (int r = 0; r < analytics_.var.size(); ++r) {
        const auto& v = analytics_.var[r];
        var_table_->setItem(r, 0, rmv_num(pfmt::upct(v.confidence * 100.0, v.confidence == 0.975 ? 1 : 0), v.confidence));
        var_table_->setItem(r, 1, rmv_num(tr("%n day(s)", "", v.horizon_days) +
                                              (v.horizon_scaled ? QStringLiteral(" √h") : QString()),
                                          v.horizon_days));
        auto cell = [&](int c, const std::optional<double>& f) {
            const double x = rmv_opt(f);
            auto* it = rmv_num(std::isfinite(x) ? QStringLiteral("%1  ·  %2").arg(pfmt::fpct(x, 2, false),
                                                                                  pfmt::money_c(x * nav, ccy))
                                                : pfmt::dash(),
                               x, std::isfinite(x) ? QColor(ui::colors::NEGATIVE()) : QColor());
            var_table_->setItem(r, c, it);
        };
        cell(2, v.historical);
        cell(3, v.cvar);
        cell(4, v.parametric);
        cell(5, v.monte_carlo);
        cell(6, v.monte_carlo_cvar);
        auto money = [&](const std::optional<double>& f) { return f ? *f * nav : portfolio::kAbsent; };
        series.append(ui::BarSeries{tr("%1 · %2d").arg(pfmt::upct(v.confidence * 100.0, 1)).arg(v.horizon_days),
                                    {money(v.historical), money(v.cvar), money(v.parametric), money(v.monte_carlo),
                                     money(v.monte_carlo_cvar)},
                                    ui::colors::chart_color(ci++)});
    }
    var_table_->setSortingEnabled(true);
    var_table_->resizeColumnsToContents();
    var_chart_->set_categories(cats);
    var_chart_->set_series(series);
    var_chart_->set_value_formatter([ccy](double v) { return pfmt::money_c(v, ccy); });
    var_chart_->set_state(series.isEmpty() ? ui::PaintedChart::Loading : ui::PaintedChart::Ready);
    var_note_->setText(have ? tr("Losses as a share of the valued book (%1) and in %2. Historical = empirical quantile "
                                 "of overlapping %3-day returns (√h = scaled from 1-day: too few windows); parametric = "
                                 "normal; Monte Carlo = %4 simulated paths of the holdings. CVaR = average loss beyond "
                                 "the VaR.")
                                  .arg(pfmt::money(nav, ccy), ccy)
                                  .arg(analytics_.var_horizon_days)
                                  .arg(analytics_.mc_paths > 0 ? ui::formatting::number(analytics_.mc_paths, 0)
                                                               : tr("no"))
                            : QString());
}

void RiskManagementView::render_contribution() {
    const QString ccy = currency_;
    const double nav = pfmt::holdings_value(summary_);
    const bool have = !analytics_.portfolio_id.isEmpty();
    contrib_table_->setHorizontalHeaderItem(7, new QTableWidgetItem(tr("COMPONENT VaR (%1)").arg(ccy)));
    QVector<const portfolio::HoldingWithQuote*> rows;
    for (const auto& h : summary_.holdings)
        if (h.valued())
            rows.append(&h);
    std::sort(rows.begin(), rows.end(), [this](const auto* a, const auto* b) {
        const auto ra = analytics_.holdings.value(a->symbol).risk_contribution.value_or(-1);
        const auto rb = analytics_.holdings.value(b->symbol).risk_contribution.value_or(-1);
        return ra > rb;
    });
    contrib_table_->setSortingEnabled(false);
    contrib_table_->setRowCount(static_cast<int>(rows.size()));
    QStringList cats;
    QVector<double> wv, rv;
    for (int r = 0; r < rows.size(); ++r) {
        const auto& h = *rows[r];
        const auto hr = analytics_.holdings.value(h.symbol);
        contrib_table_->setItem(r, 0, new QTableWidgetItem(h.symbol));
        contrib_table_->setItem(r, 1, rmv_num(pfmt::upct(h.weight, 1), h.weight));
        const double vol = hr.ann_vol ? *hr.ann_vol * 100.0 : portfolio::kAbsent;
        contrib_table_->setItem(r, 2, rmv_num(pfmt::upct(vol, 1), vol));
        const double beta = rmv_opt(hr.beta);
        contrib_table_->setItem(r, 3, rmv_num(pfmt::num(beta, 2), beta));
        const double share = hr.risk_contribution ? *hr.risk_contribution * 100.0 : portfolio::kAbsent;
        // More risk than weight is worth flagging.
        const QColor share_fg = std::isfinite(share) && share > h.weight * 1.5 ? QColor(ui::colors::WARNING()) : QColor();
        contrib_table_->setItem(r, 4, rmv_num(pfmt::upct(share, 1), share, share_fg));
        const double mvar = hr.marginal_var ? *hr.marginal_var * 100.0 : portfolio::kAbsent;
        contrib_table_->setItem(r, 5, rmv_num(pfmt::pct(mvar, 2), mvar));
        const double cvar = hr.component_var ? *hr.component_var * 100.0 : portfolio::kAbsent;
        contrib_table_->setItem(r, 6, rmv_num(pfmt::pct(cvar, 2), cvar));
        const double cvar_m = hr.component_var ? *hr.component_var * nav : portfolio::kAbsent;
        contrib_table_->setItem(r, 7, rmv_num(pfmt::money(cvar_m, ccy), cvar_m));
        const double fxv = hr.fx_vol ? *hr.fx_vol * 100.0 : portfolio::kAbsent;
        contrib_table_->setItem(r, 8, rmv_num(pfmt::upct(fxv, 1), fxv));
        contrib_table_->setItem(r, 9, rmv_num(QString::number(hr.observations), hr.observations));
        cats << h.symbol;
        wv << h.weight;
        rv << share;
    }
    contrib_table_->setSortingEnabled(true);
    contrib_table_->resizeColumnsToContents();
    contrib_chart_->set_categories(cats);
    contrib_chart_->set_series({ui::BarSeries{tr("Weight"), wv, ui::colors::chart_color(0)},
                                ui::BarSeries{tr("Risk share"), rv, ui::colors::chart_color(1)}});
    contrib_chart_->set_state(!have ? ui::PaintedChart::Loading
                              : cats.isEmpty() ? ui::PaintedChart::Empty
                                               : ui::PaintedChart::Ready);

    // Currency risk.
    struct Ccy {
        double weight = 0;
        double fx_w = 0; // weight-weighted fx vol
        int n = 0;
    };
    QMap<QString, Ccy> by;
    for (const auto* h : rows) {
        auto& c = by[h->currency.isEmpty() ? pfmt::base_ccy(summary_) : h->currency];
        c.weight += h->weight;
        const auto fx = analytics_.holdings.value(h->symbol).fx_vol;
        if (fx)
            c.fx_w += *fx * h->weight;
        ++c.n;
    }
    fx_table_->setSortingEnabled(false);
    fx_table_->setRowCount(static_cast<int>(by.size()));
    int r = 0;
    for (auto it = by.constBegin(); it != by.constEnd(); ++it, ++r) {
        fx_table_->setItem(r, 0, new QTableWidgetItem(it.key()));
        fx_table_->setItem(r, 1, rmv_num(pfmt::upct(it->weight, 1), it->weight));
        const double fxv = it->weight > 0 && have ? it->fx_w / it->weight * 100.0 : portfolio::kAbsent;
        fx_table_->setItem(r, 2, rmv_num(it.key() == pfmt::base_ccy(summary_) ? pfmt::upct(0, 1) : pfmt::upct(fxv, 1), fxv));
        fx_table_->setItem(r, 3, rmv_num(QString::number(it->n), it->n));
    }
    fx_table_->setSortingEnabled(true);
    fx_table_->sortByColumn(1, Qt::DescendingOrder);
    fx_table_->resizeColumnsToContents();
    contrib_note_->setText(
        have ? tr("Risk share = part of the portfolio variance each holding carries (sums to 100 %); a share well above "
                  "the weight (amber) is a concentration of risk. Marginal VaR = change in VaR per unit of weight; "
                  "component VaR = weight × marginal (sums to the parametric VaR, %1 confidence, %2-day). FX vol = "
                  "annualised volatility of the holding's currency against %3; currencies account for %4 of the "
                  "variance.")
                   .arg(pfmt::upct(analytics_.var_confidence * 100.0, 1))
                   .arg(analytics_.var_horizon_days)
                   .arg(ccy, pfmt::opt_fpct(analytics_.fx_vol_contribution, 0, false))
             : QString());
}

void RiskManagementView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange && has_data_)
        render();
    QWidget::changeEvent(event);
}

} // namespace fincept::screens
