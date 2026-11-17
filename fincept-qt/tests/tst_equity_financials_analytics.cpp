// tests/tst_equity_financials_analytics.cpp
//
// fin_an (src/screens/equity_research/EquityFinancialsAnalytics) against hand-computed fixtures:
// an industrial company (three fiscal years + quarters → annual, quarterly and TTM), a bank, a
// loss-making company and a filing with most lines missing. Every expected number below is
// worked out by hand from the fixture in the comment next to it. QtCore only.
#include "screens/equity_research/EquityFinancialsAnalytics.h"

#include <QJsonObject>
#include <QtTest/QtTest>

#include <cmath>
#include <initializer_list>
#include <utility>

using namespace fincept::screens;
using fincept::services::equity::FinancialsData;
using KV = std::initializer_list<std::pair<const char*, double>>;

namespace {

QJsonObject fx_obj(KV kv) {
    QJsonObject o;
    for (const auto& [k, v] : kv)
        o.insert(QLatin1String(k), v);
    return o;
}

void fx_add(QVector<QPair<QString, QJsonObject>>& stmt, const char* date, KV kv) {
    stmt.prepend({QString::fromLatin1(date) + QStringLiteral(" 00:00:00"), fx_obj(kv)}); // newest first, like the service
}

bool fx_near(double a, double b, double tol = 1e-5) {
    return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) <= tol * std::max(1.0, std::fabs(b));
}

