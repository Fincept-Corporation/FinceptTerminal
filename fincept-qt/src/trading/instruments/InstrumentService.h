#pragma once
#include "trading/TradingTypes.h"
#include "trading/instruments/InstrumentTypes.h"

#include <QMap>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include <optional>

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
/// back to the UI thread via QMetaObject::invokeMethod. All cache reads are
/// guarded by a QReadWriteLock (reads concurrent, writes exclusive on refresh).
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

    // Cache key: "broker_id:exchange:symbol" or "broker_id:token"
    // Stored as flat QMap for O(1) lookups.
    struct Cache {
        // (symbol, exchange) → Instrument
        QMap<QPair<QString, QString>, Instrument> by_symbol;
        // instrument_token → Instrument
        QMap<qint64, Instrument> by_token;
        // brsymbol+brexchange → Instrument
        QMap<QPair<QString, QString>, Instrument> by_brsymbol;
        bool loaded = false;
    };

    QMap<QString, Cache> caches_; // keyed by broker_id
    QSet<QString> refreshing_;    // brokers currently mid-download (prevents double-refresh)
    mutable QMutex mutex_;        // guards caches_ + refreshing_

    void build_cache(const QString& broker_id, const QVector<Instrument>& instruments);
    void do_refresh(const QString& broker_id, const BrokerCredentials& creds);
    static QByteArray download_zerodha_csv(const BrokerCredentials& creds);
    static QByteArray download_angel_master_json();
};

} // namespace fincept::trading
