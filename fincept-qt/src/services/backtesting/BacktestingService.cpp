// src/services/backtesting/BacktestingService.cpp
#include "services/backtesting/BacktestingService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

#include <QHash>
#include <QJsonDocument>
#include <QPointer>

static constexpr int kStrategiesTtlSec = 10 * 60;
static constexpr int kOptionsTtlSec = 10 * 60;

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

    // The UI uses short command ids (matching fincept-qt/src/services/backtesting/
    // BacktestingTypes.h::all_commands()); every Python provider's main() dispatches
    // on the canonical, longer names. Translate at the boundary so screen/types
    // code keeps the short ids while the subprocess sees what its dispatcher expects.
    static const QHash<QString, QString> kPyCommandMap{
        {"backtest", "run_backtest"},
        {"indicator", "calculate_indicator"},
        {"signals", "generate_signals"},
        {"labels", "generate_labels"},
        {"splits", "generate_splits"},
        {"returns", "analyze_returns"},
        {"indicator_sweep", "indicator_param_sweep"},
        // labels_to_signals, walk_forward, optimize, indicator_signals: UI
        // short-id matches the Python long name; pass through unchanged.
    };
    const QString py_command = kPyCommandMap.value(command, command);

    QPointer<BacktestingService> self = this;
    auto ctx = QString("%1/%2").arg(provider, command);

    python::PythonRunner::instance().run(
        script, {py_command, json_str}, [self, provider, command, ctx](python::PythonResult result) {
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
            // All Python providers wrap their payload in {success, data, error?}.
            // Surface failures via error_occurred and emit the inner `data` so
            // the screen sees the raw result (performance/trades/etc.) directly.
            auto root = doc.object();
            const bool ok = root.value("success").toBool(true);
            if (!ok) {
                auto err = root.value("error").toString();
                if (err.isEmpty())
                    err = root.value("message").toString("Backtest failed");
                LOG_ERROR("Backtesting", QString("[%1] Provider error: %2").arg(ctx, err));
                emit self->error_occurred(ctx, err);
                return;
            }
            QJsonObject payload = root.contains("data") && root.value("data").isObject()
                                      ? root.value("data").toObject()
                                      : root;
            LOG_INFO("Backtesting", QString("[%1] Result ready").arg(ctx));
            emit self->result_ready(provider, command, payload);
        });
}

void BacktestingService::load_strategies(const QString& provider) {
    const QString cache_key = "backtest:strategies:" + provider;
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (!doc.isNull()) {
            emit result_ready(provider, "get_strategies", doc.object());
            return;
        }
    }

    auto script = QString("Analytics/backtesting/%1/%1_provider.py").arg(provider);
    QPointer<BacktestingService> self = this;
    python::PythonRunner::instance().run(
        script, {"get_strategies", "{}"}, [self, provider, cache_key](python::PythonResult result) {
            if (!self)
                return;
            if (!result.success) {
                emit self->error_occurred("load_strategies/" + provider, result.error);
                return;
            }
            auto doc = QJsonDocument::fromJson(python::extract_json(result.output).toUtf8());
            if (!doc.isNull()) {
                fincept::CacheManager::instance().put(cache_key,
                                                      QVariant(QString::fromUtf8(doc.toJson(QJsonDocument::Compact))),
                                                      kStrategiesTtlSec, "backtesting");
                emit self->result_ready(provider, "get_strategies", doc.object());
            }
        });
}

void BacktestingService::load_command_options(const QString& provider) {
    const QString cache_key = "backtest:options:" + provider;
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (!doc.isNull()) {
            emit command_options_loaded(provider, doc.object());
            return;
        }
    }

    auto script = QString("Analytics/backtesting/%1/%1_provider.py").arg(provider);
    QPointer<BacktestingService> self = this;
    python::PythonRunner::instance().run(
        script, {"get_command_options", "{}"}, [self, provider, cache_key](python::PythonResult result) {
            if (!self)
                return;
            if (!result.success) {
                LOG_WARN("Backtesting", QString("get_command_options failed for %1 — using defaults").arg(provider));
                return;
            }
            auto doc = QJsonDocument::fromJson(python::extract_json(result.output).toUtf8());
            if (doc.isNull())
                return;
            auto obj = doc.object();
            auto data = obj.contains("data") ? obj.value("data").toObject() : obj;
            fincept::CacheManager::instance().put(
                cache_key, QVariant(QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact))),
                kOptionsTtlSec, "backtesting");
            emit self->command_options_loaded(provider, data);
        });
}

void BacktestingService::list_strategies() {
    // fincept_provider.py exposes the catalog under "get_strategies" and requires
    // a JSON args payload (sys.argv[2]) — pass an empty object.
    QPointer<BacktestingService> self = this;
    python::PythonRunner::instance().run(
        "Analytics/backtesting/fincept/fincept_provider.py", {"get_strategies", "{}"},
        [self](python::PythonResult result) {
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
