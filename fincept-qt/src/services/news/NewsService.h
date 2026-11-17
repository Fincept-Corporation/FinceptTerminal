#pragma once
#include "datahub/Producer.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

class QWebSocket;

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace fincept::services {

// ── Data types ──────────────────────────────────────────────────────────────

enum class Priority { FLASH, URGENT, BREAKING, ROUTINE };
enum class Sentiment { BULLISH, BEARISH, NEUTRAL };
enum class Impact { HIGH, MEDIUM, LOW };
enum class ThreatLevel { CRITICAL, HIGH, MEDIUM, LOW, INFO };

/// Source credibility flags.
enum class SourceFlag {
    NONE = 0,
    STATE_MEDIA = 1, // Government-controlled outlet
    CAUTION = 2,     // Known for sensationalism or low editorial standards
};

struct ThreatClassification {
    ThreatLevel level = ThreatLevel::INFO;
    QString category;      // "conflict", "cyber", "natural", "market", "regulatory", "general"
    double confidence = 0; // 0.0 - 1.0
};

struct NewsArticle {
    QString id;
    QString time;
    Priority priority = Priority::ROUTINE;
    QString category;
    QString headline;
    QString summary;
    QString source;
    QString region;
    Sentiment sentiment = Sentiment::NEUTRAL;
    /// Finance-tuned VADER compound score of headline + summary, -1..1 (0 = neutral or
    /// unknown — rows restored from the database carry only the label).
    double sentiment_score = 0.0;
    Impact impact = Impact::LOW;
    QStringList tickers;
    QString link;
    int64_t sort_ts = 0; // unix seconds
    int tier = 4;        // 1=wire, 2=major, 3=specialty, 4=blog
    ThreatClassification threat;
    SourceFlag source_flag = SourceFlag::NONE;
    QString lang; // ISO language code (e.g., "en", "fr", "ar")

    // ── News analytics (added fields; defaults mean "unknown") ──────────────
    /// `sentiment_score` is a measured value. False when the article's language
    /// has no sentiment model ("not scored") or the row predates persisted
    /// scores (unknown) — such rows must not be averaged as neutral.
    bool sentiment_scored = false;
    /// Market-impact event the headline reports: "RATES", "EARNINGS", "M&A",
    /// "RATING", "MACRO", "POLICY", "CORPORATE", "LEGAL"; empty = none.
    QString event_type;
    /// Confidence 0..1 of each `tickers` entry (same order); empty = unknown
    /// (rows tagged before validated tagging).
    QVector<double> ticker_confidence;
    /// Upper-case words that looked like symbols but matched nothing known —
    /// shown as "unverified", never used as tickers.
    QStringList unverified_tickers;
    /// The feed gave no publication time: `sort_ts` is when the item was first
    /// seen (it only ages the item out of the time window). Undated articles
    /// sort after dated ones and are shown without an age (NewsFeedParse.h).
    bool undated = false;
};

struct SentimentAnalysis {
    double score = 0;
    double intensity = 0;
    double confidence = 0;
};

struct MarketImpactData {
    QString urgency;    // LOW/MEDIUM/HIGH
    QString prediction; // negative/neutral/moderate_positive/positive
};

struct RiskSignal {
    QString level;
    QString details;
};

/// Named entity surfaced by the /news/analyze endpoint. `kind` is one of
/// "organization" / "person" / "location"; `detail` carries the secondary
/// field (ticker for orgs, country_code for locations) when present.
struct AnalysisEntity {
    QString name;
    QString detail;       // ticker (orgs), role (people) or country code (locations); may be empty
    QString sector;       // orgs only
    double sentiment = 0; // orgs only
};

/// Article fetch metadata reported by the analyze endpoint — lets the UI
/// warn when the publisher blocked content and the analysis is metadata-only.
struct AnalysisContent {
    QString headline;
    int word_count = 0;
    QString fetch_note; // e.g. "Article content blocked by publisher (HTTP 401)..."
};

struct NewsAnalysis {
    SentimentAnalysis sentiment;
    MarketImpactData market_impact;
    QStringList keywords;
    QStringList topics;
    QStringList key_points;
    QString summary;
    RiskSignal regulatory;
    RiskSignal geopolitical;
    RiskSignal operational;
    RiskSignal market;
    QVector<AnalysisEntity> organizations;
    QVector<AnalysisEntity> people;
    QVector<AnalysisEntity> locations;
    AnalysisContent content;
    /// What the analysis was based on (`analysis_basis`, newer servers):
    /// "article_page" = the article itself, "supplied_text" = the headline and feed
    /// summary sent with the request (the page could not be read). Empty from older
    /// servers and older cached results.
    QString basis;
    /// The page's declared publication time; empty when the server sent null / none.
    QString published_at;
    /// Fractional (one call can cost 0.35); < 0 = not reported by the server.
    double credits_used = -1;
    double credits_remaining = -1;
    /// Failure detail when the callback reports !ok (e.g. "HTTP_402: ..." for
    /// insufficient credits). Empty on success.
    QString error;
};

// ── RSS Feed definition ─────────────────────────────────────────────────────

struct RSSFeed {
    QString id;
    QString name;
    QString url;
    QString category;
    QString region;
    QString source;
    int tier = 3;
    /// Declared language (ISO 639-1) — the prior for language detection. Empty
    /// for user feeds (detected from the text alone).
    QString lang;
    /// Built-in feeds that are off until the user enables them in the source
    /// manager (non-core regional / local-language outlets).
    bool default_enabled = true;
    /// UTC offset (minutes) of item dates that carry no zone at all — e.g. the
    /// RBI writes Indian wall-clock times with no zone (+330). 0 = read as UTC.
    int naive_utc_offset_min = 0;
};

/// Last outcome of one feed, for the source-health display and the per-feed
/// back-off (a feed that keeps failing is retried less and less often).
struct FeedHealth {
    QString feed_id;
    QString name;
    QString region;
    QString lang;
    qint64 last_ok_at = 0;   ///< unix seconds; 0 = never
    qint64 last_fail_at = 0; ///< unix seconds; 0 = never
    int consecutive_failures = 0;
    QString last_error; ///< e.g. "HTTP 403", "returned an HTML page", "no items"
    int last_http = 0;
    int last_count = 0;      ///< articles in the last successful fetch
    qint64 retry_after = 0;  ///< backing off until (unix seconds); 0 = not backing off
};

// ── Service ─────────────────────────────────────────────────────────────────

/// Phase 5 — DataHub producer for `news:general`, `news:symbol:*`,
/// `news:category:*`, `news:cluster:*`. Existing `articles_updated`
/// / `articles_partial` Qt signals remain live in parallel with hub
/// publishes so consumers can migrate incrementally.
class NewsService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    using ArticlesCallback = std::function<void(bool ok, QVector<NewsArticle>)>;
    using AnalysisCallback = std::function<void(bool ok, NewsAnalysis)>;
    using SummaryCallback = std::function<void(bool ok, QString summary)>;

    static NewsService& instance();

    /// Register with the hub + install news:* policies. Idempotent.
    /// Called from main.cpp after `datahub::register_metatypes()`.
    void ensure_registered_with_hub();

    // ── fincept::datahub::Producer ────────────────────────────────────────
    QStringList topic_patterns() const override;
    /// Refresh splits by prefix: `news:general` → fetch_all_news(true);
    /// `news:symbol:<sym>` / `news:category:<cat>` derive from the
    /// general fetch + filter; `news:cluster:*` is push-only.
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override; // RSS — cap at 2/s

    void fetch_all_news(bool force, ArticlesCallback cb);
    /// POST /news/analyze. `headline` / `text` (the feed item's title and summary) are
    /// sent when non-empty: newer servers analyse them — and say so — when the
    /// article page itself cannot be read, instead of answering 422
    /// article_content_unavailable (nothing charged). Older servers ignore them.
    void analyze_article(const QString& url, AnalysisCallback cb, const QString& headline = {},
                         const QString& text = {});
    /// Transfer timeout of the /news/analyze request (the server may fetch the
    /// article for up to 15 s and spend up to 60 s in the model call). UI guards
    /// should wait at least this long before giving up on a result.
    static constexpr int kAnalyzeTimeoutMs = 90000;

    /// Request body of POST /news/analyze: `url`, plus `headline` / `text` when the
    /// terminal has them, cut to the server's limits (500 / 20 000 characters — a
    /// longer value is a 422 validation error). A summary that only repeats the
    /// headline is not sent as text.
    static QJsonObject analyze_request_body(const QString& url, const QString& headline, const QString& text) {
        const auto cut = [](QString s, int max_chars) {
            s = s.trimmed();
            if (s.size() > max_chars) {
                s.truncate(max_chars);
                if (!s.isEmpty() && s.back().isHighSurrogate())
                    s.chop(1); // never split a surrogate pair
            }
            return s;
        };
        QJsonObject body;
        body["url"] = url;
        const QString h = cut(headline, 500);
        const QString t = cut(text, 20000);
        if (!h.isEmpty())
            body["headline"] = h;
        if (!t.isEmpty() && t.compare(h, Qt::CaseInsensitive) != 0)
            body["text"] = t;
        return body;
    }

    /// Load a previously-persisted analysis for an article URL, if one exists.
    /// Lets the detail panel re-show a prior ANALYZE result on reopen without
    /// hitting the network. Returns nullopt when nothing is cached.
    std::optional<NewsAnalysis> cached_analysis(const QString& url);

    /// Deterministic offline digest of the top N headlines (most important first,
    /// grouped by category). No network, no AI.
    void summarize_headlines(const QVector<NewsArticle>& articles, int count, SummaryCallback cb);
    /// The articles that digest is made of, as records: the first `count`
    /// (1..100) with a headline, most important first (priority, impact, source
    /// tier). The BRIEF view renders these.
    static QVector<NewsArticle> brief_articles(const QVector<NewsArticle>& articles, int count);

    int feed_count() const { return feed_count_; }
    QStringList active_sources() const { return active_sources_; }

    /// Articles stored by earlier fetches (news_articles table) published at or
    /// after `since_ts`, newest first — read on a worker thread, delivered on the
    /// GUI thread. Lets a screen show what it already knows at once while the
    /// feeds are fetched. Also seeds the first-seen times of undated items.
    void load_recent_from_db(qint64 since_ts, int limit, ArticlesCallback cb);

    /// The enabled feed list as of the last fetch (empty before the first one).
    /// GUI thread; no database access — for badges and counters.
    QVector<RSSFeed> effective_feeds_snapshot() const { return effective_feeds_snapshot_; }

    /// Most items one feed contributes per fetch (its newest). Firehose feeds
    /// carry 100-200 items; without a bound one publisher fills the list.
    static constexpr int kMaxItemsPerFeed = 40;

    /// Outcome of every enabled feed's most recent fetch (feeds not fetched yet
    /// are absent). GUI thread.
    QVector<FeedHealth> feed_health() const;
    /// Feeds currently skipped because they kept failing (retried later).
    int feeds_backing_off() const;
    /// Forget a feed's failures so the next refresh fetches it again.
    void reset_feed_health(const QString& feed_id);

    /// Market-impact event a headline reports ("RATES", "EARNINGS", "M&A",
    /// "RATING", "MACRO", "POLICY", "CORPORATE", "LEGAL"), empty when none.
    /// `text` is the lower-cased headline + summary.
    static QString classify_event(const QString& text);

    // ── Feed catalog (built-ins + user overlay from RssFeedRepository) ────
    //
    // `list_effective_feeds` returns the merged, ordered list used by the
    // fetcher: built-ins with any user patches applied + user-added feeds,
    // with disabled rows filtered out.
    //
    // `list_all_feeds_for_editor` returns the same merged list but INCLUDES
    // disabled rows (so the manager UI can show & toggle them), and tags
    // each row with whether it is a built-in default and whether the user
    // has customized it.
    struct EditorFeed {
        RSSFeed feed;
        bool is_builtin = false;    // came from default_feeds()
        bool is_customized = false; // has an overlay row (built-ins only)
        bool enabled = true;
    };
    QVector<RSSFeed> list_effective_feeds() const;
    QVector<EditorFeed> list_all_feeds_for_editor() const;

    /// Earlier URLs of built-in feeds (id -> old URLs). How catalogue updates
    /// reach existing installs: built-ins live in code (default_feeds()), and
    /// the rss_feeds table only holds the user's rows — overlays of built-ins
    /// (written when one is toggled or edited) and the user's own feeds. A
    /// built-in without a row simply follows the code; an overlay row that
    /// still holds one of these copied URLs gets the current built-in URL, so
    /// moved feeds follow without overriding a URL the user typed. Built-ins
    /// dropped from the catalogue disappear (their overlay rows are ignored);
    /// user-added feeds and every enable / disable choice are untouched.
    static const QHash<QString, QStringList>& retired_builtin_urls();

    /// Insert/update a user-added feed. Pass is_builtin=false; new IDs
    /// should be prefixed "usr-" by the caller.
    bool add_user_feed(const RSSFeed& f);
    /// Update fields on an existing feed. For built-ins this writes/updates
    /// an overlay row with is_builtin=true.
    bool update_feed(const RSSFeed& f, bool enabled);
    /// Delete a user feed entirely. For a built-in id this is treated as
    /// "reset to default" (overlay row is deleted; default reappears).
    bool remove_or_reset_feed(const QString& id);
    /// Set the enabled flag. For built-ins, creates an overlay row if none.
    bool set_feed_enabled(const QString& id, bool enabled);

    /// Drop the cached articles list and notify subscribers that the feed
    /// set has changed. Callers should typically trigger a refresh after.
    void reload_feeds();

    void set_refresh_interval(int minutes);
    void start_auto_refresh();
    void stop_auto_refresh();

    void fetch_all_news_progressive(bool force, ArticlesCallback final_cb);

    /// Whether a live-feed server exists to connect to. False until the backend
    /// ships its WebSocket news route; while false connect_live_feed() is a
    /// no-op and the UI shows the feed as unavailable. Single switch:
    /// kNewsLiveFeedAvailable in NewsService_LiveFeed.cpp.
    static bool live_feed_available();

    /// Connect to a WebSocket endpoint for live breaking news push.
    /// If url is empty, uses default. Emits articles_partial on new data.
    /// Gives up (no reconnect loop) when the server refuses the handshake.
    void connect_live_feed(const QString& ws_url = {});
    void disconnect_live_feed();
    bool is_live_connected() const;

    /// Source credibility lookup.
    static SourceFlag source_flag_for(const QString& source);
    static QString source_flag_label(SourceFlag flag);

    /// Threat level helpers.
    static ThreatClassification classify_threat(const NewsArticle& article);
    /// Overload that accepts pre-built lowercased text to avoid redundant allocation.
    static ThreatClassification classify_threat(const NewsArticle& article, const QString& text);
    /// Priority, market impact, event type and threat of `article` from its
    /// headline, summary, tier, validated tags and tone (NewsClassify.h). Run on
    /// fresh articles and again on stored / cached ones, so rows labelled by an
    /// older rule set follow the current rules.
    static void classify_article(NewsArticle& article);

  signals:
    void articles_updated(QVector<NewsArticle> articles);
    void articles_partial(QVector<NewsArticle> articles, int feeds_done, int feeds_total);
    /// Emitted after add/update/remove/set_enabled mutations so the news
    /// screen (or any other consumer) can refresh.
    void feeds_changed();
    /// Emitted when the WebSocket live feed connects or disconnects. UI
    /// consumes this to drive a status badge.
    void live_state_changed(bool connected);
    /// Feed health changed (a fetch round finished or a feed was reset).
    void feed_health_changed();

  private:
    NewsService();

    /// Start the HTTP requests for `feeds` — at most kMaxConcurrentFeedRequests
    /// in flight, each next one starting as another finishes, so a refresh of
    /// ~120 feeds is spread out instead of hitting the network in one burst —
    /// and call `on_done(feed, articles)` on the GUI thread for each, in
    /// completion order. Feeds that answered last time go first. Feeds backing
    /// off after repeated failures are not requested (on_done gets an empty
    /// list for them).
    void run_feed_requests(const QVector<RSSFeed>& feeds,
                           std::function<void(const RSSFeed&, QVector<NewsArticle>)> on_done);
    struct FetchState; // defined in NewsService.cpp
    /// Shared body of fetch_all_news / fetch_all_news_progressive: optional
    /// cache read (worker), feed list (worker), requests, partial snapshots
    /// (progressive only, at most every kPartialEmitIntervalMs), final list.
    void run_fetch(bool force, bool progressive, ArticlesCallback cb);
    void start_network_fetch(bool progressive, ArticlesCallback cb);
    void emit_partial(const std::shared_ptr<FetchState>& state);
    void finish_fetch(const std::shared_ptr<FetchState>& state);
    static constexpr int kPartialEmitIntervalMs = 250;
    struct FeedQueue; // defined in NewsService.cpp
    /// Start queued requests until kMaxConcurrentFeedRequests are in flight.
    void pump_feed_queue(const std::shared_ptr<FeedQueue>& queue);
    /// Whether `feed_id` is backing off at `now` (repeated failures).
    bool feed_backing_off(const QString& feed_id, qint64 now) const;
    /// Record one feed's outcome (GUI thread).
    void note_feed_result(const RSSFeed& feed, bool ok, int http_code, const QString& error, int article_count);
    /// Write the health rows touched since the last call to the database (worker).
    void persist_feed_health();
    /// Load persisted health once (worker), so back-off survives restarts.
    void load_feed_health_once();
    /// Hourly volume rows (category / region / language) for the volume baselines,
    /// computed from the stored articles and upserted. Worker thread only.
    static void record_volume_history();
    /// 10 in flight: a slow publisher (the BoJ answers in ~15 s) no longer holds
    /// back a sixth of the whole refresh.
    static constexpr int kMaxConcurrentFeedRequests = 10;
    QHash<QString, FeedHealth> feed_health_;
    QVector<RSSFeed> effective_feeds_snapshot_;
    QSet<QString> feed_health_dirty_;
    bool feed_health_loaded_ = false;
    bool feed_health_loading_ = false;

    static QVector<RSSFeed> default_feeds();
    /// Parse one feed payload (RSS / RDF / Atom, see NewsFeedParse.h) into
    /// enriched articles: time policy applied, newest kMaxItemsPerFeed kept.
    /// `problem` (optional) receives why nothing came out of a non-empty payload.
    static QVector<NewsArticle> parse_rss_xml(const QByteArray& xml, const RSSFeed& feed, QString* problem = nullptr);
    /// Remember when undated articles were first seen (worker-safe).
    static void remember_first_seen(const QVector<NewsArticle>& articles);
    static void enrich_article(NewsArticle& article);
    static QString strip_html(const QString& html);

    /// Deterministic article id. Hash of the (fragment/trailing-slash
    /// normalised) link, falling back to source + headline when the item has no
    /// link, so the same story keeps the same id across refreshes and across
    /// feeds. Ids used to embed the fetch timestamp, which made every refresh
    /// look like a brand-new set of articles — breaking seen/bookmark state,
    /// notification dedup and the history merge.
    static QString stable_article_id(const QString& link, const QString& source, const QString& headline);
    /// Feed-supplied links are untrusted: only http(s) URLs are ever kept.
    static bool is_web_url(const QString& url);

    /// JSON round-trip used for the CacheManager copy of the article list.
    /// Deserialising re-derives threat + source flag (not stored in the cache).
    static QString serialize_articles(const QVector<NewsArticle>& articles);
    static QVector<NewsArticle> deserialize_articles(const QString& json);

    /// Append `incoming` to `all`, skipping ids already recorded in `seen_ids`.
    static void merge_unique(QVector<NewsArticle>& all, QSet<QString>& seen_ids,
                             const QVector<NewsArticle>& incoming);

    /// Write a freshly fetched list to the news_articles table on a worker
    /// thread (history, bookmarks, seen state and FTS all read from it).
    void persist_articles_async(const QVector<NewsArticle>& articles);

    /// Worker-thread half of one feed fetch: shape check + RSS/Atom parse
    /// (with enrichment / threat classification) of the reply payload the GUI
    /// thread copied out. Pure — touches no service state.
    static QVector<NewsArticle> parse_feed_payload(const QByteArray& data, int http_code, bool ok,
                                                   const QString& error, const RSSFeed& feed,
                                                   QString* problem = nullptr);

    /// Serialise `articles` into the "news:articles" cache entry on a worker
    /// thread (CacheDatabase gives each thread its own connection). A write that
    /// a newer list superseded before it landed is dropped.
    static void store_cache_async(const QVector<NewsArticle>& articles);

    QNetworkAccessManager* nam_ = nullptr;
    QTimer* refresh_timer_ = nullptr;
    static constexpr int kArticleCacheTtlSec = 600; // 10 min
    int feed_count_ = 0;
    QStringList active_sources_;

    /// Last complete article list (RSS + live pushes). Lets a live push extend
    /// the current list directly instead of depending on the cache TTL.
    QVector<NewsArticle> latest_articles_;

    // WebSocket live feed
    QWebSocket* live_ws_ = nullptr;
    bool live_connected_ = false;
    int live_reconnect_attempts_ = 0;
    /// Bumped on every connect/disconnect so a reconnect timer armed for an
    /// earlier socket becomes a no-op instead of re-opening a dropped one.
    quint64 live_epoch_ = 0;
    quint64 live_reconnect_token_ = 0; ///< only the latest armed reconnect timer may fire

    /// Publish `news:general` + fan out `news:symbol:<sym>` and
    /// `news:category:<cat>` derived slices. Called from both the
    /// progressive partial-snapshot path and the final fetch path —
    /// subscribers see the same accumulated list each time, so the
    /// progressive-publish pattern (see DATAHUB_ARCHITECTURE.md §11)
    /// collapses to a series of full-list republishes.
    void publish_articles_to_hub(const QVector<NewsArticle>& accumulated);
    bool hub_registered_ = false;
};

// ── Helpers ─────────────────────────────────────────────────────────────────

QString priority_string(Priority p);
QString sentiment_string(Sentiment s);
QString impact_string(Impact i);
// The *_color() helpers return the active theme's colour, read on every call.
QString priority_color(Priority p);
QString sentiment_color(Sentiment s);
QString relative_time(int64_t unix_ts);
QString threat_level_string(ThreatLevel t);
QString threat_level_color(ThreatLevel t);

// Round-trip helpers (string → enum)
Priority priority_from_string(const QString& s);
Sentiment sentiment_from_string(const QString& s);
Impact impact_from_string(const QString& s);

} // namespace fincept::services

#include <QMetaType>
Q_DECLARE_METATYPE(fincept::services::NewsArticle)
Q_DECLARE_METATYPE(QVector<fincept::services::NewsArticle>)
