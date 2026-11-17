#pragma once
// InstrumentResolver — THE symbol search / resolution service of the terminal
// (symbol system WP3, plan E4). Every place a user can type, paste, import or
// restore a symbol goes through it (directly, or through ui::SymbolSearchBox),
// so formatting, parsing, venue suffixes and ranking are decided in ONE place.
//
// Sources, in order
//   1. the local instrument index (InstrumentIndex: bundled seed + backend master +
//      broker masters + everything learned) — instant, works offline;
//   2. the backend: GET /symbols/search (the symbol master) when deployed, else GET
//      /market/search (the HttpClient path MarketSearchService uses) — see
//      BackendSymbolSearch.h — queried with the NORMALISED text (RELIANCE.NS ->
//      "RELIANCE", BRK-B -> "BRK.B"); /market/search rows are mapped through
//      core::instrument::to_yahoo_tv(). A row whose
//      venue has no Yahoo symbol is kept with an empty `symbol` (no quote data) and an
//      id of the form "<MIC or TV.<code>>:<listing code>";
//   3. Yahoo search through the existing Python command (`yfinance_data.py search`,
//      routed to the warm yfinance daemon) — only when 1 + 2 found no exact match or
//      the backend failed; ISINs go straight here (the backend has none);
//   4. resolve() only: `yfinance_data.py resolve_symbol` as the last step, to confirm
//      that a ticker-shaped text with no exact hit really quotes on Yahoo.
//   Results are merged, de-duplicated by instrument id (Instrument::same_instrument;
//   the local record wins, empty fields are filled from the remote copy) and ordered
//   with core::instrument::rank_instruments() — the same order everywhere.
//
// Learning: Yahoo rows (Yahoo's own symbols), validated symbols, exact resolve()
// answers and every learn() / remember_pick() call are written to the index. Backend
// rows are learned only when picked (their Yahoo form is derived, not confirmed).
//
// Rate limits: remote sources are asked >= 250 ms after the last search() of a
// context, at most one backend and one Yahoo request in flight per context (the newest
// text waits, older ones are dropped), replies cached for 5 minutes per query. A source
// that cannot be reached is skipped for a while (backend 60 s, Python 5 min) and the
// result carries a plain note; local results are still returned.
//
// Threads: create / call on the GUI thread (calls from another thread are re-posted).
// Callbacks run on the thread of `context` (normally the GUI thread). A destroyed
// context drops its callbacks. Nothing here blocks: the index answers from its worker
// threads, HTTP and Python are asynchronous.
//
// Owner rules applied (symbol-audit G1, G2, G5)
//   * the user's home country ranks first; a bare ticker that other issuers also use is
//     NOT exact (the list opens), a second listing of the same issuer does not count
//     against exactness (RELIANCE -> RELIANCE.NS over RELIANCE.BO, AAPL over AAPL.MX);
//   * the class the text implies wins ("BTC" -> BTC-USD, "gold" -> GC=F, "NIFTY" ->
//     ^NSEI): same-named listings of OTHER classes do not make it ambiguous;
//   * a renamed symbol resolves to its successor with a note ("TATAMOTORS is now TMPV");
//     nothing stored is ever migrated silently (resolve_stored reports, never rewrites).

#include "core/instrument/Instrument.h"
#include "core/instrument/InstrumentRank.h"
#include "core/instrument/SymbolInput.h"

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <atomic>
#include <functional>
#include <memory>

namespace fincept::services {

struct ResolveOptions {
    QString venue_hint;   ///< exchange hint for the parser ("NSE", "XETRA", a broker code); empty = none
    QString home_country; ///< ISO 3166-1 alpha-2; empty = ctx.home_country, else the user's market region
    /// Ranking / filtering: allowed_classes FILTERS (search, resolve and the box),
    /// class_priority orders, include_derivatives / include_cfd / include_inactive.
    fincept::core::instrument::RankContext ctx;
    int limit = 12;           ///< rows returned (1..50)
    bool allow_remote = true; ///< false = local index only (no network, no Python)
};

struct ResolveResult {
    QVector<fincept::core::instrument::Instrument> matches; ///< best first
    bool exact = false;       ///< matches[0] is an unambiguous exact hit for the input
    bool from_remote = false; ///< remote sources contributed
    QString note;             ///< plain-language note, e.g. "TATAMOTORS is now TMPV"
    QString error;            ///< plain-language; empty on success (an empty match list is not an error)

