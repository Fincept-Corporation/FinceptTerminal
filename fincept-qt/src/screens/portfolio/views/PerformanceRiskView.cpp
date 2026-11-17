// src/screens/portfolio/views/PerformanceRiskView.cpp
#include "screens/portfolio/views/PerformanceRiskView.h"

#include "screens/portfolio/PortfolioFormat.h"
#include "services/portfolio/PortfolioPerformance.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/charts/ChartExport.h"
#include "ui/charts/DistributionChart.h"
#include "ui/charts/MonthlyReturnsHeatmap.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDate>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

using PW = portfolio::PerfWindow;
const PW kPrWindows[] = {PW::MTD, PW::QTD, PW::YTD, PW::Y1, PW::Y3, PW::Y5, PW::All};

QString prv_window_name(PW w) {
    switch (w) {
        case PW::MTD:
            return PerformanceRiskView::tr("Month to date");
        case PW::QTD:
            return PerformanceRiskView::tr("Quarter to date");
        case PW::YTD:
            return PerformanceRiskView::tr("Year to date");
        case PW::Y1:
            return PerformanceRiskView::tr("1 year");
        case PW::Y3:
            return PerformanceRiskView::tr("3 years");
        case PW::Y5:
            return PerformanceRiskView::tr("5 years");
        case PW::All:
            break;
    }
    return PerformanceRiskView::tr("Since inception");
}

struct PrvPoint {
    double x;
    double v;
};

double prv_stdev(const double* r, int n, double* mean_out) {
    double m = 0;
    for (int i = 0; i < n; ++i)
        m += r[i];
    m /= n;
    double s = 0;
    for (int i = 0; i < n; ++i)
        s += (r[i] - m) * (r[i] - m);
    if (mean_out)
        *mean_out = m;
    return n > 1 ? std::sqrt(s / (n - 1)) : portfolio::kAbsent;
}

QTableWidgetItem* prv_num(const QString& text, double v, const QColor& fg = {}) {
    auto* it = new ui::NumericTableWidgetItem(text, std::isfinite(v) ? v : -1e300);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (fg.isValid())
        it->setForeground(fg);
    return it;
}

} // namespace

PerformanceRiskView::PerformanceRiskView(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::PortfolioService::instance();
    connect(&svc, &services::PortfolioService::metrics_computed, this,
            [this](const portfolio::ComputedMetrics& m) { set_metrics(m); });
    // Restyle and re-render once an appearance change has settled (never inside theme_changed).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        apply_styles();
        if (has_data_) {
            render_periods();
            render_notice();
        }
    });
    // Currency / number format changed: re-format the metrics already held (no recompute).
    ui::formatting::on_format_changed(this, [this]() {
        if (!has_data_)
            return;
        render_periods();
        render_tiles();
        render_distribution();
        render_chart();
        render_calendar();
        render_notice();
    });
}

