// MAAnalyticsTools.cpp — M&A Analytics tab MCP tools
// Covers all 8 modules: Valuation, Merger, Deal Structure, Deal Database,
// Startup, Fairness, Industry, Advanced Analytics, Deal Comparison

#include "mcp/tools/MAAnalyticsTools.h"

#include "core/logging/Logger.h"
#include "services/ma_analytics/MAAnalyticsService.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QTimer>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "MAAnalyticsTools";
static constexpr int kTimeoutMs = 30000;

// Helper: call an async MA service method, wait for result or error
static ToolResult run_ma_sync(const QString& context,
                               std::function<void()> trigger)
{
    QJsonObject result_data;
    QString error_msg;
    bool got_result = false;

    QEventLoop loop;
    auto& svc = fincept::services::ma::MAAnalyticsService::instance();

    QObject::connect(&svc, &fincept::services::ma::MAAnalyticsService::result_ready,
                     &loop, [&](const QString& ctx, const QJsonObject& data) {
                         if (ctx != context) return;
                         result_data = data;
                         got_result = true;
                         loop.quit();
                     });

    QObject::connect(&svc, &fincept::services::ma::MAAnalyticsService::error_occurred,
                     &loop, [&](const QString& ctx, const QString& msg) {
                         if (ctx != context) return;
                         error_msg = msg;
                         got_result = true;
                         loop.quit();
                     });

    QTimer::singleShot(kTimeoutMs, &loop, &QEventLoop::quit);

    trigger();
    loop.exec();

    if (!got_result)
        return ToolResult::fail("Timeout waiting for M&A result: " + context);
    if (!error_msg.isEmpty())
        return ToolResult::fail(error_msg);

    return ToolResult::ok_data(result_data);
}

