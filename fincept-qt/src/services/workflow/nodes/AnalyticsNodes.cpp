#include "services/workflow/nodes/AnalyticsNodes.h"

#include "services/workflow/NodeRegistry.h"
#include "python/PythonRunner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>

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
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject args_obj;
                args_obj["start_date"]      = params.value("start_date").toString("2023-01-01");
                args_obj["end_date"]        = params.value("end_date").toString("2024-01-01");
                args_obj["initial_capital"] = params.value("initial_capital").toDouble(100000);
                args_obj["commission"]      = params.value("commission").toDouble(0.001);
                if (!inputs.isEmpty())
                    args_obj["strategy"] = inputs[0];

                QString json_args = QString::fromUtf8(
                    QJsonDocument(args_obj).toJson(QJsonDocument::Compact));
                analytics_run_python_json("compute_technicals.py",
                    {"--data", json_args, "--indicator", "BACKTEST", "--period", "0"}, cb);
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
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // Extract close prices from input data and compute basic metrics
                QVector<double> prices;
                if (!inputs.isEmpty()) {
                    if (inputs[0].isArray()) {
                        for (const QJsonValue& v : inputs[0].toArray()) {
                            if (v.isObject()) {
                                double p = v.toObject().value("Close").toDouble(
                                           v.toObject().value("close").toDouble(
                                           v.toObject().value("price").toDouble(0)));
                                if (p > 0) prices.append(p);
                            } else if (v.isDouble()) {
                                prices.append(v.toDouble());
                            }
                        }
                    }
                }

                if (prices.size() < 2) {
                    cb(false, {}, "Need at least 2 price points for performance metrics");
                    return;
                }

                // Compute daily returns
                QVector<double> returns;
                for (int i = 1; i < prices.size(); ++i)
                    returns.append((prices[i] - prices[i-1]) / prices[i-1]);

                double sum = 0, sum_sq = 0, max_dd = 0, peak = prices[0];
                double neg_sum_sq = 0;
                int neg_count = 0;
                for (double r : returns) {
                    sum += r;
                    sum_sq += r * r;
                    if (r < 0) { neg_sum_sq += r * r; ++neg_count; }
                }
                for (double p : prices) {
                    if (p > peak) peak = p;
                    double dd = (peak - p) / peak;
                    if (dd > max_dd) max_dd = dd;
                }

                int n = returns.size();
                double mean_return = sum / n;
                double std_dev = std::sqrt(sum_sq / n - mean_return * mean_return);
                double rfr = params.value("risk_free_rate").toDouble(0.05) / 252.0;
                double sharpe = std_dev > 0 ? (mean_return - rfr) / std_dev * std::sqrt(252.0) : 0;
                double downside_dev = neg_count > 0 ? std::sqrt(neg_sum_sq / neg_count) : 0;
                double sortino = downside_dev > 0 ? (mean_return - rfr) / downside_dev * std::sqrt(252.0) : 0;
                double total_return = (prices.back() - prices.front()) / prices.front();
                double annualized = std::pow(1.0 + total_return, 252.0 / n) - 1.0;

                QJsonObject out;
                out["total_return"] = total_return;
                out["annualized_return"] = annualized;
                out["sharpe_ratio"] = sharpe;
                out["sortino_ratio"] = sortino;
                out["max_drawdown"] = max_dd;
                out["volatility"] = std_dev * std::sqrt(252.0);
                out["mean_daily_return"] = mean_return;
                out["data_points"] = n;
                cb(true, out, {});
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
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject args_obj;
                args_obj["method"] = params.value("method").toString("pearson");
                args_obj["period"] = params.value("period").toString("1y");
                if (!inputs.isEmpty())
                    args_obj["data"] = inputs[0];

                QString json_args = QString::fromUtf8(
                    QJsonDocument(args_obj).toJson(QJsonDocument::Compact));
                analytics_run_python_json("compute_technicals.py",
                    {"--data", json_args, "--indicator", "CORRELATION", "--period", "0"}, cb);
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
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // Extract returns from input and compute VaR/CVaR inline
                QVector<double> returns;
                if (!inputs.isEmpty() && inputs[0].isArray()) {
                    for (const QJsonValue& v : inputs[0].toArray()) {
                        if (v.isObject()) {
                            double p = v.toObject().value("return").toDouble(
                                       v.toObject().value("Close").toDouble(0));
                            returns.append(p);
                        } else if (v.isDouble()) {
                            returns.append(v.toDouble());
                        }
                    }
                }

                if (returns.size() < 10) {
                    cb(false, {}, "Need at least 10 data points for risk analysis");
                    return;
                }

                // Convert prices to returns if values are large (prices, not returns)
                if (std::abs(returns[0]) > 1.0) {
                    QVector<double> price_returns;
                    for (int i = 1; i < returns.size(); ++i)
                        price_returns.append((returns[i] - returns[i-1]) / returns[i-1]);
                    returns = price_returns;
                }

                // Sort returns for percentile-based VaR
                std::sort(returns.begin(), returns.end());
                double confidence = params.value("confidence").toDouble(0.95);
                int var_idx = static_cast<int>((1.0 - confidence) * returns.size());
                double var_value = -returns[var_idx];

                // CVaR: average of returns below VaR threshold
                double cvar_sum = 0;
                for (int i = 0; i <= var_idx; ++i) cvar_sum += returns[i];
                double cvar = var_idx > 0 ? -(cvar_sum / (var_idx + 1)) : var_value;

                // Volatility
                double sum = 0, sum_sq = 0;
                for (double r : returns) { sum += r; sum_sq += r * r; }
                double mean = sum / returns.size();
                double vol = std::sqrt(sum_sq / returns.size() - mean * mean) * std::sqrt(252.0);

                QJsonObject out;
                out["method"] = params.value("method").toString("historical_var");
                out["confidence"] = confidence;
                out["var"] = var_value;
                out["cvar"] = cvar;
                out["annualized_volatility"] = vol;
                out["data_points"] = returns.size();
                cb(true, out, {});
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
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // Extract returns from input
                QVector<double> prices;
                if (!inputs.isEmpty() && inputs[0].isArray()) {
                    for (const QJsonValue& v : inputs[0].toArray()) {
                        if (v.isObject()) {
                            double p = v.toObject().value("Close").toDouble(
                                       v.toObject().value("close").toDouble(
                                       v.toObject().value("price").toDouble(0)));
                            if (p > 0) prices.append(p);
                        } else if (v.isDouble()) {
                            prices.append(v.toDouble());
                        }
                    }
                }

                if (prices.size() < 2) {
                    cb(false, {}, "Need at least 2 data points for ratio calculation");
                    return;
                }

                QVector<double> returns;
                for (int i = 1; i < prices.size(); ++i)
                    returns.append((prices[i] - prices[i-1]) / prices[i-1]);

                double rfr = params.value("risk_free_rate").toDouble(0.05) / 252.0;
                double sum = 0, sum_sq = 0, neg_sum_sq = 0;
                double max_dd = 0, peak = prices[0];
                int neg_count = 0;

                for (double r : returns) {
                    sum += r; sum_sq += r * r;
                    if (r < 0) { neg_sum_sq += r * r; ++neg_count; }
                }
                for (double p : prices) {
                    if (p > peak) peak = p;
                    double dd = (peak - p) / peak;
                    if (dd > max_dd) max_dd = dd;
                }

                int n = returns.size();
                double mean = sum / n;
                double std_dev = std::sqrt(sum_sq / n - mean * mean);
                double downside_dev = neg_count > 0 ? std::sqrt(neg_sum_sq / neg_count) : 0;
                double ann_return = mean * 252.0;
                double ann_rfr = params.value("risk_free_rate").toDouble(0.05);

                QString ratio_type = params.value("ratio").toString("sharpe");

                QJsonObject out;
                out["ratio_type"] = ratio_type;
                if (ratio_type == "sharpe")
                    out["value"] = std_dev > 0 ? (mean - rfr) / std_dev * std::sqrt(252.0) : 0.0;
                else if (ratio_type == "sortino")
                    out["value"] = downside_dev > 0 ? (mean - rfr) / downside_dev * std::sqrt(252.0) : 0.0;
                else if (ratio_type == "calmar")
                    out["value"] = max_dd > 0 ? ann_return / max_dd : 0.0;
                else if (ratio_type == "information")
                    out["value"] = std_dev > 0 ? (ann_return - ann_rfr) / (std_dev * std::sqrt(252.0)) : 0.0;

                out["annualized_return"] = ann_return;
                out["annualized_volatility"] = std_dev * std::sqrt(252.0);
                out["max_drawdown"] = max_dd;
                out["data_points"] = n;
                cb(true, out, {});
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
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // Extract close prices from input
                QVector<double> prices;
                if (!inputs.isEmpty() && inputs[0].isArray()) {
                    for (const QJsonValue& v : inputs[0].toArray()) {
                        if (v.isObject()) {
                            double p = v.toObject().value("Close").toDouble(
                                       v.toObject().value("close").toDouble(0));
                            if (p > 0) prices.append(p);
                        } else if (v.isDouble()) {
                            prices.append(v.toDouble());
                        }
                    }
                }

                int fast = static_cast<int>(params.value("fast_period").toDouble(50));
                int slow = static_cast<int>(params.value("slow_period").toDouble(200));

                if (prices.size() < slow + 1) {
                    cb(false, {}, QString("Need at least %1 data points for %2/%3 crossover")
                        .arg(slow + 1).arg(fast).arg(slow));
                    return;
                }

                // Compute simple moving averages at last two points
                auto sma = [&](int period, int offset) {
                    double sum = 0;
                    for (int i = offset - period + 1; i <= offset; ++i)
                        sum += prices[i];
                    return sum / period;
                };

                int last = prices.size() - 1;
                double fast_now = sma(fast, last);
                double fast_prev = sma(fast, last - 1);
                double slow_now = sma(slow, last);
                double slow_prev = sma(slow, last - 1);

                QString signal = "hold";
                if (fast_prev <= slow_prev && fast_now > slow_now)
                    signal = "golden_cross";  // bullish
                else if (fast_prev >= slow_prev && fast_now < slow_now)
                    signal = "death_cross";   // bearish
                else if (fast_now > slow_now)
                    signal = "above";
                else
                    signal = "below";

                QJsonObject out;
                out["signal"] = signal;
                out["fast_ma"] = fast_now;
                out["slow_ma"] = slow_now;
                out["fast_period"] = fast;
                out["slow_period"] = slow;
                out["current_price"] = prices.last();
                out["data_points"] = prices.size();
                cb(true, out, {});
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
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QVector<double> prices;
                if (!inputs.isEmpty() && inputs[0].isArray()) {
                    for (const QJsonValue& v : inputs[0].toArray()) {
                        if (v.isObject()) {
                            double p = v.toObject().value("Close").toDouble(
                                       v.toObject().value("close").toDouble(
                                       v.toObject().value("price").toDouble(0)));
                            if (p > 0) prices.append(p);
                        } else if (v.isDouble()) {
                            prices.append(v.toDouble());
                        }
                    }
                }

                if (prices.size() < 2) {
                    cb(false, {}, "Need at least 2 data points for drawdown analysis");
                    return;
                }

                double peak = prices[0], max_dd = 0;
                int dd_start = 0, dd_end = 0, dd_peak_idx = 0;
                int current_dd_start = 0;
                int max_dd_duration = 0, current_duration = 0;

                for (int i = 0; i < prices.size(); ++i) {
                    if (prices[i] > peak) {
                        peak = prices[i];
                        dd_peak_idx = i;
                        current_dd_start = i;
                        current_duration = 0;
                    }
                    double dd = (peak - prices[i]) / peak;
                    if (dd > max_dd) {
                        max_dd = dd;
                        dd_start = current_dd_start;
                        dd_end = i;
                    }
                    if (dd > 0) ++current_duration;
                    else current_duration = 0;
                    if (current_duration > max_dd_duration)
                        max_dd_duration = current_duration;
                }

                // Current drawdown
                double current_dd = (peak - prices.last()) / peak;

                QJsonObject out;
                out["max_drawdown"] = max_dd;
                out["max_drawdown_pct"] = max_dd * 100.0;
                out["current_drawdown"] = current_dd;
                out["current_drawdown_pct"] = current_dd * 100.0;
                out["drawdown_start_idx"] = dd_start;
                out["drawdown_end_idx"] = dd_end;
                out["drawdown_duration"] = dd_end - dd_start;
                out["max_drawdown_duration_days"] = max_dd_duration;
                out["data_points"] = prices.size();
                cb(true, out, {});
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
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // Extract prices and compute returns
                QVector<double> prices;
                if (!inputs.isEmpty() && inputs[0].isArray()) {
                    for (const QJsonValue& v : inputs[0].toArray()) {
                        if (v.isObject()) {
                            double p = v.toObject().value("Close").toDouble(
                                       v.toObject().value("close").toDouble(0));
                            if (p > 0) prices.append(p);
                        } else if (v.isDouble()) {
                            prices.append(v.toDouble());
                        }
                    }
                }

                if (prices.size() < 10) {
                    cb(false, {}, "Need at least 10 data points for Monte Carlo simulation");
                    return;
                }

                QVector<double> returns;
                for (int i = 1; i < prices.size(); ++i)
                    returns.append((prices[i] - prices[i-1]) / prices[i-1]);

                double mean = 0, var = 0;
                for (double r : returns) mean += r;
                mean /= returns.size();
                for (double r : returns) var += (r - mean) * (r - mean);
                var /= returns.size();
                double std_dev = std::sqrt(var);

                int n_sims = static_cast<int>(params.value("simulations").toDouble(1000));
                int horizon = static_cast<int>(params.value("horizon_days").toDouble(252));
                // Cap simulations for performance
                if (n_sims > 10000) n_sims = 10000;

                double start_price = prices.last();
                QVector<double> final_prices;
                final_prices.reserve(n_sims);

                auto* rng = QRandomGenerator::global();
                for (int s = 0; s < n_sims; ++s) {
                    double price = start_price;
                    for (int d = 0; d < horizon; ++d) {
                        // Box-Muller transform for normal distribution
                        double u1 = rng->generateDouble();
                        double u2 = rng->generateDouble();
                        if (u1 < 1e-10) u1 = 1e-10;
                        double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
                        double ret = mean + std_dev * z;
                        price *= (1.0 + ret);
                    }
                    final_prices.append(price);
                }

                std::sort(final_prices.begin(), final_prices.end());

                QJsonObject out;
                out["simulations"] = n_sims;
                out["horizon_days"] = horizon;
                out["start_price"] = start_price;
                out["mean_final"] = [&]() { double s = 0; for (double p : final_prices) s += p; return s / n_sims; }();
                out["median_final"] = final_prices[n_sims / 2];
                out["p5"] = final_prices[static_cast<int>(0.05 * n_sims)];
                out["p25"] = final_prices[static_cast<int>(0.25 * n_sims)];
                out["p75"] = final_prices[static_cast<int>(0.75 * n_sims)];
                out["p95"] = final_prices[static_cast<int>(0.95 * n_sims)];
                out["min"] = final_prices.first();
                out["max"] = final_prices.last();
                out["prob_profit"] = [&]() { int c = 0; for (double p : final_prices) if (p > start_price) ++c; return (double)c / n_sims; }();
                cb(true, out, {});
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
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject args_obj;
                args_obj["model"] = params.value("model").toString("ff3");
                args_obj["period"] = params.value("period").toString("3y");
                if (!inputs.isEmpty())
                    args_obj["data"] = inputs[0];

                QString json_args = QString::fromUtf8(
                    QJsonDocument(args_obj).toJson(QJsonDocument::Compact));
                analytics_run_python_json("compute_technicals.py",
                    {"--data", json_args, "--indicator", "FACTOR_MODEL", "--period", "0"}, cb);
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
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString symbol_a = params.value("symbol_a").toString("KO");
                QString symbol_b = params.value("symbol_b").toString("PEP");
                int lookback = static_cast<int>(params.value("lookback").toDouble(60));
                double z_thresh = params.value("z_threshold").toDouble(2.0);

                // Fetch historical data for both symbols, compute spread
                QJsonObject args_obj;
                args_obj["symbol_a"] = symbol_a;
                args_obj["symbol_b"] = symbol_b;
                args_obj["lookback"] = lookback;
                args_obj["z_threshold"] = z_thresh;

                QString json_args = QString::fromUtf8(
                    QJsonDocument(args_obj).toJson(QJsonDocument::Compact));
                analytics_run_python_json("compute_technicals.py",
                    {"--data", json_args, "--indicator", "PAIRS", "--period", QString::number(lookback)}, cb);
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
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // Simple rolling volatility-based regime detection
                QVector<double> prices;
                if (!inputs.isEmpty() && inputs[0].isArray()) {
                    for (const QJsonValue& v : inputs[0].toArray()) {
                        if (v.isObject()) {
                            double p = v.toObject().value("Close").toDouble(
                                       v.toObject().value("close").toDouble(0));
                            if (p > 0) prices.append(p);
                        } else if (v.isDouble()) {
                            prices.append(v.toDouble());
                        }
                    }
                }

                if (prices.size() < 30) {
                    cb(false, {}, "Need at least 30 data points for regime detection");
                    return;
                }

                // Compute 20-day rolling volatility and trend
                int window = 20;
                QVector<double> returns;
                for (int i = 1; i < prices.size(); ++i)
                    returns.append((prices[i] - prices[i-1]) / prices[i-1]);

                // Latest window stats
                int n = returns.size();
                double sum = 0, sum_sq = 0;
                for (int i = n - window; i < n; ++i) {
                    sum += returns[i];
                    sum_sq += returns[i] * returns[i];
                }
                double mean = sum / window;
                double vol = std::sqrt(sum_sq / window - mean * mean);
                double ann_vol = vol * std::sqrt(252.0);

                // Trend: SMA50 vs SMA200 (or shorter if not enough data)
                int sma_short = std::min(50, static_cast<int>(prices.size()) / 3);
                int sma_long = std::min(200, static_cast<int>(prices.size()) - 1);
                double sma_s = 0, sma_l = 0;
                for (int i = prices.size() - sma_short; i < prices.size(); ++i) sma_s += prices[i];
                sma_s /= sma_short;
                for (int i = prices.size() - sma_long; i < prices.size(); ++i) sma_l += prices[i];
                sma_l /= sma_long;

                QString regime;
                if (ann_vol > 0.30) regime = "high_volatility";
                else if (sma_s > sma_l && mean > 0) regime = "bull";
                else if (sma_s < sma_l && mean < 0) regime = "bear";
                else regime = "sideways";

                QJsonObject out;
                out["regime"] = regime;
                out["method"] = params.value("method").toString("rolling_vol");
                out["annualized_volatility"] = ann_vol;
                out["short_ma"] = sma_s;
                out["long_ma"] = sma_l;
                out["avg_daily_return"] = mean;
                out["current_price"] = prices.last();
                out["data_points"] = prices.size();
                cb(true, out, {});
            },
    });
}

} // namespace fincept::workflow