void PerformanceRiskView::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Toolbar: window + chart mode + rolling + export ──────────────────────
    auto* bar = new QWidget(this);
    bar->setObjectName("prvBar");
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(10, 6, 10, 6);
    h->setSpacing(4);
    window_group_ = new QButtonGroup(this);
    window_group_->setExclusive(true);
    for (const PW w : kPrWindows) {
        auto* b = new QPushButton(portfolio::perf_window_key(w), bar);
        b->setCheckable(true);
        b->setObjectName("prvChip");
        b->setToolTip(prv_window_name(w));
        b->setCursor(Qt::PointingHandCursor);
        window_group_->addButton(b, static_cast<int>(w));
        h->addWidget(b);
        if (w == window_)
            b->setChecked(true);
    }
    connect(window_group_, &QButtonGroup::idClicked, this, [this](int id) {
        window_ = static_cast<PW>(id);
        request_windows(false);
        render_tiles();
        render_chart();
        render_distribution();
        render_notice();
    });
    h->addSpacing(14);
    return_btn_ = new QPushButton(tr("RETURN"), bar);
    nav_btn_ = new QPushButton(tr("NAV"), bar);
    for (auto* b : {return_btn_, nav_btn_}) {
        b->setCheckable(true);
        b->setObjectName("prvChip");
        b->setCursor(Qt::PointingHandCursor);
        h->addWidget(b);
    }
    return_btn_->setChecked(true);
    return_btn_->setToolTip(tr("Time-weighted return vs the benchmark, rebased to the start of the window"));
    nav_btn_->setToolTip(tr("Net asset value in the base currency (deposits / withdrawals included)"));
    connect(return_btn_, &QPushButton::clicked, this, [this]() {
        chart_mode_ = ChartMode::Return;
        return_btn_->setChecked(true);
        nav_btn_->setChecked(false);
        render_chart();
    });
    connect(nav_btn_, &QPushButton::clicked, this, [this]() {
        chart_mode_ = ChartMode::Nav;
        nav_btn_->setChecked(true);
        return_btn_->setChecked(false);
        render_chart();
    });
    h->addSpacing(14);
    auto* rl = new QLabel(tr("Rolling:"), bar);
    h->addWidget(rl);
    rolling_cb_ = new QComboBox(bar);
    rolling_cb_->addItem(tr("Volatility"), static_cast<int>(Rolling::Volatility));
    rolling_cb_->addItem(tr("Sharpe"), static_cast<int>(Rolling::Sharpe));
    rolling_cb_->addItem(tr("Beta"), static_cast<int>(Rolling::Beta));
    h->addWidget(rolling_cb_);
    rolling_len_cb_ = new QComboBox(bar);
    for (int n : {21, 63, 126, 252})
        rolling_len_cb_->addItem(tr("%1 obs").arg(n), n);
    rolling_len_cb_->setCurrentIndex(1);
    h->addWidget(rolling_len_cb_);
    connect(rolling_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { render_chart(); });
    connect(rolling_len_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { render_chart(); });
    h->addStretch(1);
    export_btn_ = new QPushButton(tr("EXPORT CSV"), bar);
    export_btn_->setObjectName("prvExport");
    export_btn_->setToolTip(tr("Every window's figures and the selected window's metrics"));
    connect(export_btn_, &QPushButton::clicked, this, &PerformanceRiskView::export_csv);
    h->addWidget(export_btn_);
    root->addWidget(bar);

    notice_ = new pfmt::PortfolioNoticeBar(this);
    connect(notice_->retry_button(), &QPushButton::clicked, this, [this]() { request_windows(true); });
    root->addWidget(notice_);

    basis_label_ = new QLabel(this);
    basis_label_->setObjectName("prvBasis");
    basis_label_->setWordWrap(true);
    basis_label_->setContentsMargins(10, 4, 10, 0);
    root->addWidget(basis_label_);

    kpis_ = new ui::KpiStrip(this);
    kpis_->set_min_tile_width(ui::metrics::width(132));
    const std::pair<const char*, QString> tiles[] = {
        {"twr", tr("TWR")},          {"mwr", tr("MWR")},           {"ann", tr("ANNUALISED")},
        {"bench", tr("BENCHMARK")},  {"excess", tr("EXCESS")},     {"vol", tr("VOLATILITY")},
        {"sharpe", tr("SHARPE")},    {"sortino", tr("SORTINO")},   {"mdd", tr("MAX DRAWDOWN")},
        {"curdd", tr("DRAWDOWN NOW")}, {"beta", tr("BETA")},       {"alpha", tr("ALPHA")},
        {"te", tr("TRACKING ERROR")}, {"ir", tr("INFO RATIO")},    {"upcap", tr("UP CAPTURE")},
        {"downcap", tr("DOWN CAPTURE")}, {"calmar", tr("CALMAR")}, {"win", tr("WIN RATE")},
        {"var", tr("VaR 95% 1D")},   {"cvar", tr("CVaR 95% 1D")},
    };
    for (const auto& t : tiles) {
        auto* tile = kpis_->add_tile(QString::fromLatin1(t.first), t.second);
        tile->set_compact(true);
        tile->set_state(ui::StatTile::State::Loading);
    }
    for (const char* k : {"vol", "mdd", "curdd", "te", "downcap", "var", "cvar"})
        kpis_->tile(QString::fromLatin1(k))->set_delta_inverted(true);
    auto* kpi_wrap = new QWidget(this);
    auto* kv = new QVBoxLayout(kpi_wrap);
    kv->setContentsMargins(10, 6, 10, 6);
    kv->addWidget(kpis_);
    root->addWidget(kpi_wrap);

    tabs_ = new QTabWidget(this);
    chart_ = new ui::TimeSeriesChart(tabs_);
    chart_->set_utc(true);
    chart_->set_show_mode_buttons(false);
    chart_->set_export_name(QStringLiteral("portfolio_performance_risk"));
    chart_->set_time_formatter([](double x) { return ui::formatting::bar_date(static_cast<qint64>(x / 1000.0)); });
    tabs_->addTab(chart_, tr("CHART"));

    periods_table_ = new QTableWidget(0, 11, tabs_);
    periods_table_->setHorizontalHeaderLabels({tr("PERIOD"), tr("FROM"), tr("TO"), tr("TWR"), tr("ANNUALISED"),
                                               tr("MWR"), tr("BENCHMARK"), tr("EXCESS"), tr("VOLATILITY"),
                                               tr("SHARPE"), tr("MAX DD")});
    periods_table_->verticalHeader()->setVisible(false);
    periods_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    periods_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    periods_table_->setShowGrid(false);
    periods_table_->horizontalHeader()->setStretchLastSection(true);
    ui::metrics::follow_row_height(periods_table_, 26);
    tabs_->addTab(periods_table_, tr("PERIODS"));

    heatmap_ = new ui::MonthlyReturnsHeatmap(tabs_);
    tabs_->addTab(heatmap_, tr("CALENDAR"));

    auto* dist_page = new QWidget(tabs_);
    auto* dv = new QVBoxLayout(dist_page);
    dv->setContentsMargins(0, 0, 0, 0);
    dist_ = new ui::DistributionChart(dist_page);
    dist_->set_signed_colors(true);
    dist_->set_normal_overlay(true);
    dist_->set_value_formatter([](double v) { return pfmt::pct(v, 2); });
    dv->addWidget(dist_, 1);
    dist_note_ = new QLabel(dist_page);
    dist_note_->setObjectName("prvBasis");
    dist_note_->setWordWrap(true);
    dist_note_->setContentsMargins(10, 4, 10, 6);
    dv->addWidget(dist_note_);
    tabs_->addTab(dist_page, tr("DISTRIBUTION"));

    root->addWidget(tabs_, 1);
    apply_styles();
}

