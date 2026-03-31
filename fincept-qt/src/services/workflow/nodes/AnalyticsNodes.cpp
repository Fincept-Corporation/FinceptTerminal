#include "services/workflow/nodes/AnalyticsNodes.h"

#include "services/workflow/NodeRegistry.h"
#include "python/PythonRunner.h"

#include <QJsonDocument>

namespace fincept::workflow {

using fincept::python::PythonRunner;
using fincept::python::PythonResult;
using fincept::python::extract_json;

namespace {

// Helper: run a Python script and parse the JSON result.
void analytics_run_python_json(const QString& script, const QStringList& args,
                     std::function<void(bool, QJsonValue, QString)> cb)
{
    PythonRunner::instance().run(script, args, [cb](const PythonResult& res) {
        if (!res.success) {
            cb(false, {}, res.error);
            return;
        }
        QString json_str = extract_json(res.output).trimmed();
        auto doc = QJsonDocument::fromJson(json_str.toUtf8());
        if (doc.isNull()) {
            cb(false, {}, "Invalid JSON: " + res.output.left(200));
            return;
        }
        cb(true, doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array()), {});
    });
}

// Helper: produce a clear "not yet implemented" result object.
QJsonValue analytics_not_implemented(const QString& type_id)
{
    QJsonObject obj;
    obj["error"] = "not_yet_implemented";
    obj["node"]  = type_id;
    return obj;
}

} // anonymous namespace

