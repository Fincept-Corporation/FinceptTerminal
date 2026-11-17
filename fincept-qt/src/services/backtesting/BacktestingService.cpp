// src/services/backtesting/BacktestingService.cpp
#include "services/backtesting/BacktestingService.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "services/backtesting/BacktestBrokerData.h"
#include "storage/cache/CacheManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

static constexpr int kStrategiesTtlSec = 10 * 60;
static constexpr int kOptionsTtlSec = 10 * 60;
static constexpr int kCapabilitiesTtlSec = 24 * 60 * 60;

namespace fincept::services::backtest {

BacktestingService& BacktestingService::instance() {
    static BacktestingService inst;
    return inst;
}

BacktestingService::BacktestingService(QObject* parent) : QObject(parent) {
    qRegisterMetaType<fincept::services::backtest::BtProgress>();
    qRegisterMetaType<fincept::services::backtest::BtOutcome>();
}

BacktestingService::~BacktestingService() {
    // Flags of cancelled runs whose process never reported back (app closing mid-run).
    for (quint64 id : std::as_const(cancelled_))
        QFile::remove(cancel_flag_path(id));
}

QString BacktestingService::cancel_flag_path(quint64 run_id) {
    // The edition's own temp folder: the OS temp folder is shared with every
    // other program and swept by another build's uninstaller (fincept_*).
    return AppPaths::temp_file(
        QStringLiteral("bt_cancel_%1_%2.flag").arg(QCoreApplication::applicationPid()).arg(run_id));
}

quint64 BacktestingService::execute(const QString& provider, const QString& command, const QJsonObject& args) {
    const quint64 run_id = next_run_id_++;
    Run run;
    run.provider = provider;
    run.command = command;
    run.kill = std::make_shared<std::atomic<bool>>(false);
    run.started_ms = QDateTime::currentMSecsSinceEpoch();
    runs_.insert(run_id, run);
    LOG_INFO("Backtesting", logf("Backtest run started", {{"engine", provider}, {"command", command}, {"run", run_id}}));

    // Cooperative cancellation: providers poll this path between loop iterations
    // (base/progress.py CancelToken) and exit with code 3 as soon as cancel() creates it.
    QFile::remove(cancel_flag_path(run_id)); // stale flag from a recycled pid
    QJsonObject run_args = args;
    run_args["cancelFile"] = QDir::toNativeSeparators(cancel_flag_path(run_id));

    const QJsonArray symbols = args.value("symbols").toArray();

    // Broker data is wired for VectorBT only (v1). Skip for metadata calls
    // (get_indicators sends empty args → no symbols) and when no Indian broker
    // is connected — those paths are byte-for-byte the original behaviour.
    const bool wants_broker =
        provider == QLatin1String("vectorbt") && !symbols.isEmpty() && BacktestBrokerData::has_active_indian_broker();
    if (!wants_broker) {
        dispatch_python(run_id, provider, command, run_args);
        return run_id;
    }

    QStringList syms;
    syms.reserve(symbols.size());
    for (const auto& s : symbols)
        syms << s.toString();
    const QString start = args.value("startDate").toString();
    const QString end = args.value("endDate").toString();
    const QString interval = args.value("interval").toString(QStringLiteral("1d"));

    QPointer<BacktestingService> self = this;
    BacktestBrokerData::fetch(this, syms, start, end, interval,
                              [self, run_id, provider, command, run_args](QJsonObject candles, QString broker_id) {
                                  if (!self || !self->runs_.contains(run_id))
                                      return; // cancelled while the broker fetch was in flight
                                  QJsonObject enriched = run_args;
                                  if (!candles.isEmpty()) {
                                      enriched["brokerCandles"] = candles;
                                      enriched["brokerDataSource"] = broker_id;
                                      LOG_DEBUG("Backtesting", QString("Using %1 broker data for %2 symbol(s)")
                                                                   .arg(broker_id)
                                                                   .arg(candles.size()));
                                  } else {
                                      LOG_DEBUG("Backtesting", "No broker candles resolved — using yfinance");
                                  }
                                  self->dispatch_python(run_id, provider, command, enriched);
                              });
    return run_id;
}

bool BacktestingService::cancel(quint64 run_id) {
    auto it = runs_.find(run_id);
    if (it == runs_.end())
        return false;
    const Run run = it.value();
    runs_.erase(it);
    cancelled_.insert(run_id);

    QFile flag(cancel_flag_path(run_id));
    if (!flag.open(QIODevice::WriteOnly))
        LOG_WARN("Backtesting", logf("Could not create the cancel flag",
                                     {{"path", log_path(flag.fileName())}, {"error", flag.errorString()}}));
    flag.close();

    // Kill fallback: a provider that never polls the flag (one long library call)
    // would keep its Python slot until it ends. Raising the token kills the whole
    // process tree within ~250 ms (PythonRunner); a provider that honoured the flag
    // has already exited and the token is a no-op.
    std::shared_ptr<std::atomic<bool>> kill = run.kill;
    QPointer<BacktestingService> self = this;
    QTimer::singleShot(kCancelGraceMs, this, [self, run_id, kill]() {
        if (!self || !kill)
            return;
        if (self->cancelled_.contains(run_id)) // the process has not reported back yet
            kill->store(true);
    });

    emit run_cancelled(run_id, run.provider, run.command);
    return true;
}

void BacktestingService::dispatch_python(quint64 run_id, const QString& provider, const QString& command,
                                         QJsonObject args) {
    // Build script path: Analytics/backtesting/{provider}/{provider}_provider.py
    const auto script = QString("Analytics/backtesting/%1/%1_provider.py").arg(provider);

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

    // Args travel on stdin, never argv: broker-injected candles (and long symbol
    // lists) blew past the ~32 KB Windows command-line limit, and argv is readable
    // by other same-user processes. Every provider's main() accepts `--stdin`.
    python::PythonRunner::RunOptions opts;
    opts.stdin_data = QJsonDocument(args).toJson(QJsonDocument::Compact);
    const auto run_it = runs_.constFind(run_id);
    if (run_it != runs_.constEnd())
        opts.cancel_token = run_it->kill;
    // Only stderr is streamed: stdout (the one, possibly multi-MB, result line) is buffered whole and
    // never scanned / decoded line by line on the GUI thread.
    opts.stream_stdout = false;

    // Progress lines arrive on stderr ("@@BT_PROGRESS {...}"); stdout carries the one
    // result line and is read once the process ends. Unparseable lines are ignored.
    python::PythonRunner::StreamCallback on_line = [self, run_id](QString line, bool is_stderr) {
        if (!is_stderr || !self || !self->runs_.contains(run_id))
            return;
        if (const auto p = parse_progress_line(line))
            emit self->run_progress(run_id, *p);
    };

    python::PythonRunner::instance().run_with_options(
        script, {py_command, QStringLiteral("--stdin")}, opts,
        [self, run_id, provider, command](python::PythonResult result) {
            if (!self)
                return;
            const qint64 started = self->runs_.value(run_id).started_ms;
            const qint64 duration_ms = started > 0 ? QDateTime::currentMSecsSinceEpoch() - started : -1;
            if (self->cancelled_.remove(run_id)) {
                // The user cancelled this run: whatever the process printed (a late
                // result, or the provider's own "cancelled" exit) is discarded.
                QFile::remove(cancel_flag_path(run_id));
                LOG_INFO("Backtesting", logf("Backtest run finished",
                                             {{"engine", provider}, {"command", command}, {"run", run_id},
                                              {"outcome", "cancelled"}, {"exit", result.exit_code}}));
                return;
            }
            if (!self->runs_.contains(run_id))
                return;
            // Classify (parses up to ~1 MB of JSON) off the GUI thread.
            const QByteArray out = result.output.toUtf8();
            const bool ok = result.success;
            const int exit_code = result.exit_code;
            const QString err = result.error;
            QPointer<BacktestingService> guard = self;
            (void)QtConcurrent::run([guard, run_id, provider, command, ok, exit_code, err, out, duration_ms]() {
                BtOutcome outcome = classify_run_output(ok, exit_code, err, out);
                QMetaObject::invokeMethod(
                    QCoreApplication::instance(),
                    [guard, run_id, provider, command, outcome = std::move(outcome), duration_ms]() {
                        if (guard)
                            guard->finish_run(run_id, provider, command, outcome, duration_ms);
                    },
                    Qt::QueuedConnection);
            });
        },
        on_line);
}

void BacktestingService::finish_run(quint64 run_id, const QString& provider, const QString& command,
                                    const BtOutcome& outcome, qint64 duration_ms) {
    if (cancelled_.remove(run_id)) {
        // Cancelled while the output was being classified.
        QFile::remove(cancel_flag_path(run_id));
        LOG_INFO("Backtesting", logf("Backtest run finished", {{"engine", provider}, {"command", command},
                                                               {"run", run_id}, {"outcome", "cancelled"}}));
        return;
    }
    if (!runs_.remove(run_id))
        return;
    QFile::remove(cancel_flag_path(run_id));
    const auto ctx = QString("%1/%2").arg(provider, command);
    const char* kind = outcome.ok() ? "ok" : (outcome.cancelled() ? "cancelled" : "failed");
    QList<LogField> fields{{"engine", provider}, {"command", command}, {"run", run_id}, {"outcome", kind},
                           {"duration_ms", duration_ms}};
    if (!outcome.ok() && !outcome.cancelled()) {
        fields.append(LogField("error_code", outcome.error_code));
        fields.append(LogField("error", outcome.message.left(200)));
    }
    if (outcome.ok() || outcome.cancelled())
        LOG_INFO("Backtesting", logf("Backtest run finished", fields));
    else
        LOG_WARN("Backtesting", logf("Backtest run finished", fields));

    emit run_finished(run_id, outcome);
    if (outcome.cancelled()) {
        // The provider stopped itself (exit 3) without a cancel from this side.
        emit run_cancelled(run_id, provider, command);
        return;
    }
    if (!outcome.ok()) {
        if (!outcome.envelope.isEmpty())
            emit run_failed_details(run_id, outcome.envelope);
        emit run_failed(run_id, ctx, outcome.message);
        emit error_occurred(ctx, outcome.message);
        return;
    }
    emit run_completed(run_id, provider, command, outcome.data);
    emit result_ready(provider, command, outcome.data);
}

void BacktestingService::load_strategies(const QString& provider) {
    // v3: the request lists every strategy (includeUnsupported) with its reason.
    const QString cache_key = "backtest:strategies:v3:" + provider;
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
        script, {"get_strategies", QStringLiteral("{\"includeUnsupported\": true}")},
        [self, provider, cache_key](python::PythonResult result) {
            if (!self)
                return;
            if (!result.success) {
                LOG_WARN("Backtesting", logf("Strategy catalogue could not be loaded",
                                             {{"engine", provider}, {"error", result.error.left(200)}}));
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
    // Cache key versioned (v2) — Python option names changed (e.g. mode names,
    // splitter names, returns analysis types) and older cached entries will
    // serve UI strings that no longer round-trip through the provider.
    const QString cache_key = "backtest:options:v2:" + provider;
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
                LOG_WARN("Backtesting", logf("Command options could not be loaded, using defaults",
                                             {{"engine", provider}}));
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

void BacktestingService::load_capabilities(const QString& provider) {
    const QString cache_key = "backtest:capabilities:v1:" + provider;
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        const auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (doc.isObject()) {
            emit capabilities_loaded(provider, doc.object());
            return;
        }
    }
    const auto script = QString("Analytics/backtesting/%1/%1_provider.py").arg(provider);
    QPointer<BacktestingService> self = this;
    python::PythonRunner::instance().run(
        script, {"get_capabilities", "{}"}, [self, provider, cache_key](python::PythonResult result) {
            if (!self || !result.success)
                return; // engines without get_capabilities keep provider_support()
            const auto doc = QJsonDocument::fromJson(python::extract_json(result.output).toUtf8());
            if (!doc.isObject())
                return;
            const QJsonObject root = doc.object();
            if (!root.value(QStringLiteral("success")).toBool(false))
                return;
            const QJsonObject data = root.value(QStringLiteral("data")).toObject();
            if (data.isEmpty())
                return;
            fincept::CacheManager::instance().put(
                cache_key, QVariant(QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact))),
                kCapabilitiesTtlSec, "backtesting");
            emit self->capabilities_loaded(provider, data);
        });
}

void BacktestingService::set_pending_portfolio_config(const QJsonObject& config) {
    pending_portfolio_config_ = config;
    emit pending_portfolio_config_set();
}

QJsonObject BacktestingService::take_pending_portfolio_config() {
    QJsonObject result;
    std::swap(result, pending_portfolio_config_);
    return result;
}

} // namespace fincept::services::backtest