void PerformanceRiskView::apply_styles() {
    // Set only when the text changed (each set re-polishes the whole view).
    const QString css = QString("#prvBar { background:%1; border-bottom:1px solid %2; }"
                          "#prvBar QLabel { color:%3; font-size:%4px; }"
                          "QPushButton#prvChip { background:transparent; color:%3; border:1px solid %2; padding:2px 8px;"
                          "  font-size:%4px; font-weight:700; }"
                          "QPushButton#prvChip:checked { color:%5; border-color:%5; background:%6; }"
                          "QPushButton#prvExport { background:transparent; color:%5; border:1px solid %5;"
                          "  padding:2px 10px; font-size:%4px; font-weight:700; }"
                          "QComboBox { background:%7; color:%8; border:1px solid %2; padding:1px 6px; font-size:%4px; }"
                          "#prvBasis { color:%3; font-size:%4px; }"
                          "QTableWidget { background:%9; color:%8; border:none; font-size:%10px; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY(),
                           ui::fonts::px(-3), ui::colors::AMBER(), ui::colors::ACCENT_BG(), ui::colors::BG_RAISED(),
                           ui::colors::TEXT_PRIMARY(), ui::colors::BG_BASE())
                      .arg(ui::fonts::px(-2));
    if (styleSheet() != css)
        setStyleSheet(css);
}

// ── Data in ──────────────────────────────────────────────────────────────────

void PerformanceRiskView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    const bool new_book = summary.portfolio.id != summary_.portfolio.id;
    summary_ = summary;
    currency_ = currency.isEmpty() ? pfmt::base_ccy(summary) : currency;
    has_data_ = true;
    if (new_book) {
        metrics_.clear();
        pending_.clear();
        request_windows(true);
    }
    render_notice();
}

void PerformanceRiskView::set_snapshots(const QVector<portfolio::PortfolioSnapshot>& snapshots) {
    snapshots_ = snapshots;
    std::sort(snapshots_.begin(), snapshots_.end(),
              [](const auto& a, const auto& b) { return a.snapshot_date < b.snapshot_date; });
    if (chart_mode_ == ChartMode::Nav)
        render_chart();
}

void PerformanceRiskView::set_metrics(const portfolio::ComputedMetrics& m) {
    if (!has_data_ || (!m.portfolio_id.isEmpty() && m.portfolio_id != summary_.portfolio.id))
        return;
    const PW w = portfolio::perf_window_from_key(m.window);
    const bool refresh_others = w == PW::All && metrics_.contains(static_cast<int>(PW::All)) &&
                                (metrics_.value(static_cast<int>(PW::All)).perf_end != m.perf_end ||
                                 metrics_.value(static_cast<int>(PW::All)).risk_free_rate != m.risk_free_rate ||
                                 metrics_.value(static_cast<int>(PW::All)).benchmark_symbol != m.benchmark_symbol);
    metrics_.insert(static_cast<int>(w), m);
    pending_.remove(static_cast<int>(w));
    if (refresh_others && isVisible())
        request_windows(true); // the data under every window moved
    render_periods();
    if (w == window_) {
        render_tiles();
        render_distribution();
    }
    if (w == PW::All) {
        render_chart();
        render_calendar();
    }
    render_notice();
}

void PerformanceRiskView::set_benchmark(const QString& symbol, const QStringList& dates, const QVector<double>& closes) {
    bench_symbol_ = symbol;
    bench_dates_ = dates;
    bench_closes_ = closes;
    render_chart();
}

void PerformanceRiskView::request_windows(bool all) {
    if (!has_data_ || summary_.portfolio.id.isEmpty())
        return;
    auto& svc = services::PortfolioService::instance();
    for (const PW w : kPrWindows) {
        if (!all && w != window_)
            continue;
        if (!all && metrics_.contains(static_cast<int>(w)))
            continue;
        pending_.insert(static_cast<int>(w), true);
        svc.compute_metrics(summary_, w);
    }
    render_notice();
}

