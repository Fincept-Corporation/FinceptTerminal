// src/screens/equity_research/EquityFinancialsAnalytics.h
//
// Every number the Financials tab derives from the statements: canonical lines, ratio families,
// growth, balance-sheet structure, income waterfall, cash bridge, KPI readings and the three
// quality scores (Piotroski F, Altman Z, Beneish M). Pure: QtCore only, no widgets, no I/O, safe
// on a worker thread. Unit-tested by tests/tst_equity_financials_analytics.cpp.
//
// ─── Conventions ─────────────────────────────────────────────────────────────────────────────
//  * Amounts are in the company's REPORTING currency, full units, as filed (yfinance lines).
//  * Signs as filed: expenses positive on the income statement (cost of revenue, tax), cash
//    OUTflows negative on the cash-flow statement (capex, dividends, buybacks, debt repaid).
//    Ratios that need a magnitude take |x| and say so.
//  * Missing input or a division by zero / by a non-positive base gives NaN, and Series::note
//    says why ("n/a — no interest expense reported"). A reported zero stays 0.
//  * Periods are chronological (oldest first). Annual = fiscal years; Quarterly = fiscal
//    quarters; TTM = each fiscal year plus rolling four-quarter sums after it.
//  * TTM: flow items (income, cash flow) = sum of the last four consecutive quarters; stock
//    items (balance sheet, beginning / ending cash) = the latest quarter's (the first quarter's
//    for beginning cash). Built by fin_model::columns_for.
//  * Annualisation: a return or turnover over a quarter multiplies the quarter's flow by
//    365 / days-in-period (≈ 4); annual and TTM flows are used as they are. Days ratios
//    (DSO, DIO, DPO) use the period's own day count.
//  * AVERAGE balances (mean of this period end and the previous one — a year earlier for annual
//    and TTM periods, a quarter earlier for quarterly ones) are used by: ROE, ROA, ROIC, ROCE,
//    ROTE, asset turnover, equity multiplier (DuPont), DSO, DIO, DPO, cash conversion cycle,
//    inventory turnover, accruals ratio, NII / assets. When the previous balance sheet is not on
//    file the period-end balance is used and the note says so. Every other balance ratio uses
//    the period-end balance.
//
// ─── Definitions (id — formula) ──────────────────────────────────────────────────────────────
//  PROFITABILITY  gross_margin = gross profit ÷ revenue; ebitda_margin = EBITDA ÷ revenue;
//    operating_margin = operating income ÷ revenue; pretax_margin = pretax income ÷ revenue;
//    net_margin = net income to common ÷ revenue; fcf_margin = FCF ÷ revenue;
//    rnd_ratio = R&D ÷ revenue; sga_ratio = SG&A ÷ revenue; tax_rate = tax ÷ pretax income.
//    (revenue must be > 0; tax_rate needs pretax income > 0)
//  RETURNS  roe = NI × a ÷ avg equity; roa = NI × a ÷ avg total assets;
//    roic = EBIT × (1 − t) × a ÷ avg invested capital, invested capital = total debt + equity −
//    cash & short-term investments, t = effective tax rate clamped to 0–50 % (Yahoo's
//    "Tax Rate For Calcs" when present); roce = EBIT × a ÷ avg (total assets − current
//    liabilities); rote (banks, insurers) = NI × a ÷ avg tangible book value. a = annualiser.
//    Denominators must be > 0.
//  DUPONT  net_margin × asset_turnover × equity_multiplier = roe (identical by construction):
//    asset_turnover = revenue × a ÷ avg total assets; equity_multiplier = avg total assets ÷
//    avg equity.
//  GROWTH  *_growth = (|x_t| − |x_t−1y|) ÷ |x_t−1y| against the same period a year earlier,
//    only when both share a sign (a sign change has no meaningful percentage); two negative
//    readings (a loss in both periods) are "NM" with the direction in the note. The statement
//    tables' YoY columns keep magnitude growth for negative lines (an outflow that grew).
//  LIQUIDITY  current_ratio = CA ÷ CL; quick_ratio = (CA − inventory) ÷ CL;
//    cash_ratio = cash & short-term investments ÷ CL.
//  LEVERAGE  debt_to_equity = total debt ÷ equity (equity > 0); net_debt = total debt − cash &
//    short-term investments (negative = net cash); net_debt_ebitda = net debt ÷ (EBITDA × a),
//    EBITDA > 0; interest_cover = EBIT ÷ |interest expense| (both > 0; an operating loss is
//    n/a, not a negative cover); debt_to_assets;
//    equity_to_assets.
//  EFFICIENCY  dso = avg accounts receivable ÷ revenue × days; dio = avg inventory ÷ cost of
//    revenue × days (0 when the company carries no inventory line but reports a cost of
//    revenue); dpo = avg accounts payable ÷ cost of revenue × days; ccc = dso + dio − dpo;
//    inventory_turnover = cost of revenue × a ÷ avg inventory.
//  CASH FLOW QUALITY  cfo_to_ni = CFO ÷ NI (NI > 0); fcf_conversion = FCF ÷ NI (NI > 0);
//    accruals_ratio = (NI − CFO) × a ÷ avg total assets (Sloan, cash-flow form); capex_to_da =
//    |capex| ÷ D&A; capex_to_revenue = |capex| ÷ revenue; sbc_to_revenue = SBC ÷ revenue;
//    ar_vs_revenue = receivables growth − revenue growth (pp; > 0 = receivables outgrow sales).
//  PER SHARE  eps_diluted (as filed); revenue_ps = revenue ÷ diluted shares; fcf_ps = FCF ÷
//    diluted shares; bvps = equity ÷ shares outstanding; dps = |dividends paid| ÷ diluted shares;
//    shares_diluted; share_change = diluted shares YoY.
//  CAPITAL RETURN  payout_ratio = |dividends| ÷ NI; total_payout_fcf = (|dividends| +
//    |buybacks|) ÷ FCF (FCF > 0); total_payout_ni = (|dividends| + |buybacks|) ÷ NI.
//  BANK  nii_share = net interest income ÷ revenue; nii_to_assets = NII × a ÷ avg total assets
//    (a margin proxy); efficiency_ratio = non-interest expense ÷ revenue; loans_to_deposits
//    when both lines are filed; provisions_to_revenue.
//  SCORES (fiscal years, latest vs the one before):
//    Piotroski F — nine binary tests (ROA > 0, CFO > 0, ΔROA > 0, CFO > NI, Δ long-term debt ÷
//      avg assets < 0, Δ current ratio > 0, no new shares, Δ gross margin > 0, Δ asset
//      turnover > 0); ROA and turnover on beginning-of-year total assets; the net income of
//      ROA / CFO > NI is income from continuing operations BEFORE preferred dividends (the
//      paper's "net income before extraordinary items"), unlike the ratio families, which use
//      net income to common. Score = tests passed out of those computable.
//    Altman Z — Z (1968, public manufacturers: 1.2·X1 + 1.4·X2 + 3.3·X3 + 0.6·X4 + 1.0·X5,
//      X4 = market value of equity ÷ total liabilities; safe > 2.99, distress < 1.81) when the
//      company is in a manufacturing sector (Technology, Industrials, Basic Materials, Energy,
//      Consumer Cyclical / Defensive, Healthcare), carries inventory, and its market value is
//      known in the reporting currency (the listing trades in the currency it reports in);
//      otherwise Z'' (1995, non-manufacturers: 6.56·X1 + 3.26·X2 + 6.72·X3 + 1.05·X4,
//      X4 = book equity ÷ total liabilities; safe > 2.60, distress < 1.10). X1 = working
//      capital ÷ TA, X2 = retained earnings ÷ TA, X3 = EBIT ÷ TA, X5 = revenue ÷ TA.
//      Not defined for banks and insurers.
//    Beneish M (8 variables, 1999) — −4.84 + 0.920·DSRI + 0.528·GMI + 0.404·AQI + 0.892·SGI +
//      0.115·DEPI − 0.172·SGAI + 4.679·TATA − 0.327·LVGI; M > −1.78 flags likely earnings
//      manipulation (−2.22 the conservative cut); securities = long-term investments (0 when
//      not filed), long-term debt 0 when not filed; needs positive revenue and gross profit in
//      both years. Not defined for banks and insurers.
#pragma once

