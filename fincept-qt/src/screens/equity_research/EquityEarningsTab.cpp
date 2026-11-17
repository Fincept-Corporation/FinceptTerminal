// src/screens/equity_research/EquityEarningsTab.cpp
#include "screens/equity_research/EquityEarningsTab.h"

#include "screens/equity_research/EquityResearchFormat.h"
#include "screens/equity_research/EquityResearchUi.h"
#include "screens/equity_research/EquityStatementModel.h"
#include "services/equity/EquityResearchService.h"
#include "services/equity/EquityTechnicalMath.h"
#include "ui/charts/BarChart.h"
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
#include <QLocale>
#include <QScrollArea>
#include <QTableWidget>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace earnings_tab_detail {

namespace en_uf = fincept::ui::formatting;

bool en_ok(double v) {
    return std::isfinite(v);
}

QTableWidgetItem* en_cell(const QString& t, Qt::Alignment a = Qt::AlignRight | Qt::AlignVCenter) {
    auto* it = new QTableWidgetItem(t);
    it->setTextAlignment(a);
    return it;
}

/// Price move around a report day: from the close `before` trading days before it to the
/// close `after` trading days after (the report day itself counts as day 0, so a release
/// before the open or after the close are both inside the −1/+1 window).
double en_reaction(const QVector<services::equity::Candle>& daily, const QDate& report, int before, int after) {
    if (daily.isEmpty() || !report.isValid())
        return std::nan("");
    const qint64 t = QDateTime(report, QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch();
    int i0 = -1; // first bar on or after the report day
    for (int i = 0; i < daily.size(); ++i)
        if (daily[i].timestamp >= t) {
            i0 = i;
            break;
        }
    if (i0 < 0)
        return std::nan("");
    const int a = i0 - before, b = i0 + after;
    if (a < 0 || b >= daily.size() || !(daily[a].close > 0))
        return std::nan("");
    return daily[b].close / daily[a].close - 1.0;
}

} // namespace earnings_tab_detail

using namespace earnings_tab_detail;

