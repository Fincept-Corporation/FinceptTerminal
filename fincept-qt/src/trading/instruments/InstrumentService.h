#pragma once
#include "trading/TradingTypes.h"
#include "trading/instruments/InstrumentTypes.h"

#include <QMap>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace fincept::trading {

/// InstrumentService — downloads, parses, caches, and looks up broker instruments.
///
/// Usage:
///   // On broker connect (async, non-blocking):
///   InstrumentService::instance().refresh("zerodha", creds);
///   InstrumentService::instance().refresh("angelone", creds);
///
///   // Anywhere else (instant, in-memory):
///   auto token = InstrumentService::instance().instrument_token("RELIANCE", "NSE", "zerodha");
///   auto sym   = InstrumentService::instance().to_brsymbol("NIFTY28MAR24FUT", "NFO", "zerodha");
///
/// Thread safety: refresh() runs on a QtConcurrent worker thread and posts results
/// back to the UI thread via QMetaObject::invokeMethod. Each broker's cache is an
/// immutable snapshot (maps + search index) built OFF the UI thread and swapped
/// in under mutex_ in O(1); mutex_ guards only the broker → snapshot map and
/// refreshing_. Lookups and searches copy the snapshot pointer and read it with
/// no lock held, so a 100k-row rebuild never blocks a lookup.
class InstrumentService : public QObject {
    Q_OBJECT
  public:
    static InstrumentService& instance();

    // ── Refresh (async) ───────────────────────────────────────────────────────

    /// Download instruments from broker API, parse, store to DB, rebuild cache.
    /// Non-blocking — emits refresh_done or refresh_failed on completion.
    /// Skips download if instruments are already fresh (< max_age_hours old).
    void refresh(const QString& broker_id, const BrokerCredentials& creds, int max_age_hours = 12);

    /// Force refresh even if cache is fresh.
    void force_refresh(const QString& broker_id, const BrokerCredentials& creds);

    /// Load instruments from DB into memory (no network call).
    /// Call this on startup after DB opens.
    void load_from_db(const QString& broker_id);

    /// Load instruments from DB on a worker thread. Non-blocking.
    /// On completion, invokes callback(count) on the UI thread.
    /// If broker already loaded, callback fires immediately with cached count.
    void load_from_db_async(const QString& broker_id, std::function<void(int)> callback = nullptr);

    /// Synchronous DB load safe to call from a QtConcurrent worker thread.
    /// Opens a private named QSqlDatabase connection (does NOT touch the shared
    /// main-thread connection). Blocks until the cache is built. No-op if loaded.
    void load_from_db_worker(const QString& broker_id);

    // ── Lookups (synchronous, in-memory) ─────────────────────────────────────

    /// symbol + exchange → numeric instrument_token  (for get_history)
    std::optional<qint64> instrument_token(const QString& symbol, const QString& exchange,
                                           const QString& broker_id) const;

    /// symbol + exchange → brsymbol  (for order placement)
    std::optional<QString> to_brsymbol(const QString& symbol, const QString& exchange, const QString& broker_id) const;

    /// brsymbol + brexchange → normalised symbol  (for parsing order responses)
    std::optional<QString> from_brsymbol(const QString& brsymbol, const QString& brexchange,
                                         const QString& broker_id) const;

    /// Full instrument lookup by symbol + exchange
    std::optional<Instrument> find(const QString& symbol, const QString& exchange, const QString& broker_id) const;

    /// Full instrument lookup by numeric instrument_token (for WebSocket tick enrichment)
    std::optional<Instrument> find_by_token(quint32 instrument_token, const QString& broker_id) const;

    // ── Search (for symbol picker UI) ────────────────────────────────────────
    QVector<Instrument> search(const QString& query, const QString& exchange, const QString& broker_id,
                               int limit = 50) const;

    /// Unified cross-broker search. Returns one row per (broker, instrument)
    /// across all `broker_ids` (empty = every broker in the catalog), with the
    /// first broker in the list sorted first. Used by the watchlist when more
    /// than one broker is connected. Each row carries its own broker_id +
    /// brsymbol + token, so selection routes to that broker.
    QVector<Instrument> search_all(const QString& query, const QString& exchange, const QStringList& broker_ids,
                                   int limit = 50) const;

    // ── F&O / Options chain helpers ──────────────────────────────────────────
    //
    // All three read the in-memory cache; they expect refresh()/load_from_db()
    // to have populated NFO instruments first. Empty results when cache is
    // not loaded — callers should check is_loaded(broker_id).

