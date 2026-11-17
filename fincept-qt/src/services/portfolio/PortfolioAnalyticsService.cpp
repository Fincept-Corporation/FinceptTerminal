#include "services/portfolio/PortfolioAnalyticsService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>

namespace fincept::services {

PortfolioAnalyticsService& PortfolioAnalyticsService::instance() {
    static PortfolioAnalyticsService s;
    return s;
}

AnalyticsRequest PortfolioAnalyticsService::make_request() {
    static std::atomic<quint64> next_id{0};
    AnalyticsRequest req;
    req.id = ++next_id;
    req.token = std::make_shared<std::atomic<bool>>(false);
    return req;
}

static QJsonArray portfolio_analytics_to_json_array(const QList<double>& xs) {
    QJsonArray a;
    for (double x : xs)
        a.append(std::isfinite(x) ? QJsonValue(x) : QJsonValue(QJsonValue::Null));
    return a;
}

/// `context` keys the call did not set explicitly.
static void portfolio_analytics_merge_context(QJsonObject& args, const QJsonObject& context) {
    for (auto it = context.begin(); it != context.end(); ++it)
        if (!args.contains(it.key()))
            args.insert(it.key(), it.value());
}

AnalyticsRequest PortfolioAnalyticsService::run_quantstats(const QStringList& symbols, const QList<double>& weights,
                                                           double risk_free_rate, const QString& benchmark,
                                                           AnalyticsCallback cb, const QString& period,
                                                           const QJsonObject& context) {
    QJsonObject args;
    args["symbols"] = QJsonArray::fromStringList(symbols);
    args["weights"] = portfolio_analytics_to_json_array(weights);
    if (std::isfinite(risk_free_rate))
        args["risk_free_rate"] = risk_free_rate;
    if (!benchmark.isEmpty())
        args["benchmark"] = benchmark;
    // The script accepts a window; it used to be dropped here, so every run was 1y.
    if (!period.isEmpty())
        args["period"] = period;
    portfolio_analytics_merge_context(args, context);
    // quantstats_analysis.py takes its input on stdin ONLY. Handing it the JSON as
    // argv (as this used to) left it blocked in sys.stdin.read() on a pipe nobody
    // ever closed until the 5-minute watchdog killed it.
    return run_script(QStringLiteral("quantstats_analysis"),
                      QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact)), std::move(cb),
                      /*via_stdin=*/true);
}

AnalyticsRequest PortfolioAnalyticsService::run_monte_carlo(const QStringList& symbols, const QList<double>& weights,
                                                            int num_simulations, AnalyticsCallback cb,
                                                            const QJsonObject& context) {
    QJsonObject args;
    args["symbols"] = QJsonArray::fromStringList(symbols);
    args["weights"] = portfolio_analytics_to_json_array(weights);
    args["num_simulations"] = num_simulations;
    portfolio_analytics_merge_context(args, context);
    return run_script(QStringLiteral("quantstats_monte_carlo"),
                      QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact)), std::move(cb),
                      /*via_stdin=*/true); // stdin-only script, see run_quantstats()
}

AnalyticsRequest PortfolioAnalyticsService::optimize_weights(const QString& args_json, AnalyticsCallback cb,
                                                             const QJsonObject& context) {
    if (context.isEmpty())
        return run_script(QStringLiteral("optimize_portfolio_weights"), args_json, std::move(cb));
    QJsonObject args = QJsonDocument::fromJson(args_json.toUtf8()).object();
    portfolio_analytics_merge_context(args, context);
    return run_script(QStringLiteral("optimize_portfolio_weights"),
                      QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact)), std::move(cb));
}

AnalyticsRequest PortfolioAnalyticsService::run_ffn(const QStringList& symbols, const QJsonObject& weights_by_symbol,
                                                    double risk_free_rate, AnalyticsCallback cb,
                                                    const QJsonObject& context) {
    QJsonObject args;
    args["symbols"] = QJsonArray::fromStringList(symbols);
    args["weights"] = weights_by_symbol;
    if (std::isfinite(risk_free_rate))
        args["risk_free_rate"] = risk_free_rate;
    portfolio_analytics_merge_context(args, context);
    return run_script(QStringLiteral("ffn_analysis"),
                      QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact)), std::move(cb),
                      /*via_stdin=*/true); // stdin-only script, see run_quantstats()
}

AnalyticsRequest PortfolioAnalyticsService::run_script(const QString& script, const QString& args_json,
                                                       AnalyticsCallback cb, bool via_stdin) {
    // PythonRunner::run() resolves scripts_dir/<name> verbatim — the name must
    // include the ".py" extension (as e.g. the yfinance_data.py daemon path
    // does). These analytics callers pass the bare stem, so normalise here;
    // otherwise the existence check fails with "Script not found".
    const QString script_file = script.endsWith(QLatin1String(".py")) ? script : script + ".py";
    const AnalyticsRequest req = make_request();
    python::PythonRunner::RunOptions opts;
    opts.cancel_token = req.token;
    if (via_stdin)
        opts.stdin_data = args_json.toUtf8();
    python::PythonRunner::instance().run_with_options(
        script_file, via_stdin ? QStringList{} : QStringList{args_json}, opts,
        [cb = std::move(cb), script = script_file, id = req.id, token = req.token](const python::PythonResult& result) {
            AnalyticsResult out;
            out.request_id = id;
            if (token && token->load()) {
                // The caller cancelled (left the tab, superseded the run): nothing to show.
                out.cancelled = true;
                out.error = QStringLiteral("Cancelled");
                LOG_DEBUG("PortfolioAnalyticsService",
                          logf("Portfolio analytics run cancelled", {{"script", script}, {"request", id}}));
                if (cb)
                    cb(out);
                return;
            }
            // A script that failed cleanly prints {"error": ...} (and may exit 1):
            // surface that message rather than a generic process error.
            const auto doc = QJsonDocument::fromJson(python::extract_json(result.output).toUtf8());
            if (doc.isObject() && doc.object().contains("error")) {
                out.error = doc.object().value("error").toString();
                if (cb)
                    cb(out);
                return;
            }
            if (!result.success || result.output.trimmed().isEmpty()) {
                out.error =
                    result.error.isEmpty() ? QStringLiteral("%1 returned empty output").arg(script) : result.error;
                LOG_ERROR("PortfolioAnalyticsService",
                          logf("Portfolio analytics script failed",
                               {{"script", script}, {"request", id}, {"exit_code", result.exit_code},
                                {"error", out.error.left(300)}}));
                if (cb)
                    cb(out);
                return;
            }
            if (!doc.isObject()) {
                out.error = QStringLiteral("%1 returned malformed JSON").arg(script);
                LOG_ERROR("PortfolioAnalyticsService",
                          logf("Portfolio analytics script returned unreadable output",
                               {{"script", script}, {"request", id}, {"bytes", result.output.size()},
                                {"head", result.output.left(200)}}));
                if (cb)
                    cb(out);
                return;
            }
            out.success = true;
            out.data = doc.object();
            if (cb)
                cb(out);
        });
    return req;
}

} // namespace fincept::services
