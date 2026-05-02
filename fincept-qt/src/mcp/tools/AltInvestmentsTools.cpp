// AltInvestmentsTools.cpp — Alternative Investments tab MCP tools (27 analyzers)

#include "mcp/tools/AltInvestmentsTools.h"

#include "core/logging/Logger.h"
#include "mcp/AsyncDispatch.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QPromise>

#include <memory>

namespace fincept::mcp::tools {

static constexpr int kAltTimeoutMs = 30000;
static constexpr int kAltTtlSec = 10 * 60; // 10 min — deterministic given same inputs

// ── Shared async helper ──────────────────────────────────────────────────────
// Phase 4: Resolves `promise` with the metrics JSON when cli.py finishes.
// Returns immediately; the worker thread never blocks. Cache hits resolve
// synchronously inside this function.
//
// command  = analyzer id (e.g. "high_yield_bond")
// params   = JSON object with name/type/value/rate/term/fee/current_value/additional

static void run_alt_async(const QString& command, const QJsonObject& params, ToolContext ctx,
                           std::shared_ptr<QPromise<ToolResult>> promise) {
    const QString data_json = QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact));
    const QString cache_key = "alt:" + command + ":" + data_json;

    // Cache hit — resolve immediately, no Python spawn.
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (!doc.isNull() && doc.isObject()) {
            promise->addResult(ToolResult::ok_data(doc.object()));
            promise->finish();
            return;
        }
    }

    auto* runner = &python::PythonRunner::instance();
    AsyncDispatch::callback_to_promise(
        runner, ctx, promise,
        [runner, command, data_json, cache_key, ctx](auto resolve) {
            runner->run("Analytics/alternateInvestment/cli.py", {command, "--data", data_json},
                        [resolve, cache_key, ctx](const python::PythonResult& result) {
                            if (ctx.cancelled()) {
                                resolve(ToolResult::fail("cancelled"));
                                return;
                            }
                            if (!result.success) {
                                resolve(ToolResult::fail(
                                    result.error.isEmpty() ? "Analysis failed" : result.error));
                                return;
                            }
                            // Extract JSON from stdout
                            QString out = result.output;
                            int start = out.indexOf('{');
                            int end = out.lastIndexOf('}');
                            if (start < 0 || end < 0 || end <= start) {
                                resolve(ToolResult::fail("No JSON in output"));
                                return;
                            }
                            QJsonParseError err;
                            auto doc = QJsonDocument::fromJson(
                                out.mid(start, end - start + 1).toUtf8(), &err);
                            if (doc.isNull() || !doc.isObject()) {
                                resolve(ToolResult::fail("Invalid JSON: " + err.errorString()));
                                return;
                            }
                            auto obj = doc.object();
                            if (obj.contains("error")) {
                                resolve(ToolResult::fail(obj["error"].toString()));
                                return;
                            }
                            auto metrics = obj.contains("metrics") ? obj["metrics"].toObject() : obj;
                            fincept::CacheManager::instance().put(
                                cache_key,
                                QVariant(QString::fromUtf8(QJsonDocument(metrics).toJson(QJsonDocument::Compact))),
                                kAltTtlSec, "alt_investments");
                            resolve(ToolResult::ok_data(metrics));
                        });
        });
}

// ── Common input schema ──────────────────────────────────────────────────────

static ToolSchema make_schema(const QString& type_description) {
    ToolSchema s;
    s.properties = QJsonObject{
        {"name", QJsonObject{{"type", "string"}, {"description", "Investment name or label"}}},
        {"type", QJsonObject{{"type", "string"}, {"description", type_description}}},
        {"value", QJsonObject{{"type", "number"}, {"description", "Investment value / purchase price ($)"}}},
        {"rate", QJsonObject{{"type", "number"}, {"description", "Rate or yield as percentage (e.g. 5.0 for 5%)"}}},
        {"term", QJsonObject{{"type", "number"}, {"description", "Term in years"}}},
        {"fee", QJsonObject{{"type", "number"}, {"description", "Annual fee / cost as percentage"}}},
        {"current_value", QJsonObject{{"type", "number"}, {"description", "Current market value ($). 0 if unknown"}}},
        {"additional", QJsonObject{{"type", "number"}, {"description", "Additional parameter (analyzer-specific)"}}},
    };
    s.required = {"value"};
    return s;
}

// ── Tool factory macro ───────────────────────────────────────────────────────
// Phase 4: every alt analyzer runs cli.py asynchronously. The macro emits an
// AsyncToolHandler that builds the params dict and delegates to run_alt_async,
// which resolves the promise when Python finishes (or the watchdog fires).

