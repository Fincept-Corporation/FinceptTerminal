// src/screens/equity_research/EquityFinancialsTab_Populate.cpp
//
// Data-loading + view-population: on_financials_loaded, populate_*_view,
// rebuild_*_chart.
//
// Part of the partial-class split of EquityFinancialsTab.cpp.

#include "screens/equity_research/EquityFinancialsTab.h"
#include "screens/equity_research/EquityFinancialsTab_internal.h"
#include "services/equity/EquityResearchService.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLineSeries>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <QValueAxis>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

using namespace financials_internal;

namespace {

/// Rendered in place of any figure the filings did not supply. The tab used to
/// print a fabricated 0 / 0.00% / "-100.00% YoY" for these.
const QString kNoData = QStringLiteral("—");

/// get_val() now returns NaN for "the filing does not carry this line", so every
/// derived figure is NaN-poisoned rather than silently zeroed.
bool missing(double v) {
    return std::isnan(v);
}

/// Ratio over a strictly positive denominator — the shape the old code wrote as
/// `den > 0 ? num / den : 0.0`, but yielding "not computable" instead of a
/// fabricated zero. Never substitutes a denominator.
double pos_ratio(double num, double den) {
    return (std::isnan(num) || std::isnan(den) || den <= 0.0) ? qQNaN() : num / den;
}

/// Period-over-period growth. Needs a real, positive base: a missing current
/// value used to produce exactly -1.0, which printed as "-100.00% YoY" for a
/// company that never reported the field.
double growth_ratio(double current, double previous) {
    return (std::isnan(current) || std::isnan(previous) || previous <= 0.0) ? qQNaN()
                                                                           : (current - previous) / previous;
}

} // namespace

void EquityFinancialsTab::on_financials_loaded(services::equity::FinancialsData payload) {
    if (payload.symbol != current_symbol_)
        return;
    cached_data_ = payload;
    loaded_ = true;
    loading_overlay_->hide_loading();

    populate_income_view(payload);
    populate_balance_view(payload);
    populate_cashflow_view(payload);
    populate_table(inc_table_, payload.income_statement);
    populate_table(bal_table_, payload.balance_sheet);
    populate_table(cf_table_, payload.cash_flow);
    rebuild_revenue_chart(payload);
    rebuild_margin_chart(payload);
    rebuild_balance_chart(payload);
    rebuild_cashflow_chart(payload);
    rebuild_return_chart(payload);
}

// ── Populate helpers ──────────────────────────────────────────────────────────

// Returns NaN — not 0.0 — when the filing carries none of `keys`. A 0.0 miss was
// indistinguishable from a reported zero, and every caller then divided by it,
// charted it, or labelled it as if it were a real figure.
double EquityFinancialsTab::get_val(const QJsonObject& o, const QStringList& keys) {
    bool reported_zero = false;
    for (const auto& k : keys) {
        if (!o.contains(k) || o[k].isNull())
            continue;
        bool ok = false;
        const double v = o[k].toVariant().toDouble(&ok);
        if (!ok)
            continue;
        if (v != 0.0)
            return v;
        // A line item that IS present and reports 0 is data. Keep scanning the
        // aliases for a non-zero reading first, then fall back to that zero.
        reported_zero = true;
    }
    return reported_zero ? 0.0 : qQNaN();
}

