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

    QSqlQuery q(db_mgr.raw_db());
    if (!q.exec("SELECT symbol, sector, resolved_at FROM sector_cache")) {
        LOG_WARN("SectorResolver", "Cache load failed: " + q.lastError().text());
        return;
    }

    QMutexLocker lock(&mutex_);
    qint64 now = QDateTime::currentSecsSinceEpoch();
    int loaded = 0;
    while (q.next()) {
        QString sym = q.value(0).toString();
        QString sec = q.value(1).toString();
        qint64 resolved = q.value(2).toLongLong();

        // Drop stale negative cache entries (sector == Unclassified/empty and
        // older than TTL) so we retry on next access.
        bool is_negative = sec.isEmpty() || sec == "Unclassified";
        if (is_negative && (now - resolved) > kNegativeTtlSeconds)
            continue;

        if (!sec.isEmpty()) {
            cache_.insert(sym, sec);
            ++loaded;
        }
    }
    LOG_INFO("SectorResolver", QString("Loaded %1 sector cache entries").arg(loaded));
}

void SectorResolver::persist(const QString& symbol, const QString& sector, const QString& industry,
                             const QString& quote_type) {
    auto& db_mgr = fincept::Database::instance();
    if (!db_mgr.is_open())
        return;

    QSqlQuery q(db_mgr.raw_db());
    q.prepare("INSERT INTO sector_cache (symbol, sector, industry, quote_type, resolved_at) "
              "VALUES (?, ?, ?, ?, ?) "
              "ON CONFLICT(symbol) DO UPDATE SET "
              "sector=excluded.sector, industry=excluded.industry, "
              "quote_type=excluded.quote_type, resolved_at=excluded.resolved_at");
    q.addBindValue(symbol);
    q.addBindValue(sector);
    q.addBindValue(industry);
    q.addBindValue(quote_type);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    if (!q.exec())
        LOG_WARN("SectorResolver", "Cache write failed: " + q.lastError().text());
}

void SectorResolver::remember(const QString& symbol, const QString& sector) {
    QString sym = normalize(symbol);
    QString sec = sector.trimmed();
    if (sym.isEmpty() || sec.isEmpty())
        return;

    {
        QMutexLocker lock(&mutex_);
        cache_.insert(sym, sec);
    }
    persist(sym, sec, {}, "override");
    emit sector_resolved(sym, sec);
}

QString SectorResolver::sector_for(const QString& symbol) {
    QString sym = normalize(symbol);
    if (sym.isEmpty())
        return {};

    QMutexLocker lock(&mutex_);
    auto it = cache_.constFind(sym);
    if (it != cache_.constEnd())
        return it.value();

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

    auto finish = [self, symbol](QString sector, QString industry, QString quote_type) {
        if (!self)
            return;
        QString final_sector = sector.trimmed();
        if (final_sector.isEmpty() || final_sector == "N/A") {
            // yfinance didn't resolve — use a graceful fallback.
            final_sector = SectorResolver::fallback_for_unresolvable(symbol);
        }
        {
            QMutexLocker lock(&self->mutex_);
            self->cache_.insert(symbol, final_sector);
            self->inflight_.remove(symbol);
        }
        self->persist(symbol, final_sector, industry, quote_type);
        emit self->sector_resolved(symbol, final_sector);
    };

    // Try 1: as-is.
    MarketDataService::instance().fetch_info(symbol, [self, symbol, finish](bool ok, InfoData info) {
        if (!self)
            return;
        QString sector = info.sector.trimmed();
        bool resolved = ok && !sector.isEmpty() && sector != "N/A";
        if (resolved) {
            finish(sector, info.industry, {});
            return;
        }

        // Try 2: TSX suffix fallback for bare Canadian tickers.
        if (looks_like_canadian_bare(symbol)) {
            QString tsx_symbol = symbol + ".TO";
            MarketDataService::instance().fetch_info(
                tsx_symbol, [self, symbol, finish](bool ok2, InfoData info2) {
                    if (!self)
                        return;
                    finish(ok2 ? info2.sector : QString{}, info2.industry, {});
                });
            return;
        }
        finish({}, info.industry, {});
    });
}

} // namespace fincept::services
