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

class QWebSocket;

#include <atomic>
#include <cstdint>
#include <functional>

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

class NewsService : public QObject {
    Q_OBJECT
  public:
    using ArticlesCallback = std::function<void(bool ok, QVector<NewsArticle>)>;
    using AnalysisCallback = std::function<void(bool ok, NewsAnalysis)>;
    using SummaryCallback = std::function<void(bool ok, QString summary)>;

    static NewsService& instance();

    void fetch_all_news(bool force, ArticlesCallback cb);
    void analyze_article(const QString& url, AnalysisCallback cb);

    /// Summarize top N headlines via AI. Cached for 10 min per headline signature.
    void summarize_headlines(const QVector<NewsArticle>& articles, int count, SummaryCallback cb);

    int feed_count() const { return feed_count_; }
    QStringList active_sources() const { return active_sources_; }

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

  signals:
    void articles_updated(QVector<NewsArticle> articles);
    void analysis_ready(NewsAnalysis analysis);
    void articles_partial(QVector<NewsArticle> articles, int feeds_done, int feeds_total);

  private:
    NewsService();

    static QVector<RSSFeed> default_feeds();
    static QVector<NewsArticle> parse_rss_xml(const QByteArray& xml, const RSSFeed& feed);
    static void enrich_article(NewsArticle& article);
    static QString strip_html(const QString& html);

    QNetworkAccessManager* nam_ = nullptr;
    QTimer* refresh_timer_ = nullptr;
    QVector<NewsArticle> cache_;
    int64_t cache_ts_ = 0;
    int cache_ttl_sec_ = 600; // 10 min
    int feed_count_ = 0;
    QStringList active_sources_;

    // Summary cache
    HeadlineSummary summary_cache_;
    static constexpr int SUMMARY_CACHE_TTL_SEC = 600;

    // WebSocket live feed
    QWebSocket* live_ws_ = nullptr;
    bool live_connected_ = false;
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

} // namespace fincept::services