void EquityFinancialsTab::populate_income_view(const services::equity::FinancialsData& d) {
    if (d.income_statement.isEmpty())
        return;

    const auto& latest = d.income_statement[0].second;
    const auto& prev = d.income_statement.size() > 1 ? d.income_statement[1].second : QJsonObject();

    double revenue = get_val(latest, {"Total Revenue", "Revenue"});
    double prev_rev = get_val(prev, {"Total Revenue", "Revenue"});
    double gross = get_val(latest, {"Gross Profit"});
    double op_income = get_val(latest, {"Operating Income", "Operating Profit"});
    double net_income = get_val(latest, {"Net Income", "Net Income Common Stockholders"});
    double prev_net = get_val(prev, {"Net Income", "Net Income Common Stockholders"});
    double ebitda = get_val(latest, {"EBITDA", "Normalized EBITDA"});

    // Margins — NaN when revenue is absent. Never substitute a denominator.
    double gross_margin = pos_ratio(gross, revenue);
    double op_margin = pos_ratio(op_income, revenue);
    double net_margin = pos_ratio(net_income, revenue);
    double ebitda_margin = pos_ratio(ebitda, revenue);

    // Growth labels
    double rev_growth = growth_ratio(revenue, prev_rev);
    double net_growth = growth_ratio(net_income, prev_net);

    auto set = [](QLabel* l, const QString& t) {
        if (l)
            l->setText(t);
    };

    set(inc_revenue_val_, missing(revenue) ? kNoData : fmt_large(revenue));
    set(inc_revenue_sub_, missing(rev_growth) ? QString() : tr("%1 YoY").arg(fmt_pct(rev_growth)));
    set(inc_gross_val_, missing(gross) ? kNoData : fmt_large(gross));
    set(inc_gross_sub_, missing(gross_margin) ? QString() : tr("%1 margin").arg(fmt_pct(gross_margin)));
    set(inc_opincome_val_, missing(op_income) ? kNoData : fmt_large(op_income));
    set(inc_opincome_sub_, missing(op_margin) ? QString() : tr("%1 margin").arg(fmt_pct(op_margin)));
    set(inc_netincome_val_, missing(net_income) ? kNoData : fmt_large(net_income));
    set(inc_netincome_sub_, missing(net_growth) ? QString() : tr("%1 YoY").arg(fmt_pct(net_growth)));
    set(inc_ebitda_val_, missing(ebitda) ? kNoData : fmt_large(ebitda));
    set(inc_ebitda_sub_, missing(ebitda_margin) ? QString() : tr("%1 margin").arg(fmt_pct(ebitda_margin)));
    set(inc_gross_margin_, missing(gross_margin) ? kNoData : fmt_pct(gross_margin));
    set(inc_op_margin_, missing(op_margin) ? kNoData : fmt_pct(op_margin));
    set(inc_net_margin_, missing(net_margin) ? kNoData : fmt_pct(net_margin));
    set(inc_ebitda_margin_, missing(ebitda_margin) ? kNoData : fmt_pct(ebitda_margin));

    // DuPont (needs balance sheet too)
    if (!d.balance_sheet.isEmpty()) {
        const auto& bal = d.balance_sheet[0].second;
        double total_assets = get_val(bal, {"Total Assets"});
        double total_equity = get_val(bal, {"Stockholders Equity", "Total Equity", "Total Stockholder Equity"});
        double asset_turnover = pos_ratio(revenue, total_assets);
        double eq_mult = pos_ratio(total_assets, total_equity);
        double roe = pos_ratio(net_income, total_equity);
        double roa = pos_ratio(net_income, total_assets);

        set(dupont_net_margin_, missing(net_margin) ? kNoData : fmt_pct(net_margin));
        set(dupont_asset_turn_, missing(asset_turnover) ? kNoData : fmt_ratio(asset_turnover) + QStringLiteral("x"));
        set(dupont_eq_mult_, missing(eq_mult) ? kNoData : fmt_ratio(eq_mult) + QStringLiteral("x"));
        set(dupont_roe_result_, missing(roe) ? kNoData : fmt_pct(roe));

        // Invested capital for ROIC — NaN-poisoned if any leg is absent, so a
        // partial balance sheet reports "no data" rather than an ROIC computed
        // off a phantom zero cash / zero debt balance.
        double total_debt = get_val(bal, {"Total Debt", "Long Term Debt"});
        double cash = get_val(bal, {"Cash And Cash Equivalents", "Cash"});
        double inv_cap = total_equity + total_debt - cash;
        double roic = pos_ratio(op_income * 0.75, inv_cap);
        double cur_liab = get_val(bal, {"Current Liabilities", "Total Current Liabilities"});
        double roce = pos_ratio(op_income, total_assets - cur_liab);

        set(ret_roe_val_, missing(roe) ? kNoData : fmt_pct(roe));
        set(ret_roa_val_, missing(roa) ? kNoData : fmt_pct(roa));
        set(ret_roic_val_, missing(roic) ? kNoData : fmt_pct(roic));
        set(ret_roce_val_, missing(roce) ? kNoData : fmt_pct(roce));
    }
}

