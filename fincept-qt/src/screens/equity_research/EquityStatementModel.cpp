// src/screens/equity_research/EquityStatementModel.cpp
#include "screens/equity_research/EquityStatementModel.h"

#include <QCoreApplication>
#include <QHash>
#include <QLocale>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace fincept::screens::fin_model {

namespace fin_model_detail {

using services::equity::FinancialsData;
using Stmt = QVector<QPair<QString, QJsonObject>>;

bool ok(double v) {
    return std::isfinite(v);
}

enum : unsigned { kTotal = 1, kHeading = 2, kPerShare = 4, kShares = 8, kRatio = 16 };

struct TplLine {
    const char* label;
    const char* keys; ///< '|'-separated yfinance line names, first found wins
    int level;
    unsigned flags;
};

// Labels are translated in the "EquityFinancials" context by the view.
#define FIN_TR(s) QT_TRANSLATE_NOOP("EquityFinancials", s)

const TplLine kIncomeIndustrial[] = {
    {FIN_TR("Revenue"), "Total Revenue|Operating Revenue", 0, kTotal},
    {FIN_TR("Cost of revenue"), "Cost Of Revenue|Reconciled Cost Of Revenue", 1, 0},
    {FIN_TR("Gross profit"), "Gross Profit", 0, kTotal},
    {FIN_TR("Operating expenses"), "Operating Expense", 1, 0},
    {FIN_TR("Research & development"), "Research And Development", 2, 0},
    {FIN_TR("Selling, general & administrative"), "Selling General And Administration", 2, 0},
    {FIN_TR("Selling & marketing"), "Selling And Marketing Expense", 2, 0},
    {FIN_TR("General & administrative"), "General And Administrative Expense", 2, 0},
    {FIN_TR("Depreciation & amortisation"), "Depreciation And Amortization In Income Statement", 2, 0},
    {FIN_TR("Other operating expenses"), "Other Operating Expenses", 2, 0},
    {FIN_TR("Operating income"), "Operating Income|Total Operating Income As Reported", 0, kTotal},
    {FIN_TR("Net non-operating interest"), "Net Non Operating Interest Income Expense", 1, 0},
    {FIN_TR("Interest income"), "Interest Income Non Operating|Interest Income", 2, 0},
    {FIN_TR("Interest expense"), "Interest Expense Non Operating|Interest Expense", 2, 0},
    {FIN_TR("Other income / expense"), "Other Income Expense", 1, 0},
    {FIN_TR("Special income / charges"), "Special Income Charges", 2, 0},
    {FIN_TR("Other non-operating items"), "Other Non Operating Income Expenses", 2, 0},
    {FIN_TR("Pretax income"), "Pretax Income", 0, kTotal},
    {FIN_TR("Income tax"), "Tax Provision", 1, 0},
    {FIN_TR("Net income from continuing operations"), "Net Income Continuous Operations", 1, 0},
    {FIN_TR("Discontinued operations"), "Net Income Discontinuous Operations", 1, 0},
    {FIN_TR("Minority interests"), "Minority Interests", 1, 0},
    {FIN_TR("Net income"), "Net Income|Net Income Including Noncontrolling Interests", 0, kTotal},
    {FIN_TR("Preferred dividends"), "Preferred Stock Dividends", 1, 0},
    {FIN_TR("Net income to common"), "Net Income Common Stockholders", 1, 0},
    {FIN_TR("PER SHARE"), "", 0, kHeading},
    {FIN_TR("EPS — basic"), "Basic EPS", 1, kPerShare},
    {FIN_TR("EPS — diluted"), "Diluted EPS", 1, kPerShare},
    {FIN_TR("Average shares — basic"), "Basic Average Shares", 1, kShares},
    {FIN_TR("Average shares — diluted"), "Diluted Average Shares", 1, kShares},
    {FIN_TR("SUPPLEMENTAL"), "", 0, kHeading},
    {FIN_TR("EBIT"), "EBIT", 1, 0},
    {FIN_TR("EBITDA"), "EBITDA", 1, 0},
    {FIN_TR("EBITDA (normalised)"), "Normalized EBITDA", 1, 0},
    {FIN_TR("Net income (normalised)"), "Normalized Income", 1, 0},
    {FIN_TR("Depreciation (reconciled)"), "Reconciled Depreciation", 1, 0},
    {FIN_TR("Unusual items"), "Total Unusual Items", 1, 0},
    {FIN_TR("Effective tax rate"), "Tax Rate For Calcs", 1, kRatio},
};

const TplLine kIncomeBank[] = {
    {FIN_TR("Total revenue (net)"), "Total Revenue|Operating Revenue", 0, kTotal},
    {FIN_TR("Interest income"), "Interest Income", 1, 0},
    {FIN_TR("Interest expense"), "Interest Expense", 1, 0},
    {FIN_TR("Net interest income"), "Net Interest Income", 1, kTotal},
    {FIN_TR("Non-interest income"), "Non Interest Income|Total Other Finance Cost", 1, 0},
    {FIN_TR("Fees & commissions"), "Fees And Commissions|Fees And Commission Income", 2, 0},
    {FIN_TR("Gain on sale of securities"), "Gain On Sale Of Security", 2, 0},
    {FIN_TR("Provision for credit losses"), "Credit Losses Provision|Provision For Doubtful Accounts", 1, 0},
    {FIN_TR("Non-interest expense"), "Non Interest Expense|Selling General And Administration", 1, 0},
    {FIN_TR("Salaries & wages"), "Salaries And Wages", 2, 0},
    {FIN_TR("Occupancy & equipment"), "Occupancy And Equipment", 2, 0},
    {FIN_TR("Professional & contract services"), "Professional Expense And Contract Services Expense", 2, 0},
    {FIN_TR("Selling & marketing"), "Selling And Marketing Expense", 2, 0},
    {FIN_TR("General & administrative"), "General And Administrative Expense", 2, 0},
    {FIN_TR("Other non-interest expense"), "Other Non Interest Expense", 2, 0},
    {FIN_TR("Special charges"), "Special Income Charges", 1, 0},
    {FIN_TR("Pretax income"), "Pretax Income", 0, kTotal},
    {FIN_TR("Income tax"), "Tax Provision", 1, 0},
    {FIN_TR("Net income"), "Net Income|Net Income Including Noncontrolling Interests", 0, kTotal},
    {FIN_TR("Preferred dividends"), "Preferred Stock Dividends", 1, 0},
    {FIN_TR("Net income to common"), "Net Income Common Stockholders", 1, 0},
    {FIN_TR("PER SHARE"), "", 0, kHeading},
    {FIN_TR("EPS — basic"), "Basic EPS", 1, kPerShare},
    {FIN_TR("EPS — diluted"), "Diluted EPS", 1, kPerShare},
    {FIN_TR("Average shares — basic"), "Basic Average Shares", 1, kShares},
    {FIN_TR("Average shares — diluted"), "Diluted Average Shares", 1, kShares},
    {FIN_TR("SUPPLEMENTAL"), "", 0, kHeading},
    {FIN_TR("Net income (normalised)"), "Normalized Income", 1, 0},
    {FIN_TR("Unusual items"), "Total Unusual Items", 1, 0},
    {FIN_TR("Effective tax rate"), "Tax Rate For Calcs", 1, kRatio},
};

const TplLine kIncomeInsurer[] = {
    {FIN_TR("Total revenue"), "Total Revenue|Operating Revenue", 0, kTotal},
    {FIN_TR("Premiums earned"), "Total Premiums Earned|Net Premiums Written", 1, 0},
    {FIN_TR("Net investment income"), "Net Investment Income|Interest Income", 1, 0},
    {FIN_TR("Total benefits & expenses"), "Total Expenses", 0, kTotal},
    {FIN_TR("Policyholder benefits & claims (net)"), "Net Policyholder Benefits And Claims", 1, 0},
    {FIN_TR("Benefits — gross"), "Policyholder Benefits Gross", 2, 0},
    {FIN_TR("Benefits — ceded to reinsurers"), "Policyholder Benefits Ceded", 2, 0},
    {FIN_TR("Loss adjustment expense"), "Loss Adjustment Expense", 1, 0},
    {FIN_TR("Selling, general & administrative"), "Selling General And Administration", 1, 0},
    {FIN_TR("Other operating expenses"), "Other Operating Expenses", 1, 0},
    {FIN_TR("Depreciation & amortisation"), "Depreciation And Amortization In Income Statement", 1, 0},
    {FIN_TR("EBIT"), "EBIT", 0, kTotal},
    {FIN_TR("Interest expense"), "Interest Expense Non Operating|Interest Expense", 1, 0},
    {FIN_TR("Special charges"), "Special Income Charges", 1, 0},
    {FIN_TR("Pretax income"), "Pretax Income", 0, kTotal},
    {FIN_TR("Income tax"), "Tax Provision", 1, 0},
    {FIN_TR("Discontinued operations"), "Net Income Discontinuous Operations", 1, 0},
    {FIN_TR("Minority interests"), "Minority Interests", 1, 0},
    {FIN_TR("Net income"), "Net Income|Net Income Including Noncontrolling Interests", 0, kTotal},
    {FIN_TR("Preferred dividends"), "Preferred Stock Dividends", 1, 0},
    {FIN_TR("Net income to common"), "Net Income Common Stockholders", 1, 0},
    {FIN_TR("PER SHARE"), "", 0, kHeading},
    {FIN_TR("EPS — basic"), "Basic EPS", 1, kPerShare},
    {FIN_TR("EPS — diluted"), "Diluted EPS", 1, kPerShare},
    {FIN_TR("Average shares — basic"), "Basic Average Shares", 1, kShares},
    {FIN_TR("Average shares — diluted"), "Diluted Average Shares", 1, kShares},
    {FIN_TR("SUPPLEMENTAL"), "", 0, kHeading},
    {FIN_TR("Net income (normalised)"), "Normalized Income", 1, 0},
    {FIN_TR("Unusual items"), "Total Unusual Items", 1, 0},
    {FIN_TR("Effective tax rate"), "Tax Rate For Calcs", 1, kRatio},
};

const TplLine kBalanceIndustrial[] = {
    {FIN_TR("ASSETS"), "", 0, kHeading},
    {FIN_TR("Cash, equivalents & short-term investments"), "Cash Cash Equivalents And Short Term Investments", 1, 0},
    {FIN_TR("Cash & equivalents"), "Cash And Cash Equivalents", 2, 0},
    {FIN_TR("Short-term investments"), "Other Short Term Investments", 2, 0},
    {FIN_TR("Receivables"), "Receivables", 1, 0},
    {FIN_TR("Accounts receivable"), "Accounts Receivable", 2, 0},
    {FIN_TR("Inventory"), "Inventory", 1, 0},
    {FIN_TR("Other current assets"), "Other Current Assets", 1, 0},
    {FIN_TR("Total current assets"), "Current Assets", 0, kTotal},
    {FIN_TR("Net PP&E"), "Net PPE", 1, 0},
    {FIN_TR("Gross PP&E"), "Gross PPE", 2, 0},
    {FIN_TR("Accumulated depreciation"), "Accumulated Depreciation", 2, 0},
    {FIN_TR("Goodwill & intangibles"), "Goodwill And Other Intangible Assets", 1, 0},
    {FIN_TR("Goodwill"), "Goodwill", 2, 0},
    {FIN_TR("Other intangibles"), "Other Intangible Assets", 2, 0},
    {FIN_TR("Investments & advances"), "Investments And Advances", 1, 0},
    {FIN_TR("Deferred tax assets"), "Non Current Deferred Taxes Assets|Non Current Deferred Assets", 1, 0},
    {FIN_TR("Other non-current assets"), "Other Non Current Assets", 1, 0},
    {FIN_TR("Total non-current assets"), "Total Non Current Assets", 0, kTotal},
    {FIN_TR("Total assets"), "Total Assets", 0, kTotal},
    {FIN_TR("LIABILITIES"), "", 0, kHeading},
    {FIN_TR("Payables & accrued expenses"), "Payables And Accrued Expenses", 1, 0},
    {FIN_TR("Accounts payable"), "Accounts Payable", 2, 0},
    {FIN_TR("Accrued expenses"), "Current Accrued Expenses", 2, 0},
    {FIN_TR("Short-term debt"), "Current Debt And Capital Lease Obligation|Current Debt", 1, 0},
    {FIN_TR("Deferred revenue (current)"), "Current Deferred Revenue|Current Deferred Liabilities", 1, 0},
    {FIN_TR("Other current liabilities"), "Other Current Liabilities", 1, 0},
    {FIN_TR("Total current liabilities"), "Current Liabilities", 0, kTotal},
    {FIN_TR("Long-term debt"), "Long Term Debt And Capital Lease Obligation|Long Term Debt", 1, 0},
    {FIN_TR("Deferred tax liabilities"), "Non Current Deferred Taxes Liabilities|Non Current Deferred Liabilities", 1, 0},
    {FIN_TR("Other non-current liabilities"), "Other Non Current Liabilities", 1, 0},
    {FIN_TR("Total non-current liabilities"), "Total Non Current Liabilities Net Minority Interest", 0, kTotal},
    {FIN_TR("Total liabilities"), "Total Liabilities Net Minority Interest", 0, kTotal},
    {FIN_TR("EQUITY"), "", 0, kHeading},
    {FIN_TR("Share capital"), "Capital Stock|Common Stock", 1, 0},
    {FIN_TR("Additional paid-in capital"), "Additional Paid In Capital", 1, 0},
    {FIN_TR("Retained earnings"), "Retained Earnings", 1, 0},
    {FIN_TR("Treasury stock"), "Treasury Stock", 1, 0},
    {FIN_TR("Accumulated other comprehensive income"), "Gains Losses Not Affecting Retained Earnings", 1, 0},
    {FIN_TR("Shareholders' equity"), "Stockholders Equity|Common Stock Equity", 0, kTotal},
    {FIN_TR("Minority interest"), "Minority Interest", 1, 0},
    {FIN_TR("Total equity"), "Total Equity Gross Minority Interest", 0, kTotal},
    {FIN_TR("SUPPLEMENTAL"), "", 0, kHeading},
    {FIN_TR("Total debt"), "Total Debt", 1, 0},
    {FIN_TR("Net debt"), "Net Debt", 1, 0},
    {FIN_TR("Working capital"), "Working Capital", 1, 0},
    {FIN_TR("Invested capital"), "Invested Capital", 1, 0},
    {FIN_TR("Tangible book value"), "Tangible Book Value", 1, 0},
    {FIN_TR("Shares outstanding"), "Ordinary Shares Number|Share Issued", 1, kShares},
    {FIN_TR("Treasury shares"), "Treasury Shares Number", 1, kShares},
};

const TplLine kBalanceBank[] = {
    {FIN_TR("ASSETS"), "", 0, kHeading},
    {FIN_TR("Cash & due from banks"), "Cash And Cash Equivalents|Cash Financial", 1, 0},
    {FIN_TR("Cash incl. fed funds sold"), "Cash Cash Equivalents And Federal Funds Sold", 1, 0},
    {FIN_TR("Trading securities"), "Trading Securities", 1, 0},
    {FIN_TR("Available-for-sale securities"), "Available For Sale Securities", 1, 0},
    {FIN_TR("Held-to-maturity securities"), "Held To Maturity Securities", 1, 0},
    {FIN_TR("Short-term investments"), "Other Short Term Investments", 1, 0},
    {FIN_TR("Net loans"), "Net Loan|Gross Loan|Loans Receivable", 1, 0},
    {FIN_TR("Allowance for loan losses"), "Allowance For Loans And Lease Losses", 2, 0},
    {FIN_TR("Receivables"), "Receivables|Accounts Receivable", 1, 0},
    {FIN_TR("Investments & advances"), "Investments And Advances", 1, 0},
    {FIN_TR("Goodwill & intangibles"), "Goodwill And Other Intangible Assets", 1, 0},
    {FIN_TR("Net PP&E"), "Net PPE", 1, 0},
    {FIN_TR("Total assets"), "Total Assets", 0, kTotal},
    {FIN_TR("LIABILITIES"), "", 0, kHeading},
    {FIN_TR("Deposits"), "Total Deposits|Deposits", 1, 0},
    {FIN_TR("Short-term borrowings"), "Other Current Borrowings|Current Debt And Capital Lease Obligation|Current Debt", 1, 0},
    {FIN_TR("Payables & accrued expenses"), "Payables And Accrued Expenses", 1, 0},
    {FIN_TR("Long-term debt"), "Long Term Debt And Capital Lease Obligation|Long Term Debt", 1, 0},
    {FIN_TR("Total liabilities"), "Total Liabilities Net Minority Interest", 0, kTotal},
    {FIN_TR("EQUITY"), "", 0, kHeading},
    {FIN_TR("Preferred stock"), "Preferred Stock Equity|Preferred Stock", 1, 0},
    {FIN_TR("Share capital"), "Capital Stock|Common Stock", 1, 0},
    {FIN_TR("Additional paid-in capital"), "Additional Paid In Capital", 1, 0},
    {FIN_TR("Retained earnings"), "Retained Earnings", 1, 0},
    {FIN_TR("Treasury stock"), "Treasury Stock", 1, 0},
    {FIN_TR("Accumulated other comprehensive income"), "Gains Losses Not Affecting Retained Earnings", 1, 0},
    {FIN_TR("Shareholders' equity"), "Stockholders Equity", 0, kTotal},
    {FIN_TR("Common equity"), "Common Stock Equity", 1, 0},
    {FIN_TR("Total equity"), "Total Equity Gross Minority Interest", 0, kTotal},
    {FIN_TR("SUPPLEMENTAL"), "", 0, kHeading},
    {FIN_TR("Total debt"), "Total Debt", 1, 0},
    {FIN_TR("Tangible book value"), "Tangible Book Value", 1, 0},
    {FIN_TR("Shares outstanding"), "Ordinary Shares Number|Share Issued", 1, kShares},
};

const TplLine kBalanceInsurer[] = {
    {FIN_TR("ASSETS"), "", 0, kHeading},
    {FIN_TR("Investments"), "Investments And Advances|Investmentin Financial Assets", 1, 0},
    {FIN_TR("Available-for-sale securities"), "Available For Sale Securities", 2, 0},
    {FIN_TR("Equity-method investments"), "Long Term Equity Investment", 2, 0},
    {FIN_TR("Cash, equivalents & short-term investments"), "Cash Cash Equivalents And Short Term Investments", 1, 0},
    {FIN_TR("Cash & equivalents"), "Cash And Cash Equivalents", 2, 0},
    {FIN_TR("Premiums & other receivables"), "Receivables|Accounts Receivable|Premiums Receivable", 1, 0},
    {FIN_TR("Loans"), "Loans Receivable", 1, 0},
    {FIN_TR("Reinsurance recoverable"), "Reinsurance Recoverable|Reinsurance Balances Recoverable", 1, 0},
    {FIN_TR("Deferred acquisition costs"), "Deferred Policy Acquisition Costs|Deferred Costs", 1, 0},
    {FIN_TR("Goodwill & intangibles"), "Goodwill And Other Intangible Assets", 1, 0},
    {FIN_TR("Net PP&E"), "Net PPE", 1, 0},
    {FIN_TR("Total assets"), "Total Assets", 0, kTotal},
    {FIN_TR("LIABILITIES"), "", 0, kHeading},
    {FIN_TR("Loss & loss-adjustment reserves"), "Unpaid Loss And Loss Reserve|Loss And Loss Adjustment Expense Reserves", 1, 0},
    {FIN_TR("Unearned premiums"), "Unearned Income|Unearned Premiums", 1, 0},
    {FIN_TR("Policy reserves"), "Policy Reserves Benefits|Future Policy Benefits", 1, 0},
    {FIN_TR("Short-term debt"), "Current Debt And Capital Lease Obligation|Current Debt|Current Notes Payable", 1, 0},
    {FIN_TR("Long-term debt"), "Long Term Debt And Capital Lease Obligation|Long Term Debt", 1, 0},
    {FIN_TR("Deferred tax liabilities"), "Non Current Deferred Taxes Liabilities|Non Current Deferred Liabilities", 1, 0},
    {FIN_TR("Total liabilities"), "Total Liabilities Net Minority Interest", 0, kTotal},
    {FIN_TR("EQUITY"), "", 0, kHeading},
    {FIN_TR("Preferred stock"), "Preferred Stock Equity|Preferred Stock", 1, 0},
    {FIN_TR("Share capital"), "Capital Stock|Common Stock", 1, 0},
    {FIN_TR("Additional paid-in capital"), "Additional Paid In Capital", 1, 0},
    {FIN_TR("Retained earnings"), "Retained Earnings", 1, 0},
    {FIN_TR("Treasury stock"), "Treasury Stock", 1, 0},
    {FIN_TR("Accumulated other comprehensive income"), "Gains Losses Not Affecting Retained Earnings", 1, 0},
    {FIN_TR("Shareholders' equity"), "Stockholders Equity", 0, kTotal},
    {FIN_TR("Minority interest"), "Minority Interest", 1, 0},
    {FIN_TR("Total equity"), "Total Equity Gross Minority Interest", 0, kTotal},
    {FIN_TR("SUPPLEMENTAL"), "", 0, kHeading},
    {FIN_TR("Total debt"), "Total Debt", 1, 0},
    {FIN_TR("Tangible book value"), "Tangible Book Value", 1, 0},
    {FIN_TR("Shares outstanding"), "Ordinary Shares Number|Share Issued", 1, kShares},
};

const TplLine kCashFlow[] = {
    {FIN_TR("OPERATING ACTIVITIES"), "", 0, kHeading},
    {FIN_TR("Net income"), "Net Income From Continuing Operations", 1, 0},
    {FIN_TR("Depreciation & amortisation"), "Depreciation And Amortization|Depreciation Amortization Depletion|Depreciation", 1, 0},
    {FIN_TR("Stock-based compensation"), "Stock Based Compensation", 1, 0},
    {FIN_TR("Deferred taxes"), "Deferred Tax|Deferred Income Tax", 1, 0},
    {FIN_TR("Gains / losses"), "Operating Gains Losses", 1, 0},
    {FIN_TR("Other non-cash items"), "Other Non Cash Items", 1, 0},
    {FIN_TR("Change in working capital"), "Change In Working Capital", 1, 0},
    {FIN_TR("Receivables"), "Change In Receivables|Changes In Account Receivables", 2, 0},
    {FIN_TR("Inventory"), "Change In Inventory", 2, 0},
    {FIN_TR("Payables"), "Change In Payables And Accrued Expense|Change In Payable", 2, 0},
    {FIN_TR("Other working capital"), "Change In Other Working Capital", 2, 0},
    {FIN_TR("Cash from operations"), "Operating Cash Flow|Cash Flow From Continuing Operating Activities", 0, kTotal},
    {FIN_TR("INVESTING ACTIVITIES"), "", 0, kHeading},
    {FIN_TR("Capital expenditure"), "Capital Expenditure|Purchase Of PPE", 1, 0},
    {FIN_TR("Sale of PP&E"), "Sale Of PPE", 1, 0},
    {FIN_TR("Acquisitions"), "Purchase Of Business|Net Business Purchase And Sale", 1, 0},
    {FIN_TR("Purchase of investments"), "Purchase Of Investment", 1, 0},
    {FIN_TR("Sale of investments"), "Sale Of Investment", 1, 0},
    {FIN_TR("Other investing"), "Net Other Investing Changes", 1, 0},
    {FIN_TR("Cash from investing"), "Investing Cash Flow|Cash Flow From Continuing Investing Activities", 0, kTotal},
    {FIN_TR("FINANCING ACTIVITIES"), "", 0, kHeading},
    {FIN_TR("Debt issued"), "Issuance Of Debt|Long Term Debt Issuance", 1, 0},
    {FIN_TR("Debt repaid"), "Repayment Of Debt|Long Term Debt Payments", 1, 0},
    {FIN_TR("Shares issued"), "Issuance Of Capital Stock|Common Stock Issuance", 1, 0},
    {FIN_TR("Share buybacks"), "Repurchase Of Capital Stock|Common Stock Payments", 1, 0},
    {FIN_TR("Dividends paid"), "Cash Dividends Paid|Common Stock Dividend Paid", 1, 0},
    {FIN_TR("Other financing"), "Net Other Financing Charges", 1, 0},
    {FIN_TR("Cash from financing"), "Financing Cash Flow|Cash Flow From Continuing Financing Activities", 0, kTotal},
    {FIN_TR("Effect of exchange rates"), "Effect Of Exchange Rate Changes", 1, 0},
    {FIN_TR("Net change in cash"), "Changes In Cash", 0, kTotal},
    {FIN_TR("Cash at beginning"), "Beginning Cash Position", 1, 0},
    {FIN_TR("Cash at end"), "End Cash Position", 1, 0},
    {FIN_TR("SUPPLEMENTAL"), "", 0, kHeading},
    {FIN_TR("Free cash flow"), "Free Cash Flow", 1, kTotal},
    {FIN_TR("Interest paid"), "Interest Paid Supplemental Data", 1, 0},
    {FIN_TR("Income tax paid"), "Income Tax Paid Supplemental Data", 1, 0},
};

#undef FIN_TR

struct TplRef {
    const TplLine* lines;
    int count;
};

template <int N>
TplRef tpl(const TplLine (&a)[N]) {
    return {a, N};
}

TplRef template_for(Statement st, Template t) {
    switch (st) {
        case Statement::Income:
            return t == Template::Bank ? tpl(kIncomeBank) : t == Template::Insurer ? tpl(kIncomeInsurer) : tpl(kIncomeIndustrial);
        case Statement::Balance:
            return t == Template::Bank ? tpl(kBalanceBank)
                                       : t == Template::Insurer ? tpl(kBalanceInsurer) : tpl(kBalanceIndustrial);
        case Statement::CashFlow:
        default:
            return tpl(kCashFlow);
    }
}

/// How a line that is not in a template behaves in a TTM sum / common size.
Kind kind_of_line(const QString& name) {
    if (name.contains(QLatin1String("EPS")))
        return Kind::PerShare;
    if (name.contains(QLatin1String("Shares")) || name == QLatin1String("Share Issued"))
        return Kind::Shares;
    if (name.contains(QLatin1String("Tax Rate")))
        return Kind::Ratio;
    return Kind::Amount;
}

double value_of(const QJsonObject& o, const QString& key) {
    const QJsonValue v = o.value(key);
    if (v.isDouble())
        return v.toDouble();
    if (v.isString()) {
        bool good = false;
        const double d = v.toString().toDouble(&good);
        if (good)
            return d;
    }
    return kNaN;
}

QDate col_date(const QString& period) {
    return QDate::fromString(period.left(10), Qt::ISODate);
}

/// A 52/53-week period ending in the first days of a month belongs to the previous month.
QDate effective_end(const QDate& d) {
    return (d.isValid() && d.day() <= 7) ? d.addDays(-d.day()) : d;
}

QString long_date(const QDate& d) {
    return QLocale().toString(d, QLocale::LongFormat);
}

/// Index of the column whose end is within `tol` days of `d`, or -1.
int match_date(const QVector<Column>& cols, const QDate& d, int tol = 12) {
    int best = -1;
    qint64 gap = tol + 1;
    for (int i = 0; i < cols.size(); ++i) {
        const qint64 g = qAbs(cols[i].end.daysTo(d));
        if (g < gap) {
            gap = g;
            best = i;
        }
    }
    return best;
}

QVector<Column> raw_columns(const Stmt& stmt) {
    QVector<Column> out;
    for (const auto& p : stmt) {
        Column c;
        c.end = col_date(p.first);
        if (!c.end.isValid())
            continue;
        c.data = p.second;
        out.append(c);
    }
    std::sort(out.begin(), out.end(), [](const Column& a, const Column& b) { return a.end < b.end; });
    // Yahoo pads the oldest period(s) with a stub of a few stray lines (interest, lease
    // or supplemental items: 6 of 33 for AAPL) and none of the anchors — revenue, total
    // assets, operating cash flow. Shown, it is a column of dashes and an empty slot in
    // every chart. Drop leading stubs that carry under a quarter of the fullest column.
    auto finite_count = [](const QJsonObject& o) {
        int n = 0;
        for (auto it = o.constBegin(); it != o.constEnd(); ++it)
            n += ok(value_of(o, it.key())) ? 1 : 0;
        return n;
    };
    int most = 0;
    for (const auto& c : out)
        most = std::max(most, finite_count(c.data));
    while (out.size() > 1 && most >= 12 && finite_count(out.first().data) * 4 < most)
        out.removeFirst();
    return out;
}

const Stmt& annual_of(const FinancialsData& fd, Statement st) {
    return st == Statement::Income ? fd.income_statement : st == Statement::Balance ? fd.balance_sheet : fd.cash_flow;
}
const Stmt& quarterly_of(const FinancialsData& fd, Statement st) {
    return st == Statement::Income ? fd.q_income : st == Statement::Balance ? fd.q_balance : fd.q_cash_flow;
}
const QStringList& order_of(const FinancialsData& fd, Statement st, bool quarterly) {
    if (quarterly)
        return st == Statement::Income ? fd.q_income_order : st == Statement::Balance ? fd.q_balance_order : fd.q_cash_flow_order;
    return st == Statement::Income ? fd.income_order : st == Statement::Balance ? fd.balance_order : fd.cash_flow_order;
}

/// Four consecutive quarters ending at quarters[i] summed (amounts and per-share figures),
/// shares averaged; a line missing in any of the four is left out. Empty when the four are
/// not consecutive (a quarter Yahoo skipped).
QJsonObject sum_four_quarters(const QVector<Column>& q, int i) {
    if (i < 3)
        return {};
    for (int k = i; k > i - 3; --k) {
        const qint64 gap = q[k - 1].end.daysTo(q[k].end);
        if (gap < 80 || gap > 100)
            return {};
    }
    QJsonObject out;
    for (auto it = q[i].data.constBegin(); it != q[i].data.constEnd(); ++it) {
        const Kind kind = kind_of_line(it.key());
        if (kind == Kind::Ratio)
            continue;
        // The cash positions of a cash-flow statement are stocks, not flows: the twelve
        // months open with the first quarter's opening cash and close with the last one's.
        if (it.key() == QLatin1String("Beginning Cash Position") || it.key() == QLatin1String("End Cash Position")) {
            const bool opening = it.key() == QLatin1String("Beginning Cash Position");
            const double v = value_of(q[opening ? i - 3 : i].data, it.key());
            if (ok(v))
                out.insert(it.key(), v);
            continue;
        }
        double acc = 0.0;
        bool all = true;
        for (int k = i - 3; k <= i; ++k) {
            const double v = value_of(q[k].data, it.key());
            if (!ok(v)) {
                all = false;
                break;
            }
            acc += v;
        }
        if (!all)
            continue;
        out.insert(it.key(), kind == Kind::Shares ? acc / 4.0 : acc);
    }
    return out;
}

} // namespace fin_model_detail

