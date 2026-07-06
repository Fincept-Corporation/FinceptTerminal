// MAAnalyticsTools.cpp — M&A Analytics tab MCP tools
// Covers all 8 modules: Valuation, Merger, Deal Structure, Deal Database,
// Startup, Fairness, Industry, Advanced Analytics, Deal Comparison

#include "mcp/tools/MAAnalyticsTools.h"

#include "core/logging/Logger.h"
#include "mcp/AsyncDispatch.h"
#include "mcp/tools/ThreadHelper.h"
#include "services/ma_analytics/MAAnalyticsService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QPromise>

#include <memory>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "MAAnalyticsTools";

// Phase 4 — both shapes provided:
//
//   run_ma_sync   — blocks worker thread until svc emits result_ready/error.
//                   Pre-Phase-4 default; 46 tools use this. Each can migrate
//                   to async incrementally by switching its handler to
//                   t.async_handler and calling run_ma_async.
//
//   run_ma_async  — non-blocking. Invokes `trigger` on the MA service's
//                   thread, hooks result_ready/error_occurred for the given
//                   context, resolves the promise when either fires.
//                   Cancellation observed via ctx.cancelled before resolve.
//                   Trigger MUST capture inputs by value — it runs on the
//                   service thread after the calling lambda has returned.
static ToolResult run_ma_sync(const QString& context, std::function<void()> trigger) {
    QJsonObject result_data;
    QString error_msg;
    bool got_result = false;

    auto& svc = fincept::services::ma::MAAnalyticsService::instance();

    detail::run_async_wait(&svc, [&](auto signal_done) {
        auto* gate = new QObject;
        QObject::connect(&svc, &fincept::services::ma::MAAnalyticsService::result_ready, gate,
                         [&, gate, signal_done](const QString& ctx, const QJsonObject& data) {
                             if (ctx != context)
                                 return;
                             result_data = data;
                             got_result = true;
                             gate->deleteLater();
                             signal_done();
                         });
        QObject::connect(&svc, &fincept::services::ma::MAAnalyticsService::error_occurred, gate,
                         [&, gate, signal_done](const QString& ctx, const QString& msg) {
                             if (ctx != context)
                                 return;
                             error_msg = msg;
                             got_result = true;
                             gate->deleteLater();
                             signal_done();
                         });
        trigger();
    });

    if (!got_result)
        return ToolResult::fail("M&A result missing: " + context);
    if (!error_msg.isEmpty())
        return ToolResult::fail(error_msg);
    return ToolResult::ok_data(result_data);
}

[[maybe_unused]]
static void run_ma_async(const QString& context, std::function<void()> trigger, ToolContext ctx,
                         std::shared_ptr<QPromise<ToolResult>> promise) {
    auto* svc = &fincept::services::ma::MAAnalyticsService::instance();

    AsyncDispatch::callback_to_promise(
        svc, ctx, promise, [svc, context, trigger = std::move(trigger), ctx](auto resolve) {
            auto* gate = new QObject;
            QObject::connect(svc, &fincept::services::ma::MAAnalyticsService::result_ready, gate,
                             [gate, context, resolve, ctx](const QString& c, const QJsonObject& data) {
                                 if (c != context)
                                     return;
                                 if (ctx.cancelled())
                                     resolve(ToolResult::fail("cancelled"));
                                 else
                                     resolve(ToolResult::ok_data(data));
                                 gate->deleteLater();
                             });
            QObject::connect(svc, &fincept::services::ma::MAAnalyticsService::error_occurred, gate,
                             [gate, context, resolve](const QString& c, const QString& msg) {
                                 if (c != context)
                                     return;
                                 resolve(ToolResult::fail(msg));
                                 gate->deleteLater();
                             });
            trigger();
        });
}

// Compact registration for the analytics library wrappers (skfolio, options,
// valuation, …). Each forwards its args straight to the Python analytic and
// waits on `context`, which matches the service method's run_python_json result
// context (so results route unambiguously).
using MaMethod = void (fincept::services::ma::MAAnalyticsService::*)(const QJsonObject&);
static ToolDef make_ma_tool(const char* name, const char* desc, const QString& context, MaMethod method,
                            QJsonObject props, QStringList required) {
    ToolDef t;
    t.name = name;
    t.description = desc;
    t.category = "ma-analytics";
    t.input_schema.properties = std::move(props);
    t.input_schema.required = std::move(required);
    t.handler = [method, context](const QJsonObject& args) -> ToolResult {
        return run_ma_sync(context,
                           [&, method]() { (fincept::services::ma::MAAnalyticsService::instance().*method)(args); });
    };
    return t;
}

// Variant for the multi-command dispatcher methods (portfolio management): binds
// a fixed `command` and forwards the tool args as that command's params.
using MaCmdMethod = void (fincept::services::ma::MAAnalyticsService::*)(const QString&, const QJsonObject&);
static ToolDef make_ma_cmd_tool(const char* name, const char* desc, const QString& context, MaCmdMethod method,
                                const QString& command, QJsonObject props, QStringList required) {
    ToolDef t;
    t.name = name;
    t.description = desc;
    t.category = "ma-analytics";
    t.input_schema.properties = std::move(props);
    t.input_schema.required = std::move(required);
    t.handler = [method, context, command](const QJsonObject& args) -> ToolResult {
        return run_ma_sync(context, [&, method, command]() {
            (fincept::services::ma::MAAnalyticsService::instance().*method)(command, args);
        });
    };
    return t;
}

// The keyless data-connector manifest (auto-generated). Defines kDataConnectorManifest.
#include "mcp/tools/DataConnectorManifest.inc"