    // ── additions (beyond the fixed interface) ──
    /// Parallel to `matches`: how each matched (match kind, matched text, former-symbol flag).
    QVector<fincept::core::instrument::RankedInstrument> ranked;
    QString query;            ///< the text this result answers (as passed in)
    quint64 request_id = 0;
    /// search(): false while a remote source may still deliver a fuller list for this
    /// request (every callback carries the complete current list; replace, don't append).
    bool final = true;
    bool offline = false;     ///< a remote source could not be reached; the list may be incomplete
};

/// One answer of a remote source (backend search, Yahoo search, Yahoo validation).
struct ResolverRemoteReply {
    bool ok = false;
    bool unreachable = false; ///< network / server / Python unavailable (not "no match")
    QVector<fincept::core::instrument::Instrument> rows; ///< mapped, in the source's own order
    QString error;            ///< plain language (logged; shown only as a note)
    bool first_confirmed = false; ///< validation: rows[0] was confirmed to return market data
};

/// The resolver's seams. Production sources (InstrumentResolverSources.cpp) use the
/// InstrumentIndex, HttpClient and PythonRunner; tests pass fakes. Every callback
/// must be invoked exactly once (unless the request was cancelled), on the thread of
/// the `context` passed in, never synchronously from inside the call.
struct ResolverSources {
    using Instrument = fincept::core::instrument::Instrument;
    using RankedInstrument = fincept::core::instrument::RankedInstrument;

    struct LocalQuery {
        fincept::core::instrument::ParsedInput parsed;
        QString venue_hint;
        fincept::core::instrument::RankContext ctx; ///< home filled in
        int limit = 12;
        QString channel; ///< non-empty: a newer query on the same channel supersedes this one
    };
    using LocalSearchDone = std::function<void(const QVector<RankedInstrument>& hits, const QString& error)>;
    using LookupDone = std::function<void(const QVector<Instrument>& found, const QString& error)>;
    using RemoteDone = std::function<void(const ResolverRemoteReply& reply)>;
    enum class LookupMode : quint8 {
        Current, ///< key = canonical symbol or id: [record] or [renamed record, current successor]
        Isin,    ///< key = ISIN
    };

    std::function<quint64(const LocalQuery&, QObject* context, LocalSearchDone)> local_search;
    std::function<void(quint64)> local_cancel;
    std::function<void(const QString& key, LookupMode mode, QObject* context, LookupDone)> local_lookup;
    std::function<void(const Instrument&)> learn;
    /// The backend with the normalised query: /symbols/search (instruments_from_symbols_search()),
    /// else /market/search (instruments_from_market_search()); see BackendSymbolSearch.
    std::function<void(const QString& query, int limit, QObject* context, RemoteDone)> backend_search;
    /// `yfinance_data.py search <query> <limit>`; rows mapped by instruments_from_yahoo_search().
    std::function<void(const QString& query, int limit, QObject* context, RemoteDone)> yahoo_search;
    /// `yfinance_data.py resolve_symbol <text>`: ok + one row when Yahoo confirms the symbol quotes.
    std::function<void(const QString& text, QObject* context, RemoteDone)> yahoo_validate;
    /// The user's home country when the caller gives none (the market region); may be empty.
    std::function<QString()> default_home_country;
    /// Recent picks per key, newest first, as compact Instrument JSON.
    std::function<QStringList(const QString& key)> load_recent;
    std::function<void(const QString& key, const QStringList& json)> save_recent;