#define ALT_TOOL(CMD, NAME, DESC, TYPE_DESC)                                                                           \
    {                                                                                                                  \
        ToolDef t;                                                                                                     \
        t.name = (NAME);                                                                                               \
        t.description = (DESC);                                                                                        \
        t.category = "alt-investments";                                                                                \
        t.input_schema = make_schema(TYPE_DESC);                                                                       \
        t.default_timeout_ms = kAltTimeoutMs;                                                                          \
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,                                                 \
                              std::shared_ptr<QPromise<ToolResult>> promise) {                                          \
            QJsonObject p;                                                                                             \
            p["name"] = args["name"].toString("Investment");                                                           \
            p["type"] = args["type"].toString("default");                                                              \
            p["value"] = args["value"].toDouble(0);                                                                    \
            p["rate"] = args["rate"].toDouble(0) / 100.0;                                                              \
            p["term"] = args["term"].toDouble(10);                                                                     \
            p["fee"] = args["fee"].toDouble(0) / 100.0;                                                                \
            p["current_value"] = args["current_value"].toDouble(0);                                                    \
            p["additional"] = args["additional"].toDouble(0);                                                          \
            run_alt_async(CMD, p, ctx, promise);                                                                       \
        };                                                                                                             \
        tools.push_back(std::move(t));                                                                                 \
    }

// ── Tool registration ────────────────────────────────────────────────────────

