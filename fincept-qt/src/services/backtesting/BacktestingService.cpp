// src/services/backtesting/BacktestingService.cpp
#include "services/backtesting/BacktestingService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QJsonDocument>
#include <QPointer>

namespace fincept::services::backtest {

BacktestingService& BacktestingService::instance() {
    static BacktestingService inst;
    return inst;
}

BacktestingService::BacktestingService(QObject* parent) : QObject(parent) {}

void BacktestingService::execute(const QString& provider, const QString& command, const QJsonObject& args) {
    // Build script path: Analytics/backtesting/{provider}/{provider}_provider.py
    auto script = QString("Analytics/backtesting/%1/%1_provider.py").arg(provider);
    auto json_str = QJsonDocument(args).toJson(QJsonDocument::Compact);

    QPointer<BacktestingService> self = this;
    auto ctx = QString("%1/%2").arg(provider, command);

    python::PythonRunner::instance().run(
        script, {command, json_str}, [self, provider, command, ctx](python::PythonResult result) {
            if (!self)
                return;
            if (!result.success) {
                LOG_ERROR("Backtesting", QString("[%1] Failed: %2").arg(ctx, result.error));
                emit self->error_occurred(ctx, result.error);
                return;
            }
            auto json_out = python::extract_json(result.output);
            auto doc = QJsonDocument::fromJson(json_out.toUtf8());
            if (doc.isNull()) {
                LOG_ERROR("Backtesting", QString("[%1] Invalid JSON").arg(ctx));
                emit self->error_occurred(ctx, "Invalid JSON response");
                return;
            }
            LOG_INFO("Backtesting", QString("[%1] Result ready").arg(ctx));
            emit self->result_ready(provider, command, doc.object());
        });
}

void BacktestingService::list_strategies() {
    QPointer<BacktestingService> self = this;
    python::PythonRunner::instance().run(
        "Analytics/backtesting/fincept/fincept_provider.py", {"list_strategies"}, [self](python::PythonResult result) {
            if (!self)
                return;
            if (!result.success) {
                emit self->error_occurred("list_strategies", result.error);
                return;
            }
            auto doc = QJsonDocument::fromJson(python::extract_json(result.output).toUtf8());
            if (!doc.isNull())
                emit self->strategies_loaded(doc.object());
        });
}

} // namespace fincept::services::backtest
