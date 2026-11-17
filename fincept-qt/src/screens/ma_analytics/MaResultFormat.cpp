// MaResultFormat.cpp — how M&A results read: units by field name, headline KPIs per analysis, money text.
// Moved out of MAModulePanel.cpp so the result view, COPY / CSV and Add to Report share one set of rules.
#include "screens/ma_analytics/MaResultFormat.h"

#include "ui/formatting/NumberFormat.h"

#include <QHash>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

#include <cmath>
#include <initializer_list>
#include <limits>

namespace fincept::screens::ma_fmt {

namespace {

using MaUnit = fincept::ui::result::Unit;

QStringList ma_tokens(const QString& key) {
    static const QRegularExpression kSplit(QStringLiteral("[^a-z0-9]+"));
    return key.toLower().split(kSplit, Qt::SkipEmptyParts);
}

bool ma_has(const QStringList& tokens, std::initializer_list<const char*> words) {
    for (const char* w : words)
        if (tokens.contains(QLatin1String(w)))
            return true;
    return false;
}

// Fields whose unit differs from the conventions in ma_unit(), per context:
// an exact dotted path, a plain key, or a path prefix ("a.b" covers "a.b.*").
const QHash<QString, QHash<QString, MaUnit>>& ma_unit_exceptions() {
    static const QHash<QString, MaUnit> kLbo = {
        {QStringLiteral("total_debt_to_capital"), MaUnit::Percent},
        {QStringLiteral("equity_to_capital"), MaUnit::Percent},
        {QStringLiteral("debt_to_ebitda"), MaUnit::Multiple},
        {QStringLiteral("senior_debt_to_ebitda"), MaUnit::Multiple},
    };
    static const QHash<QString, QHash<QString, MaUnit>> m = {
        {QStringLiteral("accretion_dilution"),
         {{QStringLiteral("purchase_pe"), MaUnit::Multiple},
          {QStringLiteral("acquirer_pe"), MaUnit::Multiple},
          {QStringLiteral("eps_bridge.value"), MaUnit::Money},
          {QStringLiteral("new_shares_issued"), MaUnit::Plain},
          {QStringLiteral("pro_forma_shares"), MaUnit::Plain}}},
        {QStringLiteral("football_field"),
         {{QStringLiteral("low"), MaUnit::Money},
          {QStringLiteral("high"), MaUnit::Money},
          {QStringLiteral("mid"), MaUnit::Money},
          {QStringLiteral("current_price"), MaUnit::Money},
          {QStringLiteral("offer_price"), MaUnit::Money}}},
        {QStringLiteral("dcf"),
         {{QStringLiteral("wacc.wacc"), MaUnit::Fraction},
          {QStringLiteral("cost_of_equity_pretax"), MaUnit::Fraction},
          {QStringLiteral("base_year_fcf.capex_breakdown"), MaUnit::Money}}},
        {QStringLiteral("lbo_model"), kLbo},
        {QStringLiteral("lbo_sensitivity"), kLbo},
        {QStringLiteral("precedent_txns"), {{QStringLiteral("target_valuation.valuations"), MaUnit::Money}}},
        {QStringLiteral("pro_forma"),
         {{QStringLiteral("revenue_growth"), MaUnit::Fraction},
          {QStringLiteral("sg_a"), MaUnit::Money},
          {QStringLiteral("r_d"), MaUnit::Money},
          {QStringLiteral("standalone_vs_proforma.standalone_combined"), MaUnit::Money},
          {QStringLiteral("standalone_vs_proforma.pro_forma"), MaUnit::Money},
          {QStringLiteral("standalone_vs_proforma.difference"), MaUnit::Money}}},
        {QStringLiteral("scorecard"), {{QStringLiteral("weight"), MaUnit::Fraction}}},
        {QStringLiteral("vc_method"), {{QStringLiteral("exit_year_metric"), MaUnit::Money}}},
        {QStringLiteral("fairness_opinion"),
         {{QStringLiteral("position_in_range"), MaUnit::Percent},
          {QStringLiteral("contribution_to_weighted_mid"), MaUnit::Money},
          {QStringLiteral("methods_used.low"), MaUnit::Money},
          {QStringLiteral("methods_used.high"), MaUnit::Money},
          {QStringLiteral("methods_used.midpoint"), MaUnit::Money}}},
        {QStringLiteral("tech_metrics"), {{QStringLiteral("take_rate"), MaUnit::Fraction}}},
        {QStringLiteral("finserv_metrics"), {{QStringLiteral("advisory_fee_rate"), MaUnit::Fraction}}},
        {QStringLiteral("monte_carlo"),
         {{QStringLiteral("mean"), MaUnit::Money},
          {QStringLiteral("median"), MaUnit::Money},
          {QStringLiteral("std"), MaUnit::Money},
          {QStringLiteral("p5"), MaUnit::Money},
          {QStringLiteral("p25"), MaUnit::Money},
          {QStringLiteral("p75"), MaUnit::Money},
          {QStringLiteral("p95"), MaUnit::Money}}},
        // The deal comparator works in $M as entered ("68700"): plain numbers, no symbol.
        {QStringLiteral("compare_deals"),
         {{QStringLiteral("deal_value"), MaUnit::Plain}, {QStringLiteral("avg_deal_value"), MaUnit::Plain}}},
        {QStringLiteral("rank_deals"), {{QStringLiteral("deal_value"), MaUnit::Plain}}},
        {QStringLiteral("industry_deals"),
         {{QStringLiteral("total_value"), MaUnit::Plain}, {QStringLiteral("avg_deal_value"), MaUnit::Plain}}},
        {QStringLiteral("payment_structures"),
         {{QStringLiteral("avg_value"), MaUnit::Plain}, {QStringLiteral("distribution"), MaUnit::Integer}}},
    };
    return m;
}

// Unit of an M&A result field, decided by its name (never by its magnitude
// alone): *_pct / rates / margins / premiums arrive in percent units, amounts
// in dollars, counts and years as whole numbers, multiples as "x".
MaUnit ma_unit(const QString& context, const QString& path, const QString& key, double v, int depth = 0) {
    const auto ex = ma_unit_exceptions().value(context);
    if (ex.contains(path))
        return ex.value(path);
    if (ex.contains(key))
        return ex.value(key);
    for (auto it = ex.constBegin(); it != ex.constEnd(); ++it)
        if (path.startsWith(it.key() + QLatin1Char('.')))
            return it.value();

    const QString k = key.toLower();
    const QStringList t = ma_tokens(key);
    const bool integral = std::isfinite(v) && std::floor(v) == v;

    if (key.contains(QLatin1Char('%')) || ma_has(t, {"pct", "percent", "percentage"}))
        return MaUnit::Percent;
    if (ma_has(t, {"bps", "bp"}))
        return MaUnit::Bps;
    if (key.contains(QLatin1String("($")))
        return MaUnit::Plain; // the label carries its own unit ("Deal Value ($M)")
    if (ma_has(t, {"dollar", "dollars"}))
        return MaUnit::Money;
    static const QSet<QString> kPercentRatios = {"efficiency_ratio", "loan_to_deposit_ratio", "combined_ratio",
                                                 "loss_ratio",       "expense_ratio",         "npa_ratio",
                                                 "npl_ratio",        "tier1_capital_ratio",   "cet1_ratio"};
    if (kPercentRatios.contains(k))
        return MaUnit::Percent;
    if (integral && ma_has(t, {"count", "num", "number"}))
        return MaUnit::Integer; // "ev_ebit_count" is a count, not a multiple
    if (!t.isEmpty() && t.first() == QLatin1String("pe"))
        return MaUnit::Multiple;
    if (ma_has(t, {"multiple", "multiples", "moic", "ptbv", "ptb", "ltv"}) || k.startsWith(QLatin1String("ev_")) ||
        k.contains(QLatin1String("_ev_")) || key.contains(QLatin1String("EV/")) || key.contains(QLatin1String("P/E")) ||
        k.startsWith(QLatin1String("price_to_")) || k == QLatin1String("pe") || k == QLatin1String("pe_ratio"))
        return MaUnit::Multiple;
    if (integral && ma_has(t, {"year", "years", "period", "periods", "months", "days", "count", "num", "number", "rank",
                               "steps", "simulations", "deals", "comps", "methods", "phase", "candidates", "shares",
                               "holding", "comparables"}))
        return MaUnit::Integer;
    if (ma_has(t, {"months", "days", "years"}))
        return MaUnit::Plain;
    if (ma_has(t, {"factor", "multiplier", "score", "ratio", "exchange", "beta", "coefficient", "squared",
                   "efficiency", "coverage", "quality", "index", "strength", "compliance", "level", "percentile"}))
        return MaUnit::Plain;
    const bool rate = ma_has(t, {"rate",  "rates",     "margin",    "growth",  "yield",  "irr",     "wacc",
                                 "return", "returns",  "probability", "weight", "ownership", "cagr", "premium",
                                 "retention", "dilution", "contribution", "ror", "roe",    "roa",     "roic",
                                 "nim",   "nrr",       "tax",       "discount", "upside", "accretion"}) ||
                      k.contains(QLatin1String("cost_of_"));
    // No percentage is in the thousands: a "tax" or "premium" that large is an amount.
    if (rate && !ma_has(t, {"written", "deposit"}) && !(std::isfinite(v) && std::fabs(v) >= 1000.0))
        return MaUnit::Percent;
    if (k.contains(QLatin1String("_to_")))
        return MaUnit::Plain;
    if (ma_has(t, {"value",       "values",     "price",     "revenue",  "revenues", "ebitda",    "ebit",
                   "income",      "cash",       "debt",      "equity",   "consideration", "payment", "payments",
                   "fees",        "fee",        "cost",      "costs",    "capex",    "nopat",     "fcf",
                   "gain",        "aum",        "gmv",       "arr",      "deposits", "loans",     "assets",
                   "book",        "synergy",    "synergies", "valuation", "valuations", "proceeds", "interest",
                   "taxes",       "depreciation", "amortization", "sources", "uses",  "financing", "amount",
                   "threshold",   "payout",     "investment", "invested", "nav",     "capital",   "ev",
                   "backlog",     "premium",    "eps",       "rnpv",     "npv",      "pv",        "productivity",
                   "flows",       "exposure",   "earnout",   "purchase", "offer",    "sales",     "profit",
                   "expense",     "expenses",   "cogs",      "nwc",      "balance",  "shortfall", "excess",
                   "imbalance",   "remaining",  "used",      "required", "portion",  "adjustment", "comp",
                   "tax",         "ebt",        "paydown",   "opening",  "closing",  "beginning", "ending",
                   "amort",       "sweep",      "revolver",  "tranche",  "burn"}))
        return MaUnit::Money;
    // No unit word of its own ("to_low", "p25", "median"): the unit family of the parent
    // object (percent / money / multiple) — never its count-ness ("base_year_fcf").
    const QString parent_path = path.section(QLatin1Char('.'), 0, -2);
    const QString parent = parent_path.section(QLatin1Char('.'), -1);
    if (depth < 2 && !parent.isEmpty() && parent != key) {
        const MaUnit pu = ma_unit(context, parent_path, parent, v, depth + 1);
        return (pu == MaUnit::Integer || pu == MaUnit::Year) ? MaUnit::Plain : pu;
    }
    return MaUnit::Plain;
}

// Headline KPIs per context (dotted paths into the result), in display order.
QStringList ma_kpi_paths(const QString& context, const QJsonObject& payload) {
    static const QHash<QString, QStringList> kKpis = {
        {"dcf",
         {"summary.enterprise_value", "summary.equity_value", "summary.price_per_share", "summary.wacc",
          "enterprise_value.terminal_value_contribution"}},
        {"lbo_returns", {"irr_pct", "moic", "absolute_gain", "exit_equity_value", "total_debt"}},
        {"lbo_model",
         {"returns.irr_pct", "returns.moic", "returns.entry_equity", "returns.exit_equity",
          "debt_summary.paydown_percentage"}},
        {"lbo_debt_schedule",
         {"summary.total_debt", "summary.exit_debt", "summary.total_paydown", "summary.total_interest",
          "summary.paydown_percentage"}},
        {"lbo_sensitivity",
         {"base_case.returns.irr_pct", "base_case.returns.moic", "base_case.returns.entry_equity",
          "base_case.returns.exit_equity"}},
        {"precedent_txns",
         {"target_valuation.valuations.blended_median", "target_valuation.valuations.ev_revenue_median",
          "target_valuation.valuations.ev_ebitda_median", "comp_count"}},
        {"accretion_dilution",
         {"accretion_dilution.accretion_dilution_pct", "accretion_dilution.pro_forma_eps",
          "accretion_dilution.standalone_eps", "breakeven_synergies", "max_premium_eps_neutral_pct",
          "deal_overview.acquirer_ownership_pct"}},
        {"pro_forma", {"pro_forma.revenue", "pro_forma.ebitda", "pro_forma.net_income", "pro_forma.eps", "year"}},
        {"sources_uses", {"total_sources", "total_uses", "imbalance", "balanced", "funding_shortfall"}},
        {"synergy_dcf", {"total_synergy_value", "explicit_forecast_pv", "terminal_value_pv", "terminal_value_pct"}},
        {"payment_structure",
         {"cash_required", "debt_required", "new_shares_issued", "shareholder_dilution", "cash_percentage"}},
        {"earnout",
         {"total_earnout_face_value", "total_earnout_expected_value", "total_earnout_pv", "base_price",
          "earnout_as_pct_of_deal"}},
        {"exchange_ratio", {"exchange_ratio", "offer_price_per_target_share", "premium_to_target"}},
        {"collar", {"value_range.min", "value_range.base", "value_range.max", "floor_price", "cap_price"}},
        {"cvr", {"total_expected_value", "total_face_value", "blended_probability", "discount_rate"}},
        {"berkus", {"total_valuation", "max_possible_valuation", "valuation_percentage"}},
        {"scorecard", {"final_valuation", "baseline_valuation", "cumulative_multiplier", "adjustment_pct"}},
        {"vc_method",
         {"post_money_valuation", "pre_money_valuation", "investor_ownership_pct", "investor_irr",
          "investor_return_multiple"}},
        {"first_chicago", {"valuation", "expected_present_value", "discount_rate"}},
        {"risk_factor", {"final_valuation", "base_valuation", "total_adjustment", "adjustment_pct"}},
        {"startup_comprehensive",
         {"recommendation.suggested_valuation", "valuation_summary.min", "valuation_summary.median",
          "valuation_summary.max", "recommendation.confidence"}},
        // The range check (formerly "fairness opinion"; the context id is kept so saved inputs survive).
        {"fairness_opinion",
         {"offer_price", "weighted_valuation_range.low", "weighted_valuation_range.high", "position_in_range",
          "range_check_result"}},
        {"premium_analysis", {"offer_price_per_share", "offer_value"}},
        {"process_quality", {"quality_rating", "process_adequate"}},
        {"monte_carlo", {"mean", "median", "p5", "p95", "probability_positive"}},
        {"benchmark_premium",
         {"premium_comparison.target", "premium_comparison.median", "target_position", "num_comparables"}},
        {"payment_structures", {"total_deals", "average_cash_pct", "average_stock_pct"}},
    };
    QStringList out = kKpis.value(context);
    // Industry tabs: the script's own valuation_metrics are the headline.
    if (out.isEmpty() && (context == QLatin1String("tech_metrics") || context == QLatin1String("healthcare_metrics") ||
                          context == QLatin1String("finserv_metrics"))) {
        const QJsonObject vm = payload.value(QStringLiteral("valuation_metrics")).toObject();
        for (auto it = vm.constBegin(); it != vm.constEnd(); ++it)
            out << QStringLiteral("valuation_metrics.") + it.key();
    }
    return out;
}

} // namespace

/// An M&A amount, abbreviated per the user's compact scheme. `intrinsic` pins
/// the currency (Deals: USD filings); empty = a calculator the user
/// denominates in their display currency (read at format time, not captured,
/// so a re-render after a settings change picks up the new code) — the same setting
/// as the "($)" input labels beside these results (fmt::bind_currency_label).
QString ma_money_text(double v, const QString& intrinsic) {
    namespace fmt = fincept::ui::formatting;
    const QString ccy = intrinsic.isEmpty() ? fmt::display_currency() : intrinsic;
    return fmt::money(v, ccy, fmt::Compact::FromSettings);
}

fincept::ui::result::Options ma_result_options(const QString& context, const QJsonObject& payload,
                                               const QString& intrinsic) {
    fincept::ui::result::Options o;
    o.unit_for = [context](const QString& path, const QString& key, double v) { return ma_unit(context, path, key, v); };
    o.money_format = [intrinsic](double v) { return ma_money_text(v, intrinsic); };
    // samples: drawn as the Monte Carlo histogram; missing_inputs / needs_data: handled by the panel.
    o.hidden = {QStringLiteral("success"), QStringLiteral("command"), QStringLiteral("samples"),
                QStringLiteral("missing_inputs"), QStringLiteral("needs_data")};
    o.kpi_paths = ma_kpi_paths(context, payload);
    // Rows = entry multiple, columns = exit multiple (as the script builds them).
    o.matrix_axes.insert(QStringLiteral("irr_pct_matrix"),
                         {QStringLiteral("entry_multiple_range"), QStringLiteral("exit_multiple_range")});
    o.matrix_axes.insert(QStringLiteral("moic_matrix"),
                         {QStringLiteral("entry_multiple_range"), QStringLiteral("exit_multiple_range")});
    o.x_keys << QStringLiteral("acquirer_price"); // collar scenarios
    o.bar_columns = {{QStringLiteral("factor_assessments"),
                      context == QLatin1String("scorecard") ? QStringLiteral("weight") : QStringLiteral("value")},
                     {QStringLiteral("risk_assessments"), QStringLiteral("adjustment")},
                     {QStringLiteral("scenarios"), QStringLiteral("weighted_present_value")},
                     {QStringLiteral("valuations_by_method"), QStringLiteral("valuation")},
                     {QStringLiteral("premiums"), QStringLiteral("premium_pct")},
                     {QStringLiteral("by_industry"), QStringLiteral("avg_premium")},
                     {QStringLiteral("stats_by_type"), QStringLiteral("avg_premium")},
                     {QStringLiteral("rankings"), QStringLiteral("premium_1day")}};
    // The sensitivity grid is the answer; the full base-case model stays one click away.
    o.collapsed_paths.insert(QStringLiteral("base_case"));
    if (context == QLatin1String("contribution")) {
        for (const char* k : {"ownership", "revenue_contribution", "ebitda_contribution", "net_income_contribution",
                              "assets_contribution"})
            o.consumed.insert(QString::fromLatin1(k));
    } else if (context == QLatin1String("process_quality")) {
        o.consumed.insert(QStringLiteral("factor_scores"));
        o.consumed.insert(QStringLiteral("overall_score"));
    } else if (context == QLatin1String("dcf")) {
        o.consumed.insert(QStringLiteral("sensitivity")); // drawn as the sensitivity table
    } else if (context == QLatin1String("lbo_sensitivity")) {
        o.consumed.insert(QStringLiteral("irr_pct_matrix")); // drawn as the sensitivity table (IRR / MOIC)
        o.consumed.insert(QStringLiteral("moic_matrix"));
        o.consumed.insert(QStringLiteral("sensitivity_matrix"));
    } else if (context == QLatin1String("accretion_dilution")) {
        o.consumed.insert(QStringLiteral("sensitivity")); // drawn as the two sensitivity tables
        o.hidden.insert(QStringLiteral("kind"));          // EPS bridge step type (the chart shows it)
    } else if (context == QLatin1String("football_field")) {
        o.consumed.insert(QStringLiteral("markers"));
    }
    return o;
}

double ma_num(const QJsonObject& o, const char* key) {
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isDouble() ? v.toDouble() : std::numeric_limits<double>::quiet_NaN();
}

} // namespace fincept::screens::ma_fmt

namespace fincept::screens::ma_fmt {
QStringList ma_headline_paths(const QString& context, const QJsonObject& payload) {
    return ma_kpi_paths(context, payload);
}

QJsonObject ma_flatten_result(const QJsonObject& envelope) {
    if (!envelope.value(QStringLiteral("success")).toBool(false) || !envelope.contains(QStringLiteral("data")))
        return envelope;
    const QJsonValue data = envelope.value(QStringLiteral("data"));
    QJsonObject flat;
    for (auto it = envelope.begin(); it != envelope.end(); ++it)
        if (it.key() != QLatin1String("success") && it.key() != QLatin1String("data") && !it.value().isObject() &&
            !it.value().isArray())
            flat.insert(it.key(), it.value());
    if (data.isObject()) {
        const QJsonObject inner = data.toObject();
        for (auto it = inner.begin(); it != inner.end(); ++it)
            flat.insert(it.key(), it.value());
        return flat;
    }
    if (data.isArray()) {
        flat.insert(QStringLiteral("results"), data);
        return flat;
    }
    return envelope;
}
} // namespace fincept::screens::ma_fmt
