#pragma once
// SymbolMasterRepository — the SQL of the local instrument index (migration v060,
// symbol system plan E7 / WP2). Every function takes the CALLER'S connection and
// keeps no state, so it runs on whatever thread owns that connection:
// InstrumentIndex (services/instruments) calls it on its worker threads with
// Database::instance().connection(); the unit tests call it on a private
// connection. Nothing here may run on the GUI thread — InstrumentIndex registers
// the GUI thread with set_forbidden_thread() and every entry point logs (and
// asserts in debug builds) when called there.
//
// Records are core/instrument/Instrument (WP1). Keys written to symbol_alias use
// WP1's normalisers: alias_key() for alias_norm, normalize_name() for name_norm,
// instrument_id_for() for ids missing on the incoming record. Former symbols are
// alias rows of kind "former" pointing at the CURRENT instrument (TATAMOTORS ->
// XNSE:TMPV), so a former ticker resolves to the active listing and the ranker flags
// it (RankedInstrument::via_former_symbol). The renamed record itself stays, with
// status Renamed and successor_id set; resolve_current() follows that chain.
//
// Writers and precedence
//   Four sources write: the backend master (WP8), the bundled seed, learned
//   resolutions (quotes / picks anywhere in the app) and broker-master imports.
//   Source::Master > Seed > Learned > Broker. merge() decides field by field:
//     incoming source >= stored source: non-empty strings / a known class / a non-zero
//       weight replace the stored value; the curated sources (Seed, Master) also set
//       status, successor_id and primary_listing as given (a master record can
//       re-activate a listing); Learned / Broker only set a non-active status or a
//       primary flag that is true.
//     incoming source < stored source: only fills fields that are still empty.
//     Always: provider_symbols merged per key (incoming wins for its keys); aliases and
//       former_symbols unioned (stored order first, de-duplicated by alias_key).
//   The row keeps the higher of the two sources. Re-applying identical data writes
//   nothing (UpsertStats::unchanged).
//
// Errors are returned as raw driver text for the log; InstrumentIndex turns them
// into plain language with db::user_message().

#include "core/instrument/Instrument.h"
#include "core/instrument/InstrumentRank.h"
#include "core/instrument/SymbolInput.h"
#include "core/result/Result.h"

#include <QHash>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVector>

class QThread;

namespace fincept {

class SymbolMasterRepository {
  public:
    using Instrument = core::instrument::Instrument;
    using ParsedInput = core::instrument::ParsedInput;
    using RankContext = core::instrument::RankContext;
    using RankedInstrument = core::instrument::RankedInstrument;
    using AssetClass = core::instrument::AssetClass;

    /// Writer precedence; the value is stored as symbol_master.source_rank.
    enum class Source : int { Broker = 1, Learned = 2, Seed = 3, Master = 4 };
    /// "broker" / "learned" / "seed" / "master".
    static QString source_slug(Source s);
    /// Stored rank -> Source (anything out of range is Broker).
    static Source source_from_rank(int rank);

    // ── Writes ──────────────────────────────────────────────────────────────

    struct UpsertOptions {
        Source source = Source::Learned;
        /// Text stored in symbol_master.source when the slug is not precise enough
        /// ("broker:zerodha"); empty = source_slug(source).
        QString source_detail;
        /// Last day a former symbol was used: key "<id>|<alias_key(former)>",
        /// value "YYYY-MM-DD". Stored in symbol_alias.valid_to; kept across rewrites.
        QHash<QString, QString> former_valid_to;
        qint64 now_ms = 0; ///< updated_at; 0 = the current time
    };

    struct UpsertStats {
        int inserted = 0;
        int updated = 0;
        int unchanged = 0;
        int rejected = 0;          ///< records without an id and without a symbol that yields one
        QStringList changed_ids;   ///< inserted + updated, in batch order
        int changed() const { return inserted + updated; }
    };

    /// Normalise (normalized()), merge duplicates inside the batch, merge with the
    /// stored rows (merge()) and write — ONE transaction for the whole batch. The
    /// alias / provider lookup rows of every changed record are rebuilt.
    static Result<UpsertStats> upsert(QSqlDatabase& db, const QVector<Instrument>& batch, const UpsertOptions& opt);

    // ── Reads (all return records as stored) ────────────────────────────────