void EquityFinancialsTab::populate_balance_view(const services::equity::FinancialsData& d) {
    if (d.balance_sheet.isEmpty())
        return;

    const auto& b = d.balance_sheet[0].second;
    double total_assets = get_val(b, {"Total Assets"});
    double total_liab = get_val(b, {"Total Liabilities Net Minority Interest", "Total Liabilities"});
    double total_equity = get_val(b, {"Stockholders Equity", "Total Equity", "Total Stockholder Equity"});
    double total_debt = get_val(b, {"Total Debt"});
    double cash = get_val(b, {"Cash And Cash Equivalents", "Cash"});
    double cur_assets = get_val(b, {"Current Assets", "Total Current Assets"});
    double cur_liab = get_val(b, {"Current Liabilities", "Total Current Liabilities"});
    double inventory = get_val(b, {"Inventory"});
    // No income statement means no EBITDA / interest reading at all — not zero.
    double ebitda = qQNaN();
    double interest = qQNaN();
    if (!d.income_statement.isEmpty()) {
        const auto& inc = d.income_statement[0].second;
        ebitda = get_val(inc, {"EBITDA", "Normalized EBITDA"});
        interest = get_val(inc, {"Interest Expense", "Interest Expense Non Operating"});
        if (interest < 0)
            interest = -interest;
    }

    auto set = [](QLabel* l, const QString& t) {
        if (l)
            l->setText(t);
    };

    set(bal_assets_val_, missing(total_assets) ? kNoData : fmt_large(total_assets));
    set(bal_liabilities_val_, missing(total_liab) ? kNoData : fmt_large(total_liab));
    set(bal_equity_val_, missing(total_equity) ? kNoData : fmt_large(total_equity));
    set(bal_debt_val_, missing(total_debt) ? kNoData : fmt_large(total_debt));
    set(bal_cash_val_, missing(cash) ? kNoData : fmt_large(cash));

    double cur_ratio = pos_ratio(cur_assets, cur_liab);
    double quick_ratio = pos_ratio(cur_assets - inventory, cur_liab);
    double work_cap = cur_assets - cur_liab;
    double d_e = pos_ratio(total_debt, total_equity);
    double d_a = pos_ratio(total_debt, total_assets);
    double int_cov = pos_ratio(ebitda, interest);

    set(bal_current_ratio_, missing(cur_ratio) ? kNoData : fmt_ratio(cur_ratio));
    set(bal_quick_ratio_, missing(quick_ratio) ? kNoData : fmt_ratio(quick_ratio));
    set(bal_working_cap_, missing(work_cap) ? kNoData : fmt_large(work_cap));
    set(bal_debt_equity_, missing(d_e) ? kNoData : fmt_ratio(d_e));
    set(bal_debt_assets_, missing(d_a) ? kNoData : fmt_pct(d_a));
    set(bal_int_coverage_, missing(int_cov) ? tr("N/A") : fmt_ratio(int_cov) + QStringLiteral("x"));
}

