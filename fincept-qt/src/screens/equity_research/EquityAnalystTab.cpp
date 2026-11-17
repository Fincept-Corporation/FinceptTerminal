// src/screens/equity_research/EquityAnalystTab.cpp
#include "screens/equity_research/EquityAnalystTab.h"

#include "screens/equity_research/EquityResearchFormat.h"
#include "services/fincept_data/FinceptDataService.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/RangeBar.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QRegularExpression>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

namespace fd = services::fincept_data;

namespace equity_analyst_tab_detail {

/// Upgrades / downgrades asked for (the server keeps hundreds; the newest matter).
constexpr int kRatingsLimit = 25;

enum TargetStat { kCurrent, kLow, kMedian, kMean, kHigh, kUpside, kAnalysts, kConsensus, kStatCount };

} // namespace equity_analyst_tab_detail

using namespace equity_analyst_tab_detail;

EquityAnalystTab::EquityAnalystTab(QWidget* parent) : EquityDataTab(QStringLiteral("equityAnalystTab"), parent) {
    build_ui();
    retranslate();
}

EquityAnalystTab::Stat EquityAnalystTab::make_stat(QWidget* parent, QGridLayout* grid, int column) {
    Stat s;
    s.caption = new QLabel(parent);
    s.caption->setObjectName(QStringLiteral("fdStatCaption"));
    s.value = new QLabel(equity_fmt::dash(), parent);
    s.value->setObjectName(QStringLiteral("fdStatValue"));
    grid->addWidget(s.caption, 0, column);
    grid->addWidget(s.value, 1, column);
    return s;
}

void EquityAnalystTab::build_ui() {
    auto* scroll = new QScrollArea(body_widget());
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("fdScrollContent"));
    content->setStyleSheet(QStringLiteral("#fdScrollContent { background:transparent; }"));
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(0, 0, 4, 0);
    vl->setSpacing(6);

    sec_targets_ = make_section(QString(), content);
    vl->addWidget(sec_targets_);
    auto* stats = new QGridLayout;
    stats->setHorizontalSpacing(28);
    stats->setVerticalSpacing(2);
    for (int i = 0; i < kStatCount; ++i)
        target_stats_.append(make_stat(content, stats, i));
    stats->setColumnStretch(kStatCount, 1);
    vl->addLayout(stats);
    // Where the price sits against the low / median / mean / high target.
    target_range_ = new ui::RangeBar(content);
    target_range_->setMinimumHeight(ui::metrics::height(52));
    vl->addWidget(target_range_);

    auto add_table = [&](QLabel*& section, QTableWidget*& table, QWidget* chart = nullptr) {
        section = make_section(QString(), content);
        vl->addWidget(section);
        if (chart) {
            chart->setMinimumHeight(ui::metrics::height(200));
            vl->addWidget(chart);
        }
        table = make_table(content);
        vl->addWidget(table);
    };
    trend_chart_ = new ui::BarChart(content);
    trend_chart_->set_mode(ui::BarChart::Mode::Stacked);
    add_table(sec_trend_, trend_table_, trend_chart_);
    add_table(sec_estimates_, estimates_table_);
    revisions_chart_ = new ui::TimeSeriesChart(content);
    revisions_chart_->set_x_is_time(false);
    revisions_chart_->set_transparent_background(true);
    revisions_chart_->set_show_legend(true);
    revisions_chart_->set_zoom_enabled(false);
    add_table(sec_revisions_, revisions_table_, revisions_chart_);
    history_chart_ = new ui::BarChart(content);
    add_table(sec_history_, history_table_, history_chart_);
    add_table(sec_ratings_, ratings_table_);
    ratings_note_ = make_note(content);
    vl->addWidget(ratings_note_);
    vl->addStretch(1);

    scroll->setWidget(content);
    body_layout()->addWidget(scroll, 1);
}

