#include "storage/HistoricalDataStore.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"
#include "trading/AccountManager.h"
#include "trading/BrokerInterface.h"

#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QTimeZone>
#include <QtConcurrent>
#include <QVariantList>

#include <algorithm>

namespace fincept::storage {

namespace {

Database& db() { return Database::instance(); }

// Map a SELECT row (timestamp_ms, open, high, low, close, volume, oi) to a candle.
trading::BrokerCandle map_candle(QSqlQuery& q) {
    trading::BrokerCandle c;
    c.timestamp = q.value(0).toLongLong();
    c.open = q.value(1).toDouble();
    c.high = q.value(2).toDouble();
    c.low = q.value(3).toDouble();
    c.close = q.value(4).toDouble();
    c.volume = q.value(5).toDouble();
    c.oi = q.value(6).toDouble();
    return c;
}

// Resample plan: which stored base interval a target is computed from, and the
// fixed bucket width in ms for time-aligned targets (4h/8h). Calendar targets
// (1w/1mo) bucket by date boundaries instead — see bucket_key_*.
struct ResamplePlan {
    QString base_interval;     // "1h" or "1d"
    qint64 fixed_bucket_ms = 0; // >0 for 4h/8h; 0 = calendar bucketing
    bool calendar_week = false;
    bool calendar_month = false;
};

// Returns a plan for the supported resample targets, or std::nullopt if the
// target is not one we resample (caller should fall back to get_candles).
std::optional<ResamplePlan> plan_for(const QString& target_interval) {
    const QString t = target_interval.trimmed().toLower();
    constexpr qint64 hour_ms = 3600LL * 1000;
    if (t == "4h")
        return ResamplePlan{"1h", 4 * hour_ms, false, false};
    if (t == "8h")
        return ResamplePlan{"1h", 8 * hour_ms, false, false};
    if (t == "1w" || t == "1wk" || t == "1week")
        return ResamplePlan{"1d", 0, true, false};
    if (t == "1mo" || t == "1month" || t == "1mon")
        return ResamplePlan{"1d", 0, false, true};
    return std::nullopt;
}

// Bucket key for a fixed-width (4h/8h) target: floor(ts / width) * width gives
// the candle's epoch-ms start, aligned to the epoch.
qint64 fixed_bucket_start(qint64 ts_ms, qint64 width_ms) {
    if (width_ms <= 0)
        return ts_ms;
    return (ts_ms / width_ms) * width_ms;
}

// Bucket start (epoch ms) for a weekly candle: 00:00:00 UTC of the Monday of
// the candle's ISO week.
qint64 week_bucket_start(qint64 ts_ms) {
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(ts_ms, QTimeZone::utc());
    QDate d = dt.date();
    // Qt: dayOfWeek() Monday=1 .. Sunday=7. Roll back to Monday.
    int back = d.dayOfWeek() - 1;
    QDate monday = d.addDays(-back);
    return QDateTime(monday, QTime(0, 0, 0), QTimeZone::utc()).toMSecsSinceEpoch();
}

// Bucket start (epoch ms) for a monthly candle: 00:00:00 UTC of the 1st of the
// candle's calendar month.
qint64 month_bucket_start(qint64 ts_ms) {
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(ts_ms, QTimeZone::utc());
    QDate d = dt.date();
    QDate first(d.year(), d.month(), 1);
    return QDateTime(first, QTime(0, 0, 0), QTimeZone::utc()).toMSecsSinceEpoch();
}

} // anonymous namespace

HistoricalDataStore& HistoricalDataStore::instance() {
    static HistoricalDataStore s;
    return s;
}

// ── OHLCV storage ──────────────────────────────────────────────────────────────

bool HistoricalDataStore::store_candles(const QString& symbol, const QString& exchange,
                                        const QString& interval,
                                        const QVector<trading::BrokerCandle>& candles) {
    if (candles.isEmpty())
        return true;

    auto begin = db().begin_transaction();
    const bool in_tx = begin.is_ok();
    if (!in_tx)
        LOG_WARN("Historify", "transaction begin failed; storing without batch transaction");

    const QString sql = QStringLiteral(
        "INSERT OR REPLACE INTO market_data "
        "(symbol, exchange, interval, timestamp_ms, open, high, low, close, volume, oi) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    const QString sym = symbol.toUpper();
    const QString exc = exchange.toUpper();

    for (const auto& c : candles) {
        auto r = db().execute(sql, {sym, exc, interval, static_cast<qint64>(c.timestamp), c.open, c.high,
                                    c.low, c.close, c.volume, c.oi});
        if (r.is_err()) {
            LOG_ERROR("Historify", QString("store_candles failed for %1:%2:%3 — %4")
                                       .arg(sym, exc, interval, QString::fromStdString(r.error())));
            if (in_tx)
                db().rollback();
            return false;
        }
    }

    if (in_tx) {
        auto c = db().commit();
        if (c.is_err()) {
            LOG_ERROR("Historify", QString("commit failed — %1").arg(QString::fromStdString(c.error())));
            return false;
        }
    }
    LOG_INFO("Historify", QString("Stored %1 candles for %2:%3:%4")
                              .arg(candles.size())
                              .arg(sym, exc, interval));
    return true;
}

QVector<trading::BrokerCandle> HistoricalDataStore::get_candles(const QString& symbol,
                                                                const QString& exchange,
                                                                const QString& interval, qint64 from_ms,
                                                                qint64 to_ms) const {
    QString sql = QStringLiteral(
        "SELECT timestamp_ms, open, high, low, close, volume, oi FROM market_data "
        "WHERE symbol = ? AND exchange = ? AND interval = ?");
    QVariantList params{symbol.toUpper(), exchange.toUpper(), interval};

    if (from_ms > 0) {
        sql += " AND timestamp_ms >= ?";
        params.append(from_ms);
    }
    if (to_ms > 0) {
        sql += " AND timestamp_ms <= ?";
        params.append(to_ms);
    }
    sql += " ORDER BY timestamp_ms ASC";

    QVector<trading::BrokerCandle> out;
    auto r = db().execute(sql, params);
    if (r.is_err()) {
        LOG_ERROR("Historify", QString("get_candles failed — %1").arg(QString::fromStdString(r.error())));
        return out;
    }
    auto& q = r.value();
    while (q.next())
        out.append(map_candle(q));
    return out;
}

QVector<trading::BrokerCandle> HistoricalDataStore::get_resampled(const QString& symbol,
                                                                  const QString& exchange,
                                                                  const QString& target_interval,
                                                                  qint64 from_ms, qint64 to_ms) const {
    auto plan = plan_for(target_interval);
    if (!plan) {
        LOG_WARN("Historify", QString("get_resampled: unsupported target interval '%1'").arg(target_interval));
        return {};
    }

    // Load the base series. For calendar/fixed bucketing we widen the lower bound
    // is not strictly required (we just bucket whatever falls in [from,to]); a
    // partial leading bucket is acceptable and matches OpenAlgo's behaviour.
    QVector<trading::BrokerCandle> base = get_candles(symbol, exchange, plan->base_interval, from_ms, to_ms);
    if (base.isEmpty())
        return {};

    // base is already ordered ascending by timestamp. Aggregate sequentially:
    // open=first, high=max, low=min, close=last, volume=sum, oi=last.
    QVector<trading::BrokerCandle> out;
    bool have_bucket = false;
    qint64 cur_key = 0;
    trading::BrokerCandle agg;

    auto bucket_key = [&](qint64 ts) -> qint64 {
        if (plan->calendar_week)
            return week_bucket_start(ts);
        if (plan->calendar_month)
            return month_bucket_start(ts);
        return fixed_bucket_start(ts, plan->fixed_bucket_ms);
    };

    for (const auto& c : base) {
        const qint64 key = bucket_key(static_cast<qint64>(c.timestamp));
        if (!have_bucket || key != cur_key) {
            if (have_bucket)
                out.append(agg);
            // Start a new bucket. Stamp the candle at the bucket start.
            cur_key = key;
            have_bucket = true;
            agg = c;
            agg.timestamp = key;
        } else {
            agg.high = std::max(agg.high, c.high);
            agg.low = std::min(agg.low, c.low);
            agg.close = c.close; // last
            agg.volume += c.volume;
            agg.oi = c.oi; // last
        }
    }
    if (have_bucket)
        out.append(agg);
    return out;
}

// ── Watchlist ───────────────────────────────────────────────────────────────────

bool HistoricalDataStore::add_to_watchlist(const QString& symbol, const QString& exchange,
                                           const QString& interval) {
    auto r = db().execute(
        "INSERT OR IGNORE INTO historify_watchlist (symbol, exchange, interval) VALUES (?, ?, ?)",
        {symbol.toUpper(), exchange.toUpper(), interval});
    if (r.is_err()) {
        LOG_ERROR("Historify", QString("add_to_watchlist failed — %1").arg(QString::fromStdString(r.error())));
        return false;
    }
    return true;
}

bool HistoricalDataStore::remove_from_watchlist(const QString& symbol, const QString& exchange,
                                                const QString& interval) {
    auto r = db().execute(
        "DELETE FROM historify_watchlist WHERE symbol = ? AND exchange = ? AND interval = ?",
        {symbol.toUpper(), exchange.toUpper(), interval});
    if (r.is_err()) {
        LOG_ERROR("Historify",
                  QString("remove_from_watchlist failed — %1").arg(QString::fromStdString(r.error())));
        return false;
    }
    return true;
}

QVector<HistoricalDataStore::WatchEntry> HistoricalDataStore::watchlist() const {
    QVector<WatchEntry> out;
    auto r = db().execute("SELECT symbol, exchange, interval FROM historify_watchlist "
                          "ORDER BY symbol, exchange, interval");
    if (r.is_err()) {
        LOG_ERROR("Historify", QString("watchlist failed — %1").arg(QString::fromStdString(r.error())));
        return out;
    }
    auto& q = r.value();
    while (q.next()) {
        WatchEntry e;
        e.symbol = q.value(0).toString();
        e.exchange = q.value(1).toString();
        e.interval = q.value(2).toString();
        out.append(e);
    }
    return out;
}

// ── Catalog ───────────────────────────────────────────────────────────────────

QVector<HistoricalDataStore::CatalogEntry> HistoricalDataStore::catalog() const {
    QVector<CatalogEntry> out;
    auto r = db().execute(
        "SELECT symbol, exchange, interval, MIN(timestamp_ms), MAX(timestamp_ms), COUNT(*) "
        "FROM market_data GROUP BY symbol, exchange, interval ORDER BY symbol, exchange, interval");
    if (r.is_err()) {
        LOG_ERROR("Historify", QString("catalog failed — %1").arg(QString::fromStdString(r.error())));
        return out;
    }
    auto& q = r.value();
    while (q.next()) {
        CatalogEntry e;
        e.symbol = q.value(0).toString();
        e.exchange = q.value(1).toString();
        e.interval = q.value(2).toString();
        e.first_ts = q.value(3).toLongLong();
        e.last_ts = q.value(4).toLongLong();
        e.record_count = q.value(5).toInt();
        out.append(e);
    }
    return out;
}

// ── Export ────────────────────────────────────────────────────────────────────

bool HistoricalDataStore::export_csv(const QString& symbol, const QString& exchange,
                                     const QString& interval, const QString& file_path, qint64 from_ms,
                                     qint64 to_ms) const {
    const QVector<trading::BrokerCandle> candles = get_candles(symbol, exchange, interval, from_ms, to_ms);

    QFile f(file_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        LOG_ERROR("Historify", QString("export_csv: cannot open %1 — %2").arg(file_path, f.errorString()));
        return false;
    }
    QTextStream ts(&f);
    ts << "timestamp,open,high,low,close,volume,oi\n";
    for (const auto& c : candles) {
        ts << static_cast<qint64>(c.timestamp) << ',' << c.open << ',' << c.high << ',' << c.low << ','
           << c.close << ',' << c.volume << ',' << c.oi << '\n';
    }
    f.close();
    LOG_INFO("Historify", QString("Exported %1 rows to %2").arg(candles.size()).arg(file_path));
    return true;
}

bool HistoricalDataStore::export_parquet(const QString& symbol, const QString& exchange,
                                         const QString& interval, const QString& file_path,
                                         qint64 from_ms, qint64 to_ms) const {
    // TODO(parquet): no Arrow/Parquet library is vendored. Downgrade to CSV and
    // warn so the caller still gets data. Replace with a real Parquet writer
    // once Arrow/Parquet is added to the build.
    LOG_WARN("Historify", "export_parquet: no Parquet library available — writing CSV instead");
    return export_csv(symbol, exchange, interval, file_path, from_ms, to_ms);
}

// ── Auto-download hook ──────────────────────────────────────────────────────────

bool HistoricalDataStore::refresh_now(const QString& symbol, const QString& exchange,
                                      const QString& interval,
                                      const QVector<trading::BrokerCandle>& candles) {
    // Documented entry point for a future scheduler / broker-fetch service to
    // push fetched candles. Currently just persists them.
    return store_candles(symbol, exchange, interval, candles);
}

int HistoricalDataStore::refresh_watchlist() {
    const auto entries = watchlist();
    if (entries.isEmpty())
        return 0;

    // The store has no notion of "the" broker — auto-download uses whichever
    // active account is currently connected (first one wins). Resolve it on the
    // calling thread; the actual fetch runs on a worker.
    auto& am = trading::AccountManager::instance();
    QString account_id;
    trading::BrokerCredentials creds;
    for (const auto& acct : am.active_accounts()) {
        if (am.connection_state(acct.account_id) != trading::ConnectionState::Connected)
            continue;
        auto c = am.load_credentials(acct.account_id);
        if (c.api_key.isEmpty())
            continue;
        account_id = acct.account_id;
        creds = c;
        break;
    }
    if (account_id.isEmpty()) {
        LOG_INFO("Historify",
                 QString("refresh_watchlist: %1 entries pending, but no connected broker — skipping")
                     .arg(entries.size()));
        return 0;
    }

    // Fetch off the GUI thread — get_history() blocks on HTTP. Database::instance()
    // hands out a per-thread connection, so refresh_now()/store_candles() from the
    // worker is safe (see class header). Entries are fetched sequentially to stay
    // friendly to broker rate limits. The watchlist `interval` is passed through as
    // the broker resolution and `symbol` as-is — a watchlist UI should store
    // broker-compatible values (token brokers may need extra symbol resolution).
    const QVector<WatchEntry> work = entries;
    (void)QtConcurrent::run([work, account_id, creds]() {
        auto* broker = trading::AccountManager::instance().broker_for(account_id);
        if (!broker) {
            LOG_WARN("Historify", "refresh_watchlist: no broker for the connected account");
            return;
        }
        int ok = 0;
        for (const auto& e : work) {
            const QString to_dt = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
            QString from_dt;
            if (e.interval == "1d" || e.interval == "D" || e.interval == "1w" || e.interval == "W")
                from_dt = QDate::currentDate().addYears(-2).toString("yyyy-MM-dd") + " 00:00";
            else if (e.interval == "1h" || e.interval == "60")
                from_dt = QDate::currentDate().addDays(-30).toString("yyyy-MM-dd") + " 00:00";
            else
                from_dt = QDate::currentDate().addDays(-7).toString("yyyy-MM-dd") + " 00:00";

            auto resp = broker->get_history(creds, e.symbol, e.interval, from_dt, to_dt);
            if (resp.success && resp.data && !resp.data->isEmpty()) {
                if (HistoricalDataStore::instance().refresh_now(e.symbol, e.exchange, e.interval, *resp.data))
                    ++ok;
            } else {
                LOG_WARN("Historify", QString("refresh_watchlist: fetch failed for %1:%2 [%3] — %4")
                                          .arg(e.exchange, e.symbol, e.interval, resp.error));
            }
        }
        LOG_INFO("Historify", QString("refresh_watchlist: refreshed %1/%2 series").arg(ok).arg(work.size()));
    });

    return entries.size();
}

} // namespace fincept::storage