    /// Records for these ids, in the order given (unknown ids are skipped).
    static Result<QVector<Instrument>> find_by_ids(QSqlDatabase& db, const QStringList& ids);
    /// Exact canonical symbol (canonical_symbol() applied to `symbol` first).
    static Result<QVector<Instrument>> find_by_symbol(QSqlDatabase& db, const QString& symbol);
    /// Exact listing code ("RELIANCE" -> RELIANCE.NS, RELIANCE.BO; "700" == "0700"), any venue,
    /// heaviest first.
    static Result<QVector<Instrument>> find_by_listing_code(QSqlDatabase& db, const QString& code, int limit = 50);
    /// Exact alias_key match on any key kind (code, alias, former, name, isin, lead); best kind
    /// first (code, former, alias, isin, name, lead), then heaviest.
    static Result<QVector<Instrument>> find_by_alias(QSqlDatabase& db, const QString& text, int limit = 50);
    /// Instruments carrying this ISIN (current or a former ISIN a master sent).
    static Result<QVector<Instrument>> find_by_isin(QSqlDatabase& db, const QString& isin);
    /// Reverse provider lookup: ("broker:zerodha", "RELIANCE", "NSE"), ("binance", "BTC/USDT", "").
    /// An empty `exchange` matches any exchange.
    static Result<QVector<Instrument>> find_by_provider_symbol(QSqlDatabase& db, const QString& provider,
                                                             const QString& symbol, const QString& exchange = {});
    /// Code / alias / name prefix ("tata mot", "RELI"), heaviest first. `classes` empty = any.
    static Result<QVector<Instrument>> search_prefix(QSqlDatabase& db, const QString& text,
                                                     const QVector<AssetClass>& classes = {}, int limit = 50);
    /// Normalised name contains `text` (3+ characters after normalisation), heaviest first.
    static Result<QVector<Instrument>> search_name_contains(QSqlDatabase& db, const QString& text,
                                                            const QVector<AssetClass>& classes = {}, int limit = 50);
    /// Records whose successor_id is `id` (what was renamed into it).
    static Result<QVector<Instrument>> predecessors(QSqlDatabase& db, const QString& id);
    /// The record for an id or a symbol; when it is Renamed, its current successor is
    /// appended (chain followed, at most 8 hops, cycle-safe). [] when unknown.
    static Result<QVector<Instrument>> resolve_current(QSqlDatabase& db, const QString& symbol_or_id);
    /// Number of records.
    static Result<int> count(QSqlDatabase& db);

    struct GatherStats {
        int by_symbol = 0;      ///< exact canonical / candidate symbols
        int by_exact_key = 0;   ///< exact alias_norm (code, alias, former, name, isin)
        int by_key_prefix = 0;  ///< alias_norm range scan
        int by_name_prefix = 0; ///< name_norm range scan
        int by_contains = 0;    ///< name_norm contains (stage 2)
        int by_fuzzy = 0;       ///< 2-character key range + edit distance (stage 2)
        int loaded = 0;         ///< distinct records handed to the ranker
        bool stage2 = false;    ///< stage 2 ran (stage 1 gave fewer hits than the limit)
    };

    /// Index search: gathers candidates with indexed SQL passes, then orders them with
    /// WP1's rank_instruments(parsed, ctx). Stage 1: exact symbols (parsed candidates
    /// and canonical), exact keys (code / alias / former / name / ISIN), key prefix and
    /// name prefix (each heaviest first, capped; a dense prefix such as "A" walks the
    /// rank_weight index instead of sorting its whole range). Stage 2 only when stage 1
    /// ranks fewer than `limit` hits: name contains (not for fully specified symbols, FX
    /// pairs, contracts), and — when nothing matched exactly and the key has 4+ characters —
    /// fuzzy candidates (same first two characters, edit distance within the ranker's
    /// bound). `limit` <= 0 uses ctx.limit, else 50.
    static Result<QVector<RankedInstrument>> search(QSqlDatabase& db, const ParsedInput& parsed, const RankContext& ctx,
                                                    int limit, GatherStats* stats = nullptr);

    // ── Meta / sync state ───────────────────────────────────────────────────

    static constexpr const char* kMetaSeedVersion = "seed_version";
    static constexpr const char* kMetaMasterCursor = "master_cursor";
    static constexpr const char* kMetaMasterVersion = "master_version";
    /// "broker_import:<broker_id>" — MAX(instruments.updated_at) of the last import.
    static QString broker_meta_key(const QString& broker_id);

    /// Value, or an empty string when the key is not set.
    static Result<QString> meta_get(QSqlDatabase& db, const QString& key);
    static Result<void> meta_set(QSqlDatabase& db, const QString& key, const QString& value);

