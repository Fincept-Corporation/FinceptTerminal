// src/services/algo_trading/AlgoTradingService.cpp
#include "services/algo_trading/AlgoTradingService.h"

#include "algo_engine/AlgoMarket.h"
#include "algo_engine/BacktestEngine.h"
#include "algo_engine/CandleDataFetcher.h"
#include "algo_engine/ExecutionModel.h"
#include "core/logging/Logger.h"
#include "core/market/MarketProfile.h"
#include "core/market/MarketSessions.h"
#include "services/algo_trading/AlgoRuleLogic.h"
#include "services/algo_trading/AlgoStrategyLibrary.h"
#include "storage/sqlite/Database.h"
#include "trading/AccountManager.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QTime>
#include <QTimeZone>
#include <QUuid>
#include <QtConcurrent>

#include <cmath>

namespace fincept::services::algo {

// Fewest bars of history kept before the backtest From date (more when the rules' slowest
// indicator needs it — rules::warmup_bars). Passed to BacktestEngine as its warm-up so
// evaluation starts on From with every indicator defined.
static constexpr int kAlgoSvcBtWarmupBars = 50;
// Points kept in a persisted last-backtest equity curve (sparkline in the strategy list).
static constexpr int kAlgoSvcSummaryPoints = 120;

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

    // v057 columns in their own statement: on a database that has not run v057 yet
    // this one fails (logged) while the strategy itself is still saved.
    auto extra = fincept::Database::instance().execute(
        "UPDATE algo_strategies SET position_size_pct = ?, cost_model_json = ?, entry_side = ?, risk_free_pct = ? "
        "WHERE id = ?",
        {strategy.position_size_pct > 0 ? strategy.position_size_pct : 100.0,
         QString::fromUtf8(QJsonDocument(strategy.cost_model).toJson(QJsonDocument::Compact)),
         strategy.entry_side.isEmpty() ? QStringLiteral("BUY") : strategy.entry_side, strategy.risk_free_pct,
         resolved_id});
    if (extra.is_err())
        LOG_WARN("AlgoTrading", logf("Strategy saved without its size and cost fields (schema v057 missing?)",
                                     {{"strategy", resolved_id}, {"error", QString::fromStdString(extra.error())}}));