using namespace fin_model_detail;

// ── Public helpers ───────────────────────────────────────────────────────────

double line(const QJsonObject& o, std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        const double v = value_of(o, QLatin1String(k));
        if (ok(v))
            return v;
    }
    return kNaN;
}

int find_row(const QVector<RatioRow>& rows, const QString& id) {
    for (int i = 0; i < rows.size(); ++i)
        if (rows[i].id == id)
            return i;
    return -1;
}

int find_row(const QVector<Row>& rows, const QString& id) {
    for (int i = 0; i < rows.size(); ++i)
        if (rows[i].id == id)
            return i;
    return -1;
}

Template detect_template(const FinancialsData& fd, const QString& sector, const QString& industry) {
    QSet<QString> keys;
    for (const auto* stmt : {&fd.income_statement, &fd.q_income})
        for (const auto& p : *stmt)
            for (auto it = p.second.constBegin(); it != p.second.constEnd(); ++it)
                keys.insert(it.key());
    const bool insurance_lines = keys.contains(QStringLiteral("Net Policyholder Benefits And Claims")) ||
                                 keys.contains(QStringLiteral("Policyholder Benefits Gross")) ||
                                 keys.contains(QStringLiteral("Loss Adjustment Expense")) ||
                                 keys.contains(QStringLiteral("Total Premiums Earned"));
    const bool industrial_lines = keys.contains(QStringLiteral("Cost Of Revenue")) ||
                                  keys.contains(QStringLiteral("Gross Profit"));
    const bool bank_lines = keys.contains(QStringLiteral("Net Interest Income")) && !industrial_lines &&
                            (keys.contains(QStringLiteral("Interest Income")) ||
                             keys.contains(QStringLiteral("Occupancy And Equipment")) ||
                             keys.contains(QStringLiteral("Other Non Interest Expense")));
    const QString ind = industry.toLower();
    if (insurance_lines || ind.contains(QLatin1String("insurance")))
        return Template::Insurer;
    if (bank_lines || ind.contains(QLatin1String("bank")))
        return Template::Bank;
    if (!industrial_lines && sector.contains(QLatin1String("Financial"), Qt::CaseInsensitive) &&
        keys.contains(QStringLiteral("Net Interest Income")))
        return Template::Bank;
    return Template::Industrial;
}

