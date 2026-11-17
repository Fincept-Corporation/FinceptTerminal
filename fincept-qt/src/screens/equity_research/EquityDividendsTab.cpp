// src/screens/equity_research/EquityDividendsTab.cpp
#include "screens/equity_research/EquityDividendsTab.h"

#include "screens/equity_research/EquityResearchFormat.h"
#include "screens/equity_research/EquityResearchUi.h"
#include "screens/equity_research/EquityStatementModel.h"
#include "services/equity/EquityResearchService.h"
#include "services/equity/EquityTechnicalMath.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMap>
#include <QScrollArea>
#include <QTableWidget>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace dividends_tab_detail {

namespace dv_uf = fincept::ui::formatting;

bool dv_ok(double v) {
    return std::isfinite(v);
}

QTableWidgetItem* dv_cell(const QString& t, Qt::Alignment a = Qt::AlignRight | Qt::AlignVCenter) {
    auto* it = new QTableWidgetItem(t);
    it->setTextAlignment(a);
    return it;
}

double dv_cagr(double first, double last, double years) {
    return (first > 0 && last > 0 && years > 0) ? std::pow(last / first, 1.0 / years) - 1.0 : std::nan("");
}

} // namespace dividends_tab_detail

using namespace dividends_tab_detail;

EquityDividendsTab::EquityDividendsTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::events_loaded, this,
            [this](const services::equity::EquityEvents& e) {
                if (e.symbol != current_symbol_)
                    return;
                events_ = e;
                events_loaded_ = true;
                status_->clear();
                render();
            });
    connect(&svc, &services::equity::EquityResearchService::financials_loaded, this,
            [this](const services::equity::FinancialsData& f) {
                if (f.symbol != current_symbol_)
                    return;
                fin_ = f;
                if (events_loaded_)
                    render();
            });
    connect(&svc, &services::equity::EquityResearchService::historical_loaded, this,
            [this](const QString& s, const QString& p, const QVector<services::equity::Candle>& c) {
                if (s != current_symbol_ || p != QLatin1String("max") || c.isEmpty())
                    return;
                daily_ = services::equity::tmath::normalize_daily(c);
                if (events_loaded_)
                    render();
            });
    connect(&svc, &services::equity::EquityResearchService::load_failed, this,
            [this](const services::equity::LoadFailure& f) {
                if (f.symbol != current_symbol_ || f.leg != QLatin1String("Events") || events_loaded_)
                    return;
                status_->show_error(tr("Dividend data unavailable for %1: %2").arg(current_symbol_, f.message.left(160)),
                                    f.message);
            });
    // Re-render what is held once a theme / font change has settled, and for a currency /
    // number-format change (no new request).
    const auto rerender = [this]() {
        if (events_loaded_)
            render();
    };
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, rerender);
    ui::formatting::on_format_changed(this, rerender);
}

void EquityDividendsTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    events_ = {};
    fin_ = {};
    daily_.clear();
    events_loaded_ = false;
    render();
    request(false);
}

void EquityDividendsTab::request(bool force) {
    if (current_symbol_.isEmpty())
        return;
    status_->show_loading(tr("Loading dividends for %1…").arg(current_symbol_));
    auto& svc = services::equity::EquityResearchService::instance();
    svc.fetch_events(current_symbol_, force);
    svc.fetch_financials(current_symbol_);
    svc.load_historical_only(current_symbol_, QStringLiteral("max"));
}