void PerformanceRiskView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (has_data_ && metrics_.size() < static_cast<int>(std::size(kPrWindows)))
        request_windows(true);
}

const portfolio::ComputedMetrics* PerformanceRiskView::window_metrics() const {
    const auto it = metrics_.constFind(static_cast<int>(window_));
    return it == metrics_.constEnd() ? nullptr : &*it;
}

const portfolio::ComputedMetrics* PerformanceRiskView::all_metrics() const {
    const auto it = metrics_.constFind(static_cast<int>(PW::All));
    return it == metrics_.constEnd() ? nullptr : &*it;
}

// ── Rendering ────────────────────────────────────────────────────────────────

void PerformanceRiskView::render_notice() {
    if (!has_data_)
        return;
    QStringList lines;
    auto kind = pfmt::PortfolioNoticeBar::Kind::Hidden;
    const auto* m = window_metrics();
    if (!m && pending_.contains(static_cast<int>(window_))) {
        kind = pfmt::PortfolioNoticeBar::Kind::Loading;
        lines << tr("Computing %1 figures from the NAV history and the ledger…").arg(prv_window_name(window_));
    }
    if (!summary_.complete) {
        kind = pfmt::PortfolioNoticeBar::Kind::Warning;
        lines << pfmt::incomplete_line(summary_);
    }
    if (!summary_.fx_stale.isEmpty()) {
        kind = pfmt::PortfolioNoticeBar::Kind::Warning;
        lines << pfmt::fx_stale_line(summary_);
    }
    if (m) {
        if (!m->risk_free_known) {
            kind = pfmt::PortfolioNoticeBar::Kind::Warning;
            lines << tr("No risk-free rate is known for %1: 0 % is used for Sharpe, Sortino and alpha. Set an override "
                        "in Portfolio Settings.")
                         .arg(pfmt::base_ccy(summary_));
        }
        if (m->perf_suspect > 0) {
            kind = pfmt::PortfolioNoticeBar::Kind::Warning;
            lines << tr("%n period(s) moved more than 25 % — often an unrecorded deposit / withdrawal or a bad price; "
                        "they are kept in the figures.",
                        "", m->perf_suspect);
        }
        if (!m->notes.isEmpty() && kind == pfmt::PortfolioNoticeBar::Kind::Hidden)
            kind = pfmt::PortfolioNoticeBar::Kind::Info;
        lines << m->notes;
    }
    notice_->set_notice(lines.isEmpty() ? pfmt::PortfolioNoticeBar::Kind::Hidden : kind, lines.join(QLatin1Char('\n')),
                        false);

    if (m) {
        basis_label_->setText(
            tr("%1: %2 → %3 · %4 observations on the %5 calendar (%6 / year) · benchmark %7 · risk-free %8 (%9)")
                .arg(prv_window_name(window_), pfmt::date(m->perf_start), pfmt::date(m->perf_end))
                .arg(m->perf_observations)
                .arg(m->calendar.isEmpty() ? tr("trading") : m->calendar)
                .arg(QString::number(m->periods_per_year, 'f', 0))
                .arg(m->benchmark_symbol.isEmpty() ? pfmt::dash() : m->benchmark_symbol,
                     pfmt::fpct(m->risk_free_rate, 2, false), m->risk_free_known ? m->risk_free_source : tr("unknown")));
    } else {
        basis_label_->clear();
    }
}

