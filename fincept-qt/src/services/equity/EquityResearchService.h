// src/services/equity/EquityResearchService.h
#pragma once
#include "services/equity/EquityResearchModels.h"

#include <QHash>
#include <QObject>
#include <QVector>

#include <functional>

class QNetworkAccessManager;

namespace fincept::services::equity {

/// News source for fetch_news(). Auto = the default GNews → Yahoo chain;
/// NewsApi = newsapi.org (only when a key is configured in Data Sources).
enum class NewsProvider { Auto, NewsApi };

/// One failed load, attributed to the symbol and leg it was for (see
/// EquityResearchService::load_failed). A listener drops a failure whose symbol it no
/// longer shows, so a late failure for the previous symbol cannot blank the new one.
struct LoadFailure {
    QString symbol;  ///< the symbol exactly as the caller passed it
    QString leg;     ///< "Quote" | "Info" | "Historical" | "Financials" | "Technicals" | "News" | "Peers"
    QString period;  ///< Historical / Technicals: the period that was requested; empty otherwise
    QString message; ///< the real reason (script / runner text); never empty
    /// Failure class: "not_found" | "no_data" | "rate_limited" | "network" | "unknown", from the
    /// script's "code" field or, for an older script, read off the message. Empty when unclear.
    QString code;
};

/// A 10-year government bond yield used as the risk-free rate (fetch_risk_free()).
struct RiskFreeRate {
    QString currency; ///< major ISO code it was asked for ("GBP" for "GBp")
    double rate_pct = 0.0;
    QString source;   ///< "live" (^TNX) | "oecd" (FRED monthly) | "reference" (static, must be checked)
    QString label;    ///< what it is, for the UI ("US 10Y Treasury (^TNX)")
    QString as_of;    ///< "2026-11-15" / "2026-10" / "2025-06"
};

/// One "did you mean" entry from resolve_symbol().
struct SymbolCandidate {
    QString symbol;
    QString name;
    QString exchange;
    QString type;
};

/// Answer of resolve_symbol().
struct SymbolResolution {
    QString query;           ///< the text that was resolved (trimmed, as passed)
    QString resolved_symbol; ///< the Yahoo form when found (RELIANCE.NS, BRK-B); else the query
    QString exchange;
    QString name;    ///< company / instrument name when found (may be empty)
    QString message; ///< the script's explanation when not found (may be empty)
    bool found = false;
    QVector<SymbolCandidate> candidates; ///< up to 8 suggestions; may be empty
};

class EquityResearchService : public QObject {
    Q_OBJECT
  public:
    static EquityResearchService& instance();

    // ── Async API — results delivered via signals ─────────────────────────────
    /// Fetches quote + info + historical in parallel (three Python calls).
    /// Used by the UI panels that need all three. Tool handlers that only
    /// need one slice should use the per-data loaders below to avoid wasting
    /// daemon slots and Yahoo API calls.
    void load_symbol(const QString& symbol, const QString& period = "1y");

    /// Per-data loaders. Each fires exactly one Python call and emits the
    /// matching *_loaded signal. Cache-aware: re-emits immediately on hit.
    void load_quote_only(const QString& symbol);
    void load_info_only(const QString& symbol);
    void load_historical_only(const QString& symbol, const QString& period = "1y");

    void fetch_financials(const QString& symbol);
    /// `force` drops the cached indicator frame and its candles first (a user-pressed REFRESH).
    void fetch_technicals(const QString& symbol, const QString& period = "1y", bool force = false);
    void fetch_peers(const QString& symbol, const QStringList& peer_symbols);
    /// Same-industry companies for `symbol` (largest first), from Yahoo's industry
    /// grouping. Emits suggested_peers_loaded; an empty list means none were found.
    void fetch_suggested_peers(const QString& symbol, int count = 5);
    /// `force` drops the cached result first, so a user-pressed REFRESH really re-fetches.
    void fetch_news(const QString& symbol, int count = 20, NewsProvider provider = NewsProvider::Auto,
                    bool force = false);

    /// The configured & enabled NewsAPI.org key from the Data Sources tab, or an
    /// empty string when none. Used to gate the News-tab provider switch and to
    /// supply the key at fetch time.
    QString configured_newsapi_key() const;

    /// One-shot OHLCV pull for the "Download price data (CSV)" dialog — any yfinance
    /// period/interval (intraday included), never cached, never broadcast (one caller,
    /// one result). `done(ok, candles, error)` runs on the service thread; `candles` is
    /// ascending by time. Implemented in EquityResearchService_Ext.cpp.
    using HistoryCallback = std::function<void(bool ok, QVector<Candle> candles, QString error)>;
    void fetch_price_history(const QString& symbol, const QString& period, const QString& interval,
                             HistoryCallback done);

