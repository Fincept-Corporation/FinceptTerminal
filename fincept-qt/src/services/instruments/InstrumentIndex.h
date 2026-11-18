#pragma once
// InstrumentIndex — the local instrument index of the symbol system (plan E7 / WP2):
// asynchronous search and lookups over SymbolMasterRepository (migration v060), the
// bundled seed, learning hooks, the broker-master import and the backend-master sync
// interface. No widgets. The resolver (WP3, services/instruments/InstrumentResolver)
// is its main client; screens should go through the resolver / picker, not here.
//
// Threads
//   Nothing here blocks the calling thread. Reads run on a reader pool (2 threads),
//   writes (seed, learned batches, broker imports, master deltas) on a single writer
//   thread, each with its own Database::connection() clone; with WAL, searches never
//   wait for an import. Results are delivered on the thread of the `context` object the
//   caller passes (usually the widget / service asking), or on this object's thread
//   (the GUI thread) when `context` is null. A destroyed context drops the delivery.
//   `context` should live on the GUI thread or outlive the request.
//   Every call is safe from any thread. The repository refuses (logs + debug assert)
//   to run on the GUI thread.
//
// Stale requests
//   Every query returns a request id. cancel(id) drops it; a request with a non-empty
//   `channel` is superseded by the next request on the same channel (type-ahead: pass
//   the widget's own channel name) — superseded work is skipped before it reaches the
//   database when it is still queued, and its result is never delivered.
//
// Start-up
//   start() is idempotent and called lazily by every entry point; it waits for
//   Database to be open (retries every 2 s), then on the writer thread applies the
//   bundled seed once per kSymbolSeedVersion and imports the cash-market rows of every
//   broker master in the trading `instruments` table whose MAX(updated_at) changed
//   since its last import (re-checked every 30 minutes). A database re-opened on a
//   different file (profile switch) is seeded on the next call.
//
// Learning (write-through after any successful resolution elsewhere)
//   learn(inst) / learn_from_quote(...) merge the record with Source::Learned
//   (never overrides seed / master data, only fills it in; see SymbolMasterRepository).
//   Calls are coalesced for 1.5 s and written as one batch; repeats of an identical
//   record are dropped. Call ONLY with a symbol that resolved / quoted successfully.
//
// Backend master (WP8)
//   apply_master_delta(json) applies one page of GET /symbols/changes (JSON shape in
//   SymbolMasterRepository.h) in one transaction and stores its cursor;
//   read_sync_cursor() returns the stored cursor for the next request.
//
// Errors reach callers as plain language (db::user_message, "The symbol index ...")
// in the result's `error` and through index_error(); driver text goes to the log.

#include "core/instrument/Instrument.h"
#include "core/instrument/InstrumentRank.h"
#include "core/instrument/SymbolInput.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QVector>

#include <atomic>
#include <functional>
#include <string>

class QTimer;

namespace fincept::services {

class InstrumentIndex : public QObject {
    Q_OBJECT
  public:
    using Instrument = core::instrument::Instrument;
    using RankedInstrument = core::instrument::RankedInstrument;
    using RankContext = core::instrument::RankContext;
    using ParsedInput = core::instrument::ParsedInput;
    using AssetClass = core::instrument::AssetClass;

    /// Create on (or move to) the application's main thread. Any thread may call it after.
    static InstrumentIndex& instance();

    /// Idempotent: seeds / imports once the database is open (see the header comment).
    void start();
    /// Stop taking work and wait for the worker pools. Runs when the application object is
    /// destroyed (a post routine), i.e. before static objects the workers use are torn down.
    void quiesce();

    // ── Search ──────────────────────────────────────────────────────────────

    struct SearchRequest {
        QString text;          ///< typed text; parsed on the worker with venue_hint and ctx.home_country
        ParsedInput parsed;    ///< used instead of `text` when `text` is empty and parsed.kind != Empty
        QString venue_hint;    ///< exchange hint for the parser ("NSE", "XETRA", ...)
        RankContext ctx;       ///< home country, class priority / filter, derivatives, inactive rows
        int limit = 12;
        QString channel;       ///< non-empty: superseded by the next request on the same channel
    };
    struct SearchResult {
        quint64 request_id = 0;
        QVector<RankedInstrument> hits; ///< ranked by core::instrument::rank_instruments
        QString error;                  ///< plain language; empty = ok
    };
    using SearchCallback = std::function<void(const SearchResult&)>;

    /// Search the index; `done` runs on `context`'s thread (see the header comment).
    quint64 search(const SearchRequest& req, QObject* context = nullptr, SearchCallback done = {});

    // ── Lookups ─────────────────────────────────────────────────────────────

