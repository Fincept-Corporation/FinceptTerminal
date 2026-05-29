#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

#    include "datahub/Producer.h"

class QWebSocket;

#include <atomic>
#include <cstdint>
#include <functional>
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
    Impact impact = Impact::LOW;
    QStringList tickers;
    QString link;
    int64_t sort_ts = 0; // unix seconds
    int tier = 4;        // 1=wire, 2=major, 3=specialty, 4=blog
    ThreatClassification threat;
    SourceFlag source_flag = SourceFlag::NONE;
    QString lang; // ISO language code (e.g., "en", "fr", "ar")
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
    QString detail;        // ticker (orgs) or country code (locations); may be empty
    QString sector;        // orgs only
    double sentiment = 0;  // orgs only
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
    int credits_used = 0;
    int credits_remaining = 0;
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
};

// ── AI Summarization ────────────────────────────────────────────────────────

struct HeadlineSummary {
    QString summary;
    int64_t cached_at = 0;
    QString headline_signature; // hash of headlines used to generate
};

// ── Service ─────────────────────────────────────────────────────────────────

/// Phase 5 — DataHub producer for `news:general`, `news:symbol:*`,
/// `news:category:*`, `news:cluster:*`. Existing `articles_updated`
/// / `articles_partial` Qt signals remain live in parallel with hub
/// publishes so consumers can migrate incrementally.
class NewsService : public QObject
    , public fincept::datahub::Producer
{
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
    int max_requests_per_sec() const override;  // RSS — cap at 2/s

    void fetch_all_news(bool force, ArticlesCallback cb);
    void analyze_article(const QString& url, AnalysisCallback cb);

    /// Load a previously-persisted analysis for an article URL, if one exists.
    /// Lets the detail panel re-show a prior ANALYZE result on reopen without
    /// hitting the network. Returns nullopt when nothing is cached.
    std::optional<NewsAnalysis> cached_analysis(const QString& url);

    /// Summarize top N headlines via AI. Cached for 10 min per headline signature.
    void summarize_headlines(const QVector<NewsArticle>& articles, int count, SummaryCallback cb);

    int feed_count() const { return feed_count_; }
    QStringList active_sources() const { return active_sources_; }

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
        bool is_builtin = false;     // came from default_feeds()
        bool is_customized = false;  // has an overlay row (built-ins only)
        bool enabled = true;
    };
    QVector<RSSFeed> list_effective_feeds() const;
    QVector<EditorFeed> list_all_feeds_for_editor() const;

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

    /// Connect to a WebSocket endpoint for live breaking news push.
    /// If url is empty, uses default. Emits articles_partial on new data.
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

  signals:
    void articles_updated(QVector<NewsArticle> articles);
    void articles_partial(QVector<NewsArticle> articles, int feeds_done, int feeds_total);
    /// Emitted after add/update/remove/set_enabled mutations so the news
    /// screen (or any other consumer) can refresh.
    void feeds_changed();
    /// Emitted when the WebSocket live feed connects or disconnects. UI
    /// consumes this to drive a status badge.
    void live_state_changed(bool connected);

  private:
    NewsService();

    static QVector<RSSFeed> default_feeds();
    static QVector<NewsArticle> parse_rss_xml(const QByteArray& xml, const RSSFeed& feed);
    static void enrich_article(NewsArticle& article);
    static QString strip_html(const QString& html);

    QNetworkAccessManager* nam_ = nullptr;
    QTimer* refresh_timer_ = nullptr;
    static constexpr int kArticleCacheTtlSec = 600; // 10 min
    static constexpr int kSummaryCacheTtlSec = 600;
    int feed_count_ = 0;
    QStringList active_sources_;

    // WebSocket live feed
    QWebSocket* live_ws_ = nullptr;
    bool live_connected_ = false;

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