void EquityAnalystTab::retranslate() {
    sec_targets_->setText(tr("PRICE TARGETS"));
    sec_trend_->setText(tr("RECOMMENDATION TREND"));
    sec_estimates_->setText(tr("EARNINGS & REVENUE ESTIMATES"));
    sec_revisions_->setText(tr("EPS TREND & REVISIONS"));
    sec_history_->setText(tr("EARNINGS SURPRISES"));
    sec_ratings_->setText(tr("UPGRADES & DOWNGRADES"));
    const QStringList captions = {tr("CURRENT"), tr("LOW"),      tr("MEDIAN"),  tr("MEAN"),
                                  tr("HIGH"),    tr("UPSIDE"),   tr("ANALYSTS"), tr("CONSENSUS")};
    for (int i = 0; i < target_stats_.size() && i < captions.size(); ++i)
        target_stats_[i].caption->setText(captions[i]);
    target_stats_[kUpside].caption->setToolTip(tr("Mean target vs the current price"));
    target_stats_[kConsensus].caption->setToolTip(tr("Recommendation mean: 1 = strong buy … 5 = strong sell"));
    populate();
}

QString EquityAnalystTab::loading_text() const {
    return tr("LOADING ANALYST DATA…");
}

QString EquityAnalystTab::not_found_text(const fd::DataError& e) const {
    return e.message.isEmpty() ? tr("No analyst coverage found for %1.").arg(symbol()) : e.message;
}

void EquityAnalystTab::clear_view() {
    data_ = {};
    loaded_ = false;
    populate();
}

void EquityAnalystTab::request(bool refresh) {
    const quint64 token = start_request();
    fd::FinceptDataService::instance().fetch_analyst(
        symbol(), kRatingsLimit, this,
        [this, token](const fd::DataResult& r) {
            if (!is_current(token))
                return; // the user moved to another symbol meanwhile
            finish(r);
            if (!r.ok())
                return;
            data_ = fd::parse_analyst(r.data);
            loaded_ = true;
            populate();
        },
        refresh);
}

bool EquityAnalystTab::apply_remembered(const fd::DataResult& r) {
    data_ = fd::parse_analyst(r.data);
    loaded_ = true;
    populate();
    return true;
}

QString EquityAnalystTab::period_text(const QString& period) const {
    static const QRegularExpression re(QStringLiteral("^([+-]?)(\\d+)([mqy])$"));
    const auto m = re.match(period);
    if (!m.hasMatch())
        return period;
    const int n = m.captured(2).toInt();
    const bool ahead = m.captured(1) == QLatin1String("+");
    const QChar unit = m.captured(3).at(0);
    if (unit == QLatin1Char('m')) {
        if (n == 0)
            return tr("This month");
        return ahead ? tr("In %n month(s)", nullptr, n) : tr("%n month(s) ago", nullptr, n);
    }
    if (unit == QLatin1Char('q')) {
        if (n == 0)
            return tr("Current quarter");
        if (n == 1)
            return ahead ? tr("Next quarter") : tr("Last quarter");
        return ahead ? tr("%n quarter(s) ahead", nullptr, n) : tr("%n quarter(s) ago", nullptr, n);
    }
    if (n == 0)
        return tr("Current year");
    if (n == 1)
        return ahead ? tr("Next year") : tr("Last year");
    return ahead ? tr("%n year(s) ahead", nullptr, n) : tr("%n year(s) ago", nullptr, n);
}

void EquityAnalystTab::populate() {
    populate_targets();
    populate_trend();
    populate_estimates();
    populate_revisions();
    populate_history();
    populate_ratings();
}

