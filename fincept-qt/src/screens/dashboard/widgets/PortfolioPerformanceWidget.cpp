#include "screens/dashboard/widgets/PortfolioPerformanceWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/portfolio/PortfolioFormat.h"
#include "services/markets/MarketDataService.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimeZone>
#include <QTimer>

#include <cmath>

namespace fincept::screens::widgets {

namespace {

namespace pp_fmt = fincept::ui::formatting;
using fincept::services::PortfolioService;

double pp_ms(const QDate& d) {
    return double(QDateTime(d, QTime(0, 0), QTimeZone::UTC).toMSecsSinceEpoch());
}

// `approx`: the series converted some flows at today's FX rate — "≈" in the
// figure itself (never by colour alone), the reason in the tooltip.
void pp_set_return(ui::StatTile* t, double pct, const QString& tip, bool approx = false) {
    if (!t)
        return;
    t->set_tooltip_text(tip);
    if (!std::isfinite(pct)) {
        t->set_value(pp_fmt::placeholder());
        t->set_tone(ui::StatTile::Tone::Neutral);
        return;
    }
    t->set_value(approx ? QStringLiteral("≈ ") + pp_fmt::pct(pct) : pp_fmt::pct(pct));
    t->set_tone(pct > 0 ? ui::StatTile::Tone::Positive : pct < 0 ? ui::StatTile::Tone::Negative
                                                               : ui::StatTile::Tone::Neutral);
}

} // namespace

PortfolioPerformanceWidget::PortfolioPerformanceWidget(const QJsonObject& cfg, QWidget* parent)
    : PortfolioTileBase(tr("PORTFOLIO PERFORMANCE"), parent, ui::colors::SUCCESS()) {
    kpis_ = new ui::KpiStrip(this);
    kpis_->set_spacing(4);
    kpis_->add_tile("value", tr("VALUE"))->set_compact(true);
    kpis_->add_tile("today", tr("TODAY"))->set_compact(true);
    kpis_->add_tile("mtd", tr("MTD"))->set_compact(true);
    kpis_->add_tile("ytd", tr("YTD"))->set_compact(true);
    kpis_->add_tile("1y", tr("1Y"))->set_compact(true);
    kpis_->add_tile("all", tr("SINCE START"))->set_compact(true);
    content_layout()->addWidget(kpis_);

    chart_ = new ui::TimeSeriesChart(this);
    chart_->set_value_mode(ui::TimeSeriesChart::ValueMode::Percent);
    chart_->set_show_mode_buttons(false);
    chart_->set_range_presets({"1M", "3M", "YTD", "1Y", "ALL"});
    chart_->set_export_name(QStringLiteral("portfolio_performance"));
    chart_->set_state(ui::TimeSeriesChart::State::Loading);
    chart_->setMinimumHeight(80);
    content_layout()->addWidget(chart_, 1);

    auto* foot = new QWidget(this);
    auto* fl = new QHBoxLayout(foot);
    fl->setContentsMargins(6, 0, 6, 2);
    footer_ = new QLabel(foot);
    footer_->setWordWrap(true);
    fl->addWidget(footer_, 1);
    backfill_btn_ = new QPushButton(tr("Build 1Y history"), foot);
    backfill_btn_->setToolTip(tr("Rebuild one year of daily portfolio values from market prices for the current "
                                 "holdings (written to the portfolio's history)"));
    backfill_btn_->hide();
    fl->addWidget(backfill_btn_);
    content_layout()->addWidget(foot);

    connect(backfill_btn_, &QPushButton::clicked, this, [this]() {
        if (!has_portfolio() || backfilling_)
            return;
        backfilling_ = true;
        backfill_btn_->setEnabled(false);
        footer_->setText(tr("Building history from market prices…"));
        PortfolioService::instance().backfill_history(portfolio_id(), QStringLiteral("1y"));
    });

    auto& svc = PortfolioService::instance();
    connect(&svc, &PortfolioService::summary_loaded, this, [this](const portfolio::PortfolioSummary& s) {
        if (has_portfolio() && s.portfolio.id == portfolio_id())
            on_summary(s);
    });
    connect(&svc, &PortfolioService::summary_error, this, [this](const QString& pid, const QString& err) {
        if (pid != portfolio_id())
            return;
        if (auto* t = kpis_->tile("value"))
            t->set_state(ui::StatTile::State::Error, err);
        set_loading(false);
    });
    connect(&svc, &PortfolioService::snapshots_loaded, this,
            [this](const QString& pid, const QVector<portfolio::PortfolioSnapshot>& snaps) {
                if (pid == portfolio_id())
                    on_snapshots(snaps);
            });
    connect(&svc, &PortfolioService::history_backfilled, this, [this](const QString& pid, int) {
        if (pid != portfolio_id())
            return;
        backfilling_ = false;
        backfill_btn_->setEnabled(true);
        PortfolioService::instance().load_snapshots(portfolio_id(), 400);
    });
    for (auto sig : {&PortfolioService::asset_added, &PortfolioService::asset_sold, &PortfolioService::ledger_changed})
        connect(&svc, sig, this, [this](const QString& pid) {
            if (pid == portfolio_id() && isVisible())
                request_all();
        });

    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(60'000);
    connect(refresh_timer_, &QTimer::timeout, this, [this]() {
        if (has_portfolio())
            PortfolioService::instance().load_summary(portfolio_id());
    });
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        if (!has_portfolio())
            return;
        PortfolioService::instance().refresh_summary(portfolio_id());
        PortfolioService::instance().load_snapshots(portfolio_id(), 400);
        if (!bench_topic_.isEmpty())
            datahub::DataHub::instance().request(bench_topic_, true);
    });