void PerformanceRiskView::render_tiles() {
    const auto* m = window_metrics();
    using State = ui::StatTile::State;
    using Tone = ui::StatTile::Tone;
    auto tile = [this](const char* k) { return kpis_->tile(QString::fromLatin1(k)); };
    if (!m) {
        for (auto* t : kpis_->findChildren<ui::StatTile*>())
            t->set_state(pending_.contains(static_cast<int>(window_)) ? State::Loading : State::Normal);
        return;
    }
    const QString ccy = currency_;
    auto tone_of = [](const std::optional<double>& v) {
        if (!v || *v == 0)
            return Tone::Neutral;
        return *v > 0 ? Tone::Positive : Tone::Negative;
    };
    auto set = [&](const char* k, const QString& value, Tone tone, const QString& sub, const QString& tip) {
        auto* t = tile(k);
        t->set_state(State::Normal);
        t->set_value(value);
        t->set_tone(tone);
        t->set_sub_label(sub);
        t->set_tooltip_text(tip);
    };
    const QString bench = m->benchmark_symbol.isEmpty() ? tr("benchmark") : m->benchmark_symbol;
    const bool short_window = !m->annualized_return.has_value();

    set("twr", pfmt::opt_pct(m->total_return), tone_of(m->total_return), tr("time-weighted"),
        tr("Time-weighted return over the window: the manager's performance, independent of when money was added or "
           "withdrawn."));
    set("mwr", m->mwr ? pfmt::opt_pct(m->mwr) : pfmt::opt_pct(m->mwr_period), tone_of(m->mwr ? m->mwr : m->mwr_period),
        m->mwr ? tr("p.a., money-weighted") : tr("money-weighted"),
        tr("Money-weighted return (IRR of your own deposits / withdrawals and trades): the return YOU earned, timing "
           "of flows included. Annualised only for windows of a year or more."));
    set("ann", pfmt::opt_pct(m->annualized_return), tone_of(m->annualized_return),
        short_window ? tr("window < 1 year") : tr("p.a."),
        tr("Annualised TWR. Not shown for windows shorter than a year (annualising a short period misleads)."));
    set("bench", pfmt::opt_pct(m->benchmark_return), tone_of(m->benchmark_return), bench,
        tr("Benchmark total return over the same dates, in the base currency."));
    set("excess", m->excess_return ? pfmt::pp(*m->excess_return) : pfmt::dash(), tone_of(m->excess_return),
        tr("vs %1").arg(bench), tr("Portfolio TWR minus the benchmark's, same periods."));
    set("vol", pfmt::opt_pct(m->volatility, 1, false), Tone::Neutral,
        m->volatility_is_ex_ante ? tr("ex-ante") : tr("realised"),
        tr("Annualised volatility. Realised: %1. Ex-ante (current weights): %2.")
            .arg(pfmt::opt_pct(m->realised_volatility, 1, false), pfmt::opt_pct(m->ex_ante_volatility, 1, false)));
    set("sharpe", pfmt::opt_num(m->sharpe), tone_of(m->sharpe),
        m->risk_free_known ? tr("rf %1").arg(pfmt::fpct(m->risk_free_rate, 2, false)) : tr("rf unknown · 0 %"),
        tr("Annualised excess return over the risk-free rate per unit of volatility."));
    set("sortino", pfmt::opt_num(m->sortino), tone_of(m->sortino), tr("downside risk"),
        tr("Like Sharpe, but only returns below the risk-free rate count as risk."));
    set("mdd", pfmt::opt_pct(m->max_drawdown, 1), m->max_drawdown && *m->max_drawdown < 0 ? Tone::Negative : Tone::Neutral,
        QString(), tr("Worst peak-to-trough fall of the TWR index in the window."));
    set("curdd", pfmt::opt_pct(m->current_drawdown, 1),
        m->current_drawdown && *m->current_drawdown < 0 ? Tone::Negative : Tone::Neutral, tr("below the peak"),
        tr("Latest TWR index vs its running peak."));
    set("beta", pfmt::opt_num(m->beta), Tone::Neutral,
        m->volatility_is_ex_ante ? tr("ex-ante vs %1").arg(bench) : tr("vs %1").arg(bench),
        tr("Realised beta: %1 · ex-ante beta: %2.").arg(pfmt::opt_num(m->realised_beta), pfmt::opt_num(m->ex_ante_beta)));
    set("alpha", pfmt::opt_pct(m->alpha), tone_of(m->alpha), tr("Jensen, p.a."),
        tr("Annualised return not explained by beta to the benchmark (CAPM)."));
    set("te", pfmt::opt_pct(m->tracking_error, 1, false), Tone::Neutral,
        m->tracking_error_ex_ante ? tr("ex-ante") : tr("realised"),
        tr("Annualised standard deviation of the return relative to the benchmark."));
    set("ir", pfmt::opt_num(m->information_ratio), tone_of(m->information_ratio), QString(),
        tr("Annualised active return per unit of tracking error."));
    set("upcap", pfmt::opt_pct(m->up_capture, 0, false), Tone::Neutral, tr("benchmark-up periods"),
        tr("Share of the benchmark's gains captured in periods when it rose (>100 % = more than the market)."));
    set("downcap", pfmt::opt_pct(m->down_capture, 0, false), Tone::Neutral, tr("benchmark-down periods"),
        tr("Share of the benchmark's losses taken in periods when it fell (<100 % = lost less than the market)."));
    set("calmar", pfmt::opt_num(m->calmar), tone_of(m->calmar), QString(),
        tr("Annualised return divided by the maximum drawdown (windows of a year or more)."));
    set("win", pfmt::opt_pct(m->win_rate, 0, false), Tone::Neutral, tr("positive periods"),
        tr("Share of periods with a positive return."));
    set("var", pfmt::money(pfmt::opt_or_nan(m->var_95), ccy), Tone::Neutral,
        m->risk_ex_ante ? tr("ex-ante, current weights") : tr("historical"),
        tr("1-day loss not exceeded on 95 % of days, in %1.").arg(ccy));
    set("cvar", pfmt::money(pfmt::opt_or_nan(m->cvar_95), ccy), Tone::Neutral, tr("expected shortfall"),
        tr("Average loss on the worst 5 % of days, in %1.").arg(ccy));
}