std::vector<ToolDef> get_ma_analytics_tools() {
    std::vector<ToolDef> tools;

    // ════════════════════════════════════════════════════════════════════
    // VALUATION MODULE
    // ════════════════════════════════════════════════════════════════════

    // ── ma_dcf ──────────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_dcf";
        t.description = "Run a DCF (Discounted Cash Flow) valuation. Returns WACC, enterprise value, equity value, and per-share value.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"revenue", QJsonObject{{"type", "number"}, {"description", "Current revenue"}}},
            {"ebitda", QJsonObject{{"type", "number"}, {"description", "Current EBITDA"}}},
            {"growth_rate", QJsonObject{{"type", "number"}, {"description", "Revenue growth rate (decimal, e.g. 0.10)"}}},
            {"wacc", QJsonObject{{"type", "number"}, {"description", "Weighted average cost of capital (decimal)"}}},
            {"terminal_growth", QJsonObject{{"type", "number"}, {"description", "Terminal growth rate (decimal)"}}},
            {"net_debt", QJsonObject{{"type", "number"}, {"description", "Net debt"}}},
            {"shares_outstanding", QJsonObject{{"type", "number"}, {"description", "Shares outstanding"}}}};
        t.input_schema.required = {"revenue", "ebitda", "wacc", "terminal_growth"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("dcf", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_dcf(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_dcf_sensitivity ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_dcf_sensitivity";
        t.description = "Run a DCF sensitivity analysis varying WACC and terminal growth rate.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"base_params", QJsonObject{{"type", "object"}, {"description", "Base DCF parameters (same as ma_dcf)"}}},
            {"wacc_range", QJsonObject{{"type", "array"}, {"description", "Array of WACC values to test"}}},
            {"tgr_range", QJsonObject{{"type", "array"}, {"description", "Array of terminal growth rates to test"}}}};
        t.input_schema.required = {"base_params"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("dcf_sensitivity", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_dcf_sensitivity(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_lbo_returns ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_lbo_returns";
        t.description = "Calculate LBO returns (IRR and MOIC) given entry/exit assumptions.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"entry_ev", QJsonObject{{"type", "number"}, {"description", "Entry enterprise value"}}},
            {"exit_ev", QJsonObject{{"type", "number"}, {"description", "Exit enterprise value"}}},
            {"equity_invested", QJsonObject{{"type", "number"}, {"description", "Equity invested at entry"}}},
            {"hold_years", QJsonObject{{"type", "integer"}, {"description", "Holding period in years"}}},
            {"debt", QJsonObject{{"type", "number"}, {"description", "Initial debt"}}}};
        t.input_schema.required = {"entry_ev", "exit_ev", "equity_invested", "hold_years"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("lbo_returns", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_lbo_returns(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_lbo_model ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_lbo_model";
        t.description = "Build a full LBO model with debt tranches, cash sweeps, and exit analysis.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"purchase_price", QJsonObject{{"type", "number"}, {"description", "Total purchase price"}}},
            {"ebitda", QJsonObject{{"type", "number"}, {"description", "Entry EBITDA"}}},
            {"debt_multiple", QJsonObject{{"type", "number"}, {"description", "Debt/EBITDA multiple"}}},
            {"interest_rate", QJsonObject{{"type", "number"}, {"description", "Debt interest rate (decimal)"}}},
            {"exit_multiple", QJsonObject{{"type", "number"}, {"description", "Exit EV/EBITDA multiple"}}},
            {"hold_years", QJsonObject{{"type", "integer"}, {"description", "Holding period in years"}}}};
        t.input_schema.required = {"purchase_price", "ebitda", "debt_multiple", "exit_multiple", "hold_years"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("lbo_model", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().build_lbo_model(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_lbo_debt_schedule ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_lbo_debt_schedule";
        t.description = "Generate an LBO debt repayment schedule with amortization and cash sweeps.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"initial_debt", QJsonObject{{"type", "number"}, {"description", "Initial debt amount"}}},
            {"interest_rate", QJsonObject{{"type", "number"}, {"description", "Annual interest rate (decimal)"}}},
            {"ebitda", QJsonObject{{"type", "number"}, {"description", "Annual EBITDA"}}},
            {"capex", QJsonObject{{"type", "number"}, {"description", "Annual capex"}}},
            {"years", QJsonObject{{"type", "integer"}, {"description", "Schedule years"}}}};
        t.input_schema.required = {"initial_debt", "interest_rate", "ebitda", "years"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("lbo_debt_schedule", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().analyze_lbo_debt_schedule(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_lbo_sensitivity ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_lbo_sensitivity";
        t.description = "Run LBO sensitivity table varying entry multiple and exit multiple.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"base_params", QJsonObject{{"type", "object"}, {"description", "Base LBO parameters"}}},
            {"entry_multiples", QJsonObject{{"type", "array"}, {"description", "Entry EV/EBITDA multiples to test"}}},
            {"exit_multiples", QJsonObject{{"type", "array"}, {"description", "Exit EV/EBITDA multiples to test"}}}};
        t.input_schema.required = {"base_params"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("lbo_sensitivity", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_lbo_sensitivity(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_trading_comps ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_trading_comps";
        t.description = "Calculate trading comparable company multiples (EV/EBITDA, P/E, EV/Revenue).";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"target_revenue", QJsonObject{{"type", "number"}, {"description", "Target company revenue"}}},
            {"target_ebitda", QJsonObject{{"type", "number"}, {"description", "Target company EBITDA"}}},
            {"target_earnings", QJsonObject{{"type", "number"}, {"description", "Target company net earnings"}}},
            {"peer_ev_ebitda", QJsonObject{{"type", "number"}, {"description", "Peer median EV/EBITDA multiple"}}},
            {"peer_ev_revenue", QJsonObject{{"type", "number"}, {"description", "Peer median EV/Revenue multiple"}}},
            {"peer_pe", QJsonObject{{"type", "number"}, {"description", "Peer median P/E multiple"}}},
            {"net_debt", QJsonObject{{"type", "number"}, {"description", "Target net debt"}}}};
        t.input_schema.required = {"target_ebitda", "peer_ev_ebitda"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("trading_comps", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_trading_comps(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_precedent_transactions ───────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_precedent_transactions";
        t.description = "Value a target using precedent transaction multiples from comparable deals.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"target_ebitda", QJsonObject{{"type", "number"}, {"description", "Target EBITDA"}}},
            {"target_revenue", QJsonObject{{"type", "number"}, {"description", "Target revenue"}}},
            {"precedent_ev_ebitda", QJsonObject{{"type", "number"}, {"description", "Precedent median EV/EBITDA"}}},
            {"precedent_ev_revenue", QJsonObject{{"type", "number"}, {"description", "Precedent median EV/Revenue"}}},
            {"control_premium", QJsonObject{{"type", "number"}, {"description", "Control premium (decimal, e.g. 0.25)"}}}};
        t.input_schema.required = {"target_ebitda", "precedent_ev_ebitda"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("precedent_transactions", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_precedent_transactions(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_football_field ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_football_field";
        t.description = "Generate a football field valuation chart showing ranges across DCF, comps, and precedents.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"dcf_low", QJsonObject{{"type", "number"}, {"description", "DCF low value"}}},
            {"dcf_high", QJsonObject{{"type", "number"}, {"description", "DCF high value"}}},
            {"comps_low", QJsonObject{{"type", "number"}, {"description", "Trading comps low value"}}},
            {"comps_high", QJsonObject{{"type", "number"}, {"description", "Trading comps high value"}}},
            {"precedent_low", QJsonObject{{"type", "number"}, {"description", "Precedent transactions low"}}},
            {"precedent_high", QJsonObject{{"type", "number"}, {"description", "Precedent transactions high"}}},
            {"current_price", QJsonObject{{"type", "number"}, {"description", "Current share price (optional)"}}}};
        t.input_schema.required = {"dcf_low", "dcf_high", "comps_low", "comps_high"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("football_field", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().generate_football_field(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // MERGER ANALYSIS MODULE
    // ════════════════════════════════════════════════════════════════════

    // ── ma_merger_model ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_merger_model";
        t.description = "Build a full merger model combining acquirer and target financials.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"acquirer_revenue", QJsonObject{{"type", "number"}, {"description", "Acquirer revenue"}}},
            {"acquirer_ebitda", QJsonObject{{"type", "number"}, {"description", "Acquirer EBITDA"}}},
            {"acquirer_eps", QJsonObject{{"type", "number"}, {"description", "Acquirer EPS"}}},
            {"target_revenue", QJsonObject{{"type", "number"}, {"description", "Target revenue"}}},
            {"target_ebitda", QJsonObject{{"type", "number"}, {"description", "Target EBITDA"}}},
            {"deal_value", QJsonObject{{"type", "number"}, {"description", "Total deal value"}}},
            {"cash_pct", QJsonObject{{"type", "number"}, {"description", "Cash consideration % (0-1)"}}},
            {"synergies", QJsonObject{{"type", "number"}, {"description", "Expected annual synergies"}}}};
        t.input_schema.required = {"acquirer_eps", "target_ebitda", "deal_value"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("merger_model", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().build_merger_model(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_accretion_dilution ───────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_accretion_dilution";
        t.description = "Calculate EPS accretion/dilution from an acquisition.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"acquirer_eps", QJsonObject{{"type", "number"}, {"description", "Acquirer standalone EPS"}}},
            {"acquirer_shares", QJsonObject{{"type", "number"}, {"description", "Acquirer shares outstanding"}}},
            {"target_net_income", QJsonObject{{"type", "number"}, {"description", "Target net income"}}},
            {"deal_value", QJsonObject{{"type", "number"}, {"description", "Total deal value"}}},
            {"cash_pct", QJsonObject{{"type", "number"}, {"description", "Cash % of consideration (0-1)"}}},
            {"new_share_price", QJsonObject{{"type", "number"}, {"description", "Acquirer share price for stock issuance"}}},
            {"synergies_after_tax", QJsonObject{{"type", "number"}, {"description", "After-tax synergies"}}},
            {"financing_cost", QJsonObject{{"type", "number"}, {"description", "After-tax cost of debt financing"}}}};
        t.input_schema.required = {"acquirer_eps", "acquirer_shares", "target_net_income", "deal_value"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("accretion_dilution", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_accretion_dilution(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_pro_forma ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_pro_forma";
        t.description = "Build pro forma combined financials post-merger.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"acquirer_revenue", QJsonObject{{"type", "number"}, {"description", "Acquirer revenue"}}},
            {"acquirer_ebitda", QJsonObject{{"type", "number"}, {"description", "Acquirer EBITDA"}}},
            {"acquirer_net_income", QJsonObject{{"type", "number"}, {"description", "Acquirer net income"}}},
            {"target_revenue", QJsonObject{{"type", "number"}, {"description", "Target revenue"}}},
            {"target_ebitda", QJsonObject{{"type", "number"}, {"description", "Target EBITDA"}}},
            {"target_net_income", QJsonObject{{"type", "number"}, {"description", "Target net income"}}},
            {"revenue_synergies", QJsonObject{{"type", "number"}, {"description", "Revenue synergies"}}},
            {"cost_synergies", QJsonObject{{"type", "number"}, {"description", "Cost synergies"}}},
            {"integration_costs", QJsonObject{{"type", "number"}, {"description", "One-time integration costs"}}}};
        t.input_schema.required = {"acquirer_revenue", "target_revenue"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("pro_forma", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().build_pro_forma(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_sources_uses ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_sources_uses";
        t.description = "Build a sources and uses of funds table for a deal.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"equity_offered", QJsonObject{{"type", "number"}, {"description", "Equity consideration"}}},
            {"cash_offered", QJsonObject{{"type", "number"}, {"description", "Cash consideration"}}},
            {"debt_raised", QJsonObject{{"type", "number"}, {"description", "New debt raised"}}},
            {"target_debt_assumed", QJsonObject{{"type", "number"}, {"description", "Target debt assumed"}}},
            {"fees", QJsonObject{{"type", "number"}, {"description", "Advisory and financing fees"}}}};
        t.input_schema.required = {"equity_offered", "cash_offered"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("sources_uses", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_sources_uses(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_contribution_analysis ────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_contribution_analysis";
        t.description = "Analyze relative contribution of acquirer and target to combined entity.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"acquirer_revenue", QJsonObject{{"type", "number"}, {"description", "Acquirer revenue"}}},
            {"acquirer_ebitda", QJsonObject{{"type", "number"}, {"description", "Acquirer EBITDA"}}},
            {"acquirer_market_cap", QJsonObject{{"type", "number"}, {"description", "Acquirer market cap"}}},
            {"target_revenue", QJsonObject{{"type", "number"}, {"description", "Target revenue"}}},
            {"target_ebitda", QJsonObject{{"type", "number"}, {"description", "Target EBITDA"}}},
            {"target_deal_value", QJsonObject{{"type", "number"}, {"description", "Target deal value"}}}};
        t.input_schema.required = {"acquirer_revenue", "target_revenue", "acquirer_market_cap", "target_deal_value"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("contribution_analysis", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().analyze_contribution(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_revenue_synergies ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_revenue_synergies";
        t.description = "Model and value revenue synergies from a merger.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"combined_revenue", QJsonObject{{"type", "number"}, {"description", "Combined revenue baseline"}}},
            {"synergy_pct", QJsonObject{{"type", "number"}, {"description", "Expected synergy as % of revenue"}}},
            {"ramp_years", QJsonObject{{"type", "integer"}, {"description", "Years to full synergy realization"}}},
            {"discount_rate", QJsonObject{{"type", "number"}, {"description", "Discount rate for NPV (decimal)"}}}};
        t.input_schema.required = {"combined_revenue", "synergy_pct"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("revenue_synergies", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_revenue_synergies(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_cost_synergies ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_cost_synergies";
        t.description = "Model and value cost synergies from a merger.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"combined_opex", QJsonObject{{"type", "number"}, {"description", "Combined operating expenses"}}},
            {"synergy_pct", QJsonObject{{"type", "number"}, {"description", "Expected cost reduction %"}}},
            {"ramp_years", QJsonObject{{"type", "integer"}, {"description", "Years to full realization"}}},
            {"discount_rate", QJsonObject{{"type", "number"}, {"description", "Discount rate for NPV (decimal)"}}}};
        t.input_schema.required = {"combined_opex", "synergy_pct"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("cost_synergies", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_cost_synergies(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_synergies_dcf ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_synergies_dcf";
        t.description = "Calculate NPV of combined revenue and cost synergies using DCF.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"annual_synergies", QJsonObject{{"type", "number"}, {"description", "Annual synergy amount"}}},
            {"ramp_years", QJsonObject{{"type", "integer"}, {"description", "Years to full realization"}}},
            {"discount_rate", QJsonObject{{"type", "number"}, {"description", "Discount rate (decimal)"}}},
            {"tax_rate", QJsonObject{{"type", "number"}, {"description", "Tax rate (decimal)"}}},
            {"terminal_growth", QJsonObject{{"type", "number"}, {"description", "Terminal growth rate (decimal)"}}}};
        t.input_schema.required = {"annual_synergies", "discount_rate"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("synergies_dcf", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().value_synergies_dcf(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_integration_costs ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_integration_costs";
        t.description = "Estimate post-merger integration costs (restructuring, IT, HR, etc.).";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"deal_value", QJsonObject{{"type", "number"}, {"description", "Total deal value"}}},
            {"combined_revenue", QJsonObject{{"type", "number"}, {"description", "Combined revenue"}}},
            {"combined_headcount", QJsonObject{{"type", "integer"}, {"description", "Combined employee headcount"}}},
            {"integration_complexity", QJsonObject{{"type", "string"}, {"description", "LOW, MEDIUM, HIGH"}}}};
        t.input_schema.required = {"deal_value"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("integration_costs", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().estimate_integration_costs(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // DEAL STRUCTURE MODULE
    // ════════════════════════════════════════════════════════════════════

    // ── ma_payment_structure ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_payment_structure";
        t.description = "Analyze deal payment structure (cash vs stock mix, tax implications).";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"deal_value", QJsonObject{{"type", "number"}, {"description", "Total deal value"}}},
            {"cash_pct", QJsonObject{{"type", "number"}, {"description", "Cash % (0-1)"}}},
            {"stock_pct", QJsonObject{{"type", "number"}, {"description", "Stock % (0-1)"}}},
            {"acquirer_shares_outstanding", QJsonObject{{"type", "number"}, {"description", "Acquirer shares outstanding"}}},
            {"acquirer_share_price", QJsonObject{{"type", "number"}, {"description", "Acquirer share price"}}},
            {"target_shares_outstanding", QJsonObject{{"type", "number"}, {"description", "Target shares outstanding"}}}};
        t.input_schema.required = {"deal_value", "cash_pct"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("payment_structure", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().analyze_payment_structure(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_earnout ──────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_earnout";
        t.description = "Value an earnout provision using probability-weighted scenarios.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"max_earnout", QJsonObject{{"type", "number"}, {"description", "Maximum earnout payment"}}},
            {"target_metric", QJsonObject{{"type", "number"}, {"description", "Target metric threshold (e.g. revenue)"}}},
            {"probability_achieve", QJsonObject{{"type", "number"}, {"description", "Probability of achieving target (0-1)"}}},
            {"discount_rate", QJsonObject{{"type", "number"}, {"description", "Discount rate (decimal)"}}},
            {"years_to_payment", QJsonObject{{"type", "integer"}, {"description", "Years until earnout payment"}}}};
        t.input_schema.required = {"max_earnout", "probability_achieve", "discount_rate", "years_to_payment"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("earnout", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().value_earnout(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_exchange_ratio ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_exchange_ratio";
        t.description = "Calculate stock-for-stock exchange ratio and resulting ownership split.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"offer_price_per_target_share", QJsonObject{{"type", "number"}, {"description", "Offer price per target share"}}},
            {"acquirer_share_price", QJsonObject{{"type", "number"}, {"description", "Acquirer share price"}}},
            {"target_shares_outstanding", QJsonObject{{"type", "number"}, {"description", "Target shares outstanding"}}},
            {"acquirer_shares_outstanding", QJsonObject{{"type", "number"}, {"description", "Acquirer shares outstanding"}}}};
        t.input_schema.required = {"offer_price_per_target_share", "acquirer_share_price", "target_shares_outstanding"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("exchange_ratio", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_exchange_ratio(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_collar_mechanism ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_collar_mechanism";
        t.description = "Analyze a collar mechanism protecting buyer/seller from price swings.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"fixed_exchange_ratio", QJsonObject{{"type", "number"}, {"description", "Base exchange ratio"}}},
            {"collar_floor", QJsonObject{{"type", "number"}, {"description", "Floor acquirer share price"}}},
            {"collar_ceiling", QJsonObject{{"type", "number"}, {"description", "Ceiling acquirer share price"}}},
            {"acquirer_share_price", QJsonObject{{"type", "number"}, {"description", "Current acquirer share price"}}},
            {"target_shares_outstanding", QJsonObject{{"type", "number"}, {"description", "Target shares outstanding"}}}};
        t.input_schema.required = {"fixed_exchange_ratio", "collar_floor", "collar_ceiling", "acquirer_share_price"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("collar_mechanism", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().analyze_collar_mechanism(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_cvr ──────────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_cvr";
        t.description = "Value a Contingent Value Right (CVR) tied to milestone achievement.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"cvr_payment", QJsonObject{{"type", "number"}, {"description", "CVR payment if milestone achieved"}}},
            {"probability", QJsonObject{{"type", "number"}, {"description", "Probability of milestone (0-1)"}}},
            {"time_to_milestone", QJsonObject{{"type", "number"}, {"description", "Years to milestone"}}},
            {"discount_rate", QJsonObject{{"type", "number"}, {"description", "Discount rate (decimal)"}}},
            {"shares_outstanding", QJsonObject{{"type", "number"}, {"description", "Shares outstanding for per-share value"}}}};
        t.input_schema.required = {"cvr_payment", "probability", "time_to_milestone", "discount_rate"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("cvr", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().value_cvr(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // DEAL DATABASE MODULE
    // ════════════════════════════════════════════════════════════════════

    // ── ma_get_deals ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_get_deals";
        t.description = "Get all deals from the M&A deal database.";
        t.category = "ma-analytics";
        t.handler = [](const QJsonObject&) -> ToolResult {
            return run_ma_sync("get_all_deals", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().get_all_deals();
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_search_deals ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_search_deals";
        t.description = "Search the deal database by company name, industry, or deal type.";
        t.category = "ma-analytics";
        t.input_schema.properties =
            QJsonObject{{"query", QJsonObject{{"type", "string"}, {"description", "Search query"}}}};
        t.input_schema.required = {"query"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString query = args["query"].toString().trimmed();
            if (query.isEmpty())
                return ToolResult::fail("Missing 'query'");
            return run_ma_sync("search_deals", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().search_deals(query);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_scan_filings ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_scan_filings";
        t.description = "Scan SEC EDGAR filings for M&A activity in the last N days.";
        t.category = "ma-analytics";
        t.input_schema.properties =
            QJsonObject{{"days_back", QJsonObject{{"type", "integer"}, {"description", "Days to look back (default: 30)"}}}};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int days = args["days_back"].toInt(30);
            return run_ma_sync("scan_filings", [days]() {
                fincept::services::ma::MAAnalyticsService::instance().scan_filings(days);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_create_deal ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_create_deal";
        t.description = "Add a new deal record to the M&A deal database.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"acquirer", QJsonObject{{"type", "string"}, {"description", "Acquirer company name"}}},
            {"target", QJsonObject{{"type", "string"}, {"description", "Target company name"}}},
            {"deal_value", QJsonObject{{"type", "number"}, {"description", "Deal value in millions"}}},
            {"deal_type", QJsonObject{{"type", "string"}, {"description", "Acquisition, Merger, LBO, etc."}}},
            {"industry", QJsonObject{{"type", "string"}, {"description", "Industry sector"}}},
            {"announced_date", QJsonObject{{"type", "string"}, {"description", "Announcement date YYYY-MM-DD"}}},
            {"status", QJsonObject{{"type", "string"}, {"description", "Announced, Pending, Completed, Failed"}}}};
        t.input_schema.required = {"acquirer", "target", "deal_value"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("create_deal", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().create_deal(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_update_deal ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_update_deal";
        t.description = "Update an existing deal record in the M&A database.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"deal_id", QJsonObject{{"type", "string"}, {"description", "Deal ID to update"}}},
            {"updates", QJsonObject{{"type", "object"}, {"description", "Fields to update (status, deal_value, etc.)"}}}};
        t.input_schema.required = {"deal_id", "updates"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString deal_id = args["deal_id"].toString().trimmed();
            if (deal_id.isEmpty())
                return ToolResult::fail("Missing 'deal_id'");
            QJsonObject updates = args["updates"].toObject();
            return run_ma_sync("update_deal", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().update_deal(deal_id, updates);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_parse_filing ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_parse_filing";
        t.description = "Parse an SEC filing URL to extract M&A deal indicators and key terms.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"filing_url", QJsonObject{{"type", "string"}, {"description", "SEC EDGAR filing URL"}}},
            {"filing_type", QJsonObject{{"type", "string"}, {"description", "Filing type: SC13D, 8-K, S-4, DEFM14A"}}}};
        t.input_schema.required = {"filing_url", "filing_type"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString url = args["filing_url"].toString().trimmed();
            QString type = args["filing_type"].toString().trimmed();
            if (url.isEmpty() || type.isEmpty())
                return ToolResult::fail("Missing 'filing_url' or 'filing_type'");
            return run_ma_sync("parse_filing", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().parse_filing(url, type);
            });
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // STARTUP VALUATION MODULE
    // ════════════════════════════════════════════════════════════════════

    // ── ma_startup_berkus ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_startup_berkus";
        t.description = "Value a pre-revenue startup using the Berkus method (5 qualitative factors, max $2.5M each).";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"sound_idea", QJsonObject{{"type", "number"}, {"description", "Score 0-2.5M: sound idea / basic value"}}},
            {"prototype", QJsonObject{{"type", "number"}, {"description", "Score 0-2.5M: prototype / technology"}}},
            {"quality_team", QJsonObject{{"type", "number"}, {"description", "Score 0-2.5M: quality management team"}}},
            {"strategic_relationships", QJsonObject{{"type", "number"}, {"description", "Score 0-2.5M: strategic relationships"}}},
            {"product_rollout", QJsonObject{{"type", "number"}, {"description", "Score 0-2.5M: product rollout / sales"}}}};
        t.input_schema.required = {"sound_idea", "prototype", "quality_team"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("berkus", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_berkus(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_startup_vc ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_startup_vc";
        t.description = "Value a startup using the VC method (terminal value discounted back to today).";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"projected_revenue", QJsonObject{{"type", "number"}, {"description", "Projected revenue at exit"}}},
            {"exit_multiple", QJsonObject{{"type", "number"}, {"description", "Exit revenue or earnings multiple"}}},
            {"target_ror", QJsonObject{{"type", "number"}, {"description", "Target rate of return (decimal, e.g. 0.40)"}}},
            {"years_to_exit", QJsonObject{{"type", "integer"}, {"description", "Years until exit"}}},
            {"investment_needed", QJsonObject{{"type", "number"}, {"description", "Investment amount needed"}}}};
        t.input_schema.required = {"projected_revenue", "exit_multiple", "target_ror", "years_to_exit"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("vc_method", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_vc_method(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_startup_scorecard ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_startup_scorecard";
        t.description = "Value a startup using the Scorecard method (weighted comparison to median pre-money).";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"median_pre_money", QJsonObject{{"type", "number"}, {"description", "Median pre-money for comparable startups"}}},
            {"team_score", QJsonObject{{"type", "number"}, {"description", "Team strength multiplier (0.5-1.5)"}}},
            {"opportunity_score", QJsonObject{{"type", "number"}, {"description", "Market opportunity multiplier"}}},
            {"product_score", QJsonObject{{"type", "number"}, {"description", "Product/technology multiplier"}}},
            {"competition_score", QJsonObject{{"type", "number"}, {"description", "Competitive environment multiplier"}}},
            {"marketing_score", QJsonObject{{"type", "number"}, {"description", "Marketing/sales channels multiplier"}}}};
        t.input_schema.required = {"median_pre_money", "team_score", "opportunity_score"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("scorecard", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_scorecard(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_startup_first_chicago ────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_startup_first_chicago";
        t.description = "Value a startup using the First Chicago method (3 probability-weighted scenarios).";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"success_value", QJsonObject{{"type", "number"}, {"description", "Value in success scenario"}}},
            {"base_value", QJsonObject{{"type", "number"}, {"description", "Value in base scenario"}}},
            {"failure_value", QJsonObject{{"type", "number"}, {"description", "Value in failure scenario"}}},
            {"success_prob", QJsonObject{{"type", "number"}, {"description", "Probability of success (0-1)"}}},
            {"base_prob", QJsonObject{{"type", "number"}, {"description", "Probability of base (0-1)"}}},
            {"discount_rate", QJsonObject{{"type", "number"}, {"description", "Discount rate (decimal)"}}}};
        t.input_schema.required = {"success_value", "base_value", "failure_value", "success_prob", "base_prob"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("first_chicago", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_first_chicago(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_startup_risk_factor ──────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_startup_risk_factor";
        t.description = "Value a startup using the Risk Factor Summation method (12 risk adjustments).";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"base_value", QJsonObject{{"type", "number"}, {"description", "Base median comparable valuation"}}},
            {"risk_scores", QJsonObject{{"type", "object"}, {"description", "Risk scores -2 to +2 for: management, stage, legislation, manufacturing, sales, funding, competition, technology, litigation, international, reputation, exit"}}}};
        t.input_schema.required = {"base_value", "risk_scores"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("risk_factor", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_risk_factor(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_startup_comprehensive ────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_startup_comprehensive";
        t.description = "Run all startup valuation methods and return a weighted composite value.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"params", QJsonObject{{"type", "object"}, {"description", "Combined params for all startup methods"}}}};
        t.input_schema.required = {"params"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("comprehensive_startup", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_comprehensive_startup(
                    args["params"].toObject());
            });
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // FAIRNESS OPINION MODULE
    // ════════════════════════════════════════════════════════════════════

    // ── ma_fairness_opinion ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_fairness_opinion";
        t.description = "Generate a fairness opinion — concludes whether the deal price is fair to shareholders.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"offer_price", QJsonObject{{"type", "number"}, {"description", "Offer price per share"}}},
            {"dcf_value", QJsonObject{{"type", "number"}, {"description", "DCF per-share value"}}},
            {"comps_value", QJsonObject{{"type", "number"}, {"description", "Trading comps per-share value"}}},
            {"precedent_value", QJsonObject{{"type", "number"}, {"description", "Precedent transactions per-share value"}}},
            {"52w_high", QJsonObject{{"type", "number"}, {"description", "52-week high price"}}},
            {"52w_low", QJsonObject{{"type", "number"}, {"description", "52-week low price"}}}};
        t.input_schema.required = {"offer_price", "dcf_value", "comps_value"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("fairness_opinion", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().generate_fairness_opinion(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_premium_analysis ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_premium_analysis";
        t.description = "Analyze acquisition premium over multiple look-back periods (1d, 1w, 1m, 3m).";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"offer_price", QJsonObject{{"type", "number"}, {"description", "Offer price per share"}}},
            {"price_1d", QJsonObject{{"type", "number"}, {"description", "Stock price 1 day before announcement"}}},
            {"price_1w", QJsonObject{{"type", "number"}, {"description", "Stock price 1 week before"}}},
            {"price_1m", QJsonObject{{"type", "number"}, {"description", "Stock price 1 month before"}}},
            {"price_3m", QJsonObject{{"type", "number"}, {"description", "Stock price 3 months before"}}}};
        t.input_schema.required = {"offer_price", "price_1d"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("premium_analysis", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().analyze_premium(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_process_quality ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_process_quality";
        t.description = "Assess the quality of the M&A sale process (board process, market check, advisors).";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"num_bidders_contacted", QJsonObject{{"type", "integer"}, {"description", "Number of parties contacted"}}},
            {"num_bidders_participated", QJsonObject{{"type", "integer"}, {"description", "Number that submitted bids"}}},
            {"go_shop_period", QJsonObject{{"type", "integer"}, {"description", "Go-shop period in days (0 if none)"}}},
            {"market_check_conducted", QJsonObject{{"type", "boolean"}, {"description", "Was a market check conducted?"}}},
            {"independent_committee", QJsonObject{{"type", "boolean"}, {"description", "Was an independent committee formed?"}}},
            {"financial_advisor_engaged", QJsonObject{{"type", "boolean"}, {"description", "Was a financial advisor engaged?"}}}};
        t.input_schema.required = {"num_bidders_contacted", "market_check_conducted"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("process_quality", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().assess_process_quality(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // INDUSTRY METRICS MODULE
    // ════════════════════════════════════════════════════════════════════

    // ── ma_tech_metrics ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_tech_metrics";
        t.description = "Calculate technology sector M&A metrics (SaaS, Marketplace, Semiconductor).";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"sub_sector", QJsonObject{{"type", "string"}, {"description", "SAAS, MARKETPLACE, SEMICONDUCTOR"}}},
            {"arr", QJsonObject{{"type", "number"}, {"description", "Annual Recurring Revenue (SaaS)"}}},
            {"gmv", QJsonObject{{"type", "number"}, {"description", "Gross Merchandise Value (Marketplace)"}}},
            {"revenue", QJsonObject{{"type", "number"}, {"description", "Total revenue"}}},
            {"growth_rate", QJsonObject{{"type", "number"}, {"description", "YoY growth rate (decimal)"}}},
            {"gross_margin", QJsonObject{{"type", "number"}, {"description", "Gross margin (decimal)"}}}};
        t.input_schema.required = {"sub_sector", "revenue"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("tech_metrics", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_tech_metrics(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_healthcare_metrics ───────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_healthcare_metrics";
        t.description = "Calculate healthcare sector M&A metrics (Pharma, Biotech, Medical Devices).";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"sub_sector", QJsonObject{{"type", "string"}, {"description", "PHARMA, BIOTECH, DEVICES"}}},
            {"revenue", QJsonObject{{"type", "number"}, {"description", "Revenue"}}},
            {"pipeline_value", QJsonObject{{"type", "number"}, {"description", "Estimated pipeline NPV (Biotech)"}}},
            {"rd_spend", QJsonObject{{"type", "number"}, {"description", "R&D spending"}}},
            {"ebitda", QJsonObject{{"type", "number"}, {"description", "EBITDA"}}}};
        t.input_schema.required = {"sub_sector", "revenue"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("healthcare_metrics", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_healthcare_metrics(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_financial_metrics ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_financial_metrics";
        t.description = "Calculate financial services sector M&A metrics (Banking, Insurance, Asset Management).";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"sub_sector", QJsonObject{{"type", "string"}, {"description", "BANKING, INSURANCE, ASSET_MANAGEMENT"}}},
            {"revenue", QJsonObject{{"type", "number"}, {"description", "Revenue"}}},
            {"aum", QJsonObject{{"type", "number"}, {"description", "Assets under management (Asset Mgmt)"}}},
            {"book_value", QJsonObject{{"type", "number"}, {"description", "Book value (Banking)"}}},
            {"net_income", QJsonObject{{"type", "number"}, {"description", "Net income"}}}};
        t.input_schema.required = {"sub_sector", "revenue"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("financial_services_metrics", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().calculate_financial_services_metrics(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // ADVANCED ANALYTICS MODULE
    // ════════════════════════════════════════════════════════════════════

    // ── ma_monte_carlo ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_monte_carlo";
        t.description = "Run a Monte Carlo simulation on deal value with configurable distributions (1K-100K runs).";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"base_value", QJsonObject{{"type", "number"}, {"description", "Base case deal/equity value"}}},
            {"std_dev_pct", QJsonObject{{"type", "number"}, {"description", "Standard deviation as % of base value"}}},
            {"num_simulations", QJsonObject{{"type", "integer"}, {"description", "Number of simulations (default: 10000)"}}},
            {"revenue_growth_mean", QJsonObject{{"type", "number"}, {"description", "Mean revenue growth assumption"}}},
            {"revenue_growth_std", QJsonObject{{"type", "number"}, {"description", "Std dev of revenue growth"}}},
            {"wacc_mean", QJsonObject{{"type", "number"}, {"description", "Mean WACC"}}},
            {"wacc_std", QJsonObject{{"type", "number"}, {"description", "Std dev of WACC"}}}};
        t.input_schema.required = {"base_value", "std_dev_pct"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("monte_carlo", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().run_monte_carlo(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_regression ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_regression";
        t.description = "Run OLS or multiple regression analysis on deal/financial data.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"y_values", QJsonObject{{"type", "array"}, {"description", "Dependent variable values"}}},
            {"x_values", QJsonObject{{"type", "array"}, {"description", "Independent variable(s) — array of arrays for multiple regression"}}},
            {"regression_type", QJsonObject{{"type", "string"}, {"description", "OLS or MULTIPLE (default: OLS)"}}},
            {"variable_names", QJsonObject{{"type", "array"}, {"description", "Names for independent variables (optional)"}}}};
        t.input_schema.required = {"y_values", "x_values"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("regression", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().run_regression(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // DEAL COMPARISON MODULE
    // ════════════════════════════════════════════════════════════════════

    // ── ma_compare_deals ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_compare_deals";
        t.description = "Side-by-side comparison of multiple M&A deals by key metrics.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"deals", QJsonObject{{"type", "array"}, {"description", "Array of deal objects with fields: name, ev, ebitda, revenue, premium, payment_type"}}}};
        t.input_schema.required = {"deals"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("compare_deals", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().compare_deals(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_rank_deals ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_rank_deals";
        t.description = "Rank deals by a specified metric (deal_value, premium, ev_ebitda, etc.).";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"deals", QJsonObject{{"type", "array"}, {"description", "Array of deal objects"}}},
            {"rank_by", QJsonObject{{"type", "string"}, {"description", "Metric to rank by: deal_value, premium, ev_ebitda, ev_revenue"}}},
            {"ascending", QJsonObject{{"type", "boolean"}, {"description", "Sort ascending (default: false)"}}}};
        t.input_schema.required = {"deals", "rank_by"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("rank_deals", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().rank_deals(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_benchmark_premium ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_benchmark_premium";
        t.description = "Benchmark a deal premium against historical M&A premiums by industry and deal size.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"premium_pct", QJsonObject{{"type", "number"}, {"description", "Deal premium percentage"}}},
            {"industry", QJsonObject{{"type", "string"}, {"description", "Industry sector"}}},
            {"deal_size_bucket", QJsonObject{{"type", "string"}, {"description", "SMALL (<$500M), MID ($500M-$5B), LARGE (>$5B)"}}}};
        t.input_schema.required = {"premium_pct"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("benchmark_premium", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().benchmark_deal_premium(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_payment_structures_analysis ─────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_payment_structures_analysis";
        t.description = "Analyze payment structure trends across a set of comparable deals.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"deals", QJsonObject{{"type", "array"}, {"description", "Array of deals with payment_type, cash_pct, stock_pct fields"}}}};
        t.input_schema.required = {"deals"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("payment_structures_analysis", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().analyze_payment_structures(args);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_industry_deals ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_industry_deals";
        t.description = "Analyze M&A deal activity and trends by industry sector.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"industry", QJsonObject{{"type", "string"}, {"description", "Industry to analyze"}}},
            {"years", QJsonObject{{"type", "integer"}, {"description", "Years of history to analyze (default: 5)"}}}};
        t.input_schema.required = {"industry"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("industry_deals", [&]() {
                fincept::services::ma::MAAnalyticsService::instance().analyze_industry_deals(args);
            });
        };
        tools.push_back(std::move(t));
    }

    LOG_DEBUG(TAG, QString("Registered %1 MA Analytics tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