void register_analytics_nodes(NodeRegistry& registry) {
    registry.register_type({
        .type_id = "analytics.technical_indicators",
        .display_name = "Technical Indicators",
        .category = "Analytics",
        .description = "Calculate SMA, RSI, MACD, Bollinger Bands, etc.",
        .icon_text = "A",
        .accent_color = "#7c3aed",
        .version = 1,
        .inputs = {{"input_0", "Price Data", PortDirection::Input, ConnectionType::PriceData}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::TechnicalData}},
        .parameters =
            {
                {"indicator",
                 "Indicator",
                 "select",
                 "SMA",
                 {"SMA", "EMA", "RSI", "MACD", "BBANDS", "ATR", "STOCH", "ADX", "CCI", "WILLR", "OBV", "VWAP"},
                 "",
                 true},
                {"period", "Period", "number", 14, {}, "Lookback period"},
                {"symbol", "Symbol", "string", "", {}, "Ticker symbol"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // Serialise input price data to pass as a JSON string arg.
                QJsonValue input_val = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QJsonDocument input_doc;
                if (input_val.isArray())
                    input_doc = QJsonDocument(input_val.toArray());
                else if (input_val.isObject())
                    input_doc = QJsonDocument(input_val.toObject());

                QString json_data = input_doc.isNull()
                    ? "{}"
                    : QString::fromUtf8(input_doc.toJson(QJsonDocument::Compact));

                QString indicator = params.value("indicator").toString("SMA");
                QString period    = QString::number(static_cast<int>(params.value("period").toDouble(14)));
                QString symbol    = params.value("symbol").toString();

                QStringList args = {"--data", json_data, "--indicator", indicator, "--period", period};
                if (!symbol.isEmpty())
                    args << "--symbol" << symbol;

                analytics_run_python_json("compute_technicals.py", args, cb);
            },
    });

    registry.register_type({
        .type_id = "analytics.backtest",
        .display_name = "Backtest Engine",
        .category = "Analytics",
        .description = "Run backtesting simulation on a trading strategy",
        .icon_text = "A",
        .accent_color = "#7c3aed",
        .version = 1,
        .inputs = {{"input_0", "Strategy", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Results", PortDirection::Output, ConnectionType::BacktestData}},
        .parameters =
            {
                {"start_date", "Start Date", "string", "2023-01-01", {}, "YYYY-MM-DD"},
                {"end_date", "End Date", "string", "2024-01-01", {}, "YYYY-MM-DD"},
                {"initial_capital", "Initial Capital", "number", 100000, {}, ""},
                {"commission", "Commission %", "number", 0.001, {}, ""},
            },
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(false, analytics_not_implemented("analytics.backtest"), "not_yet_implemented");
            },
    });

    registry.register_type({
        .type_id = "analytics.portfolio_optimization",
        .display_name = "Portfolio Optimization",
        .category = "Analytics",
        .description = "Optimize portfolio allocation (mean-variance, Black-Litterman)",
        .icon_text = "A",
        .accent_color = "#7c3aed",
        .version = 1,
        .inputs = {{"input_0", "Holdings", PortDirection::Input, ConnectionType::PortfolioData}},
        .outputs = {{"output_main", "Optimized", PortDirection::Output, ConnectionType::PortfolioData}},
        .parameters =
            {
                {"method",
                 "Method",
                 "select",
                 "mean_variance",
                 {"mean_variance", "black_litterman", "risk_parity", "min_variance", "max_sharpe"},
                 ""},
                {"risk_free_rate", "Risk-Free Rate", "number", 0.05, {}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // Build a JSON args object containing the input data and params.
                QJsonObject args_obj;
                args_obj["method"]         = params.value("method").toString("mean_variance");
                args_obj["risk_free_rate"] = params.value("risk_free_rate").toDouble(0.05);
                if (!inputs.isEmpty())
                    args_obj["holdings"] = inputs[0];

                QString json_args = QString::fromUtf8(
                    QJsonDocument(args_obj).toJson(QJsonDocument::Compact));

                analytics_run_python_json("optimize_portfolio_weights.py", {"--args", json_args}, cb);
            },
    });

    registry.register_type({
        .type_id = "analytics.performance_metrics",
        .display_name = "Performance Metrics",
        .category = "Analytics",
        .description = "Calculate returns, Sharpe, Sortino, max drawdown",
        .icon_text = "A",
        .accent_color = "#7c3aed",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"benchmark", "Benchmark", "string", "SPY", {}, "Benchmark symbol"},
                {"risk_free_rate", "Risk-Free Rate", "number", 0.05, {}, ""},
            },
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(false, analytics_not_implemented("analytics.performance_metrics"), "not_yet_implemented");
            },
    });

    registry.register_type({
        .type_id = "analytics.correlation_matrix",
        .display_name = "Correlation Matrix",
        .category = "Analytics",
        .description = "Calculate asset correlation matrix",
        .icon_text = "A",
        .accent_color = "#7c3aed",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::PriceData}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"method", "Method", "select", "pearson", {"pearson", "spearman", "kendall"}, ""},
                {"period", "Period", "string", "1y", {}, "Lookback period"},
            },
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(false, analytics_not_implemented("analytics.correlation_matrix"), "not_yet_implemented");
            },
    });

    registry.register_type({
        .type_id = "analytics.risk_analysis",
        .display_name = "Risk Analysis",
        .category = "Analytics",
        .description = "VaR, CVaR, stress testing, Monte Carlo simulation",
        .icon_text = "A",
        .accent_color = "#7c3aed",
        .version = 1,
        .inputs = {{"input_0", "Portfolio", PortDirection::Input, ConnectionType::PortfolioData}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::RiskData}},
        .parameters =
            {
                {"method",
                 "Method",
                 "select",
                 "historical_var",
                 {"historical_var", "parametric_var", "monte_carlo", "stress_test"},
                 ""},
                {"confidence", "Confidence Level", "number", 0.95, {}, ""},
                {"horizon", "Horizon (days)", "number", 1, {}, ""},
            },
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(false, analytics_not_implemented("analytics.risk_analysis"), "not_yet_implemented");
            },
    });

    // ── Tier 1 additions ───────────────────────────────────────────

    registry.register_type({
        .type_id = "analytics.sharpe_ratio",
        .display_name = "Sharpe / Sortino",
        .category = "Analytics",
        .description = "Calculate Sharpe, Sortino, Calmar ratios",
        .icon_text = "A",
        .accent_color = "#7c3aed",
        .version = 1,
        .inputs = {{"input_0", "Returns", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"ratio", "Ratio", "select", "sharpe", {"sharpe", "sortino", "calmar", "information"}, ""},
                {"risk_free_rate", "Risk-Free Rate", "number", 0.05, {}, ""},
                {"benchmark", "Benchmark", "string", "SPY", {}, ""},
            },
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(false, analytics_not_implemented("analytics.sharpe_ratio"), "not_yet_implemented");
            },
    });

    registry.register_type({
        .type_id = "analytics.ma_crossover",
        .display_name = "MA Crossover",
        .category = "Analytics",
        .description = "Detect moving average crossover signals (golden/death cross)",
        .icon_text = "A",
        .accent_color = "#7c3aed",
        .version = 1,
        .inputs = {{"input_0", "Price Data", PortDirection::Input, ConnectionType::PriceData}},
        .outputs =
            {
                {"output_signal", "Signal", PortDirection::Output, ConnectionType::SignalData},
                {"output_data", "Data", PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"fast_period", "Fast MA Period", "number", 50, {}, ""},
                {"slow_period", "Slow MA Period", "number", 200, {}, ""},
                {"ma_type", "MA Type", "select", "SMA", {"SMA", "EMA", "WMA"}, ""},
            },
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(false, analytics_not_implemented("analytics.ma_crossover"), "not_yet_implemented");
            },
    });

    registry.register_type({
        .type_id = "analytics.drawdown",
        .display_name = "Max Drawdown",
        .category = "Analytics",
        .description = "Calculate maximum drawdown, drawdown duration, recovery time",
        .icon_text = "A",
        .accent_color = "#7c3aed",
        .version = 1,
        .inputs = {{"input_0", "Returns", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"window", "Window", "string", "all", {}, "'all' or number of days"},
            },
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(false, analytics_not_implemented("analytics.drawdown"), "not_yet_implemented");
            },
    });

    registry.register_type({
        .type_id = "analytics.monte_carlo",
        .display_name = "Monte Carlo Sim",
        .category = "Analytics",
        .description = "Monte Carlo simulation for portfolio returns",
        .icon_text = "A",
        .accent_color = "#7c3aed",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"simulations", "Simulations", "number", 10000, {}, ""},
                {"horizon_days", "Horizon (days)", "number", 252, {}, ""},
                {"confidence", "Confidence Levels", "string", "0.95,0.99", {}, "Comma-separated"},
            },
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(false, analytics_not_implemented("analytics.monte_carlo"), "not_yet_implemented");
            },
    });

    // ── Tier 3: Advanced Analytics ─────────────────────────────────

    registry.register_type({
        .type_id = "analytics.factor_model",
        .display_name = "Factor Model",
        .category = "Analytics",
        .description = "Fama-French factor decomposition (3/5 factor)",
        .icon_text = "A",
        .accent_color = "#7c3aed",
        .version = 1,
        .inputs = {{"input_0", "Returns", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"model", "Model", "select", "ff3", {"ff3", "ff5", "capm", "carhart4"}, ""},
                {"period", "Period", "string", "3y", {}, ""},
            },
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(false, analytics_not_implemented("analytics.factor_model"), "not_yet_implemented");
            },
    });

    registry.register_type({
        .type_id = "analytics.pairs_trading",
        .display_name = "Pairs Trading",
        .category = "Analytics",
        .description = "Cointegration test + pairs trading signal generation",
        .icon_text = "A",
        .accent_color = "#7c3aed",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::PriceData}},
        .outputs =
            {
                {"output_signal", "Signal", PortDirection::Output, ConnectionType::SignalData},
                {"output_data", "Spread Data", PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"symbol_a", "Symbol A", "string", "KO", {}, "", true},
                {"symbol_b", "Symbol B", "string", "PEP", {}, "", true},
                {"lookback", "Lookback Days", "number", 60, {}, ""},
                {"z_threshold", "Z-Score Threshold", "number", 2.0, {}, ""},
            },
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(false, analytics_not_implemented("analytics.pairs_trading"), "not_yet_implemented");
            },
    });

    registry.register_type({
        .type_id = "analytics.regime_detection",
        .display_name = "Regime Detection",
        .category = "Analytics",
        .description = "Detect market regime (bull/bear/sideways) using HMM",
        .icon_text = "A",
        .accent_color = "#7c3aed",
        .version = 1,
        .inputs = {{"input_0", "Price Data", PortDirection::Input, ConnectionType::PriceData}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"n_regimes", "Number of Regimes", "number", 3, {}, ""},
                {"method", "Method", "select", "hmm", {"hmm", "rolling_vol", "trend_following"}, ""},
            },
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(false, analytics_not_implemented("analytics.regime_detection"), "not_yet_implemented");
            },
    });
}

} // namespace fincept::workflow