    // ── Backend symbol master sync (WP8 feeds this) ─────────────────────────
    //
    // Delta JSON (one page of GET /symbols/changes?since=<cursor>, or of a snapshot):
    //   {
    //     "since":    "1200",            optional: the cursor this page continues from; when
    //                                    present it must equal the stored cursor (an empty stored
    //                                    cursor accepts anything), else nothing is applied
    //     "next":     "1450",            REQUIRED: cursor stored once the page is applied
    //                                    (string or integer)
    //     "version":  "2026-11-16.1",    optional: dataset version, stored as master_version
    //     "has_more": false,             optional: another page follows (fetch with "next")
    //     "changes": [                   applied in order
    //       {"op": "upsert", "instrument": { Instrument::to_json() fields ...,
    //                                       "former_valid_to": {"TATAMOTORS": "2025-10-24"} }},
    //       {"op": "delist", "id": "XNSE:OLDCO"},
    //       {"op": "rename", "id": "XNSE:TATAMOTORS", "successor_id": "XNSE:TMPV",
    //                        "valid_to": "2025-10-24"}
    //     ]
    //   }
    // upsert merges with Source::Master precedence. delist sets status Delisted on a stored
    // record (unknown ids are counted as rejected). rename sets status Renamed + successor_id
    // on the old record and, when the successor is stored, adds the old listing code to the
    // successor's former_symbols (valid_to kept on the alias row). Unknown ops are rejected.
    // Rows are never deleted. The whole page, including the cursor, is ONE transaction.
    struct DeltaStats {
        int upserted = 0;   ///< records inserted or changed by "upsert"
        int unchanged = 0;  ///< "upsert" records identical to the stored ones
        int delisted = 0;
        int renamed = 0;
        int rejected = 0;   ///< unknown op, missing id / instrument, unknown id for delist / rename
        QString cursor;     ///< the stored cursor after the page ("next")
        QString version;
        bool has_more = false;
        QStringList changed_ids;
    };
    static Result<DeltaStats> apply_master_delta(QSqlDatabase& db, const QJsonObject& delta, qint64 now_ms = 0);

    // ── Broker masters (READ-ONLY use of the trading `instruments` table) ───

    struct BrokerState {
        QString broker_id;
        QString last_updated; ///< MAX(updated_at) as stored ("yyyy-MM-dd HH:mm:ss")
    };
    /// One entry per broker present in `instruments`; empty when the table does not exist.
    static Result<QVector<BrokerState>> broker_master_state(QSqlDatabase& db);

    struct BrokerRow {
        qint64 rowid = 0;
        QString symbol;          ///< normalised broker symbol (instruments.symbol)
        QString brsymbol;        ///< broker-native spelling
        QString name;
        QString exchange;        ///< normalised exchange ("NSE", "BSE", "NSE_INDEX")
        QString instrument_type; ///< "EQ" / "INDEX"
        QString token;           ///< broker_token when set, else instrument_token
    };
    /// Next page of a broker's cash-market rows (instrument_type EQ / INDEX), rowid > after_rowid.
    static Result<QVector<BrokerRow>> read_broker_rows(QSqlDatabase& db, const QString& broker_id, qint64 after_rowid,
                                                       int limit);
    /// Index record for a broker row: Yahoo form through VenueSymbols::to_yahoo (NSE ->
    /// .NS, BSE -> .BO, NSE_INDEX NIFTY -> ^NSEI), skeleton fields, the broker's name when
    /// it adds one, and provider_symbols["broker:<id>"] = "EXCH|SYMBOL|TOKEN[|BRSYMBOL]".
    /// Invalid (empty id) when the row has no Yahoo form (derivatives venues, unknown indices).
    static Instrument instrument_from_broker_row(const QString& broker_id, const BrokerRow& row);

    // ── Pure helpers (exposed for tests) ────────────────────────────────────

    /// Trimmed fields; symbol through canonical_symbol() (kept as given when that is empty);
    /// id from instrument_id_for(symbol) when missing; country upper case; an ISIN that fails
    /// its check digit dropped; aliases / former symbols de-duplicated by alias_key; empty
    /// provider entries dropped.
    static Instrument normalized(const Instrument& in);
    /// Field-level merge, see the header comment.
    static Instrument merge(const Instrument& stored, Source stored_source, const Instrument& incoming,
                            Source incoming_source);

    /// Debug guard: when set, every entry point logs an error (Q_ASSERT in debug builds)
    /// if it runs on `t`. InstrumentIndex passes the GUI thread. nullptr disables it.
    static void set_forbidden_thread(QThread* t);
};

} // namespace fincept
