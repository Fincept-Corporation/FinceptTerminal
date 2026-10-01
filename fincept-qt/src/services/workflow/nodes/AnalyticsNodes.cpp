#include "services/workflow/nodes/AnalyticsNodes.h"

#include "python/PythonRunner.h"
#include "services/workflow/NodeRegistry.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>

#include <QJsonObject>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::workflow {

using fincept::python::extract_json;
using fincept::python::PythonResult;
using fincept::python::PythonRunner;

namespace {

// Helper: run a Python script and parse the JSON result.
void analytics_run_python_json(const QString& script, const QStringList& args,
                               std::function<void(bool, QJsonValue, QString)> cb) {
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
        // Check for Python-level error: {"success": false, "error": "..."}
        if (doc.isObject()) {
            auto obj = doc.object();
            if (obj.contains("success") && !obj.value("success").toBool(true)) {
                cb(false, {}, obj.value("error").toString("Python script returned failure"));
                return;
            }
            // Scripts such as optimize_portfolio_weights.py report failure as a bare
            // {"error": "..."} with no "success" key — that used to reach the next node as a
            // SUCCESSFUL result whose payload was just the error text.
            if (obj.contains("error") && !obj.contains("success") && obj.size() <= 2) {
                cb(false, {}, obj.value("error").toString("Script returned an error"));
                return;
            }
        }
        cb(true, doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array()), {});
    });
}

// Fetch daily closes for one symbol through the same yfinance_data.py command the market nodes use.
void an_fetch_closes(const QString& symbol, const QString& period,
                     std::function<void(bool, QVector<double>, QString)> done) {
    PythonRunner::instance().run(
        "yfinance_data.py", {"historical_period", symbol, period, "1d"}, [done, symbol](const PythonResult& res) {
            if (!res.success) {
                done(false, {}, res.error);
                return;
            }
            const auto doc = QJsonDocument::fromJson(extract_json(res.output).trimmed().toUtf8());
            if (!doc.isArray()) {
                done(false, {}, QString("No price history for %1").arg(symbol));
                return;
            }
            QVector<double> closes;
            for (const QJsonValue& v : doc.array()) {
                const double c = v.toObject().value("close").toDouble(0);
                if (c > 0)
                    closes.append(c);
            }
            if (closes.size() < 10) {
                done(false, {}, QString("Not enough price history for %1").arg(symbol));
                return;
            }
            done(true, closes, {});
        });
}

// ── Pure helpers (also exercised by the logic harness) ─────────────────────

// Inverse standard-normal CDF (Acklam's rational approximation, |rel. error| < 1.2e-9).
double an_inv_norm_cdf(double p) {
    if (p <= 0.0)
        return -std::numeric_limits<double>::infinity();
    if (p >= 1.0)
        return std::numeric_limits<double>::infinity();
    static const double a[] = {-3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02,
                               1.383577518672690e+02,  -3.066479806614716e+01, 2.506628277459239e+00};
    static const double b[] = {-5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02,
                               6.680131188771972e+01,  -1.328068155288572e+01};
    static const double c[] = {-7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00,
                               -2.549732539343734e+00, 4.374664141464968e+00,  2.938163982698783e+00};
    static const double d[] = {7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00,
                               3.754408661907416e+00};
    const double p_low = 0.02425;
    if (p < p_low) {
        const double q = std::sqrt(-2.0 * std::log(p));
        return (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
               ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    }
    if (p > 1.0 - p_low) {
        const double q = std::sqrt(-2.0 * std::log(1.0 - p));
        return -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
               ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    }
    const double q = p - 0.5;
    const double r = q * q;
    return (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q /
           (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1.0);
}

// Moving average of `prices` ending at index `offset` (inclusive). type: SMA | EMA | WMA.
// Returns NaN when there is not enough history. Period must be >= 1.
double an_moving_average(const QVector<double>& prices, int period, int offset, const QString& type) {
    if (period < 1 || offset < period - 1 || offset >= prices.size())
        return std::numeric_limits<double>::quiet_NaN();
    if (type == "EMA") {
        // Seed with the SMA of the first `period` prices, then roll forward.
        double ema = 0;
        for (int i = 0; i < period; ++i)
            ema += prices[i];
        ema /= period;
        const double alpha = 2.0 / (period + 1.0);
        for (int i = period; i <= offset; ++i)
            ema = alpha * prices[i] + (1.0 - alpha) * ema;
        return ema;
    }
    if (type == "WMA") {
        double num = 0;
        double den = 0;
        for (int k = 0; k < period; ++k) {
            const double w = k + 1; // most recent price carries the highest weight
            num += w * prices[offset - period + 1 + k];
            den += w;
        }
        return num / den;
    }
    double sum = 0;
    for (int i = offset - period + 1; i <= offset; ++i)
        sum += prices[i];
    return sum / period;
}

// Average ranks (ties share the mean rank) — the Spearman transform.
QVector<double> an_ranks(const QVector<double>& v) {
    const int n = v.size();
    QVector<int> idx(n);
    for (int i = 0; i < n; ++i)
        idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](int x, int y) { return v[x] < v[y]; });
    QVector<double> r(n);
    int i = 0;
    while (i < n) {
        int j = i;
        while (j + 1 < n && v[idx[j + 1]] == v[idx[i]])
            ++j;
        const double avg = (i + j) / 2.0 + 1.0;
        for (int k = i; k <= j; ++k)
            r[idx[k]] = avg;
        i = j + 1;
    }
    return r;
}