void EquityDividendsTab::build_ui() {
    setObjectName(QStringLiteral("equityDividendsTab"));
    er_ui::apply_style(this, []() {
        return QStringLiteral("#equityDividendsTab, QWidget#divContent { background:%1; }"
                              "QScrollArea#divScroll { background:transparent; border:0; }")
                   .arg(ui::colors::BG_BASE()) +
               er_ui::stylesheet();
    });
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("divScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);
    auto* content = new QWidget;
    content->setObjectName(QStringLiteral("divContent"));
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);
    status_ = new er_ui::StatusStrip(content);
    connect(status_, &er_ui::StatusStrip::retry_requested, this, [this]() { request(true); });
    vl->addWidget(status_);
    kpis_ = new ui::KpiStrip(content);
    kpis_->set_max_columns(6);
    vl->addWidget(kpis_);
    auto* charts = new QHBoxLayout;
    QVBoxLayout* b1 = nullptr;
    auto* p1 = er_ui::panel(QString(), &b1, &bars_title_, content);
    bars_ = new ui::BarChart(p1);
    bars_->setMinimumHeight(ui::metrics::height(240));
    b1->addWidget(bars_);
    charts->addWidget(p1, 1);
    QVBoxLayout* b2 = nullptr;
    auto* p2 = er_ui::panel(QString(), &b2, &yield_title_, content);
    yield_chart_ = new ui::TimeSeriesChart(p2);
    yield_chart_->setMinimumHeight(ui::metrics::height(240));
    yield_chart_->set_utc(true);
    yield_chart_->set_transparent_background(true);
    yield_chart_->set_range_presets({QStringLiteral("3Y"), QStringLiteral("5Y"), QStringLiteral("10Y"), QStringLiteral("MAX")});
    b2->addWidget(yield_chart_);
    charts->addWidget(p2, 1);
    vl->addLayout(charts);
    QVBoxLayout* b3 = nullptr;
    auto* p3 = er_ui::panel(QString(), &b3, &table_title_, content);
    table_ = new QTableWidget(p3);
    table_->setObjectName(QStringLiteral("erTable"));
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->hide();
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui::metrics::follow_row_height(table_, 22);
    table_->setMinimumHeight(ui::metrics::height(260));
    b3->addWidget(table_);
    vl->addWidget(p3);
    note_ = new QLabel(content);
    note_->setObjectName(QStringLiteral("erNote"));
    note_->setWordWrap(true);
    vl->addWidget(note_);
    vl->addStretch();
    scroll->setWidget(content);
    retranslateUi();
}