    // Settings > Currency & Number Format: re-format what is held, no fetch.
    pp_fmt::on_format_changed(this, [this]() {
        render_summary();
        if (snapshots_loaded_)
            render_returns();
    });

    on_theme_changed();
    set_loading(true);
    apply_config(cfg);
}

QString PortfolioPerformanceWidget::benchmark_symbol() const {
    return PortfolioService::default_benchmark_for_currency(base_currency());
}

void PortfolioPerformanceWidget::on_portfolio_selected() {
    twr_.clear();
    twr_approx_.clear();
    summary_ = portfolio::PortfolioSummary{};
    have_summary_ = false;
    bench_.clear();
    snapshot_count_ = 0;
    snapshots_loaded_ = false;
    for (const char* k : {"value", "today", "mtd", "ytd", "1y", "all"})
        if (auto* t = kpis_->tile(QString::fromLatin1(k))) {
            t->set_value(pp_fmt::placeholder());
            t->clear_delta();
            t->set_state(ui::StatTile::State::Normal);
        }
    chart_->clear();
    if (!has_portfolio()) {
        set_loading(false);
        chart_->set_state(ui::TimeSeriesChart::State::Empty, tr("No portfolio yet — create one in the Portfolio screen"));
        footer_->setText(tr("No portfolio"));
        return;
    }
    chart_->set_state(ui::TimeSeriesChart::State::Loading);
    if (isVisible())
        request_all();
}

void PortfolioPerformanceWidget::request_all() {
    if (!has_portfolio())
        return;
    auto& svc = PortfolioService::instance();
    svc.load_summary(portfolio_id());
    svc.load_snapshots(portfolio_id(), 400);
    subscribe_benchmark();
}

void PortfolioPerformanceWidget::subscribe_benchmark() {
    auto& hub = datahub::DataHub::instance();
    if (!bench_topic_.isEmpty())
        hub.unsubscribe(this, bench_topic_);
    bench_topic_ = services::MarketDataService::performance_history_topic(benchmark_symbol());
    hub.subscribe(this, bench_topic_, [this](const QVariant& v) {
        if (!v.canConvert<QVector<services::HistoryPoint>>())
            return;
        bench_.clear();
        for (const auto& b : v.value<QVector<services::HistoryPoint>>())
            if (b.close > 0)
                bench_.append(QPointF(double(b.timestamp) * 1000.0, b.close));
        render_chart();
    });
}

void PortfolioPerformanceWidget::showEvent(QShowEvent* e) {
    PortfolioTileBase::showEvent(e);
    refresh_timer_->start();
    request_all();
}

void PortfolioPerformanceWidget::hideEvent(QHideEvent* e) {
    PortfolioTileBase::hideEvent(e); // drops the hub subscriptions
    refresh_timer_->stop();
    bench_topic_.clear();
}

void PortfolioPerformanceWidget::on_summary(const portfolio::PortfolioSummary& s) {
    set_loading(false);
    summary_ = s;
    have_summary_ = true;
    render_summary();
}