/// ACME: an industrial company, December year end, FY2022–FY2024 + quarters to Mar 2025.
FinancialsData fx_acme() {
    FinancialsData fd;
    fd.symbol = QStringLiteral("ACME");
    fx_add(fd.income_statement, "2022-12-31",
           {{"Total Revenue", 1000}, {"Cost Of Revenue", 600}, {"Gross Profit", 400}, {"Research And Development", 50},
            {"Selling General And Administration", 150}, {"Operating Income", 200}, {"Interest Expense", 20},
            {"Pretax Income", 180}, {"Tax Provision", 36}, {"Net Income", 144}, {"Net Income Common Stockholders", 144},
            {"Diluted EPS", 1.44}, {"Diluted Average Shares", 100}, {"EBITDA", 260}, {"EBIT", 200}});
    fx_add(fd.income_statement, "2023-12-31",
           {{"Total Revenue", 1100}, {"Cost Of Revenue", 638}, {"Gross Profit", 462}, {"Research And Development", 55},
            {"Selling General And Administration", 160}, {"Operating Income", 247}, {"Interest Expense", 22},
            {"Pretax Income", 225}, {"Tax Provision", 45}, {"Net Income", 180}, {"Net Income Common Stockholders", 180},
            {"Diluted EPS", 1.8}, {"Diluted Average Shares", 100}, {"EBITDA", 312}, {"EBIT", 247}});
    fx_add(fd.income_statement, "2024-12-31",
           {{"Total Revenue", 1210}, {"Cost Of Revenue", 690}, {"Gross Profit", 520}, {"Research And Development", 60},
            {"Selling General And Administration", 170}, {"Operating Income", 290}, {"Interest Expense", 20},
            {"Pretax Income", 270}, {"Tax Provision", 54}, {"Net Income", 216}, {"Net Income Common Stockholders", 216},
            {"Diluted EPS", 2.25}, {"Diluted Average Shares", 96}, {"EBITDA", 360}, {"EBIT", 290}});
    fx_add(fd.balance_sheet, "2022-12-31",
           {{"Total Assets", 2000}, {"Current Assets", 800}, {"Current Liabilities", 400},
            {"Total Liabilities Net Minority Interest", 1200}, {"Stockholders Equity", 800},
            {"Cash And Cash Equivalents", 200}, {"Cash Cash Equivalents And Short Term Investments", 300},
            {"Accounts Receivable", 150}, {"Inventory", 120}, {"Net PPE", 700}, {"Goodwill And Other Intangible Assets", 100},
            {"Accounts Payable", 90}, {"Payables And Accrued Expenses", 130}, {"Current Debt", 50}, {"Long Term Debt", 450},
            {"Total Debt", 500}, {"Retained Earnings", 400}, {"Ordinary Shares Number", 100}, {"Working Capital", 400}});
    fx_add(fd.balance_sheet, "2023-12-31",
           {{"Total Assets", 2200}, {"Current Assets", 900}, {"Current Liabilities", 420},
            {"Total Liabilities Net Minority Interest", 1250}, {"Stockholders Equity", 950},
            {"Cash And Cash Equivalents", 240}, {"Cash Cash Equivalents And Short Term Investments", 360},
            {"Accounts Receivable", 170}, {"Inventory", 130}, {"Net PPE", 760}, {"Goodwill And Other Intangible Assets", 100},
            {"Accounts Payable", 100}, {"Payables And Accrued Expenses", 140}, {"Current Debt", 40}, {"Long Term Debt", 420},
            {"Total Debt", 460}, {"Retained Earnings", 500}, {"Ordinary Shares Number", 100}, {"Working Capital", 480}});
    fx_add(fd.balance_sheet, "2024-12-31",
           {{"Total Assets", 2400}, {"Current Assets", 1000}, {"Current Liabilities", 440},
            {"Total Liabilities Net Minority Interest", 1300}, {"Stockholders Equity", 1100},
            {"Cash And Cash Equivalents", 280}, {"Cash Cash Equivalents And Short Term Investments", 400},
            {"Accounts Receivable", 200}, {"Inventory", 140}, {"Net PPE", 820}, {"Goodwill And Other Intangible Assets", 100},
            {"Accounts Payable", 110}, {"Payables And Accrued Expenses", 150}, {"Current Debt", 30}, {"Long Term Debt", 400},
            {"Total Debt", 430}, {"Retained Earnings", 600}, {"Ordinary Shares Number", 96}, {"Working Capital", 560}});
    fx_add(fd.cash_flow, "2022-12-31",
           {{"Operating Cash Flow", 220}, {"Capital Expenditure", -100}, {"Free Cash Flow", 120},
            {"Depreciation And Amortization", 60}, {"Stock Based Compensation", 10}, {"Cash Dividends Paid", -40},
            {"Repurchase Of Capital Stock", -20}, {"Net Issuance Payments Of Debt", -10}, {"Beginning Cash Position", 150},
            {"End Cash Position", 200}, {"Changes In Cash", 50}});
    fx_add(fd.cash_flow, "2023-12-31",
           {{"Operating Cash Flow", 250}, {"Capital Expenditure", -110}, {"Free Cash Flow", 140},
            {"Depreciation And Amortization", 65}, {"Stock Based Compensation", 12}, {"Cash Dividends Paid", -45},
            {"Repurchase Of Capital Stock", -30}, {"Net Issuance Payments Of Debt", -40}, {"Beginning Cash Position", 200},
            {"End Cash Position", 240}, {"Changes In Cash", 40}});
    fx_add(fd.cash_flow, "2024-12-31",
           {{"Operating Cash Flow", 290}, {"Capital Expenditure", -120}, {"Free Cash Flow", 170},
            {"Depreciation And Amortization", 70}, {"Stock Based Compensation", 14}, {"Cash Dividends Paid", -50},
            {"Repurchase Of Capital Stock", -80}, {"Net Issuance Payments Of Debt", -30}, {"Beginning Cash Position", 240},
            {"End Cash Position", 280}, {"Changes In Cash", 40}});
    // Quarters: Mar 2024 … Mar 2025 (the four 2024 quarters sum to FY2024).
    const char* qd[] = {"2024-03-31", "2024-06-30", "2024-09-30", "2024-12-31", "2025-03-31"};
    const double rev[] = {290, 300, 305, 315, 320};
    const double ni[] = {50, 52, 55, 59, 60};
    const double sh[] = {97, 96, 96, 95, 95};
    const double ocf[] = {70, 70, 72, 78, 75};
    const double capex[] = {-30, -30, -30, -30, -32};
    const double begin[] = {240, 250, 260, 270, 280};
    const double end[] = {250, 260, 270, 280, 300};
    for (int i = 0; i < 5; ++i) {
        fx_add(fd.q_income, qd[i],
               {{"Total Revenue", rev[i]}, {"Net Income", ni[i]}, {"Net Income Common Stockholders", ni[i]},
                {"Diluted Average Shares", sh[i]}});
        fx_add(fd.q_cash_flow, qd[i],
               {{"Operating Cash Flow", ocf[i]}, {"Capital Expenditure", capex[i]}, {"Beginning Cash Position", begin[i]},
                {"End Cash Position", end[i]}, {"Changes In Cash", end[i] - begin[i]}});
    }
    fx_add(fd.q_balance, "2024-03-31", {{"Total Assets", 2250}, {"Stockholders Equity", 980}});
    fx_add(fd.q_balance, "2024-12-31", {{"Total Assets", 2400}, {"Stockholders Equity", 1100}});
    fx_add(fd.q_balance, "2025-03-31", {{"Total Assets", 2450}, {"Stockholders Equity", 1130}});
    return fd;
}

