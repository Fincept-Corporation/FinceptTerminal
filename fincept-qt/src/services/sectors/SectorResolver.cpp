#include "services/sectors/SectorResolver.h"

#include "core/logging/Logger.h"
#include "services/markets/MarketDataService.h"
#include "storage/sqlite/Database.h"

#include <QDateTime>
#include <QPointer>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace fincept::services {

namespace {

constexpr qint64 kNegativeTtlSeconds = 7 * 24 * 3600; // 7 days

// quote_type marker written for rows whose sector is a fallback placeholder
// rather than a real upstream answer. Distinguishes them from a user override
// (which uses "override") and from legacy rows (empty).
const QString kUnresolvedMarker = QStringLiteral("unresolved");
const QString kOverrideMarker = QStringLiteral("override");

// Symbols that look like TSX bare tickers (letters only, 1–4 chars, no dot).
// We try a `.TO` suffix first before giving up, since many Canadian holdings
// in imported portfolios arrive without the exchange suffix.
bool looks_like_canadian_bare(const QString& symbol) {
    if (symbol.contains('.') || symbol.contains('-') || symbol.contains('='))
        return false;
    if (symbol.length() < 1 || symbol.length() > 5)
        return false;
    for (auto ch : symbol) {
        if (!ch.isLetter())
            return false;
    }
    return true;
}

} // namespace

SectorResolver& SectorResolver::instance() {
    static SectorResolver s;
    return s;
}

SectorResolver::SectorResolver() {
    load_cache();
}

QString SectorResolver::normalize(const QString& symbol) {
    return symbol.trimmed().toUpper();
}

QString SectorResolver::fallback_for_unresolvable(const QString& symbol) {
    // Heuristic: symbols ending in letters with no exchange suffix and length
    // >= 5 that failed yfinance are almost always Canadian mutual fund codes
    // like TDB8150. Treat those as Cash/Fund.
    if (symbol.length() >= 6 && symbol.startsWith("TDB"))
        return "Cash";
    return "Unclassified";
}

void SectorResolver::load_cache() {
    auto& db_mgr = fincept::Database::instance();
    if (!db_mgr.is_open())
        return;

    // Route through Database::execute() — it hands back the CALLING thread's
    // connection. raw_db() is the main-thread connection object, and this runs
    // from the singleton ctor, i.e. on whichever thread touches the resolver
    // first (see the rule documented in storage/secure/SecureStorage.cpp).
    auto r = db_mgr.execute("SELECT symbol, sector, quote_type, resolved_at FROM sector_cache");
    if (r.is_err()) {
        LOG_WARN("SectorResolver", "Cache load failed: " + QString::fromStdString(r.error()));
        return;
    }
    auto& q = r.value();

    QMutexLocker lock(&mutex_);
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    int loaded = 0;
    int negatives = 0;
    while (q.next()) {
        const QString sym = q.value(0).toString();
        const QString sec = q.value(1).toString();
        const QString quote_type = q.value(2).toString();
        const qint64 resolved = q.value(3).toLongLong();
        if (sym.isEmpty())
            continue;

        // A row is "negative" when its sector is a placeholder rather than an
        // upstream answer. New rows say so explicitly via quote_type; legacy
        // rows are detected by comparing against what the fallback would have
        // produced for that symbol — this is what catches the old "Cash"
        // entries written for TDB* codes, which the string-equality check
        // against "Unclassified" missed and therefore never expired.
        const bool negative =
            quote_type != kOverrideMarker && (quote_type == kUnresolvedMarker || sec.isEmpty() ||
                                              sec == fallback_for_unresolvable(sym) || sec == "Unclassified");

        if (negative && (now - resolved) > kNegativeTtlSeconds)
            continue; // expired placeholder — drop it so the next access retries
        if (sec.isEmpty())
            continue;

        cache_.insert(sym, Entry{sec, resolved, negative});
        ++loaded;
        if (negative)
            ++negatives;
    }
    LOG_INFO("SectorResolver",
             QString("Loaded %1 sector cache entries (%2 unresolved placeholders)").arg(loaded).arg(negatives));
}

void SectorResolver::persist(const QString& symbol, const QString& sector, const QString& industry,
                             const QString& quote_type, qint64 resolved_at) {
    auto& db_mgr = fincept::Database::instance();
    if (!db_mgr.is_open())
        return;

    auto r = db_mgr.execute("INSERT INTO sector_cache (symbol, sector, industry, quote_type, resolved_at) "
                            "VALUES (?, ?, ?, ?, ?) "
                            "ON CONFLICT(symbol) DO UPDATE SET "
                            "sector=excluded.sector, industry=excluded.industry, "
                            "quote_type=excluded.quote_type, resolved_at=excluded.resolved_at",
                            {symbol, sector, industry, quote_type, resolved_at});
    if (r.is_err())
        LOG_WARN("SectorResolver", "Cache write failed: " + QString::fromStdString(r.error()));
}