void EquityAnalystTab::populate_targets() {
    const auto& t = data_.targets;
    const QString ccy = data_.currency;
    auto money = [&](double v) { return fd::has(v) ? equity_fmt::price(v, ccy) : equity_fmt::dash(); };
    target_stats_[kCurrent].value->setText(money(t.current_price));
    target_stats_[kLow].value->setText(money(t.low));
    target_stats_[kMedian].value->setText(money(t.median));
    target_stats_[kMean].value->setText(money(t.mean));
    target_stats_[kHigh].value->setText(money(t.high));
    const double upside =
        fd::has(t.mean) && fd::has(t.current_price) && t.current_price > 0 ? (t.mean / t.current_price - 1.0) * 100.0
                                                                          : fd::kNaN;
    target_stats_[kUpside].value->setText(percent(upside, 1, true));
    target_stats_[kUpside].value->setStyleSheet(
        fd::has(upside) ? QStringLiteral("color:%1;").arg(sign_color(upside).name()) : QString());
    target_stats_[kAnalysts].value->setText(count(t.analysts));
    QString consensus = equity_fmt::dash();
    if (!t.recommendation_key.isEmpty()) {
        consensus = t.recommendation_key.toUpper().replace(QLatin1Char('_'), QLatin1Char(' '));
        if (fd::has(t.recommendation_mean))
            consensus += QStringLiteral(" (%1)").arg(QString::number(t.recommendation_mean, 'f', 2));
    }
    target_stats_[kConsensus].value->setText(consensus);

    if (fd::has(t.low) && fd::has(t.high) && t.high >= t.low) {
        ui::RangeBarData d;
        const double px = fd::has(t.current_price) ? t.current_price : t.low;
        d.low = std::min(t.low, px);
        d.high = std::max(t.high, px);
        d.fill_to_current = false;
        d.markers.append({t.low, tr("Low"), ui::RangeMarker::Role::Low});
        d.markers.append({t.high, tr("High"), ui::RangeMarker::Role::High});
        if (fd::has(t.median))
            d.markers.append({t.median, tr("Median"), ui::RangeMarker::Role::Neutral});
        if (fd::has(t.mean))
            d.markers.append({t.mean, tr("Mean"), ui::RangeMarker::Role::Target});
        if (fd::has(t.current_price))
            d.markers.append({t.current_price, tr("Price"), ui::RangeMarker::Role::Current});
        target_range_->set_value_formatter([ccy](double v) { return equity_fmt::price(v, ccy); });
        target_range_->set_data(d);
        target_range_->set_state(ui::PaintedChart::Ready);
    } else {
        target_range_->set_data({});
        target_range_->set_state(ui::PaintedChart::Empty, loaded_ ? tr("No price targets") : QString());
    }
}

void EquityAnalystTab::populate_trend() {
    reset_table(trend_table_,
                {tr("PERIOD"), tr("STRONG BUY"), tr("BUY"), tr("HOLD"), tr("SELL"), tr("STRONG SELL"), tr("TOTAL"),
                 tr("BUY %")},
                static_cast<int>(data_.trend.size()));
    const QColor pos(ui::colors::POSITIVE()), neg(ui::colors::NEGATIVE()), neutral(ui::colors::WARNING());
    for (int r = 0; r < data_.trend.size(); ++r) {
        const auto& p = data_.trend[r];
        double total = 0.0;
        for (double v : {p.strong_buy, p.buy, p.hold, p.sell, p.strong_sell})
            total += fd::has(v) ? v : 0.0;
        const double buys = (fd::has(p.strong_buy) ? p.strong_buy : 0.0) + (fd::has(p.buy) ? p.buy : 0.0);
        trend_table_->setItem(r, 0, text_item(period_text(p.period)));
        trend_table_->setItem(r, 1, number_item(p.strong_buy, count(p.strong_buy), pos));
        trend_table_->setItem(r, 2, number_item(p.buy, count(p.buy), pos));
        trend_table_->setItem(r, 3, number_item(p.hold, count(p.hold), neutral));
        trend_table_->setItem(r, 4, number_item(p.sell, count(p.sell), neg));
        trend_table_->setItem(r, 5, number_item(p.strong_sell, count(p.strong_sell), neg));
        trend_table_->setItem(r, 6, number_item(total, count(total)));
        const double buy_pct = total > 0 ? buys / total * 100.0 : fd::kNaN;
        trend_table_->setItem(r, 7, number_item(buy_pct, percent(buy_pct, 0)));
    }
    trend_table_->resizeColumnsToContents();
    trend_table_->setSortingEnabled(true);
    fit_table_height(trend_table_);

    // Stacked mix per month, oldest on the left (the source lists the newest first).
    QStringList cats;
    QVector<ui::BarSeries> series(5);
    const char* names[5] = {QT_TR_NOOP("Strong buy"), QT_TR_NOOP("Buy"), QT_TR_NOOP("Hold"), QT_TR_NOOP("Sell"),
                            QT_TR_NOOP("Strong sell")};
    const QColor colors[5] = {QColor(ui::colors::POSITIVE()), QColor(ui::colors::POSITIVE_DIM()),
                              QColor(ui::colors::WARNING()), QColor(ui::colors::NEGATIVE_DIM()),
                              QColor(ui::colors::NEGATIVE())};
    for (int i = 0; i < 5; ++i) {
        series[i].name = tr(names[i]);
        series[i].color = colors[i];
    }
    for (int r = static_cast<int>(data_.trend.size()) - 1; r >= 0; --r) {
        const auto& p = data_.trend[r];
        cats << period_text(p.period);
        const double v[5] = {p.strong_buy, p.buy, p.hold, p.sell, p.strong_sell};
        for (int i = 0; i < 5; ++i)
            series[i].values << (fd::has(v[i]) ? v[i] : 0.0);
    }
    trend_chart_->set_categories(cats);
    trend_chart_->set_series(series);
    trend_chart_->set_value_formatter([](double v) { return ui::formatting::number(v, 0); });
    trend_chart_->set_state(cats.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready);
}