void PortfolioPerformanceWidget::render_summary() {
    if (!have_summary_)
        return;
    const auto& s = summary_;
    const QString ccy = s.base_currency.isEmpty() ? base_currency() : s.base_currency;
    const auto compact = ui::formatting::Compact::FromSettings;
    // "≈" when a holding was converted at a stale FX rate (the rate's age in the tooltip).
    const QString approx = pfmt::approx_prefix(s);
    const QString stale_fx = pfmt::fx_stale_line(s);
    if (auto* t = kpis_->tile("value")) {
        t->set_value(approx + pp_fmt::money(s.total_market_value, ccy, compact));
        t->set_tooltip_text(tr("Market value of the valued holdings, in %1").arg(ccy) +
                            (stale_fx.isEmpty() ? QString() : QLatin1Char('\n') + stale_fx));
        if (!s.complete) {
            QStringList why;
            if (!s.unpriced.isEmpty())
                why << tr("no quote: %1").arg(s.unpriced.join(QStringLiteral(", ")));
            if (!s.fx_missing.isEmpty())
                why << tr("no FX rate: %1").arg(s.fx_missing.join(QStringLiteral(", ")));
            t->set_state(ui::StatTile::State::Stale, tr("Totals cover %1 of %2 positions — %3")
                                                         .arg(s.valued_positions)
                                                         .arg(s.total_positions)
                                                         .arg(why.join(QStringLiteral("; "))));
        } else {
            t->set_state(ui::StatTile::State::Normal);
        }
        t->set_sub_label(tr("%n position(s)", "", s.total_positions));
    }
    if (auto* t = kpis_->tile("today")) {
        t->set_value(approx + pp_fmt::signed_money(s.total_day_change, ccy, compact));
        t->set_delta(s.total_day_change_percent);
        t->set_tone(s.total_day_change > 0   ? ui::StatTile::Tone::Positive
                    : s.total_day_change < 0 ? ui::StatTile::Tone::Negative
                                             : ui::StatTile::Tone::Neutral);
        t->set_tooltip_text(tr("Change of the valued holdings since the previous close, in %1").arg(ccy) +
                            (stale_fx.isEmpty() ? QString() : QLatin1Char('\n') + stale_fx));
    }
}

void PortfolioPerformanceWidget::on_snapshots(const QVector<portfolio::PortfolioSnapshot>& snaps) {
    snapshots_loaded_ = true;
    snapshot_count_ = snaps.size();
    twr_.clear();
    // Flows whose dated FX rate is not loaded yet are converted at today's rate and
    // listed in twr_approx_: the returns are then marked approximate.
    const auto series = PortfolioService::build_twr_series(snaps, PortfolioService::instance().ledger(portfolio_id()),
                                                           &twr_approx_);
    for (const auto& p : series) {
        const QDate d = QDate::fromString(p.date.left(10), Qt::ISODate);
        if (!d.isValid() || !std::isfinite(p.cumulative))
            continue;
        if (!twr_.isEmpty() && !p.valid)
            continue; // no capital at risk in that period: the index does not move
        twr_.append({d, 1.0 + p.cumulative});
    }
    backfill_btn_->setVisible(twr_.size() < 5 && !backfilling_);
    render_returns();
    render_chart();
}

double PortfolioPerformanceWidget::period_return(const QDate& anchor) const {
    if (twr_.size() < 2)
        return services::kMarketDataAbsent;
    const IndexPoint* base = nullptr;
    for (const auto& p : twr_) {
        if (p.date <= anchor)
            base = &p;
        else
            break;
    }
    if (!base) {
        // The series starts after the anchor: accept a start within 4 days
        // (the first trading day of the window), else the window is not covered.
        if (twr_.first().date.toJulianDay() - anchor.toJulianDay() > 4)
            return services::kMarketDataAbsent;
        base = &twr_.first();
    }
    if (base->index <= 0)
        return services::kMarketDataAbsent;
    return (twr_.last().index / base->index - 1.0) * 100.0;
}