void SectorResolver::remember(const QString& symbol, const QString& sector) {
    QString sym = normalize(symbol);
    QString sec = sector.trimmed();
    if (sym.isEmpty() || sec.isEmpty())
        return;

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    {
        QMutexLocker lock(&mutex_);
        cache_.insert(sym, Entry{sec, now, /*negative=*/false});
    }
    persist(sym, sec, {}, kOverrideMarker, now);
    emit sector_resolved(sym, sec);
}

QString SectorResolver::sector_for(const QString& symbol) {
    QString sym = normalize(symbol);
    if (sym.isEmpty())
        return {};

    QMutexLocker lock(&mutex_);
    auto it = cache_.find(sym);
    if (it != cache_.end()) {
        if (!it->negative)
            return it->sector;
        // Placeholder: honour the negative TTL HERE, not only at load. A session
        // that outlives the TTL used to serve the stale placeholder forever.
        if ((QDateTime::currentSecsSinceEpoch() - it->resolved_at) <= kNegativeTtlSeconds)
            return it->sector;
        cache_.erase(it); // expired — fall through and retry
    }

    if (!inflight_.contains(sym)) {
        inflight_.insert(sym);
        lock.unlock();
        resolve_async(sym);
    }
    return {};
}

void SectorResolver::prefetch(const QStringList& symbols) {
    for (const auto& s : symbols)
        (void)sector_for(s);
}

void SectorResolver::resolve_async(const QString& symbol) {
    QPointer<SectorResolver> self = this;

    // Reached ONLY when the upstream lookup actually completed. An empty sector
    // here is a real answer ("this instrument has no sector") and is cached as a
    // TTL'd placeholder. A fetch FAILURE never reaches this lambda — see give_up.
    auto finish = [self, symbol](QString sector, QString industry, QString quote_type) {
        if (!self)
            return;
        QString final_sector = sector.trimmed();
        const bool negative = final_sector.isEmpty() || final_sector == "N/A";
        if (negative)
            final_sector = SectorResolver::fallback_for_unresolvable(symbol);

        const qint64 now = QDateTime::currentSecsSinceEpoch();
        {
            QMutexLocker lock(&self->mutex_);
            self->cache_.insert(symbol, Entry{final_sector, now, negative});
            self->inflight_.remove(symbol);
        }
        self->persist(symbol, final_sector, industry, negative ? kUnresolvedMarker : quote_type, now);
        emit self->sector_resolved(symbol, final_sector);
    };

    // The fetch FAILED (script crash, network error, rate limit). Release the
    // in-flight slot and touch NOTHING else: caching a transport failure as a
    // sector persists a wrong answer that outlives the outage.
    auto give_up = [self, symbol](const char* reason) {
        if (!self)
            return;
        {
            QMutexLocker lock(&self->mutex_);
            self->inflight_.remove(symbol);
        }
        LOG_DEBUG("SectorResolver", QString("Lookup failed for %1 (%2) — not cached, will retry")
                                        .arg(symbol, QString::fromLatin1(reason)));
    };

    // Try 1: as-is.
    MarketDataService::instance().fetch_info(symbol, [self, symbol, finish, give_up](bool ok, InfoData info) {
        if (!self)
            return;
        const QString sector = info.sector.trimmed();
        if (ok && !sector.isEmpty() && sector != "N/A") {
            finish(sector, info.industry, {});
            return;
        }

        // Try 2: TSX suffix fallback for bare Canadian tickers. Worth doing even
        // when try 1 errored — a bare TSX ticker often 404s before it resolves.
        if (looks_like_canadian_bare(symbol)) {
            const QString tsx_symbol = symbol + ".TO";
            // `finish` / `give_up` already carry `symbol`, so it is not captured again.
            MarketDataService::instance().fetch_info(tsx_symbol, [self, finish, give_up](bool ok2, InfoData info2) {
                if (!self)
                    return;
                if (!ok2) {
                    give_up("both bare and .TO lookups failed");
                    return;
                }
                finish(info2.sector, info2.industry, {});
            });
            return;
        }

        if (!ok) {
            give_up("fetch_info reported failure");
            return;
        }
        // Fetch succeeded but the instrument genuinely carries no sector.
        finish({}, info.industry, {});
    });
}

} // namespace fincept::services
