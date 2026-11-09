#include "services/news/NewsNlpService.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"

#include <QMetaObject>
#include <QPointer>
#include <QSet>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace fincept::services {

namespace news_nlp_detail {

/// Bound on remembered per-article results. Beyond it, entries for articles that are
/// not in the current request are dropped (the feed itself holds far fewer).
constexpr int kNlpCacheMax = 4000;

constexpr const char* kOverpassInterpreterUrl = "https://overpass-api.de/api/interpreter";

/// Insertion-ordered tally — ties keep first-seen order, like Python's Counter.
struct Tally {
    QHash<QString, QPair<int, int>> by_key; // key -> (count, first-seen index)
    int next_index = 0;
    void add(const QString& key) {
        if (key.isEmpty())
            return;
        auto it = by_key.find(key);
        if (it == by_key.end())
            by_key.insert(key, {1, next_index++});
        else
            ++it->first;
    }
    QVector<TopEntity> top(int n) const {
        QVector<QString> keys = by_key.keys().toVector();
        std::sort(keys.begin(), keys.end(), [this](const QString& a, const QString& b) {
            const auto& va = by_key[a];
            const auto& vb = by_key[b];
            if (va.first != vb.first)
                return va.first > vb.first;
            return va.second < vb.second;
        });
        QVector<TopEntity> out;
        for (int i = 0; i < std::min<qsizetype>(n, keys.size()); ++i)
            out.append({keys[i], by_key[keys[i]].first});
        return out;
    }
};

QVector<news_nlp::TextItem> text_items(const QVector<NewsArticle>& articles) {
    QVector<news_nlp::TextItem> items;
    items.reserve(articles.size());
    for (const auto& a : articles)
        items.append({a.id, a.headline, a.summary});
    return items;
}

} // namespace news_nlp_detail

NewsNlpService& NewsNlpService::instance() {
    static NewsNlpService s;
    return s;
}

NewsNlpService::NewsNlpService() = default;

size_t NewsNlpService::content_hash(const QString& headline, const QString& summary) {
    return qHashMulti(0, headline, summary);
}

void NewsNlpService::enrich_articles(const QVector<NewsArticle>& articles, EnrichCallback cb) {
    if (enrich_inflight_) {
        // Superseded requests are dropped: only the newest set matters to the caller.
        pending_enrich_ = PendingEnrich{articles, std::move(cb)};
        return;
    }

    QVector<news_nlp::TextItem> missing;
    for (const auto& a : articles) {
        if (a.id.isEmpty())
            continue;
        const auto it = enrich_cache_.constFind(a.id);
        if (it == enrich_cache_.cend() || it->content_hash != content_hash(a.headline, a.summary))
            missing.append({a.id, a.headline, a.summary});
    }
    if (missing.isEmpty()) {
        if (cb)
            cb(true, build_enrichment(articles));
        return;
    }

    enrich_inflight_ = true;
    LOG_DEBUG("NewsNlpService", QString("Enriching %1 new of %2 articles").arg(missing.size()).arg(articles.size()));
    QPointer<NewsNlpService> self = this;
    (void)QtConcurrent::run([self, articles, missing, cb]() {
        QVector<CacheEntry> entries;
        entries.reserve(missing.size());
        for (const auto& item : missing) {
            CacheEntry entry;
            entry.content_hash = content_hash(item.headline, item.summary);
            entry.entities = news_nlp::extract_entities(item);
            entry.geo = news_nlp::geocode(item).value_or(ArticleGeo{});
            entry.geo.id = item.id;
            entries.append(entry);
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, articles, missing, cb, entries = std::move(entries)]() {
                if (!self)
                    return;
                self->enrich_inflight_ = false;
                // Every article processed is now known — including those with no
                // entity or place, so they are never processed again.
                for (qsizetype i = 0; i < missing.size(); ++i)
                    self->enrich_cache_.insert(missing[i].id, entries[i]);
                self->trim_cache(articles);
                if (cb)
                    cb(true, self->build_enrichment(articles));
                self->run_pending_enrich();
            },
            Qt::QueuedConnection);
    });
}

void NewsNlpService::run_pending_enrich() {
    if (!pending_enrich_)
        return;
    PendingEnrich next = std::move(*pending_enrich_);
    pending_enrich_.reset();
    enrich_articles(next.articles, std::move(next.cb));
}

NewsEnrichment NewsNlpService::build_enrichment(const QVector<NewsArticle>& articles) const {
    NewsEnrichment out;
    news_nlp_detail::Tally countries, orgs, people;
    out.ner.per_article.reserve(articles.size());
    for (const auto& a : articles) {
        const auto it = enrich_cache_.constFind(a.id);
        if (it == enrich_cache_.cend())
            continue;
        out.ner.per_article.append(it->entities);
        for (const auto& country : it->entities.countries)
            countries.add(country.second); // tallied by ISO code
        for (const auto& org : it->entities.organizations)
            orgs.add(org);
        for (const auto& person : it->entities.people)
            people.add(person);
        if (!it->geo.locations.isEmpty())
            out.geo.append(it->geo);
    }
    out.ner.top_countries = countries.top(10);
    out.ner.top_organizations = orgs.top(10);
    out.ner.top_people = people.top(10);
    return out;
}