// Build one dispatcher tool per keyless `scripts/*_data.py` connector from the
// embedded manifest. Each connector shares the uniform positional ABI
// `python <script> <command> [args…] → JSON`; the tool exposes the command as an
// enum and forwards optional positional args. Category "data-connectors".
static void append_data_connector_tools(std::vector<ToolDef>& tools) {
    using MASvc = fincept::services::ma::MAAnalyticsService;
    const auto doc = QJsonDocument::fromJson(QByteArray(kDataConnectorManifest));
    const QJsonArray entries = doc.array();
    for (const auto& ev : entries) {
        const QJsonObject e = ev.toObject();
        const QString name = e.value("name").toString();
        const QString script = e.value("script").toString();
        const QString base_desc = e.value("desc").toString();
        const QJsonArray commands = e.value("commands").toArray();
        const QJsonArray env_keys = e.value("env_keys").toArray();
        if (name.isEmpty() || script.isEmpty())
            continue;

        QStringList clist;
        for (const auto& c : commands)
            clist << c.toString();
        const QString base_ctx = QStringLiteral("dc_") + name;

        QString key_note;
        if (!env_keys.isEmpty()) {
            QStringList keys;
            for (const auto& k : env_keys)
                keys << k.toString();
            key_note = QStringLiteral(" Requires an API key (") + keys.join(QStringLiteral(", ")) +
                       QStringLiteral(") set in Settings › Credentials; without it the connector returns a "
                                      "missing-key error.");
        }

        ToolDef t;
        t.name = name;
        t.description = base_desc + QStringLiteral(" Data connector; pick an endpoint. Endpoints: ") +
                        clist.join(QStringLiteral(", ")) + QStringLiteral(".") + key_note;
        t.category = QStringLiteral("data-connectors");
        QJsonObject cmd_prop{{"type", "string"},
                             {"description", QStringLiteral("Endpoint/command to call for this connector.")}};
        if (!commands.isEmpty())
            cmd_prop.insert("enum", commands);
        t.input_schema.properties = QJsonObject{
            {"command", cmd_prop},
            {"args", QJsonObject{{"type", "array"},
                                 {"items", QJsonObject{{"type", "string"}}},
                                 {"description", "Optional positional arguments after the command (strings), such as "
                                                 "a country code or date range, if the endpoint requires them."}}}};
        t.input_schema.required = {"command"};
        t.handler = [script, base_ctx](const QJsonObject& args) -> ToolResult {
            const QString command = args.value("command").toString();
            if (command.isEmpty())
                return ToolResult::fail("'command' is required");
            QStringList full;
            full << command;
            for (const auto& v : args.value("args").toArray())
                full << v.toVariant().toString();
            const QString ctx = base_ctx + QStringLiteral("_") + command;
            return run_ma_sync(ctx, [&]() { MASvc::instance().run_data_connector(script, full, ctx); });
        };
        tools.push_back(std::move(t));
    }
    LOG_DEBUG(TAG, QString("Registered %1 data-connector tools").arg(entries.size()));
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
        t.description = "Run a DCF (Discounted Cash Flow) valuation. Returns WACC, enterprise value, equity value, and "
                        "per-share value.";
        t.category = "ma-analytics";
        t.input_schema.properties = QJsonObject{
            {"revenue", QJsonObject{{"type", "number"}, {"description", "Current revenue"}}},
            {"ebitda", QJsonObject{{"type", "number"}, {"description", "Current EBITDA"}}},
            {"growth_rate",
             QJsonObject{{"type", "number"}, {"description", "Revenue growth rate (decimal, e.g. 0.10)"}}},
            {"wacc", QJsonObject{{"type", "number"}, {"description", "Weighted average cost of capital (decimal)"}}},
            {"terminal_growth", QJsonObject{{"type", "number"}, {"description", "Terminal growth rate (decimal)"}}},
            {"net_debt", QJsonObject{{"type", "number"}, {"description", "Net debt"}}},
            {"shares_outstanding", QJsonObject{{"type", "number"}, {"description", "Shares outstanding"}}}};
        t.input_schema.required = {"revenue", "ebitda", "wacc", "terminal_growth"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("dcf",
                               [&]() { fincept::services::ma::MAAnalyticsService::instance().calculate_dcf(args); });
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
            return run_ma_sync("lbo_model",
                               [&]() { fincept::services::ma::MAAnalyticsService::instance().build_lbo_model(args); });
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
            {"control_premium",
             QJsonObject{{"type", "number"}, {"description", "Control premium (decimal, e.g. 0.25)"}}}};
        t.input_schema.required = {"target_ebitda", "precedent_ev_ebitda"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("precedent_txns", [&]() {
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
        t.input_schema.properties =
            QJsonObject{{"acquirer_revenue", QJsonObject{{"type", "number"}, {"description", "Acquirer revenue"}}},
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
            {"new_share_price",
             QJsonObject{{"type", "number"}, {"description", "Acquirer share price for stock issuance"}}},
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
            return run_ma_sync("pro_forma",
                               [&]() { fincept::services::ma::MAAnalyticsService::instance().build_pro_forma(args); });
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
            return run_ma_sync("contribution", [&]() {
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
            return run_ma_sync("synergy_dcf", [&]() {
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
            {"acquirer_shares_outstanding",
             QJsonObject{{"type", "number"}, {"description", "Acquirer shares outstanding"}}},
            {"acquirer_share_price", QJsonObject{{"type", "number"}, {"description", "Acquirer share price"}}},
            {"target_shares_outstanding",
             QJsonObject{{"type", "number"}, {"description", "Target shares outstanding"}}}};
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
            {"target_metric",
             QJsonObject{{"type", "number"}, {"description", "Target metric threshold (e.g. revenue)"}}},
            {"probability_achieve",
             QJsonObject{{"type", "number"}, {"description", "Probability of achieving target (0-1)"}}},
            {"discount_rate", QJsonObject{{"type", "number"}, {"description", "Discount rate (decimal)"}}},
            {"years_to_payment", QJsonObject{{"type", "integer"}, {"description", "Years until earnout payment"}}}};
        t.input_schema.required = {"max_earnout", "probability_achieve", "discount_rate", "years_to_payment"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("earnout",
                               [&]() { fincept::services::ma::MAAnalyticsService::instance().value_earnout(args); });
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
            {"offer_price_per_target_share",
             QJsonObject{{"type", "number"}, {"description", "Offer price per target share"}}},
            {"acquirer_share_price", QJsonObject{{"type", "number"}, {"description", "Acquirer share price"}}},
            {"target_shares_outstanding",
             QJsonObject{{"type", "number"}, {"description", "Target shares outstanding"}}},
            {"acquirer_shares_outstanding",
             QJsonObject{{"type", "number"}, {"description", "Acquirer shares outstanding"}}}};
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
            {"target_shares_outstanding",
             QJsonObject{{"type", "number"}, {"description", "Target shares outstanding"}}}};
        t.input_schema.required = {"fixed_exchange_ratio", "collar_floor", "collar_ceiling", "acquirer_share_price"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("collar", [&]() {
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
            {"shares_outstanding",
             QJsonObject{{"type", "number"}, {"description", "Shares outstanding for per-share value"}}}};
        t.input_schema.required = {"cvr_payment", "probability", "time_to_milestone", "discount_rate"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("cvr", [&]() { fincept::services::ma::MAAnalyticsService::instance().value_cvr(args); });
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
            return run_ma_sync("all_deals",
                               [&]() { fincept::services::ma::MAAnalyticsService::instance().get_all_deals(); });
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
            return run_ma_sync("search_deals",
                               [&]() { fincept::services::ma::MAAnalyticsService::instance().search_deals(query); });
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
            return run_ma_sync("create_deal",
                               [&]() { fincept::services::ma::MAAnalyticsService::instance().create_deal(args); });
        };
        tools.push_back(std::move(t));
    }

    // ── ma_update_deal ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ma_update_deal";
        t.description = "Update an existing deal record in the M&A database.";
        t.category = "ma-analytics";
        t.input_schema.properties =
            QJsonObject{{"deal_id", QJsonObject{{"type", "string"}, {"description", "Deal ID to update"}}},
                        {"updates", QJsonObject{{"type", "object"},
                                                {"description", "Fields to update (status, deal_value, etc.)"}}}};
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
            {"strategic_relationships",
             QJsonObject{{"type", "number"}, {"description", "Score 0-2.5M: strategic relationships"}}},
            {"product_rollout",
             QJsonObject{{"type", "number"}, {"description", "Score 0-2.5M: product rollout / sales"}}}};
        t.input_schema.required = {"sound_idea", "prototype", "quality_team"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("berkus",
                               [&]() { fincept::services::ma::MAAnalyticsService::instance().calculate_berkus(args); });
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
            {"target_ror",
             QJsonObject{{"type", "number"}, {"description", "Target rate of return (decimal, e.g. 0.40)"}}},
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
            {"median_pre_money",
             QJsonObject{{"type", "number"}, {"description", "Median pre-money for comparable startups"}}},
            {"team_score", QJsonObject{{"type", "number"}, {"description", "Team strength multiplier (0.5-1.5)"}}},
            {"opportunity_score", QJsonObject{{"type", "number"}, {"description", "Market opportunity multiplier"}}},
            {"product_score", QJsonObject{{"type", "number"}, {"description", "Product/technology multiplier"}}},
            {"competition_score",
             QJsonObject{{"type", "number"}, {"description", "Competitive environment multiplier"}}},
            {"marketing_score",
             QJsonObject{{"type", "number"}, {"description", "Marketing/sales channels multiplier"}}}};
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
        // clang-format off
        t.input_schema.properties = QJsonObject{
            {"base_value", QJsonObject{{"type", "number"}, {"description", "Base median comparable valuation"}}},
            {"risk_scores", QJsonObject{{"type", "object"}, {"description", "Risk scores -2 to +2 for: management, stage, legislation, manufacturing, sales, funding, competition, technology, litigation, international, reputation, exit"}}}};
        // clang-format on
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
            return run_ma_sync("startup_comprehensive", [&]() {
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
            {"precedent_value",
             QJsonObject{{"type", "number"}, {"description", "Precedent transactions per-share value"}}},
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
            return run_ma_sync("premium_analysis",
                               [&]() { fincept::services::ma::MAAnalyticsService::instance().analyze_premium(args); });
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
            {"num_bidders_participated",
             QJsonObject{{"type", "integer"}, {"description", "Number that submitted bids"}}},
            {"go_shop_period", QJsonObject{{"type", "integer"}, {"description", "Go-shop period in days (0 if none)"}}},
            {"market_check_conducted",
             QJsonObject{{"type", "boolean"}, {"description", "Was a market check conducted?"}}},
            {"independent_committee",
             QJsonObject{{"type", "boolean"}, {"description", "Was an independent committee formed?"}}},
            {"financial_advisor_engaged",
             QJsonObject{{"type", "boolean"}, {"description", "Was a financial advisor engaged?"}}}};
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
            return run_ma_sync("finserv_metrics", [&]() {
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
            {"num_simulations",
             QJsonObject{{"type", "integer"}, {"description", "Number of simulations (default: 10000)"}}},
            {"revenue_growth_mean", QJsonObject{{"type", "number"}, {"description", "Mean revenue growth assumption"}}},
            {"revenue_growth_std", QJsonObject{{"type", "number"}, {"description", "Std dev of revenue growth"}}},
            {"wacc_mean", QJsonObject{{"type", "number"}, {"description", "Mean WACC"}}},
            {"wacc_std", QJsonObject{{"type", "number"}, {"description", "Std dev of WACC"}}}};
        t.input_schema.required = {"base_value", "std_dev_pct"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("monte_carlo",
                               [&]() { fincept::services::ma::MAAnalyticsService::instance().run_monte_carlo(args); });
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
            {"x_values",
             QJsonObject{{"type", "array"},
                         {"description", "Independent variable(s) — array of arrays for multiple regression"}}},
            {"regression_type", QJsonObject{{"type", "string"}, {"description", "OLS or MULTIPLE (default: OLS)"}}},
            {"variable_names",
             QJsonObject{{"type", "array"}, {"description", "Names for independent variables (optional)"}}}};
        t.input_schema.required = {"y_values", "x_values"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("regression",
                               [&]() { fincept::services::ma::MAAnalyticsService::instance().run_regression(args); });
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
            {"deals",
             QJsonObject{{"type", "array"},
                         {"description",
                          "Array of deal objects with fields: name, ev, ebitda, revenue, premium, payment_type"}}}};
        t.input_schema.required = {"deals"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("compare_deals",
                               [&]() { fincept::services::ma::MAAnalyticsService::instance().compare_deals(args); });
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
            {"rank_by", QJsonObject{{"type", "string"},
                                    {"description", "Metric to rank by: deal_value, premium, ev_ebitda, ev_revenue"}}},
            {"ascending", QJsonObject{{"type", "boolean"}, {"description", "Sort ascending (default: false)"}}}};
        t.input_schema.required = {"deals", "rank_by"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("rank_deals",
                               [&]() { fincept::services::ma::MAAnalyticsService::instance().rank_deals(args); });
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
            {"deal_size_bucket",
             QJsonObject{{"type", "string"}, {"description", "SMALL (<$500M), MID ($500M-$5B), LARGE (>$5B)"}}}};
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
            {"deals", QJsonObject{{"type", "array"},
                                  {"description", "Array of deals with payment_type, cash_pct, stock_pct fields"}}}};
        t.input_schema.required = {"deals"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            return run_ma_sync("payment_structures", [&]() {
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

    // ════════════════════════════════════════════════════════════════════
    // ANALYTICS LIBRARY WRAPPERS — skfolio, options, valuation
    // (linked service methods exposed so the AI/agents can actually call them)
    // ════════════════════════════════════════════════════════════════════
    {
        using MASvc = fincept::services::ma::MAAnalyticsService;
        auto num = [](const char* d) { return QJsonObject{{"type", "number"}, {"description", d}}; };
        auto str = [](const char* d) { return QJsonObject{{"type", "string"}, {"description", d}}; };
        auto arr = [](const char* d) { return QJsonObject{{"type", "array"}, {"description", d}}; };
        auto obj = [](const char* d) { return QJsonObject{{"type", "object"}, {"description", d}}; };

        // ── skfolio (portfolio optimization / risk) ──────────────────────
        const QJsonObject skf_props{
            {"prices", arr("Historical prices matrix, or use 'returns'.")},
            {"returns", arr("Asset returns matrix (rows=periods, cols=assets).")},
            {"optimization_method", str("mean_risk | hrp | herc | nco | risk_budgeting | max_diversification | "
                                        "equal_weight | inverse_volatility")},
            {"objective_function", str("minimize_risk | maximize_return | maximize_ratio | maximize_utility")},
            {"risk_measure", str("variance | cvar | max_drawdown | standard_deviation | …")},
        };
        tools.push_back(make_ma_tool(
            "ma_skfolio_optimize",
            "skfolio: optimize a portfolio (mean-risk, HRP, HERC, NCO, risk budgeting, …). Returns weights + metrics.",
            "skfolio_optimize", &MASvc::skfolio_optimize, skf_props, {}));
        tools.push_back(make_ma_tool("ma_skfolio_efficient_frontier",
                                     "skfolio: compute the efficient frontier (weights, returns, risks, sharpe).",
                                     "skfolio_efficient_frontier", &MASvc::skfolio_efficient_frontier, skf_props, {}));
        tools.push_back(make_ma_tool("ma_skfolio_risk_metrics",
                                     "skfolio: portfolio risk metrics for given weights/returns.",
                                     "skfolio_risk_metrics", &MASvc::skfolio_risk_metrics, skf_props, {}));
        tools.push_back(make_ma_tool("ma_skfolio_stress_test", "skfolio: stress-test a portfolio against scenarios.",
                                     "skfolio_stress_test", &MASvc::skfolio_stress_test,
                                     QJsonObject{{"returns", arr("Asset returns matrix.")},
                                                 {"weights", arr("Portfolio weights.")},
                                                 {"scenarios", obj("Stress scenarios.")}},
                                     {}));
        tools.push_back(make_ma_tool("ma_skfolio_backtest",
                                     "skfolio: walk-forward backtest of an optimization strategy.", "skfolio_backtest",
                                     &MASvc::skfolio_backtest,
                                     QJsonObject{{"prices", arr("Historical prices.")},
                                                 {"optimization_method", str("Strategy to backtest.")},
                                                 {"rebalance_freq", str("Rebalance frequency (e.g. 'M', 'Q').")}},
                                     {}));
        tools.push_back(make_ma_tool(
            "ma_skfolio_compare_strategies", "skfolio: compare multiple optimization strategies side by side.",
            "skfolio_compare_strategies", &MASvc::skfolio_compare_strategies,
            QJsonObject{{"prices", arr("Historical prices.")}, {"strategies", arr("List of strategy configs.")}}, {}));
        tools.push_back(make_ma_tool("ma_skfolio_risk_attribution",
                                     "skfolio: decompose portfolio risk by asset/factor.", "skfolio_risk_attribution",
                                     &MASvc::skfolio_risk_attribution, skf_props, {}));
        tools.push_back(make_ma_tool("ma_skfolio_hyperparameter_tune",
                                     "skfolio: cross-validated hyperparameter tuning for an optimizer.",
                                     "skfolio_hyperparameter_tune", &MASvc::skfolio_hyperparameter_tune,
                                     QJsonObject{{"prices", arr("Historical prices.")},
                                                 {"param_grid", obj("Grid of parameters.")},
                                                 {"cv_method", str("Cross-validation method.")}},
                                     {}));
        tools.push_back(make_ma_tool(
            "ma_skfolio_measures", "skfolio: compute a named risk/return measure for a series.", "skfolio_measures",
            &MASvc::skfolio_measures,
            QJsonObject{{"returns", arr("Returns series.")}, {"measure_name", str("Measure to compute.")}}, {}));
        tools.push_back(make_ma_tool("ma_skfolio_validate_model",
                                     "skfolio: validate an optimization model (in/out-of-sample).",
                                     "skfolio_validate_model", &MASvc::skfolio_validate_model, skf_props, {}));
        tools.push_back(make_ma_tool("ma_skfolio_scenario_analysis", "skfolio: scenario analysis over a portfolio.",
                                     "skfolio_scenario_analysis", &MASvc::skfolio_scenario_analysis,
                                     QJsonObject{{"returns", arr("Returns matrix.")}, {"scenarios", obj("Scenarios.")}},
                                     {}));
        tools.push_back(make_ma_tool("ma_skfolio_generate_report",
                                     "skfolio: generate a full portfolio analytics report.", "skfolio_generate_report",
                                     &MASvc::skfolio_generate_report, skf_props, {}));

        // ── Options analytics ────────────────────────────────────────────
        const QJsonObject opt_props{
            {"chain", arr("Option chain rows, e.g. [{strike, ce_oi, pe_oi, ce_iv, pe_iv, ce_price, pe_price}].")},
            {"spot", num("Underlying spot price.")},
            {"expiry", str("Expiry date (YYYY-MM-DD).")},
            {"interest_rate", num("Risk-free rate (decimal, e.g. 0.07).")},
            {"lot_size", num("Contract lot size.")},
        };
        tools.push_back(make_ma_tool("ma_options_gamma_exposure",
                                     "Options Gamma Exposure (GEX): net/call/put GEX, gamma call/put walls, PCR.",
                                     "options_gex", &MASvc::options_gamma_exposure, opt_props, {"chain", "spot"}));
        tools.push_back(make_ma_tool("ma_options_iv_smile",
                                     "Options implied-volatility smile + skew across strikes for one expiry.",
                                     "options_iv_smile", &MASvc::options_iv_smile, opt_props, {"chain", "spot"}));
        tools.push_back(make_ma_tool("ma_options_iv_surface",
                                     "Options implied-volatility surface across expiries and strikes.",
                                     "options_iv_surface", &MASvc::options_iv_surface,
                                     QJsonObject{{"expiries", arr("Per-expiry chains: [{expiry, chain:[…]}].")},
                                                 {"spot", num("Spot.")},
                                                 {"interest_rate", num("Risk-free rate.")}},
                                     {"expiries", "spot"}));
        tools.push_back(make_ma_tool("ma_options_open_interest",
                                     "Options OI tracker: PCR, max-pain, OI change, per-strike OI/volume.",
                                     "options_oi", &MASvc::options_open_interest, opt_props, {"chain", "spot"}));
        tools.push_back(make_ma_tool("ma_options_straddle_sim",
                                     "Simulate a short/long straddle with adjustments over an intraday candle series.",
                                     "options_straddle", &MASvc::options_straddle_sim,
                                     QJsonObject{{"candles", arr("OHLC candle series.")},
                                                 {"underlying", str("Underlying symbol.")},
                                                 {"expiry", str("Expiry date.")},
                                                 {"lots", num("Number of lots.")},
                                                 {"lot_size", num("Lot size.")},
                                                 {"strike_step", num("Strike step.")}},
                                     {"candles"}));
        tools.push_back(make_ma_tool(
            "ma_options_strategy_payoff",
            "Multi-leg options strategy payoff: breakevens, max profit/loss, net greeks, PnL curve.",
            "options_strategy_payoff", &MASvc::options_strategy_payoff,
            QJsonObject{
                {"legs", arr("Strategy legs: [{type:'CE'|'PE', side:'buy'|'sell', strike, qty, premium, iv}].")},
                {"spot", num("Spot.")},
                {"interest_rate", num("Risk-free rate.")},
                {"lot_size", num("Lot size.")}},
            {"legs", "spot"}));

        // ── Corporate-finance valuation summary ──────────────────────────
        tools.push_back(make_ma_tool(
            "ma_valuation_comprehensive",
            "Comprehensive valuation across methods (DCF, multiples, …) for the given target metrics.",
            "valuation_comprehensive", &MASvc::valuation_comprehensive,
            QJsonObject{{"target_metrics", obj("Target company metrics (revenue, ebitda, growth, etc.).")}},
            {"target_metrics"}));
        tools.push_back(make_ma_tool(
            "ma_valuation_executive_summary", "Generate an executive summary from prior valuation results.",
            "valuation_executive_summary", &MASvc::valuation_executive_summary,
            QJsonObject{{"valuation_results", obj("Output of a comprehensive valuation.")}}, {"valuation_results"}));
        tools.push_back(make_ma_tool(
            "ma_valuation_football_field", "Build a football-field valuation range chart from valuation results.",
            "valuation_football_field", &MASvc::valuation_football_field,
            QJsonObject{{"valuation_results", obj("Output of a comprehensive valuation.")}}, {"valuation_results"}));
    }

    // ════════════════════════════════════════════════════════════════════
    // ANALYTICS LIBRARIES (PR #349 backends) — expose all as MCP tools so the
    // AI/agents can reach them (previously linked but unwired).
    // ════════════════════════════════════════════════════════════════════
    {
        using MASvc = fincept::services::ma::MAAnalyticsService;
        auto num = [](const char* d) { return QJsonObject{{"type", "number"}, {"description", d}}; };
        auto str = [](const char* d) { return QJsonObject{{"type", "string"}, {"description", d}}; };
        auto arr = [](const char* d) { return QJsonObject{{"type", "array"}, {"description", d}}; };
        auto obj = [](const char* d) { return QJsonObject{{"type", "object"}, {"description", d}}; };
        const QJsonObject ret{{"returns", arr("Periodic returns series/matrix.")},
                              {"benchmark", arr("Optional benchmark returns.")}};
        const QJsonObject pf{{"returns", arr("Asset returns matrix.")},
                             {"weights", arr("Portfolio weights.")},
                             {"prices", arr("Asset prices matrix.")}};
        const QJsonObject ppo{{"prices", arr("Historical prices.")},
                              {"expected_returns", arr("Expected returns.")},
                              {"cov_matrix", arr("Covariance matrix.")},
                              {"weights", arr("Weights.")}};
        const QJsonObject ts{{"data", arr("Time-series values.")}, {"horizon", num("Forecast horizon.")}};
        const QJsonObject sm{{"data", arr("Input data series/matrix.")},
                             {"order", arr("(p,d,q) for ARIMA.")},
                             {"formula", str("Regression formula (OLS).")}};
        const QJsonObject vollib{{"flag", str("'c' (call) or 'p' (put).")},
                                 {"S", num("Spot.")},
                                 {"F", num("Forward (black model).")},
                                 {"K", num("Strike.")},
                                 {"t", num("Time to expiry (years).")},
                                 {"r", num("Risk-free rate.")},
                                 {"sigma", num("Volatility.")},
                                 {"price", num("Option price (for IV).")}};
        const QJsonObject pme{{"cashflows", arr("Fund cashflows.")},
                              {"dates", arr("Cashflow dates.")},
                              {"index_prices", arr("Benchmark index values.")}};
        const QJsonObject fin{{"income_statement", obj("Income statement.")},
                              {"balance_sheet", obj("Balance sheet.")},
                              {"cash_flow", obj("Cash-flow statement.")}};
        const QJsonObject startup{{"metrics", obj("Startup metrics (ARR, burn, growth, …).")}};
        const QJsonObject gen{{"params", obj("Analytic parameters (JSON).")}};
        (void)startup;
        (void)gen; // startup already wired above; gen is a spare passthrough schema
        tools.push_back(make_ma_tool("ma_income_analysis", "Financial analysis: analyze income statement.",
                                     "income_analysis", &MASvc::analyze_income_statement, fin, {}));
        tools.push_back(make_ma_tool("ma_balance_analysis", "Financial analysis: analyze balance sheet.",
                                     "balance_analysis", &MASvc::analyze_balance_sheet, fin, {}));
        tools.push_back(make_ma_tool("ma_cashflow_analysis", "Financial analysis: analyze cashflow statement.",
                                     "cashflow_analysis", &MASvc::analyze_cashflow_statement, fin, {}));
        tools.push_back(make_ma_tool("ma_financial_analysis", "Financial analysis: analyze comprehensive financials.",
                                     "financial_analysis", &MASvc::analyze_comprehensive_financials, fin, {}));
        tools.push_back(make_ma_tool("ma_financial_key_metrics", "Financial analysis: get financial key metrics.",
                                     "financial_key_metrics", &MASvc::get_financial_key_metrics, fin, {}));
        tools.push_back(make_ma_tool("ma_quantstats_stats", "QuantStats: quantstats stats.", "quantstats_stats",
                                     &MASvc::quantstats_stats, ret, {}));
        tools.push_back(make_ma_tool("ma_quantstats_returns", "QuantStats: quantstats returns.", "quantstats_returns",
                                     &MASvc::quantstats_returns, ret, {}));
        tools.push_back(make_ma_tool("ma_quantstats_drawdown", "QuantStats: quantstats drawdown.",
                                     "quantstats_drawdown", &MASvc::quantstats_drawdown, ret, {}));
        tools.push_back(make_ma_tool("ma_quantstats_rolling", "QuantStats: quantstats rolling.", "quantstats_rolling",
                                     &MASvc::quantstats_rolling, ret, {}));
        tools.push_back(make_ma_tool("ma_quantstats_full_report", "QuantStats: quantstats full report.",
                                     "quantstats_full_report", &MASvc::quantstats_full_report, ret, {}));
        tools.push_back(make_ma_tool("ma_arima", "statsmodels: fit arima.", "arima", &MASvc::fit_arima, sm, {}));
        tools.push_back(make_ma_tool("ma_arima_forecast", "statsmodels: forecast arima.", "arima_forecast",
                                     &MASvc::forecast_arima, sm, {}));
        tools.push_back(make_ma_tool("ma_ols", "statsmodels: fit ols.", "ols", &MASvc::fit_ols, sm, {}));
        tools.push_back(make_ma_tool("ma_descriptive_statistics", "statsmodels: descriptive statistics.",
                                     "descriptive_statistics", &MASvc::descriptive_statistics, sm, {}));
        tools.push_back(make_ma_tool("ma_ffn_performance", "FFN: calculate ffn performance.", "ffn_performance",
                                     &MASvc::calculate_ffn_performance, pf, {}));
        tools.push_back(make_ma_tool("ma_ffn_drawdowns", "FFN: calculate ffn drawdowns.", "ffn_drawdowns",
                                     &MASvc::calculate_ffn_drawdowns, pf, {}));
        tools.push_back(make_ma_tool("ma_ffn_rolling_metrics", "FFN: calculate ffn rolling metrics.",
                                     "ffn_rolling_metrics", &MASvc::calculate_ffn_rolling_metrics, pf, {}));
        tools.push_back(make_ma_tool("ma_ffn_risk_metrics", "FFN: calculate ffn risk metrics.", "ffn_risk_metrics",
                                     &MASvc::calculate_ffn_risk_metrics, pf, {}));
        tools.push_back(make_ma_tool("ma_ffn_portfolio", "FFN: optimize ffn portfolio.", "ffn_portfolio",
                                     &MASvc::optimize_ffn_portfolio, pf, {}));
        tools.push_back(make_ma_tool("ma_ffn_full_analysis", "FFN: run ffn full analysis.", "ffn_full_analysis",
                                     &MASvc::run_ffn_full_analysis, pf, {}));
        tools.push_back(make_ma_tool("ma_pca", "statsmodels: perform pca.", "pca", &MASvc::perform_pca, sm, {}));
        tools.push_back(make_ma_tool("ma_functime_forecast", "functime: functime forecast.", "functime_forecast",
                                     &MASvc::functime_forecast, ts, {}));
        tools.push_back(make_ma_tool("ma_functime_anomaly_detection", "functime: functime anomaly detection.",
                                     "functime_anomaly_detection", &MASvc::functime_anomaly_detection, ts, {}));
        tools.push_back(make_ma_tool("ma_functime_seasonality", "functime: functime seasonality.",
                                     "functime_seasonality", &MASvc::functime_seasonality, ts, {}));
        tools.push_back(make_ma_tool("ma_functime_metrics", "functime: functime metrics.", "functime_metrics",
                                     &MASvc::functime_metrics, ts, {}));
        tools.push_back(make_ma_tool("ma_functime_confidence_intervals", "functime: functime confidence intervals.",
                                     "functime_confidence_intervals", &MASvc::functime_confidence_intervals, ts, {}));
        tools.push_back(make_ma_tool("ma_functime_stationarity", "functime: functime stationarity.",
                                     "functime_stationarity", &MASvc::functime_stationarity, ts, {}));
        tools.push_back(make_ma_tool("ma_portfolio_optimize", "PyPortfolioOpt: optimize portfolio.",
                                     "portfolio_optimize", &MASvc::optimize_portfolio, ppo, {}));
        tools.push_back(make_ma_tool("ma_efficient_frontier", "PyPortfolioOpt: generate efficient frontier.",
                                     "efficient_frontier", &MASvc::generate_efficient_frontier, ppo, {}));
        tools.push_back(make_ma_tool("ma_discrete_allocation", "PyPortfolioOpt: calculate discrete allocation.",
                                     "discrete_allocation", &MASvc::calculate_discrete_allocation, ppo, {}));
        tools.push_back(make_ma_tool("ma_portfolio_backtest", "PyPortfolioOpt: run portfolio backtest.",
                                     "portfolio_backtest", &MASvc::run_portfolio_backtest, ppo, {}));
        tools.push_back(make_ma_tool("ma_risk_decomposition", "PyPortfolioOpt: calculate risk decomposition.",
                                     "risk_decomposition", &MASvc::calculate_risk_decomposition, ppo, {}));
        tools.push_back(make_ma_tool("ma_black_litterman", "PyPortfolioOpt: optimize black litterman.",
                                     "black_litterman", &MASvc::optimize_black_litterman, ppo, {}));
        tools.push_back(make_ma_tool("ma_hrp_optimization", "PyPortfolioOpt: optimize hrp.", "hrp_optimization",
                                     &MASvc::optimize_hrp, ppo, {}));
        tools.push_back(make_ma_tool("ma_portfolio_report", "PyPortfolioOpt: generate portfolio report.",
                                     "portfolio_report", &MASvc::generate_portfolio_report, ppo, {}));
        tools.push_back(make_ma_tool("ma_gs_risk_metrics", "GS Quant: calculate risk metrics.", "gs_risk_metrics",
                                     &MASvc::calculate_risk_metrics, pf, {}));
        tools.push_back(make_ma_tool("ma_gs_portfolio", "GS Quant: analyze portfolio.", "gs_portfolio",
                                     &MASvc::analyze_portfolio, pf, {}));
        tools.push_back(
            make_ma_tool("ma_gs_greeks", "GS Quant: calculate greeks.", "gs_greeks", &MASvc::calculate_greeks, pf, {}));
        tools.push_back(make_ma_tool("ma_gs_var", "GS Quant: perform var analysis.", "gs_var",
                                     &MASvc::perform_var_analysis, pf, {}));
        tools.push_back(make_ma_tool("ma_gs_stress_test", "GS Quant: perform stress test.", "gs_stress_test",
                                     &MASvc::perform_stress_test, pf, {}));
        tools.push_back(make_ma_tool("ma_gs_backtest", "GS Quant: run gs backtest.", "gs_backtest",
                                     &MASvc::run_gs_backtest, pf, {}));
        tools.push_back(make_ma_tool("ma_gs_statistics", "GS Quant: calculate statistics.", "gs_statistics",
                                     &MASvc::calculate_statistics, pf, {}));
        tools.push_back(make_ma_tool("ma_fortitudo_check_status", "Fortitudo Tech: fortitudo check status.",
                                     "fortitudo_check_status", &MASvc::fortitudo_check_status, pf, {}));
        tools.push_back(make_ma_tool("ma_fortitudo_portfolio_metrics", "Fortitudo Tech: fortitudo portfolio metrics.",
                                     "fortitudo_portfolio_metrics", &MASvc::fortitudo_portfolio_metrics, pf, {}));
        tools.push_back(make_ma_tool("ma_fortitudo_covariance_matrix", "Fortitudo Tech: fortitudo covariance matrix.",
                                     "fortitudo_covariance_matrix", &MASvc::fortitudo_covariance_matrix, pf, {}));
        tools.push_back(
            make_ma_tool("ma_fortitudo_mean_variance_optimize", "Fortitudo Tech: fortitudo mean variance optimize.",
                         "fortitudo_mean_variance_optimize", &MASvc::fortitudo_mean_variance_optimize, pf, {}));
        tools.push_back(make_ma_tool("ma_fortitudo_mean_cvar_optimize", "Fortitudo Tech: fortitudo mean cvar optimize.",
                                     "fortitudo_mean_cvar_optimize", &MASvc::fortitudo_mean_cvar_optimize, pf, {}));
        tools.push_back(make_ma_tool("ma_fortitudo_efficient_frontier", "Fortitudo Tech: fortitudo efficient frontier.",
                                     "fortitudo_efficient_frontier", &MASvc::fortitudo_efficient_frontier, pf, {}));
        tools.push_back(
            make_ma_tool("ma_fortitudo_exp_decay_probabilities", "Fortitudo Tech: fortitudo exp decay probabilities.",
                         "fortitudo_exp_decay_probabilities", &MASvc::fortitudo_exp_decay_probabilities, pf, {}));
        tools.push_back(make_ma_tool("ma_pme", "PyPME: calculate pme.", "pme", &MASvc::calculate_pme, pme, {}));
        tools.push_back(make_ma_tool("ma_verbose_pme", "PyPME: calculate verbose pme.", "verbose_pme",
                                     &MASvc::calculate_verbose_pme, pme, {}));
        tools.push_back(make_ma_tool("ma_xpme", "PyPME: calculate xpme.", "xpme", &MASvc::calculate_xpme, pme, {}));
        tools.push_back(make_ma_tool("ma_verbose_xpme", "PyPME: calculate verbose xpme.", "verbose_xpme",
                                     &MASvc::calculate_verbose_xpme, pme, {}));
        tools.push_back(make_ma_tool("ma_tessa_xpme", "PyPME: calculate tessa xpme.", "tessa_xpme",
                                     &MASvc::calculate_tessa_xpme, pme, {}));
        tools.push_back(make_ma_tool("ma_tessa_verbose_xpme", "PyPME: calculate tessa verbose xpme.",
                                     "tessa_verbose_xpme", &MASvc::calculate_tessa_verbose_xpme, pme, {}));
        tools.push_back(make_ma_tool("ma_black_price", "py_vollib: calculate black price.", "black_price",
                                     &MASvc::calculate_black_price, vollib, {}));
        tools.push_back(make_ma_tool("ma_black_greeks", "py_vollib: calculate black greeks.", "black_greeks",
                                     &MASvc::calculate_black_greeks, vollib, {}));
        tools.push_back(make_ma_tool("ma_black_iv", "py_vollib: calculate black iv.", "black_iv",
                                     &MASvc::calculate_black_iv, vollib, {}));
        tools.push_back(make_ma_tool("ma_bs_price", "py_vollib: calculate bs price.", "bs_price",
                                     &MASvc::calculate_bs_price, vollib, {}));
        tools.push_back(make_ma_tool("ma_bs_greeks", "py_vollib: calculate bs greeks.", "bs_greeks",
                                     &MASvc::calculate_bs_greeks, vollib, {}));
        tools.push_back(
            make_ma_tool("ma_bs_iv", "py_vollib: calculate bs iv.", "bs_iv", &MASvc::calculate_bs_iv, vollib, {}));
        tools.push_back(make_ma_tool("ma_bsm_price", "py_vollib: calculate bsm price.", "bsm_price",
                                     &MASvc::calculate_bsm_price, vollib, {}));
        tools.push_back(make_ma_tool("ma_bsm_greeks", "py_vollib: calculate bsm greeks.", "bsm_greeks",
                                     &MASvc::calculate_bsm_greeks, vollib, {}));
        tools.push_back(
            make_ma_tool("ma_bsm_iv", "py_vollib: calculate bsm iv.", "bsm_iv", &MASvc::calculate_bsm_iv, vollib, {}));
        tools.push_back(make_ma_tool("ma_gluonts_check_status", "GluonTS: gluonts check status.",
                                     "gluonts_check_status", &MASvc::gluonts_check_status, ts, {}));
        tools.push_back(make_ma_tool("ma_gluonts_probabilistic_forecast", "GluonTS: gluonts probabilistic forecast.",
                                     "gluonts_probabilistic_forecast", &MASvc::gluonts_probabilistic_forecast, ts, {}));
        tools.push_back(make_ma_tool("ma_gluonts_quantile_forecast", "GluonTS: gluonts quantile forecast.",
                                     "gluonts_quantile_forecast", &MASvc::gluonts_quantile_forecast, ts, {}));
        tools.push_back(make_ma_tool("ma_gluonts_distribution_fit", "GluonTS: gluonts distribution fit.",
                                     "gluonts_distribution_fit", &MASvc::gluonts_distribution_fit, ts, {}));
        tools.push_back(make_ma_tool("ma_gluonts_evaluate_forecast", "GluonTS: gluonts evaluate forecast.",
                                     "gluonts_evaluate_forecast", &MASvc::gluonts_evaluate_forecast, ts, {}));
        tools.push_back(make_ma_tool("ma_gluonts_seasonal_naive", "GluonTS: gluonts seasonal naive.",
                                     "gluonts_seasonal_naive", &MASvc::gluonts_seasonal_naive, ts, {}));
    }

    // ════════════════════════════════════════════════════════════════════
    // PORTFOLIO MANAGEMENT (CFA suite) — normalized to (command + JSON), exposed
    // as MCP tools. Contexts are per-script-prefixed (command names repeat).
    // ════════════════════════════════════════════════════════════════════
    {
        using MASvc = fincept::services::ma::MAAnalyticsService;
        auto num = [](const char* d) { return QJsonObject{{"type", "number"}, {"description", d}}; };
        auto str = [](const char* d) { return QJsonObject{{"type", "string"}, {"description", d}}; };
        auto arr = [](const char* d) { return QJsonObject{{"type", "array"}, {"description", d}}; };
        auto obj = [](const char* d) { return QJsonObject{{"type", "object"}, {"description", d}}; };
        (void)num;
        (void)str;
        (void)obj;
        tools.push_back(make_ma_cmd_tool("ma_am_value_added", "Active management: value added.", "am_value_added",
                                         &MASvc::run_active_management, "value_added",
                                         QJsonObject{{"portfolio_returns", arr("portfolio returns.")},
                                                     {"benchmark_returns", arr("benchmark returns.")},
                                                     {"weights", arr("weights.")}},
                                         {"portfolio_returns", "benchmark_returns", "weights"}));
        tools.push_back(make_ma_cmd_tool("ma_am_information_ratio", "Active management: information ratio.",
                                         "am_information_ratio", &MASvc::run_active_management, "information_ratio",
                                         QJsonObject{{"portfolio_returns", arr("portfolio returns.")},
                                                     {"benchmark_returns", arr("benchmark returns.")}},
                                         {"portfolio_returns", "benchmark_returns"}));
        tools.push_back(make_ma_cmd_tool("ma_am_tracking_risk", "Active management: tracking risk.", "am_tracking_risk",
                                         &MASvc::run_active_management, "tracking_risk",
                                         QJsonObject{{"portfolio_returns", arr("portfolio returns.")},
                                                     {"benchmark_returns", arr("benchmark returns.")}},
                                         {"portfolio_returns", "benchmark_returns"}));
        tools.push_back(make_ma_cmd_tool(
            "ma_am_comprehensive_analysis", "Active management: comprehensive analysis.", "am_comprehensive_analysis",
            &MASvc::run_active_management, "comprehensive_analysis",
            QJsonObject{{"portfolio_data", arr("portfolio data.")}, {"benchmark_data", arr("benchmark data.")}},
            {"portfolio_data", "benchmark_data"}));
        tools.push_back(make_ma_cmd_tool("ma_am_manager_selection", "Active management: manager selection.",
                                         "am_manager_selection", &MASvc::run_active_management, "manager_selection",
                                         QJsonObject{{"manager_candidates", arr("manager candidates.")}},
                                         {"manager_candidates"}));
        tools.push_back(make_ma_cmd_tool(
            "ma_em_comprehensive_analysis", "Economics & markets: comprehensive analysis.", "em_comprehensive_analysis",
            &MASvc::run_economics_markets, "comprehensive_analysis",
            QJsonObject{{"cycle_phase_str", str("cycle phase str.")}, {"economic_data", arr("economic data.")}},
            {"cycle_phase_str", "economic_data"}));
        tools.push_back(
            make_ma_cmd_tool("ma_em_business_cycle_analysis", "Economics & markets: business cycle analysis.",
                             "em_business_cycle_analysis", &MASvc::run_economics_markets, "business_cycle_analysis",
                             QJsonObject{{"cycle_phase_str", str("cycle phase str.")}}, {"cycle_phase_str"}));
        tools.push_back(make_ma_cmd_tool("ma_em_equity_risk_premium", "Economics & markets: equity risk premium.",
                                         "em_equity_risk_premium", &MASvc::run_economics_markets, "equity_risk_premium",
                                         QJsonObject{{"risk_free_rate", num("risk free rate.")},
                                                     {"market_risk_premium", num("market risk premium.")}},
                                         {"risk_free_rate", "market_risk_premium"}));
        tools.push_back(make_ma_cmd_tool("ma_pmg_comprehensive_management_analysis",
                                         "Portfolio management: comprehensive management analysis.",
                                         "pmg_comprehensive_management_analysis", &MASvc::run_portfolio_management,
                                         "comprehensive_management_analysis",
                                         QJsonObject{{"investor_data", arr("investor data.")}}, {"investor_data"}));
        tools.push_back(make_ma_cmd_tool(
            "ma_pmg_pension_analysis", "Portfolio management: pension analysis.", "pmg_pension_analysis",
            &MASvc::run_portfolio_management, "pension_analysis",
            QJsonObject{{"plan_type", str("plan type.")}, {"participant_data", arr("participant data.")}},
            {"plan_type", "participant_data"}));
        tools.push_back(make_ma_cmd_tool("ma_ppl_asset_allocation", "Portfolio planning: asset allocation.",
                                         "ppl_asset_allocation", &MASvc::run_portfolio_planning, "asset_allocation",
                                         QJsonObject{{"age", num("age.")},
                                                     {"risk_tolerance", str("risk tolerance.")},
                                                     {"time_horizon", num("time horizon.")}},
                                         {"age", "risk_tolerance", "time_horizon"}));
        tools.push_back(make_ma_cmd_tool("ma_ppl_retirement_planning", "Portfolio planning: retirement planning.",
                                         "ppl_retirement_planning", &MASvc::run_portfolio_planning,
                                         "retirement_planning",
                                         QJsonObject{{"current_age", num("current age.")},
                                                     {"retirement_age", num("retirement age.")},
                                                     {"current_savings", num("current savings.")},
                                                     {"annual_contribution", num("annual contribution.")}},
                                         {"current_age", "retirement_age", "current_savings", "annual_contribution"}));
        tools.push_back(make_ma_cmd_tool(
            "ma_rmg_comprehensive_risk_analysis", "Risk management: comprehensive risk analysis.",
            "rmg_comprehensive_risk_analysis", &MASvc::run_risk_management, "comprehensive_risk_analysis",
            QJsonObject{{"returns_data", arr("returns data.")},
                        {"weights", arr("weights.")},
                        {"portfolio_value", num("portfolio value.")}},
            {"returns_data", "weights", "portfolio_value"}));
        tools.push_back(make_ma_cmd_tool("ma_pan_comprehensive_analysis",
                                         "Portfolio analytics: comprehensive analysis.", "pan_comprehensive_analysis",
                                         &MASvc::run_portfolio_analytics, "comprehensive_analysis",
                                         QJsonObject{{"returns_data", obj("Map of asset -> returns array.")},
                                                     {"weights", arr("Portfolio weights.")},
                                                     {"market_returns", arr("Market returns.")}},
                                         {"returns_data"}));
        tools.push_back(make_ma_cmd_tool(
            "ma_pan_calculate_portfolio_metrics", "Portfolio analytics: calculate portfolio metrics.",
            "pan_calculate_portfolio_metrics", &MASvc::run_portfolio_analytics, "calculate_portfolio_metrics",
            QJsonObject{{"holdings", arr("Portfolio holdings.")}}, {"holdings"}));
        tools.push_back(make_ma_cmd_tool("ma_pan_tax_report", "Portfolio analytics: tax report.", "pan_tax_report",
                                         &MASvc::run_portfolio_analytics, "tax_report",
                                         QJsonObject{{"holdings", arr("Holdings with cost basis/lots.")}}, {}));
        tools.push_back(make_ma_cmd_tool(
            "ma_pan_pme_analysis", "Portfolio analytics: pme analysis.", "pan_pme_analysis",
            &MASvc::run_portfolio_analytics, "pme_analysis",
            QJsonObject{{"cashflows", arr("Fund cashflows.")}, {"index_prices", arr("Benchmark index values.")}}, {}));
        tools.push_back(make_ma_cmd_tool("ma_pan_allocation_analysis", "Portfolio analytics: allocation analysis.",
                                         "pan_allocation_analysis", &MASvc::run_portfolio_analytics,
                                         "allocation_analysis",
                                         QJsonObject{{"holdings", arr("Holdings by asset class/sector.")}}, {}));
    }

    // ════════════════════════════════════════════════════════════════════
    // TECHNICAL ANALYSIS / ECONOMICS / DEAL SCANNER + BACKTESTING BRIDGES
    // Multi-arg CLIs wired via Qt bridge shims (see MAAnalyticsService.cpp).
    // ════════════════════════════════════════════════════════════════════
    {
        using MASvc = fincept::services::ma::MAAnalyticsService;
        auto num = [](const char* d) { return QJsonObject{{"type", "number"}, {"description", d}}; };
        auto str = [](const char* d) { return QJsonObject{{"type", "string"}, {"description", d}}; };
        auto arr = [](const char* d) { return QJsonObject{{"type", "array"}, {"description", d}}; };
        auto obj = [](const char* d) { return QJsonObject{{"type", "object"}, {"description", d}}; };

        // ── Economics (zero-param analytics) ──
        tools.push_back(make_ma_tool(
            "ma_business_cycle",
            "Business-cycle indicators derived from live market proxies (S&P 500, VIX, 10y-3m yield-curve spread, "
            "market sentiment). Takes no parameters.",
            "business_cycle", &MASvc::business_cycle, QJsonObject{}, {}));
        tools.push_back(make_ma_tool(
            "ma_equity_risk_premium",
            "Estimate the equity risk premium via multiple methods (historical 10y/5y, implied earnings-yield, "
            "supply-side, VIX-implied, consensus). Takes no parameters.",
            "equity_risk_premium", &MASvc::equity_risk_premium, QJsonObject{}, {}));

        // ── Momentum indicator (indicator_type selects the calc) ──
        {
            ToolDef t;
            t.name = "ma_momentum_indicator";
            t.description = "Compute a momentum technical indicator for a symbol from yfinance OHLC data. "
                            "indicator_type is one of: rsi, macd, stochastic, cci, roc, williams_r, "
                            "awesome_oscillator, tsi, ultimate_oscillator. Returns the indicator time series.";
            t.category = "ma-analytics";
            t.input_schema.properties = QJsonObject{
                {"symbol", str("Ticker symbol, e.g. AAPL.")},
                {"indicator_type", str("rsi | macd | stochastic | cci | roc | williams_r | awesome_oscillator | tsi | "
                                       "ultimate_oscillator.")},
                {"timeframe", str("yfinance period, e.g. 1y, 6mo (default 1y).")},
                {"interval", str("yfinance interval, e.g. 1d, 1h (default 1d).")},
                {"period", num("Look-back period for single-period indicators.")}};
            t.input_schema.required = {"symbol", "indicator_type"};
            t.handler = [](const QJsonObject& args) -> ToolResult {
                const QString ind = args.value("indicator_type").toString();
                QJsonObject params = args;
                params.remove("indicator_type");
                return run_ma_sync("momentum_" + ind, [&]() {
                    fincept::services::ma::MAAnalyticsService::instance().momentum_indicator(ind, params);
                });
            };
            tools.push_back(std::move(t));
        }

        // ── Deal scanner (SEC EDGAR M&A filings; 3 commands → 3 tools) ──
        tools.push_back(make_ma_cmd_tool(
            "ma_deal_scan", "Scan recent SEC EDGAR filings for M&A deal indicators over the last N days.", "deal_scan",
            &MASvc::run_deal_scanner, "scan", QJsonObject{{"days_back", num("Look-back window in days (default 30).")}},
            {}));
        tools.push_back(make_ma_cmd_tool(
            "ma_deal_scan_company", "Scan a specific company's SEC filing history for M&A activity.",
            "deal_scan_company", &MASvc::run_deal_scanner, "scan_company",
            QJsonObject{{"ticker", str("Ticker or CIK.")}, {"years_back", num("Years of history (default 3).")}},
            {"ticker"}));
        tools.push_back(make_ma_cmd_tool(
            "ma_deal_high_confidence", "Return previously-scanned M&A deals above a confidence threshold.",
            "deal_high_confidence", &MASvc::run_deal_scanner, "high_confidence",
            QJsonObject{{"min_confidence", num("Minimum confidence 0-1 (default 0.75).")}}, {}));

        // ── SEC DCF inputs (build DCF inputs from SEC + FRED) ──
        tools.push_back(make_ma_tool(
            "ma_sec_dcf_inputs",
            "Build DCF model inputs for a ticker from SEC filings and FRED macro data (FRED key optional). "
            "Returns a dcf_inputs object.",
            "sec_dcf_inputs", &MASvc::sec_dcf_inputs,
            QJsonObject{{"ticker", str("Ticker symbol, e.g. AAPL.")},
                        {"fred_api_key", str("Optional FRED API key.")},
                        {"overrides", obj("Optional DCF input overrides, e.g. {\"beta\":1.5}.")}},
            {"ticker"}));

        // ── Backtesting bridges — run a registered Fincept strategy via a provider ──
        {
            ToolDef t;
            t.name = "ma_run_backtest";
            t.description = "Run a registered Fincept strategy (FCT-… id from the strategy registry) through a "
                            "backtesting provider over historical yfinance data. Returns performance (total return, "
                            "final equity, trade count), the trade list, and the equity curve.";
            t.category = "ma-analytics";
            t.input_schema.properties =
                QJsonObject{{"provider", str("Backtest provider: backtestingpy, bt, fasttrade, vectorbt, or zipline.")},
                            {"strategy_id", str("Registered Fincept strategy id, e.g. FCT-19E8A564.")},
                            {"symbols", arr("Symbols to backtest, e.g. [\"AAPL\"].")},
                            {"start_date", str("Backtest start date, YYYY-MM-DD.")},
                            {"end_date", str("Backtest end date, YYYY-MM-DD.")},
                            {"initial_cash", num("Starting cash.")},
                            {"resolution", str("daily or hourly (default daily).")},
                            {"strategy_params", obj("Optional strategy-specific parameters.")}};
            t.input_schema.required = {"provider", "strategy_id", "symbols", "start_date", "end_date", "initial_cash"};
            t.handler = [](const QJsonObject& args) -> ToolResult {
                const QString provider = args.value("provider").toString("bt");
                const QString strategy_id = args.value("strategy_id").toString();
                QJsonObject params = args;
                params.remove("provider");
                params.remove("strategy_id");
                return run_ma_sync("backtest_" + provider, [&]() {
                    fincept::services::ma::MAAnalyticsService::instance().run_backtest(provider, strategy_id, params);
                });
            };
            tools.push_back(std::move(t));
        }
    }

    // ════════════════════════════════════════════════════════════════════
    // VISIONQUANT — candlestick-pattern intelligence (engine/scorer/backtester/
    // index). Standard (command, JSON) CLIs on venv-numpy2 (torch/faiss/PIL).
    // ════════════════════════════════════════════════════════════════════
    {
        using MASvc = fincept::services::ma::MAAnalyticsService;
        auto num = [](const char* d) { return QJsonObject{{"type", "number"}, {"description", d}}; };
        auto str = [](const char* d) { return QJsonObject{{"type", "string"}, {"description", d}}; };
        auto arr = [](const char* d) { return QJsonObject{{"type", "array"}, {"description", d}}; };

        // engine.py — status | search | encode
        tools.push_back(make_ma_cmd_tool(
            "vq_engine_status", "VisionQuant: report whether the pattern model + FAISS index are built and ready.",
            "vq_engine_status", &MASvc::run_vision_engine, "status", QJsonObject{}, {}));
        tools.push_back(make_ma_cmd_tool(
            "vq_search",
            "VisionQuant: find historical candlestick patterns most similar to a symbol's recent chart "
            "(CNN embedding + FAISS nearest-neighbour search). Requires the index to be built first.",
            "vq_engine_search", &MASvc::run_vision_engine, "search",
            QJsonObject{{"symbol", str("Ticker symbol, e.g. AAPL.")},
                        {"date", str("As-of date YYYY-MM-DD (default latest).")},
                        {"top_k", num("Number of similar patterns to return (default 10).")},
                        {"lookback", num("Chart lookback window in bars (default 60).")}},
            {"symbol"}));
        tools.push_back(
            make_ma_cmd_tool("vq_encode", "VisionQuant: encode a chart image file into its pattern embedding vector.",
                             "vq_engine_encode", &MASvc::run_vision_engine, "encode",
                             QJsonObject{{"image_path", str("Absolute path to a chart image file.")}}, {"image_path"}));

        // scorer.py — score | vision_score | fundamental_score | technical_score
        tools.push_back(make_ma_cmd_tool(
            "vq_score",
            "VisionQuant: composite scorecard for a symbol blending vision (pattern win-rate), fundamental and "
            "technical scores into a BUY/WAIT/SELL verdict.",
            "vq_scorer_score", &MASvc::run_vision_scorer, "score",
            QJsonObject{{"symbol", str("Ticker symbol, e.g. AAPL.")},
                        {"win_rate", num("Historical pattern win-rate percent (default 50).")},
                        {"date", str("As-of date YYYY-MM-DD (default latest).")}},
            {"symbol"}));
        tools.push_back(
            make_ma_cmd_tool("vq_vision_score", "VisionQuant: vision-only sub-score derived from a pattern win-rate.",
                             "vq_scorer_vision_score", &MASvc::run_vision_scorer, "vision_score",
                             QJsonObject{{"win_rate", num("Historical pattern win-rate percent (default 50).")}}, {}));
        tools.push_back(make_ma_cmd_tool("vq_fundamental_score", "VisionQuant: fundamental sub-score for a symbol.",
                                         "vq_scorer_fundamental_score", &MASvc::run_vision_scorer, "fundamental_score",
                                         QJsonObject{{"symbol", str("Ticker symbol, e.g. AAPL.")}}, {"symbol"}));
        tools.push_back(make_ma_cmd_tool("vq_technical_score", "VisionQuant: technical sub-score for a symbol.",
                                         "vq_scorer_technical_score", &MASvc::run_vision_scorer, "technical_score",
                                         QJsonObject{{"symbol", str("Ticker symbol, e.g. AAPL.")},
                                                     {"date", str("As-of date YYYY-MM-DD (default latest).")}},
                                         {"symbol"}));

        // backtester.py — backtest (single command)
        tools.push_back(make_ma_tool(
            "vq_backtest",
            "VisionQuant: backtest a pattern-driven RSI/MACD/MA signal strategy for one symbol over a date range. "
            "Returns the trade list and performance.",
            "vq_backtest", &MASvc::run_vision_backtest,
            QJsonObject{{"symbol", str("Ticker symbol, e.g. AAPL.")},
                        {"start", str("Start date YYYY-MM-DD.")},
                        {"end", str("End date YYYY-MM-DD.")},
                        {"capital", num("Initial capital (default 100000).")},
                        {"stop_loss", num("Stop-loss fraction (default 0.03).")},
                        {"take_profit", num("Take-profit fraction (default 0.05).")},
                        {"max_hold", num("Max holding period in bars (default 20).")},
                        {"entry_rsi", num("RSI entry threshold (default 40).")},
                        {"exit_rsi", num("RSI exit threshold (default 70).")},
                        {"ma_period", num("Moving-average period (default 60).")}},
            {"symbol", "start", "end"}));

        // setup_index.py — status | build (build trains the CNN + builds FAISS: long-running)
        tools.push_back(make_ma_cmd_tool("vq_index_status",
                                         "VisionQuant: status of the pattern model + FAISS index build.",
                                         "vq_index_status", &MASvc::run_vision_index, "status", QJsonObject{}, {}));
        tools.push_back(make_ma_cmd_tool(
            "vq_build_index",
            "VisionQuant: build the pattern index — download charts, train the CNN autoencoder, and build the FAISS "
            "index. LONG-RUNNING (minutes). Run once before vq_search / vq_score.",
            "vq_index_build", &MASvc::run_vision_index, "build",
            QJsonObject{{"symbols", arr("Symbols to index (default a built-in universe).")},
                        {"start", str("History start date YYYY-MM-DD (default 2020-01-01).")},
                        {"stride", num("Sampling stride in bars (default 5).")},
                        {"window", num("Chart window in bars (default 60).")},
                        {"epochs", num("Training epochs (default 30).")},
                        {"batch_size", num("Training batch size (default 32).")}},
            {}));
    }

    // Data-source connectors (auto-generated dispatcher tools; keyless + keyed).
    append_data_connector_tools(tools);

    LOG_DEBUG(TAG, QString("Registered %1 MA Analytics tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
