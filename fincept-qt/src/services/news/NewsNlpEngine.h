#pragma once
// News NLP rules: named entities, gazetteer geolocation, finance-tuned headline
// sentiment, and the request/response handling of the translation (Google gtx) and
// nearby-infrastructure (OpenStreetMap Overpass) lookups. Pure functions over shared
// read-only tables — safe to call from any thread. NewsNlpService owns caching,
// threading and the network calls.

#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace fincept::services {

// ── Result types ────────────────────────────────────────────────────────────

struct EntityResult {
    QString id;                                 // article id
    QVector<QPair<QString, QString>> countries; // (name, code)
    QStringList organizations;
    QStringList people;
    QStringList tickers;
};

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

namespace news_nlp {

/// The text of one article, as the rules read it ("<headline> <summary>").
struct TextItem {
    QString id;
    QString headline;
    QString summary;
};

/// Countries (max 5, deduplicated by ISO code), organisations, people and ticker
/// candidates (max 5 each) named in the article.
EntityResult extract_entities(const TextItem& item);

/// Known cities, then countries with a centroid, the article names (max 5 kept; the
/// primary coordinate is the first). nullopt when it names no known place.
std::optional<ArticleGeo> geocode(const TextItem& item);

/// Headline sentiment: VADER with a finance lexicon on top (engine "vader"), or the
/// keyword scorer alone if the VADER lexicon resource is unavailable (engine "lexicon").
NewsSentimentResult analyze_sentiment(const QVector<TextItem>& items);

// ── Translation ─────────────────────────────────────────────────────────────

/// Script-range guess — "zh" "ja" "ko" "ar" "ru" "hi", else "en" (every Latin script).
QString detect_language(const QString& text);

struct Translation {
    bool ok = false;
    QString translated;
    QString detected_lang;
};

/// The outcomes that need no request: blank text, source == target, or a non-Latin
/// script that already is the target language. nullopt: ask the translator.
std::optional<Translation> translate_without_request(const QString& text, const QString& source, const QString& target);

/// Google's keyless web endpoint (client=gtx); the text goes in the form body as "q".
QString gtx_translate_url(const QString& source, const QString& target);

/// (translated text, detected source language) from a gtx reply; nullopt if malformed.
std::optional<QPair<QString, QString>> parse_gtx_response(const QJsonDocument& doc);

// ── Nearby infrastructure ───────────────────────────────────────────────────

/// Overpass QL for airports, military sites, power plants and harbours within
/// `radius_km` of the point (sent to overpass-api.de as the form field "data").
QString overpass_query(double lat, double lon, qint64 radius_km);

/// Up to 15 elements nearest first, each classified and with its haversine distance
/// from (lat, lon); nullopt if the reply is not an Overpass result.
std::optional<QVector<InfrastructureItem>> parse_overpass_response(const QJsonDocument& doc, double lat, double lon);

} // namespace news_nlp
} // namespace fincept::services
