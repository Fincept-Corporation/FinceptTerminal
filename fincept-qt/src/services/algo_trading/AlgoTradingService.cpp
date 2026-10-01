// src/services/algo_trading/AlgoTradingService.cpp
#include "services/algo_trading/AlgoTradingService.h"

#include "algo_engine/BacktestEngine.h"
#include "algo_engine/CandleDataFetcher.h"
#include "core/logging/Logger.h"
#include "services/algo_trading/AlgoStrategyLibrary.h"
#include "storage/sqlite/Database.h"
#include "trading/AccountManager.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTime>
#include <QTimeZone>
#include <QUuid>

#include <cmath>

namespace fincept::services::algo {

// Bars of history kept before the backtest From date. Must match BacktestEngine's
// kWarmupBars (indicator warm-up it skips before evaluating) so evaluation starts on From.
static constexpr int kAlgoSvcBtWarmupBars = 50;

AlgoTradingService& AlgoTradingService::instance() {
    static AlgoTradingService inst;
    return inst;
}

AlgoTradingService::AlgoTradingService(QObject* parent) : QObject(parent) {}

// ── Strategy CRUD ─────────────────────────────────────────────────────────────
// Native SQLite UPSERT into algo_strategies (schema owned by migration v023).
void AlgoTradingService::save_strategy(const AlgoStrategy& strategy) {
    // Upsert by name: if an active strategy with the same name exists, reuse its ID
    QString resolved_id = strategy.id;
    if (resolved_id.isEmpty() && !strategy.name.isEmpty()) {
        auto q = fincept::Database::instance().execute(
            "SELECT id FROM algo_strategies WHERE name = ? AND is_active = 1 LIMIT 1", {strategy.name});
        if (q.is_ok() && q.value().next())
            resolved_id = q.value().value(0).toString();
    }
    if (resolved_id.isEmpty())
        resolved_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    const QString entry_json =
        QString::fromUtf8(QJsonDocument(strategy.entry_conditions).toJson(QJsonDocument::Compact));
    const QString exit_json = QString::fromUtf8(QJsonDocument(strategy.exit_conditions).toJson(QJsonDocument::Compact));
    const QString legs_json = QString::fromUtf8(QJsonDocument(strategy.legs).toJson(QJsonDocument::Compact));

    // is_active=1 in the UPDATE branch so re-saving a soft-deleted strategy revives it.
    auto r = fincept::Database::instance().execute(
        "INSERT INTO algo_strategies "
        "(id, name, description, timeframe, entry_conditions, exit_conditions, "
        " entry_logic, exit_logic, stop_loss, take_profit, trailing_stop, "
        " instrument_type, legs_json, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP) "
        "ON CONFLICT(id) DO UPDATE SET "
        "  name=excluded.name, description=excluded.description, timeframe=excluded.timeframe, "
        "  entry_conditions=excluded.entry_conditions, exit_conditions=excluded.exit_conditions, "
        "  entry_logic=excluded.entry_logic, exit_logic=excluded.exit_logic, "
        "  stop_loss=excluded.stop_loss, take_profit=excluded.take_profit, "
        "  trailing_stop=excluded.trailing_stop, instrument_type=excluded.instrument_type, "
        "  legs_json=excluded.legs_json, is_active=1, updated_at=CURRENT_TIMESTAMP",
        {resolved_id, strategy.name, strategy.description, strategy.timeframe, entry_json, exit_json,
         strategy.entry_logic, strategy.exit_logic, strategy.stop_loss, strategy.take_profit, strategy.trailing_stop,
         strategy.instrument_type, legs_json});

    if (r.is_err()) {
        LOG_ERROR("AlgoTrading", QString("save_strategy failed: %1").arg(QString::fromStdString(r.error())));
        emit error_occurred("save_strategy", QString::fromStdString(r.error()));
        return;
    }
    LOG_INFO("AlgoTrading", QString("Saved strategy %1 (%2)").arg(resolved_id, strategy.name));
    emit strategy_saved(resolved_id);
}

static QVector<AlgoStrategy> load_dsl_strategies_from_db() {
    QVector<AlgoStrategy> result;
    auto q = fincept::Database::instance().execute(
        "SELECT id, name, description, timeframe, entry_conditions, exit_conditions, "
        "entry_logic, exit_logic, stop_loss, take_profit, trailing_stop, "
        "created_at, updated_at, instrument_type, legs_json "
        "FROM algo_strategies WHERE is_active = 1 ORDER BY created_at DESC",
        {});
    if (q.is_err())
        return result;
    auto& query = q.value();
    while (query.next()) {
        AlgoStrategy s;
        s.id = query.value(0).toString();
        s.name = query.value(1).toString();
        s.description = query.value(2).toString();
        s.timeframe = query.value(3).toString();
        s.entry_conditions = QJsonDocument::fromJson(query.value(4).toString().toUtf8()).array();
        s.exit_conditions = QJsonDocument::fromJson(query.value(5).toString().toUtf8()).array();
        s.entry_logic = query.value(6).isNull() ? QStringLiteral("AND") : query.value(6).toString();
        s.exit_logic = query.value(7).isNull() ? QStringLiteral("AND") : query.value(7).toString();
        s.stop_loss = query.value(8).toDouble();
        s.take_profit = query.value(9).toDouble();
        s.trailing_stop = query.value(10).toDouble();
        s.created_at = query.value(11).toString();
        s.updated_at = query.value(12).toString();
        s.instrument_type = query.value(13).isNull() ? QStringLiteral("equity") : query.value(13).toString();
        s.legs = QJsonDocument::fromJson(query.value(14).toString().toUtf8()).array();
        result.append(s);
    }
    return result;
}

// Bump when the curated library's *definitions* change (e.g. a fixed strategy).
// On a bump, existing LIB-* rows are refreshed to the latest definition (without
// reviving rows the user deleted); otherwise only missing ids are inserted, so
// user-created strategies and edits are preserved.
static constexpr int kLibraryVersion = 3;

void AlgoTradingService::seed_library() {
    int stored = 0;
    {
        auto q = fincept::Database::instance().execute(
            "SELECT value FROM key_value_storage WHERE key='algo_library_version'", {});
        if (q.is_ok() && q.value().next())
            stored = q.value().value(0).toInt();
    }
    const bool force = stored < kLibraryVersion;

    for (const auto& s : algo_library_strategies()) {
        const QString entry_json = QString::fromUtf8(QJsonDocument(s.entry_conditions).toJson(QJsonDocument::Compact));
        const QString exit_json = QString::fromUtf8(QJsonDocument(s.exit_conditions).toJson(QJsonDocument::Compact));
        const QVariantList args = {s.id,          s.name,       s.description, s.timeframe,   entry_json,     exit_json,
                                   s.entry_logic, s.exit_logic, s.stop_loss,   s.take_profit, s.trailing_stop};
        if (force) {
            // Refresh definition in place; do NOT touch is_active (keeps deletions).
            fincept::Database::instance().execute(
                "INSERT INTO algo_strategies "
                "(id, name, description, timeframe, entry_conditions, exit_conditions, "
                " entry_logic, exit_logic, stop_loss, take_profit, trailing_stop, is_active, "
                " created_at, updated_at) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP) "
                "ON CONFLICT(id) DO UPDATE SET name=excluded.name, description=excluded.description, "
                " timeframe=excluded.timeframe, entry_conditions=excluded.entry_conditions, "
                " exit_conditions=excluded.exit_conditions, entry_logic=excluded.entry_logic, "
                " exit_logic=excluded.exit_logic, stop_loss=excluded.stop_loss, "
                " take_profit=excluded.take_profit, trailing_stop=excluded.trailing_stop, "
                " updated_at=CURRENT_TIMESTAMP",
                args);
        } else {
            fincept::Database::instance().execute(
                "INSERT OR IGNORE INTO algo_strategies "
                "(id, name, description, timeframe, entry_conditions, exit_conditions, "
                " entry_logic, exit_logic, stop_loss, take_profit, trailing_stop, is_active, "
                " created_at, updated_at) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)",
                args);
        }
    }

    if (force) {
        fincept::Database::instance().execute(
            "INSERT INTO key_value_storage(key, value, updated_at) "
            "VALUES('algo_library_version', ?, CAST(strftime('%s','now') AS INTEGER)) "
            "ON CONFLICT(key) DO UPDATE SET value=excluded.value, updated_at=excluded.updated_at",
            {QString::number(kLibraryVersion)});
        LOG_INFO("AlgoTrading", QString("Curated library refreshed to v%1").arg(kLibraryVersion));
    }
}

void AlgoTradingService::list_strategies() {
    seed_library(); // idempotent — ensures the curated library exists
    QVector<AlgoStrategy> all = load_dsl_strategies_from_db();
    LOG_INFO("AlgoTrading", QString("Loaded %1 strategies from DB").arg(all.size()));
    emit strategies_loaded(all);
}

void AlgoTradingService::delete_strategy(const QString& id) {
    // Soft delete — keeps history; list_strategies filters on is_active = 1.
    auto r = fincept::Database::instance().execute(
        "UPDATE algo_strategies SET is_active = 0, updated_at = CURRENT_TIMESTAMP WHERE id = ?", {id});
    if (r.is_err()) {
        LOG_ERROR("AlgoTrading", QString("delete_strategy failed: %1").arg(QString::fromStdString(r.error())));
        emit error_occurred("delete_strategy", QString::fromStdString(r.error()));
        return;
    }
    LOG_INFO("AlgoTrading", QString("Deleted strategy %1").arg(id));
    emit strategy_deleted(id);
}

// ── Deployment lifecycle — now handled by AlgoEngine (src/algo_engine/) ──────
// deploy_strategy, stop_deployment, stop_all_deployments, remove_deployment,
// list_deployments, check_bridge_needed have been removed.
// Use fincept::algo::AlgoEngine::instance() for all deployment operations.

// ── Backtesting (native C++ engine) ────────────────────────────────────────────
void AlgoTradingService::run_backtest(const AlgoStrategy& strategy, const QString& symbol, const QString& start_date,
                                      const QString& end_date, double capital) {
    // CandleDataFetcher only knows "the last N days, ending now". The From/To pickers used
    // to be reduced to just the span (To - From), so asking for 2021 silently backtested
    // the most recent span instead. Fetch from (From - indicator warm-up) up to today, then
    // trim to [From, To] below once the candles are in hand.
    const QDate d1 = QDate::fromString(start_date, "yyyy-MM-dd");
    const QDate d2 = QDate::fromString(end_date, "yyyy-MM-dd");
    const bool have_range = d1.isValid() && d2.isValid() && d1 < d2;
    const QString tf_for_warmup = strategy.timeframe.isEmpty() ? QStringLiteral("1d") : strategy.timeframe;
    // BacktestEngine skips its first 50 bars for indicator warm-up; pull enough extra
    // history that evaluation starts on the requested From date. A session is ~6.5h, so
    // 50 bars of N seconds is 50*N/23400 trading days; x1.5 turns that into calendar days.
    const int tf_secs = fincept::algo::timeframe_seconds(fincept::algo::timeframe_from_string(tf_for_warmup));
    const int warm_days = qMax(3, static_cast<int>(std::ceil(50.0 * tf_secs / 23400.0 * 1.5)));
    int lookback_days = 365;
    if (have_range)
        lookback_days = static_cast<int>(d1.daysTo(QDate::currentDate())) + warm_days + 1;
    if (lookback_days < 1)
        lookback_days = 365;
    const qint64 range_start_ms =
        have_range ? QDateTime(d1, QTime(0, 0), QTimeZone::UTC).toMSecsSinceEpoch() : 0;
    const qint64 range_end_ms =
        have_range ? QDateTime(d2.addDays(1), QTime(0, 0), QTimeZone::UTC).toMSecsSinceEpoch() : 0; // exclusive
    // Latest-request-wins: a slow earlier fetch must not paint over a newer run's report.
    const int my_seq = ++backtest_seq_;

    // Data source: a connected broker if one exists, otherwise native Yahoo.
    QString broker_id, account_id;
    auto& accts = trading::AccountManager::instance();
    for (const auto& acc : accts.active_accounts()) {
        if (accts.connection_state(acc.account_id) == trading::ConnectionState::Connected) {
            broker_id = acc.broker_id;
            account_id = acc.account_id;
            break;
        }
    }
    const fincept::algo::DataSource source =
        broker_id.isEmpty() ? fincept::algo::DataSource::YFinance : fincept::algo::DataSource::Auto;

    // Capture strategy parameters for the async callback.
    const QJsonArray entry = strategy.entry_conditions;
    const QJsonArray exit = strategy.exit_conditions;
    const QString entry_logic = strategy.entry_logic.isEmpty() ? QStringLiteral("AND") : strategy.entry_logic;
    const QString exit_logic = strategy.exit_logic.isEmpty() ? QStringLiteral("AND") : strategy.exit_logic;
    const double sl = strategy.stop_loss;
    const double tp = strategy.take_profit;
    const double trail = strategy.trailing_stop;
    const double size_pct = strategy.position_size_pct > 0 ? strategy.position_size_pct : 100.0;
    const QString timeframe = strategy.timeframe.isEmpty() ? QStringLiteral("1d") : strategy.timeframe;

    LOG_INFO("AlgoTrading", QString("Backtest %1 [%2] %3 — source=%4")
                                .arg(symbol, timeframe, strategy.name, broker_id.isEmpty() ? "yahoo" : broker_id));

    // Singleton — `this` outlives any async work, so capture directly.
    fincept::algo::CandleDataFetcher::instance().fetch(
        symbol, timeframe, lookback_days, source, broker_id, account_id,
        [this, entry, exit, entry_logic, exit_logic, sl, tp, trail, size_pct, capital, timeframe, symbol, my_seq,
         have_range, range_start_ms, range_end_ms,
         warm_bars = kAlgoSvcBtWarmupBars](bool ok, const QVector<fincept::algo::OhlcvCandle>& fetched,
                                           const QString& err) {
            if (my_seq != backtest_seq_)
                return; // superseded by a newer run
            if (!ok || fetched.isEmpty()) {
                emit error_occurred("backtest", err.isEmpty() ? QStringLiteral("No data") : err);
                return;
            }

            // Trim to the requested [From, To] window, keeping `warm_bars` of history before
            // From so indicators are already warm on the first evaluated bar.
            QVector<fincept::algo::OhlcvCandle> candles = fetched;
            if (have_range) {
                int first = 0;
                while (first < fetched.size() && fetched[first].open_time < range_start_ms)
                    ++first;
                int last = fetched.size();
                while (last > first && fetched[last - 1].open_time >= range_end_ms)
                    --last;
                if (last <= first) {
                    emit error_occurred("backtest",
                                        QStringLiteral("No %1 bars between %2 and %3 — the data source only returned "
                                                       "history from %4. Intraday history is capped by the provider; "
                                                       "pick a more recent range.")
                                            .arg(timeframe,
                                                 QDateTime::fromMSecsSinceEpoch(range_start_ms, QTimeZone::UTC)
                                                     .toString("yyyy-MM-dd"),
                                                 QDateTime::fromMSecsSinceEpoch(range_end_ms - 1, QTimeZone::UTC)
                                                     .toString("yyyy-MM-dd"),
                                                 QDateTime::fromMSecsSinceEpoch(fetched.first().open_time,
                                                                                QTimeZone::UTC)
                                                     .toString("yyyy-MM-dd")));
                    return;
                }
                const int begin = qMax(0, first - warm_bars);
                candles = fetched.mid(begin, last - begin);
            }
            const QJsonObject result = fincept::algo::BacktestEngine::run(candles, entry, entry_logic, exit, exit_logic,
                                                                          sl, tp, trail, capital, timeframe, size_pct);
            if (!result.value("success").toBool(false)) {
                emit error_occurred("backtest", result.value("error").toString(QStringLiteral("Backtest failed")));
                return;
            }
            emit backtest_result(result);
        });
}

// Scanner is now in AlgoScanner (src/algo_engine/AlgoScanner.h/.cpp).

} // namespace fincept::services::algo
