#pragma once
#include "core/result/Result.h"
#include "datahub/Producer.h"
#include "services/markets/MarketDataTypes.h" // QuoteData, InfoData, HistoryPoint, TickerDef, ...

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVector>

#include <functional>

namespace fincept::services {

/// Fetches market data via Python/yfinance and produces the `market:*` DataHub
/// topics:
///
///   market:quote:<SYM>                       QuoteData                 TTL 30 s *
///   market:sparkline:<SYM>                   QVector<double> (5d, 1h)  TTL 10 min *
///   market:history:<SYM>:<period>:<interval> QVector<HistoryPoint>     TTL 30 min * (intraday: ~1 bar)
///   market:info:<SYM>                        InfoData                  TTL 4 h
///
/// * while the symbol's venue trades. Each value is published with a TTL from
///   the venue's session (core/market/MarketSessions): a quote is fresh for 60 s
///   in pre / post market and, once the session and a delayed feed's catch-up
///   are over, until the venue's next session change (at most 12 h); daily bars
///   and sparklines of a closed venue until it reopens. So nothing is polled
///   for a closed market. Yahoo reporting the session as REGULAR always wins.
///
/// Also publishes its request counters on the push-only topic
/// "diag:market_data" (QVariantMap) and logs a traffic summary every 10 min.
///
/// Quotes, sparklines and histories of one refresh tick share one `batch_all`
/// call on the persistent yfinance daemon; info topics are fetched by a
/// separate `batch_info` subprocess (up to kInfoBatchMax symbols each) so slow
/// fundamentals never stall the daemon's quote refreshes.
///
/// Also: fetch_quotes() batching (100 ms window) + quote cache for one-shot reads.
class MarketDataService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    using QuoteCallback = std::function<void(bool, QVector<QuoteData>)>;

    static MarketDataService& instance();

    /// DataHub topic of `symbol`'s quote: "market:quote:<SYM>" (value: QuoteData).
    static QString quote_topic(const QString& symbol);
    /// DataHub topic of `symbol`'s 5-day hourly close series:
    /// "market:sparkline:<SYM>" (value: QVector<double>).
    static QString sparkline_topic(const QString& symbol);
    /// DataHub topic of a price history: "market:history:<SYM>:<period>:<interval>"
    /// (value: QVector<HistoryPoint>), e.g. ("AAPL", "1mo", "1d"). Use these
    /// rather than assembling the strings by hand.
    static QString history_topic(const QString& symbol, const QString& period, const QString& interval);
    /// The daily history compute_performance() (MarketPerformance.h) is built
    /// for: "market:history:<SYM>:1y:1d". Every screen deriving performance
    /// should use this one so they share a single fetch per symbol.
    static QString performance_history_topic(const QString& symbol);
    /// DataHub topic of `symbol`'s fundamentals: "market:info:<SYM>" (value:
    /// InfoData; every numeric field NaN when Yahoo did not report it — see
    /// InfoData). Refreshed at most every few hours; fetched in batches.
    static QString info_topic(const QString& symbol);

    /// When `q` was actually fetched from Yahoo, epoch ms: its fetched_at, never
    /// later than `published_ms` (the hub's publish time, DataHub::last_publish_ms).
    /// A quote without a fetch time (older script) is as old as its publish. Use
    /// it — not the publish time alone — wherever a quote's age is shown or acted
    /// on: the Python layer serves cached quotes with their original fetch time.
    static qint64 quote_fetched_ms(const QuoteData& q, qint64 published_ms);
    /// True while the hub holds `symbol`'s quote inside its freshness window. The
    /// window follows the venue's session, so a closed market's quote stays
    /// current until the venue reopens however long ago it was published.
    static bool quote_is_current(const QString& symbol);

    /// Register this service as a DataHub producer + install the default
    /// `market:*` policies. Idempotent — safe if called more than once.
    /// Called from main.cpp after `datahub::register_metatypes()`.
    void ensure_registered_with_hub();

    // ── fincept::datahub::Producer ────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;