/// BANK: two fiscal years of a bank (no cost of revenue, net interest income filed).
FinancialsData fx_bank() {
    FinancialsData fd;
    fd.symbol = QStringLiteral("BANK");
    fx_add(fd.income_statement, "2023-12-31",
           {{"Total Revenue", 90}, {"Interest Income", 130}, {"Interest Expense", 80}, {"Net Interest Income", 50},
            {"Selling General And Administration", 33}, {"Occupancy And Equipment", 5}, {"Other Non Interest Expense", 4},
            {"Pretax Income", 36}, {"Tax Provision", 7}, {"Net Income", 29}, {"Net Income Common Stockholders", 27},
            {"Diluted EPS", 2.7}, {"Diluted Average Shares", 10}});
    fx_add(fd.income_statement, "2024-12-31",
           {{"Total Revenue", 100}, {"Interest Income", 150}, {"Interest Expense", 90}, {"Net Interest Income", 60},
            {"Selling General And Administration", 35}, {"Occupancy And Equipment", 5}, {"Other Non Interest Expense", 5},
            {"Pretax Income", 40}, {"Tax Provision", 8}, {"Net Income", 32}, {"Net Income Common Stockholders", 30},
            {"Diluted EPS", 3.0}, {"Diluted Average Shares", 10}});
    fx_add(fd.balance_sheet, "2023-12-31",
           {{"Total Assets", 900}, {"Total Liabilities Net Minority Interest", 815}, {"Stockholders Equity", 85},
            {"Tangible Book Value", 70}, {"Cash And Cash Equivalents", 45}, {"Net Loan", 540}, {"Total Deposits", 640},
            {"Total Debt", 90}, {"Long Term Debt", 72}, {"Current Debt", 18}});
    fx_add(fd.balance_sheet, "2024-12-31",
           {{"Total Assets", 1000}, {"Total Liabilities Net Minority Interest", 900}, {"Stockholders Equity", 100},
            {"Tangible Book Value", 80}, {"Cash And Cash Equivalents", 50}, {"Net Loan", 600}, {"Total Deposits", 700},
            {"Total Debt", 100}, {"Long Term Debt", 80}, {"Current Debt", 20}});
    fx_add(fd.cash_flow, "2024-12-31",
           {{"Operating Cash Flow", 12}, {"Cash Dividends Paid", -10}, {"Repurchase Of Capital Stock", -5}});
    return fd;
}

/// LOSS: a loss-making company, two fiscal years.
FinancialsData fx_loss() {
    FinancialsData fd;
    fd.symbol = QStringLiteral("LOSS");
    fx_add(fd.income_statement, "2023-12-31",
           {{"Total Revenue", 100}, {"Cost Of Revenue", 120}, {"Operating Income", -80}, {"Interest Expense", 5},
            {"Pretax Income", -85}, {"Tax Provision", 0}, {"Net Income", -85}, {"Diluted EPS", -0.85}, {"EBITDA", -60}});
    fx_add(fd.income_statement, "2024-12-31",
           {{"Total Revenue", 150}, {"Cost Of Revenue", 140}, {"Operating Income", -50}, {"Interest Expense", 6},
            {"Pretax Income", -55}, {"Tax Provision", 0}, {"Net Income", -55}, {"Diluted EPS", -0.5}, {"EBITDA", -30}});
    fx_add(fd.balance_sheet, "2023-12-31",
           {{"Total Assets", 500}, {"Stockholders Equity", 300}, {"Total Liabilities Net Minority Interest", 200},
            {"Current Assets", 200}, {"Current Liabilities", 100}, {"Total Debt", 100},
            {"Cash Cash Equivalents And Short Term Investments", 150}});
    fx_add(fd.balance_sheet, "2024-12-31",
           {{"Total Assets", 450}, {"Stockholders Equity", 245}, {"Total Liabilities Net Minority Interest", 205},
            {"Current Assets", 180}, {"Current Liabilities", 110}, {"Total Debt", 110},
            {"Cash Cash Equivalents And Short Term Investments", 90}});
    fx_add(fd.cash_flow, "2023-12-31", {{"Operating Cash Flow", -40}, {"Capital Expenditure", -30}});
    fx_add(fd.cash_flow, "2024-12-31", {{"Operating Cash Flow", -20}, {"Capital Expenditure", -25}});
    return fd;
}

/// SPARSE: revenue and net income only, nothing else filed.
FinancialsData fx_sparse() {
    FinancialsData fd;
    fd.symbol = QStringLiteral("SPARSE");
    fx_add(fd.income_statement, "2023-12-31", {{"Total Revenue", 50}, {"Net Income", 5}});
    fx_add(fd.income_statement, "2024-12-31", {{"Total Revenue", 60}, {"Net Income", 8}});
    return fd;
}

double fx_metric(const fin_an::Analysis& a, const char* id, int i) {
    return a.metric_series(QLatin1String(id)).at(i);
}
QString fx_note(const fin_an::Analysis& a, const char* id, int i) {
    return a.metric_series(QLatin1String(id)).note_at(i);
}

} // namespace

class TestEquityFinancialsAnalytics : public QObject {
    Q_OBJECT

  private slots:
    void helpers();
    void industrial_annual_ratios();
    void industrial_period_end_balance_note();
    void industrial_scores();
    void industrial_altman_variants();
    void industrial_waterfall_and_bridge();
    void industrial_structure_and_kpis();
    void quarterly_annualises_and_averages();
    void ttm_sums_flows_and_takes_latest_stocks();
    void bank_template_and_ratios();
    void negative_earnings();
    void missing_lines_say_why();
};