void EquityFinancialsTab::populate_cashflow_view(const services::equity::FinancialsData& d) {
    if (d.cash_flow.isEmpty())
        return;

    const auto& cf = d.cash_flow[0].second;
    double op_cf = get_val(cf, {"Operating Cash Flow", "Total Cash From Operating Activities"});
    double inv_cf = get_val(cf, {"Investing Cash Flow", "Total Cash From Investing Activities"});
    double fin_cf = get_val(cf, {"Financing Cash Flow", "Total Cash From Financing Activities"});
    double capex = get_val(cf, {"Capital Expenditure", "Capital Expenditures"});
    if (capex > 0)
        capex = -capex;         // capex is usually negative
    double fcf = op_cf + capex; // capex is negative so this subtracts
    double dividends = get_val(cf, {"Cash Dividends Paid", "Payment Of Dividends"});
    if (dividends > 0)
        dividends = -dividends;
    double buybacks = get_val(
        cf, {"Repurchase Of Capital Stock", "Common Stock Repurchased", "Common Stock Payments", "Purchase Of Stock"});
    if (buybacks > 0)
        buybacks = -buybacks;

    double revenue = qQNaN();
    if (!d.income_statement.isEmpty())
        revenue = get_val(d.income_statement[0].second, {"Total Revenue", "Revenue"});
    double fcf_margin = pos_ratio(fcf, revenue);
    double capex_rev = pos_ratio(qAbs(capex), revenue);

    auto set = [](QLabel* l, const QString& t) {
        if (l)
            l->setText(t);
    };

    set(cf_operating_val_, missing(op_cf) ? kNoData : fmt_large(op_cf));
    set(cf_investing_val_, missing(inv_cf) ? kNoData : fmt_large(inv_cf));
    set(cf_financing_val_, missing(fin_cf) ? kNoData : fmt_large(fin_cf));
    set(cf_fcf_val_, missing(fcf) ? kNoData : fmt_large(fcf));
    set(cf_fcf_sub_, missing(fcf_margin) ? QString() : tr("%1 margin").arg(fmt_pct(fcf_margin)));
    set(cf_capex_val_, missing(capex) ? kNoData : fmt_large(qAbs(capex)));
    set(cf_dividends_val_, missing(dividends) ? kNoData : fmt_large(qAbs(dividends)));
    set(cf_buybacks_val_, missing(buybacks) ? kNoData : fmt_large(qAbs(buybacks)));
    set(cf_fcf_margin_, missing(fcf_margin) ? kNoData : fmt_pct(fcf_margin));
    set(cf_capex_rev_, missing(capex_rev) ? kNoData : fmt_pct(capex_rev));
}

// ── Chart builders ────────────────────────────────────────────────────────────

static void style_chart(QChart* chart) {
    chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_SURFACE())));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart->setPlotAreaBackgroundVisible(true);
    chart->legend()->setLabelColor(QColor(ui::colors::TEXT_SECONDARY()));
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->setMargins(QMargins(4, 4, 4, 8));
}

static QBarCategoryAxis* make_x_axis(const QStringList& cats) {
    auto* ax = new QBarCategoryAxis;
    ax->append(cats);
    ax->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    ax->setLabelsFont(QFont("monospace", 8));
    ax->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    return ax;
}

static QValueAxis* make_y_axis(const QString& fmt = "%.1fB") {
    auto* ay = new QValueAxis;
    ay->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    ay->setLabelsFont(QFont("monospace", 8));
    ay->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    ay->setLabelFormat(fmt);
    return ay;
}