    /// Turn what a user typed — a bare non-US ticker (RELIANCE), a class share (BRK.B) or a
    /// company name — into the Yahoo symbol (`yfinance_data.py resolve_symbol`). `done` runs
    /// once, on the GUI thread. ok=false means the lookup itself failed (Python missing,
    /// network, rate limit; `error` says why) — NOT that the symbol is unknown: an unknown
    /// symbol is ok=true with resolution.found=false plus any candidates. Found answers are
    /// cached for a day. Implemented in EquityResearchService_Ext.cpp.
    using ResolveCallback = std::function<void(bool ok, SymbolResolution resolution, QString error)>;
    void resolve_symbol(const QString& text, ResolveCallback done);

    /// Dividends, splits, earnings history (estimate vs actual, report dates), the next
    /// earnings / ex-dividend dates and the per-share extras the info payload lacks
    /// (equity_research_data.py events). Cached for 6 h; `force` drops the copy first.
    /// Emits events_loaded, or load_failed with leg "Events". Implemented in
    /// EquityResearchService_Ext.cpp.
    void fetch_events(const QString& symbol, bool force = false);

    /// The 10-year government yield of `currency` (minor units accepted: "GBp" → GBP).
    /// `done` runs once on the GUI thread; cached for 12 h. Implemented in
    /// EquityResearchService_Ext.cpp.
    using RiskFreeCallback = std::function<void(bool ok, RiskFreeRate rate, QString error)>;
    void fetch_risk_free(const QString& currency, RiskFreeCallback done);

  signals:
    void events_loaded(fincept::services::equity::EquityEvents events);
    void quote_loaded(fincept::services::equity::QuoteData quote);
    void info_loaded(fincept::services::equity::StockInfo info);
    /// `period` is the one the candles were requested for — a late answer for a period the
    /// chart has since left can be told apart. Never emitted with an empty series (that is
    /// a load_failed instead).
    void historical_loaded(QString symbol, QString period, QVector<fincept::services::equity::Candle> candles);
    void financials_loaded(fincept::services::equity::FinancialsData data);
    void technicals_loaded(fincept::services::equity::TechnicalsData data);
    void peers_loaded(QVector<fincept::services::equity::PeerData> peers);
    void suggested_peers_loaded(QString symbol, QStringList peers);
    /// `provider` is the one the request asked for — the News tab (provider picker) and the
    /// Sentiment blend (Auto) both fetch the same symbol, and each must ignore the other's
    /// answer. `fell_back` = NewsAPI was asked for but the articles come from the Auto chain.
    void news_loaded(QString symbol, fincept::services::equity::NewsProvider provider, bool fell_back,
                     QVector<fincept::services::equity::NewsArticle> articles);
    /// Legacy, symbol-less failure notice (`context` = the leg name, `message` = the real
    /// reason). Kept for existing listeners; new code should use load_failed().
    void error_occurred(QString context, QString message);
    /// The same failure, attributed: emitted right after error_occurred for every failed leg.
    void load_failed(fincept::services::equity::LoadFailure failure);

  private:
    explicit EquityResearchService(QObject* parent = nullptr);
    Q_DISABLE_COPY(EquityResearchService)

    // ── Python helpers ────────────────────────────────────────────────────────
    void run_python(const QString& script, const QStringList& args, std::function<void(bool, const QString&)> cb);
    /// run_python plus the failure class: on failure `code` is the script's "code" field, or
    /// a best-effort reading of the message for an older script ("" when unclear).
    using PyCodedCallback = std::function<void(bool ok, const QString& out_or_error, const QString& code)>;
    void run_python_coded(const QString& script, const QStringList& args, PyCodedCallback cb);
    /// Emit error_occurred + load_failed for one failed leg. `message` falls back to
    /// `fallback` when empty.
    void report_failure(const QString& symbol, const QString& leg, const QString& period, const QString& message,
                        const QString& code, const QString& fallback);

    // Candle source: cache → region-matched connected broker → yfinance
    // fallback. done(ok, hist_json_or_error) is invoked on the main thread; on failure the
    // string is the reason. Cached under the same "equity:candles:<symbol>:<period>" key as
    // load_historical_only().
    void ensure_candles(const QString& symbol, const QString& period, std::function<void(bool, const QString&)> done);

    // The default Auto chain (cache → Google News → yfinance). `requested` is the provider the
    // caller asked for; answers are emitted tagged with it (fell_back when it was NewsApi).
    void fetch_news_auto(const QString& symbol, int count, NewsProvider requested);