double an_pearson(const QVector<double>& x, const QVector<double>& y) {
    const int n = std::min(x.size(), y.size());
    if (n < 2)
        return std::numeric_limits<double>::quiet_NaN();
    double mx = 0;
    double my = 0;
    for (int i = 0; i < n; ++i) {
        mx += x[i];
        my += y[i];
    }
    mx /= n;
    my /= n;
    double sxy = 0;
    double sxx = 0;
    double syy = 0;
    for (int i = 0; i < n; ++i) {
        const double dx = x[i] - mx;
        const double dy = y[i] - my;
        sxy += dx * dy;
        sxx += dx * dx;
        syy += dy * dy;
    }
    if (sxx <= 0 || syy <= 0)
        return std::numeric_limits<double>::quiet_NaN(); // a constant series has no correlation
    return sxy / std::sqrt(sxx * syy);
}

double an_kendall(const QVector<double>& x, const QVector<double>& y) {
    const int n = std::min(x.size(), y.size());
    if (n < 2)
        return std::numeric_limits<double>::quiet_NaN();
    double concordant = 0;
    double discordant = 0;
    double ties_x = 0;
    double ties_y = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            const double dx = x[i] - x[j];
            const double dy = y[i] - y[j];
            if (dx == 0 && dy == 0)
                continue;
            if (dx == 0)
                ++ties_x;
            else if (dy == 0)
                ++ties_y;
            else if ((dx > 0) == (dy > 0))
                ++concordant;
            else
                ++discordant;
        }
    }
    const double denom = std::sqrt((concordant + discordant + ties_x) * (concordant + discordant + ties_y));
    return denom > 0 ? (concordant - discordant) / denom : std::numeric_limits<double>::quiet_NaN();
}

// Correlation matrix of equal-length series. method: pearson | spearman | kendall.
QVector<QVector<double>> an_correlation_matrix(const QVector<QVector<double>>& series, const QString& method) {
    const int k = series.size();
    QVector<QVector<double>> in = series;
    if (method == "spearman") {
        for (auto& s : in)
            s = an_ranks(s);
    }
    QVector<QVector<double>> m(k, QVector<double>(k, 0.0));
    for (int i = 0; i < k; ++i) {
        m[i][i] = 1.0;
        for (int j = i + 1; j < k; ++j) {
            const double c = (method == "kendall") ? an_kendall(in[i], in[j]) : an_pearson(in[i], in[j]);
            m[i][j] = c;
            m[j][i] = c;
        }
    }
    return m;
}

// Price series -> simple returns. Non-positive prices are skipped, never divided by.
QVector<double> an_returns(const QVector<double>& prices) {
    QVector<double> r;
    for (int i = 1; i < prices.size(); ++i) {
        if (prices[i - 1] > 0)
            r.append((prices[i] - prices[i - 1]) / prices[i - 1]);
        else
            r.append(0.0);
    }
    return r;
}