int fiscal_year_end_month(const FinancialsData& fd) {
    QHash<int, int> votes;
    for (const auto& p : fd.income_statement) {
        const QDate d = effective_end(col_date(p.first));
        if (d.isValid())
            ++votes[d.month()];
    }
    if (votes.isEmpty())
        for (const auto& p : fd.balance_sheet) {
            const QDate d = effective_end(col_date(p.first));
            if (d.isValid())
                ++votes[d.month()];
        }
    int best = 12, n = 0;
    for (auto it = votes.constBegin(); it != votes.constEnd(); ++it)
        if (it.value() > n) {
            n = it.value();
            best = it.key();
        }
    return best;
}

QString fiscal_year_label(const QDate& end, int fy_end_month) {
    Q_UNUSED(fy_end_month);
    const QDate e = effective_end(end);
    return QStringLiteral("FY%1").arg(e.year());
}

QString fiscal_quarter_label(const QDate& end, int fy_end_month) {
    const QDate e = effective_end(end);
    const int months_after = ((e.month() - fy_end_month) % 12 + 12) % 12;
    const int q = months_after == 0 ? 4 : (months_after + 2) / 3;
    const int fy = e.month() <= fy_end_month ? e.year() : e.year() + 1;
    return QStringLiteral("Q%1 FY%2").arg(q).arg(fy % 100, 2, 10, QLatin1Char('0'));
}