void TestEquityFinancialsAnalytics::helpers() {
    QString why;
    QVERIFY(fx_near(fin_an::yoy_change(110, 100), 0.10));
    QVERIFY(fx_near(fin_an::yoy_change(-120, -100), 0.20)); // magnitude growth of an outflow
    QVERIFY(std::isnan(fin_an::yoy_change(10, -5, &why)));
    QVERIFY(why.contains(QStringLiteral("turned positive")));
    QVERIFY(std::isnan(fin_an::yoy_change(10, 0, &why)));
    QVERIFY(why.contains(QStringLiteral("zero")));
    // CAGR: 100 → 121 over two years = 10 %.
    QVector<fin_an::PeriodInfo> p(3);
    p[0].end = QDate(2022, 12, 31);
    p[1].end = QDate(2023, 12, 31);
    p[2].end = QDate(2024, 12, 31);
    QVERIFY(fx_near(fin_an::cagr({100, 110, 121}, p), 0.10, 1e-3));
    QVERIFY(std::isnan(fin_an::cagr({100, 110}, p.mid(0, 2), -1, &why))); // one year only
    QVERIFY(why.contains(QStringLiteral("two years")));
    // The first POSITIVE reading starts the span: 5 (FY2023) → 8 (FY2024) is one year only.
    QVERIFY(std::isnan(fin_an::cagr({-10, 5, 8}, p, -1, &why)));
    QVERIFY(fx_near(fin_an::cagr({50, -5, 72}, p), std::pow(72.0 / 50.0, 365.25 / 731.0) - 1.0));
}

void TestEquityFinancialsAnalytics::industrial_annual_ratios() {
    fin_an::Context ctx;
    ctx.sector = QStringLiteral("Industrials");
    const fin_an::Analysis a = fin_an::analyse(fx_acme(), fin_model::Period::Annual, ctx);
    QCOMPARE(a.tmpl, fin_model::Template::Industrial);
    QCOMPARE(a.count(), 3);
    QCOMPARE(a.periods[2].label, QStringLiteral("FY2024"));
    QCOMPARE(a.periods[2].year_ago, 1);
    QCOMPARE(a.latest(), 2);
    const int t = 2;
    QVERIFY(fx_near(fx_metric(a, "gross_margin", t), 520.0 / 1210));
    QVERIFY(fx_near(fx_metric(a, "operating_margin", t), 290.0 / 1210));
    QVERIFY(fx_near(fx_metric(a, "net_margin", t), 216.0 / 1210));
    QVERIFY(fx_near(fx_metric(a, "fcf_margin", t), 170.0 / 1210));
    QVERIFY(fx_near(fx_metric(a, "tax_rate", t), 0.2));
    // Average balances: equity (1100 + 950) / 2 = 1025; assets (2400 + 2200) / 2 = 2300.
    QVERIFY(fx_near(fx_metric(a, "roe", t), 216.0 / 1025));
    QVERIFY(fx_note(a, "roe", t).isEmpty());
    QVERIFY(fx_near(fx_metric(a, "roa", t), 216.0 / 2300));
    // ROIC: EBIT 290 × (1 − 0.2) / avg(430 + 1100 − 400, 460 + 950 − 360) = 232 / 1090.
    QVERIFY(fx_near(fx_metric(a, "roic", t), 232.0 / 1090));
    // ROCE: 290 / avg(2400 − 440, 2200 − 420) = 290 / 1870.
    QVERIFY(fx_near(fx_metric(a, "roce", t), 290.0 / 1870));
    // DuPont identity.
    const double nm = fx_metric(a, "net_margin", t), at = fx_metric(a, "asset_turnover", t);
    const double em = fx_metric(a, "equity_multiplier", t);
    QVERIFY(fx_near(at, 1210.0 / 2300));
    QVERIFY(fx_near(em, 2300.0 / 1025));
    QVERIFY(fx_near(nm * at * em, fx_metric(a, "roe", t)));
    // Growth.
    QVERIFY(fx_near(fx_metric(a, "revenue_growth", t), 0.10));
    QVERIFY(fx_near(fx_metric(a, "eps_growth", t), 0.25));
    QVERIFY(std::isnan(fx_metric(a, "revenue_growth", 0)));
    QVERIFY(fx_note(a, "revenue_growth", 0).contains(QStringLiteral("year earlier")));
    // Liquidity, leverage.
    QVERIFY(fx_near(fx_metric(a, "current_ratio", t), 1000.0 / 440));
    QVERIFY(fx_near(fx_metric(a, "quick_ratio", t), 860.0 / 440));
    QVERIFY(fx_near(fx_metric(a, "cash_ratio", t), 400.0 / 440));
    QVERIFY(fx_near(fx_metric(a, "debt_to_equity", t), 430.0 / 1100));
    QVERIFY(fx_near(fx_metric(a, "net_debt", t), 30.0));
    QVERIFY(fx_near(fx_metric(a, "net_debt_ebitda", t), 30.0 / 360));
    QVERIFY(fx_near(fx_metric(a, "interest_cover", t), 14.5));
    // Efficiency (average balances × 365 days).
    const double dso = 185.0 / 1210 * 365, dio = 135.0 / 690 * 365, dpo = 105.0 / 690 * 365;
    QVERIFY(fx_near(fx_metric(a, "dso", t), dso));
    QVERIFY(fx_near(fx_metric(a, "dio", t), dio));
    QVERIFY(fx_near(fx_metric(a, "dpo", t), dpo));
    QVERIFY(fx_near(fx_metric(a, "ccc", t), dso + dio - dpo));
    QVERIFY(fx_near(fx_metric(a, "inventory_turnover", t), 690.0 / 135));
    // Cash-flow quality.
    QVERIFY(fx_near(fx_metric(a, "cfo_to_ni", t), 290.0 / 216));
    QVERIFY(fx_near(fx_metric(a, "fcf_conversion", t), 170.0 / 216));
    QVERIFY(fx_near(fx_metric(a, "accruals_ratio", t), (216.0 - 290.0) / 2300));
    QVERIFY(fx_near(fx_metric(a, "capex_to_da", t), 120.0 / 70));
    QVERIFY(fx_near(fx_metric(a, "sbc_to_revenue", t), 14.0 / 1210));
    QVERIFY(fx_near(fx_metric(a, "ar_vs_revenue", t), (200.0 / 170 - 1.0) - 0.10));
    // Per share, capital return.
    QVERIFY(fx_near(fx_metric(a, "bvps", t), 1100.0 / 96));
    QVERIFY(fx_near(fx_metric(a, "dps", t), 50.0 / 96));
    QVERIFY(fx_near(fx_metric(a, "share_change", t), -0.04));
    QVERIFY(fx_near(fx_metric(a, "payout_ratio", t), 50.0 / 216));
    QVERIFY(fx_near(fx_metric(a, "total_payout_fcf", t), 130.0 / 170));
    // Every metric family the tab lists is present.
    QStringList families;
    for (const auto& m : a.metrics)
        if (!families.contains(m.family))
            families << m.family;
    QCOMPARE(families.size(), 10); // profitability … capital return
}