#include "screens/equity_research/EquityStatementModel.h"
#include "services/equity/EquityResearchModels.h"

#include <QDate>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::screens::fin_an {

using fin_model::Period;
using fin_model::Template;
inline constexpr double kNaN = fin_model::kNaN;
/// The hurdle drawn under ROIC / ROE: a typical cost of capital.
inline constexpr double kHurdleRate = 0.10;

/// How a value is formatted.
enum class Unit {
    Money,     ///< reporting currency, full units
    PerShare,  ///< reporting currency per share
    Shares,    ///< share count
    Percent,   ///< a fraction (0.25 = 25 %)
    PctPoints, ///< a difference of two fractions (0.031 = +3.1 pp)
    Multiple,  ///< x
    Days,      ///< days
    Score,     ///< a plain score (Altman Z, Beneish M)
};

/// Per-period values of one line or metric.
struct Series {
    QVector<double> v;     ///< aligned with Analysis::periods; NaN = not computable
    QVector<QString> note; ///< per period: why it is NaN, or a caveat for a finite value ("" = none)

    int size() const { return static_cast<int>(v.size()); }
    double at(int i) const { return i >= 0 && i < v.size() ? v[i] : kNaN; }
    QString note_at(int i) const { return i >= 0 && i < note.size() ? note[i] : QString(); }
    bool any() const;
    /// The last finite value at or before `upto` (-1 = the end); its index in *index.
    double last(int upto = -1, int* index = nullptr) const;
    static Series filled(int n);
};