QVector<Column> columns_for(const FinancialsData& fd, Statement st, Period period, int fy) {
    const char* ctx = "EquityFinancials";
    auto label_annual = [&](Column& c) {
        c.label = fiscal_year_label(c.end, fy);
        c.tooltip = QCoreApplication::translate(ctx, "Fiscal year ended %1").arg(long_date(c.end));
    };
    auto label_quarter = [&](Column& c) {
        c.label = fiscal_quarter_label(c.end, fy);
        c.tooltip = QCoreApplication::translate(ctx, "Quarter ended %1").arg(long_date(c.end));
    };

    if (period == Period::Annual) {
        QVector<Column> cols = raw_columns(annual_of(fd, st));
        for (auto& c : cols)
            label_annual(c);
        return cols;
    }
    QVector<Column> quarters = raw_columns(quarterly_of(fd, st));
    if (period == Period::Quarterly) {
        for (auto& c : quarters)
            label_quarter(c);
        return quarters;
    }

    // TTM: each fiscal year is a trailing-twelve-month point …
    QVector<Column> cols = raw_columns(annual_of(fd, st));
    for (auto& c : cols) {
        label_annual(c);
        c.ttm = st != Statement::Balance;
    }
    const QDate last_fy = cols.isEmpty() ? QDate() : cols.last().end;
    // … then the quarters after the last fiscal year.
    for (int i = 0; i < quarters.size(); ++i) {
        const Column& qc = quarters[i];
        if (last_fy.isValid() && qc.end <= last_fy.addDays(10))
            continue;
        Column c;
        c.end = qc.end;
        if (st == Statement::Balance) {
            c.data = qc.data;
            label_quarter(c);
        } else {
            c.data = sum_four_quarters(quarters, i);
            c.ttm = true;
            c.label = QCoreApplication::translate(ctx, "LTM %1").arg(fiscal_quarter_label(c.end, fy));
            c.tooltip = QCoreApplication::translate(ctx, "Twelve months to %1").arg(long_date(c.end));
            if (c.data.isEmpty())
                continue; // filled from Yahoo's TTM column below when it is the latest
        }
        cols.append(c);
    }
    // Yahoo's own TTM column when the quarters could not be summed for that date.
    if (st != Statement::Balance) {
        const Stmt& ttm = st == Statement::Income ? fd.ttm_income : fd.ttm_cash_flow;
        for (const auto& p : ttm) {
            const QDate d = col_date(p.first);
            if (!d.isValid() || p.second.isEmpty() || match_date(cols, d) >= 0)
                continue;
            if (last_fy.isValid() && d <= last_fy)
                continue;
            Column c;
            c.end = d;
            c.data = p.second;
            c.ttm = true;
            c.label = QCoreApplication::translate(ctx, "LTM %1").arg(fiscal_quarter_label(d, fy));
            c.tooltip = QCoreApplication::translate(ctx, "Twelve months to %1 (Yahoo trailing figures)").arg(long_date(d));
            cols.append(c);
        }
    }
    std::sort(cols.begin(), cols.end(), [](const Column& a, const Column& b) { return a.end < b.end; });
    return cols;
}