    int debounce_ms = 250;             ///< remote calls start this long after the last keystroke
    int cache_ttl_ms = 5 * 60 * 1000;  ///< remote reply cache
    int backend_retry_ms = 60 * 1000;  ///< skip an unreachable backend this long
    int yahoo_retry_ms = 5 * 60 * 1000;///< skip an unavailable Python / Yahoo this long
    int local_timeout_ms = 5000;       ///< resolve(): continue without the index after this long
};

/// Plain market facts for pricing / settling a paper trade without another lookup.
struct TradingMeta {
    QString venue;      ///< MarketSessions venue id whose hours apply ("NSE", "NYSE" for bare US, "FX", "CRYPTO")
    QString time_zone;  ///< IANA id of that venue ("Asia/Kolkata"); empty when no session venue is known
    double lot_size = 1.0;   ///< minimum order unit in shares / units
    double price_tick = 0.0; ///< minimum price step; 0 when unknown
    bool lot_known = false;  ///< lot_size is the venue's rule for this class (cash equities / ETFs only)
    bool tick_known = false; ///< always false today: ticks depend on price band and venue, not carried yet
};

/// Venue hours / lot facts for an instrument (MarketSessions read-only). Lots: 1 for cash
/// equities / ETFs on US, Indian, UK, European, Canadian, Australian, Korean ... venues; 100
/// on TSE (JP), SSE / SZSE (CN), SGX, Bursa Malaysia, SET, IDX; 1000 on TWSE; unknown (1,
/// lot_known false) for Hong Kong (board lot per stock), indices, FX, crypto, futures, options.
TradingMeta trading_meta_for(const fincept::core::instrument::Instrument& inst);

/// True when the instrument has a market-data symbol (Yahoo form) the terminal can quote.
inline bool is_quotable(const fincept::core::instrument::Instrument& inst) {
    return !inst.symbol.trimmed().isEmpty();
}

/// Missing id / display / class / venue / mic / country / currency / name filled from the
/// canonical symbol (VenueSymbols::instrument_skeleton); index currency from its venue.
fincept::core::instrument::Instrument complete_instrument(const fincept::core::instrument::Instrument& inst);

/// GET /market/search body ({"results": [...]} or a bare array) -> instruments, in row order.
/// Rows of type "economic" are skipped. rank_weight 1..5 from the row position (the backend's
/// own order) unless a curated copy supplies one.
QVector<fincept::core::instrument::Instrument> instruments_from_market_search(const QJsonDocument& body);
/// One /market/search row (position = its index in the reply). Invalid (empty id) when skipped.
fincept::core::instrument::Instrument instrument_from_market_search_row(const QJsonObject& row, int position);
/// `yfinance_data.py search` reply ({"results": [...]}) -> instruments. `isin_query`: the ISIN
/// the search was for (Yahoo matched every row by it), stored on the rows.
QVector<fincept::core::instrument::Instrument> instruments_from_yahoo_search(const QJsonObject& reply,
                                                                            const QString& isin_query = {});
/// One Yahoo search row ({symbol, name, exchange, type, currency}).
fincept::core::instrument::Instrument instrument_from_yahoo_row(const QJsonObject& row, int position);

/// The InstrumentIndex-backed half of the sources (local_search, local_cancel, local_lookup,
/// learn); InstrumentResolverIndex.cpp. Remote members stay empty.
ResolverSources index_resolver_sources();

class InstrumentResolver : public QObject {
    Q_OBJECT
  public:
    using Instrument = fincept::core::instrument::Instrument;
    using Callback = std::function<void(const ResolveResult&)>;

    /// The application's resolver (production sources, see register_default_sources()).
    static InstrumentResolver& instance();

    /// A private resolver over `sources` (tests, tools). Lives on the creating thread.
    explicit InstrumentResolver(ResolverSources sources, QObject* parent = nullptr);
    ~InstrumentResolver() override;

    /// Search-as-you-type: local index at once, remote results merged when they arrive (cb
    /// fires once with the local list — `final` false when remote sources will still be
    /// asked — and once more per remote source that changes the answer; the last call has
    /// `final` true). A newer search() with the same `context` drops the older request.
    quint64 search(const QString& text, const ResolveOptions& opts, QObject* context, Callback cb);
    /// One final answer for committed text (Enter / paste / import): exact -> local -> remote
    /// -> validation. cb fires exactly once (unless cancelled). Not superseded by context.
    quint64 resolve(const QString& text, const ResolveOptions& opts, QObject* context, Callback cb);
    /// For symbols already saved (watchlists, portfolios, layouts): canonicalise, follow
    /// renames, never guess. Local index only (no network). matches[0] is the STORED
    /// instrument (status Delisted / Renamed when so); when renamed, matches[1] is its current
    /// successor and `note` says so. A bare code without exchange that the index does not
    /// know comes back as its Yahoo (US) skeleton with exact == false and a note. Text that is
    /// not a symbol (a name, an unknown venue) gives no match and a note; an ISIN is looked up
    /// (exact only when one listing carries it).
    quint64 resolve_stored(const QString& stored_symbol, const QString& exchange_hint, QObject* context, Callback cb);
    /// Drop a pending request: its callback never runs.
    void cancel(quint64 id);
    /// Call when the user picks an instrument (writes it to the index).
    void learn(const Instrument& picked);

