// src/services/equity/EquityValuationModels.cpp
#include "services/equity/EquityValuationModels.h"

#include <QCoreApplication>
#include <QDate>
#include <QJsonValue>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace fincept::services::equity::valuation {

namespace valuation_detail {

QString vtr(const char* text) {
    return QCoreApplication::translate("EquityValuationModels", text);
}

bool has(double v) {
    return std::isfinite(v);
}

/// First alias the statement reports. A line that IS present and reads 0 is data; a
/// missing line is NaN.
double line(const QJsonObject& o, std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        const QJsonValue v = o.value(QLatin1String(k));
        if (v.isDouble())
            return v.toDouble();
        if (v.isString()) {
            bool ok = false;
            const double d = v.toString().toDouble(&ok);
            if (ok)
                return d;
        }
    }
    return std::nan("");
}

/// Index of the statement column whose period end is within 10 days of `date` (exact first).
qsizetype match_period(const QVector<QPair<QString, QJsonObject>>& stmt, const QDate& date) {
    qsizetype best = -1;
    qint64 best_gap = 11;
    for (qsizetype i = 0; i < stmt.size(); ++i) {
        const QDate d = QDate::fromString(stmt[i].first.left(10), Qt::ISODate);
        if (!d.isValid())
            continue;
        const qint64 gap = qAbs(d.daysTo(date));
        if (gap < best_gap) {
            best_gap = gap;
            best = i;
        }
    }
    return best;
}

double ratio(double num, double den) {
    return (has(num) && has(den) && den != 0.0) ? num / den : std::nan("");
}

} // namespace valuation_detail

using namespace valuation_detail;

QVector<FiscalYear> fiscal_years(const FinancialsData& fd) {
    QVector<FiscalYear> out;
    for (const auto& inc_col : fd.income_statement) {
        const QDate date = QDate::fromString(inc_col.first.left(10), Qt::ISODate);
        if (!date.isValid())
            continue;
        const QJsonObject& inc = inc_col.second;
        const qsizetype bi = match_period(fd.balance_sheet, date);
        const qsizetype ci = match_period(fd.cash_flow, date);
        const QJsonObject bal = bi >= 0 ? fd.balance_sheet[bi].second : QJsonObject();
        const QJsonObject cf = ci >= 0 ? fd.cash_flow[ci].second : QJsonObject();

        FiscalYear y;
        y.period = date.toString(Qt::ISODate);
        y.revenue = line(inc, {"Total Revenue", "Operating Revenue", "Revenue"});
        y.gross_profit = line(inc, {"Gross Profit"});
        y.ebit = line(inc, {"EBIT", "Operating Income"});
        y.net_income = line(inc, {"Net Income", "Net Income Common Stockholders",
                                  "Net Income From Continuing Operation Net Minority Interest"});
        y.sga = line(inc, {"Selling General And Administration"});
        y.depreciation = line(cf, {"Depreciation And Amortization", "Depreciation Amortization Depletion"});
        if (!has(y.depreciation))
            y.depreciation = line(inc, {"Reconciled Depreciation"});
        y.operating_cash_flow = line(cf, {"Operating Cash Flow", "Cash Flow From Continuing Operating Activities"});
        y.total_assets = line(bal, {"Total Assets"});
        y.total_liabilities = line(bal, {"Total Liabilities Net Minority Interest", "Total Liabilities"});
        y.current_assets = line(bal, {"Current Assets"});
        y.current_liabilities = line(bal, {"Current Liabilities"});
        y.retained_earnings = line(bal, {"Retained Earnings"});
        y.long_term_debt = line(bal, {"Long Term Debt", "Long Term Debt And Capital Lease Obligation"});
        y.receivables = line(bal, {"Accounts Receivable", "Receivables"});
        y.net_ppe = line(bal, {"Net PPE"});
        y.shares = line(bal, {"Ordinary Shares Number", "Share Issued"});
        out.append(y);
    }
    std::sort(out.begin(), out.end(), [](const FiscalYear& a, const FiscalYear& b) { return a.period > b.period; });
    return out;
}