void PerformanceRiskView::render_chart() {
    const auto* all = all_metrics();
    chart_->clear(); // series and annotations; the panes persist
    const QString ccy = currency_;

    // Panes: 0 = main, 1 = underwater, 2 = rolling (created on first use).
    if (chart_->pane_count() < 3) {
        chart_->add_pane(0.35);
        chart_->add_pane(0.35);
    }
    chart_->set_pane_title(1, tr("Underwater (drawdown from peak)"));

    if (!all || all->twr_series.size() < 2) {
        chart_->set_state(ui::TimeSeriesChart::State::Loading, tr("Waiting for the NAV history…"));
        return;
    }
    const auto& series = all->twr_series;

    // Main pane.
    if (chart_mode_ == ChartMode::Return) {
        chart_->set_value_mode(ui::TimeSeriesChart::ValueMode::Percent);
        QVector<QPointF> idx;
        for (const auto& p : series)
            idx.append({pfmt::utc_ms(p.date), 1.0 + p.cumulative});
        ui::TimeSeriesChart::SeriesSpec ts;
        ts.name = tr("Portfolio (TWR)");
        ts.points = idx;
        ts.color = ui::TimeSeriesChart::ColorRole::Accent;
        ts.line_width = 2.0;
        chart_->add_series(ts);
        if (!bench_dates_.isEmpty()) {
            QVector<QPointF> b;
            for (int i = 0; i < bench_dates_.size() && i < bench_closes_.size(); ++i)
                if (bench_closes_[i] > 0)
                    b.append({pfmt::utc_ms(bench_dates_[i]), bench_closes_[i]});
            ui::TimeSeriesChart::SeriesSpec bs;
            bs.name = bench_symbol_.isEmpty() ? tr("Benchmark") : bench_symbol_;
            bs.points = b;
            bs.color = ui::TimeSeriesChart::ColorRole::Muted;
            bs.dashed = true;
            chart_->add_series(bs);
        }
    } else {
        chart_->set_value_mode(ui::TimeSeriesChart::ValueMode::Absolute);
        QVector<QPointF> nav;
        for (const auto& s : snapshots_)
            if (s.total_value > 0)
                nav.append({pfmt::utc_ms(s.snapshot_date), s.total_value});
        if (nav.isEmpty())
            for (const auto& p : series)
                if (p.nav > 0)
                    nav.append({pfmt::utc_ms(p.date), p.nav});
        ui::TimeSeriesChart::SeriesSpec ns;
        ns.name = tr("NAV (%1)").arg(ccy);
        ns.points = nav;
        ns.style = ui::TimeSeriesChart::Style::Area;
        ns.color = ui::TimeSeriesChart::ColorRole::Accent;
        ns.rebase = false;
        ns.formatter = [ccy](double v) { return pfmt::money(v, ccy); };
        chart_->add_series(ns);
        chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [ccy](double v) { return pfmt::money_c(v, ccy); });
    }

    // Underwater pane from the TWR index.
    QVector<QPointF> dd;
    double peak = 0;
    for (const auto& p : series) {
        const double v = 1.0 + p.cumulative;
        peak = std::max(peak, v);
        dd.append({pfmt::utc_ms(p.date), peak > 0 ? (v / peak - 1.0) * 100.0 : portfolio::kAbsent});
    }
    ui::TimeSeriesChart::SeriesSpec ds;
    ds.name = tr("Drawdown");
    ds.points = dd;
    ds.pane = 1;
    ds.style = ui::TimeSeriesChart::Style::Area;
    ds.color = ui::TimeSeriesChart::ColorRole::Negative;
    ds.baseline = 0.0;
    ds.rebase = false;
    ds.formatter = [](double v) { return pfmt::pct(v, 2); };
    chart_->add_series(ds);
    chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return pfmt::pct(v, 0); }, 1);

    // Rolling pane.
    const int n = rolling_len_cb_->currentData().toInt();
    const auto kind = static_cast<Rolling>(rolling_cb_->currentData().toInt());
    const double ppy = all->periods_per_year > 0 ? all->periods_per_year : 252.0;
    const double rf_p = std::pow(1.0 + all->risk_free_rate, 1.0 / ppy) - 1.0;
    QVector<double> rets;
    QVector<double> xs;
    QVector<double> brets;
    // Benchmark level as-of lookup on the TWR dates.
    QMap<QString, double> blevel;
    for (int i = 0; i < bench_dates_.size() && i < bench_closes_.size(); ++i)
        blevel.insert(bench_dates_[i], bench_closes_[i]);
    auto bench_at = [&blevel](const QString& d) {
        auto it = blevel.upperBound(d);
        if (it == blevel.constBegin())
            return portfolio::kAbsent;
        --it;
        return it.value();
    };
    for (int i = 1; i < series.size(); ++i) {
        if (!series[i].valid)
            continue;
        rets.append(series[i].period_return);
        xs.append(pfmt::utc_ms(series[i].date));
        const double b0 = bench_at(series[i - 1].date);
        const double b1 = bench_at(series[i].date);
        brets.append(std::isfinite(b0) && std::isfinite(b1) && b0 > 0 ? b1 / b0 - 1.0 : portfolio::kAbsent);
    }
    QVector<QPointF> roll;
    for (int i = n - 1; i < rets.size(); ++i) {
        const double* r = rets.constData() + (i - n + 1);
        double mean = 0;
        const double sd = prv_stdev(r, n, &mean);
        double v = portfolio::kAbsent;
        if (kind == Rolling::Volatility) {
            v = std::isfinite(sd) ? sd * std::sqrt(ppy) * 100.0 : portfolio::kAbsent;
        } else if (kind == Rolling::Sharpe) {
            v = std::isfinite(sd) && sd > 0 ? (mean - rf_p) / sd * std::sqrt(ppy) : portfolio::kAbsent;
        } else {
            double mb = 0, mp = 0;
            int k = 0;
            for (int j = i - n + 1; j <= i; ++j)
                if (std::isfinite(brets[j])) {
                    mb += brets[j];
                    mp += rets[j];
                    ++k;
                }
            if (k >= n / 2) {
                mb /= k;
                mp /= k;
                double cov = 0, var = 0;
                for (int j = i - n + 1; j <= i; ++j)
                    if (std::isfinite(brets[j])) {
                        cov += (rets[j] - mp) * (brets[j] - mb);
                        var += (brets[j] - mb) * (brets[j] - mb);
                    }
                v = var > 0 ? cov / var : portfolio::kAbsent;
            }
        }
        roll.append({xs[i], v});
    }
    ui::TimeSeriesChart::SeriesSpec rs;
    rs.name = kind == Rolling::Volatility ? tr("Rolling volatility (%1 obs)").arg(n)
              : kind == Rolling::Sharpe   ? tr("Rolling Sharpe (%1 obs)").arg(n)
                                          : tr("Rolling beta vs %1 (%2 obs)").arg(bench_symbol_).arg(n);
    rs.points = roll;
    rs.pane = 2;
    rs.color = ui::TimeSeriesChart::ColorRole::Info;
    rs.rebase = false;
    if (kind == Rolling::Volatility)
        rs.formatter = [](double v) { return pfmt::upct(v, 1); };
    else
        rs.formatter = [](double v) { return pfmt::num(v, 2); };
    chart_->add_series(rs);
    chart_->set_pane_title(2, rs.name);
    if (kind == Rolling::Volatility)
        chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return pfmt::upct(v, 0); }, 2);
    else
        chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return pfmt::num(v, 1); }, 2);
    if (kind != Rolling::Volatility)
        chart_->add_hline(kind == Rolling::Beta ? 1.0 : 0.0, QString(), ui::TimeSeriesChart::ColorRole::Muted, 2);
    if (roll.isEmpty())
        chart_->set_pane_title(2, tr("%1 — needs more than %2 observations").arg(rs.name).arg(n));

    chart_->set_state(ui::TimeSeriesChart::State::Ready);
    // Show the selected window.
    const QDate end = pfmt::iso_date(series.last().date);
    const QDate base = portfolio::perf_window_base_date(window_, end);
    if (base.isValid())
        chart_->set_x_range(pfmt::utc_ms(base), pfmt::utc_ms(end));
    else
        chart_->reset_x_range();
}