void EquityDividendsTab::render() {
    kpis_->clear();
    bars_->clear();
    yield_chart_->clear();
    table_->setRowCount(0);
    if (!events_loaded_) {
        bars_->set_state(ui::PaintedChart::Loading);
        yield_chart_->set_state(ui::TimeSeriesChart::State::Loading);
        note_->clear();
        return;
    }
    const QString qccy = events_.currency;
    const auto listing = equity_fmt::Listing{events_.currency, events_.financial_currency};
    const auto& divs = events_.dividends;
    if (divs.isEmpty()) {
        bars_->set_state(ui::PaintedChart::Empty, tr("No dividends on record"));
        yield_chart_->set_state(ui::TimeSeriesChart::State::Empty, tr("No dividends on record"));
        note_->setText(tr("%1 has paid no dividend that Yahoo Finance records.").arg(current_symbol_));
        return;
    }
    // Calendar-year totals (the current year is partial).
    QMap<int, double> by_year;
    QMap<int, int> count_year;
    for (const auto& d : divs) {
        by_year[d.date.year()] += d.amount;
        ++count_year[d.date.year()];
    }
    const int this_year = QDate::currentDate().year();
    const QDate today = QDate::currentDate();
    double ttm = 0.0;
    for (const auto& d : divs)
        if (d.date > today.addYears(-1))
            ttm += d.amount;
    const double last_price = daily_.isEmpty() ? std::nan("") : daily_.last().close;

    // Growth streak: consecutive complete years with total >= the year before.
    int streak = 0;
    {
        int y = this_year - 1;
        while (by_year.contains(y) && by_year.contains(y - 1) && by_year.value(y) >= by_year.value(y - 1) * 0.999) {
            ++streak;
            --y;
        }
    }
    auto cagr_n = [&](int n) {
        const int last = this_year - 1, first = last - n;
        return by_year.contains(first) && by_year.contains(last) ? dv_cagr(by_year.value(first), by_year.value(last), n)
                                                                 : std::nan("");
    };

    auto tile = [&](const QString& label, const QString& value, const QString& sub = {}, const QString& tip = {}) {
        auto* t = kpis_->add_tile(label);
        t->set_compact(true);
        t->set_value(value);
        if (!sub.isEmpty())
            t->set_sub_label(sub);
        if (!tip.isEmpty())
            t->set_tooltip_text(tip);
        return t;
    };
    tile(tr("TRAILING 12M"), equity_fmt::price(ttm, qccy),
         dv_ok(last_price) && last_price > 0 ? tr("yield %1").arg(dv_uf::fraction_pct(ttm / last_price, 2, false)) : QString(),
         tr("Dividends with an ex-date in the last 12 months, in the quote's units"));
    if (dv_ok(events_.dividend_rate) && events_.dividend_rate > 0)
        // Yahoo's dividendRate is in the quote's MAJOR currency (VOD.L: £0.04 = 3.4 % of 117.8p),
        // not the reporting currency (it was labelled "€0.0400").
        tile(tr("FORWARD RATE"),
             equity_fmt::price(events_.dividend_rate,
                               listing.quote_major().isEmpty() ? listing.reporting_major() : listing.quote_major()),
             dv_ok(events_.dividend_yield_pct) ? tr("yield %1").arg(dv_uf::pct(events_.dividend_yield_pct, 2, false)) : QString(),
             tr("Annual dividend rate Yahoo reports (the quote's major currency)"));
    if (dv_ok(events_.payout_ratio))
        tile(tr("PAYOUT (EPS)"), dv_uf::fraction_pct(events_.payout_ratio, 0, false), QString(),
             tr("Dividends ÷ earnings per share (trailing)"))
            ->set_tone(events_.payout_ratio > 1.0 ? ui::StatTile::Tone::Warning : ui::StatTile::Tone::Neutral);
    // Payout on free cash flow from the statements (TTM).
    if (!fin_.symbol.isEmpty()) {
        const int fy = fin_model::fiscal_year_end_month(fin_);
        const auto cf = fin_model::columns_for(fin_, fin_model::Statement::CashFlow, fin_model::Period::Ttm, fy);
        if (!cf.isEmpty()) {
            const double div = fin_model::line(cf.last().data, {"Cash Dividends Paid", "Common Stock Dividend Paid"});
            double fcf = fin_model::line(cf.last().data, {"Free Cash Flow"});
            if (dv_ok(div) && dv_ok(fcf) && fcf > 0)
                tile(tr("PAYOUT (FCF)"), dv_uf::fraction_pct(std::fabs(div) / fcf, 0, false), cf.last().label,
                     tr("Cash dividends paid ÷ free cash flow"))
                    ->set_tone(std::fabs(div) / fcf > 1.0 ? ui::StatTile::Tone::Warning : ui::StatTile::Tone::Neutral);
        }
    }
    const double c3 = cagr_n(3), c5 = cagr_n(5), c10 = cagr_n(10);
    tile(tr("GROWTH (CAGR)"), dv_ok(c5) ? dv_uf::fraction_pct(c5, 1, true) : dv_ok(c3) ? dv_uf::fraction_pct(c3, 1, true) : equity_fmt::dash(),
         tr("3y %1 · 5y %2 · 10y %3")
             .arg(dv_ok(c3) ? dv_uf::fraction_pct(c3, 1, true) : equity_fmt::dash(),
                  dv_ok(c5) ? dv_uf::fraction_pct(c5, 1, true) : equity_fmt::dash(),
                  dv_ok(c10) ? dv_uf::fraction_pct(c10, 1, true) : equity_fmt::dash()),
         tr("Compound growth of calendar-year totals to the last complete year"));
    tile(tr("GROWTH STREAK"), tr("%n year(s)", "", streak), QString(),
         tr("Consecutive complete calendar years with no cut in the total dividend"));
    if (events_.ex_dividend_date.isValid()) {
        const bool upcoming = events_.ex_dividend_date >= today;
        tile(upcoming ? tr("NEXT EX-DATE") : tr("LAST EX-DATE"), dv_uf::calendar_date(events_.ex_dividend_date),
             events_.dividend_pay_date.isValid() ? tr("pay %1").arg(dv_uf::calendar_date(events_.dividend_pay_date)) : QString());
    }

    // Annual bars (last 15 years).
    QStringList cats;
    ui::BarSeries s;
    s.name = tr("Dividends per share");
    const int first_year = std::max(by_year.firstKey(), this_year - 15);
    for (int y = first_year; y <= this_year; ++y) {
        cats << (y == this_year ? tr("%1 YTD").arg(y) : QString::number(y));
        s.values << (by_year.contains(y) ? by_year.value(y) : 0.0);
    }
    bars_->set_categories(cats);
    bars_->set_series({s});
    bars_->set_state(ui::PaintedChart::Ready);
    // Fixed decimals on the axis: the price rule (4 decimals under 1) printed "$0.5000"
    // between "$0.00" and "$1.00".
    bars_->set_value_formatter([qccy](double v) {
        return equity_fmt::price(v, qccy, (v != 0.0 && std::fabs(v) < 0.1) ? 3 : 2);
    });

    // Trailing-12-month yield over time (weekly points) and its average.
    if (daily_.size() > 260) {
        QVector<QPointF> pts;
        QVector<double> vals;
        int j0 = 0;
        double acc = 0.0;
        int j1 = 0;
        for (int i = 0; i < daily_.size(); i += 5) {
            const QDate d = QDateTime::fromSecsSinceEpoch(daily_[i].timestamp, QTimeZone::UTC).date();
            while (j1 < divs.size() && divs[j1].date <= d)
                acc += divs[j1++].amount;
            while (j0 < j1 && divs[j0].date <= d.addYears(-1))
                acc -= divs[j0++].amount;
            if (d < divs.first().date.addYears(1) || !(daily_[i].close > 0))
                continue;
            const double y = acc / daily_[i].close;
            pts << QPointF(static_cast<double>(daily_[i].timestamp) * 1000.0, y);
            vals << y;
        }
        if (!pts.isEmpty()) {
            ui::TimeSeriesChart::SeriesSpec spec;
            spec.name = tr("Trailing yield");
            spec.points = pts;
            spec.style = ui::TimeSeriesChart::Style::Area;
            spec.rebase = false;
            spec.color = ui::ChartColorRole::Accent;
            yield_chart_->add_series(spec);
            double m = 0;
            for (double v : vals)
                m += v;
            m /= vals.size();
            yield_chart_->add_hline(m, tr("avg %1").arg(dv_uf::fraction_pct(m, 2, false)), ui::ChartColorRole::Muted);
            yield_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left,
                                              [](double v) { return dv_uf::fraction_pct(v, 1, false); });
            yield_chart_->set_time_formatter([](double x) { return dv_uf::bar_date(static_cast<qint64>(x / 1000.0)); });
            yield_chart_->set_state(ui::TimeSeriesChart::State::Ready);
            yield_chart_->set_active_preset(QStringLiteral("10Y"));
        }
    } else {
        yield_chart_->set_state(ui::TimeSeriesChart::State::Loading, tr("Waiting for price history…"));
    }

    // History table (newest first): ex-date, amount, change vs the same payment a year earlier.
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({tr("EX-DATE"), tr("AMOUNT"), tr("YEAR TOTAL"), tr("CHANGE VS A YEAR EARLIER")});
    const int rows = std::min<int>(divs.size(), 60);
    table_->setRowCount(rows);
    for (int r = 0; r < rows; ++r) {
        const auto& d = divs[divs.size() - 1 - r];
        table_->setItem(r, 0, dv_cell(dv_uf::calendar_date(d.date), Qt::AlignLeft | Qt::AlignVCenter));
        table_->setItem(r, 1, dv_cell(equity_fmt::price(d.amount, qccy)));
        table_->setItem(r, 2, dv_cell(by_year.contains(d.date.year()) ? equity_fmt::price(by_year.value(d.date.year()), qccy)
                                                                       : equity_fmt::dash()));
        // The payment closest to a year before.
        double prev = std::nan("");
        qint64 best = 60;
        for (const auto& e : divs) {
            const qint64 gap = qAbs(e.date.daysTo(d.date.addYears(-1)));
            if (gap < best) {
                best = gap;
                prev = e.amount;
            }
        }
        table_->setItem(r, 3, dv_cell(dv_ok(prev) && prev > 0 ? dv_uf::fraction_pct(d.amount / prev - 1.0, 1, true)
                                                               : equity_fmt::dash()));
    }
    QStringList note;
    note << tr("Per-share amounts as Yahoo Finance quotes them (%1), split-adjusted").arg(qccy.isEmpty() ? tr("quote units") : qccy);
    if (listing.minor_quote())
        note << tr("%1 is 1/%2 of a %3").arg(qccy).arg(dv_uf::minor_unit_divisor(qccy)).arg(listing.quote_major());
    note << tr("the yield history divides by dividend-adjusted closes, so older yields read slightly high");
    note_->setText(note.join(QStringLiteral(" · ")));
}

void EquityDividendsTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquityDividendsTab::retranslateUi() {
    bars_title_->setText(tr("DIVIDENDS PER SHARE BY YEAR"));
    yield_title_->setText(tr("TRAILING DIVIDEND YIELD"));
    table_title_->setText(tr("PAYMENT HISTORY"));
    if (events_loaded_)
        render();
}

} // namespace fincept::screens