void TestEquityFinancialsAnalytics::industrial_period_end_balance_note() {
    const fin_an::Analysis a = fin_an::analyse(fx_acme(), fin_model::Period::Annual);
    // FY2022 has no balance sheet a year earlier: ROE on the year-end equity, with a note.
    QVERIFY(fx_near(fx_metric(a, "roe", 0), 144.0 / 800));
    QVERIFY(fx_note(a, "roe", 0).contains(QStringLiteral("period-end")));
}

void TestEquityFinancialsAnalytics::industrial_scores() {
    fin_an::Context ctx;
    ctx.sector = QStringLiteral("Industrials");
    const fin_an::Analysis a = fin_an::analyse(fx_acme(), fin_model::Period::Annual, ctx);
    // Piotroski FY2024 vs FY2023: 8 of 9 — asset turnover is flat (1210 / 2200 = 1100 / 2000 = 0.55).
    const fin_an::Score& p = a.piotroski;
    QCOMPARE(p.available, 9);
    QCOMPARE(p.passed, 8);
    QCOMPARE(p.value, 8.0);
    QCOMPARE(p.zone, fin_an::Zone::Safe);
    QCOMPARE(p.period_label, QStringLiteral("FY2024 vs FY2023"));
    QCOMPARE(p.parts.size(), 9);
    QVERIFY(fx_near(p.parts[0].value, 216.0 / 2200)); // ROA on beginning assets
    QVERIFY(fx_near(p.parts[2].prior, 180.0 / 2000));  // last year's ROA
    QVERIFY(fx_near(p.parts[3].value, 290.0 / 2200));  // CFO / beginning assets
    QVERIFY(fx_near(p.parts[4].value, 400.0 / 2300));  // LTD / avg assets
    QVERIFY(fx_near(p.parts[4].prior, 420.0 / 2100));
    QCOMPARE(p.parts[8].id, QStringLiteral("turnover_up"));
    QCOMPARE(p.parts[8].pass, 0);
    for (int k = 0; k < 8; ++k)
        QCOMPARE(p.parts[k].pass, 1);

    // Beneish FY2024 vs FY2023 (no long-term investments line → securities = 0).
    const fin_an::Score& b = a.beneish;
    QVERIFY(std::isfinite(b.value));
    const double dsri = (200.0 / 1210) / (170.0 / 1100);
    const double gmi = (462.0 / 1100) / (520.0 / 1210);
    const double aqi = (1.0 - 1820.0 / 2400) / (1.0 - 1660.0 / 2200);
    const double sgi = 1.1;
    const double depi = (65.0 / 825) / (70.0 / 890);
    const double sgai = (170.0 / 1210) / (160.0 / 1100);
    const double lvgi = (840.0 / 2400) / (840.0 / 2200);
    const double tata = (216.0 - 290.0) / 2400;
    const double m = -4.84 + 0.920 * dsri + 0.528 * gmi + 0.404 * aqi + 0.892 * sgi + 0.115 * depi - 0.172 * sgai +
                     4.679 * tata - 0.327 * lvgi;
    QVERIFY(fx_near(b.value, m));
    QVERIFY(fx_near(b.value, -2.456015, 1e-4));
    QCOMPARE(b.zone, fin_an::Zone::Safe);
    QCOMPARE(b.parts.size(), 8);
}

