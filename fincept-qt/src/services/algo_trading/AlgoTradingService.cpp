// src/services/algo_trading/AlgoTradingService.cpp
#include "services/algo_trading/AlgoTradingService.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"
#include "storage/sqlite/Database.h"
#include "trading/UnifiedTrading.h"

#include <QFile>
#include <QJsonDocument>
#include <QPointer>
#include <QUuid>

namespace fincept::services::algo {

static constexpr int kStrategiesTtlSec = 30;
static constexpr int kDeploymentsTtlSec = 30;
static constexpr const char* kStrategiesCacheKey = "algo:strategies:registry";
static constexpr const char* kDeploymentsCacheKey = "algo:deployments";

// ── DB path helper ────────────────────────────────────────────────────────────
static QString algo_db_path() {
    return fincept::AppPaths::data() + "/fincept.db";
}

AlgoTradingService& AlgoTradingService::instance() {
    static AlgoTradingService inst;
    return inst;
}

AlgoTradingService::AlgoTradingService(QObject* parent) : QObject(parent) {}

void AlgoTradingService::run_python(const QString& script, const QStringList& args, const QString& context,
                                    std::function<void(bool, const QString&)> cb) {
    QPointer<AlgoTradingService> self = this;
    python::PythonRunner::instance().run(script, args, [self, context, cb](python::PythonResult result) {
        if (!self)
            return;
        cb(result.success, result.success ? result.output : result.error);
    });
}

// ── Strategy CRUD ─────────────────────────────────────────────────────────────
void AlgoTradingService::save_strategy(const AlgoStrategy& strategy) {
    QJsonObject obj;

    // Upsert by name: if a strategy with the same name exists, reuse its ID
    QString resolved_id = strategy.id;
    if (resolved_id.isEmpty() && !strategy.name.isEmpty()) {
        auto q = fincept::Database::instance().execute(
            "SELECT id FROM algo_strategies WHERE name = ? AND is_active = 1 LIMIT 1",
            {strategy.name});
        if (q.is_ok() && q.value().next())
            resolved_id = q.value().value(0).toString();
    }
    if (resolved_id.isEmpty())
        resolved_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    obj["id"] = resolved_id;
    obj["name"] = strategy.name;
    obj["description"] = strategy.description;
    obj["timeframe"] = strategy.timeframe;
    obj["entry_conditions"] = strategy.entry_conditions;
    obj["exit_conditions"] = strategy.exit_conditions;
    obj["entry_logic"] = strategy.entry_logic;
    obj["exit_logic"] = strategy.exit_logic;
    obj["stop_loss"] = strategy.stop_loss;
    obj["take_profit"] = strategy.take_profit;
    obj["trailing_stop"] = strategy.trailing_stop;

    auto json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    run_python("algo_trading/backtest_engine.py", {"save_strategy", json, "--db", algo_db_path()}, "save_strategy",
               [this, obj](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("save_strategy", out);
                       return;
                   }
                   fincept::CacheManager::instance().remove(kStrategiesCacheKey);
                   emit strategy_saved(obj["id"].toString());
               });
}

static QVector<AlgoStrategy> parse_strategies(const QJsonArray& arr) {
    QVector<AlgoStrategy> strategies;
    strategies.reserve(arr.size());
    for (const auto& v : arr) {
        auto o = v.toObject();
        AlgoStrategy s;
        s.id = o["id"].toString();
        s.name = o["name"].toString();
        s.description = o["description"].toString();
        s.timeframe = o["timeframe"].toString();
        s.entry_conditions = o["entry_conditions"].toArray();
        s.exit_conditions = o["exit_conditions"].toArray();
        s.entry_logic = o["entry_logic"].toString("AND");
        s.exit_logic = o["exit_logic"].toString("AND");
        s.stop_loss = o["stop_loss"].toDouble();
        s.take_profit = o["take_profit"].toDouble();
        s.trailing_stop = o["trailing_stop"].toDouble();
        s.created_at = o["created_at"].toString();
        s.updated_at = o["updated_at"].toString();
        s.script_path = o["script_path"].toString(); // empty for DSL, file path for QC
        strategies.append(s);
    }
    return strategies;
}

