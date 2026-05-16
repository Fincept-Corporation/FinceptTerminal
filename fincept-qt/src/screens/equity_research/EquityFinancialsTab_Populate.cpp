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
#include <QTableWidget>
#include <QTabWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <QValueAxis>

namespace fincept::screens {

using namespace financials_internal;


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

double EquityFinancialsTab::get_val(const QJsonObject& o, const QStringList& keys) {
    for (const auto& k : keys) {
        if (o.contains(k) && !o[k].isNull()) {
            double v = o[k].toVariant().toDouble();
            if (v != 0.0)
                return v;
        }
    }
    return 0.0;
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

    // Margins
    double gross_margin = revenue > 0 ? gross / revenue : 0.0;
    double op_margin = revenue > 0 ? op_income / revenue : 0.0;
    double net_margin = revenue > 0 ? net_income / revenue : 0.0;
    double ebitda_margin = revenue > 0 ? ebitda / revenue : 0.0;

    // Growth labels
    double rev_growth = prev_rev > 0 ? (revenue - prev_rev) / prev_rev : 0.0;
    double net_growth = prev_net > 0 ? (net_income - prev_net) / prev_net : 0.0;

    auto set = [](QLabel* l, const QString& t) {
        if (l)
            l->setText(t);
    };

    set(inc_revenue_val_, fmt_large(revenue));
    set(inc_revenue_sub_, rev_growth != 0.0 ? fmt_pct(rev_growth) + " YoY" : "");
    set(inc_gross_val_, fmt_large(gross));
    set(inc_gross_sub_, fmt_pct(gross_margin) + " margin");
    set(inc_opincome_val_, fmt_large(op_income));
    set(inc_opincome_sub_, fmt_pct(op_margin) + " margin");
    set(inc_netincome_val_, fmt_large(net_income));
    set(inc_netincome_sub_, net_growth != 0.0 ? fmt_pct(net_growth) + " YoY" : "");
    set(inc_ebitda_val_, fmt_large(ebitda));
    set(inc_ebitda_sub_, fmt_pct(ebitda_margin) + " margin");
    set(inc_gross_margin_, fmt_pct(gross_margin));
    set(inc_op_margin_, fmt_pct(op_margin));
    set(inc_net_margin_, fmt_pct(net_margin));
    set(inc_ebitda_margin_, fmt_pct(ebitda_margin));

    // DuPont (needs balance sheet too)
    if (!d.balance_sheet.isEmpty()) {
        const auto& bal = d.balance_sheet[0].second;
        double total_assets = get_val(bal, {"Total Assets"});
        double total_equity = get_val(bal, {"Stockholders Equity", "Total Equity", "Total Stockholder Equity"});
        double asset_turnover = total_assets > 0 ? revenue / total_assets : 0.0;
        double eq_mult = total_equity > 0 ? total_assets / total_equity : 0.0;
        double roe = total_equity > 0 ? net_income / total_equity : 0.0;
        double roa = total_assets > 0 ? net_income / total_assets : 0.0;

        set(dupont_net_margin_, fmt_pct(net_margin));
        set(dupont_asset_turn_, QString::number(asset_turnover, 'f', 2) + "x");
        set(dupont_eq_mult_, QString::number(eq_mult, 'f', 2) + "x");
        set(dupont_roe_result_, fmt_pct(roe));

        // Invested capital for ROIC
        double total_debt = get_val(bal, {"Total Debt", "Long Term Debt"});
        double cash = get_val(bal, {"Cash And Cash Equivalents", "Cash"});
        double inv_cap = total_equity + total_debt - cash;
        double roic = inv_cap > 0 ? (op_income * 0.75) / inv_cap : 0.0;
        double cur_liab = get_val(bal, {"Current Liabilities", "Total Current Liabilities"});
        double roce = (total_assets - cur_liab) > 0 ? op_income / (total_assets - cur_liab) : 0.0;

        set(ret_roe_val_, fmt_pct(roe));
        set(ret_roa_val_, fmt_pct(roa));
        set(ret_roic_val_, fmt_pct(roic));
        set(ret_roce_val_, fmt_pct(roce));
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
    double ebitda = 0.0;
    double interest = 0.0;
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

    set(bal_assets_val_, fmt_large(total_assets));
    set(bal_liabilities_val_, fmt_large(total_liab));
    set(bal_equity_val_, fmt_large(total_equity));
    set(bal_debt_val_, fmt_large(total_debt));
    set(bal_cash_val_, fmt_large(cash));

    double cur_ratio = cur_liab > 0 ? cur_assets / cur_liab : 0.0;
    double quick_ratio = cur_liab > 0 ? (cur_assets - inventory) / cur_liab : 0.0;
    double work_cap = cur_assets - cur_liab;
    double d_e = total_equity > 0 ? total_debt / total_equity : 0.0;
    double d_a = total_assets > 0 ? total_debt / total_assets : 0.0;
    double int_cov = interest > 0 ? ebitda / interest : 0.0;

    set(bal_current_ratio_, fmt_ratio(cur_ratio));
    set(bal_quick_ratio_, fmt_ratio(quick_ratio));
    set(bal_working_cap_, fmt_large(work_cap));
    set(bal_debt_equity_, fmt_ratio(d_e));
    set(bal_debt_assets_, fmt_pct(d_a));
    set(bal_int_coverage_, int_cov > 0 ? fmt_ratio(int_cov) + "x" : "N/A");
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

    double revenue = 0.0;
    if (!d.income_statement.isEmpty())
        revenue = get_val(d.income_statement[0].second, {"Total Revenue", "Revenue"});
    double fcf_margin = revenue > 0 ? fcf / revenue : 0.0;
    double capex_rev = revenue > 0 ? qAbs(capex) / revenue : 0.0;

    auto set = [](QLabel* l, const QString& t) {
        if (l)
            l->setText(t);
    };

    set(cf_operating_val_, fmt_large(op_cf));
    set(cf_investing_val_, fmt_large(inv_cf));
    set(cf_financing_val_, fmt_large(fin_cf));
    set(cf_fcf_val_, fmt_large(fcf));
    set(cf_fcf_sub_, fcf_margin != 0.0 ? fmt_pct(fcf_margin) + " margin" : "");
    set(cf_capex_val_, fmt_large(qAbs(capex)));
    set(cf_dividends_val_, fmt_large(qAbs(dividends)));
    set(cf_buybacks_val_, fmt_large(qAbs(buybacks)));
    set(cf_fcf_margin_, fmt_pct(fcf_margin));
    set(cf_capex_rev_, fmt_pct(capex_rev));
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

    // Up to 5 periods, reversed (oldest first)
    int n = qMin(5, d.income_statement.size());
    QStringList cats;
    QVector<double> revenue_v, gross_v, net_v;

    for (int i = n - 1; i >= 0; --i) {
        cats << d.income_statement[i].first.left(4);
        revenue_v << get_val(d.income_statement[i].second, {"Total Revenue", "Revenue"}) / 1e9;
        gross_v << get_val(d.income_statement[i].second, {"Gross Profit"}) / 1e9;
        net_v << get_val(d.income_statement[i].second, {"Net Income", "Net Income Common Stockholders"}) / 1e9;
    }

    auto* rev_set = new QBarSet("Revenue");
    rev_set->setColor(QColor(kBlue));
    auto* gross_set = new QBarSet("Gross Profit");
    gross_set->setColor(QColor(kCyan));
    for (int i = 0; i < cats.size(); ++i) {
        *rev_set << revenue_v[i];
        *gross_set << gross_v[i];
    }

    auto* bar_series = new QBarSeries;
    bar_series->append(rev_set);
    bar_series->append(gross_set);

    auto* net_series = new QLineSeries;
    net_series->setName("Net Income");
    net_series->setColor(QColor(kGreen));
    net_series->setPen(QPen(QColor(kGreen), 2));
    for (int i = 0; i < cats.size(); ++i)
        net_series->append(i, net_v[i]);

    auto* chart = new QChart;
    style_chart(chart);
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
        double rev = get_val(stmt, {"Total Revenue", "Revenue"});
        if (rev == 0.0)
            rev = 1.0;
        gross_v << get_val(stmt, {"Gross Profit"}) / rev * 100.0;
        op_v << get_val(stmt, {"Operating Income", "Operating Profit"}) / rev * 100.0;
        net_v << get_val(stmt, {"Net Income", "Net Income Common Stockholders"}) / rev * 100.0;
        ebitda_v << get_val(stmt, {"EBITDA", "Normalized EBITDA"}) / rev * 100.0;
    }

    auto make_line = [&](const QString& name, const QString& color, const QVector<double>& vals) {
        auto* s = new QLineSeries;
        s->setName(name);
        s->setColor(QColor(color));
        s->setPen(QPen(QColor(color), 2));
        for (int i = 0; i < cats.size(); ++i)
            s->append(i, vals[i]);
        return s;
    };

    auto* chart = new QChart;
    style_chart(chart);
    auto* s1 = make_line("Gross", kGreen, gross_v);
    auto* s2 = make_line("Operating", kCyan, op_v);
    auto* s3 = make_line("Net", kOrange, net_v);
    auto* s4 = make_line("EBITDA", kPurple, ebitda_v);
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

    int n = qMin(5, d.balance_sheet.size());
    QStringList cats;

    auto* assets_set = new QBarSet("Assets");
    assets_set->setColor(QColor(kBlue));
    auto* liab_set = new QBarSet("Liabilities");
    liab_set->setColor(QColor(kRed));
    auto* equity_set = new QBarSet("Equity");
    equity_set->setColor(QColor(kGreen));

    for (int i = n - 1; i >= 0; --i) {
        cats << d.balance_sheet[i].first.left(4);
        const auto& b = d.balance_sheet[i].second;
        *assets_set << get_val(b, {"Total Assets"}) / 1e9;
        *liab_set << get_val(b, {"Total Liabilities Net Minority Interest", "Total Liabilities"}) / 1e9;
        *equity_set << get_val(b, {"Stockholders Equity", "Total Equity"}) / 1e9;
    }

    auto* series = new QBarSeries;
    series->append(assets_set);
    series->append(liab_set);
    series->append(equity_set);

    auto* chart = new QChart;
    style_chart(chart);
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

    int n = qMin(5, d.cash_flow.size());
    QStringList cats;

    auto* op_set = new QBarSet("Operating");
    op_set->setColor(QColor(kGreen));
    auto* inv_set = new QBarSet("Investing");
    inv_set->setColor(QColor(kOrange));
    auto* fin_set = new QBarSet("Financing");
    fin_set->setColor(QColor(kPurple));

    auto* fcf_series = new QLineSeries;
    fcf_series->setName("Free CF");
    fcf_series->setColor(QColor(kCyan));
    fcf_series->setPen(QPen(QColor(kCyan), 2));

    for (int i = n - 1; i >= 0; --i) {
        cats << d.cash_flow[i].first.left(4);
        const auto& cf = d.cash_flow[i].second;
        double op = get_val(cf, {"Operating Cash Flow", "Total Cash From Operating Activities"});
        double inv = get_val(cf, {"Investing Cash Flow", "Total Cash From Investing Activities"});
        double fin = get_val(cf, {"Financing Cash Flow", "Total Cash From Financing Activities"});
        double capex = get_val(cf, {"Capital Expenditure", "Capital Expenditures"});
        if (capex > 0)
            capex = -capex;
        *op_set << op / 1e9;
        *inv_set << inv / 1e9;
        *fin_set << fin / 1e9;
        fcf_series->append(cats.size() - 1, (op + capex) / 1e9);
    }

    auto* bar_series = new QBarSeries;
    bar_series->append(op_set);
    bar_series->append(inv_set);
    bar_series->append(fin_set);

    auto* chart = new QChart;
    style_chart(chart);
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
    roe_series->setName("ROE %");
    roe_series->setColor(QColor(kCyan));
    roe_series->setPen(QPen(QColor(kCyan), 2));

    auto* roa_series = new QLineSeries;
    roa_series->setName("ROA %");
    roa_series->setColor(QColor(kGreen));
    roa_series->setPen(QPen(QColor(kGreen), 2));

    for (int i = n - 1; i >= 0; --i) {
        cats << d.income_statement[i].first.left(4);
        double net = get_val(d.income_statement[i].second, {"Net Income", "Net Income Common Stockholders"});
        double assets = get_val(d.balance_sheet[i].second, {"Total Assets"});
        double equity = get_val(d.balance_sheet[i].second, {"Stockholders Equity", "Total Equity"});
        roe_series->append(cats.size() - 1, equity > 0 ? (net / equity) * 100.0 : 0.0);
        roa_series->append(cats.size() - 1, assets > 0 ? (net / assets) * 100.0 : 0.0);
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