Table build(const FinancialsData& fd, Statement st, Period period, Template tmpl) {
    Table t;
    t.tmpl = tmpl;
    t.fy_end_month = fiscal_year_end_month(fd);
    t.columns = columns_for(fd, st, period, t.fy_end_month);
    const int n = static_cast<int>(t.columns.size());

    QSet<QString> present;
    for (const auto& c : t.columns)
        for (auto it = c.data.constBegin(); it != c.data.constEnd(); ++it)
            present.insert(it.key());

    QSet<QString> used;
    const TplRef ref = template_for(st, tmpl);
    for (int i = 0; i < ref.count; ++i) {
        const TplLine& tl = ref.lines[i];
        Row r;
        r.label = QString::fromUtf8(tl.label);
        r.level = tl.level;
        r.total = (tl.flags & kTotal) != 0;
        if (tl.flags & kHeading) {
            r.kind = Kind::Heading;
            r.id = r.label;
            r.values = QVector<double>(n, kNaN);
            t.rows.append(r);
            continue;
        }
        r.kind = (tl.flags & kPerShare) ? Kind::PerShare
                 : (tl.flags & kShares) ? Kind::Shares
                 : (tl.flags & kRatio)  ? Kind::Ratio
                                        : Kind::Amount;
        const QStringList keys = QString::fromLatin1(tl.keys).split(QLatin1Char('|'), Qt::SkipEmptyParts);
        bool any_unused = false;
        for (const auto& k : keys)
            if (present.contains(k) && !used.contains(k)) {
                any_unused = true;
                if (r.id.isEmpty())
                    r.id = k;
            }
        if (!any_unused)
            continue; // not reported (or already shown under another label)
        r.values.resize(n);
        bool has_value = false;
        for (int c = 0; c < n; ++c) {
            double v = kNaN;
            for (const auto& k : keys) {
                if (used.contains(k))
                    continue;
                v = value_of(t.columns[c].data, k);
                if (ok(v))
                    break;
            }
            r.values[c] = v;
            has_value = has_value || ok(v);
        }
        for (const auto& k : keys)
            used.insert(k);
        if (has_value)
            t.rows.append(r);
    }

    // Lines the template does not list, in the source's presentation order (yfinance
    // lists them bottom-up), then A–Z for anything the order does not mention.
    QStringList order = order_of(fd, st, period == Period::Quarterly);
    if (order.isEmpty())
        order = order_of(fd, st, period != Period::Quarterly);
    std::reverse(order.begin(), order.end());
    QStringList rest;
    for (const auto& k : order)
        if (present.contains(k) && !used.contains(k) && !rest.contains(k))
            rest << k;
    QStringList leftovers;
    for (const auto& k : present)
        if (!used.contains(k) && !rest.contains(k))
            leftovers << k;
    leftovers.sort();
    rest << leftovers;
    if (!rest.isEmpty()) {
        Row h;
        h.kind = Kind::Heading;
        h.label = QStringLiteral(QT_TRANSLATE_NOOP("EquityFinancials", "OTHER REPORTED LINES"));
        h.id = h.label;
        h.values = QVector<double>(n, kNaN);
        t.rows.append(h);
        for (const auto& k : rest) {
            Row r;
            r.id = k;
            r.label = k;
            r.level = 1;
            r.templated = false;
            r.kind = kind_of_line(k);
            r.values.resize(n);
            bool has_value = false;
            for (int c = 0; c < n; ++c) {
                r.values[c] = value_of(t.columns[c].data, k);
                has_value = has_value || ok(r.values[c]);
            }
            if (has_value)
                t.rows.append(r);
        }
    }

    // Drop headings with nothing under them.
    QVector<Row> kept;
    for (int i = 0; i < t.rows.size(); ++i) {
        if (t.rows[i].kind == Kind::Heading && (i + 1 >= t.rows.size() || t.rows[i + 1].kind == Kind::Heading))
            continue;
        kept.append(t.rows[i]);
    }
    t.rows = kept;

    // Common-size denominators.
    t.common_base = QVector<double>(n, kNaN);
    if (st == Statement::Balance) {
        for (int c = 0; c < n; ++c)
            t.common_base[c] = line(t.columns[c].data, {"Total Assets"});
    } else if (st == Statement::Income) {
        for (int c = 0; c < n; ++c)
            t.common_base[c] = line(t.columns[c].data, {"Total Revenue", "Operating Revenue"});
    } else {
        const QVector<Column> inc = columns_for(fd, Statement::Income, period, t.fy_end_month);
        for (int c = 0; c < n; ++c) {
            const int m = match_date(inc, t.columns[c].end);
            if (m >= 0)
                t.common_base[c] = line(inc[m].data, {"Total Revenue", "Operating Revenue"});
        }
    }
    return t;
}

Table apply_view(const Table& in, Statement st, View view) {
    Q_UNUSED(st);
    if (view == View::AsReported)
        return in;
    Table t = in;
    const int n = static_cast<int>(t.columns.size());
    if (view == View::CommonSize) {
        for (auto& r : t.rows) {
            if (r.kind != Kind::Amount)
                continue;
            for (int c = 0; c < n; ++c) {
                const double b = t.common_base.value(c, kNaN);
                r.values[c] = (ok(r.values[c]) && ok(b) && b != 0.0) ? r.values[c] / std::fabs(b) : kNaN;
            }
        }
        return t;
    }
    // Growth vs the same period a year earlier.
    QVector<int> prior(n, -1);
    for (int c = 0; c < n; ++c) {
        const QDate target = t.columns[c].end.addYears(-1);
        int best = -1;
        qint64 gap = 26;
        for (int k = 0; k < c; ++k) {
            const qint64 g = qAbs(t.columns[k].end.daysTo(target));
            if (g < gap) {
                gap = g;
                best = k;
            }
        }
        prior[c] = best;
    }
    for (auto& r : t.rows) {
        if (r.kind == Kind::Heading || r.kind == Kind::Ratio) {
            if (r.kind == Kind::Ratio)
                r.values.fill(kNaN);
            continue;
        }
        const QVector<double> src = r.values;
        for (int c = 0; c < n; ++c) {
            const int p = prior[c];
            const double cur = src[c];
            const double prev = p >= 0 ? src[p] : kNaN;
            // Magnitude growth when both readings share a sign (a loss that shrank reads
            // as a fall); a sign change has no meaningful percentage.
            if (!ok(cur) || !ok(prev) || prev == 0.0 || (cur < 0) != (prev < 0))
                r.values[c] = kNaN;
            else
                r.values[c] = (std::fabs(cur) - std::fabs(prev)) / std::fabs(prev);
        }
    }
    return t;
}

// ── Ratios ───────────────────────────────────────────────────────────────────