DcfResult dcf(const DcfInputs& in) {
    DcfResult r;
    if (!(in.discount > in.terminal_growth)) {
        r.error = vtr("The discount rate must exceed terminal growth.");
        return r;
    }
    if (in.years < 1 || in.years > 50) {
        r.error = vtr("Projection years must be between 1 and 50.");
        return r;
    }
    if (!(in.shares > 0.0) || !has(in.shares)) {
        r.error = vtr("Shares outstanding are missing.");
        return r;
    }
    if (!has(in.fcf) || in.fcf <= 0.0) {
        r.error = vtr("A DCF needs positive free cash flow — this company's base-year FCF is zero or negative.");
        return r;
    }
    double fcf_t = in.fcf;
    double pv = 0.0;
    for (int t = 1; t <= in.years; ++t) {
        fcf_t *= 1.0 + in.growth;
        pv += fcf_t / std::pow(1.0 + in.discount, t);
    }
    const double terminal = fcf_t * (1.0 + in.terminal_growth) / (in.discount - in.terminal_growth);
    const double pv_terminal = terminal / std::pow(1.0 + in.discount, in.years);
    r.pv_fcf = pv;
    r.pv_terminal = pv_terminal;
    r.equity_value = pv + pv_terminal;
    r.per_share = r.equity_value / in.shares;
    r.terminal_fraction = r.equity_value != 0.0 ? pv_terminal / r.equity_value : 0.0;
    r.ok = has(r.per_share);
    if (!r.ok)
        r.error = vtr("The DCF did not produce a finite value.");
    return r;
}

AltmanResult altman_z(const FiscalYear& fy, double market_cap) {
    AltmanResult r;
    if (!has(fy.total_assets) || fy.total_assets <= 0.0) {
        r.missing = vtr("total assets");
        return r;
    }
    if (!has(fy.total_liabilities) || fy.total_liabilities <= 0.0) {
        r.missing = vtr("total liabilities");
        return r;
    }
    if (!has(fy.current_assets) || !has(fy.current_liabilities)) {
        r.missing = vtr("current assets / liabilities");
        return r;
    }
    if (!has(fy.retained_earnings)) {
        r.missing = vtr("retained earnings");
        return r;
    }
    if (!has(fy.ebit)) {
        r.missing = vtr("EBIT");
        return r;
    }
    if (!has(fy.revenue)) {
        r.missing = vtr("revenue");
        return r;
    }
    if (!has(market_cap) || market_cap <= 0.0) {
        r.missing = vtr("market capitalisation");
        return r;
    }
    r.x1 = (fy.current_assets - fy.current_liabilities) / fy.total_assets;
    r.x2 = fy.retained_earnings / fy.total_assets;
    r.x3 = fy.ebit / fy.total_assets;
    r.x4 = market_cap / fy.total_liabilities;
    r.x5 = fy.revenue / fy.total_assets;
    r.z = 1.2 * r.x1 + 1.4 * r.x2 + 3.3 * r.x3 + 0.6 * r.x4 + 1.0 * r.x5;
    r.ok = has(r.z);
    return r;
}

PiotroskiResult piotroski_f(const FiscalYear& c, const FiscalYear& p) {
    PiotroskiResult r;
    auto add = [&r](const char* name, int outcome) {
        r.tests.append({vtr(name), outcome});
        if (outcome >= 0)
            ++r.tests_available;
        if (outcome == 1)
            ++r.score;
    };
    auto test = [](bool computable, bool pass) { return computable ? (pass ? 1 : 0) : -1; };

    const double roa_c = ratio(c.net_income, c.total_assets);
    const double roa_p = ratio(p.net_income, p.total_assets);
    add(QT_TRANSLATE_NOOP("EquityValuationModels", "Net income positive"), test(has(c.net_income), c.net_income > 0.0));
    add(QT_TRANSLATE_NOOP("EquityValuationModels", "Operating cash flow positive"),
        test(has(c.operating_cash_flow), c.operating_cash_flow > 0.0));
    add(QT_TRANSLATE_NOOP("EquityValuationModels", "Return on assets improved"),
        test(has(roa_c) && has(roa_p), roa_c > roa_p));
    add(QT_TRANSLATE_NOOP("EquityValuationModels", "Cash flow exceeds net income"),
        test(has(c.operating_cash_flow) && has(c.net_income), c.operating_cash_flow > c.net_income));

    // Long-term debt / assets fell. A line reported in only one year means the debt was
    // raised or retired (0 on the other side); absent in both years = nothing to judge.
    const bool ltd_any = has(c.long_term_debt) || has(p.long_term_debt);
    const double lev_c = ratio(has(c.long_term_debt) ? c.long_term_debt : 0.0, c.total_assets);
    const double lev_p = ratio(has(p.long_term_debt) ? p.long_term_debt : 0.0, p.total_assets);
    add(QT_TRANSLATE_NOOP("EquityValuationModels", "Leverage (LT debt / assets) fell"),
        test(ltd_any && has(lev_c) && has(lev_p), lev_c < lev_p || (lev_c == 0.0 && lev_p == 0.0)));

    const double cr_c = ratio(c.current_assets, c.current_liabilities);
    const double cr_p = ratio(p.current_assets, p.current_liabilities);
    add(QT_TRANSLATE_NOOP("EquityValuationModels", "Current ratio improved"), test(has(cr_c) && has(cr_p), cr_c > cr_p));
    add(QT_TRANSLATE_NOOP("EquityValuationModels", "No new shares issued"),
        test(has(c.shares) && has(p.shares), c.shares <= p.shares));
    const double gm_c = ratio(c.gross_profit, c.revenue);
    const double gm_p = ratio(p.gross_profit, p.revenue);
    add(QT_TRANSLATE_NOOP("EquityValuationModels", "Gross margin improved"), test(has(gm_c) && has(gm_p), gm_c > gm_p));
    const double at_c = ratio(c.revenue, c.total_assets);
    const double at_p = ratio(p.revenue, p.total_assets);
    add(QT_TRANSLATE_NOOP("EquityValuationModels", "Asset turnover improved"), test(has(at_c) && has(at_p), at_c > at_p));

    // Seven of nine is the least that still separates strong (8–9) from weak (0–4) honestly.
    r.ok = r.tests_available >= 7;
    if (!r.ok)
        r.missing = vtr("only %1 of 9 tests have the statement lines they need").arg(r.tests_available);
    return r;
}