void EquityFinancialsTab::rebuild_revenue_chart(const services::equity::FinancialsData& d) {
    if (d.income_statement.isEmpty() || !inc_revenue_chart_)
        return;

    // Up to 5 periods, reversed (oldest first). A period with no reported
    // revenue is dropped outright: QBarSet has no "missing" slot, so a bar
    // plotted at zero would assert the company earned nothing that year.
    int n = qMin(5, d.income_statement.size());
    QStringList cats;
    QVector<double> revenue_v, gross_v, net_v;

    for (int i = n - 1; i >= 0; --i) {
        const auto& stmt = d.income_statement[i].second;
        const double rev = get_val(stmt, {"Total Revenue", "Revenue"});
        if (missing(rev))
            continue;
        cats << d.income_statement[i].first.left(4);
        revenue_v << rev / 1e9;
        gross_v << get_val(stmt, {"Gross Profit"}) / 1e9;
        net_v << get_val(stmt, {"Net Income", "Net Income Common Stockholders"}) / 1e9;
    }

    auto* chart = new QChart;
    style_chart(chart);
    if (cats.isEmpty()) {
        // Nothing reported — an empty plot, not a chart of zeroes.
        inc_revenue_chart_->setChart(chart);
        return;
    }

    auto* rev_set = new QBarSet(tr("Revenue"));
    rev_set->setColor(QColor(kBlue));
    for (double v : revenue_v)
        *rev_set << v;

    auto* bar_series = new QBarSeries;
    bar_series->append(rev_set);

    // Gross profit is charted only when every plotted period reports it. Banks
    // and insurers carry no "Gross Profit" line at all; a zero-height bar there
    // would be a claim, not a gap.
    if (std::none_of(gross_v.cbegin(), gross_v.cend(), missing)) {
        auto* gross_set = new QBarSet(tr("Gross Profit"));
        gross_set->setColor(QColor(kCyan));
        for (double v : gross_v)
            *gross_set << v;
        bar_series->append(gross_set);
    }

    auto* net_series = new QLineSeries;
    net_series->setName(tr("Net Income"));
    net_series->setColor(QColor(kGreen));
    net_series->setPen(QPen(QColor(kGreen), 2));
    for (int i = 0; i < cats.size(); ++i)
        if (!missing(net_v[i]))
            net_series->append(i, net_v[i]);

    chart->addSeries(bar_series);
    chart->addSeries(net_series);
    chart->legend()->show();

    auto* axX = make_x_axis(cats);
    auto* axY = make_y_axis("$%.0fB");
    chart->addAxis(axX, Qt::AlignBottom);
    chart->addAxis(axY, Qt::AlignLeft);
    bar_series->attachAxis(axX);
    bar_series->attachAxis(axY);
    net_series->attachAxis(axX);
    net_series->attachAxis(axY);

    inc_revenue_chart_->setChart(chart);
}

void EquityFinancialsTab::rebuild_margin_chart(const services::equity::FinancialsData& d) {
    if (d.income_statement.isEmpty() || !inc_margin_chart_)
        return;

    int n = qMin(5, d.income_statement.size());
    QStringList cats;
    QVector<double> gross_v, op_v, net_v, ebitda_v;

    for (int i = n - 1; i >= 0; --i) {
        const auto& stmt = d.income_statement[i].second;
        cats << d.income_statement[i].first.left(4);
        // The denominator is never invented. `if (rev == 0.0) rev = 1.0;` used
        // to turn a missing revenue line into a divide-by-one, plotting raw
        // dollars × 100 — margins in the billions of percent.
        const double rev = get_val(stmt, {"Total Revenue", "Revenue"});
        gross_v << pos_ratio(get_val(stmt, {"Gross Profit"}), rev) * 100.0;
        op_v << pos_ratio(get_val(stmt, {"Operating Income", "Operating Profit"}), rev) * 100.0;
        net_v << pos_ratio(get_val(stmt, {"Net Income", "Net Income Common Stockholders"}), rev) * 100.0;
        ebitda_v << pos_ratio(get_val(stmt, {"EBITDA", "Normalized EBITDA"}), rev) * 100.0;
    }

    // Line series carry gaps natively: an uncomputable period is simply not
    // appended rather than being pinned to the axis at 0%.
    auto make_line = [&](const QString& name, const QString& color, const QVector<double>& vals) {
        auto* s = new QLineSeries;
        s->setName(name);
        s->setColor(QColor(color));
        s->setPen(QPen(QColor(color), 2));
        for (int i = 0; i < cats.size(); ++i)
            if (!missing(vals[i]))
                s->append(i, vals[i]);
        return s;
    };

    auto* chart = new QChart;
    style_chart(chart);
    auto* s1 = make_line(tr("Gross"), kGreen, gross_v);
    auto* s2 = make_line(tr("Operating"), kCyan, op_v);
    auto* s3 = make_line(tr("Net"), kOrange, net_v);
    auto* s4 = make_line(tr("EBITDA"), kPurple, ebitda_v);
    chart->addSeries(s1);
    chart->addSeries(s2);
    chart->addSeries(s3);
    chart->addSeries(s4);
    chart->legend()->show();

    auto* axX = make_x_axis(cats);
    auto* axY = make_y_axis("%.0f%%");
    chart->addAxis(axX, Qt::AlignBottom);
    chart->addAxis(axY, Qt::AlignLeft);
    for (auto* s : {s1, s2, s3, s4}) {
        s->attachAxis(axX);
        s->attachAxis(axY);
    }

    inc_margin_chart_->setChart(chart);
}

