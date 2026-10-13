#pragma once
#include "services/news/NewsService.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>
#include <optional>

namespace fincept::services {

// ── NER Result Types ────────────────────────────────────────────────────────

struct EntityResult {
    QString id;                                 // article id
    QVector<QPair<QString, QString>> countries; // (name, code)
    QStringList organizations;
    QStringList people;
    QStringList tickers;
};

struct TopEntity {
    QString name;
    int count = 0;
};

struct NerResult {
    QVector<EntityResult> per_article;
    QVector<TopEntity> top_countries; // name = ISO country code
    QVector<TopEntity> top_organizations;
    QVector<TopEntity> top_people;
};

// ── Geolocation Types ───────────────────────────────────────────────────────

struct GeoLocation {
    QString name;
    QString code;
    double lat = 0;
    double lon = 0;
    QString type; // "city" or "country"
};

struct ArticleGeo {
    QString id;
    QVector<GeoLocation> locations;
    double primary_lat = 0;
    double primary_lon = 0;
};

struct InfrastructureItem {
    QString name;
    QString type; // "airport", "military", "power_plant", "port"
    double lat = 0;
    double lon = 0;
    double distance_km = 0;
};

/// NER + geolocation for a set of articles (see NewsNlpService::enrich_articles).
struct NewsEnrichment {
    NerResult ner;               // per_article for every input article; top_* over the set
    QVector<ArticleGeo> geo;     // only articles that mention a known place
};

// ── Sentiment Types ───────────────────────────────────────────────────────────

struct ArticleSentimentResult {
    QString id;
    QString label;           // "BULLISH" | "BEARISH" | "NEUTRAL"
    double score = 0.0;      // -1..1
    double confidence = 0.0; // 0..1
};

struct NewsSentimentResult {
    QString engine;             // "vader" | "lexicon"
    double overall_score = 0.0; // -1..1
    int bullish = 0;
    int bearish = 0;
    int neutral = 0;
    QVector<ArticleSentimentResult> per_article;
};

// ── Service ─────────────────────────────────────────────────────────────────

class NewsNlpService : public QObject {
    Q_OBJECT
  public:
    using EnrichCallback = std::function<void(bool, NewsEnrichment)>;
    using InfraCallback = std::function<void(bool, QVector<InfrastructureItem>)>;
    using TranslateCallback = std::function<void(bool, QString translated, QString detected_lang)>;
    using SentimentCallback = std::function<void(bool, NewsSentimentResult)>;

    static NewsNlpService& instance();

    /// Entities (countries, orgs, people, tickers) + places for `articles`.
    ///
    /// Results are cached per article (id + content hash). Only articles not seen
    /// before are sent to Python — all of them in ONE news_nlp.py `enrich_batch`
    /// process — so a filter change over already-enriched articles costs no process
    /// at all. While a batch is running, a newer request replaces any queued one and
    /// runs when the batch lands (at most one enrichment process at a time).
    void enrich_articles(const QVector<NewsArticle>& articles, EnrichCallback cb);

    /// Cached per-article results (populated by enrich_articles).
    std::optional<EntityResult> cached_entities(const QString& article_id) const;
    /// Cached places for an article; nullopt when unknown or when it names no place.
    std::optional<ArticleGeo> cached_geo(const QString& article_id) const;

    /// Headline sentiment (VADER when available, keyword fallback) via
    /// news_nlp.py. Per-article results are keyed by NewsArticle::id.
    void analyze_sentiment(const QVector<NewsArticle>& articles, SentimentCallback cb);

    /// Find nearby infrastructure for a coordinate.
    void nearby_infrastructure(double lat, double lon, int radius_km, InfraCallback cb);

    /// Translate text to target language.
    void translate_text(const QString& text, const QString& target_lang, TranslateCallback cb);

  private:
    NewsNlpService();

    struct CacheEntry {
        size_t content_hash = 0;
        EntityResult entities;
        ArticleGeo geo; // empty locations = enriched, no place found
    };
    struct PendingEnrich {
        QVector<NewsArticle> articles;
        EnrichCallback cb;
    };

    static size_t content_hash(const NewsArticle& a);
    NewsEnrichment build_enrichment(const QVector<NewsArticle>& articles) const;
    void trim_cache(const QVector<NewsArticle>& keep);
    void run_pending_enrich();

    QHash<QString, CacheEntry> enrich_cache_; // article id -> results
    bool enrich_inflight_ = false;
    qint64 enrich_failed_at_ = 0; // unix secs of the last failed batch; 0 = none
    std::optional<PendingEnrich> pending_enrich_;
};

} // namespace fincept::services
