#include "services/portfolio/PortfolioAnalyticsService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace fincept::services {

PortfolioAnalyticsService& PortfolioAnalyticsService::instance() {
    static PortfolioAnalyticsService s;
    return s;
}

static QJsonArray to_json_array(const QList<double>& xs) {
    QJsonArray a;
    for (double x : xs)
        a.append(x);
    return a;
}

void PortfolioAnalyticsService::run_quantstats(const QStringList& symbols,
                                               const QList<double>& weights,
                                               AnalyticsCallback cb) {
    QJsonObject args;
    args["symbols"] = QJsonArray::fromStringList(symbols);
    args["weights"] = to_json_array(weights);
    run_script(QStringLiteral("quantstats_analysis"),
               QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact)),
               std::move(cb));
}

void PortfolioAnalyticsService::run_monte_carlo(const QStringList& symbols,
                                                const QList<double>& weights,
                                                int num_simulations,
                                                AnalyticsCallback cb) {
    QJsonObject args;
    args["symbols"] = QJsonArray::fromStringList(symbols);
    args["weights"] = to_json_array(weights);
    args["num_simulations"] = num_simulations;
    run_script(QStringLiteral("quantstats_monte_carlo"),
               QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact)),
               std::move(cb));
}

void PortfolioAnalyticsService::optimize_weights(const QString& args_json,
                                                 AnalyticsCallback cb) {
    run_script(QStringLiteral("optimize_portfolio_weights"), args_json, std::move(cb));
}

void PortfolioAnalyticsService::run_ffn(const QStringList& symbols,
                                        const QJsonObject& weights_by_symbol,
                                        AnalyticsCallback cb) {
    QJsonObject args;
    args["symbols"] = QJsonArray::fromStringList(symbols);
    args["weights"] = weights_by_symbol;
    run_script(QStringLiteral("ffn_analysis"),
               QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact)),
               std::move(cb));
}

void PortfolioAnalyticsService::run_script(const QString& script,
                                           const QString& args_json,
                                           AnalyticsCallback cb) {
    fincept::python::PythonRunner::instance().run(
        script, {args_json},
        [cb = std::move(cb), script](const fincept::python::PythonResult& result) {
            AnalyticsResult out;
            if (!result.success || result.output.trimmed().isEmpty()) {
                out.error = result.error.isEmpty()
                                ? QStringLiteral("%1 returned empty output").arg(script)
                                : result.error;
                LOG_ERROR("PortfolioAnalyticsService",
                          QStringLiteral("%1 failed: %2").arg(script, out.error.left(300)));
                if (cb) cb(out);
                return;
            }
            const auto doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8());
            if (!doc.isObject()) {
                out.error = QStringLiteral("%1 returned malformed JSON").arg(script);
                LOG_ERROR("PortfolioAnalyticsService",
                          QStringLiteral("%1: bad JSON: %2").arg(script, result.output.left(200)));
                if (cb) cb(out);
                return;
            }
            const QJsonObject root = doc.object();
            if (root.contains("error")) {
                out.error = root.value("error").toString();
                if (cb) cb(out);
                return;
            }
            out.success = true;
            out.data = root;
            if (cb) cb(out);
        });
}

} // namespace fincept::services