    /// Fetch quotes — batched and cached. Callback receives filtered results for requested symbols.
    /// Phase 3+: prefer `DataHub::subscribe(this, "market:quote:<sym>", ...)` for streaming widgets.
    /// This callback API remains for one-shot reads (e.g. report builder snapshots).
    ///
    /// Symbols whose hub quote is still fresh (see quote_is_current) or whose
    /// quote-cache row is are answered from there; only the rest are fetched,
    /// with every request made inside the same 100 ms sharing one call. When
    /// nothing has to be fetched the callback runs synchronously.
    ///
    /// A request that gets no quote at all (Python failed, Yahoo returned
    /// nothing, every symbol unknown) is a failure: the callback receives
    /// whatever is still fresh in the quote cache with ok = !empty, so
    /// "nothing" is reported as `cb(false, {})`, never as an empty success.
    void fetch_quotes(const QStringList& symbols, QuoteCallback cb);

    /// Why the fetch for the fetch_quotes() request whose callback is running
    /// returned no quote for any of its symbols (user-readable, e.g. "Yahoo
    /// Finance is rate limiting requests ..."). Also set when that callback
    /// is then served from the quote cache; empty when the fetch returned
    /// quotes for it. Only meaningful inside a QuoteCallback — the value
    /// changes as the next callback is invoked.
    const QString& last_fetch_error() const { return last_fetch_error_; }

    /// Fetch full company info (P/E, 52W range, market cap, ratios, etc.) for one
    /// symbol — two Python calls per use, original InfoData fields 0 when
    /// unknown. For lists (and anything new) subscribe to info_topic() instead.
    using InfoCallback = std::function<void(bool, InfoData)>;
    void fetch_info(const QString& symbol, InfoCallback cb);

    /// Resolve human-readable display names (e.g. "^GSPC" → "S&P 500",
    /// "GC=F" → "Gold") for cryptic tickers so market tables can show names
    /// instead of raw symbols. Names come from yfinance (longName/shortName).
    ///
    /// Names are static, so they are cached to disk: the callback fires once
    /// immediately with whatever is already cached for `symbols`, and — if any
    /// were missing — again after a single `quote_names` Python call resolves
    /// and persists them. The network hit is paid once per symbol, never on
    /// the quote-refresh path.
    using NamesCallback = std::function<void(const QHash<QString, QString>&)>;
    void resolve_names(const QStringList& symbols, NamesCallback cb);

    /// Currency-symbol prefix for a symbol's price (e.g. "AAPL" → "$",
    /// "RELIANCE.NS" → "₹"), or an empty string when unknown or not meaningful
    /// (forex pairs, index levels/yields). Minor-unit listings get their code,
    /// not the major symbol ("VOD.L" → "GBp ", "NPN.JO" → "ZAc "). Resolved
    /// alongside display names by `resolve_names` (and from published quotes
    /// that carry a currency) and read from the same disk-backed cache, so this
    /// is a cheap synchronous lookup.
    QString currency_prefix(const QString& symbol);

    static QVector<MarketCategory> default_global_markets();
    static QVector<RegionalMarket> default_regional_markets();

    /// Default symbol lists for dashboard widgets
    static QStringList indices_symbols();
    static QStringList forex_symbols();
    static QStringList crypto_symbols();
    static QStringList commodity_symbols();
    static QStringList mover_symbols();
    static QStringList global_snapshot_symbols();

  private:
    MarketDataService();
    void flush_batch();

    /// Internal: publish the per-symbol result to the hub and clear
    /// in_flight for the matching topic. Called from inside `flush_batch`.
    /// Also teaches currency_prefix() the symbol's currency (see remember_quote_currency).
    void publish_quote_to_hub(const QuoteData& q);
    /// A quote that says its currency (and is not FX / an index, matching what
    /// quote_names records) fills the in-memory currency cache, so
    /// currency_prefix() knows it before / without a quote_names call.
    void remember_quote_currency(const QuoteData& q);
    /// `market:info:*` refresh: one `batch_info` subprocess per kInfoBatchMax
    /// symbols, chunks run one after another (a rate-limited chunk fails the
    /// rest without fetching); every topic ends in publish() or publish_error().
    void refresh_info(const QStringList& symbols);
    /// Daily and longer bars keep the `market:history:*` policy TTL; an intraday
    /// series is published with a TTL of about one bar (60 s – 15 min).
    void publish_history_to_hub(const QString& symbol, const QString& period, const QString& interval,
                                const QVector<HistoryPoint>& points);
    void publish_sparkline_to_hub(const QString& symbol, const QVector<double>& points);