BeneishResult beneish_m(const FiscalYear& c, const FiscalYear& p) {
    BeneishResult r;
    if (!has(c.revenue) || !has(p.revenue) || p.revenue <= 0.0 || c.revenue <= 0.0) {
        r.missing = vtr("revenue for both years");
        return r;
    }
    if (!has(c.total_assets) || !has(p.total_assets) || c.total_assets <= 0.0 || p.total_assets <= 0.0) {
        r.missing = vtr("total assets for both years");
        return r;
    }
    if (!has(c.net_income) || !has(c.operating_cash_flow)) {
        r.missing = vtr("net income and operating cash flow");
        return r;
    }
    // An index whose inputs are not reported is set to the neutral 1.0 and counted.
    auto neutral_if_missing = [&r](double v) {
        if (has(v) && v > 0.0)
            return v;
        ++r.imputed;
        return 1.0;
    };

    const double dsri =
        neutral_if_missing(ratio(ratio(c.receivables, c.revenue), ratio(p.receivables, p.revenue)));
    const double gmi = neutral_if_missing(ratio(ratio(p.gross_profit, p.revenue), ratio(c.gross_profit, c.revenue)));
    const double aq_c = 1.0 - ratio(c.current_assets + c.net_ppe, c.total_assets);
    const double aq_p = 1.0 - ratio(p.current_assets + p.net_ppe, p.total_assets);
    const double aqi = neutral_if_missing(ratio(aq_c, aq_p));
    const double sgi = c.revenue / p.revenue;
    const double dep_c = ratio(c.depreciation, c.depreciation + c.net_ppe);
    const double dep_p = ratio(p.depreciation, p.depreciation + p.net_ppe);
    const double depi = neutral_if_missing(ratio(dep_p, dep_c));
    const double sgai = neutral_if_missing(ratio(ratio(c.sga, c.revenue), ratio(p.sga, p.revenue)));
    const double ltd_c = has(c.long_term_debt) ? c.long_term_debt : 0.0;
    const double ltd_p = has(p.long_term_debt) ? p.long_term_debt : 0.0;
    const double lvgi = neutral_if_missing(ratio(ratio(c.current_liabilities + ltd_c, c.total_assets),
                                    ratio(p.current_liabilities + ltd_p, p.total_assets)));
    const double tata = (c.net_income - c.operating_cash_flow) / c.total_assets;

    r.m = -4.84 + 0.920 * dsri + 0.528 * gmi + 0.404 * aqi + 0.892 * sgi + 0.115 * depi - 0.172 * sgai +
          4.679 * tata - 0.327 * lvgi;
    r.components = {{QStringLiteral("DSRI"), dsri}, {QStringLiteral("GMI"), gmi},   {QStringLiteral("AQI"), aqi},
                    {QStringLiteral("SGI"), sgi},   {QStringLiteral("DEPI"), depi}, {QStringLiteral("SGAI"), sgai},
                    {QStringLiteral("TATA"), tata}, {QStringLiteral("LVGI"), lvgi}};
    // More than two neutral stand-ins and the score says more about the gaps than the books.
    r.ok = has(r.m) && r.imputed <= 2;
    if (!r.ok)
        r.missing = vtr("%1 of 8 indices lack statement lines").arg(r.imputed);
    return r;
}

} // namespace fincept::services::equity::valuation