void TestEquityFinancialsAnalytics::industrial_altman_variants() {
    // Manufacturer with a market value → Z (1968).
    fin_an::Context ctx;
    ctx.sector = QStringLiteral("Industrials");
    ctx.market_cap = 3000;
    fin_an::Analysis a = fin_an::analyse(fx_acme(), fin_model::Period::Annual, ctx);
    const double z = 1.2 * (560.0 / 2400) + 1.4 * (600.0 / 2400) + 3.3 * (290.0 / 2400) + 0.6 * (3000.0 / 1300) +
                     1.0 * (1210.0 / 2400);
    QVERIFY(fx_near(a.altman.value, z));
    QVERIFY(fx_near(a.altman.value, 2.917532, 1e-4));
    QCOMPARE(a.altman.zone, fin_an::Zone::Grey);
    QVERIFY(a.altman.variant.startsWith(QStringLiteral("Z (1968")));
    QCOMPARE(a.altman.parts.size(), 5);
    // No market value → Z'' on book equity.
    ctx.market_cap = fin_an::kNaN;
    ctx.market_cap_note = QStringLiteral("quoted in USD, reports in TWD");
    a = fin_an::analyse(fx_acme(), fin_model::Period::Annual, ctx);
    const double zpp = 6.56 * (560.0 / 2400) + 3.26 * (600.0 / 2400) + 6.72 * (290.0 / 2400) + 1.05 * (1100.0 / 1300);
    QVERIFY(fx_near(a.altman.value, zpp));
    QVERIFY(fx_near(a.altman.value, 4.046128, 1e-4));
    QCOMPARE(a.altman.zone, fin_an::Zone::Safe);
    QVERIFY(a.altman.variant.contains(QStringLiteral("TWD")));
    QCOMPARE(a.altman.parts.size(), 4);
    // A services company uses Z'' even with a market value.
    ctx.sector = QStringLiteral("Communication Services");
    ctx.market_cap = 3000;
    a = fin_an::analyse(fx_acme(), fin_model::Period::Annual, ctx);
    QVERIFY(a.altman.variant.startsWith(QStringLiteral("Z''")));
}

void TestEquityFinancialsAnalytics::industrial_waterfall_and_bridge() {
    const fin_an::Analysis a = fin_an::analyse(fx_acme(), fin_model::Period::Annual);
    const auto wf = fin_an::income_waterfall(a, 2);
    QStringList ids;
    for (const auto& s : wf)
        ids << s.id;
    QCOMPARE(ids, (QStringList{"revenue", "cogs", "gross_profit", "rnd", "sga", "operating_income", "interest_expense",
                               "pretax", "tax", "net_income"}));
    QCOMPARE(wf[0].kind, fin_an::Step::Kind::Start);
    QCOMPARE(wf[1].value, -690.0);
    QCOMPARE(wf[2].kind, fin_an::Step::Kind::Subtotal);
    QCOMPARE(wf[2].value, 520.0);
    QCOMPARE(wf[9].kind, fin_an::Step::Kind::End);
    QCOMPARE(wf[9].value, 216.0);
    // Every delta run adds up to the next subtotal.
    double level = 0;
    for (const auto& s : wf) {
        if (s.kind == fin_an::Step::Kind::Delta)
            level += s.value;
        else {
            if (s.kind != fin_an::Step::Kind::Start)
                QVERIFY(fx_near(level, s.value));
            level = s.value;
        }
    }
    const auto br = fin_an::cash_bridge(a, 2);
    ids.clear();
    for (const auto& s : br)
        ids << s.id;
    QCOMPARE(ids, (QStringList{"begin_cash", "cfo", "capex", "dividends", "buybacks", "net_debt_flow", "other", "end_cash"}));
    QCOMPARE(br[6].value, 30.0); // 280 − 240 − (290 − 120 − 50 − 80 − 30)
    QCOMPARE(br.last().value, 280.0);
}

void TestEquityFinancialsAnalytics::industrial_structure_and_kpis() {
    const fin_an::Analysis a = fin_an::analyse(fx_acme(), fin_model::Period::Annual);
    // Asset side FY2024: cash & STI 400, receivables 200, inventory 140, PP&E 820, intangibles 100, other 740.
    QHash<QString, double> assets;
    double sum = 0;
    for (const auto& s : a.assets_mix) {
        assets.insert(s.id, s.value.at(2));
        sum += s.value.at(2);
    }
    QCOMPARE(assets.value(QStringLiteral("other")), 740.0);
    QCOMPARE(sum, 2400.0);
    // Funding: payables 150, ST debt 30, LT debt 400, other liabilities 1300 − 580 = 720, equity 1100.
    QHash<QString, double> fund;
    sum = 0;
    for (const auto& s : a.funding_mix) {
        fund.insert(s.id, s.value.at(2));
        sum += s.value.at(2);
    }
    QCOMPARE(fund.value(QStringLiteral("other")), 720.0);
    QCOMPARE(fund.value(QStringLiteral("total_equity")), 1100.0);
    QCOMPARE(sum, 2400.0);

    const auto k = fin_an::kpis(a, 2);
    QCOMPARE(k.size(), 12);
    QCOMPARE(k[0].id, QStringLiteral("revenue"));
    QCOMPARE(k[0].value, 1210.0);
    QVERIFY(fx_near(k[0].change, 0.10));
    QCOMPARE(k[0].spark.size(), 3);
    QCOMPARE(k[1].id, QStringLiteral("gross_margin"));
    QVERIFY(fx_near(k[1].change, 520.0 / 1210 - 462.0 / 1100)); // pp difference
    QCOMPARE(k[9].id, QStringLiteral("net_debt_ebitda"));
    QVERIFY(k[9].lower_is_better);
    // Growth heat-map rows for an industrial company.
    QStringList rows;
    for (const auto& g : a.growth)
        rows << g.id;
    QCOMPARE(rows, (QStringList{"revenue", "gross_profit", "operating_income", "net_income", "eps_diluted", "cfo", "fcf",
                                "total_assets", "total_debt", "equity"}));
}

