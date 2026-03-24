#pragma once
#include "services/news/NewsService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

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
    QVector<TopEntity> top_countries;
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

// ── Service ─────────────────────────────────────────────────────────────────

class NewsNlpService : public QObject {
    Q_OBJECT
  public:
    using EntitiesCallback = std::function<void(bool, NerResult)>;
    using GeoCallback = std::function<void(bool, QVector<ArticleGeo>)>;
    using InfraCallback = std::function<void(bool, QVector<InfrastructureItem>)>;
    using TranslateCallback = std::function<void(bool, QString translated, QString detected_lang)>;
    using SemanticClustersCallback = std::function<void(bool, QJsonArray clusters)>;

    static NewsNlpService& instance();

    /// Extract entities (countries, orgs, people, tickers) from articles.
    void extract_entities(const QVector<NewsArticle>& articles, EntitiesCallback cb);

    /// Semantic clustering via TF-IDF cosine similarity.
    void cluster_semantic(const QVector<NewsArticle>& articles, SemanticClustersCallback cb);

    /// Geolocate articles — extract locations and return coordinates.
    void geolocate_articles(const QVector<NewsArticle>& articles, GeoCallback cb);

    /// Find nearby infrastructure for a coordinate.
    void nearby_infrastructure(double lat, double lon, int radius_km, InfraCallback cb);

    /// Translate text to target language.
    void translate_text(const QString& text, const QString& target_lang, TranslateCallback cb);

    /// Cached NER results (populated after extract_entities completes).
    const NerResult& cached_ner() const { return ner_cache_; }
    const QVector<ArticleGeo>& cached_geo() const { return geo_cache_; }

  private:
    NewsNlpService();

    QJsonArray articles_to_json(const QVector<NewsArticle>& articles) const;

    NerResult ner_cache_;
    QVector<ArticleGeo> geo_cache_;
};

} // namespace fincept::services