    // A draft backtested before its first save: the run had no row to update, so the
    // builder hands its summary over here (left untouched when empty).
    if (!strategy.last_backtest.isEmpty()) {
        const QString at = strategy.last_backtest.value(QStringLiteral("run_at")).toString();
        auto bt = fincept::Database::instance().execute(
            "UPDATE algo_strategies SET last_backtest_json = ?, last_backtest_at = ? WHERE id = ?",
            {QString::fromUtf8(QJsonDocument(strategy.last_backtest).toJson(QJsonDocument::Compact)), at,
             resolved_id});
        if (bt.is_ok())
            emit strategy_backtest_saved(resolved_id, strategy.last_backtest);
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

    // v057 fields (sizing, costs, side, risk-free, last backtest) — best effort.
    auto x = fincept::Database::instance().execute(
        "SELECT id, position_size_pct, cost_model_json, entry_side, risk_free_pct, last_backtest_json, "
        "last_backtest_at FROM algo_strategies WHERE is_active = 1",
        {});
    if (x.is_ok()) {
        QHash<QString, int> idx;
        for (int i = 0; i < result.size(); ++i)
            idx.insert(result[i].id, i);
        auto& xq = x.value();
        while (xq.next()) {
            const int i = idx.value(xq.value(0).toString(), -1);
            if (i < 0)
                continue;
            AlgoStrategy& s = result[i];
            const double size = xq.value(1).toDouble();
            s.position_size_pct = size > 0 ? size : 100.0;
            s.cost_model = QJsonDocument::fromJson(xq.value(2).toString().toUtf8()).object();
            const QString side = xq.value(3).toString();
            s.entry_side = side.isEmpty() ? QStringLiteral("BUY") : side;
            s.risk_free_pct = xq.value(4).toDouble();
            s.last_backtest = QJsonDocument::fromJson(xq.value(5).toString().toUtf8()).object();
            s.last_backtest_at = xq.value(6).toString();
        }
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
        LOG_DEBUG("AlgoTrading", QString("Curated library refreshed to v%1").arg(kLibraryVersion));
    }
}

void AlgoTradingService::list_strategies() {
    seed_library(); // idempotent — ensures the curated library exists
    QVector<AlgoStrategy> all = load_dsl_strategies_from_db();
    LOG_DEBUG("AlgoTrading", QString("Loaded %1 strategies from DB").arg(all.size()));
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

// Deployment lifecycle — handled by AlgoEngine (src/algo_engine/).

// ── Backtesting (native C++ engine) ────────────────────────────────────────────

QString AlgoTradingService::default_exchange_hint(const QString& symbol) {
    const QString s = symbol.trimmed().toUpper();
    const int colon = s.indexOf(QLatin1Char(':'));
    if (colon > 0)
        return s.left(colon); // "NSE:RELIANCE-EQ"
    // A connected broker lists the exchanges it trades; its first one is where a
    // bare symbol typed for it lives.
    auto& accts = trading::AccountManager::instance();
    for (const auto& acc : accts.active_accounts()) {
        if (accts.connection_state(acc.account_id) != trading::ConnectionState::Connected)
            continue;
        if (auto* broker = trading::BrokerRegistry::instance().get(acc.broker_id)) {
            const auto ex = broker->profile().exchanges;
            if (!ex.isEmpty())
                return ex.first();
        }
        break;
    }
    // Otherwise the user's market region (Settings → market), primary venue first.
    const auto& prof = fincept::core::market::MarketProfileStore::instance().current();
    return prof.venues.isEmpty() ? QString() : prof.venues.first();
}

QJsonObject AlgoTradingService::backtest_summary(const QJsonObject& r) {
    QJsonObject s;
    static const char* const kKeys[] = {
        "symbol",         "venue",         "currency",        "timeframe",      "side",
        "start_time",     "end_time",      "initial_capital", "final_value",    "total_return",
        "gross_return",   "cagr",          "sharpe_ratio",    "sortino",        "calmar",
        "max_drawdown",   "win_rate",      "profit_factor",   "profit_factor_infinite",
        "total_trades",   "expectancy",    "total_costs",     "benchmark_return", "exposure_pct",
        "costs_applied",  "ann_volatility", "risk_free_rate"};
    for (const char* k : kKeys) {
        const QJsonValue v = r.value(QLatin1String(k));
        if (!v.isUndefined())
            s[QLatin1String(k)] = v;
    }
    s["cost_preset"] = r.value("cost_model").toObject().value("preset");
    // Thinned equity curve for sparklines (keeps the last point).
    const QJsonArray eq = r.value("equity_curve").toArray();
    QJsonArray thin;
    if (!eq.isEmpty()) {
        const int step = std::max(1, static_cast<int>(eq.size()) / kAlgoSvcSummaryPoints);
        for (int i = 0; i < eq.size(); i += step)
            thin.append(eq[i]);
        if (thin.last() != eq.last())
            thin.append(eq.last());
    }
    s["equity_spark"] = thin;
    s["run_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    return s;
}

void AlgoTradingService::persist_last_backtest(const QString& strategy_id, const QJsonObject& result) {
    if (strategy_id.isEmpty())
        return;
    const QJsonObject summary = backtest_summary(result);
    auto r = fincept::Database::instance().execute(
        "UPDATE algo_strategies SET last_backtest_json = ?, last_backtest_at = ? WHERE id = ?",
        {QString::fromUtf8(QJsonDocument(summary).toJson(QJsonDocument::Compact)), summary.value("run_at").toString(),
         strategy_id});
    if (r.is_err()) {
        LOG_WARN("AlgoTrading", QString("last backtest not saved (schema v057 missing?): %1")
                                    .arg(QString::fromStdString(r.error())));
        return;
    }
    emit strategy_backtest_saved(strategy_id, summary);
}

void AlgoTradingService::cancel_backtest() {
    if (!backtest_running_)
        return;
    if (backtest_cancel_)
        backtest_cancel_->store(true);
    ++backtest_seq_; // drop whatever the fetch / worker delivers from now on
    backtest_running_ = false;
    LOG_INFO("AlgoTrading", "Backtest cancelled by the user");
    emit backtest_cancelled();
}

namespace {

// Everything a backtest / sweep run needs before its data arrives.
struct AlgoSvcRun {
    fincept::algo::BacktestConfig cfg;
    QString timeframe;
    QString exchange_hint;
    QString yahoo;
    QString broker_id;
    QString account_id;
    fincept::algo::DataSource source = fincept::algo::DataSource::YFinance;
    int lookback_days = 365;
    bool have_range = false;
    bool live_tf = false;
    qint64 range_start_ms = 0;
    qint64 range_end_ms = 0;
};

AlgoSvcRun algo_svc_prepare(const AlgoStrategy& strategy, const QString& symbol, const QString& start_date,
                            const QString& end_date, double capital, const QString& exchange_hint_in) {
    namespace fa = fincept::algo;
    AlgoSvcRun run;
    // CandleDataFetcher only knows "the last N days, ending now". Fetch from (From -
    // indicator warm-up) up to today, then trim to [From, To] once the candles are in.
    const QDate d1 = QDate::fromString(start_date, "yyyy-MM-dd");
    const QDate d2 = QDate::fromString(end_date, "yyyy-MM-dd");
    run.have_range = d1.isValid() && d2.isValid() && d1 < d2;

    // "live" evaluates per tick when deployed; its warm-up / backtest bars are the
    // bars the runner aggregates (live_bar_timeframe), so indicator periods mean
    // the same thing in the backtest and in the deployment.
    run.timeframe = fa::effective_bar_timeframe(strategy.timeframe);
    run.live_tf = strategy.timeframe.compare(QStringLiteral("live"), Qt::CaseInsensitive) == 0;
    const int tf_secs = fa::bar_seconds_for(run.timeframe);

    run.exchange_hint =
        exchange_hint_in.isEmpty() ? AlgoTradingService::default_exchange_hint(symbol) : exchange_hint_in;
    const auto* venue = fa::algo_venue(symbol, run.exchange_hint);

    // Warm-up history in CALENDAR days from the venue's real session length (it used
    // to assume a 6.5-hour session everywhere), with weekends / holidays as slack.
    double session_min = venue ? fincept::core::market::session_minutes(*venue) : 390.0;
    if (!(session_min > 0))
        session_min = 1440.0; // 24h markets
    const double bars_per_day = tf_secs >= 86400 ? 1.0 : std::max(1.0, session_min * 60.0 / tf_secs);
    // Warm-up from the rules' slowest indicator (SMA 200 needs 200 bars before From, not the
    // fixed 50 it used to get — the Golden Cross could not fire for the first ~150 bars).
    const int warmup_bars = fincept::services::algo::rules::warmup_bars(strategy.entry_conditions,
                                                                        strategy.exit_conditions, kAlgoSvcBtWarmupBars);
    const int warm_days = qMax(3, static_cast<int>(std::ceil(warmup_bars / bars_per_day * 1.6)) + 2);
    if (run.have_range)
        run.lookback_days = static_cast<int>(d1.daysTo(QDate::currentDate())) + warm_days + 1;
    if (run.lookback_days < 1)
        run.lookback_days = 365;
    // The From / To dates are the exchange's calendar dates.
    const QTimeZone tz = venue ? fincept::core::market::venue_timezone(*venue) : QTimeZone(QTimeZone::UTC);
    run.range_start_ms = run.have_range ? QDateTime(d1, QTime(0, 0), tz).toMSecsSinceEpoch() : 0;
    run.range_end_ms = run.have_range ? QDateTime(d2.addDays(1), QTime(0, 0), tz).toMSecsSinceEpoch() : 0;

    // Data source: a connected broker if one exists, otherwise native Yahoo.
    auto& accts = trading::AccountManager::instance();
    for (const auto& acc : accts.active_accounts()) {
        if (accts.connection_state(acc.account_id) == trading::ConnectionState::Connected) {
            run.broker_id = acc.broker_id;
            run.account_id = acc.account_id;
            break;
        }
    }
    run.source = run.broker_id.isEmpty() ? fa::DataSource::YFinance : fa::DataSource::Auto;

    fa::BacktestConfig& cfg = run.cfg;
    cfg.entry_conditions = strategy.entry_conditions;
    cfg.exit_conditions = strategy.exit_conditions;
    cfg.entry_logic = strategy.entry_logic.isEmpty() ? QStringLiteral("AND") : strategy.entry_logic;
    cfg.exit_logic = strategy.exit_logic.isEmpty() ? QStringLiteral("AND") : strategy.exit_logic;
    cfg.stop_loss_pct = strategy.stop_loss;
    cfg.take_profit_pct = strategy.take_profit;
    cfg.trailing_stop_pct = strategy.trailing_stop;
    cfg.initial_capital = capital;
    cfg.position_size_pct = strategy.position_size_pct > 0 ? strategy.position_size_pct : 100.0;
    cfg.timeframe = run.timeframe;
    cfg.side = strategy.entry_side.isEmpty() ? QStringLiteral("BUY") : strategy.entry_side;
    cfg.risk_free_pct = strategy.risk_free_pct;
    cfg.symbol = symbol.trimmed().toUpper();
    cfg.venue = fa::venue_id_of(venue);
    cfg.currency = fa::venue_currency_of(venue);
    cfg.periods_per_year = fa::algo_periods_per_year(venue, tf_secs);
    cfg.trading_days_per_year = venue ? fincept::core::market::trading_days_per_year(*venue) : 252.0;
    cfg.warmup_bars = warmup_bars;
    if (strategy.cost_model.isEmpty()) {
        // No model chosen yet: the venue's approximate preset rather than a silent
        // zero-cost run (the report names the preset and says it is approximate).
        cfg.costs = fa::execution::preset(fa::execution::default_preset_for(fa::venue_country_of(venue),
                                                                            tf_secs < 86400))
                        .model;
    } else {
        cfg.costs = fa::CostModel::from_json(strategy.cost_model);
    }
    run.yahoo = fa::yahoo_symbol_for(symbol, run.exchange_hint);
    return run;
}

// Trim to the requested [From, To] window, keeping warm-up bars of history before
// From so indicators are already warm on the first evaluated bar. Empty + `error`
// when the provider returned nothing inside the window.
QVector<fincept::algo::OhlcvCandle> algo_svc_trim(const QVector<fincept::algo::OhlcvCandle>& fetched, AlgoSvcRun& run,
                                                   QString* error) {
    if (!run.have_range)
        return fetched;
    int first = 0;
    while (first < fetched.size() && fetched[first].open_time < run.range_start_ms)
        ++first;
    int last = fetched.size();
    while (last > first && fetched[last - 1].open_time >= run.range_end_ms)
        --last;
    if (last <= first) {
        if (error)
            *error = AlgoTradingService::tr("No %1 bars between %2 and %3 — the data source only returned history "
                                            "from %4. Intraday history is capped by the provider; pick a more recent "
                                            "range.")
                         .arg(run.timeframe,
                              QDateTime::fromMSecsSinceEpoch(run.range_start_ms, QTimeZone::UTC).toString("yyyy-MM-dd"),
                              QDateTime::fromMSecsSinceEpoch(run.range_end_ms - 1, QTimeZone::UTC)
                                  .toString("yyyy-MM-dd"),
                              QDateTime::fromMSecsSinceEpoch(fetched.first().open_time, QTimeZone::UTC)
                                  .toString("yyyy-MM-dd"));
        return {};
    }
    const int begin = qMax(0, first - run.cfg.warmup_bars);
    // Fewer warm-up bars than asked (history starts near From): evaluate from From
    // anyway rather than shifting the start.
    run.cfg.warmup_bars = qMax(1, qMin(run.cfg.warmup_bars, first - begin));
    return fetched.mid(begin, last - begin);
}

} // namespace

void AlgoTradingService::run_backtest(const AlgoStrategy& strategy, const QString& symbol, const QString& start_date,
                                      const QString& end_date, double capital, const QString& exchange_hint_in) {
    namespace fa = fincept::algo;
    AlgoSvcRun run = algo_svc_prepare(strategy, symbol, start_date, end_date, capital, exchange_hint_in);

    // Latest-request-wins: a slow earlier fetch must not paint over a newer run's report.
    if (backtest_cancel_)
        backtest_cancel_->store(true);
    backtest_cancel_ = std::make_shared<std::atomic<bool>>(false);
    const auto cancel = backtest_cancel_;
    const int my_seq = ++backtest_seq_;
    backtest_running_ = true;
    const QString strategy_id = strategy.id;

    LOG_INFO("AlgoTrading", QString("Backtest %1 (%2, venue %3) [%4%5] %6 — source=%7")
                                .arg(symbol, run.yahoo, run.cfg.venue.isEmpty() ? QStringLiteral("?") : run.cfg.venue,
                                     run.timeframe, run.live_tf ? QStringLiteral(" for live") : QString(), strategy.name,
                                     run.broker_id.isEmpty() ? QStringLiteral("yahoo") : run.broker_id));
    emit backtest_started(run.cfg.symbol, run.yahoo);
    emit backtest_progress(0, tr("Loading %1 bars…").arg(run.timeframe));

    QPointer<AlgoTradingService> self = this;
    fa::CandleDataFetcher::instance().fetch(
        symbol, run.timeframe, run.lookback_days, run.source, run.broker_id, run.account_id,
        [self, run, my_seq, cancel, strategy_id](bool ok, const QVector<fa::OhlcvCandle>& fetched,
                                                 const QString& err) mutable {
            if (!self || my_seq != self->backtest_seq_ || cancel->load())
                return; // superseded or cancelled
            if (!ok || fetched.isEmpty()) {
                self->backtest_running_ = false;
                emit self->error_occurred("backtest",
                                          err.isEmpty() ? tr("No price data for %1").arg(run.cfg.symbol) : err);
                return;
            }
            QString trim_err;
            const QVector<fa::OhlcvCandle> candles = algo_svc_trim(fetched, run, &trim_err);
            if (candles.isEmpty()) {
                self->backtest_running_ = false;
                emit self->error_occurred("backtest", trim_err);
                return;
            }
            fa::BacktestConfig cfg = run.cfg;
            emit self->backtest_progress(5, tr("Running %1 bars…").arg(candles.size()));

            // The engine re-evaluates every rule on a sliding window per bar — seconds
            // for long intraday runs — so it runs on a worker, never the GUI thread.
            QPointer<AlgoTradingService> worker_self = self;
            cfg.cancel = cancel.get();
            cfg.progress = [worker_self, my_seq](int pct) {
                if (!worker_self)
                    return;
                QMetaObject::invokeMethod(
                    worker_self.data(),
                    [worker_self, my_seq, pct]() {
                        if (worker_self && my_seq == worker_self->backtest_seq_)
                            emit worker_self->backtest_progress(5 + pct * 95 / 100, tr("Simulating…"));
                    },
                    Qt::QueuedConnection);
            };
            (void)QtConcurrent::run([worker_self, cfg, candles, cancel, my_seq, strategy_id]() {
                const QJsonObject result = fa::BacktestEngine::run(candles, cfg);
                if (!worker_self)
                    return;
                QMetaObject::invokeMethod(
                    worker_self.data(),
                    [worker_self, result, cancel, my_seq, strategy_id]() {
                        if (!worker_self || my_seq != worker_self->backtest_seq_ || cancel->load())
                            return;
                        worker_self->backtest_running_ = false;
                        if (!result.value("success").toBool(false)) {
                            emit worker_self->error_occurred(
                                "backtest", result.value("error").toString(tr("Backtest failed")));
                            return;
                        }
                        emit worker_self->backtest_result(result);
                        worker_self->persist_last_backtest(strategy_id, result);
                    },
                    Qt::QueuedConnection);
            });
        },
        run.exchange_hint);
}

void AlgoTradingService::cancel_optimization() {
    if (optimize_cancel_)
        optimize_cancel_->store(true);
    ++optimize_seq_;
}

void AlgoTradingService::run_optimization(const AlgoStrategy& strategy, const QString& symbol,
                                          const QString& start_date, const QString& end_date, double capital,
                                          const QVector<double>& stop_losses, const QVector<double>& take_profits,
                                          double oos_fraction) {
    namespace fa = fincept::algo;
    if (stop_losses.isEmpty() || take_profits.isEmpty()) {
        emit optimization_failed(tr("Give at least one stop-loss and one take-profit value."));
        return;
    }
    AlgoSvcRun run = algo_svc_prepare(strategy, symbol, start_date, end_date, capital, QString());
    if (optimize_cancel_)
        optimize_cancel_->store(true);
    optimize_cancel_ = std::make_shared<std::atomic<bool>>(false);
    const auto cancel = optimize_cancel_;
    const int my_seq = ++optimize_seq_;
    oos_fraction = std::clamp(oos_fraction, 0.0, 0.6);
    emit optimization_progress(0, tr("Loading %1 bars…").arg(run.timeframe));

    QPointer<AlgoTradingService> self = this;
    fa::CandleDataFetcher::instance().fetch(
        symbol, run.timeframe, run.lookback_days, run.source, run.broker_id, run.account_id,
        [self, run, my_seq, cancel, stop_losses, take_profits, oos_fraction](
            bool ok, const QVector<fa::OhlcvCandle>& fetched, const QString& err) mutable {
            if (!self || my_seq != self->optimize_seq_ || cancel->load())
                return;
            if (!ok || fetched.isEmpty()) {
                emit self->optimization_failed(err.isEmpty() ? tr("No price data for %1").arg(run.cfg.symbol) : err);
                return;
            }
            QString trim_err;
            const QVector<fa::OhlcvCandle> candles = algo_svc_trim(fetched, run, &trim_err);
            if (candles.isEmpty()) {
                emit self->optimization_failed(trim_err);
                return;
            }
            const int warm = run.cfg.warmup_bars;
            const int evaluated = candles.size() - warm;
            const int oos_bars = static_cast<int>(evaluated * oos_fraction);
            const int is_end = candles.size() - oos_bars; // in-sample = [0, is_end)
            if (is_end - warm < 30) {
                emit self->optimization_failed(
                    tr("Not enough bars for a sweep: %1 evaluated bars (need at least 30 in-sample).").arg(evaluated));
                return;
            }
            QPointer<AlgoTradingService> worker_self = self;
            const fa::BacktestConfig base = run.cfg;
            (void)QtConcurrent::run([worker_self, base, candles, warm, is_end, oos_bars, stop_losses, take_profits,
                                     cancel, my_seq]() {
                const QVector<fa::OhlcvCandle> in_sample = candles.mid(0, is_end);
                QJsonArray ret_rows, sharpe_rows, dd_rows, trades_rows;
                double best_sharpe = -1e300;
                int best_i = -1, best_j = -1;
                QJsonObject best_is;
                const int total = stop_losses.size() * take_profits.size();
                int done = 0;
                for (int i = 0; i < stop_losses.size(); ++i) {
                    QJsonArray r_row, s_row, d_row, t_row;
                    for (int j = 0; j < take_profits.size(); ++j) {
                        if (cancel->load())
                            return;
                        fa::BacktestConfig cfg = base;
                        cfg.stop_loss_pct = stop_losses[i];
                        cfg.take_profit_pct = take_profits[j];
                        cfg.include_bars = false;
                        cfg.cancel = cancel.get();
                        const QJsonObject r = fa::BacktestEngine::run(in_sample, cfg);
                        const bool okr = r.value("success").toBool();
                        const double sharpe = okr ? r.value("sharpe_ratio").toDouble() : std::nan("");
                        r_row.append(okr ? r.value("total_return") : QJsonValue(QJsonValue::Null));
                        s_row.append(okr ? QJsonValue(sharpe) : QJsonValue(QJsonValue::Null));
                        d_row.append(okr ? r.value("max_drawdown") : QJsonValue(QJsonValue::Null));
                        t_row.append(okr ? r.value("total_trades") : QJsonValue(QJsonValue::Null));
                        // Best by in-sample Sharpe, among cells that actually traded.
                        if (okr && r.value("total_trades").toInt() > 0 && std::isfinite(sharpe) &&
                            sharpe > best_sharpe) {
                            best_sharpe = sharpe;
                            best_i = i;
                            best_j = j;
                            best_is = r;
                        }
                        ++done;
                        const int pct = 5 + done * 85 / std::max(1, total);
                        if (worker_self)
                            QMetaObject::invokeMethod(
                                worker_self.data(),
                                [worker_self, my_seq, pct, done, total]() {
                                    if (worker_self && my_seq == worker_self->optimize_seq_)
                                        emit worker_self->optimization_progress(
                                            pct, tr("Cell %1 of %2…").arg(done).arg(total));
                                },
                                Qt::QueuedConnection);
                    }
                    ret_rows.append(r_row);
                    sharpe_rows.append(s_row);
                    dd_rows.append(d_row);
                    trades_rows.append(t_row);
                }

                QJsonObject out;
                out["sl"] = [&] {
                    QJsonArray a;
                    for (double v : stop_losses)
                        a.append(v);
                    return a;
                }();
                out["tp"] = [&] {
                    QJsonArray a;
                    for (double v : take_profits)
                        a.append(v);
                    return a;
                }();
                out["net_return"] = ret_rows;
                out["sharpe"] = sharpe_rows;
                out["max_drawdown"] = dd_rows;
                out["trades"] = trades_rows;
                out["bars_in_sample"] = is_end - warm;
                out["bars_out_of_sample"] = oos_bars;
                out["split_time"] = static_cast<double>(is_end < candles.size() ? candles[is_end].open_time : 0);
                out["symbol"] = base.symbol;
                out["currency"] = base.currency;
                out["costs_applied"] = !base.costs.is_zero();

                if (best_i >= 0) {
                    QJsonObject best;
                    best["sl"] = stop_losses[best_i];
                    best["tp"] = take_profits[best_j];
                    auto pick = [](const QJsonObject& r) {
                        QJsonObject o;
                        for (const char* k : {"total_return", "sharpe_ratio", "max_drawdown", "total_trades",
                                              "win_rate", "profit_factor", "cagr", "start_time", "end_time"})
                            o[QLatin1String(k)] = r.value(QLatin1String(k));
                        return o;
                    };
                    best["in_sample"] = pick(best_is);
                    // The chosen cell on the untouched tail (with warm-up bars before it).
                    if (oos_bars >= 10) {
                        fa::BacktestConfig cfg = base;
                        cfg.stop_loss_pct = stop_losses[best_i];
                        cfg.take_profit_pct = take_profits[best_j];
                        cfg.include_bars = false;
                        cfg.cancel = cancel.get();
                        const int oos_begin = std::max(0, is_end - warm);
                        cfg.warmup_bars = is_end - oos_begin;
                        const QJsonObject oos = fa::BacktestEngine::run(candles.mid(oos_begin), cfg);
                        if (oos.value("success").toBool())
                            best["out_of_sample"] = pick(oos);
                    }
                    out["best"] = best;
                }
                if (!worker_self)
                    return;
                QMetaObject::invokeMethod(
                    worker_self.data(),
                    [worker_self, out, cancel, my_seq]() {
                        if (!worker_self || my_seq != worker_self->optimize_seq_ || cancel->load())
                            return;
                        emit worker_self->optimization_result(out);
                    },
                    Qt::QueuedConnection);
            });
        },
        run.exchange_hint);
}

// Scanner is now in AlgoScanner (src/algo_engine/AlgoScanner.h/.cpp).

} // namespace fincept::services::algo
