// src/screens/equity_research/EquityStatementModel.h
//
// Pure statement logic behind the Financials tab (and the Valuation / Dividends views that
// read statement history): ordered line-item templates with hierarchy for industrial
// companies, banks and insurers; fiscal-period labels that respect the fiscal-year end;
// annual / quarterly / trailing-twelve-month columns; as-reported, common-size and
// year-over-year views; ratio series computed on AVERAGE balances; DuPont 3- and 5-step.
//
// No widgets, no I/O. Values are NaN when the filing does not carry a line (a reported
// zero stays 0). Columns are chronological: oldest first, newest last.
#pragma once

#include "services/equity/EquityResearchModels.h"

#include <QDate>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <limits>

namespace fincept::screens::fin_model {

inline constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

enum class Statement { Income, Balance, CashFlow };
enum class Period { Annual, Quarterly, Ttm };
enum class Template { Industrial, Bank, Insurer };
enum class View { AsReported, CommonSize, Growth };

/// What a row holds — decides how it is summed into a TTM, common-sized and formatted.
enum class Kind { Amount, PerShare, Shares, Ratio, Heading };

struct Column {
    QDate end;           ///< period end date
    QString label;       ///< "FY2025", "Q2 FY26", "LTM Q2 FY26"
    QString tooltip;     ///< "Fiscal year ended 31 Mar 2025"
    bool ttm = false;    ///< a trailing-twelve-month column (summed quarters or Yahoo's TTM)
    QJsonObject data;    ///< the line items of this column (summed for TTM flows)
};

struct Row {
    QString id;          ///< the yfinance line it reads ("Total Revenue"), or the heading's label
    QString label;       ///< display label (English source; the view translates it)
    int level = 0;       ///< indentation: 0 = total / heading, 1, 2
    bool total = false;  ///< a subtotal / total line (bold)
    bool templated = true; ///< false = "other line items" the template does not list
    Kind kind = Kind::Amount;
    QVector<double> values; ///< aligned with the columns
};

struct Table {
    QVector<Column> columns;
    QVector<Row> rows;
    /// Common-size denominator per column: revenue (income, cash flow — the cash-flow
    /// columns take it from the income statement of the same period) or total assets.
    QVector<double> common_base;
    Template tmpl = Template::Industrial;
    int fy_end_month = 12; ///< 1..12
};

/// Template from the line items the filing carries (a bank has net interest income and
/// no cost of revenue; an insurer reports policyholder benefits), with the sector as a tie
/// breaker ("Financial Services" + "Insurance" / "Bank" in the industry).
Template detect_template(const services::equity::FinancialsData& fd, const QString& sector = {},
                         const QString& industry = {});

/// Fiscal-year end month (1..12) from the annual period ends; 52/53-week years ending in
/// the first week of a month count as the previous month (Jan 2 → Dec).
int fiscal_year_end_month(const services::equity::FinancialsData& fd);

/// "FY2025" — the fiscal year is named by the calendar year it ends in.
QString fiscal_year_label(const QDate& end, int fy_end_month);
/// "Q2 FY26" for a quarter ending `end`.
QString fiscal_quarter_label(const QDate& end, int fy_end_month);

/// The columns of one statement for a period mode. TTM: every fiscal-year end is a
/// trailing-twelve-month point, plus rolling sums of four consecutive quarters after the
/// last fiscal year (Yahoo's own TTM column fills the latest point when the quarters
/// are incomplete). Balance sheets are point-in-time in every mode.
QVector<Column> columns_for(const services::equity::FinancialsData& fd, Statement st, Period period,
                            int fy_end_month);

/// Rows in template order (with headings), then the lines the template does not list.
Table build(const services::equity::FinancialsData& fd, Statement st, Period period, Template tmpl);

/// The table re-expressed: common size = % of revenue (income, cash flow) or of total
/// assets (balance sheet); growth = change vs the same period a year earlier (annual: the
/// previous year; quarterly: the same quarter a year back; TTM: the TTM a year back).
/// Per-share / share-count rows are common-sized as-is; ratios stay as they are.
Table apply_view(const Table& t, Statement st, View view);

/// First matching line of a column (NaN when none).
double line(const QJsonObject& o, std::initializer_list<const char*> keys);

// ── Ratio series ─────────────────────────────────────────────────────────────

/// One ratio over the columns of a RatioTable.
struct RatioRow {
    QString section;  ///< "PROFITABILITY", "RETURNS", …
    QString id;
    QString label;
    enum class Unit { Percent, Multiple, Days, Money, PerShare, Shares } unit = Unit::Percent;
    QVector<double> values;
    QString note;     ///< definition, for the tooltip
    bool higher_is_better = true;
};

struct RatioTable {
    QVector<Column> columns; ///< period ends (flows are fiscal years or trailing twelve months)
    QVector<RatioRow> rows;
    bool averaged = false;   ///< balances averaged over the period (false = year-end only)
    Template tmpl = Template::Industrial;
};

/// Margins, returns on AVERAGE balances (ROE, ROA, ROIC, ROCE), DuPont 3- and 5-step,
/// leverage, liquidity, efficiency (DSO, DIO, DPO, cash conversion cycle), per-share and
/// share-count series, cash conversion. Annual → fiscal years; Quarterly / TTM → trailing
/// twelve months at each quarter end (flows summed over four quarters, balances averaged
/// with the point a year earlier).
RatioTable ratios(const services::equity::FinancialsData& fd, Period period, Template tmpl);

/// The column index of `rows` whose id is `id`, or -1.
int find_row(const QVector<RatioRow>& rows, const QString& id);
int find_row(const QVector<Row>& rows, const QString& id);

} // namespace fincept::screens::fin_model