struct PeriodInfo {
    QDate end;
    QString label;   ///< "FY2025", "Q2 FY26", "LTM Q1 FY27"
    QString tooltip; ///< "Fiscal year ended 30 September 2025"
    bool ttm = false;
    int days = 365;    ///< days the flows cover (≈ 91 for a quarter)
    int year_ago = -1; ///< index of the same period a year earlier, -1 when not on file
};

struct Metric {
    QString id;
    QString family;  ///< "PROFITABILITY", "RETURNS", … (translated)
    QString label;   ///< translated
    Unit unit = Unit::Percent;
    QString formula; ///< translated definition, for the tooltip
    bool higher_is_better = true;
    bool averaged = false; ///< uses average balances
    Series s;
};

/// One test / input of a score.
struct Check {
    QString id;
    QString label;
    QString formula;
    double value = kNaN; ///< this year's reading
    double prior = kNaN; ///< last year's (tests on a change)
    int pass = -1;       ///< 1 pass, 0 fail, -1 not computable
    QString note;        ///< why it is not computable
};

enum class Zone { NotAvailable, Safe, Grey, Distress };

struct Score {
    QString id;      ///< "piotroski" | "altman" | "beneish"
    QString label;   ///< "Piotroski F-score"
    QString variant; ///< "Z'' (1995, non-manufacturers)"
    QString formula;
    QString period_label; ///< "FY2026 vs FY2025"
    double value = kNaN;
    QString note; ///< "n/a — <missing input>" when value is NaN
    int passed = 0;    ///< Piotroski: tests passed
    int available = 0; ///< Piotroski: tests computable
    double safe = kNaN;     ///< Altman: above = safe; Beneish: below = unlikely manipulator (−2.22)
    double distress = kNaN; ///< Altman: below = distress; Beneish: above = likely manipulator (−1.78)
    Zone zone = Zone::NotAvailable;
    QVector<Check> parts;
};