void EquityFinancialsTab::rebuild_balance_chart(const services::equity::FinancialsData& d) {
    if (d.balance_sheet.isEmpty() || !bal_chart_)
        return;

    // Periods are driven by total assets; a period without it is dropped rather
    // than drawn as a zero-height stack.
    int n = qMin(5, d.balance_sheet.size());
    QStringList cats;
    QVector<double> assets_v, liab_v, equity_v;

    for (int i = n - 1; i >= 0; --i) {
        const auto& b = d.balance_sheet[i].second;
        const double assets = get_val(b, {"Total Assets"});
        if (missing(assets))
            continue;
        cats << d.balance_sheet[i].first.left(4);
        assets_v << assets / 1e9;
        liab_v << get_val(b, {"Total Liabilities Net Minority Interest", "Total Liabilities"}) / 1e9;
        equity_v << get_val(b, {"Stockholders Equity", "Total Equity"}) / 1e9;
    }

    auto* chart = new QChart;
    style_chart(chart);
    if (cats.isEmpty()) {
        bal_chart_->setChart(chart);
        return;
    }

    auto* series = new QBarSeries;
    auto add_set = [&](const QString& name, const QString& color, const QVector<double>& vals) {
        // Only complete series are plotted — a bar set cannot express a gap.
        if (std::any_of(vals.cbegin(), vals.cend(), missing))
            return;
        auto* set = new QBarSet(name);
        set->setColor(QColor(color));
        for (double v : vals)
            *set << v;
        series->append(set);
    };
    add_set(tr("Assets"), kBlue, assets_v);
    add_set(tr("Liabilities"), kRed, liab_v);
    add_set(tr("Equity"), kGreen, equity_v);

    chart->addSeries(series);
    chart->legend()->show();

    auto* axX = make_x_axis(cats);
    auto* axY = make_y_axis("$%.0fB");
    chart->addAxis(axX, Qt::AlignBottom);
    chart->addAxis(axY, Qt::AlignLeft);
    series->attachAxis(axX);
    series->attachAxis(axY);

    bal_chart_->setChart(chart);
}

