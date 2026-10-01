// src/trading/HistoricalDataService.cpp
#include "trading/HistoricalDataService.h"

#include "trading/AccountManager.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"
#include "trading/instruments/InstrumentService.h"

#include <QDateTime>
#include <QPointer>
#include <QSet>
#include <QtConcurrent>

namespace fincept::trading {

namespace {
constexpr qint64 HDS_CACHE_TTL_MS = 60LL * 1000; // candles append slowly; 60s avoids double-fetch

// Map a user-facing timeframe to the broker resolution string. Brokers accept
// both this and the "1d"/"5m" forms (verified for Zerodha/Fyers/Upstox); the
// algo path has used this convention across every broker in production.
QString timeframe_to_resolution(const QString& tf) {
    if (tf == "1m")
        return QStringLiteral("1");
    if (tf == "3m")
        return QStringLiteral("3");
    if (tf == "5m")
        return QStringLiteral("5");
    if (tf == "10m") // only reached for brokers in hds_broker_has_native_10m()
        return QStringLiteral("10");
    if (tf == "15m")
        return QStringLiteral("15");
    if (tf == "30m")
        return QStringLiteral("30");
    if (tf == "1h")
        return QStringLiteral("60");
    if (tf == "4h")
        return QStringLiteral("240");
    if (tf == "1d")
        return QStringLiteral("D");
    return tf; // already a broker resolution ("60"/"D"/…) or unknown — pass through
}

// 10-minute bars: brokers do NOT agree on this resolution. These map "10" to a real
// 10-minute interval (checked in each broker's get_history). The others either
// reject it (Dhan, Tradier, ICICI, Tradejini, Motilal) or — worse — silently fall
// back to another resolution (Alpaca → daily bars, AliceBlue / Paytm → 1-minute
// bars), so for those the request is refused rather than answered with the wrong
// timeframe; callers build 10m from 5m instead.
bool hds_broker_has_native_10m(const QString& broker_id) {
    static const QSet<QString> s = {
        QStringLiteral("zerodha"),   QStringLiteral("angelone"), QStringLiteral("upstox"),
        QStringLiteral("fyers"),     QStringLiteral("groww"),    QStringLiteral("fivepaisa"),
        QStringLiteral("flattrade"), QStringLiteral("shoonya"),  QStringLiteral("samco"),
        QStringLiteral("saxobank"),
    };
    return s.contains(broker_id);
}

// ── Broker symbol resolution (moved here from CandleDataFetcher) ─────────────
// Most broker get_history() implementations need more than a bare symbol:
//   - token brokers want EXCHANGE:SYMBOL:TOKEN (numeric instrument_token);
//   - tradejini wants the scrip id at parts[1];
//   - the rest resolve EXCHANGE:SYMBOL internally.
// Unresolvable token brokers return "" so the caller skips/falls back.
const QSet<QString>& hds_brokers_needing_token() {
    static const QSet<QString> s = {
        QStringLiteral("motilal"), QStringLiteral("aliceblue"), QStringLiteral("fivepaisa"),
        QStringLiteral("iifl"),    QStringLiteral("ibkr"),      QStringLiteral("saxobank"),
    };
    return s;
}

// Worker-thread-safe synchronous instrument load (private named connection).
bool hds_ensure_instruments_loaded_blocking(const QString& broker_id) {
    auto& svc = InstrumentService::instance();
    if (svc.is_loaded(broker_id))
        return true;
    svc.load_from_db_worker(broker_id);
    return svc.is_loaded(broker_id);
}

QString hds_resolve_broker_symbol(const QString& broker_id, const QString& bare_symbol, const QString& exchange) {
    if (bare_symbol.contains(':')) // already fully qualified by caller
        return bare_symbol;
    const QString sym = bare_symbol.trimmed().toUpper();
    const QString exch = exchange.isEmpty() ? QStringLiteral("NSE") : exchange.toUpper();

    if (broker_id == QStringLiteral("tradejini")) { // get_history reads parts[1]
        if (hds_ensure_instruments_loaded_blocking(broker_id)) {
            auto inst = InstrumentService::instance().find(sym, exch, broker_id);
            if (inst) {
                const QString symid =
                    inst->brsymbol.isEmpty() ? QString::number(inst->instrument_token) : inst->brsymbol;
                return exch + ":" + symid;
            }
        }
        return exch + ":" + sym; // best-effort
    }
    if (hds_brokers_needing_token().contains(broker_id)) { // EXCHANGE:SYMBOL:TOKEN
        if (!hds_ensure_instruments_loaded_blocking(broker_id))
            return {};
        auto tok = InstrumentService::instance().instrument_token(sym, exch, broker_id);
        if (!tok.has_value() || tok.value() <= 0)
            return {};
        return exch + ":" + sym + ":" + QString::number(tok.value());
    }
    // Brokers whose get_history resolves the symbol internally — warm the cache so
    // the lookup succeeds, but pass the bare symbol (they default exchange to NSE).
    static const QSet<QString> kInternalResolvers = {
        QStringLiteral("zerodha"),
        QStringLiteral("angelone"),
        QStringLiteral("dhan"),
        QStringLiteral("upstox"),
    };
    if (kInternalResolvers.contains(broker_id))
        hds_ensure_instruments_loaded_blocking(broker_id);
    return bare_symbol; // everyone else accepts the bare symbol as-is
}

} // namespace

HistoricalDataService& HistoricalDataService::instance() {
    static HistoricalDataService s;
    return s;
}

void HistoricalDataService::fetch(const QString& symbol, const QString& timeframe, int lookback_days,
                                  const QString& broker_id, const QString& account_id, Callback callback) {
    const QString key = broker_id + "|" + symbol + "|" + timeframe + "|" + QString::number(lookback_days);

    if (timeframe == QLatin1String("10m") && !hds_broker_has_native_10m(broker_id)) {
        callback(false, {}, QStringLiteral("%1 has no native 10-minute bars — build them from 5m").arg(broker_id));
        return;
    }

    // Serve from cache when fresh (main thread).
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (auto it = cache_.constFind(key); it != cache_.constEnd()) {
        if (now - it->fetched_ms < HDS_CACHE_TTL_MS && !it->candles.isEmpty()) {
            callback(true, it->candles, {});
            return;
        }
    }

    // Load credentials on the calling (main) thread: QSqlDatabase is bound to the
    // thread that opened it, so SecureStorage::retrieve must not run on a worker.
    auto creds = AccountManager::instance().load_credentials(account_id);
    if (creds.api_key.isEmpty()) {
        callback(false, {}, QStringLiteral("No credentials"));
        return;
    }

    QPointer<HistoricalDataService> self = this;
    (void)QtConcurrent::run([self, key, symbol, timeframe, lookback_days, broker_id, creds, callback]() {
        auto* broker = BrokerRegistry::instance().get(broker_id);
        if (!broker) {
            QMetaObject::invokeMethod(
                self ? self.data() : nullptr, [callback]() { callback(false, {}, QStringLiteral("Broker not found")); },
                Qt::QueuedConnection);
            return;
        }

        const QDateTime to = QDateTime::currentDateTime();
        const QDateTime from = to.addDays(-lookback_days);
        const QString resolution = timeframe_to_resolution(timeframe);
        const QString from_s = from.toString("yyyy-MM-dd");
        const QString to_s = to.toString("yyyy-MM-dd");
        const QString resolved = hds_resolve_broker_symbol(broker_id, symbol, QStringLiteral("NSE"));

        auto has_data = [](const ApiResponse<QVector<BrokerCandle>>& r) {
            return r.success && r.data.value_or(QVector<BrokerCandle>{}).size() > 0;
        };

        ApiResponse<QVector<BrokerCandle>> result;
        QString last_err;
        if (!resolved.isEmpty()) {
            result = broker->get_history(creds, resolved, resolution, from_s, to_s);
            if (!result.success)
                last_err = result.error;
        } else {
            last_err = QStringLiteral("symbol resolution failed (no instrument token)");
        }
        // Retry with the bare symbol if the resolved form yielded nothing.
        if (!has_data(result) && resolved != symbol) {
            auto r2 = broker->get_history(creds, symbol, resolution, from_s, to_s);
            if (has_data(r2))
                result = r2;
            else if (last_err.isEmpty())
                last_err = r2.error;
        }

        const bool ok = has_data(result);
        const QVector<BrokerCandle> candles = result.data.value_or(QVector<BrokerCandle>{});
        const QString err = ok ? QString() : (last_err.isEmpty() ? QStringLiteral("No data") : last_err);

        QMetaObject::invokeMethod(
            self ? self.data() : nullptr,
            [self, key, ok, candles, err, callback]() {
                if (self && ok) {
                    const qint64 stamp = QDateTime::currentMSecsSinceEpoch();
                    // The TTL only gated REUSE — entries were never evicted, so every
                    // distinct broker|symbol|timeframe|lookback ever charted stayed
                    // resident with its whole candle series for the life of the
                    // process. Drop expired entries on each insert.
                    for (auto it = self->cache_.begin(); it != self->cache_.end();) {
                        if (stamp - it->fetched_ms >= HDS_CACHE_TTL_MS)
                            it = self->cache_.erase(it);
                        else
                            ++it;
                    }
                    self->cache_.insert(key, CacheEntry{stamp, candles});
                }
                callback(ok, ok ? candles : QVector<BrokerCandle>{}, err);
            },
            Qt::QueuedConnection);
    });
}

} // namespace fincept::trading