    /// All option-bearing underlyings on a given exchange (default NFO).
    /// Distinct `name` field across CE/PE/FUT entries — typically yields
    /// {"NIFTY", "BANKNIFTY", "FINNIFTY", "MIDCPNIFTY", ...stock symbols...}.
    QStringList list_underlyings(const QString& broker_id, const QString& exchange = "NFO") const;

    /// Distinct expiries (display format "DD-MMM-YY") for a given underlying,
    /// sorted ascending by date.
    QStringList list_expiries(const QString& broker_id, const QString& underlying,
                              const QString& exchange = "NFO") const;

    /// All CE+PE+FUT instruments matching (underlying, expiry). Strikes
    /// returned in ascending order. FUT entries (strike==0) are included
    /// last; chain assembly callers should filter by `instrument_type`.
    QVector<Instrument> find_options_for_underlying(const QString& broker_id, const QString& underlying,
                                                    const QString& expiry, const QString& exchange = "NFO") const;

    /// How many instruments are cached for this broker.
    int cached_count(const QString& broker_id) const;

    /// Whether instruments are loaded for this broker.
    bool is_loaded(const QString& broker_id) const;

  signals:
    void refresh_started(const QString& broker_id);
    void refresh_done(const QString& broker_id, int count);
    void refresh_failed(const QString& broker_id, const QString& error);

  private:
    explicit InstrumentService(QObject* parent = nullptr) : QObject(parent) {}

    // One broker's instruments. Immutable once built (shared as
    // shared_ptr<const Cache>), never copied: the search index holds pointers
    // into by_token's nodes.
    struct Cache {
        Cache() = default;
        Cache(const Cache&) = delete;
        Cache& operator=(const Cache&) = delete;

        // (symbol, exchange) → Instrument
        QMap<QPair<QString, QString>, Instrument> by_symbol;
        // instrument_token → Instrument
        QMap<qint64, Instrument> by_token;
        // brsymbol+brexchange → Instrument
        QMap<QPair<QString, QString>, Instrument> by_brsymbol;

        // ── Search index over by_token's values (see search_all) ──
        // by_token iteration order; position = item index.
        std::vector<const Instrument*> search_items;
        // Per result type rank (EQ, INDEX, FUT, other): item indices sorted by
        // (symbol, index) — search_all's result order within a rank, so a symbol
        // prefix is one binary-searched range.
        std::array<std::vector<int>, 4> search_buckets;
        // Items whose symbol is not plain upper-case ASCII. A case-insensitive
        // prefix match can sit outside the binary-searched range for these, so
        // they are checked one by one (normally none — parsers upper-case symbols).
        std::array<std::vector<int>, 4> search_irregular;
        // Per item: bit set of case-folded ASCII characters present in symbol,
        // brsymbol and name — rejects most items before any substring scan.
        std::vector<quint64> search_masks;
    };
    using CachePtr = std::shared_ptr<const Cache>;

    QMap<QString, CachePtr> caches_; // keyed by broker_id; present = loaded
    QSet<QString> refreshing_;       // brokers currently mid-download (prevents double-refresh)
    mutable QMutex mutex_;           // guards caches_ (the pointers) + refreshing_

    /// Build a broker snapshot (maps + search index). Pure — call it OFF the UI
    /// thread; ~100k+ rows take a noticeable fraction of a second.
    static CachePtr build_cache_snapshot(const QVector<Instrument>& instruments);
    /// Swap a prebuilt snapshot in. O(1) under mutex_.
    void install_cache(const QString& broker_id, CachePtr cache);
    /// The broker's current snapshot, or null when it is not loaded.
    CachePtr cache_for(const QString& broker_id) const;
    /// build_cache_snapshot + install_cache on the calling thread.
    void build_cache(const QString& broker_id, const QVector<Instrument>& instruments);
    /// Append up to `k` of this snapshot's search hits, in search_all's order.
    static void search_cache(const Cache& cache, const QString& q, const QString& exch, int k,
                             QVector<Instrument>& out);
    void do_refresh(const QString& broker_id, const BrokerCredentials& creds);

  public:
    // Built-in source downloaders — public so SymbolResolver registration
    // (in the .cpp file) can wire them as `InstrumentSource::download` lambdas.
    // They have no instance state; they are static helpers.
    static QByteArray download_zerodha_csv(const BrokerCredentials& creds);
    static QByteArray download_angel_master_json();
    static QByteArray download_groww_csv();
    static QByteArray download_fyers_json();
    static QByteArray download_dhan_csv();
    static QByteArray download_icici_csv();
};

} // namespace fincept::trading