    // ── Display-name cache (symbol → human-readable name) ──
    // Persisted to SettingsRepository so resolution survives restarts and the
    // per-symbol yfinance .info cost is paid at most once.
    void load_name_cache();
    void persist_name_cache();
    QHash<QString, QString> name_cache_;
    QHash<QString, QString> currency_cache_; // symbol → ISO currency code (e.g. "USD")
    bool name_cache_loaded_ = false;

    // Name resolution is coalesced: every resolve_names() call inside a short
    // window shares ONE quote_names subprocess. Without this the Markets screen
    // (one call per panel) spawned ~17 processes at once, filled every
    // PythonRunner slot for half a minute and starved other Python work.
    void flush_name_requests();
    struct NameWaiter {
        QStringList symbols;
        NamesCallback cb;
    };
    QVector<NameWaiter> name_waiters_;
    QSet<QString> names_queued_;    // waiting for the next flush
    QSet<QString> names_inflight_;  // inside a running quote_names call
    QSet<QString> names_attempted_; // asked once this session — unresolvable ones are not retried
    bool names_flush_scheduled_ = false;

    // ── Batching ──
    struct PendingRequest {
        QStringList fetch;         ///< symbols not answered from the hub / quote cache
        QVector<QuoteData> served; ///< answered before the batch, handed back with the fetched ones
        QuoteCallback cb;
    };
    QVector<PendingRequest> pending_;
    bool batch_scheduled_ = false;
    QString last_fetch_error_; ///< see last_fetch_error()

    bool hub_registered_ = false;
    /// Last whole-batch failure of refresh() — logged at WARN only when it
    /// changes, so a persistent outage doesn't log on every retry.
    QString last_refresh_failure_;

    // ── Caching — delegated to CacheManager ──
    static constexpr int kQuoteCacheTtlSec = 30;

    /// Symbols per batch_info subprocess. Each symbol is ~3 Yahoo requests
    /// (Ticker.info), run 6 at a time inside the script: ~0.15 s per symbol.
    static constexpr int kInfoBatchMax = 50;

    // ── Yahoo traffic accounting (GUI thread, like every reply callback) ──
    // What this service asked Python for, and what it answered without asking.
    // report_traffic() publishes the totals on "diag:market_data" and, every
    // 10 minutes in which something happened, logs one INFO line with the
    // window's figures plus the daemon's own yahoo_stats.
    struct Traffic {
        quint64 batch_all_calls = 0;    ///< batch_all calls (quote / sparkline / history refresh)
        quint64 quote_symbols = 0;      ///< quote symbols requested (batch_all + batch_quotes)
        quint64 batch_quotes_calls = 0; ///< fetch_quotes() batches
        quint64 info_chunks = 0;        ///< batch_info subprocesses
        quint64 quote_names_calls = 0;  ///< quote_names subprocesses
        quint64 served_memory = 0;      ///< fetch_quotes() symbols answered from the hub / quote cache
        quint64 merged = 0;             ///< fetch_quotes() requests that rode on another's batch
        quint64 rate_limited = 0;       ///< replies that reported Yahoo rate limiting
        quint64 hidden_skips = 0;       ///< DataHub::hidden_skip_count(), as of the last report
    };
    Traffic traffic_;          ///< since start
    Traffic traffic_reported_; ///< traffic_ as of the last report
    QTimer* traffic_timer_ = nullptr;
    void report_traffic();
    /// The summary line: the window's counters plus, when the daemon answered,
    /// its yahoo_stats (`py`; nullptr = not asked / no answer).
    static QString traffic_line(const Traffic& window, const QJsonObject* py);
};

} // namespace fincept::services