void EquityAnalystTab::populate_estimates() {
    reset_table(estimates_table_,
                {tr("PERIOD"), tr("ENDS"), tr("EPS EST."), tr("EPS LOW"), tr("EPS HIGH"), tr("EPS YEAR AGO"),
                 tr("EPS GROWTH"), tr("# EPS"), tr("REVENUE EST."), tr("REV. LOW"), tr("REV. HIGH"), tr("REV. GROWTH"),
                 tr("# REV.")},
                static_cast<int>(data_.estimates.size()));
    for (int r = 0; r < data_.estimates.size(); ++r) {
        const auto& e = data_.estimates[r];
        const double eps_g = fd::has(e.eps.growth) ? e.eps.growth * 100.0 : fd::kNaN;
        const double rev_g = fd::has(e.revenue.growth) ? e.revenue.growth * 100.0 : fd::kNaN;
        estimates_table_->setItem(r, 0, text_item(period_text(e.period)));
        estimates_table_->setItem(r, 1, text_item(e.end_date));
        // EPS in the currency the estimate is made in (often the reporting currency, not
        // the quote currency: Vodafone estimates in EUR, quote in GBp).
        const QString eps_ccy = e.eps.currency.isEmpty() ? data_.currency : e.eps.currency;
        const QString rev_ccy = e.revenue.currency.isEmpty() ? eps_ccy : e.revenue.currency;
        auto eps = [&eps_ccy](double v) { return fd::has(v) ? equity_fmt::price(v, eps_ccy, 2) : equity_fmt::dash(); };
        auto rev = [&rev_ccy](double v) { return fd::has(v) ? equity_fmt::money(v, rev_ccy) : equity_fmt::dash(); };
        estimates_table_->setItem(r, 2, number_item(e.eps.avg, eps(e.eps.avg)));
        estimates_table_->setItem(r, 3, number_item(e.eps.low, eps(e.eps.low)));
        estimates_table_->setItem(r, 4, number_item(e.eps.high, eps(e.eps.high)));
        estimates_table_->setItem(r, 5, number_item(e.eps.year_ago, eps(e.eps.year_ago)));
        estimates_table_->setItem(r, 6, number_item(eps_g, percent(eps_g, 1, true), sign_color(eps_g)));
        estimates_table_->setItem(r, 7, number_item(e.eps.analysts, count(e.eps.analysts)));
        estimates_table_->setItem(r, 8, number_item(e.revenue.avg, rev(e.revenue.avg)));
        estimates_table_->setItem(r, 9, number_item(e.revenue.low, rev(e.revenue.low)));
        estimates_table_->setItem(r, 10, number_item(e.revenue.high, rev(e.revenue.high)));
        estimates_table_->setItem(r, 11, number_item(rev_g, percent(rev_g, 1, true), sign_color(rev_g)));
        estimates_table_->setItem(r, 12, number_item(e.revenue.analysts, count(e.revenue.analysts)));
    }
    estimates_table_->resizeColumnsToContents();
    estimates_table_->setSortingEnabled(true);
    fit_table_height(estimates_table_);
}

