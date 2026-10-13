// AltInvestmentsTools.cpp — Alternative Investments tab MCP tools.
//
// One tool per analyzer that runs end to end from structured inputs in
// scripts/Analytics/alternateInvestment/cli.py. Each tool declares the cli's OWN
// input keys and the methods that actually work, and its description says what
// comes back. (The previous 27 tools called cli.py with commands that do not
// exist — "high_yield_bond", "real_estate", ... — and a generic name/value/rate
// bag no analyzer reads, so every call failed with an argparse error.)
//
// All rates are DECIMALS (0.05 = 5%), as cli.py takes them.

#include "mcp/tools/AltInvestmentsTools.h"

#include "core/logging/Logger.h"
#include "mcp/AsyncDispatch.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPromise>

#include <memory>
#include <utility>

namespace fincept::mcp::tools {

namespace alt_tools_detail {

constexpr int kAltToolTimeoutMs = 30000;
constexpr int kAltToolTtlSec = 10 * 60; // deterministic given the same inputs
constexpr const char* kAltToolScript = "Analytics/alternateInvestment/cli.py";

/// How cli.py takes the inputs.
enum class ArgStyle {
    Data,          ///< <cmd> --data <json> --method <m>
    DataTaxFlag,   ///< asset-location: tax_bracket goes to --tax-bracket
    ReturnsPrices, ///< performance: --returns <json array of prices>
    ReturnsRisk,   ///< risk: --returns <json array> --confidence-level <c>
};

struct Prop {
    const char* key;
    const char* type; // number | integer | string | array | object
    const char* description;
};

struct AltToolSpec {
    // A constructor (not an aggregate) so the optional `style` never trips
    // -Wmissing-field-initializers on the GCC/Clang builds.
    AltToolSpec(const char* n, const char* c, const char* d, QStringList m, QList<Prop> p, QStringList r,
                ArgStyle s = ArgStyle::Data)
        : name(n), command(c), description(d), methods(std::move(m)), props(std::move(p)), required(std::move(r)),
          style(s) {}
    const char* name;
    const char* command;
    const char* description;
    QStringList methods; // first = default
    QList<Prop> props;
    QStringList required;
    ArgStyle style;
};

QJsonObject schema_prop(const Prop& p) {
    QJsonObject o{{"type", QString::fromLatin1(p.type)}, {"description", QString::fromUtf8(p.description)}};
    if (QLatin1String(p.type) == QLatin1String("array"))
        o["items"] = QJsonObject{{"type", "number"}};
    return o;
}

// Resolves `promise` with the analyzer's metrics. Cache hits resolve inline.
void run_alt_tool(const QString& command, const QStringList& cli_args, ToolContext ctx,
                  std::shared_ptr<QPromise<ToolResult>> promise) {
    const QString cache_key = "alt:" + command + ":" + cli_args.join(QLatin1Char('\x1f'));
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        const auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (doc.isObject()) {
            AsyncDispatch::callback_to_promise(nullptr, ctx, promise, [obj = doc.object()](auto resolve) {
                resolve(ToolResult::ok_data(obj));
            });
            return;
        }
    }