    // yfinance news path — the fallback fetch_news_auto() uses when Google News
    // (fetch_company_news.py) is unavailable or returns no articles.
    void fetch_news_yfinance(const QString& symbol, int count, NewsProvider requested);

    // NewsAPI.org path (native HTTP via news_nam_). Used when the equity News tab
    // selects the NewsApi provider and a key is configured. Caches under
    // "equity:news:newsapi:<symbol>"; on any failure (401/quota/network/empty)
    // it falls back to fetch_news_auto(symbol, count, NewsProvider::NewsApi).
    void fetch_news_newsapi(const QString& symbol, int count, const QString& api_key);

    // ── Parsers ───────────────────────────────────────────────────────────────
    QuoteData parse_quote(const QJsonObject& obj) const;
    StockInfo parse_info(const QJsonObject& obj) const;
    QVector<Candle> parse_candles(const QJsonArray& arr) const;
    FinancialsData parse_financials(const QJsonObject& obj) const;
    TechnicalsData parse_technicals(const QString& symbol, const QJsonArray& rows) const;
    TechSignal score_indicator(const QString& name, double value, double sma20, double sma50) const;
    QVector<PeerData> parse_peers(const QJsonArray& arr) const;
    QVector<NewsArticle> parse_news(const QJsonArray& arr) const;
    EquityEvents parse_events(const QJsonObject& obj) const;

    // ── Cache TTLs — delegated to CacheManager ───────────────────────────────
    // The Python layer caches Yahoo's answers itself by freshness class (info and
    // statements a day on disk, bars in an on-disk bar cache, quotes 15 s live /
    // until the next open when closed), so these copies mostly save the IPC round
    // trip and the parse; they can be as long as the data's own pace allows.
    static constexpr int kQuoteTtlSec = 30;
    static constexpr int kInfoTtlSec = 60 * 60;       // profile / ratios move daily at most
    static constexpr int kHistoricalTtlSec = 15 * 60; // daily bars; the open bar may lag up to 15 min
    static constexpr int kNewsTtlSec = 10 * 60;
    static constexpr int kFinancialsTtlSec = 24 * 60 * 60; // statements only change quarterly: one fetch a day
    // Last-known-good statements: replayed (flagged stale) the moment a symbol is reopened
    // after the 10-minute copy expired, while the refresh runs — and kept on screen, still
    // flagged, if that refresh fails.
    static constexpr int kFinancialsLkgTtlSec = 30 * 24 * 3600;
    // Indicator values off DAILY bars; without this every Technicals-tab visit / Sentiment blend
    // re-fetched the candles and recomputed the frame.
    static constexpr int kTechnicalsTtlSec = 300;
    // A typed text → Yahoo symbol mapping (RELIANCE → RELIANCE.NS) does not change.
    static constexpr int kResolveTtlSec = 24 * 3600;
    // Dividends / earnings dates change a few times a year; government yields daily at most.
    static constexpr int kEventsTtlSec = 6 * 3600;
    static constexpr int kRiskFreeTtlSec = 12 * 3600;
    // Peer comparison rows (ratios, caps) vs the suggested peer LIST (same-industry
    // companies), which hardly ever changes.
    static constexpr int kPeersTtlSec = 60 * 60;
    static constexpr int kSuggestedPeersTtlSec = 3 * 24 * 3600;
    QHash<QString, qint64> events_inflight_;

    // Dedicated NAM for third-party NewsAPI.org calls — kept separate from the
    // Fincept-backend HttpClient singleton (which carries base-url/session token).
    QNetworkAccessManager* news_nam_ = nullptr;

    // In-flight guards: the Financials/Valuation tabs and the Sentiment blend all
    // ask for the same symbol at about the same time, and each used to spawn its own
    // Python process. A repeat request while one is running is dropped — the
    // completion signal is broadcast to every listener anyway. Value = start time
    // (ms since epoch); an entry older than kInflightMaxAgeMs is treated as lost so a
    // callback that never fires cannot wedge the symbol forever.
    QHash<QString, qint64> financials_inflight_;
    QHash<QString, qint64> technicals_inflight_;
    // Same folding for the header-bar/Overview/Analysis legs: the screen, the Overview tab
    // and the Analysis tab each ask for the same quote/info/history when a symbol opens.
    QHash<QString, qint64> quote_inflight_;
    QHash<QString, qint64> info_inflight_;
    QHash<QString, qint64> historical_inflight_;
    static constexpr qint64 kInflightMaxAgeMs = 90'000;
};

} // namespace fincept::services::equity