void TestEquityFinancialsAnalytics::quarterly_annualises_and_averages() {
    const fin_an::Analysis a = fin_an::analyse(fx_acme(), fin_model::Period::Quarterly);
    QCOMPARE(a.count(), 5);
    QCOMPARE(a.periods[4].label, QStringLiteral("Q1 FY25"));
    QCOMPARE(a.periods[4].days, 90);
    QCOMPARE(a.periods[4].year_ago, 0);
    QVERIFY(fx_near(fx_metric(a, "revenue_growth", 4), 320.0 / 290 - 1.0));
    // ROE: 60 × 365 / 90 over the average of the Mar 2025 and Dec 2024 equity (1130 + 1100) / 2.
    QVERIFY(fx_near(fx_metric(a, "roe", 4), 60.0 * 365.0 / 90.0 / 1115.0));
}

void TestEquityFinancialsAnalytics::ttm_sums_flows_and_takes_latest_stocks() {
    const fin_an::Analysis a = fin_an::analyse(fx_acme(), fin_model::Period::Ttm);
    QCOMPARE(a.count(), 4); // FY2022–FY2024 + LTM Mar 2025
    const int t = 3;
    QVERIFY(a.periods[t].ttm);
    QCOMPARE(a.periods[t].label, QStringLiteral("LTM Q1 FY25"));
    QCOMPARE(a.line(QStringLiteral("revenue")).at(t), 1240.0);  // 300 + 305 + 315 + 320
    QCOMPARE(a.line(QStringLiteral("net_income")).at(t), 226.0); // 52 + 55 + 59 + 60
    QCOMPARE(a.line(QStringLiteral("cfo")).at(t), 295.0);
    QCOMPARE(a.line(QStringLiteral("shares_diluted")).at(t), 95.5); // average, not a sum
    // Stocks: the opening cash of the first quarter, the closing cash of the last.
    QCOMPARE(a.line(QStringLiteral("begin_cash")).at(t), 250.0);
    QCOMPARE(a.line(QStringLiteral("end_cash")).at(t), 300.0);
    QCOMPARE(a.line(QStringLiteral("total_assets")).at(t), 2450.0);
    // ROE on the average of Mar 2025 and Mar 2024 equity: 226 / ((1130 + 980) / 2).
    QVERIFY(fx_near(fx_metric(a, "roe", t), 226.0 / 1055));
    const auto br = fin_an::cash_bridge(a, t);
    QVERIFY(!br.isEmpty());
    QCOMPARE(br.first().value, 250.0);
    QCOMPARE(br.last().value, 300.0);
}

void TestEquityFinancialsAnalytics::bank_template_and_ratios() {
    fin_an::Context ctx;
    ctx.sector = QStringLiteral("Financial Services");
    const fin_an::Analysis a = fin_an::analyse(fx_bank(), fin_model::Period::Annual, ctx);
    QCOMPARE(a.tmpl, fin_model::Template::Bank);
    QVERIFY(a.financial());
    QVERIFY(a.metric(QStringLiteral("gross_margin")) == nullptr);
    QVERIFY(a.metric(QStringLiteral("current_ratio")) == nullptr);
    const int t = 1;
    QVERIFY(fx_near(fx_metric(a, "efficiency_ratio", t), 45.0 / 100));
    QVERIFY(fx_near(fx_metric(a, "nii_share", t), 0.6));
    QVERIFY(fx_near(fx_metric(a, "roe", t), 30.0 / 92.5));
    QVERIFY(fx_near(fx_metric(a, "rote", t), 30.0 / 75.0));
    QVERIFY(fx_near(fx_metric(a, "loans_to_deposits", t), 600.0 / 700));
    QVERIFY(fx_near(fx_metric(a, "nii_to_assets", t), 60.0 / 950));
    QVERIFY(std::isnan(a.altman.value));
    QVERIFY(a.altman.note.contains(QStringLiteral("banks")));
    QVERIFY(std::isnan(a.beneish.value));
    QVERIFY(a.beneish.note.contains(QStringLiteral("banks")));
    const auto k = fin_an::kpis(a, t);
    QCOMPARE(k[1].id, QStringLiteral("nii"));
    QCOMPARE(k[1].value, 60.0);
    // Waterfall: interest income → … → net income to common.
    const auto wf = fin_an::income_waterfall(a, t);
    QStringList ids;
    for (const auto& s : wf)
        ids << s.id;
    QCOMPARE(ids, (QStringList{"interest_income", "interest_expense", "nii", "non_interest_income", "revenue",
                               "non_interest_expense", "other", "pretax", "tax", "other_below", "net_income"}));
    QCOMPARE(wf[6].value, -15.0); // pretax 40 − (100 − 45): provisions & other
    QCOMPARE(wf.last().value, 30.0);
    // Structure: loans and deposits are filed, so "other" keeps its plain label.
    bool loans = false;
    for (const auto& s : a.assets_mix)
        loans = loans || (s.id == QLatin1String("loans") && s.value.at(t) == 600.0);
    QVERIFY(loans);
    for (const auto& s : a.funding_mix)
        if (s.id == QLatin1String("deposits"))
            QCOMPARE(s.value.at(t), 700.0);
    QStringList rows;
    for (const auto& g : a.growth)
        rows << g.id;
    QCOMPARE(rows, (QStringList{"revenue", "nii", "pretax", "net_income", "eps_diluted", "total_assets", "total_debt",
                                "equity"}));
}

