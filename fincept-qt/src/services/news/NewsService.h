#pragma once
#include <QObject>
#include <QTimer>
#include <QVector>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <functional>
#include <atomic>
#include <cstdint>

namespace fincept::services {

// ── Data types matching Tauri NewsArticle shape ─────────────────────────────

enum class Priority { FLASH, URGENT, BREAKING, ROUTINE };
enum class Sentiment { BULLISH, BEARISH, NEUTRAL };
enum class Impact { HIGH, MEDIUM, LOW };

struct NewsArticle {
    QString   id;
    QString   time;
    Priority  priority = Priority::ROUTINE;
    QString   category;
    QString   headline;
    QString   summary;
    QString   source;
    QString   region;
    Sentiment sentiment = Sentiment::NEUTRAL;
    Impact    impact = Impact::LOW;
    QStringList tickers;
    QString   link;
    int64_t   sort_ts = 0;   // unix seconds
    int       tier = 4;      // 1=wire, 2=major, 3=specialty, 4=blog
};

struct SentimentAnalysis {
    double score = 0;
    double intensity = 0;
    double confidence = 0;
};

struct MarketImpactData {
    QString urgency;     // LOW/MEDIUM/HIGH
    QString prediction;  // negative/neutral/moderate_positive/positive
};

struct RiskSignal {
    QString level;
    QString details;
};

struct NewsAnalysis {
    SentimentAnalysis sentiment;
    MarketImpactData  market_impact;
    QStringList       keywords;
    QStringList       topics;
    QStringList       key_points;
    QString           summary;
    RiskSignal        regulatory;
    RiskSignal        geopolitical;
    RiskSignal        operational;
    RiskSignal        market;
    int               credits_used = 0;
    int               credits_remaining = 0;
};

// ── RSS Feed definition ─────────────────────────────────────────────────────

struct RSSFeed {
    QString id;
    QString name;
    QString url;
    QString category;
    QString region;
    QString source;
    int     tier = 3;
};

// ── Service ─────────────────────────────────────────────────────────────────

class NewsService : public QObject {
    Q_OBJECT
public:
    using ArticlesCallback = std::function<void(bool ok, QVector<NewsArticle>)>;
    using AnalysisCallback = std::function<void(bool ok, NewsAnalysis)>;

    static NewsService& instance();

    void fetch_all_news(bool force, ArticlesCallback cb);
    void analyze_article(const QString& url, AnalysisCallback cb);

    int  feed_count() const { return feed_count_; }
    QStringList active_sources() const { return active_sources_; }

    void set_refresh_interval(int minutes);
    void start_auto_refresh();
    void stop_auto_refresh();

signals:
    void articles_updated(QVector<NewsArticle> articles);
    void analysis_ready(NewsAnalysis analysis);

private:
    NewsService();

    static QVector<RSSFeed> default_feeds();
    static QVector<NewsArticle> parse_rss_xml(const QByteArray& xml, const RSSFeed& feed);
    static void enrich_article(NewsArticle& article);
    static QString strip_html(const QString& html);

    QNetworkAccessManager* nam_ = nullptr;
    QTimer*     refresh_timer_ = nullptr;
    QVector<NewsArticle> cache_;
    int64_t     cache_ts_ = 0;
    int         cache_ttl_sec_ = 600;  // 10 min
    int         feed_count_ = 0;
    QStringList active_sources_;
};

// ── Helpers ─────────────────────────────────────────────────────────────────

QString priority_string(Priority p);
QString sentiment_string(Sentiment s);
QString impact_string(Impact i);
QString priority_color(Priority p);
QString sentiment_color(Sentiment s);
QString relative_time(int64_t unix_ts);

} // namespace fincept::services
