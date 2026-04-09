// src/services/algo_trading/AlgoTradingService.cpp
#include "services/algo_trading/AlgoTradingService.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

#include <QJsonDocument>
#include <QPointer>
#include <QUuid>

namespace fincept::services::algo {

static constexpr int kStrategiesTtlSec = 30;
static constexpr int kDeploymentsTtlSec = 30;
static constexpr const char* kStrategiesCacheKey = "algo:strategies";
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
    obj["id"] = strategy.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : strategy.id;
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
        strategies.append(s);
    }
    return strategies;
}

void AlgoTradingService::list_strategies() {
    const QVariant cached = fincept::CacheManager::instance().get(kStrategiesCacheKey);
    if (!cached.isNull()) {
        auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (!doc.isNull()) {
            emit strategies_loaded(parse_strategies(doc.object()["strategies"].toArray()));
            return;
        }
    }

    run_python("algo_trading/backtest_engine.py", {"list_strategies", "--db", algo_db_path()}, "list_strategies",
               [this](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("list_strategies", out);
                       return;
                   }
                   auto doc = QJsonDocument::fromJson(python::extract_json(out).toUtf8());
                   auto obj = doc.object();
                   fincept::CacheManager::instance().put(
                       kStrategiesCacheKey,
                       QVariant(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))),
                       kStrategiesTtlSec, "algo_trading");
                   emit strategies_loaded(parse_strategies(obj["strategies"].toArray()));
               });
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

// ── Deployment lifecycle ──────────────────────────────────────────────────────
void AlgoTradingService::deploy_strategy(const QString& strategy_id, const QString& symbol, const QString& mode,
                                         const QString& timeframe, double quantity) {
    auto deploy_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    // algo_live_runner is a long-running process — start and don't wait for completion
    run_python("algo_trading/algo_live_runner.py",
               {"--deploy-id", deploy_id, "--strategy-id", strategy_id, "--symbol", symbol, "--mode", mode,
                "--timeframe", timeframe, "--quantity", QString::number(quantity), "--db", algo_db_path()},
               "deploy", [this, deploy_id](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("deploy", out);
                       return;
                   }
                   emit deployment_started(deploy_id);
               });
}

void AlgoTradingService::stop_deployment(const QString& deployment_id) {
    run_python("algo_trading/algo_manager.py", {"stop", deployment_id, "--db", algo_db_path()}, "stop_deployment",
               [this, deployment_id](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("stop_deployment", out);
                       return;
                   }
                   emit deployment_stopped(deployment_id);
               });
}

void AlgoTradingService::stop_all_deployments() {
    run_python("algo_trading/algo_manager.py", {"stop_all", "--db", algo_db_path()}, "stop_all",
               [this](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("stop_all", out);
                       return;
                   }
                   LOG_INFO("AlgoTrading", "All deployments stopped");
               });
}

static QVector<AlgoDeployment> parse_deployments(const QJsonArray& arr) {
    QVector<AlgoDeployment> deployments;
    deployments.reserve(arr.size());
    for (const auto& v : arr) {
        auto o = v.toObject();
        AlgoDeployment d;
        d.id = o["id"].toString();
        d.strategy_id = o["strategy_id"].toString();
        d.strategy_name = o["strategy_name"].toString();
        d.symbol = o["symbol"].toString();
        d.mode = o["mode"].toString();
        d.status = o["status"].toString();
        d.timeframe = o["timeframe"].toString();
        d.quantity = o["quantity"].toDouble();
        d.error_message = o["error_message"].toString();
        d.total_pnl = o["total_pnl"].toDouble();
        d.unrealized_pnl = o["unrealized_pnl"].toDouble();
        d.total_trades = o["total_trades"].toInt();
        d.win_rate = o["win_rate"].toDouble();
        d.max_drawdown = o["max_drawdown"].toDouble();
        d.position_qty = o["current_position_qty"].toDouble();
        d.position_side = o["current_position_side"].toString();
        d.position_entry = o["current_position_entry"].toDouble();
        d.created_at = o["created_at"].toString();
        d.updated_at = o["updated_at"].toString();
        deployments.append(d);
    }
    return deployments;
}

void AlgoTradingService::list_deployments() {
    const QVariant cached = fincept::CacheManager::instance().get(kDeploymentsCacheKey);
    if (!cached.isNull()) {
        auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (!doc.isNull()) {
            emit deployments_loaded(parse_deployments(doc.object()["deployments"].toArray()));
            return;
        }
    }

    run_python("algo_trading/algo_manager.py", {"list_deployments", "--db", algo_db_path()}, "list_deployments",
               [this](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("list_deployments", out);
                       return;
                   }
                   auto doc = QJsonDocument::fromJson(python::extract_json(out).toUtf8());
                   auto obj = doc.object();
                   fincept::CacheManager::instance().put(
                       kDeploymentsCacheKey,
                       QVariant(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))),
                       kDeploymentsTtlSec, "algo_trading");
                   emit deployments_loaded(parse_deployments(obj["deployments"].toArray()));
               });
}

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

// ── Scanner ───────────────────────────────────────────────────────────────────
void AlgoTradingService::run_scan(const QJsonArray& conditions, const QStringList& symbols, const QString& timeframe,
                                  int lookback_days, const QString& logic) {
    QJsonObject params;
    params["conditions"] = conditions;
    params["symbols"] = QJsonArray::fromStringList(symbols);
    params["timeframe"] = timeframe;
    params["lookback_days"] = lookback_days;
    params["logic"] = logic;
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);

    run_python("algo_trading/scanner_engine.py", {"scan", json, "--db", algo_db_path()}, "scan",
               [this](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("scan", out);
                       return;
                   }
                   auto doc = QJsonDocument::fromJson(python::extract_json(out).toUtf8());
                   emit scan_result(doc.object());
               });
}

} // namespace fincept::services::algo