void PortfolioPerformanceWidget::render_returns() {
    if (twr_.size() < 2) {
        const QString why = snapshot_count_ < 2
                                ? tr("Not enough daily history yet (%n snapshot(s)) — build it from market prices",
                                     "", snapshot_count_)
                                : tr("No measurable period in the history");
        for (const char* k : {"mtd", "ytd", "1y", "all"})
            pp_set_return(kpis_->tile(QString::fromLatin1(k)), services::kMarketDataAbsent, why);
        footer_->setText(why);
        return;
    }
    const QDate last = twr_.last().date;
    const QDate mtd_anchor = QDate(last.year(), last.month(), 1).addDays(-1);
    const QDate ytd_anchor = QDate(last.year() - 1, 12, 31);
    const QDate y1_anchor = last.addYears(-1);
    const QString to = pp_fmt::calendar_date(last);
    const QString starts = pp_fmt::calendar_date(twr_.first().date);
    const QString uncovered = tr("History starts %1").arg(starts);
    const double mtd = period_return(mtd_anchor);
    const double ytd = period_return(ytd_anchor);
    const double y1 = period_return(y1_anchor);
    const bool approx = !twr_approx_.isEmpty();
    const QString approx_note =
        approx ? QLatin1Char('\n') + tr("Approximate: flows in %1 converted at today's FX rate (the rate of their date "
                                        "is not loaded yet).")
                                         .arg(twr_approx_.join(QStringLiteral(", ")))
               : QString();
    pp_set_return(kpis_->tile("mtd"), mtd,
                  std::isfinite(mtd) ? tr("Time-weighted, month to %1").arg(to) + approx_note : uncovered, approx);
    pp_set_return(kpis_->tile("ytd"), ytd,
                  std::isfinite(ytd) ? tr("Time-weighted, year to %1").arg(to) + approx_note : uncovered, approx);
    pp_set_return(kpis_->tile("1y"), y1,
                  std::isfinite(y1) ? tr("Time-weighted, 12 months to %1").arg(to) + approx_note : uncovered, approx);
    const double all = (twr_.last().index / twr_.first().index - 1.0) * 100.0;
    pp_set_return(kpis_->tile("all"), all, tr("Time-weighted, %1 to %2").arg(starts, to) + approx_note, approx);
    if (auto* t = kpis_->tile("all"))
        t->set_label(tr("SINCE %1").arg(pp_fmt::calendar_date(twr_.first().date, pp_fmt::DateStyle::Short)));
    QString foot = tr("Time-weighted returns (deposits and withdrawals removed) from %n daily value(s) to %1 · "
                      "benchmark %2",
                      "", int(twr_.size()))
                       .arg(to, benchmark_symbol());
    if (approx)
        foot += QStringLiteral(" · ") + tr("≈ approximate: %1 flows at today's FX rate")
                                            .arg(twr_approx_.join(QStringLiteral(", ")));
    footer_->setText(foot);
}

void PortfolioPerformanceWidget::render_chart() {
    if (twr_.size() < 2) {
        if (snapshots_loaded_)
            chart_->set_state(ui::TimeSeriesChart::State::Empty, tr("Not enough history to chart"));
        return;
    }
    chart_->clear();
    QVector<QPointF> pts;
    pts.reserve(twr_.size());
    for (const auto& p : twr_)
        pts.append(QPointF(pp_ms(p.date), p.index));
    ui::TimeSeriesChart::SeriesSpec port;
    port.name = tr("Portfolio (TWR)");
    port.points = pts;
    port.style = ui::TimeSeriesChart::Style::Line;
    port.color = ui::ChartColorRole::Accent;
    chart_->add_series(port);
    if (!bench_.isEmpty()) {
        QVector<QPointF> b;
        const double from = pts.first().x();
        for (const auto& p : bench_)
            if (p.x() >= from - 86'400'000.0 * 4)
                b.append(p);
        ui::TimeSeriesChart::SeriesSpec bs;
        bs.name = benchmark_symbol();
        bs.points = b;
        bs.color = ui::ChartColorRole::Muted;
        bs.dashed = true;
        chart_->add_series(bs);
    }
    chart_->set_state(ui::TimeSeriesChart::State::Ready);
}

void PortfolioPerformanceWidget::on_theme_changed() {
    set_css_cached(footer_, QString("color:%1;font-size:%2px;background:transparent;")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::px(-4)));
}

void PortfolioPerformanceWidget::retranslateUi() {
    PortfolioTileBase::retranslateUi();
    set_title(tr("PORTFOLIO PERFORMANCE"));
    render_returns();
}

} // namespace fincept::screens::widgets
