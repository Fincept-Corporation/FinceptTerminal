#pragma once
#include "services/news/NewsNlpEngine.h"
#include "services/news/NewsService.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>
#include <optional>

namespace fincept::services {

// EntityResult, GeoLocation, ArticleGeo, InfrastructureItem and the sentiment result
// types are declared in NewsNlpEngine.h.

// ── NER aggregates ──────────────────────────────────────────────────────────

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

/// NER + geolocation for a set of articles (see NewsNlpService::enrich_articles).
struct NewsEnrichment {
    NerResult ner;               // per_article for every input article; top_* over the set
    QVector<ArticleGeo> geo;     // only articles that mention a known place
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
    /// before are processed — all of them in one batch on a worker thread — so a
    /// filter change over already-enriched articles answers synchronously from the
    /// cache. While a batch is running, a newer request replaces any queued one and
    /// runs when the batch lands (at most one batch at a time).
    void enrich_articles(const QVector<NewsArticle>& articles, EnrichCallback cb);

    /// Cached per-article results (populated by enrich_articles).
    std::optional<EntityResult> cached_entities(const QString& article_id) const;
    /// Cached places for an article; nullopt when unknown or when it names no place.
    std::optional<ArticleGeo> cached_geo(const QString& article_id) const;

    /// Headline sentiment (finance-tuned VADER) computed on a worker thread; the
    /// callback runs on this object's thread. Per-article results are keyed by
    /// NewsArticle::id.
    void analyze_sentiment(const QVector<NewsArticle>& articles, SentimentCallback cb);

    /// Find nearby infrastructure for a coordinate (OpenStreetMap Overpass).
    void nearby_infrastructure(double lat, double lon, int radius_km, InfraCallback cb);

    /// Translate text to target language (Google's keyless gtx endpoint).
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

    static size_t content_hash(const QString& headline, const QString& summary);
    NewsEnrichment build_enrichment(const QVector<NewsArticle>& articles) const;
    void trim_cache(const QVector<NewsArticle>& keep);
    void run_pending_enrich();

    QHash<QString, CacheEntry> enrich_cache_; // article id -> results
    bool enrich_inflight_ = false;
    std::optional<PendingEnrich> pending_enrich_;
};

} // namespace fincept::services