std::vector<ToolDef> get_alt_investments_tools() {
    std::vector<ToolDef> tools;

    // ── Bonds & Fixed Income ─────────────────────────────────────────────────
    ALT_TOOL("high_yield_bond", "alt_high_yield_bond",
             "Analyze a high-yield (junk) bond investment. Returns credit rating, yield spread, "
             "default probability, and equity-like behavior verdict.",
             "Credit rating bucket: BBB, BB, B, CCC")

    ALT_TOOL("emerging_market_bond", "alt_emerging_market_bond",
             "Analyze an emerging-market bond. Returns yield spread, currency risk, sovereign "
             "default risk, and suitability verdict.",
             "Country/market type (e.g. 'brazil', 'india', 'frontier')")

    ALT_TOOL("convertible_bond", "alt_convertible_bond",
             "Analyze a convertible bond. Returns conversion premium, bond floor, upside "
             "participation, and risk/reward verdict.",
             "Conversion type: 'vanilla', 'mandatory', 'contingent'")

    ALT_TOOL("preferred_stock", "alt_preferred_stock",
             "Analyze preferred stock. Returns dividend yield, call risk, dividend safety score, "
             "and income-quality verdict.",
             "Preferred type: 'cumulative', 'non_cumulative', 'convertible', 'participating'")

    // ── Real Estate ──────────────────────────────────────────────────────────
    ALT_TOOL("real_estate", "alt_real_estate",
             "Analyze a direct real estate investment. Returns cap rate, NOI, DCF valuation, "
             "debt coverage ratio, and investment verdict.",
             "Property type: office, retail, industrial, multifamily, hotel")

    ALT_TOOL("reit", "alt_reit",
             "Analyze a REIT investment. Returns FFO yield, dividend sustainability, leverage "
             "ratio, and income/growth verdict.",
             "REIT type: 'equity', 'mortgage', 'hybrid', 'international'")

    // ── Hedge Funds ──────────────────────────────────────────────────────────
    ALT_TOOL("hedge_fund", "alt_long_short_equity",
             "Analyze a long/short equity hedge fund. Returns Sharpe ratio, max drawdown, "
             "fee impact (2/20), and risk-adjusted return verdict.",
             "Strategy: equity_long_short, event_driven, global_macro, relative_value")

    ALT_TOOL("managed_futures", "alt_managed_futures",
             "Analyze a managed futures / CTA fund. Returns trend-following metrics, crisis alpha, "
             "fee drag, and diversification verdict.",
             "Strategy type: 'trend_following', 'counter_trend', 'systematic_macro'")

    ALT_TOOL("market_neutral", "alt_market_neutral",
             "Analyze a market-neutral hedge fund. Returns beta exposure, factor loadings, "
             "leverage ratio, and market-neutrality verdict.",
             "Approach: 'statistical_arb', 'pairs_trading', 'factor_neutral'")

    // ── Commodities ──────────────────────────────────────────────────────────
    ALT_TOOL("precious_metals", "alt_precious_metals",
             "Analyze a precious metals investment. Returns basis analysis, contango/backwardation "
             "assessment, storage costs, and portfolio-hedge verdict.",
             "Metal: gold, silver, platinum, palladium")

    ALT_TOOL("natural_resources", "alt_natural_resources",
             "Analyze a natural resources / commodity investment. Returns sector classification, "
             "supply-demand dynamics, roll yield, and inflation-hedge verdict.",
             "Sector: energy, base_metals, agriculture, livestock")

    // ── Private Capital ──────────────────────────────────────────────────────
    ALT_TOOL("private_equity", "alt_private_equity",
             "Analyze a private equity or venture capital investment. Returns IRR, MOIC, DPI, "
             "RVPI, cash flow timeline, and risk-adjusted return verdict.",
             "Stage: buyout, venture, growth, mezzanine, distressed")

    // ── Annuities ────────────────────────────────────────────────────────────
    ALT_TOOL("fixed_annuity", "alt_fixed_annuity",
             "Analyze a fixed annuity contract. Returns payout rate, inflation erosion over term, "
             "mortality risk, and income-suitability verdict.",
             "Payout type: immediate, deferred, life_only, joint_life")

    ALT_TOOL("variable_annuity", "alt_variable_annuity",
             "Analyze a variable annuity. Returns fee drag (M&E + fund fees), tax implications, "
             "break-even horizon, and cost-efficiency verdict.",
             "Sub-account type: equity, bond, balanced, target_date")

    ALT_TOOL("equity_indexed_annuity", "alt_equity_indexed_annuity",
             "Analyze an equity-indexed annuity. Returns crediting rate, participation cap, "
             "surrender charge schedule, and upside/downside verdict.",
             "Crediting method: annual_point_to_point, monthly_sum, monthly_average")

    ALT_TOOL("inflation_annuity", "alt_inflation_annuity",
             "Analyze an inflation-indexed annuity. Returns real payout, comparison to TIPS, "
             "longevity value, and inflation-protection verdict.",
             "CPI linkage: full_cpi, partial_cpi, fixed_escalation")

    // ── Structured Products ──────────────────────────────────────────────────
    ALT_TOOL("structured_product", "alt_structured_note",
             "Analyze a structured note or principal-protected product. Returns complexity score, "
             "embedded cost, payoff profile, and suitability verdict.",
             "Structure type: principal_protected, reverse_convertible, autocallable, barrier")

    ALT_TOOL("leveraged_fund", "alt_leveraged_fund",
             "Analyze a leveraged or inverse ETF/fund. Returns volatility decay, daily rebalancing "
             "drag, holding-period erosion, and long-term suitability verdict.",
             "Leverage factor: 2x, 3x, -1x, -2x, -3x (as string)")

    // ── Inflation Protected ──────────────────────────────────────────────────
    ALT_TOOL("tips", "alt_tips",
             "Analyze a TIPS (Treasury Inflation-Protected Securities) investment. Returns real "
             "yield, inflation break-even, tax drag (phantom income), and real-return verdict.",
             "Maturity bucket: short (1-5yr), medium (5-10yr), long (10-30yr)")

    ALT_TOOL("i_bond", "alt_ibond",
             "Analyze I-Bond savings bonds. Returns composite rate, inflation adjustment, "
             "penalty-adjusted yield, and tax efficiency verdict.",
             "Holding period scenario: 1yr, 3yr, 5yr, 10yr, 30yr")

    ALT_TOOL("stable_value", "alt_stable_value_fund",
             "Analyze a stable value fund (401k/defined contribution). Returns crediting rate, "
             "market-to-book ratio, liquidity constraints, and capital-preservation verdict.",
             "Wrapper type: bank_wrap, insurance_wrap, synthetic")

    // ── Strategies ───────────────────────────────────────────────────────────
    ALT_TOOL("covered_call", "alt_covered_call",
             "Analyze a covered call options strategy. Returns premium income, upside cap, "
             "tax implications, opportunity cost, and income-enhancement verdict.",
             "Strike selection: atm, otm_5pct, otm_10pct, deep_otm")

    ALT_TOOL("sri_fund", "alt_sri_fund",
             "Analyze an SRI / ESG fund. Returns performance vs. conventional benchmark, "
             "screening methodology, expense ratio impact, and values-alignment verdict.",
             "Screen type: negative_screen, positive_screen, esg_integration, impact")

    ALT_TOOL("asset_location", "alt_asset_location",
             "Analyze tax-efficient asset location across account types. Returns after-tax "
             "return improvement, optimal placement recommendation, and tax-savings verdict.",
             "Account type: taxable, traditional_ira, roth_ira, 401k, hsa")

    ALT_TOOL("performance", "alt_performance_metrics",
             "Run comprehensive performance analysis on an investment. Returns time-weighted "
             "return, money-weighted return, Sharpe, Sortino, Calmar, and max drawdown.",
             "Benchmark: sp500, bonds, cash, custom")

    ALT_TOOL("risk", "alt_risk_analysis",
             "Run full risk analysis on an investment. Returns VaR (95/99%), CVaR, stress-test "
             "scenarios, correlation to major assets, and risk-profile verdict.",
             "Risk model: historical, parametric, monte_carlo")

    // ── Digital Assets ───────────────────────────────────────────────────────
    ALT_TOOL("digital_asset", "alt_digital_asset",
             "Analyze a cryptocurrency or digital asset investment. Returns on-chain metrics, "
             "volatility profile, correlation to traditional assets, and risk verdict.",
             "Asset type: bitcoin, ethereum, altcoin, stablecoin, defi_token")

    return tools;
}

} // namespace fincept::mcp::tools