RatioTable ratios(const FinancialsData& fd, Period period, Template tmpl) {
    RatioTable out;
    out.tmpl = tmpl;
    const int fy = fiscal_year_end_month(fd);
    const Period flows_mode = period == Period::Annual ? Period::Annual : Period::Ttm;
    const QVector<Column> inc = columns_for(fd, Statement::Income, flows_mode, fy);
    const QVector<Column> cf = columns_for(fd, Statement::CashFlow, flows_mode, fy);
    // Balances from every source (annual and quarterly), by date.
    QVector<Column> bal = raw_columns(fd.balance_sheet);
    for (const auto& q : raw_columns(fd.q_balance))
        if (match_date(bal, q.end) < 0)
            bal.append(q);
    std::sort(bal.begin(), bal.end(), [](const Column& a, const Column& b) { return a.end < b.end; });

    out.columns = inc;
    const int n = static_cast<int>(inc.size());
    const char* ctx = "EquityFinancials";
    auto tr = [ctx](const char* s) { return QCoreApplication::translate(ctx, s); };

    struct Snap {
        QJsonObject inc, cf, bal, bal_prev;
        bool has_prev = false;
    };
    QVector<Snap> snaps(n);
    for (int i = 0; i < n; ++i) {
        snaps[i].inc = inc[i].data;
        const int c = match_date(cf, inc[i].end);
        if (c >= 0)
            snaps[i].cf = cf[c].data;
        const int b = match_date(bal, inc[i].end);
        if (b >= 0)
            snaps[i].bal = bal[b].data;
        const int bp = match_date(bal, inc[i].end.addYears(-1), 25);
        if (bp >= 0) {
            snaps[i].bal_prev = bal[bp].data;
            snaps[i].has_prev = true;
        }
    }

    auto avg = [](const Snap& s, std::initializer_list<const char*> keys) {
        const double e = line(s.bal, keys);
        const double p = s.has_prev ? line(s.bal_prev, keys) : kNaN;
        return ok(e) && ok(p) ? (e + p) / 2.0 : e;
    };
    for (const auto& s : snaps)
        out.averaged = out.averaged || s.has_prev;

    auto div = [](double a, double b) { return (ok(a) && ok(b) && b != 0.0) ? a / b : kNaN; };
    auto pos_div = [](double a, double b) { return (ok(a) && ok(b) && b > 0.0) ? a / b : kNaN; };

    using U = RatioRow::Unit;
    auto add = [&](const char* section, const char* id, const char* label, U unit, const char* note,
                   const std::function<double(const Snap&, int)>& fn, bool higher_better = true) {
        RatioRow r;
        r.section = tr(section);
        r.id = QLatin1String(id);
        r.label = tr(label);
        r.unit = unit;
        r.note = tr(note);
        r.higher_is_better = higher_better;
        r.values.resize(n);
        bool any = false;
        for (int i = 0; i < n; ++i) {
            r.values[i] = fn(snaps[i], i);
            any = any || ok(r.values[i]);
        }
        if (any)
            out.rows.append(r);
    };

    // Line accessors.
    auto rev = [](const Snap& s) { return line(s.inc, {"Total Revenue", "Operating Revenue"}); };
    auto cogs = [](const Snap& s) { return line(s.inc, {"Cost Of Revenue", "Reconciled Cost Of Revenue"}); };
    auto gross = [&](const Snap& s) {
        const double g = line(s.inc, {"Gross Profit"});
        return ok(g) ? g : (ok(rev(s)) && ok(cogs(s)) ? rev(s) - cogs(s) : kNaN);
    };
    auto op_inc = [](const Snap& s) { return line(s.inc, {"Operating Income", "Total Operating Income As Reported"}); };
    auto ebit = [&](const Snap& s) {
        const double e = line(s.inc, {"EBIT"});
        return ok(e) ? e : op_inc(s);
    };
    auto ebitda = [](const Snap& s) { return line(s.inc, {"EBITDA", "Normalized EBITDA"}); };
    auto pretax = [](const Snap& s) { return line(s.inc, {"Pretax Income"}); };
    auto ni = [](const Snap& s) { return line(s.inc, {"Net Income Common Stockholders", "Net Income"}); };
    auto interest = [](const Snap& s) {
        const double v = line(s.inc, {"Interest Expense", "Interest Expense Non Operating"});
        return ok(v) ? std::fabs(v) : kNaN;
    };
    auto tax_rate = [&](const Snap& s) {
        double t = line(s.inc, {"Tax Rate For Calcs"});
        if (!ok(t))
            t = pos_div(line(s.inc, {"Tax Provision"}), pretax(s));
        return ok(t) ? std::clamp(t, 0.0, 0.5) : kNaN;
    };
    auto shares_avg = [](const Snap& s) { return line(s.inc, {"Diluted Average Shares", "Basic Average Shares"}); };
    auto shares_out = [](const Snap& s) { return line(s.bal, {"Ordinary Shares Number", "Share Issued"}); };
    auto ocf = [](const Snap& s) { return line(s.cf, {"Operating Cash Flow", "Cash Flow From Continuing Operating Activities"}); };
    auto capex = [](const Snap& s) { return line(s.cf, {"Capital Expenditure", "Purchase Of PPE"}); };
    auto fcf = [&](const Snap& s) {
        const double f = line(s.cf, {"Free Cash Flow"});
        return ok(f) ? f : (ok(ocf(s)) && ok(capex(s)) ? ocf(s) + capex(s) : kNaN);
    };
    auto da = [](const Snap& s) {
        const double v = line(s.cf, {"Depreciation And Amortization", "Depreciation Amortization Depletion"});
        return ok(v) ? v : line(s.inc, {"Reconciled Depreciation"});
    };
    auto equity = [](const Snap& s) { return line(s.bal, {"Stockholders Equity", "Common Stock Equity"}); };
    auto debt = [](const Snap& s) { return line(s.bal, {"Total Debt"}); };
    auto cash_sti = [](const Snap& s) {
        return line(s.bal, {"Cash Cash Equivalents And Short Term Investments", "Cash And Cash Equivalents"});
    };
    auto invested = [&](const Snap& s, bool prev) {
        const QJsonObject& b = prev ? s.bal_prev : s.bal;
        const double ic = line(b, {"Invested Capital"});
        if (ok(ic))
            return ic;
        const double e = line(b, {"Stockholders Equity"}), d = line(b, {"Total Debt"});
        const double c = line(b, {"Cash And Cash Equivalents"});
        return ok(e) && ok(d) ? e + d - (ok(c) ? c : 0.0) : kNaN;
    };
    const bool financial = tmpl != Template::Industrial;

    // PROFITABILITY
    if (!financial)
        add(QT_TRANSLATE_NOOP("EquityFinancials", "PROFITABILITY"), "gross_margin",
            QT_TRANSLATE_NOOP("EquityFinancials", "Gross margin"), U::Percent,
            QT_TRANSLATE_NOOP("EquityFinancials", "Gross profit ÷ revenue"),
            [&](const Snap& s, int) { return pos_div(gross(s), rev(s)); });
    add(QT_TRANSLATE_NOOP("EquityFinancials", "PROFITABILITY"), "ebitda_margin",
        QT_TRANSLATE_NOOP("EquityFinancials", "EBITDA margin"), U::Percent,
        QT_TRANSLATE_NOOP("EquityFinancials", "EBITDA ÷ revenue"),
        [&](const Snap& s, int) { return financial ? kNaN : pos_div(ebitda(s), rev(s)); });
    add(QT_TRANSLATE_NOOP("EquityFinancials", "PROFITABILITY"), "op_margin",
        QT_TRANSLATE_NOOP("EquityFinancials", "Operating margin"), U::Percent,
        QT_TRANSLATE_NOOP("EquityFinancials", "Operating income ÷ revenue"),
        [&](const Snap& s, int) { return pos_div(financial ? ebit(s) : op_inc(s), rev(s)); });
    add(QT_TRANSLATE_NOOP("EquityFinancials", "PROFITABILITY"), "pretax_margin",
        QT_TRANSLATE_NOOP("EquityFinancials", "Pretax margin"), U::Percent,
        QT_TRANSLATE_NOOP("EquityFinancials", "Pretax income ÷ revenue"),
        [&](const Snap& s, int) { return pos_div(pretax(s), rev(s)); });
    add(QT_TRANSLATE_NOOP("EquityFinancials", "PROFITABILITY"), "net_margin",
        QT_TRANSLATE_NOOP("EquityFinancials", "Net margin"), U::Percent,
        QT_TRANSLATE_NOOP("EquityFinancials", "Net income to common ÷ revenue"),
        [&](const Snap& s, int) { return pos_div(ni(s), rev(s)); });
    if (!financial)
        add(QT_TRANSLATE_NOOP("EquityFinancials", "PROFITABILITY"), "fcf_margin",
            QT_TRANSLATE_NOOP("EquityFinancials", "FCF margin"), U::Percent,
            QT_TRANSLATE_NOOP("EquityFinancials", "Free cash flow ÷ revenue"),
            [&](const Snap& s, int) { return pos_div(fcf(s), rev(s)); });

    // RETURNS (average balances)
    add(QT_TRANSLATE_NOOP("EquityFinancials", "RETURNS"), "roe", QT_TRANSLATE_NOOP("EquityFinancials", "Return on equity"),
        U::Percent, QT_TRANSLATE_NOOP("EquityFinancials", "Net income ÷ average shareholders' equity"),
        [&](const Snap& s, int) { return pos_div(ni(s), avg(s, {"Stockholders Equity", "Common Stock Equity"})); });
    add(QT_TRANSLATE_NOOP("EquityFinancials", "RETURNS"), "roa", QT_TRANSLATE_NOOP("EquityFinancials", "Return on assets"),
        U::Percent, QT_TRANSLATE_NOOP("EquityFinancials", "Net income ÷ average total assets"),
        [&](const Snap& s, int) { return pos_div(ni(s), avg(s, {"Total Assets"})); });
    if (!financial) {
        add(QT_TRANSLATE_NOOP("EquityFinancials", "RETURNS"), "roic",
            QT_TRANSLATE_NOOP("EquityFinancials", "Return on invested capital"), U::Percent,
            QT_TRANSLATE_NOOP("EquityFinancials", "EBIT × (1 − tax rate) ÷ average invested capital (equity + debt − cash)"),
            [&](const Snap& s, int) {
                const double e = invested(s, false), p = s.has_prev ? invested(s, true) : kNaN;
                const double ic = ok(e) && ok(p) ? (e + p) / 2.0 : e;
                const double t = tax_rate(s);
                return ok(t) ? pos_div(ebit(s) * (1.0 - t), ic) : kNaN;
            });
        add(QT_TRANSLATE_NOOP("EquityFinancials", "RETURNS"), "roce",
            QT_TRANSLATE_NOOP("EquityFinancials", "Return on capital employed"), U::Percent,
            QT_TRANSLATE_NOOP("EquityFinancials", "EBIT ÷ average (total assets − current liabilities)"),
            [&](const Snap& s, int) {
                const double ta = avg(s, {"Total Assets"}), cl = avg(s, {"Current Liabilities"});
                return ok(cl) ? pos_div(ebit(s), ta - cl) : kNaN;
            });
    } else {
        add(QT_TRANSLATE_NOOP("EquityFinancials", "RETURNS"), "rote",
            QT_TRANSLATE_NOOP("EquityFinancials", "Return on tangible equity"), U::Percent,
            QT_TRANSLATE_NOOP("EquityFinancials", "Net income ÷ average tangible book value"),
            [&](const Snap& s, int) { return pos_div(ni(s), avg(s, {"Tangible Book Value"})); });
    }

    // DUPONT
    add(QT_TRANSLATE_NOOP("EquityFinancials", "DUPONT"), "dp_tax_burden",
        QT_TRANSLATE_NOOP("EquityFinancials", "Tax burden (NI ÷ EBT)"), U::Multiple,
        QT_TRANSLATE_NOOP("EquityFinancials", "5-step DuPont: net income ÷ pretax income"),
        [&](const Snap& s, int) { return div(ni(s), pretax(s)); });
    if (!financial)
        add(QT_TRANSLATE_NOOP("EquityFinancials", "DUPONT"), "dp_interest_burden",
            QT_TRANSLATE_NOOP("EquityFinancials", "Interest burden (EBT ÷ EBIT)"), U::Multiple,
            QT_TRANSLATE_NOOP("EquityFinancials", "5-step DuPont: pretax income ÷ EBIT"),
            [&](const Snap& s, int) { return div(pretax(s), ebit(s)); });
    if (!financial)
        add(QT_TRANSLATE_NOOP("EquityFinancials", "DUPONT"), "dp_ebit_margin",
            QT_TRANSLATE_NOOP("EquityFinancials", "EBIT margin"), U::Percent,
            QT_TRANSLATE_NOOP("EquityFinancials", "5-step DuPont: EBIT ÷ revenue"),
            [&](const Snap& s, int) { return pos_div(ebit(s), rev(s)); });
    add(QT_TRANSLATE_NOOP("EquityFinancials", "DUPONT"), "dp_net_margin",
        QT_TRANSLATE_NOOP("EquityFinancials", "Net margin"), U::Percent,
        QT_TRANSLATE_NOOP("EquityFinancials", "3-step DuPont: net income ÷ revenue"),
        [&](const Snap& s, int) { return pos_div(ni(s), rev(s)); });
    add(QT_TRANSLATE_NOOP("EquityFinancials", "DUPONT"), "dp_asset_turnover",
        QT_TRANSLATE_NOOP("EquityFinancials", "Asset turnover"), U::Multiple,
        QT_TRANSLATE_NOOP("EquityFinancials", "Revenue ÷ average total assets"),
        [&](const Snap& s, int) { return pos_div(rev(s), avg(s, {"Total Assets"})); });
    add(QT_TRANSLATE_NOOP("EquityFinancials", "DUPONT"), "dp_equity_multiplier",
        QT_TRANSLATE_NOOP("EquityFinancials", "Equity multiplier"), U::Multiple,
        QT_TRANSLATE_NOOP("EquityFinancials", "Average total assets ÷ average equity"),
        [&](const Snap& s, int) { return pos_div(avg(s, {"Total Assets"}), avg(s, {"Stockholders Equity", "Common Stock Equity"})); },
        false);
    add(QT_TRANSLATE_NOOP("EquityFinancials", "DUPONT"), "dp_roe",
        QT_TRANSLATE_NOOP("EquityFinancials", "ROE (DuPont product)"), U::Percent,
        QT_TRANSLATE_NOOP("EquityFinancials", "Net margin × asset turnover × equity multiplier"),
        [&](const Snap& s, int) {
            const double nm = pos_div(ni(s), rev(s)), at = pos_div(rev(s), avg(s, {"Total Assets"}));
            const double em = pos_div(avg(s, {"Total Assets"}), avg(s, {"Stockholders Equity", "Common Stock Equity"}));
            return ok(nm) && ok(at) && ok(em) ? nm * at * em : kNaN;
        });

    // LEVERAGE
    add(QT_TRANSLATE_NOOP("EquityFinancials", "LEVERAGE"), "debt_equity",
        QT_TRANSLATE_NOOP("EquityFinancials", "Debt ÷ equity"), U::Multiple,
        QT_TRANSLATE_NOOP("EquityFinancials", "Total debt ÷ shareholders' equity (period end)"),
        [&](const Snap& s, int) { return pos_div(debt(s), equity(s)); }, false);
    if (!financial) {
        add(QT_TRANSLATE_NOOP("EquityFinancials", "LEVERAGE"), "net_debt_ebitda",
            QT_TRANSLATE_NOOP("EquityFinancials", "Net debt ÷ EBITDA"), U::Multiple,
            QT_TRANSLATE_NOOP("EquityFinancials", "(Total debt − cash & short-term investments) ÷ EBITDA; negative = net cash"),
            [&](const Snap& s, int) {
                const double nd = ok(debt(s)) && ok(cash_sti(s)) ? debt(s) - cash_sti(s) : line(s.bal, {"Net Debt"});
                return pos_div(nd, ebitda(s));
            },
            false);
        add(QT_TRANSLATE_NOOP("EquityFinancials", "LEVERAGE"), "interest_cover",
            QT_TRANSLATE_NOOP("EquityFinancials", "Interest coverage"), U::Multiple,
            QT_TRANSLATE_NOOP("EquityFinancials", "EBIT ÷ interest expense"),
            [&](const Snap& s, int) { return pos_div(ebit(s), interest(s)); });
    }
    add(QT_TRANSLATE_NOOP("EquityFinancials", "LEVERAGE"), "debt_assets",
        QT_TRANSLATE_NOOP("EquityFinancials", "Debt ÷ assets"), U::Percent,
        QT_TRANSLATE_NOOP("EquityFinancials", "Total debt ÷ total assets"),
        [&](const Snap& s, int) { return pos_div(debt(s), line(s.bal, {"Total Assets"})); }, false);
    add(QT_TRANSLATE_NOOP("EquityFinancials", "LEVERAGE"), "equity_assets",
        QT_TRANSLATE_NOOP("EquityFinancials", "Equity ÷ assets"), U::Percent,
        QT_TRANSLATE_NOOP("EquityFinancials", "Shareholders' equity ÷ total assets (capital strength)"),
        [&](const Snap& s, int) { return pos_div(equity(s), line(s.bal, {"Total Assets"})); });

    // LIQUIDITY (industrial balance sheets only — banks do not classify current items)
    if (!financial) {
        add(QT_TRANSLATE_NOOP("EquityFinancials", "LIQUIDITY"), "current_ratio",
            QT_TRANSLATE_NOOP("EquityFinancials", "Current ratio"), U::Multiple,
            QT_TRANSLATE_NOOP("EquityFinancials", "Current assets ÷ current liabilities"),
            [&](const Snap& s, int) { return pos_div(line(s.bal, {"Current Assets"}), line(s.bal, {"Current Liabilities"})); });
        add(QT_TRANSLATE_NOOP("EquityFinancials", "LIQUIDITY"), "quick_ratio",
            QT_TRANSLATE_NOOP("EquityFinancials", "Quick ratio"), U::Multiple,
            QT_TRANSLATE_NOOP("EquityFinancials", "(Current assets − inventory) ÷ current liabilities"),
            [&](const Snap& s, int) {
                const double ca = line(s.bal, {"Current Assets"}), inv = line(s.bal, {"Inventory"});
                return ok(ca) ? pos_div(ca - (ok(inv) ? inv : 0.0), line(s.bal, {"Current Liabilities"})) : kNaN;
            });
        add(QT_TRANSLATE_NOOP("EquityFinancials", "LIQUIDITY"), "cash_ratio",
            QT_TRANSLATE_NOOP("EquityFinancials", "Cash ratio"), U::Multiple,
            QT_TRANSLATE_NOOP("EquityFinancials", "Cash & short-term investments ÷ current liabilities"),
            [&](const Snap& s, int) { return pos_div(cash_sti(s), line(s.bal, {"Current Liabilities"})); });

        // EFFICIENCY
        add(QT_TRANSLATE_NOOP("EquityFinancials", "EFFICIENCY"), "dso",
            QT_TRANSLATE_NOOP("EquityFinancials", "Days sales outstanding"), U::Days,
            QT_TRANSLATE_NOOP("EquityFinancials", "Average receivables ÷ revenue × 365"),
            [&](const Snap& s, int) {
                const double r = pos_div(avg(s, {"Accounts Receivable", "Receivables"}), rev(s));
                return ok(r) ? r * 365.0 : kNaN;
            },
            false);
        add(QT_TRANSLATE_NOOP("EquityFinancials", "EFFICIENCY"), "dio",
            QT_TRANSLATE_NOOP("EquityFinancials", "Days inventory outstanding"), U::Days,
            QT_TRANSLATE_NOOP("EquityFinancials", "Average inventory ÷ cost of revenue × 365"),
            [&](const Snap& s, int) {
                const double r = pos_div(avg(s, {"Inventory"}), cogs(s));
                return ok(r) ? r * 365.0 : kNaN;
            },
            false);
        add(QT_TRANSLATE_NOOP("EquityFinancials", "EFFICIENCY"), "dpo",
            QT_TRANSLATE_NOOP("EquityFinancials", "Days payables outstanding"), U::Days,
            QT_TRANSLATE_NOOP("EquityFinancials", "Average accounts payable ÷ cost of revenue × 365"),
            [&](const Snap& s, int) {
                const double r = pos_div(avg(s, {"Accounts Payable", "Payables"}), cogs(s));
                return ok(r) ? r * 365.0 : kNaN;
            });
        add(QT_TRANSLATE_NOOP("EquityFinancials", "EFFICIENCY"), "ccc",
            QT_TRANSLATE_NOOP("EquityFinancials", "Cash conversion cycle"), U::Days,
            QT_TRANSLATE_NOOP("EquityFinancials", "DSO + DIO − DPO (days of working capital tied up)"),
            [&](const Snap& s, int) {
                const double r = rev(s), c = cogs(s);
                const double dso = pos_div(avg(s, {"Accounts Receivable", "Receivables"}), r) * 365.0;
                double dio = pos_div(avg(s, {"Inventory"}), c) * 365.0;
                const double dpo = pos_div(avg(s, {"Accounts Payable", "Payables"}), c) * 365.0;
                if (!ok(dio) && ok(c) && !ok(line(s.bal, {"Inventory"})))
                    dio = 0.0; // no inventory line: an asset-light business, not a gap
                return ok(dso) && ok(dio) && ok(dpo) ? dso + dio - dpo : kNaN;
            },
            false);
        add(QT_TRANSLATE_NOOP("EquityFinancials", "EFFICIENCY"), "inventory_turnover",
            QT_TRANSLATE_NOOP("EquityFinancials", "Inventory turnover"), U::Multiple,
            QT_TRANSLATE_NOOP("EquityFinancials", "Cost of revenue ÷ average inventory"),
            [&](const Snap& s, int) { return pos_div(cogs(s), avg(s, {"Inventory"})); });
    } else if (tmpl == Template::Bank) {
        add(QT_TRANSLATE_NOOP("EquityFinancials", "EFFICIENCY"), "nii_share",
            QT_TRANSLATE_NOOP("EquityFinancials", "Net interest income share"), U::Percent,
            QT_TRANSLATE_NOOP("EquityFinancials", "Net interest income ÷ total revenue"),
            [&](const Snap& s, int) { return pos_div(line(s.inc, {"Net Interest Income"}), rev(s)); });
        add(QT_TRANSLATE_NOOP("EquityFinancials", "EFFICIENCY"), "nii_assets",
            QT_TRANSLATE_NOOP("EquityFinancials", "Net interest income ÷ assets"), U::Percent,
            QT_TRANSLATE_NOOP("EquityFinancials", "Net interest income ÷ average total assets (a margin proxy)"),
            [&](const Snap& s, int) { return pos_div(line(s.inc, {"Net Interest Income"}), avg(s, {"Total Assets"})); });
        add(QT_TRANSLATE_NOOP("EquityFinancials", "EFFICIENCY"), "efficiency_ratio",
            QT_TRANSLATE_NOOP("EquityFinancials", "Efficiency ratio"), U::Percent,
            QT_TRANSLATE_NOOP("EquityFinancials", "Non-interest expense ÷ total revenue (lower is better)"),
            [&](const Snap& s, int) {
                double x = line(s.inc, {"Non Interest Expense"});
                if (!ok(x)) {
                    const double sga = line(s.inc, {"Selling General And Administration"});
                    const double other = line(s.inc, {"Other Non Interest Expense"});
                    const double occ = line(s.inc, {"Occupancy And Equipment"});
                    const double prof = line(s.inc, {"Professional Expense And Contract Services Expense"});
                    if (ok(sga))
                        x = sga + (ok(other) ? other : 0.0) + (ok(occ) ? occ : 0.0) + (ok(prof) ? prof : 0.0);
                }
                return pos_div(x, rev(s));
            },
            false);
    } else {
        add(QT_TRANSLATE_NOOP("EquityFinancials", "EFFICIENCY"), "benefit_ratio",
            QT_TRANSLATE_NOOP("EquityFinancials", "Benefits & claims ÷ revenue"), U::Percent,
            QT_TRANSLATE_NOOP("EquityFinancials", "Net policyholder benefits and claims ÷ total revenue (a loss-ratio proxy)"),
            [&](const Snap& s, int) {
                double b = line(s.inc, {"Net Policyholder Benefits And Claims"});
                const double lae = line(s.inc, {"Loss Adjustment Expense"});
                if (ok(b) && ok(lae))
                    b += lae;
                return pos_div(b, rev(s));
            },
            false);
        add(QT_TRANSLATE_NOOP("EquityFinancials", "EFFICIENCY"), "expense_ratio",
            QT_TRANSLATE_NOOP("EquityFinancials", "Total expenses ÷ revenue"), U::Percent,
            QT_TRANSLATE_NOOP("EquityFinancials", "Total benefits and expenses ÷ total revenue (a combined-ratio proxy)"),
            [&](const Snap& s, int) { return pos_div(line(s.inc, {"Total Expenses"}), rev(s)); }, false);
    }

    // PER SHARE
    add(QT_TRANSLATE_NOOP("EquityFinancials", "PER SHARE"), "eps",
        QT_TRANSLATE_NOOP("EquityFinancials", "EPS (diluted)"), U::PerShare,
        QT_TRANSLATE_NOOP("EquityFinancials", "Diluted earnings per share as reported"),
        [&](const Snap& s, int) { return line(s.inc, {"Diluted EPS", "Basic EPS"}); });
    add(QT_TRANSLATE_NOOP("EquityFinancials", "PER SHARE"), "rev_ps",
        QT_TRANSLATE_NOOP("EquityFinancials", "Revenue per share"), U::PerShare,
        QT_TRANSLATE_NOOP("EquityFinancials", "Revenue ÷ diluted average shares"),
        [&](const Snap& s, int) { return pos_div(rev(s), shares_avg(s)); });
    add(QT_TRANSLATE_NOOP("EquityFinancials", "PER SHARE"), "bvps",
        QT_TRANSLATE_NOOP("EquityFinancials", "Book value per share"), U::PerShare,
        QT_TRANSLATE_NOOP("EquityFinancials", "Shareholders' equity ÷ shares outstanding"),
        [&](const Snap& s, int) { return pos_div(equity(s), shares_out(s)); });
    if (!financial)
        add(QT_TRANSLATE_NOOP("EquityFinancials", "PER SHARE"), "fcf_ps",
            QT_TRANSLATE_NOOP("EquityFinancials", "Free cash flow per share"), U::PerShare,
            QT_TRANSLATE_NOOP("EquityFinancials", "Free cash flow ÷ diluted average shares"),
            [&](const Snap& s, int) { return pos_div(fcf(s), shares_avg(s)); });
    add(QT_TRANSLATE_NOOP("EquityFinancials", "PER SHARE"), "dps",
        QT_TRANSLATE_NOOP("EquityFinancials", "Dividends per share (cash paid)"), U::PerShare,
        QT_TRANSLATE_NOOP("EquityFinancials", "Cash dividends paid ÷ diluted average shares"),
        [&](const Snap& s, int) {
            const double d = line(s.cf, {"Cash Dividends Paid", "Common Stock Dividend Paid"});
            return ok(d) ? pos_div(std::fabs(d), shares_avg(s)) : kNaN;
        });

    // SHARES
    add(QT_TRANSLATE_NOOP("EquityFinancials", "SHARES"), "shares_diluted",
        QT_TRANSLATE_NOOP("EquityFinancials", "Diluted average shares"), U::Shares,
        QT_TRANSLATE_NOOP("EquityFinancials", "Weighted average diluted share count"),
        [&](const Snap& s, int) { return shares_avg(s); }, false);
    add(QT_TRANSLATE_NOOP("EquityFinancials", "SHARES"), "shares_out",
        QT_TRANSLATE_NOOP("EquityFinancials", "Shares outstanding"), U::Shares,
        QT_TRANSLATE_NOOP("EquityFinancials", "Ordinary shares outstanding at period end"),
        [&](const Snap& s, int) { return shares_out(s); }, false);
    add(QT_TRANSLATE_NOOP("EquityFinancials", "SHARES"), "share_change",
        QT_TRANSLATE_NOOP("EquityFinancials", "Share count change (YoY)"), U::Percent,
        QT_TRANSLATE_NOOP("EquityFinancials", "Change in diluted average shares vs a year earlier; negative = net buybacks"),
        [&](const Snap& s, int i) {
            const double cur = shares_avg(s);
            for (int k = 0; k < i; ++k)
                if (qAbs(inc[k].end.daysTo(inc[i].end.addYears(-1))) <= 25)
                    return pos_div(cur - shares_avg(snaps[k]), shares_avg(snaps[k]));
            return kNaN;
        },
        false);

    // CASH CONVERSION
    if (!financial) {
        add(QT_TRANSLATE_NOOP("EquityFinancials", "CASH CONVERSION"), "ocf_ni",
            QT_TRANSLATE_NOOP("EquityFinancials", "Operating cash flow ÷ net income"), U::Multiple,
            QT_TRANSLATE_NOOP("EquityFinancials", "Earnings quality: above 1 = profits backed by cash"),
            [&](const Snap& s, int) { return pos_div(ocf(s), ni(s)); });
        add(QT_TRANSLATE_NOOP("EquityFinancials", "CASH CONVERSION"), "fcf_ni",
            QT_TRANSLATE_NOOP("EquityFinancials", "FCF conversion"), U::Percent,
            QT_TRANSLATE_NOOP("EquityFinancials", "Free cash flow ÷ net income"),
            [&](const Snap& s, int) { return pos_div(fcf(s), ni(s)); });
        add(QT_TRANSLATE_NOOP("EquityFinancials", "CASH CONVERSION"), "capex_rev",
            QT_TRANSLATE_NOOP("EquityFinancials", "Capex ÷ revenue"), U::Percent,
            QT_TRANSLATE_NOOP("EquityFinancials", "Capital expenditure ÷ revenue"),
            [&](const Snap& s, int) { return ok(capex(s)) ? pos_div(std::fabs(capex(s)), rev(s)) : kNaN; }, false);
        add(QT_TRANSLATE_NOOP("EquityFinancials", "CASH CONVERSION"), "capex_da",
            QT_TRANSLATE_NOOP("EquityFinancials", "Capex ÷ depreciation"), U::Multiple,
            QT_TRANSLATE_NOOP("EquityFinancials", "Above 1 = investing beyond maintenance"),
            [&](const Snap& s, int) { return ok(capex(s)) ? pos_div(std::fabs(capex(s)), da(s)) : kNaN; });
        add(QT_TRANSLATE_NOOP("EquityFinancials", "CASH CONVERSION"), "payout_fcf",
            QT_TRANSLATE_NOOP("EquityFinancials", "Dividends + buybacks ÷ FCF"), U::Percent,
            QT_TRANSLATE_NOOP("EquityFinancials", "Shareholder returns as a share of free cash flow"),
            [&](const Snap& s, int) {
                const double d = line(s.cf, {"Cash Dividends Paid", "Common Stock Dividend Paid"});
                const double b = line(s.cf, {"Repurchase Of Capital Stock", "Common Stock Payments"});
                const double total = (ok(d) ? std::fabs(d) : 0.0) + (ok(b) ? std::fabs(b) : 0.0);
                return (ok(d) || ok(b)) ? pos_div(total, fcf(s)) : kNaN;
            },
            false);
    }
    return out;
}

} // namespace fincept::screens::fin_model