void PerformanceRiskView::render_periods() {
    periods_table_->setSortingEnabled(false);
    periods_table_->setRowCount(static_cast<int>(std::size(kPrWindows)));
    int r = 0;
    for (const PW w : kPrWindows) {
        const auto it = metrics_.constFind(static_cast<int>(w));
        auto* name = new QTableWidgetItem(prv_window_name(w));
        name->setForeground(QColor(w == window_ ? ui::colors::AMBER() : ui::colors::TEXT_PRIMARY()));
        periods_table_->setItem(r, 0, name);
        if (it == metrics_.constEnd()) {
            for (int c = 1; c < periods_table_->columnCount(); ++c)
                periods_table_->setItem(r, c, prv_num(pending_.contains(static_cast<int>(w)) ? tr("…") : pfmt::dash(),
                                                      portfolio::kAbsent, QColor(ui::colors::TEXT_TERTIARY())));
            ++r;
            continue;
        }
        const auto& m = *it;
        periods_table_->setItem(r, 1, new QTableWidgetItem(pfmt::date(m.perf_start, ui::formatting::DateStyle::Short)));
        periods_table_->setItem(r, 2, new QTableWidgetItem(pfmt::date(m.perf_end, ui::formatting::DateStyle::Short)));
        auto pct_cell = [&](int c, const std::optional<double>& v, bool colour) {
            const double x = pfmt::opt_or_nan(v);
            periods_table_->setItem(r, c, prv_num(pfmt::pct(x), x, colour ? pfmt::dir_color(x) : QColor()));
        };
        pct_cell(3, m.total_return, true);
        pct_cell(4, m.annualized_return, true);
        pct_cell(5, m.mwr ? m.mwr : m.mwr_period, true);
        pct_cell(6, m.benchmark_return, true);
        const double ex = pfmt::opt_or_nan(m.excess_return);
        periods_table_->setItem(r, 7, prv_num(pfmt::pp(ex), ex, pfmt::dir_color(ex)));
        const double vol = pfmt::opt_or_nan(m.volatility);
        periods_table_->setItem(r, 8, prv_num(pfmt::upct(vol, 1), vol));
        const double sh = pfmt::opt_or_nan(m.sharpe);
        periods_table_->setItem(r, 9, prv_num(pfmt::num(sh, 2), sh));
        const double mdd = pfmt::opt_or_nan(m.max_drawdown);
        periods_table_->setItem(r, 10, prv_num(pfmt::pct(mdd, 1), mdd, pfmt::dir_color(mdd)));
        ++r;
    }
    periods_table_->resizeColumnsToContents();
}

