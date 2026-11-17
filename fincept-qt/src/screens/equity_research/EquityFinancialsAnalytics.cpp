// src/screens/equity_research/EquityFinancialsAnalytics.cpp
//
// Definitions, sign conventions and the list of ratios on average balances: see the header.
#include "screens/equity_research/EquityFinancialsAnalytics.h"

#include <QCoreApplication>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>

namespace fincept::screens::fin_an {

namespace {

using services::equity::FinancialsData;
using fin_model::Column;
using fin_model::Statement;

struct FaText {
    Q_DECLARE_TR_FUNCTIONS(EquityFinancials)
};

bool fa_ok(double v) {
    return std::isfinite(v);
}

double fa_get(const QJsonObject& o, std::initializer_list<const char*> keys) {
    return fin_model::line(o, keys);
}

/// Index of the column ending within `tol` days of `d` (nearest), -1 when none.
int fa_match(const QVector<Column>& cols, const QDate& d, int tol) {
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

QString fa_missing(const QString& what) {
    return FaText::tr("n/a — %1 not reported").arg(what);
}
QString fa_not_positive(const QString& what) {
    return FaText::tr("n/a — %1 is zero or negative").arg(what);
}
QString fa_end_balance_note() {
    return FaText::tr("on the period-end balance — the previous balance sheet is not on file");
}

/// Growth of an earnings-type figure: like yoy_change(), but two negative readings (a loss in
/// both periods) have no meaningful growth rate — "NM", with which way the loss moved.
double fa_growth(double cur, double prev, QString& note) {
    if (fa_ok(cur) && fa_ok(prev) && cur < 0.0 && prev < 0.0) {
        note = std::fabs(cur) < std::fabs(prev) ? FaText::tr("NM — negative in both periods; the loss narrowed")
                                                : FaText::tr("NM — negative in both periods; the loss widened");
        return kNaN;
    }
    return yoy_change(cur, prev, &note);
}

// ── Balance-sheet lines (any balance column) ───────────────────────────────────────────────

double fa_bal(const QJsonObject& b, const QString& id) {
    if (b.isEmpty())
        return kNaN;
    if (id == QLatin1String("total_assets"))
        return fa_get(b, {"Total Assets"});
    if (id == QLatin1String("current_assets"))
        return fa_get(b, {"Current Assets"});
    if (id == QLatin1String("current_liabilities"))
        return fa_get(b, {"Current Liabilities"});
    if (id == QLatin1String("total_liabilities"))
        return fa_get(b, {"Total Liabilities Net Minority Interest"});
    if (id == QLatin1String("equity"))
        return fa_get(b, {"Stockholders Equity", "Common Stock Equity"});
    if (id == QLatin1String("total_equity")) {
        const double t = fa_get(b, {"Total Equity Gross Minority Interest"});
        if (fa_ok(t))
            return t;
        const double e = fa_bal(b, QStringLiteral("equity"));
        const double m = fa_get(b, {"Minority Interest"});
        return fa_ok(e) ? e + (fa_ok(m) ? m : 0.0) : kNaN;
    }
    if (id == QLatin1String("cash"))
        return fa_get(b, {"Cash And Cash Equivalents", "Cash Cash Equivalents And Federal Funds Sold", "Cash Financial"});
    if (id == QLatin1String("cash_sti")) {
        const double c = fa_get(b, {"Cash Cash Equivalents And Short Term Investments"});
        if (fa_ok(c))
            return c;
        const double cash = fa_bal(b, QStringLiteral("cash"));
        const double sti = fa_get(b, {"Other Short Term Investments"});
        return fa_ok(cash) ? cash + (fa_ok(sti) ? sti : 0.0) : kNaN;
    }
    if (id == QLatin1String("receivables"))
        return fa_get(b, {"Accounts Receivable", "Receivables"});
    if (id == QLatin1String("receivables_total"))
        return fa_get(b, {"Receivables", "Accounts Receivable"});
    if (id == QLatin1String("inventory"))
        return fa_get(b, {"Inventory"});
    if (id == QLatin1String("ppe"))
        return fa_get(b, {"Net PPE"});
    if (id == QLatin1String("intangibles")) {
        const double g = fa_get(b, {"Goodwill And Other Intangible Assets"});
        if (fa_ok(g))
            return g;
        const double gw = fa_get(b, {"Goodwill"}), oi = fa_get(b, {"Other Intangible Assets"});
        if (!fa_ok(gw) && !fa_ok(oi))
            return kNaN;
        return (fa_ok(gw) ? gw : 0.0) + (fa_ok(oi) ? oi : 0.0);
    }
    if (id == QLatin1String("payables"))
        return fa_get(b, {"Accounts Payable", "Payables"});
    if (id == QLatin1String("payables_accrued"))
        return fa_get(b, {"Payables And Accrued Expenses", "Payables", "Accounts Payable"});
    if (id == QLatin1String("short_debt"))
        return fa_get(b, {"Current Debt And Capital Lease Obligation", "Current Debt", "Other Current Borrowings"});
    if (id == QLatin1String("long_debt"))
        return fa_get(b, {"Long Term Debt And Capital Lease Obligation", "Long Term Debt"});
    if (id == QLatin1String("total_debt")) {
        const double t = fa_get(b, {"Total Debt"});
        if (fa_ok(t))
            return t;
        const double s = fa_bal(b, QStringLiteral("short_debt")), l = fa_bal(b, QStringLiteral("long_debt"));
        if (!fa_ok(s) && !fa_ok(l))
            return kNaN;
        return (fa_ok(s) ? s : 0.0) + (fa_ok(l) ? l : 0.0);
    }
    if (id == QLatin1String("retained_earnings"))
        return fa_get(b, {"Retained Earnings"});
    if (id == QLatin1String("working_capital")) {
        const double w = fa_get(b, {"Working Capital"});
        if (fa_ok(w))
            return w;
        const double ca = fa_bal(b, QStringLiteral("current_assets")), cl = fa_bal(b, QStringLiteral("current_liabilities"));
        return fa_ok(ca) && fa_ok(cl) ? ca - cl : kNaN;
    }
    if (id == QLatin1String("tangible_book"))
        return fa_get(b, {"Tangible Book Value"});
    if (id == QLatin1String("shares_out"))
        return fa_get(b, {"Ordinary Shares Number", "Share Issued"});
    if (id == QLatin1String("lt_investments"))
        return fa_get(b, {"Investments And Advances", "Long Term Equity Investment"});
    if (id == QLatin1String("sti"))
        return fa_get(b, {"Other Short Term Investments"});
    if (id == QLatin1String("deposits"))
        return fa_get(b, {"Total Deposits", "Deposits"});
    if (id == QLatin1String("loans"))
        return fa_get(b, {"Net Loan", "Gross Loan", "Loans Receivable"});
    if (id == QLatin1String("securities")) {
        const double ia = fa_get(b, {"Investments And Advances"});
        if (fa_ok(ia))
            return ia;
        double sum = 0.0;
        bool any = false;
        for (const char* k : {"Trading Securities", "Available For Sale Securities", "Held To Maturity Securities"}) {
            const double v = fa_get(b, {k});
            if (fa_ok(v)) {
                sum += v;
                any = true;
            }
        }
        return any ? sum : kNaN;
    }
    if (id == QLatin1String("invested_capital")) {
        const double d = fa_bal(b, QStringLiteral("total_debt")), e = fa_bal(b, QStringLiteral("equity"));
        const double c = fa_bal(b, QStringLiteral("cash_sti"));
        if (!fa_ok(e))
            return kNaN;
        return e + (fa_ok(d) ? d : 0.0) - (fa_ok(c) ? c : 0.0);
    }
    return kNaN;
}

const char* const kFaBalanceIds[] = {
    "total_assets", "current_assets", "current_liabilities", "total_liabilities", "equity",       "total_equity",
    "cash",         "cash_sti",       "receivables",         "receivables_total", "inventory",    "ppe",
    "intangibles",  "payables",       "payables_accrued",    "short_debt",        "long_debt",    "total_debt",
    "retained_earnings", "working_capital", "tangible_book", "shares_out",        "lt_investments", "sti",
    "deposits",     "loans",          "securities",          "invested_capital",
};

// ── Income / cash-flow lines ─────────────────────────────────────────────────────────────────

double fa_flow(const QJsonObject& inc, const QJsonObject& cf, const QString& id) {
    // Income statement
    if (id == QLatin1String("revenue"))
        return fa_get(inc, {"Total Revenue", "Operating Revenue"});
    if (id == QLatin1String("cogs"))
        return fa_get(inc, {"Cost Of Revenue", "Reconciled Cost Of Revenue"});
    if (id == QLatin1String("gross_profit")) {
        const double g = fa_get(inc, {"Gross Profit"});
        if (fa_ok(g))
            return g;
        const double r = fa_flow(inc, cf, QStringLiteral("revenue")), c = fa_flow(inc, cf, QStringLiteral("cogs"));
        return fa_ok(r) && fa_ok(c) ? r - c : kNaN;
    }
    if (id == QLatin1String("rnd"))
        return fa_get(inc, {"Research And Development"});
    if (id == QLatin1String("sga"))
        return fa_get(inc, {"Selling General And Administration"});
    if (id == QLatin1String("operating_income"))
        return fa_get(inc, {"Operating Income", "Total Operating Income As Reported"});
    if (id == QLatin1String("ebit")) {
        const double e = fa_get(inc, {"EBIT"});
        return fa_ok(e) ? e : fa_flow(inc, cf, QStringLiteral("operating_income"));
    }
    if (id == QLatin1String("da")) {
        const double d = fa_get(cf, {"Depreciation And Amortization", "Depreciation Amortization Depletion", "Depreciation"});
        return fa_ok(d) ? d : fa_get(inc, {"Reconciled Depreciation"});
    }
    if (id == QLatin1String("ebitda")) {
        const double e = fa_get(inc, {"EBITDA", "Normalized EBITDA"});
        if (fa_ok(e))
            return e;
        const double ebit = fa_flow(inc, cf, QStringLiteral("ebit")), da = fa_flow(inc, cf, QStringLiteral("da"));
        return fa_ok(ebit) && fa_ok(da) ? ebit + da : kNaN;
    }
    if (id == QLatin1String("interest_expense")) {
        const double v = fa_get(inc, {"Interest Expense", "Interest Expense Non Operating"});
        return fa_ok(v) ? std::fabs(v) : kNaN;
    }
    if (id == QLatin1String("interest_income"))
        return fa_get(inc, {"Interest Income", "Interest Income Non Operating"});
    if (id == QLatin1String("pretax"))
        return fa_get(inc, {"Pretax Income"});
    if (id == QLatin1String("tax"))
        return fa_get(inc, {"Tax Provision"});
    if (id == QLatin1String("tax_rate_calc"))
        return fa_get(inc, {"Tax Rate For Calcs"});
    if (id == QLatin1String("net_income"))
        return fa_get(inc, {"Net Income Common Stockholders", "Net Income"});
    if (id == QLatin1String("net_income_total"))
        return fa_get(inc, {"Net Income", "Net Income Including Noncontrolling Interests"});
    if (id == QLatin1String("ni_cont")) {
        const double v = fa_get(inc, {"Net Income Continuous Operations",
                                      "Net Income From Continuing Operation Net Minority Interest"});
        return fa_ok(v) ? v : fa_flow(inc, cf, QStringLiteral("net_income"));
    }
    if (id == QLatin1String("eps_diluted"))
        return fa_get(inc, {"Diluted EPS", "Basic EPS"});
    if (id == QLatin1String("shares_diluted"))
        return fa_get(inc, {"Diluted Average Shares", "Basic Average Shares"});
    if (id == QLatin1String("nii"))
        return fa_get(inc, {"Net Interest Income"});
    if (id == QLatin1String("non_interest_income")) {
        const double v = fa_get(inc, {"Non Interest Income"});
        if (fa_ok(v))
            return v;
        const double r = fa_flow(inc, cf, QStringLiteral("revenue")), n = fa_flow(inc, cf, QStringLiteral("nii"));
        return fa_ok(r) && fa_ok(n) ? r - n : kNaN;
    }
    if (id == QLatin1String("non_interest_expense")) {
        const double x = fa_get(inc, {"Non Interest Expense"});
        if (fa_ok(x))
            return x;
        const double sga = fa_get(inc, {"Selling General And Administration"});
        if (!fa_ok(sga))
            return kNaN;
        double sum = sga;
        for (const char* k : {"Occupancy And Equipment", "Professional Expense And Contract Services Expense",
                              "Other Non Interest Expense"}) {
            const double v = fa_get(inc, {k});
            if (fa_ok(v))
                sum += v;
        }
        return sum;
    }
    if (id == QLatin1String("provisions"))
        return fa_get(inc, {"Credit Losses Provision", "Provision For Doubtful Accounts"});
    if (id == QLatin1String("total_expenses"))
        return fa_get(inc, {"Total Expenses"});
    // Cash flow
    if (id == QLatin1String("cfo"))
        return fa_get(cf, {"Operating Cash Flow", "Cash Flow From Continuing Operating Activities"});
    if (id == QLatin1String("capex"))
        return fa_get(cf, {"Capital Expenditure", "Purchase Of PPE"});
    if (id == QLatin1String("fcf")) {
        const double f = fa_get(cf, {"Free Cash Flow"});
        if (fa_ok(f))
            return f;
        const double o = fa_flow(inc, cf, QStringLiteral("cfo")), c = fa_flow(inc, cf, QStringLiteral("capex"));
        return fa_ok(o) && fa_ok(c) ? o + c : kNaN;
    }
    if (id == QLatin1String("sbc"))
        return fa_get(cf, {"Stock Based Compensation"});
    if (id == QLatin1String("acquisitions"))
        return fa_get(cf, {"Purchase Of Business", "Net Business Purchase And Sale"});
    if (id == QLatin1String("dividends"))
        return fa_get(cf, {"Cash Dividends Paid", "Common Stock Dividend Paid"});
    if (id == QLatin1String("buybacks"))
        return fa_get(cf, {"Repurchase Of Capital Stock", "Common Stock Payments"});
    if (id == QLatin1String("net_debt_flow")) {
        const double n = fa_get(cf, {"Net Issuance Payments Of Debt"});
        if (fa_ok(n))
            return n;
        const double i = fa_get(cf, {"Issuance Of Debt", "Long Term Debt Issuance"});
        const double r = fa_get(cf, {"Repayment Of Debt", "Long Term Debt Payments"});
        if (!fa_ok(i) && !fa_ok(r))
            return kNaN;
        return (fa_ok(i) ? i : 0.0) + (fa_ok(r) ? r : 0.0);
    }
    if (id == QLatin1String("share_issued"))
        return fa_get(cf, {"Issuance Of Capital Stock", "Common Stock Issuance"});
    if (id == QLatin1String("begin_cash"))
        return fa_get(cf, {"Beginning Cash Position"});
    if (id == QLatin1String("end_cash"))
        return fa_get(cf, {"End Cash Position"});
    if (id == QLatin1String("change_in_cash"))
        return fa_get(cf, {"Changes In Cash"});
    if (id == QLatin1String("fx_effect"))
        return fa_get(cf, {"Effect Of Exchange Rate Changes"});
    if (id == QLatin1String("cfi"))
        return fa_get(cf, {"Investing Cash Flow", "Cash Flow From Continuing Investing Activities"});
    if (id == QLatin1String("cff"))
        return fa_get(cf, {"Financing Cash Flow", "Cash Flow From Continuing Financing Activities"});
    if (id == QLatin1String("change_wc"))
        return fa_get(cf, {"Change In Working Capital"});
    return kNaN;
}

const char* const kFaFlowIds[] = {
    "revenue",        "cogs",          "gross_profit",  "rnd",           "sga",           "operating_income",
    "ebit",           "da",            "ebitda",        "interest_expense", "interest_income", "pretax",
    "tax",            "tax_rate_calc", "net_income",    "net_income_total", "ni_cont",     "eps_diluted",
    "shares_diluted", "nii",           "non_interest_income", "non_interest_expense", "provisions", "total_expenses",
    "cfo",            "capex",         "fcf",           "sbc",           "acquisitions",  "dividends",
    "buybacks",       "net_debt_flow", "share_issued",  "begin_cash",    "end_cash",      "change_in_cash",
    "fx_effect",      "cfi",           "cff",           "change_wc",
};

// ── The frame of one period basis ────────────────────────────────────────────────────────────

struct FaFrame {
    QVector<PeriodInfo> periods;
    QVector<QJsonObject> inc, cf, bal, bal_prev;
    QVector<bool> has_prev;
};

QVector<Column> fa_all_balances(const FinancialsData& fd, int fy) {
    QVector<Column> all = fin_model::columns_for(fd, Statement::Balance, Period::Annual, fy);
    for (const auto& q : fin_model::columns_for(fd, Statement::Balance, Period::Quarterly, fy))
        if (fa_match(all, q.end, 12) < 0)
            all.append(q);
    std::sort(all.begin(), all.end(), [](const Column& a, const Column& b) { return a.end < b.end; });
    return all;
}

FaFrame fa_frame(const FinancialsData& fd, Period basis, int fy) {
    FaFrame f;
    const QVector<Column> inc = fin_model::columns_for(fd, Statement::Income, basis, fy);
    const QVector<Column> cf = fin_model::columns_for(fd, Statement::CashFlow, basis, fy);
    const QVector<Column> bal = fin_model::columns_for(fd, Statement::Balance, basis, fy);
    const QVector<Column> all_bal = fa_all_balances(fd, fy);
    const QVector<Column>& axis = !inc.isEmpty() ? inc : (!cf.isEmpty() ? cf : bal);
    f.periods = period_infos(axis, basis);
    const int n = static_cast<int>(axis.size());
    f.inc.resize(n);
    f.cf.resize(n);
    f.bal.resize(n);
    f.bal_prev.resize(n);
    f.has_prev = QVector<bool>(n, false);
    for (int i = 0; i < n; ++i) {
        const QDate d = axis[i].end;
        if (const int k = fa_match(inc, d, 12); k >= 0)
            f.inc[i] = inc[k].data;
        if (const int k = fa_match(cf, d, 12); k >= 0)
            f.cf[i] = cf[k].data;
        if (const int k = fa_match(bal, d, 12); k >= 0)
            f.bal[i] = bal[k].data;
        else if (const int k2 = fa_match(all_bal, d, 12); k2 >= 0)
            f.bal[i] = all_bal[k2].data;
        const bool quarterly = basis == Period::Quarterly;
        const QDate prev = quarterly ? d.addMonths(-3) : d.addYears(-1);
        if (const int k = fa_match(all_bal, prev, quarterly ? 20 : 25); k >= 0) {
            f.bal_prev[i] = all_bal[k].data;
            f.has_prev[i] = true;
        }
    }
    return f;
}

// ── Builder ──────────────────────────────────────────────────────────────────────────────────

/// One reading with its reason; fn() sets `note` (a reason for NaN or a caveat).
using FaFn = std::function<double(int i, QString& note)>;

struct FaBuild {
    Analysis& a;
    const FaFrame& f;

    int n() const { return static_cast<int>(f.periods.size()); }
    double L(const char* id, int i) const { return a.lines.value(QLatin1String(id)).at(i); }
    double annualiser(int i) const { return 365.0 / std::max(1, f.periods[i].days); }

    /// Average of this and the previous balance; the period end alone (with a note) when the
    /// previous balance sheet is not on file.
    double avg(const char* id, int i, QString& note) const {
        const double e = fa_bal(f.bal[i], QLatin1String(id));
        const double p = f.has_prev[i] ? fa_bal(f.bal_prev[i], QLatin1String(id)) : kNaN;
        if (fa_ok(e) && fa_ok(p))
            return (e + p) / 2.0;
        if (fa_ok(e)) {
            if (note.isEmpty())
                note = fa_end_balance_note();
            return e;
        }
        return kNaN;
    }

    /// num ÷ den with the reason when it cannot be computed. `den_positive`: den must be > 0.
    static double ratio(double num, double den, const QString& num_name, const QString& den_name, QString& note,
                        bool den_positive = true) {
        if (!fa_ok(num)) {
            note = fa_missing(num_name);
            return kNaN;
        }
        if (!fa_ok(den)) {
            note = fa_missing(den_name);
            return kNaN;
        }
        if (den_positive ? den <= 0.0 : den == 0.0) {
            note = fa_not_positive(den_name);
            return kNaN;
        }
        return num / den;
    }

    Metric& add(const QString& family, const char* id, const QString& label, Unit unit, const QString& formula,
                bool higher_is_better, bool averaged, const FaFn& fn) {
        Metric m;
        m.id = QLatin1String(id);
        m.family = family;
        m.label = label;
        m.unit = unit;
        m.formula = formula;
        m.higher_is_better = higher_is_better;
        m.averaged = averaged;
        m.s = Series::filled(n());
        for (int i = 0; i < n(); ++i) {
            QString note;
            const double v = fn(i, note);
            m.s.v[i] = v;
            m.s.note[i] = note;
        }
        a.metrics.append(m);
        return a.metrics.last();
    }

    /// YoY of a line against the same period a year earlier.
    double yoy_line(const char* id, int i, QString& note) const {
        const int k = f.periods[i].year_ago;
        if (k < 0) {
            note = FaText::tr("n/a — no period a year earlier on file");
            return kNaN;
        }
        const double cur = L(id, i), prev = L(id, k);
        if (!fa_ok(cur) || !fa_ok(prev)) {
            note = FaText::tr("n/a — not reported in both periods");
            return kNaN;
        }
        return fa_growth(cur, prev, note);
    }
};

// ── Metrics ──────────────────────────────────────────────────────────────────────────────────

void fa_metrics(FaBuild& b) {
    Analysis& a = b.a;
    const bool fin = a.financial();
    const bool bank = a.tmpl == Template::Bank;
    using U = Unit;
    const QString kRevenue = FaText::tr("revenue");
    const QString kNetIncome = FaText::tr("net income");

    // Margins: x ÷ revenue (revenue > 0).
    auto margin = [&b, kRevenue](const char* line, const QString& name) {
        return [&b, line, name, kRevenue](int i, QString& note) {
            return FaBuild::ratio(b.L(line, i), b.L("revenue", i), name, kRevenue, note);
        };
    };

    const QString prof = FaText::tr("PROFITABILITY");
    if (!fin) {
        b.add(prof, "gross_margin", FaText::tr("Gross margin"), U::Percent, FaText::tr("Gross profit ÷ revenue"), true,
              false, margin("gross_profit", FaText::tr("gross profit")));
        b.add(prof, "ebitda_margin", FaText::tr("EBITDA margin"), U::Percent, FaText::tr("EBITDA ÷ revenue"), true, false,
              margin("ebitda", FaText::tr("EBITDA")));
        b.add(prof, "operating_margin", FaText::tr("Operating margin"), U::Percent,
              FaText::tr("Operating income ÷ revenue"), true, false,
              margin("operating_income", FaText::tr("operating income")));
    }
    b.add(prof, "pretax_margin", FaText::tr("Pretax margin"), U::Percent, FaText::tr("Pretax income ÷ revenue"), true,
          false, margin("pretax", FaText::tr("pretax income")));
    b.add(prof, "net_margin", FaText::tr("Net margin"), U::Percent, FaText::tr("Net income to common ÷ revenue"), true,
          false, margin("net_income", kNetIncome));
    if (!fin) {
        b.add(prof, "fcf_margin", FaText::tr("FCF margin"), U::Percent, FaText::tr("Free cash flow ÷ revenue"), true,
              false, margin("fcf", FaText::tr("free cash flow")));
        b.add(prof, "rnd_ratio", FaText::tr("R&D ÷ revenue"), U::Percent, FaText::tr("Research & development ÷ revenue"),
              true, false, margin("rnd", FaText::tr("R&D")));
        b.add(prof, "sga_ratio", FaText::tr("SG&A ÷ revenue"), U::Percent,
              FaText::tr("Selling, general & administrative expense ÷ revenue"), false, false,
              margin("sga", FaText::tr("SG&A")));
    }
    b.add(prof, "tax_rate", FaText::tr("Effective tax rate"), U::Percent, FaText::tr("Income tax ÷ pretax income"),
          false, false, [&b](int i, QString& note) {
              return FaBuild::ratio(b.L("tax", i), b.L("pretax", i), FaText::tr("income tax"),
                                    FaText::tr("pretax income"), note);
          });

    // Effective tax rate for NOPAT: Yahoo's own rate, else tax ÷ pretax, clamped 0–50 %.
    auto tax_rate = [&b](int i) {
        double t = b.L("tax_rate_calc", i);
        if (!fa_ok(t)) {
            const double p = b.L("pretax", i), x = b.L("tax", i);
            t = fa_ok(p) && fa_ok(x) && p > 0.0 ? x / p : kNaN;
        }
        return fa_ok(t) ? std::clamp(t, 0.0, 0.5) : kNaN;
    };

    // RETURNS (average balances, annualised flows)
    const QString ret = FaText::tr("RETURNS");
    const QString kAvgEquity = FaText::tr("average equity");
    const QString kAvgAssets = FaText::tr("average total assets");
    b.add(ret, "roe", FaText::tr("Return on equity"), U::Percent,
          FaText::tr("Net income ÷ average shareholders' equity (annualised)"), true, true, [&b, kAvgEquity, kNetIncome](int i, QString& note) {
              QString cav;
              const double eq = b.avg("equity", i, cav);
              const double v = FaBuild::ratio(b.L("net_income", i) * b.annualiser(i), eq, kNetIncome, kAvgEquity, note);
              if (fa_ok(v))
                  note = cav;
              return v;
          });
    b.add(ret, "roa", FaText::tr("Return on assets"), U::Percent,
          FaText::tr("Net income ÷ average total assets (annualised)"), true, true, [&b, kAvgAssets, kNetIncome](int i, QString& note) {
              QString cav;
              const double ta = b.avg("total_assets", i, cav);
              const double v = FaBuild::ratio(b.L("net_income", i) * b.annualiser(i), ta, kNetIncome, kAvgAssets, note);
              if (fa_ok(v))
                  note = cav;
              return v;
          });
    if (!fin) {
        b.add(ret, "roic", FaText::tr("Return on invested capital"), U::Percent,
              FaText::tr("EBIT × (1 − effective tax rate) ÷ average (total debt + equity − cash & short-term investments)"),
              true, true, [&b, tax_rate](int i, QString& note) {
                  const double t = tax_rate(i);
                  if (!fa_ok(t)) {
                      note = fa_missing(FaText::tr("tax rate"));
                      return kNaN;
                  }
                  QString cav;
                  const double ic = b.avg("invested_capital", i, cav);
                  const double ebit = b.L("ebit", i);
                  const double v = FaBuild::ratio(fa_ok(ebit) ? ebit * (1.0 - t) * b.annualiser(i) : kNaN, ic,
                                                  FaText::tr("EBIT"), FaText::tr("average invested capital"), note);
                  if (fa_ok(v))
                      note = cav;
                  return v;
              });
        b.add(ret, "roce", FaText::tr("Return on capital employed"), U::Percent,
              FaText::tr("EBIT ÷ average (total assets − current liabilities)"), true, true, [&b](int i, QString& note) {
                  QString cav;
                  const double ta = b.avg("total_assets", i, cav), cl = b.avg("current_liabilities", i, cav);
                  const double ce = fa_ok(ta) && fa_ok(cl) ? ta - cl : kNaN;
                  const double ebit = b.L("ebit", i);
                  const double v = FaBuild::ratio(fa_ok(ebit) ? ebit * b.annualiser(i) : kNaN, ce, FaText::tr("EBIT"),
                                                  FaText::tr("capital employed"), note);
                  if (fa_ok(v))
                      note = cav;
                  return v;
              });
    } else {
        b.add(ret, "rote", FaText::tr("Return on tangible equity"), U::Percent,
              FaText::tr("Net income ÷ average tangible book value (annualised)"), true, true,
              [&b, kNetIncome](int i, QString& note) {
                  QString cav;
                  const double tb = b.avg("tangible_book", i, cav);
                  const double v = FaBuild::ratio(b.L("net_income", i) * b.annualiser(i), tb, kNetIncome,
                                                  FaText::tr("average tangible book value"), note);
                  if (fa_ok(v))
                      note = cav;
                  return v;
              });
    }

    // DUPONT: net margin × asset turnover × equity multiplier = ROE.
    const QString dup = FaText::tr("DUPONT");
    b.add(dup, "asset_turnover", FaText::tr("Asset turnover"), U::Multiple,
          FaText::tr("Revenue ÷ average total assets (annualised)"), true, true, [&b, kRevenue, kAvgAssets](int i, QString& note) {
              QString cav;
              const double ta = b.avg("total_assets", i, cav);
              const double v = FaBuild::ratio(b.L("revenue", i) * b.annualiser(i), ta, kRevenue, kAvgAssets, note);
              if (fa_ok(v))
                  note = cav;
              return v;
          });
    b.add(dup, "equity_multiplier", FaText::tr("Equity multiplier"), U::Multiple,
          FaText::tr("Average total assets ÷ average equity"), false, true, [&b, kAvgAssets, kAvgEquity](int i, QString& note) {
              QString cav;
              const double ta = b.avg("total_assets", i, cav), eq = b.avg("equity", i, cav);
              const double v = FaBuild::ratio(ta, eq, kAvgAssets, kAvgEquity, note);
              if (fa_ok(v))
                  note = cav;
              return v;
          });

    // GROWTH (vs the same period a year earlier)
    const QString gr = FaText::tr("GROWTH");
    struct GrowthDef {
        const char* id;
        const char* line;
        const char* label;
        bool industrial_only;
        bool financial_only;
    };
    static const GrowthDef kGrowth[] = {
        {"revenue_growth", "revenue", QT_TRANSLATE_NOOP("EquityFinancials", "Revenue"), false, false},
        {"nii_growth", "nii", QT_TRANSLATE_NOOP("EquityFinancials", "Net interest income"), false, true},
        {"gross_profit_growth", "gross_profit", QT_TRANSLATE_NOOP("EquityFinancials", "Gross profit"), true, false},
        {"ebit_growth", "operating_income", QT_TRANSLATE_NOOP("EquityFinancials", "EBIT (operating income)"), true, false},
        {"pretax_growth", "pretax", QT_TRANSLATE_NOOP("EquityFinancials", "Pretax income"), false, true},
        {"net_income_growth", "net_income", QT_TRANSLATE_NOOP("EquityFinancials", "Net income"), false, false},
        {"eps_growth", "eps_diluted", QT_TRANSLATE_NOOP("EquityFinancials", "EPS (diluted)"), false, false},
        {"cfo_growth", "cfo", QT_TRANSLATE_NOOP("EquityFinancials", "Operating cash flow"), true, false},
        {"fcf_growth", "fcf", QT_TRANSLATE_NOOP("EquityFinancials", "Free cash flow"), true, false},
        {"total_assets_growth", "total_assets", QT_TRANSLATE_NOOP("EquityFinancials", "Total assets"), false, false},
        {"debt_growth", "total_debt", QT_TRANSLATE_NOOP("EquityFinancials", "Total debt"), false, false},
        {"equity_growth", "equity", QT_TRANSLATE_NOOP("EquityFinancials", "Shareholders' equity"), false, false},
    };
    for (const auto& g : kGrowth) {
        if ((g.industrial_only && fin) || (g.financial_only && !fin))
            continue;
        const QString label = QCoreApplication::translate("EquityFinancials", g.label);
        const char* line = g.line;
        Metric& m = b.add(gr, g.id, label, U::Percent,
                          FaText::tr("(|this period| − |a year earlier|) ÷ |a year earlier|; none across a sign change"),
                          QLatin1String(g.id) != QLatin1String("debt_growth"), false,
                          [&b, line](int i, QString& note) { return b.yoy_line(line, i, note); });
        GrowthRow row;
        row.id = QLatin1String(g.line);
        row.label = label;
        row.yoy = m.s;
        a.growth.append(row);
    }

    // LIQUIDITY (industrial balance sheets only — banks do not classify current items)
    if (!fin) {
        const QString liq = FaText::tr("LIQUIDITY");
        const QString kCl = FaText::tr("current liabilities");
        b.add(liq, "current_ratio", FaText::tr("Current ratio"), U::Multiple, FaText::tr("Current assets ÷ current liabilities"),
              true, false, [&b, kCl](int i, QString& note) {
                  return FaBuild::ratio(b.L("current_assets", i), b.L("current_liabilities", i),
                                        FaText::tr("current assets"), kCl, note);
              });
        b.add(liq, "quick_ratio", FaText::tr("Quick ratio"), U::Multiple,
              FaText::tr("(Current assets − inventory) ÷ current liabilities"), true, false, [&b, kCl](int i, QString& note) {
                  const double ca = b.L("current_assets", i), inv = b.L("inventory", i);
                  return FaBuild::ratio(fa_ok(ca) ? ca - (fa_ok(inv) ? inv : 0.0) : kNaN, b.L("current_liabilities", i),
                                        FaText::tr("current assets"), kCl, note);
              });
        b.add(liq, "cash_ratio", FaText::tr("Cash ratio"), U::Multiple,
              FaText::tr("Cash & short-term investments ÷ current liabilities"), true, false, [&b, kCl](int i, QString& note) {
                  return FaBuild::ratio(b.L("cash_sti", i), b.L("current_liabilities", i), FaText::tr("cash"), kCl, note);
              });
    }

    // LEVERAGE
    const QString lev = FaText::tr("LEVERAGE");
    b.add(lev, "debt_to_equity", FaText::tr("Debt ÷ equity"), U::Multiple,
          FaText::tr("Total debt ÷ shareholders' equity (period end)"), false, false, [&b](int i, QString& note) {
              return FaBuild::ratio(b.L("total_debt", i), b.L("equity", i), FaText::tr("total debt"), FaText::tr("equity"),
                                    note);
          });
    if (!fin) {
        b.add(lev, "net_debt", FaText::tr("Net debt"), U::Money,
              FaText::tr("Total debt − cash & short-term investments; negative = net cash"), false, false,
              [&b](int i, QString& note) {
                  const double d = b.L("total_debt", i), c = b.L("cash_sti", i);
                  if (!fa_ok(d)) {
                      note = fa_missing(FaText::tr("total debt"));
                      return kNaN;
                  }
                  if (!fa_ok(c)) {
                      note = fa_missing(FaText::tr("cash"));
                      return kNaN;
                  }
                  return d - c;
              });
        b.add(lev, "net_debt_ebitda", FaText::tr("Net debt ÷ EBITDA"), U::Multiple,
              FaText::tr("(Total debt − cash & short-term investments) ÷ EBITDA (annualised); negative = net cash"), false,
              false, [&b](int i, QString& note) {
                  const double d = b.L("total_debt", i), c = b.L("cash_sti", i);
                  const double nd = fa_ok(d) && fa_ok(c) ? d - c : kNaN;
                  const double e = b.L("ebitda", i);
                  return FaBuild::ratio(nd, fa_ok(e) ? e * b.annualiser(i) : kNaN, FaText::tr("net debt"),
                                        FaText::tr("EBITDA"), note);
              });
        b.add(lev, "interest_cover", FaText::tr("Interest cover"), U::Multiple, FaText::tr("EBIT ÷ interest expense"), true,
              false, [&b](int i, QString& note) {
                  const double ebit = b.L("ebit", i);
                  if (fa_ok(ebit) && ebit <= 0.0) {
                      note = FaText::tr("n/a — operating loss (EBIT not positive)");
                      return kNaN;
                  }
                  return FaBuild::ratio(ebit, b.L("interest_expense", i), FaText::tr("EBIT"),
                                        FaText::tr("interest expense"), note);
              });
    }
    b.add(lev, "debt_to_assets", FaText::tr("Debt ÷ assets"), U::Percent, FaText::tr("Total debt ÷ total assets"), false,
          false, [&b](int i, QString& note) {
              return FaBuild::ratio(b.L("total_debt", i), b.L("total_assets", i), FaText::tr("total debt"),
                                    FaText::tr("total assets"), note);
          });
    b.add(lev, "equity_to_assets", FaText::tr("Equity ÷ assets"), U::Percent,
          FaText::tr("Shareholders' equity ÷ total assets (capital strength)"), true, false, [&b](int i, QString& note) {
              return FaBuild::ratio(b.L("equity", i), b.L("total_assets", i), FaText::tr("equity"),
                                    FaText::tr("total assets"), note);
          });

    // EFFICIENCY (working capital, average balances)
    if (!fin) {
        const QString eff = FaText::tr("EFFICIENCY");
        const QString kCogs = FaText::tr("cost of revenue");
        auto days = [&b](const char* bal, const char* flow, const QString& bal_name, const QString& flow_name) {
            return [&b, bal, flow, bal_name, flow_name](int i, QString& note) {
                QString cav;
                const double avg = b.avg(bal, i, cav);
                const double r = FaBuild::ratio(avg, b.L(flow, i), bal_name, flow_name, note);
                if (!fa_ok(r))
                    return kNaN;
                note = cav;
                return r * b.f.periods[i].days;
            };
        };
        b.add(eff, "dso", FaText::tr("Days sales outstanding"), U::Days,
              FaText::tr("Average accounts receivable ÷ revenue × days in period"), false, true,
              days("receivables", "revenue", FaText::tr("receivables"), kRevenue));
        b.add(eff, "dio", FaText::tr("Days inventory outstanding"), U::Days,
              FaText::tr("Average inventory ÷ cost of revenue × days in period (0 without an inventory line)"), false, true,
              [&b, kCogs](int i, QString& note) {
                  const double cogs = b.L("cogs", i);
                  if (fa_ok(cogs) && cogs > 0.0 && !fa_ok(b.L("inventory", i))) {
                      note = FaText::tr("no inventory line — an asset-light business");
                      return 0.0;
                  }
                  QString cav;
                  const double avg = b.avg("inventory", i, cav);
                  const double r = FaBuild::ratio(avg, cogs, FaText::tr("inventory"), kCogs, note);
                  if (!fa_ok(r))
                      return kNaN;
                  note = cav;
                  return r * b.f.periods[i].days;
              });
        b.add(eff, "dpo", FaText::tr("Days payables outstanding"), U::Days,
              FaText::tr("Average accounts payable ÷ cost of revenue × days in period"), true, true,
              days("payables", "cogs", FaText::tr("payables"), kCogs));
        // CCC from the three just added.
        const Metric* dso = a.metric(QStringLiteral("dso"));
        const Metric* dio = a.metric(QStringLiteral("dio"));
        const Metric* dpo = a.metric(QStringLiteral("dpo"));
        const Series sdso = dso ? dso->s : Series::filled(b.n());
        const Series sdio = dio ? dio->s : Series::filled(b.n());
        const Series sdpo = dpo ? dpo->s : Series::filled(b.n());
        b.add(eff, "ccc", FaText::tr("Cash conversion cycle"), U::Days,
              FaText::tr("DSO + DIO − DPO (days of working capital tied up)"), false, true,
              [sdso, sdio, sdpo](int i, QString& note) {
                  if (!fa_ok(sdso.at(i))) {
                      note = sdso.note_at(i);
                      return kNaN;
                  }
                  if (!fa_ok(sdio.at(i))) {
                      note = sdio.note_at(i);
                      return kNaN;
                  }
                  if (!fa_ok(sdpo.at(i))) {
                      note = sdpo.note_at(i);
                      return kNaN;
                  }
                  note = sdso.note_at(i);
                  return sdso.at(i) + sdio.at(i) - sdpo.at(i);
              });
        b.add(eff, "inventory_turnover", FaText::tr("Inventory turnover"), U::Multiple,
              FaText::tr("Cost of revenue ÷ average inventory (annualised)"), true, true, [&b, kCogs](int i, QString& note) {
                  QString cav;
                  const double inv = b.avg("inventory", i, cav);
                  const double c = b.L("cogs", i);
                  const double v = FaBuild::ratio(fa_ok(c) ? c * b.annualiser(i) : kNaN, inv, kCogs,
                                                  FaText::tr("average inventory"), note);
                  if (fa_ok(v))
                      note = cav;
                  return v;
              });
    }

    // BANK
    if (fin) {
        const QString bk = bank ? FaText::tr("BANKING") : FaText::tr("INSURANCE");
        if (bank) {
            b.add(bk, "nii_share", FaText::tr("Net interest income share"), U::Percent,
                  FaText::tr("Net interest income ÷ total revenue"), true, false,
                  margin("nii", FaText::tr("net interest income")));
            b.add(bk, "nii_to_assets", FaText::tr("NII ÷ average assets"), U::Percent,
                  FaText::tr("Net interest income ÷ average total assets (annualised; a net-interest-margin proxy)"), true,
                  true, [&b, kAvgAssets](int i, QString& note) {
                      QString cav;
                      const double ta = b.avg("total_assets", i, cav);
                      const double nii = b.L("nii", i);
                      const double v = FaBuild::ratio(fa_ok(nii) ? nii * b.annualiser(i) : kNaN, ta,
                                                      FaText::tr("net interest income"), kAvgAssets, note);
                      if (fa_ok(v))
                          note = cav;
                      return v;
                  });
            b.add(bk, "efficiency_ratio", FaText::tr("Efficiency ratio"), U::Percent,
                  FaText::tr("Non-interest expense ÷ total revenue (lower is better)"), false, false,
                  margin("non_interest_expense", FaText::tr("non-interest expense")));
            b.add(bk, "loans_to_deposits", FaText::tr("Loans ÷ deposits"), U::Percent, FaText::tr("Net loans ÷ total deposits"),
                  false, false, [&b](int i, QString& note) {
                      return FaBuild::ratio(b.L("loans", i), b.L("deposits", i), FaText::tr("loans"),
                                            FaText::tr("deposits"), note);
                  });
            b.add(bk, "provisions_to_revenue", FaText::tr("Provisions ÷ revenue"), U::Percent,
                  FaText::tr("Credit-loss provisions ÷ total revenue"), false, false,
                  margin("provisions", FaText::tr("credit-loss provisions")));
        } else {
            b.add(bk, "expense_ratio", FaText::tr("Expenses ÷ revenue"), U::Percent,
                  FaText::tr("Total benefits and expenses ÷ total revenue (a combined-ratio proxy)"), false, false,
                  margin("total_expenses", FaText::tr("total expenses")));
        }
    }

    // CASH FLOW QUALITY (not meaningful for banks: their operating cash flow is balance-sheet churn)
    if (!fin) {
        const QString q = FaText::tr("CASH FLOW QUALITY");
        b.add(q, "cfo_to_ni", FaText::tr("Cash conversion (CFO ÷ NI)"), U::Multiple,
              FaText::tr("Operating cash flow ÷ net income; above 1 = profits backed by cash"), true, false,
              [&b, kNetIncome](int i, QString& note) {
                  return FaBuild::ratio(b.L("cfo", i), b.L("net_income", i), FaText::tr("operating cash flow"), kNetIncome,
                                        note);
              });
        b.add(q, "fcf_conversion", FaText::tr("FCF conversion"), U::Percent, FaText::tr("Free cash flow ÷ net income"), true,
              false, [&b, kNetIncome](int i, QString& note) {
                  return FaBuild::ratio(b.L("fcf", i), b.L("net_income", i), FaText::tr("free cash flow"), kNetIncome,
                                        note);
              });
        b.add(q, "accruals_ratio", FaText::tr("Accruals ratio"), U::Percent,
              FaText::tr("(Net income − operating cash flow) ÷ average total assets (annualised); high = earnings ahead of cash"),
              false, true, [&b, kAvgAssets](int i, QString& note) {
                  const double ni = b.L("net_income", i), cfo = b.L("cfo", i);
                  QString cav;
                  const double ta = b.avg("total_assets", i, cav);
                  if (!fa_ok(cfo)) {
                      note = fa_missing(FaText::tr("operating cash flow"));
                      return kNaN;
                  }
                  const double v = FaBuild::ratio(fa_ok(ni) ? (ni - cfo) * b.annualiser(i) : kNaN, ta,
                                                  FaText::tr("net income"), kAvgAssets, note);
                  if (fa_ok(v))
                      note = cav;
                  return v;
              });
        b.add(q, "capex_to_da", FaText::tr("Capex ÷ depreciation"), U::Multiple,
              FaText::tr("|Capital expenditure| ÷ depreciation & amortisation; above 1 = investing beyond upkeep"), true,
              false, [&b](int i, QString& note) {
                  const double c = b.L("capex", i);
                  return FaBuild::ratio(fa_ok(c) ? std::fabs(c) : kNaN, b.L("da", i), FaText::tr("capex"),
                                        FaText::tr("depreciation"), note);
              });
        b.add(q, "capex_to_revenue", FaText::tr("Capex ÷ revenue"), U::Percent,
              FaText::tr("|Capital expenditure| ÷ revenue"), false, false, [&b, kRevenue](int i, QString& note) {
                  const double c = b.L("capex", i);
                  return FaBuild::ratio(fa_ok(c) ? std::fabs(c) : kNaN, b.L("revenue", i), FaText::tr("capex"), kRevenue,
                                        note);
              });
        b.add(q, "sbc_to_revenue", FaText::tr("SBC ÷ revenue"), U::Percent,
              FaText::tr("Stock-based compensation ÷ revenue (a non-cash cost added back to cash flow)"), false, false,
              margin("sbc", FaText::tr("stock-based compensation")));
        b.add(q, "ar_vs_revenue", FaText::tr("Receivables vs revenue growth"), U::PctPoints,
              FaText::tr("Receivables growth − revenue growth (pp); above 0 = receivables outgrow sales"), false, false,
              [&b](int i, QString& note) {
                  QString n1, n2;
                  const double ar = b.yoy_line("receivables", i, n1);
                  const double rv = b.yoy_line("revenue", i, n2);
                  if (!fa_ok(ar)) {
                      note = n1;
                      return kNaN;
                  }
                  if (!fa_ok(rv)) {
                      note = n2;
                      return kNaN;
                  }
                  return ar - rv;
              });
    }

    // PER SHARE
    const QString ps = FaText::tr("PER SHARE");
    const QString kShares = FaText::tr("diluted shares");
    b.add(ps, "eps_diluted", FaText::tr("EPS (diluted)"), U::PerShare, FaText::tr("Diluted earnings per share as filed"), true,
          false, [&b](int i, QString& note) {
              const double v = b.L("eps_diluted", i);
              if (!fa_ok(v))
                  note = fa_missing(FaText::tr("diluted EPS"));
              return v;
          });
    b.add(ps, "revenue_ps", FaText::tr("Revenue per share"), U::PerShare, FaText::tr("Revenue ÷ diluted average shares"),
          true, false, [&b, kRevenue, kShares](int i, QString& note) {
              return FaBuild::ratio(b.L("revenue", i), b.L("shares_diluted", i), kRevenue, kShares, note);
          });
    if (!fin)
        b.add(ps, "fcf_ps", FaText::tr("FCF per share"), U::PerShare, FaText::tr("Free cash flow ÷ diluted average shares"),
              true, false, [&b, kShares](int i, QString& note) {
                  return FaBuild::ratio(b.L("fcf", i), b.L("shares_diluted", i), FaText::tr("free cash flow"), kShares, note);
              });
    b.add(ps, "bvps", FaText::tr("Book value per share"), U::PerShare,
          FaText::tr("Shareholders' equity ÷ shares outstanding (period end)"), true, false, [&b](int i, QString& note) {
              return FaBuild::ratio(b.L("equity", i), b.L("shares_out", i), FaText::tr("equity"),
                                    FaText::tr("shares outstanding"), note, false);
          });
    b.add(ps, "dps", FaText::tr("Dividends per share (cash paid)"), U::PerShare,
          FaText::tr("|Cash dividends paid| ÷ diluted average shares"), true, false, [&b, kShares](int i, QString& note) {
              const double d = b.L("dividends", i);
              return FaBuild::ratio(fa_ok(d) ? std::fabs(d) : kNaN, b.L("shares_diluted", i), FaText::tr("dividends paid"),
                                    kShares, note);
          });
    b.add(ps, "shares_diluted", FaText::tr("Diluted average shares"), U::Shares,
          FaText::tr("Weighted-average diluted share count"), false, false, [&b](int i, QString& note) {
              const double v = b.L("shares_diluted", i);
              if (!fa_ok(v))
                  note = fa_missing(FaText::tr("diluted shares"));
              return v;
          });
    b.add(ps, "share_change", FaText::tr("Share count change (YoY)"), U::Percent,
          FaText::tr("Diluted average shares vs a year earlier; negative = net buybacks"), false, false,
          [&b](int i, QString& note) {
              const int k = b.f.periods[i].year_ago;
              if (k < 0) {
                  note = FaText::tr("n/a — no period a year earlier on file");
                  return kNaN;
              }
              return FaBuild::ratio(b.L("shares_diluted", i) - b.L("shares_diluted", k), b.L("shares_diluted", k),
                                    FaText::tr("diluted shares"), FaText::tr("diluted shares a year earlier"), note);
          });

    // CAPITAL RETURN
    const QString cr = FaText::tr("CAPITAL RETURN");
    b.add(cr, "dividends_paid", FaText::tr("Dividends paid"), U::Money, FaText::tr("Cash dividends paid (as a positive amount)"),
          true, false, [&b](int i, QString& note) {
              const double d = b.L("dividends", i);
              if (!fa_ok(d))
                  note = fa_missing(FaText::tr("dividends paid"));
              return fa_ok(d) ? std::fabs(d) : kNaN;
          });
    b.add(cr, "buybacks_paid", FaText::tr("Share buybacks"), U::Money, FaText::tr("Repurchase of shares (as a positive amount)"),
          true, false, [&b](int i, QString& note) {
              const double d = b.L("buybacks", i);
              if (!fa_ok(d))
                  note = fa_missing(FaText::tr("share buybacks"));
              return fa_ok(d) ? std::fabs(d) : kNaN;
          });
    b.add(cr, "payout_ratio", FaText::tr("Dividend payout"), U::Percent, FaText::tr("|Cash dividends paid| ÷ net income"),
          false, false, [&b, kNetIncome](int i, QString& note) {
              const double d = b.L("dividends", i);
              return FaBuild::ratio(fa_ok(d) ? std::fabs(d) : kNaN, b.L("net_income", i), FaText::tr("dividends paid"),
                                    kNetIncome, note);
          });
    auto total_return = [&b](int i) {
        const double d = b.L("dividends", i), r = b.L("buybacks", i);
        if (!fa_ok(d) && !fa_ok(r))
            return kNaN;
        return (fa_ok(d) ? std::fabs(d) : 0.0) + (fa_ok(r) ? std::fabs(r) : 0.0);
    };
    b.add(cr, "total_payout_ni", FaText::tr("Dividends + buybacks ÷ NI"), U::Percent,
          FaText::tr("(|Dividends| + |buybacks|) ÷ net income"), false, false, [&b, total_return, kNetIncome](int i, QString& note) {
              return FaBuild::ratio(total_return(i), b.L("net_income", i), FaText::tr("dividends and buybacks"), kNetIncome,
                                    note);
          });
    if (!fin)
        b.add(cr, "total_payout_fcf", FaText::tr("Dividends + buybacks ÷ FCF"), U::Percent,
              FaText::tr("(|Dividends| + |buybacks|) ÷ free cash flow"), false, false, [&b, total_return](int i, QString& note) {
                  return FaBuild::ratio(total_return(i), b.L("fcf", i), FaText::tr("dividends and buybacks"),
                                        FaText::tr("free cash flow"), note);
              });
}

// ── Balance-sheet structure ─────────────────────────────────────────────────────────────────

void fa_structure(FaBuild& b) {
    Analysis& a = b.a;
    const int n = b.n();
    auto seg = [n](const char* id, const QString& label) {
        Segment s;
        s.id = QLatin1String(id);
        s.label = label;
        s.value = Series::filled(n);
        return s;
    };
    const bool fin = a.financial();
    QVector<Segment> assets, funding;
    if (!fin) {
        assets << seg("cash_sti", FaText::tr("Cash & ST investments")) << seg("receivables_total", FaText::tr("Receivables"))
               << seg("inventory", FaText::tr("Inventory")) << seg("ppe", FaText::tr("PP&E"))
               << seg("intangibles", FaText::tr("Goodwill & intangibles")) << seg("other", FaText::tr("Other assets"));
        funding << seg("payables_accrued", FaText::tr("Payables & accruals")) << seg("short_debt", FaText::tr("Short-term debt"))
                << seg("long_debt", FaText::tr("Long-term debt")) << seg("other", FaText::tr("Other liabilities"))
                << seg("total_equity", FaText::tr("Equity"));
    } else {
        assets << seg("cash", FaText::tr("Cash & fed funds")) << seg("sti", FaText::tr("Short-term investments"))
               << seg("securities", FaText::tr("Securities & investments")) << seg("loans", FaText::tr("Loans"))
               << seg("receivables_total", FaText::tr("Receivables")) << seg("intangibles", FaText::tr("Goodwill & intangibles"))
               << seg("other", FaText::tr("Other assets"));
        funding << seg("deposits", FaText::tr("Deposits")) << seg("short_debt", FaText::tr("Short-term borrowings"))
                << seg("long_debt", FaText::tr("Long-term debt")) << seg("payables_accrued", FaText::tr("Payables"))
                << seg("other", FaText::tr("Other liabilities")) << seg("total_equity", FaText::tr("Equity"));
    }
    bool loans_filed = false, deposits_filed = false;
    for (int i = 0; i < n; ++i) {
        const QJsonObject& bal = b.f.bal[i];
        const double ta = fa_bal(bal, QStringLiteral("total_assets"));
        const double tl = fa_bal(bal, QStringLiteral("total_liabilities"));
        if (!fa_ok(ta) || ta <= 0.0)
            continue;
        // Asset side: explicit lines, then "other" = the rest.
        double used = 0.0;
        for (auto& s : assets) {
            if (s.id == QLatin1String("other"))
                continue;
            double v = fa_bal(bal, s.id);
            // A bank's short-term investments may already sit inside "Investments And Advances":
            // drop them when the explicit lines would exceed total assets.
            if (fa_ok(v) && v > 0.0) {
                s.value.v[i] = v;
                used += v;
            } else {
                s.value.v[i] = 0.0;
            }
            if (s.id == QLatin1String("loans") && fa_ok(v))
                loans_filed = true;
        }
        if (used > ta * 1.02) {
            for (auto& s : assets)
                if (s.id == QLatin1String("sti") && s.value.v[i] > 0.0) {
                    used -= s.value.v[i];
                    s.value.v[i] = 0.0;
                    s.value.note[i] = FaText::tr("left out — overlaps the securities line");
                }
        }
        for (auto& s : assets)
            if (s.id == QLatin1String("other"))
                s.value.v[i] = std::max(0.0, ta - used);
        // Funding side: liabilities lines, other liabilities = TL − lines, equity = TA − TL.
        const double eq = fa_ok(tl) ? ta - tl : fa_bal(bal, QStringLiteral("total_equity"));
        double lines_sum = 0.0;
        for (auto& s : funding) {
            if (s.id == QLatin1String("other") || s.id == QLatin1String("total_equity"))
                continue;
            const double v = fa_bal(bal, s.id);
            s.value.v[i] = fa_ok(v) && v > 0.0 ? v : 0.0;
            lines_sum += s.value.v[i];
            if (s.id == QLatin1String("deposits") && fa_ok(v))
                deposits_filed = true;
        }
        for (auto& s : funding) {
            if (s.id == QLatin1String("other"))
                s.value.v[i] = fa_ok(tl) ? std::max(0.0, tl - lines_sum) : 0.0;
            else if (s.id == QLatin1String("total_equity")) {
                s.value.v[i] = fa_ok(eq) ? std::max(0.0, eq) : 0.0;
                if (fa_ok(eq) && eq < 0.0)
                    s.value.note[i] = FaText::tr("shareholders' deficit — liabilities exceed assets");
            }
        }
    }
    // Labels that say what the remainder holds when the source files no separate line.
    if (fin) {
        for (auto& s : assets)
            if (s.id == QLatin1String("other") && !loans_filed)
                s.label = FaText::tr("Loans & other assets");
        for (auto& s : funding)
            if (s.id == QLatin1String("other") && !deposits_filed)
                s.label = FaText::tr("Deposits & other liabilities");
    }
    // Segments that are zero everywhere carry no information.
    auto prune = [](QVector<Segment>& v) {
        QVector<Segment> out;
        for (const auto& s : v) {
            bool any = false;
            for (double x : s.value.v)
                any = any || (fa_ok(x) && x > 0.0);
            if (any)
                out.append(s);
        }
        v = out;
    };
    prune(assets);
    prune(funding);
    a.assets_mix = assets;
    a.funding_mix = funding;
}

// ── Scores (fiscal years) ───────────────────────────────────────────────────────────────────

struct FaYear {
    QJsonObject inc, cf, bal, bal_prev;
    bool has_prev = false;
    QString label;
};

double fa_yflow(const FaYear& y, const char* id) {
    return fa_flow(y.inc, y.cf, QLatin1String(id));
}
double fa_ybal(const FaYear& y, const char* id) {
    return fa_bal(y.bal, QLatin1String(id));
}

Check fa_check(const char* id, const QString& label, const QString& formula) {
    Check c;
    c.id = QLatin1String(id);
    c.label = label;
    c.formula = formula;
    return c;
}

Score fa_piotroski(const FaYear& t, const FaYear& p, bool financial) {
    Score s;
    s.id = QStringLiteral("piotroski");
    s.label = FaText::tr("Piotroski F-score");
    s.formula = FaText::tr("Nine pass / fail tests on profitability, funding and efficiency; 8–9 strong, 0–2 weak");
    s.period_label = FaText::tr("%1 vs %2").arg(t.label, p.label);
    const double ta_beg_t = fa_bal(t.bal_prev, QStringLiteral("total_assets"));
    const double ta_beg_p = fa_bal(p.bal_prev, QStringLiteral("total_assets"));
    const double ta_t = fa_ybal(t, "total_assets"), ta_p = fa_ybal(p, "total_assets");
    // Beginning-of-year total assets: last year's year-end. The year before that may be missing:
    // then that year's own year-end is used (noted).
    const double beg_t = fa_ok(ta_p) ? ta_p : (fa_ok(ta_beg_t) ? ta_beg_t : ta_t);
    const double beg_p = fa_ok(ta_beg_p) ? ta_beg_p : ta_p;
    const QString beg_note = fa_ok(ta_beg_p) ? QString() : FaText::tr("last year's ROA on its year-end assets");
    const double ni_t = fa_yflow(t, "ni_cont"), ni_p = fa_yflow(p, "ni_cont");
    const double cfo_t = fa_yflow(t, "cfo");
    auto roa = [](double ni, double ta) { return fa_ok(ni) && fa_ok(ta) && ta > 0.0 ? ni / ta : kNaN; };
    const double roa_t = roa(ni_t, beg_t), roa_p = roa(ni_p, beg_p);

    auto set = [](Check& c, double v, double prior, bool computable, bool pass, const QString& why) {
        c.value = v;
        c.prior = prior;
        if (!computable) {
            c.pass = -1;
            c.note = why;
        } else {
            c.pass = pass ? 1 : 0;
            c.note = why;
        }
    };

    Check c1 = fa_check("roa_positive", FaText::tr("Return on assets > 0"),
                        FaText::tr("Net income before extraordinary items ÷ beginning-of-year total assets"));
    set(c1, roa_t, kNaN, fa_ok(roa_t), roa_t > 0.0, fa_ok(roa_t) ? QString() : fa_missing(FaText::tr("net income or total assets")));
    Check c2 = fa_check("cfo_positive", FaText::tr("Operating cash flow > 0"), FaText::tr("Cash from operations this year"));
    set(c2, fa_ok(cfo_t) && fa_ok(beg_t) && beg_t > 0 ? cfo_t / beg_t : cfo_t, kNaN, fa_ok(cfo_t), cfo_t > 0.0,
        fa_ok(cfo_t) ? QString() : fa_missing(FaText::tr("operating cash flow")));
    Check c3 = fa_check("roa_up", FaText::tr("ROA improved"), FaText::tr("ROA this year > ROA last year"));
    set(c3, roa_t, roa_p, fa_ok(roa_t) && fa_ok(roa_p), roa_t > roa_p,
        fa_ok(roa_t) && fa_ok(roa_p) ? beg_note : fa_missing(FaText::tr("last year's net income or assets")));
    Check c4 = fa_check("accrual", FaText::tr("Cash flow > net income"),
                        FaText::tr("CFO ÷ beginning assets > ROA (earnings backed by cash)"));
    const double cfo_ta = fa_ok(cfo_t) && fa_ok(beg_t) && beg_t > 0 ? cfo_t / beg_t : kNaN;
    set(c4, cfo_ta, roa_t, fa_ok(cfo_ta) && fa_ok(roa_t), cfo_ta > roa_t,
        fa_ok(cfo_ta) && fa_ok(roa_t) ? QString() : fa_missing(FaText::tr("operating cash flow or net income")));

    // Long-term debt ÷ average total assets: a decrease (or no debt either year) passes.
    Check c5 = fa_check("leverage_down", FaText::tr("Leverage fell"),
                        FaText::tr("Long-term debt ÷ average total assets lower than last year"));
    {
        const double ltd_t = fa_ybal(t, "long_debt"), ltd_p = fa_ybal(p, "long_debt");
        const double avg_t = fa_ok(ta_t) && fa_ok(beg_t) ? (ta_t + beg_t) / 2.0 : ta_t;
        const double avg_p = fa_ok(ta_p) && fa_ok(beg_p) ? (ta_p + beg_p) / 2.0 : ta_p;
        if (!fa_ok(ltd_t) && !fa_ok(ltd_p)) {
            set(c5, 0.0, 0.0, true, true, FaText::tr("no long-term debt either year"));
        } else {
            const double lt = (fa_ok(ltd_t) ? ltd_t : 0.0), lp = (fa_ok(ltd_p) ? ltd_p : 0.0);
            const bool ok = fa_ok(avg_t) && fa_ok(avg_p) && avg_t > 0 && avg_p > 0;
            const double rt = ok ? lt / avg_t : kNaN, rp = ok ? lp / avg_p : kNaN;
            set(c5, rt, rp, ok, rt <= rp, ok ? QString() : fa_missing(FaText::tr("total assets")));
        }
    }
    Check c6 = fa_check("liquidity_up", FaText::tr("Current ratio improved"),
                        FaText::tr("Current assets ÷ current liabilities higher than last year"));
    {
        QString unused;
        const double cr_t =
            FaBuild::ratio(fa_ybal(t, "current_assets"), fa_ybal(t, "current_liabilities"), QString(), QString(), unused);
        const double cr_p =
            FaBuild::ratio(fa_ybal(p, "current_assets"), fa_ybal(p, "current_liabilities"), QString(), QString(), unused);
        set(c6, cr_t, cr_p, fa_ok(cr_t) && fa_ok(cr_p), cr_t > cr_p,
            financial ? FaText::tr("n/a — banks and insurers do not classify current items")
                      : (fa_ok(cr_t) && fa_ok(cr_p) ? QString() : fa_missing(FaText::tr("current assets or liabilities"))));
    }
    Check c7 = fa_check("no_dilution", FaText::tr("No new shares issued"),
                        FaText::tr("Shares outstanding did not rise (0.1 % tolerance)"));
    {
        double s_t = fa_ybal(t, "shares_out"), s_p = fa_ybal(p, "shares_out");
        QString how;
        if (!fa_ok(s_t) || !fa_ok(s_p)) {
            s_t = fa_yflow(t, "shares_diluted");
            s_p = fa_yflow(p, "shares_diluted");
            how = FaText::tr("on diluted average shares");
        }
        const bool ok = fa_ok(s_t) && fa_ok(s_p);
        set(c7, s_t, s_p, ok, s_t <= s_p * 1.001, ok ? how : fa_missing(FaText::tr("share count")));
    }
    Check c8 = fa_check("margin_up", FaText::tr("Gross margin improved"), FaText::tr("Gross profit ÷ revenue higher than last year"));
    {
        const double rv_t = fa_yflow(t, "revenue"), rv_p = fa_yflow(p, "revenue");
        const double gm_t = fa_ok(rv_t) && rv_t > 0 ? fa_yflow(t, "gross_profit") / rv_t : kNaN;
        const double gm_p = fa_ok(rv_p) && rv_p > 0 ? fa_yflow(p, "gross_profit") / rv_p : kNaN;
        set(c8, gm_t, gm_p, fa_ok(gm_t) && fa_ok(gm_p), gm_t > gm_p,
            financial ? FaText::tr("n/a — no gross margin for banks and insurers")
                      : (fa_ok(gm_t) && fa_ok(gm_p) ? QString() : fa_missing(FaText::tr("gross profit"))));
    }
    Check c9 = fa_check("turnover_up", FaText::tr("Asset turnover improved"),
                        FaText::tr("Revenue ÷ beginning-of-year total assets higher than last year"));
    {
        const double at_t = fa_ok(beg_t) && beg_t > 0 ? fa_yflow(t, "revenue") / beg_t : kNaN;
        const double at_p = fa_ok(beg_p) && beg_p > 0 ? fa_yflow(p, "revenue") / beg_p : kNaN;
        set(c9, at_t, at_p, fa_ok(at_t) && fa_ok(at_p), at_t > at_p,
            fa_ok(at_t) && fa_ok(at_p) ? beg_note : fa_missing(FaText::tr("revenue or total assets")));
    }
    s.parts = {c1, c2, c3, c4, c5, c6, c7, c8, c9};
    for (const auto& c : s.parts) {
        if (c.pass >= 0) {
            ++s.available;
            s.passed += c.pass;
        }
    }
    if (s.available == 0) {
        s.note = FaText::tr("n/a — two fiscal years of statements are needed");
        return s;
    }
    s.value = s.passed;
    s.safe = 7.0;
    s.distress = 3.0;
    s.zone = s.passed >= 7 ? Zone::Safe : s.passed <= 3 ? Zone::Distress : Zone::Grey;
    if (s.available < 9)
        s.note = FaText::tr("%1 of 9 tests computable").arg(s.available);
    return s;
}

Score fa_altman(const FaYear& t, Template tmpl, const Context& ctx) {
    Score s;
    s.id = QStringLiteral("altman");
    s.label = FaText::tr("Altman Z-score");
    s.period_label = t.label;
    if (tmpl != Template::Industrial) {
        s.note = FaText::tr("n/a — not defined for banks and insurers");
        return s;
    }
    static const char* const kMfg[] = {"Technology",         "Industrials", "Basic Materials", "Energy",
                                       "Consumer Cyclical",  "Consumer Defensive", "Healthcare"};
    bool mfg_sector = false;
    for (const char* m : kMfg)
        mfg_sector = mfg_sector || ctx.sector.compare(QLatin1String(m), Qt::CaseInsensitive) == 0;
    const double inv = fa_ybal(t, "inventory");
    const bool manufacturer = mfg_sector && fa_ok(inv) && inv > 0.0;
    const bool original = manufacturer && fa_ok(ctx.market_cap) && ctx.market_cap > 0.0;

    const double ta = fa_ybal(t, "total_assets"), tl = fa_ybal(t, "total_liabilities");
    const double wc = fa_ybal(t, "working_capital"), re = fa_ybal(t, "retained_earnings");
    const double ebit = fa_yflow(t, "ebit"), rev = fa_yflow(t, "revenue");
    const double eq = original ? ctx.market_cap : fa_ybal(t, "total_equity");
    auto part = [&](const char* id, const QString& label, const QString& formula, double num, double den,
                    const QString& num_name, const QString& den_name) {
        Check c = fa_check(id, label, formula);
        QString why;
        c.value = FaBuild::ratio(num, den, num_name, den_name, why);
        c.note = why;
        c.pass = -1;
        return c;
    };
    const QString kTa = FaText::tr("total assets");
    s.parts << part("x1", FaText::tr("X1 working capital ÷ assets"), FaText::tr("(Current assets − current liabilities) ÷ total assets"),
                    wc, ta, FaText::tr("working capital"), kTa)
            << part("x2", FaText::tr("X2 retained earnings ÷ assets"), FaText::tr("Retained earnings ÷ total assets"), re, ta,
                    FaText::tr("retained earnings"), kTa)
            << part("x3", FaText::tr("X3 EBIT ÷ assets"), FaText::tr("EBIT ÷ total assets"), ebit, ta, FaText::tr("EBIT"), kTa)
            << part("x4", original ? FaText::tr("X4 market value ÷ liabilities") : FaText::tr("X4 book equity ÷ liabilities"),
                    original ? FaText::tr("Market value of equity ÷ total liabilities")
                             : FaText::tr("Book value of equity ÷ total liabilities"),
                    eq, tl, original ? FaText::tr("market value") : FaText::tr("equity"), FaText::tr("total liabilities"));
    if (original)
        s.parts << part("x5", FaText::tr("X5 revenue ÷ assets"), FaText::tr("Revenue ÷ total assets"), rev, ta,
                        FaText::tr("revenue"), kTa);
    if (original) {
        s.variant = FaText::tr("Z (1968, public manufacturers)");
        s.formula = FaText::tr("Z = 1.2·X1 + 1.4·X2 + 3.3·X3 + 0.6·X4 + 1.0·X5; safe above 2.99, distress below 1.81");
        s.safe = 2.99;
        s.distress = 1.81;
    } else {
        s.variant = FaText::tr("Z'' (1995, non-manufacturers)");
        s.formula = FaText::tr("Z'' = 6.56·X1 + 3.26·X2 + 6.72·X3 + 1.05·X4 (book equity); safe above 2.60, distress below 1.10");
        s.safe = 2.60;
        s.distress = 1.10;
        if (manufacturer && !(fa_ok(ctx.market_cap) && ctx.market_cap > 0.0))
            s.variant += QStringLiteral(" — ") +
                         (ctx.market_cap_note.isEmpty() ? FaText::tr("market value not available") : ctx.market_cap_note);
        else if (!manufacturer)
            s.variant += QStringLiteral(" — ") + FaText::tr("not a manufacturer");
    }
    static const double kZ[] = {1.2, 1.4, 3.3, 0.6, 1.0};
    static const double kZpp[] = {6.56, 3.26, 6.72, 1.05};
    double z = 0.0;
    for (int k = 0; k < s.parts.size(); ++k) {
        const Check& c = s.parts[k];
        if (!fa_ok(c.value)) {
            s.note = c.note.isEmpty() ? fa_missing(c.label) : c.note;
            return s;
        }
        z += (original ? kZ[k] : kZpp[k]) * c.value;
    }
    s.value = z;
    s.zone = z > s.safe ? Zone::Safe : z < s.distress ? Zone::Distress : Zone::Grey;
    return s;
}

Score fa_beneish(const FaYear& t, const FaYear& p, Template tmpl) {
    Score s;
    s.id = QStringLiteral("beneish");
    s.label = FaText::tr("Beneish M-score");
    s.variant = FaText::tr("8-variable model (1999)");
    s.formula = FaText::tr("M = −4.84 + 0.920·DSRI + 0.528·GMI + 0.404·AQI + 0.892·SGI + 0.115·DEPI − 0.172·SGAI + "
                           "4.679·TATA − 0.327·LVGI; above −1.78 = likely manipulator");
    s.period_label = FaText::tr("%1 vs %2").arg(t.label, p.label);
    s.safe = -2.22;
    s.distress = -1.78;
    if (tmpl != Template::Industrial) {
        s.note = FaText::tr("n/a — not designed for banks and insurers");
        return s;
    }
    const double ar_t = fa_ybal(t, "receivables"), ar_p = fa_ybal(p, "receivables");
    const double rv_t = fa_yflow(t, "revenue"), rv_p = fa_yflow(p, "revenue");
    const double gp_t = fa_yflow(t, "gross_profit"), gp_p = fa_yflow(p, "gross_profit");
    const double ca_t = fa_ybal(t, "current_assets"), ca_p = fa_ybal(p, "current_assets");
    const double ppe_t = fa_ybal(t, "ppe"), ppe_p = fa_ybal(p, "ppe");
    double sec_t = fa_ybal(t, "lt_investments"), sec_p = fa_ybal(p, "lt_investments");
    const double ta_t = fa_ybal(t, "total_assets"), ta_p = fa_ybal(p, "total_assets");
    const double dep_t = fa_yflow(t, "da"), dep_p = fa_yflow(p, "da");
    const double sga_t = fa_yflow(t, "sga"), sga_p = fa_yflow(p, "sga");
    const double cl_t = fa_ybal(t, "current_liabilities"), cl_p = fa_ybal(p, "current_liabilities");
    double ltd_t = fa_ybal(t, "long_debt"), ltd_p = fa_ybal(p, "long_debt");
    const double ni_t = fa_yflow(t, "ni_cont"), cfo_t = fa_yflow(t, "cfo");
    if (!fa_ok(sec_t))
        sec_t = 0.0;
    if (!fa_ok(sec_p))
        sec_p = 0.0;
    if (!fa_ok(ltd_t))
        ltd_t = 0.0;
    if (!fa_ok(ltd_p))
        ltd_p = 0.0;

    struct In {
        double v;
        const char* name;
    };
    const In needs[] = {{ar_t, QT_TRANSLATE_NOOP("EquityFinancials", "receivables")},
                        {ar_p, QT_TRANSLATE_NOOP("EquityFinancials", "last year's receivables")},
                        {rv_t, QT_TRANSLATE_NOOP("EquityFinancials", "revenue")},
                        {rv_p, QT_TRANSLATE_NOOP("EquityFinancials", "last year's revenue")},
                        {gp_t, QT_TRANSLATE_NOOP("EquityFinancials", "gross profit")},
                        {gp_p, QT_TRANSLATE_NOOP("EquityFinancials", "last year's gross profit")},
                        {ca_t, QT_TRANSLATE_NOOP("EquityFinancials", "current assets")},
                        {ca_p, QT_TRANSLATE_NOOP("EquityFinancials", "last year's current assets")},
                        {ppe_t, QT_TRANSLATE_NOOP("EquityFinancials", "PP&E")},
                        {ppe_p, QT_TRANSLATE_NOOP("EquityFinancials", "last year's PP&E")},
                        {ta_t, QT_TRANSLATE_NOOP("EquityFinancials", "total assets")},
                        {ta_p, QT_TRANSLATE_NOOP("EquityFinancials", "last year's total assets")},
                        {dep_t, QT_TRANSLATE_NOOP("EquityFinancials", "depreciation")},
                        {dep_p, QT_TRANSLATE_NOOP("EquityFinancials", "last year's depreciation")},
                        {sga_t, QT_TRANSLATE_NOOP("EquityFinancials", "SG&A")},
                        {sga_p, QT_TRANSLATE_NOOP("EquityFinancials", "last year's SG&A")},
                        {cl_t, QT_TRANSLATE_NOOP("EquityFinancials", "current liabilities")},
                        {cl_p, QT_TRANSLATE_NOOP("EquityFinancials", "last year's current liabilities")},
                        {ni_t, QT_TRANSLATE_NOOP("EquityFinancials", "net income")},
                        {cfo_t, QT_TRANSLATE_NOOP("EquityFinancials", "operating cash flow")}};
    for (const auto& in : needs) {
        if (!fa_ok(in.v)) {
            s.note = fa_missing(QCoreApplication::translate("EquityFinancials", in.name));
            return s;
        }
    }
    // The gross-margin and sales indices are meaningless across a negative margin or sales.
    if (rv_t <= 0.0 || rv_p <= 0.0 || gp_t <= 0.0 || gp_p <= 0.0) {
        s.note = FaText::tr("n/a — needs positive revenue and gross profit in both years");
        return s;
    }
    auto q = [](double a, double b) { return b != 0.0 ? a / b : kNaN; };
    const double dsri = q(q(ar_t, rv_t), q(ar_p, rv_p));
    const double gmi = q(q(gp_p, rv_p), q(gp_t, rv_t));
    const double aqi = q(1.0 - q(ca_t + ppe_t + sec_t, ta_t), 1.0 - q(ca_p + ppe_p + sec_p, ta_p));
    const double sgi = q(rv_t, rv_p);
    const double depi = q(q(dep_p, dep_p + ppe_p), q(dep_t, dep_t + ppe_t));
    const double sgai = q(q(sga_t, rv_t), q(sga_p, rv_p));
    const double lvgi = q(q(cl_t + ltd_t, ta_t), q(cl_p + ltd_p, ta_p));
    const double tata = q(ni_t - cfo_t, ta_t);
    struct Var {
        const char* id;
        const char* label;
        const char* formula;
        double v;
        double w;
    };
    const Var vars[] = {
        {"dsri", QT_TRANSLATE_NOOP("EquityFinancials", "DSRI receivables ÷ sales index"),
         QT_TRANSLATE_NOOP("EquityFinancials", "(Receivables ÷ revenue) this year ÷ last year"), dsri, 0.920},
        {"gmi", QT_TRANSLATE_NOOP("EquityFinancials", "GMI gross margin index"),
         QT_TRANSLATE_NOOP("EquityFinancials", "Gross margin last year ÷ this year"), gmi, 0.528},
        {"aqi", QT_TRANSLATE_NOOP("EquityFinancials", "AQI asset quality index"),
         QT_TRANSLATE_NOOP("EquityFinancials", "Share of soft assets (not current, PP&E or investments) this year ÷ last year"),
         aqi, 0.404},
        {"sgi", QT_TRANSLATE_NOOP("EquityFinancials", "SGI sales growth index"),
         QT_TRANSLATE_NOOP("EquityFinancials", "Revenue this year ÷ last year"), sgi, 0.892},
        {"depi", QT_TRANSLATE_NOOP("EquityFinancials", "DEPI depreciation index"),
         QT_TRANSLATE_NOOP("EquityFinancials", "Depreciation rate last year ÷ this year"), depi, 0.115},
        {"sgai", QT_TRANSLATE_NOOP("EquityFinancials", "SGAI SG&A index"),
         QT_TRANSLATE_NOOP("EquityFinancials", "(SG&A ÷ revenue) this year ÷ last year"), sgai, -0.172},
        {"tata", QT_TRANSLATE_NOOP("EquityFinancials", "TATA total accruals ÷ assets"),
         QT_TRANSLATE_NOOP("EquityFinancials", "(Income from continuing operations − CFO) ÷ total assets"), tata, 4.679},
        {"lvgi", QT_TRANSLATE_NOOP("EquityFinancials", "LVGI leverage index"),
         QT_TRANSLATE_NOOP("EquityFinancials", "((Current liabilities + long-term debt) ÷ assets) this year ÷ last year"),
         lvgi, -0.327},
    };
    double m = -4.84;
    for (const auto& v : vars) {
        Check c = fa_check(v.id, QCoreApplication::translate("EquityFinancials", v.label),
                           QCoreApplication::translate("EquityFinancials", v.formula));
        c.value = v.v;
        if (!fa_ok(v.v)) {
            c.note = FaText::tr("n/a — division by zero");
            s.parts << c;
            s.note = FaText::tr("n/a — %1 cannot be computed").arg(c.label);
            return s;
        }
        s.parts << c;
        m += v.w * v.v;
    }
    s.value = m;
    s.zone = m > s.distress ? Zone::Distress : m < s.safe ? Zone::Safe : Zone::Grey;
    return s;
}

void fa_scores(Analysis& a, const FinancialsData& fd, const Context& ctx) {
    const FaFrame f = fa_frame(fd, Period::Annual, a.fy_end_month);
    // Latest fiscal year with revenue and total assets, and the year before it.
    int t = -1;
    for (int i = static_cast<int>(f.periods.size()) - 1; i >= 0; --i)
        if (fa_ok(fa_flow(f.inc[i], f.cf[i], QStringLiteral("revenue"))) &&
            fa_ok(fa_bal(f.bal[i], QStringLiteral("total_assets")))) {
            t = i;
            break;
        }
    auto year = [&f](int i) {
        FaYear y;
        y.inc = f.inc[i];
        y.cf = f.cf[i];
        y.bal = f.bal[i];
        y.bal_prev = f.bal_prev[i];
        y.has_prev = f.has_prev[i];
        y.label = f.periods[i].label;
        return y;
    };
    const QString need_two = FaText::tr("n/a — two fiscal years of statements are needed");
    if (t < 0) {
        a.piotroski.id = QStringLiteral("piotroski");
        a.piotroski.label = FaText::tr("Piotroski F-score");
        a.piotroski.note = need_two;
        a.altman.id = QStringLiteral("altman");
        a.altman.label = FaText::tr("Altman Z-score");
        a.altman.note = FaText::tr("n/a — no annual balance sheet");
        a.beneish.id = QStringLiteral("beneish");
        a.beneish.label = FaText::tr("Beneish M-score");
        a.beneish.note = need_two;
        return;
    }
    const FaYear yt = year(t);
    a.altman = fa_altman(yt, a.tmpl, ctx);
    const int p = f.periods[t].year_ago;
    if (p < 0) {
        a.piotroski = fa_piotroski(yt, FaYear{}, a.financial());
        a.piotroski.value = kNaN;
        a.piotroski.zone = Zone::NotAvailable;
        a.piotroski.note = need_two;
        a.beneish.id = QStringLiteral("beneish");
        a.beneish.label = FaText::tr("Beneish M-score");
        a.beneish.note = need_two;
        return;
    }
    const FaYear yp = year(p);
    a.piotroski = fa_piotroski(yt, yp, a.financial());
    a.beneish = fa_beneish(yt, yp, a.tmpl);
}

} // namespace

// ── Public ───────────────────────────────────────────────────────────────────────────────────

bool Series::any() const {
    for (double x : v)
        if (fa_ok(x))
            return true;
    return false;
}

double Series::last(int upto, int* index) const {
    const int from = upto < 0 || upto >= v.size() ? static_cast<int>(v.size()) - 1 : upto;
    for (int i = from; i >= 0; --i)
        if (fa_ok(v[i])) {
            if (index)
                *index = i;
            return v[i];
        }
    if (index)
        *index = -1;
    return kNaN;
}

Series Series::filled(int n) {
    Series s;
    s.v = QVector<double>(n, kNaN);
    s.note = QVector<QString>(n);
    return s;
}

int Analysis::latest() const {
    const Series r = line(QStringLiteral("revenue"));
    for (int i = count() - 1; i >= 0; --i)
        if (fa_ok(r.at(i)))
            return i;
    const Series ta = line(QStringLiteral("total_assets"));
    for (int i = count() - 1; i >= 0; --i)
        if (fa_ok(ta.at(i)))
            return i;
    return count() - 1;
}

Series Analysis::line(const QString& id) const {
    const auto it = lines.constFind(id);
    return it != lines.constEnd() ? it.value() : Series::filled(count());
}

const Metric* Analysis::metric(const QString& id) const {
    for (const auto& m : metrics)
        if (m.id == id)
            return &m;
    return nullptr;
}

Series Analysis::metric_series(const QString& id) const {
    const Metric* m = metric(id);
    return m ? m->s : Series::filled(count());
}

QVector<PeriodInfo> period_infos(const QVector<Column>& cols, Period basis) {
    QVector<PeriodInfo> out;
    out.reserve(cols.size());
    for (int i = 0; i < cols.size(); ++i) {
        PeriodInfo p;
        p.end = cols[i].end;
        p.label = cols[i].label;
        p.tooltip = cols[i].tooltip;
        p.ttm = cols[i].ttm;
        if (basis == Period::Quarterly) {
            const qint64 gap = i > 0 ? cols[i - 1].end.daysTo(cols[i].end) : 0;
            p.days = gap >= 80 && gap <= 100 ? static_cast<int>(gap) : 91;
        }
        const QDate target = cols[i].end.addYears(-1);
        qint64 best = 26;
        for (int k = 0; k < i; ++k) {
            const qint64 g = qAbs(cols[k].end.daysTo(target));
            if (g < best) {
                best = g;
                p.year_ago = k;
            }
        }
        out.append(p);
    }
    return out;
}

double yoy_change(double cur, double prev, QString* why) {
    if (!fa_ok(cur) || !fa_ok(prev)) {
        if (why)
            *why = FaText::tr("n/a — not reported in both periods");
        return kNaN;
    }
    if (prev == 0.0) {
        if (why)
            *why = FaText::tr("n/a — zero a year earlier");
        return kNaN;
    }
    if ((cur < 0.0) != (prev < 0.0)) {
        if (why)
            *why = cur < 0.0 ? FaText::tr("n/a — turned negative") : FaText::tr("n/a — turned positive");
        return kNaN;
    }
    return (std::fabs(cur) - std::fabs(prev)) / std::fabs(prev);
}

double cagr(const QVector<double>& v, const QVector<PeriodInfo>& periods, int last, QString* why) {
    int hi = -1;
    for (int i = (last < 0 || last >= v.size()) ? static_cast<int>(v.size()) - 1 : last; i >= 0; --i)
        if (fa_ok(v[i])) {
            hi = i;
            break;
        }
    int lo = -1;
    for (int i = 0; i < hi; ++i)
        if (fa_ok(v[i]) && v[i] > 0.0) {
            lo = i;
            break;
        }
    if (hi < 0 || lo < 0 || hi >= periods.size()) {
        if (why)
            *why = FaText::tr("n/a — needs two readings");
        return kNaN;
    }
    const double years = periods[lo].end.daysTo(periods[hi].end) / 365.25;
    if (years < 1.9) {
        if (why)
            *why = FaText::tr("n/a — less than two years of history");
        return kNaN;
    }
    if (v[lo] <= 0.0 || v[hi] <= 0.0) {
        if (why)
            *why = FaText::tr("n/a — a negative or zero end point");
        return kNaN;
    }
    return std::pow(v[hi] / v[lo], 1.0 / years) - 1.0;
}

QString zone_text(Zone zone) {
    switch (zone) {
        case Zone::Safe:
            return FaText::tr("Safe");
        case Zone::Grey:
            return FaText::tr("Grey zone");
        case Zone::Distress:
            return FaText::tr("Distress");
        case Zone::NotAvailable:
        default:
            return FaText::tr("n/a");
    }
}

Analysis analyse(const FinancialsData& fd, Period basis, const Context& ctx) {
    Analysis a;
    a.basis = basis;
    a.tmpl = fin_model::detect_template(fd, ctx.sector, ctx.industry);
    a.fy_end_month = fin_model::fiscal_year_end_month(fd);
    const FaFrame f = fa_frame(fd, basis, a.fy_end_month);
    a.periods = f.periods;
    const int n = static_cast<int>(f.periods.size());

    // Canonical lines.
    for (const char* id : kFaFlowIds) {
        Series s = Series::filled(n);
        for (int i = 0; i < n; ++i)
            s.v[i] = fa_flow(f.inc[i], f.cf[i], QLatin1String(id));
        a.lines.insert(QLatin1String(id), s);
    }
    for (const char* id : kFaBalanceIds) {
        Series s = Series::filled(n);
        for (int i = 0; i < n; ++i)
            s.v[i] = fa_bal(f.bal[i], QLatin1String(id));
        a.lines.insert(QLatin1String(id), s);
    }
    // Beginning cash of a summed TTM column = ending cash − the year's change when the
    // statement carries no usable opening position.
    {
        Series& bc = a.lines[QStringLiteral("begin_cash")];
        const Series ec = a.lines.value(QStringLiteral("end_cash"));
        const Series ch = a.lines.value(QStringLiteral("change_in_cash"));
        for (int i = 0; i < n; ++i)
            if (!fa_ok(bc.v[i]) && fa_ok(ec.at(i)) && fa_ok(ch.at(i)))
                bc.v[i] = ec.at(i) - ch.at(i);
    }

    FaBuild b{a, f};
    fa_metrics(b);
    fa_structure(b);
    fa_scores(a, fd, ctx);
    if (basis == Period::Quarterly && n == 0)
        a.notes << FaText::tr("No quarterly statements — the company reports half-yearly, or the source has none.");
    return a;
}

QVector<Step> income_waterfall(const Analysis& a, int p) {
    QVector<Step> out;
    if (p < 0 || p >= a.count())
        return out;
    auto L = [&a, p](const char* id) { return a.line(QLatin1String(id)).at(p); };
    auto step = [&out](const char* id, const QString& label, double v, Step::Kind k) {
        Step s;
        s.id = QLatin1String(id);
        s.label = label;
        s.value = v;
        s.kind = k;
        out.append(s);
    };
    const double rev = L("revenue"), ni = L("net_income");
    if (!fa_ok(rev) || !fa_ok(ni))
        return out;
    const double pretax = L("pretax"), tax = L("tax");
    const double tiny = std::fabs(rev) * 0.0005;
    double level = rev;

    if (a.tmpl == Template::Bank) {
        const double ii = L("interest_income"), ie = L("interest_expense"), nii = L("nii");
        if (fa_ok(ii) && fa_ok(ie) && fa_ok(nii)) {
            step("interest_income", FaText::tr("Interest income"), ii, Step::Kind::Start);
            step("interest_expense", FaText::tr("Interest expense"), -ie, Step::Kind::Delta);
            step("nii", FaText::tr("Net interest income"), nii, Step::Kind::Subtotal);
            step("non_interest_income", FaText::tr("Non-interest income"), rev - nii, Step::Kind::Delta);
            step("revenue", FaText::tr("Total revenue"), rev, Step::Kind::Subtotal);
        } else {
            step("revenue", FaText::tr("Total revenue"), rev, Step::Kind::Start);
        }
        const double nie = L("non_interest_expense"), prov = L("provisions");
        if (fa_ok(nie)) {
            step("non_interest_expense", FaText::tr("Non-interest expense"), -nie, Step::Kind::Delta);
            level -= nie;
        }
        if (fa_ok(prov) && prov != 0.0) {
            step("provisions", FaText::tr("Credit-loss provisions"), -prov, Step::Kind::Delta);
            level -= prov;
        }
        if (fa_ok(pretax)) {
            const double other = pretax - level;
            if (std::fabs(other) > tiny)
                step("other", fa_ok(prov) ? FaText::tr("Other items") : FaText::tr("Provisions & other"), other,
                     Step::Kind::Delta);
            step("pretax", FaText::tr("Pretax income"), pretax, Step::Kind::Subtotal);
            level = pretax;
        }
    } else if (a.tmpl == Template::Insurer) {
        step("revenue", FaText::tr("Total revenue"), rev, Step::Kind::Start);
        const double te = L("total_expenses");
        if (fa_ok(te)) {
            step("total_expenses", FaText::tr("Benefits, claims & expenses"), -te, Step::Kind::Delta);
            level -= te;
        }
        if (fa_ok(pretax)) {
            const double other = pretax - level;
            if (std::fabs(other) > tiny)
                step("other", FaText::tr("Other items"), other, Step::Kind::Delta);
            step("pretax", FaText::tr("Pretax income"), pretax, Step::Kind::Subtotal);
            level = pretax;
        }
    } else {
        step("revenue", FaText::tr("Revenue"), rev, Step::Kind::Start);
        const double cogs = L("cogs"), gp = L("gross_profit");
        if (fa_ok(cogs) || fa_ok(gp)) {
            const double g = fa_ok(gp) ? gp : rev - cogs;
            step("cogs", FaText::tr("Cost of revenue"), g - rev, Step::Kind::Delta);
            step("gross_profit", FaText::tr("Gross profit"), g, Step::Kind::Subtotal);
            level = g;
        }
        const double op = L("operating_income");
        if (fa_ok(op)) {
            const double rnd = L("rnd"), sga = L("sga");
            double opex = level - op;
            if (fa_ok(rnd) && rnd != 0.0) {
                step("rnd", FaText::tr("Research & development"), -rnd, Step::Kind::Delta);
                opex -= rnd;
            }
            if (fa_ok(sga) && sga != 0.0) {
                step("sga", FaText::tr("SG&A"), -sga, Step::Kind::Delta);
                opex -= sga;
            }
            if (std::fabs(opex) > tiny)
                step("other_opex", (fa_ok(rnd) || fa_ok(sga)) ? FaText::tr("Other operating expenses")
                                                                : FaText::tr("Operating expenses"),
                     -opex, Step::Kind::Delta);
            step("operating_income", FaText::tr("Operating income"), op, Step::Kind::Subtotal);
            level = op;
        }
        if (fa_ok(pretax)) {
            double nonop = pretax - level;
            const double ie = L("interest_expense");
            if (fa_ok(ie) && ie != 0.0) {
                step("interest_expense", FaText::tr("Interest expense"), -ie, Step::Kind::Delta);
                nonop += ie;
            }
            if (std::fabs(nonop) > tiny)
                step("non_operating", FaText::tr("Other non-operating items"), nonop, Step::Kind::Delta);
            step("pretax", FaText::tr("Pretax income"), pretax, Step::Kind::Subtotal);
            level = pretax;
        }
    }
    if (fa_ok(tax) && fa_ok(pretax)) {
        step("tax", FaText::tr("Income tax"), -tax, Step::Kind::Delta);
        level = pretax - tax;
    }
    const double rest = ni - level;
    if (std::fabs(rest) > tiny)
        step("other_below",
             fa_ok(pretax) ? FaText::tr("Minorities, preferred & other") : FaText::tr("Costs & other items"), rest,
             Step::Kind::Delta);
    step("net_income", FaText::tr("Net income"), ni, Step::Kind::End);
    return out;
}

QVector<Step> cash_bridge(const Analysis& a, int p) {
    QVector<Step> out;
    if (p < 0 || p >= a.count())
        return out;
    auto L = [&a, p](const char* id) { return a.line(QLatin1String(id)).at(p); };
    const double begin = L("begin_cash"), end = L("end_cash"), cfo = L("cfo");
    if (!fa_ok(begin) || !fa_ok(end) || !fa_ok(cfo))
        return out;
    auto step = [&out](const char* id, const QString& label, double v, Step::Kind k) {
        Step s;
        s.id = QLatin1String(id);
        s.label = label;
        s.value = v;
        s.kind = k;
        out.append(s);
    };
    step("begin_cash", FaText::tr("Opening cash"), begin, Step::Kind::Start);
    double sum = 0.0;
    auto delta = [&](const char* id, const QString& label) {
        const double v = L(id);
        if (fa_ok(v) && v != 0.0) {
            step(id, label, v, Step::Kind::Delta);
            sum += v;
        }
    };
    delta("cfo", FaText::tr("Operating cash flow"));
    delta("capex", FaText::tr("Capital expenditure"));
    delta("acquisitions", FaText::tr("Acquisitions"));
    delta("dividends", FaText::tr("Dividends"));
    delta("buybacks", FaText::tr("Buybacks"));
    delta("net_debt_flow", FaText::tr("Debt issued − repaid"));
    const double other = end - begin - sum;
    if (std::fabs(other) > std::max(std::fabs(end), std::fabs(cfo)) * 0.0005)
        step("other", FaText::tr("Investments, other & FX"), other, Step::Kind::Delta);
    step("end_cash", FaText::tr("Closing cash"), end, Step::Kind::End);
    return out;
}

QVector<Kpi> kpis(const Analysis& a, int p) {
    QVector<Kpi> out;
    if (p < 0 || p >= a.count())
        return out;
    struct Def {
        const char* id;
        const char* label;
        bool line; ///< a canonical line (else a metric)
        Unit unit;
        bool lower_better;
        const char* tip;
    };
    static const Def kIndustrial[] = {
        {"revenue", QT_TRANSLATE_NOOP("EquityFinancials", "REVENUE"), true, Unit::Money, false,
         QT_TRANSLATE_NOOP("EquityFinancials", "Total revenue")},
        {"gross_margin", QT_TRANSLATE_NOOP("EquityFinancials", "GROSS MARGIN"), false, Unit::Percent, false, nullptr},
        {"operating_margin", QT_TRANSLATE_NOOP("EquityFinancials", "OPERATING MARGIN"), false, Unit::Percent, false, nullptr},
        {"net_margin", QT_TRANSLATE_NOOP("EquityFinancials", "NET MARGIN"), false, Unit::Percent, false, nullptr},
        {"eps_diluted", QT_TRANSLATE_NOOP("EquityFinancials", "EPS (DILUTED)"), true, Unit::PerShare, false,
         QT_TRANSLATE_NOOP("EquityFinancials", "Diluted earnings per share, reporting currency")},
        {"fcf", QT_TRANSLATE_NOOP("EquityFinancials", "FREE CASH FLOW"), true, Unit::Money, false,
         QT_TRANSLATE_NOOP("EquityFinancials", "Operating cash flow − capital expenditure")},
        {"fcf_margin", QT_TRANSLATE_NOOP("EquityFinancials", "FCF MARGIN"), false, Unit::Percent, false, nullptr},
        {"roe", QT_TRANSLATE_NOOP("EquityFinancials", "ROE"), false, Unit::Percent, false, nullptr},
        {"roic", QT_TRANSLATE_NOOP("EquityFinancials", "ROIC"), false, Unit::Percent, false, nullptr},
        {"net_debt_ebitda", QT_TRANSLATE_NOOP("EquityFinancials", "NET DEBT / EBITDA"), false, Unit::Multiple, true, nullptr},
        {"interest_cover", QT_TRANSLATE_NOOP("EquityFinancials", "INTEREST COVER"), false, Unit::Multiple, false, nullptr},
        {"cfo_to_ni", QT_TRANSLATE_NOOP("EquityFinancials", "CASH CONVERSION"), false, Unit::Multiple, false, nullptr},
    };
    static const Def kFinancial[] = {
        {"revenue", QT_TRANSLATE_NOOP("EquityFinancials", "REVENUE"), true, Unit::Money, false,
         QT_TRANSLATE_NOOP("EquityFinancials", "Total (net) revenue")},
        {"nii", QT_TRANSLATE_NOOP("EquityFinancials", "NET INTEREST INCOME"), true, Unit::Money, false,
         QT_TRANSLATE_NOOP("EquityFinancials", "Interest income − interest expense")},
        {"nii_share", QT_TRANSLATE_NOOP("EquityFinancials", "NII SHARE"), false, Unit::Percent, false, nullptr},
        {"efficiency_ratio", QT_TRANSLATE_NOOP("EquityFinancials", "EFFICIENCY RATIO"), false, Unit::Percent, true, nullptr},
        {"net_margin", QT_TRANSLATE_NOOP("EquityFinancials", "NET MARGIN"), false, Unit::Percent, false, nullptr},
        {"eps_diluted", QT_TRANSLATE_NOOP("EquityFinancials", "EPS (DILUTED)"), true, Unit::PerShare, false,
         QT_TRANSLATE_NOOP("EquityFinancials", "Diluted earnings per share, reporting currency")},
        {"roe", QT_TRANSLATE_NOOP("EquityFinancials", "ROE"), false, Unit::Percent, false, nullptr},
        {"rote", QT_TRANSLATE_NOOP("EquityFinancials", "ROTE"), false, Unit::Percent, false, nullptr},
        {"roa", QT_TRANSLATE_NOOP("EquityFinancials", "ROA"), false, Unit::Percent, false, nullptr},
        {"equity_to_assets", QT_TRANSLATE_NOOP("EquityFinancials", "EQUITY / ASSETS"), false, Unit::Percent, false, nullptr},
        {"bvps", QT_TRANSLATE_NOOP("EquityFinancials", "BOOK VALUE / SHARE"), false, Unit::PerShare, false, nullptr},
        {"total_payout_ni", QT_TRANSLATE_NOOP("EquityFinancials", "PAYOUT (DIV + BUYBACK)"), false, Unit::Percent, false, nullptr},
    };
    const bool fin = a.financial();
    const Def* defs = fin ? kFinancial : kIndustrial;
    const int count = fin ? static_cast<int>(std::size(kFinancial)) : static_cast<int>(std::size(kIndustrial));
    // The insurer set swaps the bank-only tiles for expense ratio / ROA.
    for (int d = 0; d < count; ++d) {
        Def def = defs[d];
        if (a.tmpl == Template::Insurer && (QLatin1String(def.id) == QLatin1String("nii") ||
                                            QLatin1String(def.id) == QLatin1String("nii_share") ||
                                            QLatin1String(def.id) == QLatin1String("efficiency_ratio"))) {
            if (QLatin1String(def.id) == QLatin1String("nii"))
                def = {"total_expenses", QT_TRANSLATE_NOOP("EquityFinancials", "TOTAL EXPENSES"), true, Unit::Money, true,
                       QT_TRANSLATE_NOOP("EquityFinancials", "Benefits, claims and operating expenses")};
            else if (QLatin1String(def.id) == QLatin1String("nii_share"))
                def = {"expense_ratio", QT_TRANSLATE_NOOP("EquityFinancials", "EXPENSE RATIO"), false, Unit::Percent, true, nullptr};
            else
                def = {"pretax_margin", QT_TRANSLATE_NOOP("EquityFinancials", "PRETAX MARGIN"), false, Unit::Percent, false, nullptr};
        }
        Kpi k;
        k.id = QLatin1String(def.id);
        k.label = QCoreApplication::translate("EquityFinancials", def.label);
        k.unit = def.unit;
        k.lower_is_better = def.lower_better;
        Series s;
        if (def.line) {
            s = a.line(k.id);
            k.tooltip = QCoreApplication::translate("EquityFinancials", def.tip);
        } else {
            const Metric* m = a.metric(k.id);
            s = m ? m->s : Series::filled(a.count());
            k.tooltip = m ? m->formula : QString();
            if (m && m->averaged)
                k.tooltip += QStringLiteral(" · ") + FaText::tr("average balances");
        }
        k.value = s.at(p);
        if (!fa_ok(k.value)) {
            k.note = s.note_at(p).isEmpty() ? fa_missing(k.label.toLower()) : s.note_at(p);
        } else if (!s.note_at(p).isEmpty()) {
            k.note = s.note_at(p);
        }
        const int ya = a.periods[p].year_ago;
        k.prior = s.at(ya);
        if (fa_ok(k.value) && fa_ok(k.prior)) {
            if (k.unit == Unit::Money || k.unit == Unit::PerShare) {
                QString why;
                k.change = fa_growth(k.value, k.prior, why);
                if (!fa_ok(k.change) && k.note.isEmpty())
                    k.note = why;
            } else {
                k.change = k.value - k.prior;
            }
        }
        for (int i = std::max(0, p - 4); i <= p; ++i)
            k.spark.append(s.at(i));
        out.append(k);
    }
    return out;
}

} // namespace fincept::screens::fin_an