    // ── additions ─────────────────────────────────────────────────────────
    using ListCallback = std::function<void(const QVector<ResolveResult>& results)>;
    /// resolve() for every text (imports, pasted lists), 4 at a time; results in input order.
    quint64 resolve_many(const QStringList& texts, const ResolveOptions& opts, QObject* context, ListCallback cb);
    /// learn() + remember as the newest recent pick under `key` (max 12 per key).
    void remember_pick(const QString& key, const Instrument& picked);
    /// Recent picks under `key`, newest first.
    QVector<Instrument> recent_picks(const QString& key, int limit = 8) const;
    /// Forget cached remote replies and source back-offs (e.g. after signing in).
    void clear_cache();
    /// Factory for instance(); InstrumentResolverSources.cpp registers the production one at
    /// static-initialisation time. Without one, instance() runs on empty sources.
    static bool register_default_sources(ResolverSources (*factory)());

  private:
    enum class Mode : quint8 { Search, Resolve, Stored };
    struct Request;
    struct ContextState;
    struct Batch;
    struct CacheEntry {
        qint64 at_ms = 0;
        ResolverRemoteReply reply;
    };
    using RequestPtr = std::shared_ptr<Request>;

    quint64 start_request(Mode mode, const QString& text, const ResolveOptions& opts, QObject* context, Callback cb);
    RequestPtr live(quint64 id) const;
    void begin(const RequestPtr& r);
    void run_local(const RequestPtr& r);
    void on_local(quint64 id, const QVector<fincept::core::instrument::RankedInstrument>& hits, const QString& error);
    bool remote_possible(const Request& r) const;
    void run_remote_chain(const RequestPtr& r);
    void continue_after_backend(const RequestPtr& r);
    bool wants_yahoo(const Request& r) const;
    bool wants_validate(const Request& r) const;
    void run_backend(const RequestPtr& r);
    void on_backend(quint64 id, const ResolverRemoteReply& reply);
    void run_yahoo(const RequestPtr& r);
    void on_yahoo(quint64 id, const ResolverRemoteReply& reply);
    void run_validate(const RequestPtr& r);
    void on_validate(quint64 id, const ResolverRemoteReply& reply);
    void run_stored(const RequestPtr& r);
    void merge_into_pool(Request& r, const QVector<Instrument>& rows, bool remote);
    ResolveResult build_result(const Request& r, bool final) const;
    void deliver(const RequestPtr& r, bool final);
    void deliver_result(const RequestPtr& r, ResolveResult res, bool final);
    void pump_batch(const std::shared_ptr<Batch>& b);
    ContextState& context_state(QObject* context);
    void forget_context(QObject* context);
    bool cache_get(const QString& key, ResolverRemoteReply* out);
    void cache_put(const QString& key, const ResolverRemoteReply& reply);
    QString effective_home(const ResolveOptions& opts) const;

    ResolverSources sources_;
    std::atomic<quint64> next_id_{0};
    QHash<quint64, RequestPtr> requests_;
    QHash<quint64, std::shared_ptr<Batch>> batches_;
    QSet<quint64> dead_; ///< cancelled / superseded ids whose queued callbacks must not run
    QHash<QObject*, std::shared_ptr<ContextState>> contexts_;
    QHash<QString, CacheEntry> cache_;
    qint64 backend_down_until_ms_ = 0;
    qint64 yahoo_down_until_ms_ = 0;
    int yahoo_in_flight_ = 0;
};

} // namespace fincept::services