void PerformanceRiskView::render_calendar() {
    const auto* all = all_metrics();
    if (!all)
        return;
    QVector<QPair<QDate, double>> daily;
    for (int i = 1; i < all->twr_series.size(); ++i) {
        const auto& p = all->twr_series[i];
        if (p.valid)
            daily.append({pfmt::iso_date(p.date), p.period_return * 100.0});
    }
    heatmap_->set_daily_returns(daily);
    heatmap_->set_state(daily.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready,
                        tr("No return history yet."));
}

void PerformanceRiskView::render_distribution() {
    const auto* m = window_metrics();
    if (!m) {
        dist_->set_state(ui::PaintedChart::Loading);
        return;
    }
    QVector<double> r;
    for (int i = 1; i < m->twr_series.size(); ++i)
        if (m->twr_series[i].valid)
            r.append(m->twr_series[i].period_return * 100.0);
    dist_->clear_markers();
    dist_->set_samples(r);
    if (dist_->sample_count() >= 2) {
        dist_->add_marker(dist_->mean(), tr("Mean"), ui::DistributionChart::MarkerRole::Mean);
        const double q05 = dist_->quantile(0.05);
        dist_->add_marker(q05, tr("5th pct"), ui::DistributionChart::MarkerRole::VaR);
        dist_->set_state(ui::PaintedChart::Ready);
    } else {
        dist_->set_state(ui::PaintedChart::Empty, tr("Not enough periods in this window."));
    }
    dist_note_->setText(tr("%n period return(s) of the time-weighted series in the window (%1). Best %2, worst %3. "
                           "The curve is a normal distribution with the same mean and standard deviation — fat tails "
                           "show where the bars exceed it.",
                           "", dist_->sample_count())
                            .arg(prv_window_name(window_), pfmt::opt_pct(m->best_period), pfmt::opt_pct(m->worst_period)));
}

void PerformanceRiskView::export_csv() {
    QVector<QStringList> rows;
    auto num = [](const std::optional<double>& v, int dp = 6) {
        return v ? ui::formatting::export_number(*v, dp) : QString();
    };
    for (const PW w : kPrWindows) {
        const auto it = metrics_.constFind(static_cast<int>(w));
        if (it == metrics_.constEnd())
            continue;
        const auto& m = *it;
        rows.append({m.window, m.perf_start, m.perf_end, num(m.total_return), num(m.annualized_return), num(m.mwr),
                     num(m.mwr_period), num(m.benchmark_return), num(m.excess_return), num(m.volatility),
                     num(m.sharpe), num(m.sortino), num(m.max_drawdown), num(m.beta), num(m.alpha),
                     num(m.tracking_error), num(m.information_ratio), num(m.up_capture), num(m.down_capture),
                     num(m.calmar), num(m.win_rate), num(m.var_95, 2), num(m.cvar_95, 2), m.benchmark_symbol,
                     ui::formatting::export_number(m.risk_free_rate, 6), m.risk_free_source});
    }
    if (rows.isEmpty())
        return;
    ui::ChartExport::save_csv({"window", "start", "end", "twr_pct", "annualised_pct", "mwr_pa_pct", "mwr_period_pct",
                               "benchmark_pct", "excess_pp", "volatility_pct", "sharpe", "sortino", "max_drawdown_pct",
                               "beta", "alpha_pct", "tracking_error_pct", "information_ratio", "up_capture_pct",
                               "down_capture_pct", "calmar", "win_rate_pct", "var95_1d_" + currency_,
                               "cvar95_1d_" + currency_, "benchmark", "risk_free_rate", "risk_free_source"},
                              rows, QStringLiteral("performance_%1").arg(QDate::currentDate().toString(Qt::ISODate)),
                              this);
}

void PerformanceRiskView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange && has_data_) {
        render_tiles();
        render_periods();
        render_notice();
    }
    QWidget::changeEvent(event);
}

} // namespace fincept::screens