/// One bar of a waterfall / bridge.
struct Step {
    enum class Kind { Start, Delta, Subtotal, End };
    QString id;
    QString label;
    double value = kNaN; ///< Start / Subtotal / End: the level; Delta: the signed change
    Kind kind = Kind::Delta;
    QString note;
};

/// One segment of a 100 % balance-sheet stack.
struct Segment {
    QString id;
    QString label;
    Series value; ///< amount per period (>= 0 shown; a deficit is noted)
};

struct GrowthRow {
    QString id;
    QString label;
    Series yoy; ///< fraction per period
};

/// One KPI tile reading.
struct Kpi {
    QString id;
    QString label;
    QString tooltip;
    Unit unit = Unit::Money;
    double value = kNaN;
    double prior = kNaN;  ///< same period a year earlier
    double change = kNaN; ///< Percent unit: pp difference; Money / PerShare: fractional growth; else difference
    bool lower_is_better = false;
    QVector<double> spark; ///< up to five periods ending at the reading
    QString note;          ///< why value / change is missing
};

/// Inputs that are not in the statements.
struct Context {
    QString sector;
    QString industry;
    /// Market value of equity in the REPORTING currency (NaN = unknown or not comparable).
    double market_cap = kNaN;
    QString market_cap_note; ///< why it is NaN (e.g. "quoted in USD, reports in TWD")
};

struct Analysis {
    Period basis = Period::Annual;
    Template tmpl = Template::Industrial;
    int fy_end_month = 12;
    QVector<PeriodInfo> periods;
    QHash<QString, Series> lines; ///< canonical lines by id (see lines() in the .cpp)
    QVector<Metric> metrics;      ///< ratio families in display order
    QVector<GrowthRow> growth;    ///< heat-map rows
    QVector<Segment> assets_mix;  ///< balance-sheet structure: asset side
    QVector<Segment> funding_mix; ///< liabilities + equity side
    Score piotroski;
    Score altman;
    Score beneish;
    QStringList notes; ///< analysis-wide caveats (no quarterly statements, …)

    bool financial() const { return tmpl != Template::Industrial; }
    int count() const { return static_cast<int>(periods.size()); }
    /// Index of the latest period with revenue (or total assets), -1 when empty.
    int latest() const;
    /// A canonical line (an all-NaN series when it is not filed).
    Series line(const QString& id) const;
    const Metric* metric(const QString& id) const;
    Series metric_series(const QString& id) const;
};

/// Everything for one period basis.
Analysis analyse(const services::equity::FinancialsData& fd, Period basis, const Context& ctx = {});

/// Revenue → … → net income for period `period` (bank: interest income → … → net income).
/// Empty when revenue or net income is missing.
QVector<Step> income_waterfall(const Analysis& a, int period);
/// Opening cash → CFO → capex → acquisitions → dividends → buybacks → net debt → other →
/// closing cash. Empty when the cash positions or CFO are missing.
QVector<Step> cash_bridge(const Analysis& a, int period);
/// The tab's KPI ribbon for `period` (industrial or bank set).
QVector<Kpi> kpis(const Analysis& a, int period);

/// Period facts (days, year-ago index) for statement columns.
QVector<PeriodInfo> period_infos(const QVector<fin_model::Column>& cols, Period basis);
/// Growth by magnitude when both readings share a sign; NaN (and *why) otherwise.
double yoy_change(double cur, double prev, QString* why = nullptr);
/// Compound annual growth from the first finite positive value to the one at `last` (-1 = the
/// last finite), over at least ~2 years. NaN (and *why) otherwise.
double cagr(const QVector<double>& v, const QVector<PeriodInfo>& periods, int last = -1, QString* why = nullptr);

/// "Altman Z", "Safe" … for the UI (translated).
QString zone_text(Zone zone);

} // namespace fincept::screens::fin_an