    auto* runner = &python::PythonRunner::instance();
    AsyncDispatch::callback_to_promise(runner, ctx, promise, [runner, cli_args, cache_key, ctx](auto resolve) {
        runner->run(kAltToolScript, cli_args, [resolve, cache_key, ctx](const python::PythonResult& result) {
            if (ctx.cancelled()) {
                resolve(ToolResult::fail("cancelled"));
                return;
            }
            // cli.py prints {"success": false, "error"} with a non-zero exit on bad input:
            // prefer that JSON message over the bare process error.
            const QString json = python::extract_json(result.output);
            const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
            if (!doc.isObject()) {
                resolve(ToolResult::fail(result.error.isEmpty() ? QStringLiteral("No JSON from the analyzer")
                                                                : result.error));
                return;
            }
            const QJsonObject obj = doc.object();
            if (!obj.value("success").toBool(true) || obj.value("error").isString()) {
                resolve(ToolResult::fail(obj.value("error").toString(QStringLiteral("Analysis failed"))));
                return;
            }
            const QJsonObject metrics = obj.contains("metrics") ? obj.value("metrics").toObject() : obj;
            // Analyzers report "cannot compute" in-band: {"metrics": {"error": "..."}}.
            if (metrics.value("error").isString()) {
                resolve(ToolResult::fail(metrics.value("error").toString()));
                return;
            }
            fincept::CacheManager::instance().put(
                cache_key, QVariant(QString::fromUtf8(QJsonDocument(metrics).toJson(QJsonDocument::Compact))),
                kAltToolTtlSec, "alt_investments");
            resolve(ToolResult::ok_data(metrics));
        });
    });
}

ToolDef make_tool(const AltToolSpec& spec) {
    ToolDef t;
    t.name = QString::fromLatin1(spec.name);
    t.description = QString::fromUtf8(spec.description);
    t.category = "alt-investments";
    t.default_timeout_ms = kAltToolTimeoutMs;

    QJsonObject props;
    QStringList keys;
    for (const Prop& p : spec.props) {
        props[QString::fromLatin1(p.key)] = schema_prop(p);
        keys << QString::fromLatin1(p.key);
    }
    if (spec.methods.size() > 1) {
        QJsonArray methods;
        for (const QString& m : spec.methods)
            methods.append(m);
        props["method"] = QJsonObject{{"type", "string"},
                                      {"enum", methods},
                                      {"description", "Analysis to run (default: " + spec.methods.first() + ")"}};
    }
    t.input_schema.properties = props;
    t.input_schema.required = spec.required;

    const QString command = QString::fromLatin1(spec.command);
    const QStringList methods = spec.methods;
    const ArgStyle style = spec.style;
    t.async_handler = [command, methods, style, keys](const QJsonObject& args, ToolContext ctx,
                                                       std::shared_ptr<QPromise<ToolResult>> promise) {
        QString method = args.value("method").toString(methods.first());
        if (!methods.contains(method))
            method = methods.first();

        QStringList cli_args{command};
        if (style == ArgStyle::ReturnsPrices || style == ArgStyle::ReturnsRisk) {
            const QString series_key = style == ArgStyle::ReturnsPrices ? "prices" : "returns";
            cli_args << "--returns"
                     << QString::fromUtf8(
                            QJsonDocument(args.value(series_key).toArray()).toJson(QJsonDocument::Compact));
            if (style == ArgStyle::ReturnsRisk && args.contains("confidence_level"))
                cli_args << "--confidence-level" << QString::number(args.value("confidence_level").toDouble(), 'g', 10);
        } else {
            QJsonObject data;
            for (const QString& k : keys) {
                if (args.contains(k) && !(style == ArgStyle::DataTaxFlag && k == QLatin1String("tax_bracket")))
                    data.insert(k, args.value(k));
            }
            cli_args << "--data" << QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact));
            if (style == ArgStyle::DataTaxFlag && args.contains("tax_bracket"))
                cli_args << "--tax-bracket" << QString::number(args.value("tax_bracket").toDouble(), 'g', 10);
        }
        cli_args << "--method" << method;
        run_alt_tool(command, cli_args, ctx, promise);
    };
    return t;
}

} // namespace alt_tools_detail