// Pull the close-price column out of a history row set (OHLCV objects, or bare numbers).
QVector<double> an_closes(const QJsonValue& rows) {
    QVector<double> out;
    if (!rows.isArray())
        return out;
    for (const QJsonValue& v : rows.toArray()) {
        if (v.isObject()) {
            const QJsonObject o = v.toObject();
            double p = o.value("close").toDouble(o.value("Close").toDouble(o.value("price").toDouble(0)));
            if (p > 0)
                out.append(p);
        } else if (v.isDouble() && v.toDouble() > 0) {
            out.append(v.toDouble());
        }
    }
    return out;
}

// Spread statistics for a pair: OLS hedge ratio of A on B over the last `lookback` closes and
// the z-score of the latest residual. Returns false when there is not enough data.
struct AnPairStats {
    double hedge_ratio = 0;
    double spread = 0;
    double spread_mean = 0;
    double spread_std = 0;
    double zscore = 0;
    int observations = 0;
};
bool an_pair_stats(const QVector<double>& a, const QVector<double>& b, int lookback, AnPairStats* out) {
    const int n_all = std::min(a.size(), b.size());
    const int n = std::min(n_all, std::max(lookback, 10));
    if (n < 10)
        return false;
    const int off_a = a.size() - n;
    const int off_b = b.size() - n;
    double mean_a = 0;
    double mean_b = 0;
    for (int i = 0; i < n; ++i) {
        mean_a += a[off_a + i];
        mean_b += b[off_b + i];
    }
    mean_a /= n;
    mean_b /= n;
    double sab = 0;
    double sbb = 0;
    for (int i = 0; i < n; ++i) {
        sab += (a[off_a + i] - mean_a) * (b[off_b + i] - mean_b);
        sbb += (b[off_b + i] - mean_b) * (b[off_b + i] - mean_b);
    }
    if (sbb <= 0)
        return false;
    const double beta = sab / sbb;
    QVector<double> resid(n);
    double rm = 0;
    for (int i = 0; i < n; ++i) {
        resid[i] = a[off_a + i] - beta * b[off_b + i];
        rm += resid[i];
    }
    rm /= n;
    double var = 0;
    for (double r : resid)
        var += (r - rm) * (r - rm);
    const double sd = std::sqrt(var / (n - 1));
    out->hedge_ratio = beta;
    out->spread = resid.last();
    out->spread_mean = rm;
    out->spread_std = sd;
    out->zscore = sd > 0 ? (resid.last() - rm) / sd : 0.0;
    out->observations = n;
    return true;
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

                QString json_data =
                    input_doc.isNull() ? "{}" : QString::fromUtf8(input_doc.toJson(QJsonDocument::Compact));

                QString indicator = params.value("indicator").toString("SMA");
                QString period = QString::number(static_cast<int>(params.value("period").toDouble(14)));
                QString symbol = params.value("symbol").toString();

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
            [](const QJsonObject&, const QVector<QJsonValue>&, std::function<void(bool, QJsonValue, QString)> cb) {
                // This node used to call compute_technicals.py with --indicator BACKTEST. That
                // script ignores --indicator and treats --data as an OHLCV history, so every run
                // ended in a pandas error ("If using all scalar values, you must pass an index").
                // Say what is true instead of surfacing that.
                cb(false, {},
                   "Backtest Engine is not available in the workflow runner — run the strategy from the "
                   "Backtesting screen");
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
                // optimize_portfolio_weights.py wants {"symbols": [...], "method": ...}; this node
                // used to send {"holdings": <raw input>} and so always got "No symbols provided".
                QStringList symbols;
                QVector<double> values; // position value per symbol, for the current weights
                auto add_symbol = [&](const QString& raw, double value) {
                    const QString s = raw.trimmed();
                    if (s.isEmpty())
                        return;
                    const int at = static_cast<int>(symbols.indexOf(s));
                    if (at >= 0) {
                        values[at] += value;
                        return;
                    }
                    symbols << s;
                    values << value;
                };
                auto position_value = [](const QJsonObject& o) {
                    const double v = o.value("current_value").toDouble(0);
                    if (v > 0)
                        return v;
                    const double px = o.value("ltp").toDouble(o.value("current_price").toDouble(o.value("avg_price").toDouble(o.value("price").toDouble(0))));
                    return std::abs(o.value("quantity").toDouble(0)) * px;
                };
                if (!inputs.isEmpty()) {
                    const QJsonValue in = inputs[0];
                    if (in.isArray()) {
                        for (const QJsonValue& v : in.toArray()) {
                            if (v.isObject())
                                add_symbol(v.toObject().value("symbol").toString(), position_value(v.toObject()));
                            else if (v.isString())
                                add_symbol(v.toString(), 0);
                        }
                    } else if (in.isObject()) {
                        for (const QJsonValue& v : in.toObject().value("symbols").toArray())
                            add_symbol(v.toString(), 0);
                    }
                }
                if (symbols.size() < 2) {
                    cb(false, {},
                       "Portfolio Optimization needs at least 2 symbols — connect Get Holdings / Get Positions "
                       "(or any array of records with a \"symbol\" field)");
                    return;
                }

                // The select offers names the script does not know.
                QString method = params.value("method").toString("mean_variance");
                if (method == "mean_variance")
                    method = "max_sharpe";
                else if (method == "min_variance")
                    method = "min_volatility";

                QJsonObject args_obj;
                args_obj["symbols"] = QJsonArray::fromStringList(symbols);
                args_obj["method"] = method;
                args_obj["risk_free_rate"] = params.value("risk_free_rate").toDouble(0.05);
                double total_value = 0;
                for (double v : values)
                    total_value += v;
                if (total_value > 0) {
                    QJsonArray weights;
                    for (double v : values)
                        weights.append(v / total_value);
                    args_obj["weights"] = weights; // current allocation = the market proxy for implied returns
                }

                QString json_args = QString::fromUtf8(QJsonDocument(args_obj).toJson(QJsonDocument::Compact));

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
                                    v.toObject().value("close").toDouble(v.toObject().value("price").toDouble(0)));
                                if (p > 0)
                                    prices.append(p);
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
                    returns.append((prices[i] - prices[i - 1]) / prices[i - 1]);

                double sum = 0, sum_sq = 0, max_dd = 0, peak = prices[0];
                double neg_sum_sq = 0;
                int neg_count = 0;
                for (double r : returns) {
                    sum += r;
                    sum_sq += r * r;
                    if (r < 0) {
                        neg_sum_sq += r * r;
                        ++neg_count;
                    }
                }
                for (double p : prices) {
                    if (p > peak)
                        peak = p;
                    double dd = (peak - p) / peak;
                    if (dd > max_dd)
                        max_dd = dd;
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
                // compute_technicals.py has no CORRELATION mode (it ignored --indicator and failed
                // on any input that is not an OHLCV history), so this node always errored. The
                // matrix is computed here from the series the upstream nodes delivered.
                QString method = params.value("method").toString("pearson");
                if (method != "pearson" && method != "spearman" && method != "kendall")
                    method = "pearson";
                if (inputs.isEmpty()) {
                    cb(false, {}, "Correlation Matrix needs input data (connect Historical Data nodes via Merge)");
                    return;
                }

                // Series groups: several direct inputs, a Merge output ([[rows...], [rows...]]),
                // or one history / table.
                QJsonArray groups;
                if (inputs.size() > 1) {
                    for (const auto& in : inputs)
                        groups.append(in);
                } else if (inputs[0].isArray() && !inputs[0].toArray().isEmpty() &&
                           inputs[0].toArray().first().isArray()) {
                    groups = inputs[0].toArray();
                } else {
                    groups.append(inputs[0]);
                }

                QVector<QVector<double>> series;
                QStringList labels;
                QString basis = "returns";

                if (groups.size() == 1 && groups[0].isArray() && !groups[0].toArray().isEmpty() &&
                    groups[0].toArray().first().isObject()) {
                    // A single table: every numeric column except the time index is a series.
                    const QJsonArray rows = groups[0].toArray();
                    static const QSet<QString> kTimeCols = {"timestamp", "date", "datetime", "time", "index"};
                    const QStringList cols = rows.first().toObject().keys();
                    const bool has_ohlc = rows.first().toObject().contains("close") ||
                                          rows.first().toObject().contains("Close");
                    if (!has_ohlc) {
                        for (const QString& col : cols) {
                            if (kTimeCols.contains(col.toLower()) || !rows.first().toObject().value(col).isDouble())
                                continue;
                            QVector<double> values;
                            for (const QJsonValue& r : rows)
                                values.append(r.toObject().value(col).toDouble(std::numeric_limits<double>::quiet_NaN()));
                            series.append(values);
                            labels << col;
                        }
                        basis = "values";
                    }
                }
                if (series.isEmpty()) {
                    int idx = 0;
                    for (const QJsonValue& g : groups) {
                        ++idx;
                        const QVector<double> closes = an_closes(g);
                        if (closes.size() < 3)
                            continue;
                        series.append(an_returns(closes));
                        QString label = QString("series_%1").arg(idx);
                        if (g.isArray() && !g.toArray().isEmpty() && g.toArray().first().isObject()) {
                            const QString sym = g.toArray().first().toObject().value("symbol").toString();
                            if (!sym.isEmpty())
                                label = sym;
                        }
                        labels << label;
                    }
                }

                if (series.size() < 2) {
                    cb(false, {}, "Correlation Matrix needs at least two series with 3+ observations each");
                    return;
                }
                int n = std::numeric_limits<int>::max();
                for (const auto& s : series)
                    n = std::min(n, static_cast<int>(s.size()));
                if (n < 3) {
                    cb(false, {}, "The series are too short to correlate");
                    return;
                }
                if (method == "kendall" && n > 1500)
                    n = 1500; // O(n^2) pair scan — keep the UI responsive
                for (auto& s : series)
                    s = s.mid(s.size() - n); // align on the most recent observations

                const auto matrix = an_correlation_matrix(series, method);
                QJsonArray rows_json;
                for (const auto& row : matrix) {
                    QJsonArray r;
                    for (double v : row)
                        r.append(std::isfinite(v) ? QJsonValue(v) : QJsonValue(QJsonValue::Null));
                    rows_json.append(r);
                }
                QJsonObject out;
                out["method"] = method;
                out["basis"] = basis;
                out["labels"] = QJsonArray::fromStringList(labels);
                out["matrix"] = rows_json;
                out["observations"] = n;
                cb(true, out, {});
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
                            double p = v.toObject().value("return").toDouble(v.toObject().value("Close").toDouble(0));
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
                if (std::abs(returns[0]) > 1.0)
                    returns = an_returns(returns);
                if (returns.size() < 10) {
                    cb(false, {}, "Need at least 10 return observations for risk analysis");
                    return;
                }

                // Confidence arrives as a fraction (0.95) or a percent (95). It used to index
                // the sorted returns directly: 0 or 95 put the index outside the array.
                double confidence = params.value("confidence").toDouble(0.95);
                if (confidence > 1.0 && confidence < 100.0)
                    confidence /= 100.0;
                confidence = qBound(0.5, confidence, 0.9999);
                const int horizon = qBound(1, static_cast<int>(params.value("horizon").toDouble(1)), 252);
                const QString requested = params.value("method").toString("historical_var");
                const int n = static_cast<int>(returns.size());

                double sum = 0, sum_sq = 0;
                for (double r : returns) {
                    sum += r;
                    sum_sq += r * r;
                }
                const double mean = sum / n;
                const double sd = std::sqrt(std::max(0.0, (sum_sq - n * mean * mean) / (n - 1)));

                double var_value = 0;
                double cvar = 0;
                QString used = "historical_var";
                if (requested == "parametric_var") {
                    // Normal VaR / expected shortfall from the sample mean and standard deviation.
                    const double z = an_inv_norm_cdf(confidence);
                    const double pdf = std::exp(-0.5 * z * z) / std::sqrt(2.0 * 3.14159265358979323846);
                    var_value = -(mean - z * sd);
                    cvar = -(mean - sd * pdf / (1.0 - confidence));
                    used = "parametric_var";
                } else {
                    // Percentile of the empirical distribution (also the fallback for the methods
                    // this node does not implement).
                    QVector<double> sorted = returns;
                    std::sort(sorted.begin(), sorted.end());
                    const int var_idx = qBound(0, static_cast<int>((1.0 - confidence) * n), n - 1);
                    var_value = -sorted[var_idx];
                    double tail = 0;
                    for (int i = 0; i <= var_idx; ++i)
                        tail += sorted[i];
                    cvar = -(tail / (var_idx + 1));
                }
                // Square-root-of-time scaling for a multi-day horizon.
                const double h_scale = std::sqrt(static_cast<double>(horizon));
                var_value *= h_scale;
                cvar *= h_scale;

                QJsonObject out;
                out["method"] = used;
                if (requested != used) {
                    out["requested_method"] = requested;
                    out["note"] = QString("'%1' is not implemented in this node; %2 was used instead")
                                      .arg(requested, used);
                }
                out["confidence"] = confidence;
                out["horizon_days"] = horizon;
                out["var"] = var_value;
                out["cvar"] = cvar;
                out["annualized_volatility"] = sd * std::sqrt(252.0);
                out["data_points"] = n;
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
                                v.toObject().value("close").toDouble(v.toObject().value("price").toDouble(0)));
                            if (p > 0)
                                prices.append(p);
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
                    returns.append((prices[i] - prices[i - 1]) / prices[i - 1]);

                double rfr = params.value("risk_free_rate").toDouble(0.05) / 252.0;
                double sum = 0, sum_sq = 0, neg_sum_sq = 0;
                double max_dd = 0, peak = prices[0];
                int neg_count = 0;

                for (double r : returns) {
                    sum += r;
                    sum_sq += r * r;
                    if (r < 0) {
                        neg_sum_sq += r * r;
                        ++neg_count;
                    }
                }
                for (double p : prices) {
                    if (p > peak)
                        peak = p;
                    double dd = (peak - p) / peak;
                    if (dd > max_dd)
                        max_dd = dd;
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
                            double p = v.toObject().value("Close").toDouble(v.toObject().value("close").toDouble(0));
                            if (p > 0)
                                prices.append(p);
                        } else if (v.isDouble()) {
                            prices.append(v.toDouble());
                        }
                    }
                }

                int fast = static_cast<int>(params.value("fast_period").toDouble(50));
                int slow = static_cast<int>(params.value("slow_period").toDouble(200));
                // A period of 0 divided by zero (NaN signal) and a negative one read before the
                // start of the series.
                if (fast < 1 || slow < 1) {
                    cb(false, {}, "MA periods must be at least 1");
                    return;
                }
                const QString ma_type = params.value("ma_type").toString("SMA"); // was ignored (always SMA)

                if (prices.size() < slow + 1 || prices.size() < fast + 1) {
                    cb(false, {},
                       QString("Need at least %1 data points for %2/%3 crossover")
                           .arg(std::max(slow, fast) + 1)
                           .arg(fast)
                           .arg(slow));
                    return;
                }

                // Moving averages at the last two points
                int last = prices.size() - 1;
                double fast_now = an_moving_average(prices, fast, last, ma_type);
                double fast_prev = an_moving_average(prices, fast, last - 1, ma_type);
                double slow_now = an_moving_average(prices, slow, last, ma_type);
                double slow_prev = an_moving_average(prices, slow, last - 1, ma_type);

                QString signal = "hold";
                if (fast_prev <= slow_prev && fast_now > slow_now)
                    signal = "golden_cross"; // bullish
                else if (fast_prev >= slow_prev && fast_now < slow_now)
                    signal = "death_cross"; // bearish
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
                out["ma_type"] = ma_type;
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
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QVector<double> prices;
                if (!inputs.isEmpty() && inputs[0].isArray()) {
                    for (const QJsonValue& v : inputs[0].toArray()) {
                        if (v.isObject()) {
                            double p = v.toObject().value("Close").toDouble(
                                v.toObject().value("close").toDouble(v.toObject().value("price").toDouble(0)));
                            if (p > 0)
                                prices.append(p);
                        } else if (v.isDouble()) {
                            prices.append(v.toDouble());
                        }
                    }
                }

                // Window: "all" or a number of observations (the parameter was ignored).
                bool window_ok = false;
                const int window = params.value("window").toString("all").trimmed().toInt(&window_ok);
                if (window_ok && window >= 2 && window < prices.size())
                    prices = prices.mid(prices.size() - window);

                if (prices.size() < 2) {
                    cb(false, {}, "Need at least 2 data points for drawdown analysis");
                    return;
                }

                double peak = prices[0], max_dd = 0;
                int dd_start = 0, dd_end = 0;
                int current_dd_start = 0;
                int max_dd_duration = 0, current_duration = 0;

                for (int i = 0; i < prices.size(); ++i) {
                    if (prices[i] > peak) {
                        peak = prices[i];
                        current_dd_start = i;
                        current_duration = 0;
                    }
                    double dd = (peak - prices[i]) / peak;
                    if (dd > max_dd) {
                        max_dd = dd;
                        dd_start = current_dd_start;
                        dd_end = i;
                    }
                    if (dd > 0)
                        ++current_duration;
                    else
                        current_duration = 0;
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
                            double p = v.toObject().value("Close").toDouble(v.toObject().value("close").toDouble(0));
                            if (p > 0)
                                prices.append(p);
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
                    returns.append((prices[i] - prices[i - 1]) / prices[i - 1]);

                double mean = 0, var = 0;
                for (double r : returns)
                    mean += r;
                mean /= returns.size();
                for (double r : returns)
                    var += (r - mean) * (r - mean);
                var /= returns.size();
                double std_dev = std::sqrt(var);

                int n_sims = static_cast<int>(params.value("simulations").toDouble(1000));
                int horizon = static_cast<int>(params.value("horizon_days").toDouble(252));
                // Clamp: >10000 sims is a perf risk; <1 sim leaves final_prices
                // empty and the percentile reads below (final_prices[n_sims/2],
                // .first(), .last()) would be out-of-bounds → crash. A negative
                // horizon must not underflow the inner loop bound either.
                n_sims = qBound(1, n_sims, 10000);
                if (horizon < 0)
                    horizon = 0;

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
                        if (u1 < 1e-10)
                            u1 = 1e-10;
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
                out["mean_final"] = [&]() {
                    double s = 0;
                    for (double p : final_prices)
                        s += p;
                    return s / n_sims;
                }();
                out["median_final"] = final_prices[n_sims / 2];
                out["p5"] = final_prices[static_cast<int>(0.05 * n_sims)];
                out["p25"] = final_prices[static_cast<int>(0.25 * n_sims)];
                out["p75"] = final_prices[static_cast<int>(0.75 * n_sims)];
                out["p95"] = final_prices[static_cast<int>(0.95 * n_sims)];
                out["min"] = final_prices.first();
                out["max"] = final_prices.last();
                out["prob_profit"] = [&]() {
                    int c = 0;
                    for (double p : final_prices)
                        if (p > start_price)
                            ++c;
                    return (double)c / n_sims;
                }();

                // "Confidence Levels" was declared but never read. Loss at each level, measured
                // from the starting price (positive = money lost).
                QStringList levels;
                const QJsonValue conf_param = params.value("confidence");
                if (conf_param.isDouble())
                    levels << QString::number(conf_param.toDouble());
                else
                    levels = conf_param.toString("0.95,0.99").split(',', Qt::SkipEmptyParts);
                QJsonObject value_at_risk;
                for (const QString& lv : levels) {
                    bool ok = false;
                    double c = lv.trimmed().toDouble(&ok);
                    if (!ok)
                        continue;
                    if (c > 1.0 && c < 100.0)
                        c /= 100.0;
                    if (c <= 0.0 || c >= 1.0)
                        continue;
                    const int idx = qBound(0, static_cast<int>((1.0 - c) * n_sims), n_sims - 1);
                    value_at_risk[QString::number(c, 'g', 4)] = start_price - final_prices[idx];
                }
                out["value_at_risk"] = value_at_risk;
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
            [](const QJsonObject&, const QVector<QJsonValue>&, std::function<void(bool, QJsonValue, QString)> cb) {
                // Same dead end as the Backtest node: compute_technicals.py has no factor-model mode.
                cb(false, {},
                   "Factor Model is not available in the workflow runner — there is no factor-regression "
                   "backend wired to this node yet");
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
                const QString symbol_a = params.value("symbol_a").toString("KO").trimmed();
                const QString symbol_b = params.value("symbol_b").toString("PEP").trimmed();
                const int lookback = qBound(10, static_cast<int>(params.value("lookback").toDouble(60)), 500);
                const double z_thresh = std::abs(params.value("z_threshold").toDouble(2.0));

                if (symbol_a.isEmpty() || symbol_b.isEmpty() || symbol_a.compare(symbol_b, Qt::CaseInsensitive) == 0) {
                    cb(false, {}, "Pairs Trading needs two different symbols");
                    return;
                }

                // compute_technicals.py never had a PAIRS mode (every run failed), so the spread is
                // computed here: OLS hedge ratio of A on B over the lookback window and the z-score of
                // the latest residual. Both histories come from the same yfinance command the
                // market-data nodes use.
                an_fetch_closes(symbol_a, "2y", [=](bool ok_a, QVector<double> closes_a, QString err_a) {
                    if (!ok_a) {
                        cb(false, {}, err_a);
                        return;
                    }
                    an_fetch_closes(symbol_b, "2y", [=](bool ok_b, QVector<double> closes_b, QString err_b) {
                        if (!ok_b) {
                            cb(false, {}, err_b);
                            return;
                        }
                        AnPairStats st;
                        if (!an_pair_stats(closes_a, closes_b, lookback, &st)) {
                            cb(false, {}, "Not enough overlapping price history to compute the spread");
                            return;
                        }
                        QString signal = "hold";
                        if (st.zscore >= z_thresh)
                            signal = "short_spread"; // A rich vs B: short A, long B
                        else if (st.zscore <= -z_thresh)
                            signal = "long_spread"; // A cheap vs B: long A, short B
                        else if (std::abs(st.zscore) < 0.5)
                            signal = "exit";

                        QJsonObject out;
                        out["symbol_a"] = symbol_a;
                        out["symbol_b"] = symbol_b;
                        out["signal"] = signal;
                        out["zscore"] = st.zscore;
                        out["z_threshold"] = z_thresh;
                        out["hedge_ratio"] = st.hedge_ratio;
                        out["spread"] = st.spread;
                        out["spread_mean"] = st.spread_mean;
                        out["spread_std"] = st.spread_std;
                        out["lookback"] = st.observations;
                        cb(true, out, {});
                    });
                });
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
                            double p = v.toObject().value("Close").toDouble(v.toObject().value("close").toDouble(0));
                            if (p > 0)
                                prices.append(p);
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
                    returns.append((prices[i] - prices[i - 1]) / prices[i - 1]);

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
                for (int i = prices.size() - sma_short; i < prices.size(); ++i)
                    sma_s += prices[i];
                sma_s /= sma_short;
                for (int i = prices.size() - sma_long; i < prices.size(); ++i)
                    sma_l += prices[i];
                sma_l /= sma_long;

                QString regime;
                if (ann_vol > 0.30)
                    regime = "high_volatility";
                else if (sma_s > sma_l && mean > 0)
                    regime = "bull";
                else if (sma_s < sma_l && mean < 0)
                    regime = "bear";
                else
                    regime = "sideways";

                QJsonObject out;
                out["regime"] = regime;
                // Only the rolling-volatility / trend rule is implemented (no HMM, and the
                // number of regimes is fixed). Report that instead of echoing the select.
                out["method"] = "rolling_vol";
                const QString requested_method = params.value("method").toString("hmm");
                if (requested_method != "rolling_vol")
                    out["requested_method"] = requested_method;
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