void EquityFinancialsTab::rebuild_cashflow_chart(const services::equity::FinancialsData& d) {
    if (d.cash_flow.isEmpty() || !cf_chart_)
        return;

    // Periods are driven by operating cash flow; a period without it is dropped.
    int n = qMin(5, d.cash_flow.size());
    QStringList cats;
    QVector<double> op_v, inv_v, fin_v, fcf_v;

    for (int i = n - 1; i >= 0; --i) {
        const auto& cf = d.cash_flow[i].second;
        const double op = get_val(cf, {"Operating Cash Flow", "Total Cash From Operating Activities"});
        if (missing(op))
            continue;
        double capex = get_val(cf, {"Capital Expenditure", "Capital Expenditures"});
        if (capex > 0)
            capex = -capex;
        cats << d.cash_flow[i].first.left(4);
        op_v << op / 1e9;
        inv_v << get_val(cf, {"Investing Cash Flow", "Total Cash From Investing Activities"}) / 1e9;
        fin_v << get_val(cf, {"Financing Cash Flow", "Total Cash From Financing Activities"}) / 1e9;
        // No capex line means free cash flow is unknown, not equal to op cash flow.
        fcf_v << (op + capex) / 1e9;
    }

    auto* chart = new QChart;
    style_chart(chart);
    if (cats.isEmpty()) {
        cf_chart_->setChart(chart);
        return;
    }

    auto* bar_series = new QBarSeries;
    auto add_set = [&](const QString& name, const QString& color, const QVector<double>& vals) {
        if (std::any_of(vals.cbegin(), vals.cend(), missing))
            return;
        auto* set = new QBarSet(name);
        set->setColor(QColor(color));
        for (double v : vals)
            *set << v;
        bar_series->append(set);
    };
    add_set(tr("Operating"), kGreen, op_v);
    add_set(tr("Investing"), kOrange, inv_v);
    add_set(tr("Financing"), kPurple, fin_v);

    auto* fcf_series = new QLineSeries;
    fcf_series->setName(tr("Free CF"));
    fcf_series->setColor(QColor(kCyan));
    fcf_series->setPen(QPen(QColor(kCyan), 2));
    for (int i = 0; i < cats.size(); ++i)
        if (!missing(fcf_v[i]))
            fcf_series->append(i, fcf_v[i]);

    chart->addSeries(bar_series);
    chart->addSeries(fcf_series);
    chart->legend()->show();

    auto* axX = make_x_axis(cats);
    auto* axY = make_y_axis("$%.0fB");
    chart->addAxis(axX, Qt::AlignBottom);
    chart->addAxis(axY, Qt::AlignLeft);
    bar_series->attachAxis(axX);
    bar_series->attachAxis(axY);
    fcf_series->attachAxis(axX);
    fcf_series->attachAxis(axY);

    cf_chart_->setChart(chart);
}

void EquityFinancialsTab::rebuild_return_chart(const services::equity::FinancialsData& d) {
    if (d.income_statement.isEmpty() || d.balance_sheet.isEmpty() || !ret_chart_)
        return;

    int n = qMin(5, qMin(d.income_statement.size(), d.balance_sheet.size()));
    QStringList cats;

    auto* roe_series = new QLineSeries;
    roe_series->setName(tr("ROE %"));
    roe_series->setColor(QColor(kCyan));
    roe_series->setPen(QPen(QColor(kCyan), 2));

    auto* roa_series = new QLineSeries;
    roa_series->setName(tr("ROA %"));
    roa_series->setColor(QColor(kGreen));
    roa_series->setPen(QPen(QColor(kGreen), 2));

    for (int i = n - 1; i >= 0; --i) {
        cats << d.income_statement[i].first.left(4);
        double net = get_val(d.income_statement[i].second, {"Net Income", "Net Income Common Stockholders"});
        double assets = get_val(d.balance_sheet[i].second, {"Total Assets"});
        double equity = get_val(d.balance_sheet[i].second, {"Stockholders Equity", "Total Equity"});
        // An uncomputable period is skipped, not plotted at 0% — a flat zero
        // return is a claim the filings never made.
        const double roe = pos_ratio(net, equity) * 100.0;
        const double roa = pos_ratio(net, assets) * 100.0;
        if (!missing(roe))
            roe_series->append(cats.size() - 1, roe);
        if (!missing(roa))
            roa_series->append(cats.size() - 1, roa);
    }

    auto* chart = new QChart;
    style_chart(chart);
    chart->addSeries(roe_series);
    chart->addSeries(roa_series);
    chart->legend()->show();

    auto* axX = make_x_axis(cats);
    auto* axY = make_y_axis("%.0f%%");
    chart->addAxis(axX, Qt::AlignBottom);
    chart->addAxis(axY, Qt::AlignLeft);
    roe_series->attachAxis(axX);
    roe_series->attachAxis(axY);
    roa_series->attachAxis(axX);
    roa_series->attachAxis(axY);

    ret_chart_->setChart(chart);
}

// ── Raw table ─────────────────────────────────────────────────────────────────
} // namespace fincept::screens