EquityEarningsTab::EquityEarningsTab(QWidget* parent) : QWidget(parent) {
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
                status_->show_error(tr("Earnings data unavailable for %1: %2").arg(current_symbol_, f.message.left(160)),
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

void EquityEarningsTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    events_ = {};
    daily_.clear();
    events_loaded_ = false;
    render();
    request(false);
}

void EquityEarningsTab::request(bool force) {
    if (current_symbol_.isEmpty())
        return;
    status_->show_loading(tr("Loading earnings for %1…").arg(current_symbol_));
    auto& svc = services::equity::EquityResearchService::instance();
    svc.fetch_events(current_symbol_, force);
    svc.load_historical_only(current_symbol_, QStringLiteral("max"));
}

void EquityEarningsTab::build_ui() {
    setObjectName(QStringLiteral("equityEarningsTab"));
    er_ui::apply_style(this, []() {
        return QStringLiteral("#equityEarningsTab, QWidget#earnContent { background:%1; }"
                              "QScrollArea#earnScroll { background:transparent; border:0; }")
                   .arg(ui::colors::BG_BASE()) +
               er_ui::stylesheet();
    });
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("earnScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);
    auto* content = new QWidget;
    content->setObjectName(QStringLiteral("earnContent"));
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
    auto* p1 = er_ui::panel(QString(), &b1, &eps_title_, content);
    eps_chart_ = new ui::BarChart(p1);
    eps_chart_->setMinimumHeight(ui::metrics::height(240));
    b1->addWidget(eps_chart_);
    charts->addWidget(p1, 3);
    QVBoxLayout* b2 = nullptr;
    auto* p2 = er_ui::panel(QString(), &b2, &surprise_title_, content);
    surprise_chart_ = new ui::BarChart(p2);
    surprise_chart_->set_signed_colors(true);
    surprise_chart_->set_show_values(true);
    surprise_chart_->setMinimumHeight(ui::metrics::height(240));
    b2->addWidget(surprise_chart_);
    charts->addWidget(p2, 2);
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

void EquityEarningsTab::render() {
    kpis_->clear();
    eps_chart_->clear();
    surprise_chart_->clear();
    table_->setRowCount(0);
    if (!events_loaded_) {
        eps_chart_->set_state(ui::PaintedChart::Loading);
        surprise_chart_->set_state(ui::PaintedChart::Loading);
        note_->clear();
        return;
    }
    const auto listing = equity_fmt::Listing{events_.currency, events_.financial_currency};
    const QString eps_ccy = listing.reporting_major();
    const int fy = events_.last_fiscal_year_end.isValid() ? events_.last_fiscal_year_end.addDays(-7 * (events_.last_fiscal_year_end.day() <= 7)).month() : 12;
    auto fiscal = [fy](const services::equity::EarningsEvent& e) {
        if (e.quarter.isValid())
            return fin_model::fiscal_quarter_label(e.quarter, fy);
        return QStringLiteral("~%1").arg(en_uf::calendar_date(e.report_date, en_uf::DateStyle::Short));
    };
    auto eps = [&eps_ccy](double v) { return en_ok(v) ? equity_fmt::price(v, eps_ccy, 2) : equity_fmt::dash(); };

    // KPIs.
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
    // Yahoo keeps the last report date as "next" until the following one is announced.
    if (events_.next_earnings.isValid() && events_.next_earnings < QDate::currentDate()) {
        tile(tr("LAST REPORT"), en_uf::calendar_date(events_.next_earnings), tr("next date not announced yet"),
             tr("%n day(s) ago", "", static_cast<int>(events_.next_earnings.daysTo(QDate::currentDate()))));
    } else if (events_.next_earnings.isValid()) {
        QString sub;
        if (en_ok(events_.est_eps_avg))
            sub = tr("EPS est. %1").arg(eps(events_.est_eps_avg));
        if (en_ok(events_.est_eps_low) && en_ok(events_.est_eps_high))
            sub += tr(" (%1 – %2)").arg(eps(events_.est_eps_low), eps(events_.est_eps_high));
        tile(tr("NEXT REPORT"), en_uf::calendar_date(events_.next_earnings), sub,
             tr("%n day(s) away", "", static_cast<int>(QDate::currentDate().daysTo(events_.next_earnings))));
    }
    int beats = 0, scored = 0;
    double sum1 = 0, sum5 = 0, abs1 = 0;
    int n1 = 0, n5 = 0;
    QStringList cats;
    ui::BarSeries est_s, act_s, sur_s;
    est_s.name = tr("Estimate");
    act_s.name = tr("Actual");
    sur_s.name = tr("Surprise");
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels({tr("QUARTER"), tr("REPORTED"), tr("EPS ESTIMATE"), tr("EPS ACTUAL"), tr("SURPRISE"),
                                       tr("−1/+1 DAY"), tr("−5/+5 DAYS")});
    const auto& rows = events_.earnings;
    table_->setRowCount(rows.size());
    auto signed_item = [](double v) {
        auto* c = en_cell(en_ok(v) ? en_uf::fraction_pct(v, 1, true) : equity_fmt::dash());
        if (en_ok(v) && v != 0.0)
            c->setForeground(QColor(v > 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
        return c;
    };
    for (int i = 0; i < rows.size(); ++i) {
        const auto& e = rows[rows.size() - 1 - i]; // newest first in the table
        double surprise = e.surprise_pct;
        if (!en_ok(surprise) && en_ok(e.eps_actual) && en_ok(e.eps_estimate) && e.eps_estimate != 0.0)
            surprise = (e.eps_actual - e.eps_estimate) / std::fabs(e.eps_estimate) * 100.0;
        const double r1 = en_reaction(daily_, e.report_date, 1, 1);
        const double r5 = en_reaction(daily_, e.report_date, 5, 5);
        table_->setItem(i, 0, en_cell(fiscal(e), Qt::AlignLeft | Qt::AlignVCenter));
        auto* rep = en_cell(e.report_date.isValid() ? en_uf::calendar_date(e.report_date) + (e.date_estimated ? QStringLiteral(" *") : QString())
                                                    : equity_fmt::dash(),
                            Qt::AlignLeft | Qt::AlignVCenter);
        if (e.date_estimated)
            rep->setToolTip(tr("Estimated from the trading volume — Yahoo no longer serves this report date"));
        table_->setItem(i, 1, rep);
        table_->setItem(i, 2, en_cell(eps(e.eps_estimate)));
        table_->setItem(i, 3, en_cell(eps(e.eps_actual)));
        auto* s = en_cell(en_ok(surprise) ? en_uf::pct(surprise, 1, true) : equity_fmt::dash());
        if (en_ok(surprise))
            s->setForeground(QColor(surprise >= 0 ? ui::colors::SUCCESS() : ui::colors::DANGER()));
        table_->setItem(i, 4, s);
        table_->setItem(i, 5, signed_item(r1));
        table_->setItem(i, 6, signed_item(r5));
        if (en_ok(surprise)) {
            ++scored;
            beats += surprise > 0 ? 1 : 0;
        }
        if (en_ok(r1)) {
            sum1 += r1;
            abs1 += std::fabs(r1);
            ++n1;
        }
        if (en_ok(r5)) {
            sum5 += r5;
            ++n5;
        }
    }
    // Charts: oldest → newest, up to 12 quarters with an EPS figure.
    for (const auto& e : rows) {
        if (!en_ok(e.eps_actual) && !en_ok(e.eps_estimate))
            continue;
        cats << fiscal(e);
        est_s.values << e.eps_estimate;
        act_s.values << e.eps_actual;
        double surprise = e.surprise_pct;
        if (!en_ok(surprise) && en_ok(e.eps_actual) && en_ok(e.eps_estimate) && e.eps_estimate != 0.0)
            surprise = (e.eps_actual - e.eps_estimate) / std::fabs(e.eps_estimate) * 100.0;
        sur_s.values << surprise;
    }
    if (cats.size() > 12) {
        const int drop = static_cast<int>(cats.size()) - 12;
        cats = cats.mid(drop);
        est_s.values = est_s.values.mid(drop);
        act_s.values = act_s.values.mid(drop);
        sur_s.values = sur_s.values.mid(drop);
    }
    if (scored > 0)
        tile(tr("BEAT RATE"), tr("%1 of %2").arg(beats).arg(scored), en_uf::fraction_pct(static_cast<double>(beats) / scored, 0, false),
             tr("Quarters whose reported EPS beat the consensus estimate"));
    if (!rows.isEmpty()) {
        const auto& last = rows.last();
        double s = last.surprise_pct;
        if (!en_ok(s) && en_ok(last.eps_actual) && en_ok(last.eps_estimate) && last.eps_estimate != 0.0)
            s = (last.eps_actual - last.eps_estimate) / std::fabs(last.eps_estimate) * 100.0;
        if (en_ok(s))
            tile(tr("LAST SURPRISE"), en_uf::pct(s, 1, true), fiscal(last))
                ->set_tone(s >= 0 ? ui::StatTile::Tone::Positive : ui::StatTile::Tone::Negative);
    }
    if (n1 > 0)
        tile(tr("AVG MOVE −1/+1 DAY"), en_uf::fraction_pct(sum1 / n1, 1, true),
             tr("avg absolute %1").arg(en_uf::fraction_pct(abs1 / n1, 1, false)),
             tr("Close the day before the report to the close the day after, averaged over %n report(s)", "", n1));
    if (n5 > 0)
        tile(tr("AVG MOVE −5/+5 DAYS"), en_uf::fraction_pct(sum5 / n5, 1, true));

    if (cats.isEmpty()) {
        eps_chart_->set_state(ui::PaintedChart::Empty, tr("No quarterly EPS history (half-yearly reporters have none)"));
        surprise_chart_->set_state(ui::PaintedChart::Empty, tr("No estimates on record"));
    } else {
        eps_chart_->set_categories(cats);
        eps_chart_->set_series({est_s, act_s});
        eps_chart_->set_state(ui::PaintedChart::Ready);
        eps_chart_->set_value_formatter([eps_ccy](double v) { return equity_fmt::price(v, eps_ccy, 2); });
        surprise_chart_->set_categories(cats);
        surprise_chart_->set_series({sur_s});
        surprise_chart_->set_state(ui::PaintedChart::Ready);
        surprise_chart_->set_value_formatter([](double v) { return en_uf::pct(v, 1, true); });
    }
    QStringList note;
    note << tr("EPS in %1 (the reporting currency)").arg(eps_ccy.isEmpty() ? tr("the reporting currency") : eps_ccy);
    note << tr("* report date estimated from the trading volume after the quarter end");
    note << tr("reactions from daily closes; fiscal quarters follow the fiscal year ending in %1")
                .arg(QLocale().monthName(fy));
    note_->setText(note.join(QStringLiteral(" · ")));
}

void EquityEarningsTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquityEarningsTab::retranslateUi() {
    eps_title_->setText(tr("EPS — ESTIMATE VS ACTUAL"));
    surprise_title_->setText(tr("SURPRISE (%)"));
    table_title_->setText(tr("EARNINGS HISTORY AND PRICE REACTION"));
    if (events_loaded_)
        render();
}

} // namespace fincept::screens
