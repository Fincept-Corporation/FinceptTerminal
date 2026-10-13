// QuantLabTools.cpp — Tools that drive the AI Quant Lab screen.
//
// Module × bespoke commands → 97 module/command-specific tools plus three
// generic discovery/dispatch tools. All execution paths bottom out at
// AIQuantLabService::run_module, which spawns the module's Python script (or,
// for "deep_agent", routes to the deep agent / RD-Agent task registry) and
// emits result_ready / error_occurred when it finishes. We bridge those
// Qt-signal callbacks into an async ToolResult promise via AsyncDispatch.
//
// Tool naming convention: quant_<module>_<command>. The module list and
// command catalogue live in kCatalog below; adding a tool is one line.

#include "mcp/tools/QuantLabTools.h"

#include "core/logging/Logger.h"
#include "mcp/AsyncDispatch.h"
#include "mcp/ToolSchemaBuilder.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "services/ai_quant_lab/AIQuantLabTypes.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

namespace fincept::mcp::tools {

namespace {

static constexpr const char* TAG = "QuantLabTools";

// Quant Python scripts can run long (full-period backtests, forecasts).
// 5 min default; callers can override per call via _meta.timeout_ms. Anything
// above 30 s with an async handler is job-eligible (CLAUDE.md §M1).
static constexpr int kDefaultTimeoutMs = 300000;
// Model training (qlib models, RL agents) — matches PythonRunner's long-run watchdog.
static constexpr int kTrainingTimeoutMs = 60 * 60 * 1000;
// The LangGraph deep agent: many LLM calls per run.
static constexpr int kDeepAgentTimeoutMs = 20 * 60 * 1000;
// RD-Agent start/status/list/stop/resume, UI/MCP launchers: answered from the
// service's task registry or by a short helper process — never a long wait.
static constexpr int kQuickTimeoutMs = 30000;

struct CatalogEntry {
    const char* module_id;
    const char* command;
    const char* tool_name;
    const char* description;
    // Destructiveness rule: true ONLY when the command writes or deletes
    // persistent state (models / schedules / pipelines / learning state saved to
    // disk), spawns long-lived background work (RD-Agent loops, UI / MCP
    // servers), runs an autonomous LLM agent with filesystem / shell tools, or
    // places orders. Commands that only COMPUTE and return a result are false —
    // flagging them made every agent call refused by default for no safety gain.
    // When adding a row, read the script's command handler before choosing.
    bool destructive;
    int timeout_ms;
};

// Curated from the AIQuantLabService dispatch sites (run_module +
// specialised run_python calls). Adding a new command = one row here.
static const CatalogEntry kCatalog[] = {
    // ── CORE ─────────────────────────────────────────────────────────────
    {"factor_discovery", "get_data", "quant_factor_get_data",
     "Fetch factor data from Qlib for instruments + date range.", false, kDefaultTimeoutMs},
    {"factor_discovery", "get_calendar", "quant_factor_get_calendar", "Get Qlib trading calendar for a date range.",
     false, kDefaultTimeoutMs},
    {"model_library", "train_model", "quant_model_train",
     "Train a Qlib model (LightGBM, XGBoost, LSTM, Transformer, etc).", true, kTrainingTimeoutMs},
    {"model_library", "run_backtest", "quant_model_backtest", "Backtest a trained Qlib model.", false,
     kDefaultTimeoutMs},
    {"backtesting", "run_backtest", "quant_run_backtest", "Multi-strategy backtest with portfolio optimization.", false,
     kDefaultTimeoutMs},
    {"backtesting", "optimize_portfolio", "quant_optimize_portfolio", "Run portfolio optimization across strategies.",
     false, kDefaultTimeoutMs},
    {"live_signals", "get_data", "quant_signals_get_data", "Fetch live trading signal data.", false, kDefaultTimeoutMs},
    {"live_signals", "get_factor_analysis", "quant_signals_factor_analysis", "Factor analysis on live signals.", false,
     kDefaultTimeoutMs},
    {"live_signals", "get_feature_importance", "quant_signals_feature_importance",
     "Feature importance for live signal model.", false, kDefaultTimeoutMs},

    // ── AI_ML ────────────────────────────────────────────────────────────
    {"deep_agent", "execute_task", "quant_deep_agent_execute", "Run an autonomous research task with the deep agent.",
     true, kDeepAgentTimeoutMs},
    {"deep_agent", "check_status", "quant_rd_agent_status", "Check RD-Agent / deep-agent runtime status.", false,
     kQuickTimeoutMs},
    {"deep_agent", "start_factor_mining", "quant_rd_start_factor_mining", "Start an RD-Agent factor mining task.", true,
     kQuickTimeoutMs},
    {"deep_agent", "start_model_optimization", "quant_rd_start_model_opt", "Start an RD-Agent model optimization task.",
     true, kQuickTimeoutMs},
    {"deep_agent", "start_quant_research", "quant_rd_start_quant_research", "Start an RD-Agent quant research task.",
     true, kQuickTimeoutMs},
    {"deep_agent", "get_task_status", "quant_rd_task_status", "Get status of an RD-Agent task by task_id.", false,
     kQuickTimeoutMs},
    {"deep_agent", "get_discovered_factors", "quant_rd_get_factors", "Get factors discovered by an RD-Agent task.",
     false, kQuickTimeoutMs},
    {"deep_agent", "get_optimized_model", "quant_rd_get_model", "Get the optimized model produced by an RD-Agent task.",
     false, kQuickTimeoutMs},
    {"deep_agent", "list_tasks", "quant_rd_list_tasks", "List RD-Agent tasks; optional status filter.", false,
     kQuickTimeoutMs},
    {"deep_agent", "stop_task", "quant_rd_stop_task", "Stop a running RD-Agent task.", true, kQuickTimeoutMs},
    {"deep_agent", "resume_task", "quant_rd_resume_task", "Resume a stopped RD-Agent task; optional config.", true,
     kQuickTimeoutMs},
    {"deep_agent", "start_ui", "quant_rd_start_ui", "Launch the RD-Agent inspection UI.", true, kQuickTimeoutMs},
    {"deep_agent", "start_mcp_server", "quant_rd_start_mcp", "Start the RD-Agent MCP server (default port 18765).",
     true, kQuickTimeoutMs},
    {"deep_agent", "mcp_status", "quant_rd_mcp_status", "Check the RD-Agent MCP server status.", false,
     kQuickTimeoutMs},
    {"rl_trading", "train", "quant_rl_train", "Train an RL trading agent (PPO/DQN/A2C/SAC/TD3). Streaming progress.",
     true, kTrainingTimeoutMs},
    {"rl_trading", "evaluate", "quant_rl_evaluate", "Evaluate a trained RL trading agent.", false, kDefaultTimeoutMs},
    {"online_learning", "create_model", "quant_online_create_model",
     "Create an online-learning model with concept-drift detection.", true, kDefaultTimeoutMs},
    {"online_learning", "train", "quant_online_train", "Incrementally train an online-learning model.", true,
     kDefaultTimeoutMs},
    {"online_learning", "predict", "quant_online_predict", "Get predictions from an online-learning model.", false,
     kDefaultTimeoutMs},
    {"online_learning", "performance", "quant_online_performance",
     "Get performance metrics for an online-learning model.", false, kDefaultTimeoutMs},
    {"meta_learning", "run_selection", "quant_meta_run_selection", "Run meta-learning model selection / AutoML.", true,
     kDefaultTimeoutMs},
    {"meta_learning", "create_ensemble", "quant_meta_create_ensemble", "Build a model ensemble via meta learning.",
     false, kDefaultTimeoutMs},
    {"meta_learning", "tune_hyperparameters", "quant_meta_tune_hyperparams", "Hyperparameter tuning via meta learning.",
     false, kDefaultTimeoutMs},

    // ── ADVANCED ─────────────────────────────────────────────────────────
    {"hft", "fetch_orderbook", "quant_hft_orderbook", "Fetch an order-book snapshot for HFT analysis.", false,
     kDefaultTimeoutMs},
    {"hft", "market_making", "quant_hft_market_making", "Run a market-making strategy step.", false, kDefaultTimeoutMs},
    {"hft", "slippage", "quant_hft_slippage", "Estimate slippage for a trade against the book.", false,
     kDefaultTimeoutMs},
    {"hft", "toxic_flow", "quant_hft_toxic_flow", "Detect toxic order flow on a book.", false, kDefaultTimeoutMs},
    {"hft", "analyze", "quant_hft_analyze", "General HFT order-book analytics.", false, kDefaultTimeoutMs},
    {"rolling_retraining", "create", "quant_rolling_create", "Create a rolling-retraining schedule.", true,
     kDefaultTimeoutMs},
    {"rolling_retraining", "preview", "quant_rolling_preview", "Preview the retrain tasks a schedule would generate.",
     false, kDefaultTimeoutMs},
    {"rolling_retraining", "delete", "quant_rolling_delete", "Delete a rolling-retraining schedule.", true,
     kDefaultTimeoutMs},
    {"advanced_models", "create_model", "quant_adv_create_model",
     "Create an advanced model (lstm/gru/transformer/lstm_attention). Trains on synthetic data; "
     "results carry a 'warning' and are illustrative only.",
     true, kDefaultTimeoutMs},
    {"advanced_models", "train_model", "quant_adv_train_model", "Train an advanced model.", true, kDefaultTimeoutMs},
    {"feature_engineering", "evaluate_expression", "quant_feature_eval_expr", "Evaluate a Qlib feature expression.",
     false, kDefaultTimeoutMs},
    {"feature_engineering", "select_features_by_ic", "quant_feature_select_ic",
     "Select features by Information Coefficient.", false, kDefaultTimeoutMs},
    {"factor_evaluation", "calculate_ic", "quant_eval_ic", "Calculate Information Coefficient metrics for a factor.",
     false, kDefaultTimeoutMs},
    {"factor_evaluation", "calculate_risk_metrics", "quant_eval_risk_metrics",
     "Calculate factor risk metrics (Sharpe, IR, drawdown, etc).", false, kDefaultTimeoutMs},
    {"factor_evaluation", "generate_evaluation_report", "quant_eval_report",
     "Generate a full factor evaluation report.", false, kDefaultTimeoutMs},
    {"strategy_builder", "portfolio_metrics", "quant_strategy_portfolio_metrics",
     "Strategy portfolio metrics (returns, risk, exposure).", false, kDefaultTimeoutMs},
    {"data_processors", "create_pipeline", "quant_dataproc_create_pipeline",
     "Create a Qlib data-processing pipeline (zscore/winsorize/cs_rank/tanh/fillna).", true, kDefaultTimeoutMs},
    {"data_processors", "process_data", "quant_dataproc_process", "Run a Qlib data-processing pipeline on a dataset.",
     false, kDefaultTimeoutMs},

    // ── ANALYTICS ────────────────────────────────────────────────────────
    {"quant_reporting", "check_status", "quant_reporting_status", "Check the quant-reporting service status.", false,
     kDefaultTimeoutMs},
    {"quant_reporting", "ic_analysis", "quant_reporting_ic",
     "Information Coefficient (IC) analysis report — measures a factor's predictive power on forward returns over "
     "time.",
     false, kDefaultTimeoutMs},
    {"quant_reporting", "model_performance", "quant_reporting_model_perf", "Model performance report.", false,
     kDefaultTimeoutMs},
    {"quant_reporting", "cumulative_returns", "quant_reporting_cumret", "Cumulative-returns report.", false,
     kDefaultTimeoutMs},
    {"quant_reporting", "risk_report", "quant_reporting_risk", "Risk-analysis report.", false, kDefaultTimeoutMs},
    {"quant_reporting", "factor_quantiles", "quant_reporting_factor_quantiles", "Factor quantile / decile report.",
     false, kDefaultTimeoutMs},
    {"cfa_quant", "trend_analysis", "quant_cfa_trend", "CFA-style trend analysis on a time series.", false,
     kDefaultTimeoutMs},
    {"cfa_quant", "stationarity_test", "quant_cfa_stationarity", "CFA-style stationarity tests (ADF, KPSS).", false,
     kDefaultTimeoutMs},
    {"cfa_quant", "arima_model", "quant_cfa_arima", "Fit an ARIMA model (CFA wrapper).", false, kDefaultTimeoutMs},
    {"cfa_quant", "supervised_learning", "quant_cfa_supervised", "Supervised-learning prediction (CFA wrapper).", false,
     kDefaultTimeoutMs},
    {"cfa_quant", "unsupervised_learning", "quant_cfa_unsupervised",
     "Unsupervised-learning analysis (PCA, clustering).", false, kDefaultTimeoutMs},
    {"cfa_quant", "resampling_methods", "quant_cfa_resampling", "Bootstrap / resampling-based inference.", false,
     kDefaultTimeoutMs},
    {"cfa_quant", "validate_data", "quant_cfa_validate_data", "Validate input data quality.", false, kDefaultTimeoutMs},
    {"gs_quant", "risk_metrics", "quant_gs_risk_metrics", "GS-Quant risk metrics (VaR, ES, vol, beta, etc).", false,
     kDefaultTimeoutMs},
    {"gs_quant", "portfolio_analytics", "quant_gs_portfolio_analytics", "GS-Quant portfolio analytics.", false,
     kDefaultTimeoutMs},
    {"gs_quant", "greeks", "quant_gs_greeks", "GS-Quant options Greeks.", false, kDefaultTimeoutMs},
    {"gs_quant", "var_analysis", "quant_gs_var_analysis", "GS-Quant VaR analysis.", false, kDefaultTimeoutMs},
    {"gs_quant", "stress_test", "quant_gs_stress_test", "GS-Quant scenario stress test.", false, kDefaultTimeoutMs},
    {"gs_quant", "backtest", "quant_gs_backtest", "GS-Quant backtest engine.", false, kDefaultTimeoutMs},
    {"gs_quant", "statistics", "quant_gs_statistics", "GS-Quant descriptive statistics.", false, kDefaultTimeoutMs},
    {"statsmodels", "check_status", "quant_statsmodels_status", "Statsmodels service status.", false,
     kDefaultTimeoutMs},
    {"statsmodels", "ols", "quant_statsmodels_ols", "Statsmodels OLS regression.", false, kDefaultTimeoutMs},
    {"statsmodels", "arima", "quant_statsmodels_arima", "Statsmodels ARIMA fit.", false, kDefaultTimeoutMs},
    {"statsmodels", "stationarity_tests", "quant_statsmodels_stationarity",
     "Statsmodels stationarity tests (ADF, KPSS, PP).", false, kDefaultTimeoutMs},
    {"statsmodels", "acf_pacf", "quant_statsmodels_acf_pacf", "ACF and PACF computation.", false, kDefaultTimeoutMs},
    {"statsmodels", "granger_causality", "quant_statsmodels_granger", "Granger causality test.", false,
     kDefaultTimeoutMs},
    {"statsmodels", "descriptive", "quant_statsmodels_descriptive", "Descriptive statistics.", false,
     kDefaultTimeoutMs},
    {"functime", "check_status", "quant_functime_status", "Functime service status.", false, kDefaultTimeoutMs},
    {"functime", "forecast", "quant_functime_forecast", "Functime time-series forecast.", false, kDefaultTimeoutMs},
    {"functime", "anomaly_detection", "quant_functime_anomaly", "Functime anomaly detection.", false,
     kDefaultTimeoutMs},
    {"functime", "seasonality", "quant_functime_seasonality", "Functime seasonality decomposition.", false,
     kDefaultTimeoutMs},
    {"functime", "metrics", "quant_functime_metrics", "Functime forecast metrics.", false, kDefaultTimeoutMs},
    {"functime", "confidence_intervals", "quant_functime_intervals", "Functime forecast confidence intervals.", false,
     kDefaultTimeoutMs},
    {"functime", "stationarity", "quant_functime_stationarity", "Functime stationarity tests.", false,
     kDefaultTimeoutMs},
    {"fortitudo", "check_status", "quant_fortitudo_status", "Fortitudo service status.", false, kDefaultTimeoutMs},
    {"fortitudo", "portfolio_metrics", "quant_fortitudo_portfolio_metrics", "Fortitudo portfolio metrics.", false,
     kDefaultTimeoutMs},
    {"fortitudo", "covariance_matrix", "quant_fortitudo_covariance", "Fortitudo covariance matrix.", false,
     kDefaultTimeoutMs},
    {"fortitudo", "mean_variance_optimize", "quant_fortitudo_mv_opt", "Mean-variance portfolio optimization.", false,
     kDefaultTimeoutMs},
    {"fortitudo", "mean_cvar_optimize", "quant_fortitudo_mcvar_opt", "Mean-CVaR portfolio optimization.", false,
     kDefaultTimeoutMs},
    {"fortitudo", "efficient_frontier", "quant_fortitudo_efficient_frontier", "Compute the efficient frontier.", false,
     kDefaultTimeoutMs},
    {"fortitudo", "exp_decay_probabilities", "quant_fortitudo_exp_decay", "Exponential-decay observation weights.",
     false, kDefaultTimeoutMs},
    {"gluonts", "check_status", "quant_gluonts_status", "GluonTS service status.", false, kDefaultTimeoutMs},
    {"gluonts", "probabilistic_forecast", "quant_gluonts_prob_forecast", "GluonTS probabilistic forecast.", false,
     kDefaultTimeoutMs},
    {"gluonts", "quantile_forecast", "quant_gluonts_quantile_forecast", "GluonTS quantile forecast.", false,
     kDefaultTimeoutMs},
    {"gluonts", "distribution_fit", "quant_gluonts_distribution_fit", "Fit a parametric distribution to a series.",
     false, kDefaultTimeoutMs},
    {"gluonts", "evaluate_forecast", "quant_gluonts_evaluate", "Evaluate a forecast against actuals.", false,
     kDefaultTimeoutMs},
    {"gluonts", "seasonal_naive", "quant_gluonts_seasonal_naive", "Seasonal-naive baseline forecast.", false,
     kDefaultTimeoutMs},
};

constexpr std::size_t kCatalogSize = sizeof(kCatalog) / sizeof(kCatalog[0]);

// Async dispatch: post run_module() onto the service thread, wait for the
// first matching result_ready / error_occurred signal, resolve the promise.
//
// Race note: signals are filtered by (module_id, command) for result_ready
// but only by module_id for error_occurred (the service signal only carries
// module_id). Two concurrent calls into the same module CAN have their
// errors crossed. This mirrors the existing screen code's filter; until
// the service grows per-call IDs there's no clean fix.
void dispatch_quant_async(const QString& module_id, const QString& command, const QJsonObject& params, ToolContext ctx,
                          std::shared_ptr<QPromise<ToolResult>> promise) {
    auto* svc = &services::quant::AIQuantLabService::instance();
    // Mark the handoff. Most of these tools declare multi-minute budgets, so under a job the
    // model can be polling for minutes; without this the snapshot says only
    // "running", which cannot distinguish work in flight from a dispatch that
    // never reached the service. The service reports no finer progress than
    // result/error, so this is the whole of the honest signal — one tick, not
    // a fabricated ramp.
    if (ctx.on_progress)
        ctx.on_progress(0.0, QStringLiteral("submitted to %1/%2").arg(module_id, command));
    AsyncDispatch::callback_to_promise(svc, std::move(ctx), promise, [svc, module_id, command, params](auto resolve) {
        auto* holder = new QObject(svc);
        QObject::connect(svc, &services::quant::AIQuantLabService::result_ready, holder,
                         [module_id, command, resolve, holder](QString mid, QString cmd, QJsonObject data) {
                             if (mid != module_id || cmd != command)
                                 return;
                             resolve(ToolResult::ok_data(data));
                             holder->deleteLater();
                         });
        QObject::connect(svc, &services::quant::AIQuantLabService::error_occurred, holder,
                         [module_id, resolve, holder](QString mid, QString msg) {
                             if (mid != module_id)
                                 return;
                             resolve(ToolResult::fail(msg));
                             holder->deleteLater();
                         });
        svc->run_module(module_id, command, params);
    });
}

ToolSchema generic_params_schema(const QString& command_hint = {}) {
    return ToolSchemaBuilder()
        .object("params",
                command_hint.isEmpty()
                    ? QString("Module-specific arguments object (see panel UI for fields).")
                    : QString("Module-specific arguments for '%1'. See panel UI for fields.").arg(command_hint))
        .build();
}

} // namespace

std::vector<ToolDef> get_quant_lab_tools() {
    std::vector<ToolDef> tools;
    tools.reserve(kCatalogSize + 3);

    // ── Generic: list_quant_modules ──────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_quant_modules";
        t.description = "List all AI Quant Lab modules (id, category, description, script).";
        t.category = "quant-lab";
        t.handler = [](const QJsonObject&) -> ToolResult {
            QJsonArray arr;
            for (const auto& m : services::quant::all_quant_modules()) {
                arr.append(QJsonObject{
                    {"id", m.id},
                    {"label", m.label},
                    {"short_label", m.short_label},
                    {"category", m.category},
                    {"description", m.description},
                    {"script", m.script},
                });
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // ── Generic: list_quant_module_commands ──────────────────────────────
    {
        ToolDef t;
        t.name = "list_quant_module_commands";
        t.description = "List known commands per AI Quant Lab module, with short descriptions.";
        t.category = "quant-lab";
        t.input_schema = ToolSchemaBuilder()
                             .string("module_id", "Filter by module id (empty = all modules)")
                             .default_str("")
                             .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString filter = args["module_id"].toString();
            QJsonObject by_module;
            for (std::size_t i = 0; i < kCatalogSize; ++i) {
                const auto& e = kCatalog[i];
                if (!filter.isEmpty() && filter != QString::fromUtf8(e.module_id))
                    continue;
                QJsonArray cmds = by_module[e.module_id].toArray();
                cmds.append(QJsonObject{
                    {"command", QString::fromUtf8(e.command)},
                    {"tool_name", QString::fromUtf8(e.tool_name)},
                    {"description", QString::fromUtf8(e.description)},
                    {"destructive", e.destructive},
                });
                by_module[QString::fromUtf8(e.module_id)] = cmds;
            }
            return ToolResult::ok_data(by_module);
        };
        tools.push_back(std::move(t));
    }

    // ── Generic: run_quant_module ────────────────────────────────────────
    {
        ToolDef t;
        t.name = "run_quant_module";
        t.description = "Run any AI Quant Lab module/command with custom params (generic dispatcher).";
        t.category = "quant-lab";
        // Stays destructive: it can reach every command, mutating ones included.
        // Prefer the specific quant_* tools, which carry per-command flags.
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
                             .string("module_id", "Module id (use list_quant_modules)")
                             .required()
                             .length(1, 64)
                             .string("command", "Module command (use list_quant_module_commands)")
                             .required()
                             .length(1, 64)
                             .object("params", "Module-specific arguments object")
                             .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString module_id = args["module_id"].toString();
            const QString command = args["command"].toString();
            const QJsonObject params = args["params"].toObject();
            dispatch_quant_async(module_id, command, params, std::move(ctx), promise);
        };
        tools.push_back(std::move(t));
    }

    // ── Catalog-driven specific tools ────────────────────────────────────
    for (std::size_t i = 0; i < kCatalogSize; ++i) {
        const auto& e = kCatalog[i];
        ToolDef t;
        t.name = QString::fromUtf8(e.tool_name);
        t.description = QString::fromUtf8(e.description);
        t.category = "quant-lab";
        t.is_destructive = e.destructive;
        t.default_timeout_ms = e.timeout_ms;
        t.input_schema = generic_params_schema(QString::fromUtf8(e.command));

        const QString module_id = QString::fromUtf8(e.module_id);
        const QString command = QString::fromUtf8(e.command);
        t.async_handler = [module_id, command](const QJsonObject& args, ToolContext ctx,
                                               std::shared_ptr<QPromise<ToolResult>> promise) {
            dispatch_quant_async(module_id, command, args["params"].toObject(), std::move(ctx), promise);
        };
        tools.push_back(std::move(t));
    }

    LOG_INFO(TAG, QString("Defined %1 quant-lab tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