void EquityAnalystTab::populate_revisions() {
    reset_table(revisions_table_,
                {tr("PERIOD"), tr("EPS NOW"), tr("7D AGO"), tr("30D AGO"), tr("60D AGO"), tr("90D AGO"),
                 tr("CHG 90D"), tr("UP 7D"), tr("DOWN 7D"), tr("UP 30D"), tr("DOWN 30D")},
                static_cast<int>(data_.estimates.size()));
    const QColor pos(ui::colors::POSITIVE()), neg(ui::colors::NEGATIVE());
    for (int r = 0; r < data_.estimates.size(); ++r) {
        const auto& e = data_.estimates[r];
        const double chg = fd::has(e.eps_current) && fd::has(e.eps_90d_ago) && e.eps_90d_ago != 0.0
                               ? (e.eps_current / e.eps_90d_ago - 1.0) * 100.0
                               : fd::kNaN;
        revisions_table_->setItem(r, 0, text_item(period_text(e.period)));
        revisions_table_->setItem(r, 1, number_item(e.eps_current, num(e.eps_current, 3)));
        revisions_table_->setItem(r, 2, number_item(e.eps_7d_ago, num(e.eps_7d_ago, 3)));
        revisions_table_->setItem(r, 3, number_item(e.eps_30d_ago, num(e.eps_30d_ago, 3)));
        revisions_table_->setItem(r, 4, number_item(e.eps_60d_ago, num(e.eps_60d_ago, 3)));
        revisions_table_->setItem(r, 5, number_item(e.eps_90d_ago, num(e.eps_90d_ago, 3)));
        revisions_table_->setItem(r, 6, number_item(chg, percent(chg, 1, true), sign_color(chg)));
        revisions_table_->setItem(r, 7, number_item(e.up_7d, count(e.up_7d), pos));
        revisions_table_->setItem(r, 8, number_item(e.down_7d, count(e.down_7d), neg));
        revisions_table_->setItem(r, 9, number_item(e.up_30d, count(e.up_30d), pos));
        revisions_table_->setItem(r, 10, number_item(e.down_30d, count(e.down_30d), neg));
    }
    revisions_table_->resizeColumnsToContents();
    revisions_table_->setSortingEnabled(true);
    fit_table_height(revisions_table_);

    // EPS-estimate revision lines: x = days before today (-90 ... 0), one line per period.
    revisions_chart_->clear();
    bool any = false;
    for (const auto& e : data_.estimates) {
        ui::TimeSeriesChart::SeriesSpec spec;
        spec.name = period_text(e.period);
        spec.rebase = false;
        const double xs[5] = {-90, -60, -30, -7, 0};
        const double ys[5] = {e.eps_90d_ago, e.eps_60d_ago, e.eps_30d_ago, e.eps_7d_ago, e.eps_current};
        for (int i = 0; i < 5; ++i)
            if (fd::has(ys[i]))
                spec.points << QPointF(xs[i], ys[i]);
        if (spec.points.size() >= 2) {
            revisions_chart_->add_series(spec);
            any = true;
        }
    }
    revisions_chart_->set_time_formatter([](double x) { return tr("%n day(s) ago", nullptr, static_cast<int>(-x)); });
    revisions_chart_->set_tick_time_formatter([](double x) { return tr("%1d").arg(static_cast<int>(x)); });
    revisions_chart_->set_state(any ? ui::TimeSeriesChart::State::Ready : ui::TimeSeriesChart::State::Empty,
                                loaded_ ? tr("No estimate revisions") : QString());
}