void NewsNlpService::trim_cache(const QVector<NewsArticle>& keep) {
    if (enrich_cache_.size() <= news_nlp_detail::kNlpCacheMax)
        return;
    QSet<QString> keep_ids;
    keep_ids.reserve(keep.size());
    for (const auto& a : keep)
        keep_ids.insert(a.id);
    for (auto it = enrich_cache_.begin(); it != enrich_cache_.end();) {
        if (keep_ids.contains(it.key()))
            ++it;
        else
            it = enrich_cache_.erase(it);
    }
}

std::optional<EntityResult> NewsNlpService::cached_entities(const QString& article_id) const {
    const auto it = enrich_cache_.constFind(article_id);
    if (it == enrich_cache_.cend())
        return std::nullopt;
    return it->entities;
}

std::optional<ArticleGeo> NewsNlpService::cached_geo(const QString& article_id) const {
    const auto it = enrich_cache_.constFind(article_id);
    if (it == enrich_cache_.cend() || it->geo.locations.isEmpty())
        return std::nullopt;
    return it->geo;
}

void NewsNlpService::analyze_sentiment(const QVector<NewsArticle>& articles, SentimentCallback cb) {
    // The first call also loads the VADER lexicon (~7.5k entries) — off the UI thread.
    QPointer<NewsNlpService> self = this;
    (void)QtConcurrent::run([self, items = news_nlp_detail::text_items(articles), cb]() {
        NewsSentimentResult result = news_nlp::analyze_sentiment(items);
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, cb, result = std::move(result)]() {
                if (!self)
                    return;
                if (result.engine != QLatin1String("vader"))
                    LOG_WARN("NewsNlpService", "VADER lexicon resource unavailable - keyword sentiment only");
                if (cb)
                    cb(true, result);
            },
            Qt::QueuedConnection);
    });
}

void NewsNlpService::nearby_infrastructure(double lat, double lon, int radius_km, InfraCallback cb) {
    // The point is rounded to 4 decimals (~11 m), as the query has always used it.
    const double qlat = QString::number(lat, 'f', 4).toDouble();
    const double qlon = QString::number(lon, 'f', 4).toDouble();
    HttpClient::instance().post_form(
        QString::fromLatin1(news_nlp_detail::kOverpassInterpreterUrl),
        {{QStringLiteral("data"), news_nlp::overpass_query(qlat, qlon, radius_km)}},
        [cb, qlat, qlon](Result<QJsonDocument> result) {
            if (result.is_err()) {
                LOG_WARN("NewsNlpService", "Overpass query failed: " + QString::fromStdString(result.error()));
                cb(false, {});
                return;
            }
            const auto items = news_nlp::parse_overpass_response(result.value(), qlat, qlon);
            if (!items) {
                LOG_WARN("NewsNlpService", "Overpass reply is not an Overpass result");
                cb(false, {});
                return;
            }
            cb(true, *items);
        },
        this);
}

void NewsNlpService::translate_text(const QString& text, const QString& target_lang, TranslateCallback cb) {
    const QString source = QStringLiteral("auto");
    if (const auto done = news_nlp::translate_without_request(text, source, target_lang)) {
        // Still answered asynchronously, like every other path of this method.
        QMetaObject::invokeMethod(
            this, [cb, outcome = *done]() { cb(outcome.ok, outcome.translated, outcome.detected_lang); },
            Qt::QueuedConnection);
        return;
    }
    const QString detected = news_nlp::detect_language(text);
    HttpClient::Headers headers;
    headers.insert("User-Agent", "Mozilla/5.0");
    HttpClient::instance().post_form(
        news_nlp::gtx_translate_url(source, target_lang), {{QStringLiteral("q"), text}},
        [cb, text, detected](Result<QJsonDocument> result) {
            const auto parsed = result.is_ok() ? news_nlp::parse_gtx_response(result.value())
                                               : std::optional<QPair<QString, QString>>();
            if (!parsed) {
                LOG_WARN("NewsNlpService",
                         "Translation request failed: " + (result.is_err() ? QString::fromStdString(result.error())
                                                                           : QStringLiteral("unexpected reply")));
                cb(false, text, detected);
                return;
            }
            cb(true, parsed->first.isEmpty() ? text : parsed->first,
               parsed->second.isEmpty() ? detected : parsed->second);
        },
        this, headers);
}

} // namespace fincept::services