void TestEquityFinancialsAnalytics::negative_earnings() {
    const fin_an::Analysis a = fin_an::analyse(fx_loss(), fin_model::Period::Annual);
    const int t = 1;
    // Losses in both years: NM, never a fake "−35 %".
    QVERIFY(std::isnan(fx_metric(a, "net_income_growth", t)));
    QVERIFY(fx_note(a, "net_income_growth", t).startsWith(QStringLiteral("NM")));
    QVERIFY(fx_note(a, "net_income_growth", t).contains(QStringLiteral("narrowed")));
    QVERIFY(std::isnan(fx_metric(a, "eps_growth", t)));
    QVERIFY(std::isnan(fx_metric(a, "interest_cover", t)));
    QVERIFY(fx_note(a, "interest_cover", t).contains(QStringLiteral("operating loss")));
    QVERIFY(std::isnan(fx_metric(a, "cfo_to_ni", t)));
    QVERIFY(fx_note(a, "cfo_to_ni", t).contains(QStringLiteral("net income")));
    QVERIFY(std::isnan(fx_metric(a, "net_debt_ebitda", t)));
    // A negative ROE is a real reading: −55 / ((245 + 300) / 2).
    QVERIFY(fx_near(fx_metric(a, "roe", t), -55.0 / 272.5));
    QVERIFY(fx_near(fx_metric(a, "gross_margin", t), 10.0 / 150));
    QVERIFY(std::isnan(a.beneish.value));
    QVERIFY(a.beneish.note.startsWith(QStringLiteral("n/a")));
    const auto k = fin_an::kpis(a, t);
    for (const auto& kp : k)
        if (kp.id == QLatin1String("eps_diluted")) {
            QCOMPARE(kp.value, -0.5);
            QVERIFY(std::isnan(kp.change));
            QVERIFY(kp.note.startsWith(QStringLiteral("NM")));
        }
    // Piotroski still scores the computable tests.
    QVERIFY(a.piotroski.available >= 6);
    QCOMPARE(a.piotroski.parts[0].pass, 0); // ROA < 0
}

void TestEquityFinancialsAnalytics::missing_lines_say_why() {
    const fin_an::Analysis a = fin_an::analyse(fx_sparse(), fin_model::Period::Annual);
    QCOMPARE(a.count(), 2);
    QVERIFY(std::isnan(fx_metric(a, "gross_margin", 1)));
    QVERIFY(fx_note(a, "gross_margin", 1).contains(QStringLiteral("not reported")));
    QVERIFY(std::isnan(fx_metric(a, "roe", 1)));
    QVERIFY(!fx_note(a, "roe", 1).isEmpty());
    QVERIFY(fx_near(fx_metric(a, "net_margin", 1), 8.0 / 60));
    QVERIFY(fin_an::cash_bridge(a, 1).isEmpty());
    const auto wf = fin_an::income_waterfall(a, 1);
    QCOMPARE(wf.size(), 3); // revenue → costs & other → net income
    QCOMPARE(wf[1].value, -52.0);
    QVERIFY(std::isnan(a.altman.value));
    QVERIFY(!a.altman.note.isEmpty());
    const auto k = fin_an::kpis(a, 1);
    for (const auto& kp : k)
        if (!std::isfinite(kp.value))
            QVERIFY2(!kp.note.isEmpty(), qPrintable(kp.id)); // a blank tile always says why
    // Out-of-range periods are safe.
    QVERIFY(fin_an::income_waterfall(a, 7).isEmpty());
    QVERIFY(fin_an::kpis(a, -1).isEmpty());
}

QTEST_GUILESS_MAIN(TestEquityFinancialsAnalytics)
#include "tst_equity_financials_analytics.moc"