void EquityAnalystTab::populate_history() {
    reset_table(history_table_,
                {tr("QUARTER ENDED"), tr("EPS ESTIMATE"), tr("EPS ACTUAL"), tr("DIFFERENCE"), tr("SURPRISE")},
                static_cast<int>(data_.history.size()));
    QString eps_ccy = data_.currency;
    for (const auto& e : data_.estimates)
        if (!e.eps.currency.isEmpty()) {
            eps_ccy = e.eps.currency;
            break;
        }
    // Newest first (the source lists oldest first).
    for (int i = 0; i < data_.history.size(); ++i) {
        const int r = static_cast<int>(data_.history.size()) - 1 - i;
        const auto& h = data_.history[i];
        history_table_->setItem(r, 0, text_item(h.quarter));
        history_table_->setItem(r, 1, number_item(h.eps_estimate, fd::has(h.eps_estimate) ? equity_fmt::price(h.eps_estimate, eps_ccy, 2) : equity_fmt::dash()));
        history_table_->setItem(r, 2, number_item(h.eps_actual, fd::has(h.eps_actual) ? equity_fmt::price(h.eps_actual, eps_ccy, 2) : equity_fmt::dash()));
        history_table_->setItem(r, 3, number_item(h.eps_difference, num(h.eps_difference, 2), sign_color(h.eps_difference)));
        history_table_->setItem(r, 4, number_item(h.surprise_pct, percent(h.surprise_pct, 2, true),
                                                  sign_color(h.surprise_pct)));
    }
    history_table_->resizeColumnsToContents();
    history_table_->setSortingEnabled(true);
    fit_table_height(history_table_);

    QStringList cats;
    ui::BarSeries est, act;
    est.name = tr("Estimate");
    act.name = tr("Actual");
    for (const auto& h : data_.history) {
        cats << h.quarter;
        est.values << h.eps_estimate;
        act.values << h.eps_actual;
    }
    history_chart_->set_categories(cats);
    history_chart_->set_series({est, act});
    history_chart_->set_value_formatter([eps_ccy](double v) { return equity_fmt::price(v, eps_ccy, 2); });
    history_chart_->set_state(cats.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready);
}

void EquityAnalystTab::populate_ratings() {
    reset_table(ratings_table_,
                {tr("DATE"), tr("FIRM"), tr("ACTION"), tr("FROM"), tr("TO"), tr("PRICE TARGET"), tr("PRIOR TARGET"),
                 tr("TARGET")},
                static_cast<int>(data_.rating_changes.size()));
    const QString ccy = data_.currency;
    for (int r = 0; r < data_.rating_changes.size(); ++r) {
        const auto& g = data_.rating_changes[r];
        QString action = g.action;
        QColor fg;
        if (g.action == QLatin1String("up")) {
            action = tr("Upgrade");
            fg = QColor(ui::colors::POSITIVE());
        } else if (g.action == QLatin1String("down")) {
            action = tr("Downgrade");
            fg = QColor(ui::colors::NEGATIVE());
        } else if (g.action == QLatin1String("main")) {
            action = tr("Maintain");
        } else if (g.action == QLatin1String("init")) {
            action = tr("Initiate");
            fg = QColor(ui::colors::CYAN());
        } else if (g.action == QLatin1String("reit")) {
            action = tr("Reiterate");
        }
        const QDateTime local = g.date.toLocalTime();
        auto* date = number_item(g.date.isValid() ? static_cast<double>(g.date.toSecsSinceEpoch()) : fd::kNaN,
                                 g.date.isValid() ? local.toString(QStringLiteral("yyyy-MM-dd")) : equity_fmt::dash());
        date->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        ratings_table_->setItem(r, 0, date);
        ratings_table_->setItem(r, 1, text_item(g.firm));
        ratings_table_->setItem(r, 2, text_item(action, fg));
        ratings_table_->setItem(r, 3, text_item(g.from_grade));
        ratings_table_->setItem(r, 4, text_item(g.to_grade));
        ratings_table_->setItem(
            r, 5, number_item(g.price_target, fd::has(g.price_target) ? equity_fmt::price(g.price_target, ccy) : equity_fmt::dash()));
        ratings_table_->setItem(r, 6, number_item(g.prior_price_target, fd::has(g.prior_price_target)
                                                                          ? equity_fmt::price(g.prior_price_target, ccy)
                                                                          : equity_fmt::dash()));
        ratings_table_->setItem(r, 7, text_item(g.price_target_action));
    }
    ratings_table_->resizeColumnsToContents();
    ratings_table_->setSortingEnabled(true);
    ratings_table_->sortByColumn(0, Qt::DescendingOrder); // newest first
    fit_table_height(ratings_table_);
    if (!loaded_) {
        ratings_note_->clear();
    } else if (data_.rating_changes.isEmpty()) {
        ratings_note_->setText(tr("No rating changes reported."));
    } else {
        ratings_note_->setText(tr("Newest %1 of %2 rating changes on record.")
                                   .arg(data_.rating_changes.size())
                                   .arg(data_.rating_changes_total));
    }
}

} // namespace fincept::screens