std::vector<ToolDef> get_alt_investments_tools() {
    using alt_tools_detail::ArgStyle;
    using alt_tools_detail::make_tool;

    const QList<alt_tools_detail::AltToolSpec> specs = {
        // ── Bonds & fixed income ────────────────────────────────────────────
        {"alt_high_yield_bond", "high-yield",
         "High-yield bond. credit_analysis: approximate YTM, spread over the treasury_yield you give (decimal and "
         "bps) and how wide it is for the rating; default_prob: rating-bucket 5-year default probability, recovery "
         "and expected loss.",
         {"credit_analysis", "default_prob"},
         {{"face_value", "number", "Face value"},
          {"current_market_value", "number", "Market price (same units as face value)"},
          {"coupon_rate", "number", "Annual coupon, decimal"},
          {"maturity_years", "number", "Years to maturity"},
          {"credit_rating", "string", "Rating bucket: BB+, BB, BB-, B+, B, B-, CCC+, CCC"},
          {"treasury_yield", "number", "Treasury yield of similar tenor, decimal (default 0.04)"}},
         {"face_value", "current_market_value", "coupon_rate", "maturity_years"}},
        {"alt_emerging_market_bond", "em-bonds",
         "Emerging-market sovereign bond. yield_spread: current yield, approximate YTM and spread over "
         "treasury_yield; default_risk: cumulative default probability, expected loss and whether credit_spread "
         "compensates it; currency_risk: total volatility once FX risk is added (local-currency bonds only — "
         "currency USD/EUR returns 'no FX risk').",
         {"yield_spread", "default_risk", "currency_risk"},
         {{"face_value", "number", "Face value"},
          {"current_market_value", "number", "Market price"},
          {"coupon_rate", "number", "Annual coupon, decimal"},
          {"maturity_years", "number", "Years to maturity"},
          {"credit_spread", "number", "Spread earned over treasuries, decimal"},
          {"treasury_yield", "number", "Treasury yield of similar tenor, decimal"},
          {"sovereign_rating", "string", "Sovereign rating, e.g. BB"},
          {"currency", "string", "Bond currency: USD, EUR or a local currency code"},
          {"historical_default_rate", "number", "Annual default rate for the rating, decimal (default 0.03)"},
          {"recovery_rate", "number", "Recovery on default, decimal (default 0.30)"},
          {"local_currency_volatility", "number", "FX volatility vs USD, decimal (default 0.15)"},
          {"fx_correlation", "number", "Correlation of FX with bond returns (default -0.30)"}},
         {"face_value", "current_market_value", "coupon_rate", "maturity_years"}},
        {"alt_convertible_bond", "convertible-bonds",
         "Convertible bond. conversion_premium: conversion value, premium paid over it and the stock rise needed "
         "to break even; upside_participation: convertible vs stock return across stock_price_scenarios.",
         {"conversion_premium", "upside_participation"},
         {{"face_value", "number", "Face value"},
          {"current_market_value", "number", "Convertible's market price"},
          {"coupon_rate", "number", "Annual coupon, decimal"},
          {"maturity_years", "number", "Years to maturity"},
          {"stock_price", "number", "Underlying stock price"},
          {"conversion_ratio", "number", "Shares received per bond"},
          {"stock_price_scenarios", "array", "Stock prices to test (default [40, 50, 60, 70])"}},
         {"current_market_value", "stock_price", "conversion_ratio"}},
        {"alt_preferred_stock", "preferred-stocks",
         "Preferred stock. yield_analysis: current yield and yield to call; call_risk: callability assessment "
         "(needs call_price / years_to_call); dividend_safety: qualitative suspension-risk notes.",
         {"yield_analysis", "call_risk", "dividend_safety"},
         {{"par_value", "number", "Par value"},
          {"current_price", "number", "Market price"},
          {"dividend_rate", "number", "Dividend rate on par, decimal"},
          {"call_price", "number", "Call price (optional)"},
          {"years_to_call", "number", "Years until first call (optional)"}},
         {"par_value", "current_price", "dividend_rate"}},
        // ── Real estate ─────────────────────────────────────────────────────
        {"alt_real_estate", "real-estate",
         "Direct property. noi: net operating income (gross rent less vacancy and operating expenses); caprate: "
         "NOI / price; dcf: DCF value with terminal value at terminal_cap_rate, discounted at discount_rate.",
         {"noi", "caprate", "dcf"},
         {{"acquisition_price", "number", "Purchase price"},
          {"gross_rental_income", "number", "Gross annual rent"},
          {"vacancy_rate", "number", "Vacancy, decimal"},
          {"operating_expenses", "number", "Annual operating expenses"},
          {"current_market_value", "number", "Current value (optional)"},
          {"projection_years", "integer", "DCF horizon (default 10)"},
          {"terminal_cap_rate", "number", "Exit cap rate, decimal (default 0.06)"},
          {"discount_rate", "number", "Discount rate, decimal (default 0.08)"}},
         {"acquisition_price", "gross_rental_income"}},
        {"alt_reit", "intl-reit",
         "International REIT allocation. diversification: correlation benefit vs US REITs; currency: USD return "
         "from local and currency returns; expense: net return and 10-year expense drag; regional: qualitative "
         "regional characteristics.",
         {"diversification", "currency", "expense", "regional"},
         {{"region", "string", "Region, e.g. Europe, Asia"},
          {"local_return", "number", "Local-currency annual return, decimal"},
          {"currency_return", "number", "Annual FX return vs USD, decimal"},
          {"expense_ratio", "number", "Fund expense ratio, decimal"},
          {"correlation_us", "number", "Correlation with US REITs"}},
         {}},
        // ── Hedge funds ─────────────────────────────────────────────────────
        {"alt_long_short_equity", "hedge-funds",
         "Hedge fund performance from the fund's own MONTHLY GROSS returns (>= 12): cumulative return, CAGR, "
         "annualised volatility, Sharpe, Sortino, max drawdown — gross and net of a management fee (monthly on "
         "NAV) and a performance fee over a high-water mark and hurdle — plus the annual fee drag.",
         {"metrics"},
         {{"monthly_returns", "array", "Monthly gross returns, decimals, oldest first (at least 12)"},
          {"management_fee", "number", "Annual management fee, decimal (default 0.02)"},
          {"performance_fee", "number", "Performance fee, decimal (default 0.20)"},
          {"hurdle_rate", "number", "Annual hurdle, decimal (default 0)"},
          {"risk_free_rate", "number", "Annual risk-free rate for Sharpe/Sortino, decimal (default 0)"},
          {"strategy_type", "string", "Strategy label (informational)"}},
         {"monthly_returns"}},
        {"alt_managed_futures", "managed-futures",
         "Managed futures / CTA fee drag: gross vs net wealth and CAGR over `years` at a constant gross_return "
         "under the given management / performance fee and hurdle, with a yearly breakdown.",
         {"fee_impact"},
         {{"gross_return", "number", "Annual gross return, decimal (default 0.10)"},
          {"years", "integer", "Horizon in years (default 10)"},
          {"management_fee", "number", "Annual management fee, decimal (default 0.02)"},
          {"performance_fee", "number", "Performance fee, decimal (default 0.20)"},
          {"hurdle_rate", "number", "Hurdle, decimal (default 0)"}},
         {}},
        {"alt_market_neutral", "market-neutral",
         "Market-neutral fund leverage risk: net and gross exposure, leverage ratio and the portfolio impact of a "
         "10% adverse move on both legs.",
         {"leverage_risk"},
         {{"long_exposure", "number", "Long exposure as a multiple of capital (e.g. 1.0)"},
          {"short_exposure", "number", "Short exposure as a multiple of capital (e.g. 1.0)"}},
         {"long_exposure", "short_exposure"}},
        // ── Commodities ─────────────────────────────────────────────────────
        {"alt_natural_resources", "natural-resources",
         "Commodity futures basis: futures minus spot, basis as % of spot, annualised basis and whether the curve "
         "is in contango or backwardation.",
         {"basis"},
         {{"spot_price", "number", "Spot price"},
          {"futures_price", "number", "Futures price"},
          {"expiry_months", "integer", "Months to futures expiry (default 3)"}},
         {"spot_price", "futures_price"}},
        // ── Private capital ─────────────────────────────────────────────────
        {"alt_private_equity", "private-capital",
         "Private equity / VC fund metrics from the LP's cash flows: IRR (NAV as the terminal value at nav_date), "
         "TVPI (= moic), DPI, RVPI, paid-in and distributed capital, called share of commitment.",
         {"metrics"},
         {{"cash_flows", "array",
           "List of {date: 'YYYY-MM-DD', amount}; capital calls negative, distributions positive"},
          {"current_nav", "number", "Current NAV of the LP's interest"},
          {"nav_date", "string", "NAV as-of date YYYY-MM-DD (default today)"},
          {"commitment", "number", "Total commitment (optional)"},
          {"vintage_year", "integer", "Vintage year (optional)"}},
         {"cash_flows"}},
        // ── Annuities ───────────────────────────────────────────────────────
        {"alt_fixed_annuity", "annuities",
         "Fixed annuity payouts: annual payout, total payouts over the term and undiscounted breakeven years.",
         {"payouts"},
         {{"acquisition_price", "number", "Premium paid"},
          {"annuity_rate", "number", "Annual payout rate, decimal"},
          {"payout_years", "number", "Payout term in years"}},
         {"acquisition_price", "annuity_rate"}},
        {"alt_variable_annuity", "variable-annuities",
         "Variable annuity. fees: total annual cost from M&E, investment and admin fees; tax: tax-deferred VA vs "
         "taxable index fund over `years` at gross_return; alternatives / verdict: qualitative comparison.",
         {"fees", "tax", "alternatives", "verdict"},
         {{"premium", "number", "Premium invested"},
          {"me_fee", "number", "Mortality & expense fee, decimal"},
          {"investment_fee", "number", "Sub-account fee, decimal"},
          {"admin_fee", "number", "Admin fee, decimal"},
          {"surrender_period", "integer", "Surrender period in years"},
          {"years", "integer", "Horizon for `tax` (default 20)"},
          {"gross_return", "number", "Gross annual return for `tax`, decimal (default 0.08)"}},
         {}},
        {"alt_equity_indexed_annuity", "eia",
         "Equity-indexed annuity. crediting: credited return for an index_return after participation, cap, floor "
         "and spread; upside_limitation: the same across market_scenarios; surrender_charges: declining schedule.",
         {"crediting", "upside_limitation", "surrender_charges"},
         {{"index_return", "number", "Index return for `crediting`, decimal"},
          {"participation_rate", "number", "Participation, decimal"},
          {"cap_rate", "number", "Cap, decimal"},
          {"floor_rate", "number", "Floor, decimal"},
          {"spread", "number", "Spread/fee deducted, decimal"},
          {"market_scenarios", "array", "Index returns to test, decimals"}},
         {}},
        {"alt_inflation_annuity", "inflation-annuity",
         "Inflation-indexed annuity. compare_fixed: initial payout vs a fixed annuity and the crossover year; "
         "compare_tips: income vs a TIPS ladder; longevity: break-even age; inflation_value: real value preserved.",
         {"compare_fixed", "compare_tips", "longevity", "inflation_value"},
         {{"acquisition_price", "number", "Premium"},
          {"age", "integer", "Age at purchase"},
          {"real_payout_rate", "number", "Initial real payout rate, decimal"},
          {"inflation_rate", "number", "Assumed inflation, decimal"},
          {"payout_years", "number", "Payout term in years"},
          {"fixed_payout_rate", "number", "Fixed annuity's payout rate, decimal"},
          {"tips_real_yield", "number", "TIPS real yield, decimal (default 0.02)"}},
         {}},
        // ── Structured products ─────────────────────────────────────────────
        {"alt_structured_note", "structured-products",
         "Structured notes: a fixed qualitative verdict (complexity, embedded costs, issuer credit risk). Takes no "
         "numeric inputs and does not price a specific note.",
         {"verdict"},
         {{"name", "string", "Product name (label only)"}},
         {}},
        {"alt_leveraged_fund", "leveraged-funds",
         "Leveraged / inverse ETF. decay: compounding of daily-rebalanced leverage over two choppy return paths "
         "vs the unleveraged path; verdict: qualitative assessment.",
         {"decay", "verdict"},
         {{"leverage_ratio", "number", "Daily leverage multiple, e.g. 2 or 3"},
          {"expense_ratio", "number", "Expense ratio, decimal"}},
         {}},
        // ── Inflation protected ─────────────────────────────────────────────
        {"alt_tips", "tips",
         "TIPS. real_yield: current yield and approximate real YTM from the price; inflation_scenarios: principal "
         "and value under inflation scenarios; tax_efficiency: tax on coupon and phantom (inflation) income.",
         {"real_yield", "inflation_scenarios", "tax_efficiency"},
         {{"acquisition_price", "number", "Face / principal"},
          {"current_price", "number", "Market price"},
          {"coupon_rate", "number", "Real coupon, decimal"},
          {"maturity_years", "number", "Years to maturity"},
          {"inflation_scenarios", "array", "Annual inflation scenarios, decimals"},
          {"tax_rate_ordinary", "number", "Ordinary tax rate, decimal (default 0.30)"},
          {"tax_rate_capital_gains", "number", "Capital-gains rate, decimal (default 0.15)"}},
         {}},
        {"alt_ibond", "ibonds",
         "US Series I savings bond. composite_rate: fixed + 2 x semiannual inflation + fixed x semiannual "
         "inflation; penalty: 3-month interest penalty before 5 years; compare_tips; tax_efficiency (qualitative).",
         {"composite_rate", "penalty", "compare_tips", "tax_efficiency"},
         {{"face_value", "number", "Amount purchased"},
          {"fixed_rate", "number", "Fixed rate, decimal"},
          {"inflation_rate", "number", "SEMIANNUAL CPI-U inflation rate as announced by Treasury, decimal"},
          {"years_held", "number", "Years held so far"},
          {"tips_yield", "number", "TIPS real yield for compare_tips, decimal"}},
         {"fixed_rate", "inflation_rate"}},
        {"alt_stable_value_fund", "stable-value",
         "Stable value fund. market_to_book: market/book ratio and the disconnect; crediting_rate: crediting rate "
         "vs portfolio yield, wrap fee and rates derived from treasury_yield; suitability: by age and risk "
         "tolerance.",
         {"market_to_book", "crediting_rate", "suitability"},
         {{"acquisition_price", "number", "Book value"},
          {"current_market_value", "number", "Market value"},
          {"crediting_rate", "number", "Crediting rate, decimal"},
          {"portfolio_yield", "number", "Underlying portfolio yield, decimal"},
          {"wrap_fee", "number", "Wrap fee, decimal"},
          {"treasury_yield", "number", "Treasury yield, decimal"},
          {"credit_spread", "number", "Credit spread, decimal"},
          {"investor_age", "integer", "Investor age"},
          {"retirement_age", "integer", "Retirement age"},
          {"risk_tolerance", "string", "conservative | moderate | aggressive"}},
         {}},
        // ── Strategies ──────────────────────────────────────────────────────
        {"alt_covered_call", "covered-calls",
         "Covered call. tax: tax on the premium and on assignment vs long-term treatment (US rules; stock held "
         "over a year keeps long-term treatment); opportunity_cost: covered-call vs stock return across scenarios; "
         "alternative / verdict: qualitative.",
         {"tax", "opportunity_cost", "alternative", "verdict"},
         {{"stock_price", "number", "Stock price"},
          {"strike_price", "number", "Call strike"},
          {"option_premium", "number", "Premium per share"},
          {"shares_owned", "number", "Shares"},
          {"cost_basis", "number", "Cost basis per share"},
          {"holding_period_days", "integer", "Days the stock has been held"},
          {"ordinary_tax_rate", "number", "Ordinary income tax rate, decimal"},
          {"ltcg_rate", "number", "Long-term capital gains rate, decimal"}},
         {"stock_price", "strike_price", "option_premium"}},
        {"alt_sri_fund", "sri",
         "SRI / ESG fund. performance: terminal wealth vs benchmark and attribution; screening: diversification "
         "impact; expenses: cost of the expense premium over 10/20/30 years; approaches: qualitative comparison.",
         {"performance", "screening", "expenses", "approaches"},
         {{"fund_return", "number", "Fund annual return, decimal"},
          {"benchmark_return", "number", "Benchmark annual return, decimal"},
          {"expense_ratio", "number", "Fund expense ratio, decimal"},
          {"benchmark_expense", "number", "Benchmark expense ratio, decimal"},
          {"num_holdings", "integer", "Fund holdings"},
          {"benchmark_holdings", "integer", "Benchmark holdings"}},
         {}},
        {"alt_asset_location", "asset-location",
         "Tax-efficient asset location. optimal: best account type for an asset_class; value_added: after-tax "
         "value of optimal vs suboptimal placement; muni_bond: tax-equivalent yield decision.",
         {"optimal", "value_added", "muni_bond"},
         {{"asset_class", "string",
           "reits | high_yield_bonds | stocks | municipal_bonds | tips | commodities | cash"},
          {"tax_bracket", "number", "Marginal tax rate, decimal (default 0.24)"},
          {"value", "number", "Asset value for value_added"},
          {"years", "integer", "Horizon for value_added (default 30)"},
          {"municipal_yield", "number", "Muni yield, decimal"},
          {"taxable_yield", "number", "Taxable bond yield, decimal"}},
         {},
         ArgStyle::DataTaxFlag},
        {"alt_performance_metrics", "performance",
         "Time-weighted return of a MONTHLY price (or NAV) series: cumulative TWR, annualised TWR, periods.",
         {"twr"},
         {{"prices", "array", "Month-end prices / NAVs, oldest first (at least 2)"}},
         {"prices"},
         ArgStyle::ReturnsPrices},
        {"alt_risk_analysis", "risk",
         "Risk of a return series: var — historical and parametric VaR with CVaR at confidence_level; cvar — "
         "expected shortfall; stress — stress-test scenarios. Values are losses as decimals of value.",
         {"var", "cvar", "stress"},
         {{"returns", "array", "Periodic returns, decimals"},
          {"confidence_level", "number", "Confidence, e.g. 0.95 or 0.99 (default 0.95)"}},
         {"returns"},
         ArgStyle::ReturnsRisk},
        // ── Digital assets ──────────────────────────────────────────────────
        {"alt_digital_asset", "digital-assets",
         "Cryptocurrency fundamentals: implied price (market cap / circulating supply), potential supply "
         "inflation, volume / market cap and a liquidity tier.",
         {"fundamental"},
         {{"market_cap", "number", "Market capitalisation"},
          {"circulating_supply", "number", "Circulating supply"},
          {"total_supply", "number", "Total or max supply"},
          {"trading_volume_24h", "number", "24h trading volume"},
          {"staking_yield", "number", "Staking yield, decimal (optional)"}},
         {"market_cap", "circulating_supply"}},
    };

    std::vector<ToolDef> tools;
    tools.reserve(specs.size());
    for (const auto& spec : specs)
        tools.push_back(make_tool(spec));
    return tools;
}

} // namespace fincept::mcp::tools