static QVector<AlgoStrategy> load_dsl_strategies_from_db() {
    QVector<AlgoStrategy> result;
    auto q = fincept::Database::instance().execute(
        "SELECT id, name, description, timeframe, entry_conditions, exit_conditions, "
        "entry_logic, exit_logic, stop_loss, take_profit, trailing_stop, "
        "created_at, updated_at "
        "FROM algo_strategies WHERE is_active = 1 ORDER BY created_at DESC", {});
    if (q.is_err())
        return result;
    auto& query = q.value();
    while (query.next()) {
        AlgoStrategy s;
        s.id               = query.value(0).toString();
        s.name             = query.value(1).toString();
        s.description      = query.value(2).toString();
        s.timeframe        = query.value(3).toString();
        s.entry_conditions = QJsonDocument::fromJson(query.value(4).toString().toUtf8()).array();
        s.exit_conditions  = QJsonDocument::fromJson(query.value(5).toString().toUtf8()).array();
        s.entry_logic      = query.value(6).isNull() ? QStringLiteral("AND") : query.value(6).toString();
        s.exit_logic       = query.value(7).isNull() ? QStringLiteral("AND") : query.value(7).toString();
        s.stop_loss        = query.value(8).toDouble();
        s.take_profit      = query.value(9).toDouble();
        s.trailing_stop    = query.value(10).toDouble();
        s.created_at       = query.value(11).toString();
        s.updated_at       = query.value(12).toString();
        result.append(s);
    }
    return result;
}

void AlgoTradingService::list_strategies() {
    QVector<AlgoStrategy> all;

    // 1. Load QC strategies from pre-generated registry_index.json
    const QString json_path =
        python::PythonRunner::instance().scripts_dir() + "/strategies/registry_index.json";
    QFile f(json_path);
    if (f.open(QIODevice::ReadOnly)) {
        auto doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (!doc.isNull())
            all = parse_strategies(doc.object()["strategies"].toArray());
    }

    // 2. Load user-saved DSL strategies from the database
    auto dsl = load_dsl_strategies_from_db();
    if (!dsl.isEmpty()) {
        all.append(dsl);
        LOG_INFO("AlgoTrading", QString("Loaded %1 DSL strategies from DB").arg(dsl.size()));
    }

    LOG_INFO("AlgoTrading", QString("Total strategies: %1 (QC + DSL)").arg(all.size()));
    emit strategies_loaded(all);
}

void AlgoTradingService::delete_strategy(const QString& id) {
    run_python("algo_trading/backtest_engine.py", {"delete_strategy", id, "--db", algo_db_path()}, "delete_strategy",
               [this, id](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("delete_strategy", out);
                       return;
                   }
                   fincept::CacheManager::instance().remove(kStrategiesCacheKey);
                   emit strategy_deleted(id);
               });
}

// ── Deployment lifecycle — now handled by AlgoEngine (src/algo_engine/) ──────
// deploy_strategy, stop_deployment, stop_all_deployments, remove_deployment,
// list_deployments, check_bridge_needed have been removed.
// Use fincept::algo::AlgoEngine::instance() for all deployment operations.

// ── Backtesting ───────────────────────────────────────────────────────────────
void AlgoTradingService::run_backtest(const QString& strategy_id, const QString& symbol, const QString& start_date,
                                      const QString& end_date, double capital) {
    QJsonObject params;
    params["strategy_id"] = strategy_id;
    params["symbol"] = symbol;
    params["start_date"] = start_date;
    params["end_date"] = end_date;
    params["initial_capital"] = capital;
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);

    run_python("algo_trading/backtest_engine.py", {"run_backtest", json, "--db", algo_db_path()}, "backtest",
               [this](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("backtest", out);
                       return;
                   }
                   auto doc = QJsonDocument::fromJson(python::extract_json(out).toUtf8());
                   emit backtest_result(doc.object());
               });
}

// Scanner is now in AlgoScanner (src/algo_engine/AlgoScanner.h/.cpp).

} // namespace fincept::services::algo