    enum class LookupKind : quint8 {
        Id,             ///< key = instrument id
        Symbol,         ///< key = canonical symbol (canonical_symbol applied)
        ListingCode,    ///< key = listing code on any venue ("RELIANCE", "700")
        Alias,          ///< key = any alias / former symbol / name text
        Isin,           ///< key = ISIN
        Prefix,         ///< key = code / alias / name prefix
        NameContains,   ///< key = part of the name (3+ characters)
        ProviderSymbol, ///< key = the provider's symbol; provider + exchange below
        Current,        ///< key = id or symbol: [record] or [renamed record, current successor]
        Predecessors,   ///< key = id: the records renamed into it
    };
    struct LookupRequest {
        LookupKind kind = LookupKind::Current;
        QString key;
        QString provider;              ///< ProviderSymbol: "broker:zerodha", "binance", "tv", ...
        QString exchange;              ///< ProviderSymbol: broker exchange code ("NSE"); empty = any
        QVector<AssetClass> classes;   ///< Prefix / NameContains: allowed classes; empty = any
        int limit = 50;
        QString channel;
    };
    struct LookupResult {
        quint64 request_id = 0;
        QVector<Instrument> found;
        QString error; ///< plain language; empty = ok
    };
    using LookupCallback = std::function<void(const LookupResult&)>;

    quint64 lookup(const LookupRequest& req, QObject* context = nullptr, LookupCallback done = {});

    /// Drop a pending search / lookup: its result is never delivered.
    void cancel(quint64 request_id);

    // ── Learning ────────────────────────────────────────────────────────────

    /// Upsert a record the app resolved elsewhere (Source::Learned). Missing id / display /
    /// class / venue / country / currency are filled from the canonical symbol's skeleton.
    /// Records whose symbol yields no instrument id (ISINs, names, junk) are ignored.
    void learn(const Instrument& inst);
    /// learn() from a successful quote: Yahoo symbol, name (longName / shortName), Yahoo
    /// exchange code (quote.exchange: "NMS", "NSI", "LSE"), quoteType, currency as quoted.
    void learn_from_quote(const QString& symbol, const QString& name, const QString& exchange,
                          const QString& quote_type, const QString& currency);

    // ── Broker masters ──────────────────────────────────────────────────────

    /// Re-import one broker's cash-market rows (EQ / INDEX) now, even when unchanged.
    void import_broker_instruments(const QString& broker_id);
    /// Import every broker master whose MAX(updated_at) changed since its last import.
    void import_all_broker_instruments();

    // ── Backend master sync (WP8) ───────────────────────────────────────────

    /// ok: `cursor_or_error` is the stored cursor; else the plain-language error.
    using DeltaCallback = std::function<void(bool ok, const QString& cursor_or_error, bool has_more)>;
    void apply_master_delta(const QJsonObject& delta, QObject* context = nullptr, DeltaCallback done = {});
    /// The stored cursor ("" before the first delta).
    using CursorCallback = std::function<void(const QString& cursor)>;
    void read_sync_cursor(QObject* context, CursorCallback done);

  signals:
    /// Every search / lookup result that is delivered (after its callback), on the
    /// delivering thread.
    void search_finished(const fincept::services::InstrumentIndex::SearchResult& result);
    void lookup_finished(const fincept::services::InstrumentIndex::LookupResult& result);
    /// Records were added or changed. `ids` lists them when there are at most 200,
    /// else it is empty (bulk change: re-query).
    void index_changed(int changed, const QStringList& ids);
    void seed_applied(int changed, const QString& version);
    void broker_import_finished(const QString& broker_id, int changed);
    void master_delta_applied(const QString& cursor, int changed, bool has_more);
    /// Plain-language error (the log has the driver text).
    void index_error(const QString& message);

  private:
    InstrumentIndex();
    ~InstrumentIndex() override;
    InstrumentIndex(const InstrumentIndex&) = delete;
    InstrumentIndex& operator=(const InstrumentIndex&) = delete;

    void ensure_started();
    void run_read(std::function<void(QSqlDatabase&)> task);
    void run_write(std::function<void(QSqlDatabase&)> task);

    quint64 register_request(const QString& channel);
    bool is_current(quint64 id);
    bool take_if_current(quint64 id); ///< and forget it
    void forget(quint64 id);

    /// Log `raw`, emit index_error, and return the plain-language message.
    QString report_error(const std::string& raw, const char* what);
    void post_changed(int changed, const QStringList& ids);

    void seed_task(QSqlDatabase& db);
    void broker_task(QSqlDatabase& db, const QString& only_broker, bool force);
    int import_broker_rows(QSqlDatabase& db, const QString& broker_id, QString* error);
    void flush_learned();

    QThreadPool reader_pool_;
    QThreadPool writer_pool_;
    std::atomic<bool> shutting_down_{false};
    std::atomic<quint64> next_request_id_{0};

    QMutex request_mutex_;                      // guards the three below
    QSet<quint64> cancelled_;
    QHash<QString, quint64> latest_by_channel_;
    QHash<quint64, QString> channel_of_;

    QMutex start_mutex_;                        // guards started_path_
    QString started_path_;                      // database file the seed / import ran for
    QTimer* retry_timer_ = nullptr;
    QTimer* broker_timer_ = nullptr;

    QMutex learn_mutex_;                        // guards the three below
    QVector<Instrument> pending_learn_;
    QHash<QString, QByteArray> learned_seen_;   // id -> compact JSON of the last learned record
    bool learn_flush_scheduled_ = false;
    QTimer* learn_timer_ = nullptr;
};

} // namespace fincept::services
