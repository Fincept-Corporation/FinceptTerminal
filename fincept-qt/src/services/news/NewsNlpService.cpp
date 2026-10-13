#include "services/news/NewsNlpService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>

#include <algorithm>

namespace fincept::services {

namespace news_nlp_detail {

/// Bound on remembered per-article results. Beyond it, entries for articles that are
/// not in the current request are dropped (the feed itself holds far fewer).
constexpr int kNlpCacheMax = 4000;
/// After a failed enrichment, don't respawn Python for new articles for this long.
constexpr qint64 kNlpRetrySec = 60;

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

QJsonArray sentiment_payload(const QVector<NewsArticle>& articles) {
    QJsonArray arr;
    for (const auto& a : articles) {
        QJsonObject obj;
        obj["id"] = a.id;
        obj["headline"] = a.headline;
        obj["summary"] = a.summary;
        arr.append(obj);
    }
    return arr;
}

} // namespace news_nlp_detail

NewsNlpService& NewsNlpService::instance() {
    static NewsNlpService s;
    return s;
}

NewsNlpService::NewsNlpService() = default;

size_t NewsNlpService::content_hash(const NewsArticle& a) {
    return qHashMulti(0, a.headline, a.summary);
}

void NewsNlpService::enrich_articles(const QVector<NewsArticle>& articles, EnrichCallback cb) {
    if (enrich_inflight_) {
        // Superseded requests are dropped: only the newest set matters to the caller.
        pending_enrich_ = PendingEnrich{articles, std::move(cb)};
        return;
    }

    QVector<NewsArticle> missing;
    for (const auto& a : articles) {
        if (a.id.isEmpty())
            continue;
        const auto it = enrich_cache_.constFind(a.id);
        if (it == enrich_cache_.cend() || it->content_hash != content_hash(a))
            missing.append(a);
    }
    if (missing.isEmpty()) {
        if (cb)
            cb(true, build_enrichment(articles));
        return;
    }
    if (enrich_failed_at_ > 0 && QDateTime::currentSecsSinceEpoch() - enrich_failed_at_ < news_nlp_detail::kNlpRetrySec) {
        if (cb)
            cb(false, {});
        return;
    }

    // NER needs only the text; keep the payload (argv or @spill file) small.
    QJsonArray arr;
    for (const auto& a : missing)
        arr.append(QJsonObject{{"id", a.id}, {"headline", a.headline}, {"summary", a.summary}});
    const QString json = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));

    enrich_inflight_ = true;
    LOG_DEBUG("NewsNlpService", QString("Enriching %1 new of %2 articles").arg(missing.size()).arg(articles.size()));
    QPointer<NewsNlpService> self = this;
    python::PythonRunner::instance().run(
        "news_nlp.py", {"enrich_batch", json}, [self, articles, missing, cb](python::PythonResult result) {
            if (!self)
                return;
            self->enrich_inflight_ = false;

            bool ok = result.success;
            QString error = result.error;
            if (ok) {
                const QJsonObject obj = QJsonDocument::fromJson(result.output.toUtf8()).object();
                ok = obj.value("success").toBool();
                if (!ok)
                    error = obj.value("error").toString();
                if (ok) {
                    QHash<QString, EntityResult> entities;
                    for (const auto& v : obj.value("entities").toArray()) {
                        const QJsonObject e = v.toObject();
                        EntityResult er;
                        er.id = e.value("id").toString();
                        for (const auto& c : e.value("countries").toArray()) {
                            const QJsonObject co = c.toObject();
                            er.countries.append({co.value("name").toString(), co.value("code").toString()});
                        }
                        for (const auto& o : e.value("organizations").toArray())
                            er.organizations.append(o.toString());
                        for (const auto& p : e.value("people").toArray())
                            er.people.append(p.toString());
                        for (const auto& t : e.value("tickers").toArray())
                            er.tickers.append(t.toString());
                        entities.insert(er.id, er);
                    }
                    QHash<QString, ArticleGeo> places;
                    for (const auto& v : obj.value("geolocated_articles").toArray()) {
                        const QJsonObject g = v.toObject();
                        ArticleGeo ag;
                        ag.id = g.value("id").toString();
                        ag.primary_lat = g.value("primary_lat").toDouble();
                        ag.primary_lon = g.value("primary_lon").toDouble();
                        for (const auto& lv : g.value("locations").toArray()) {
                            const QJsonObject lo = lv.toObject();
                            ag.locations.append({lo.value("name").toString(), lo.value("code").toString(),
                                                 lo.value("lat").toDouble(), lo.value("lon").toDouble(),
                                                 lo.value("type").toString()});
                        }
                        places.insert(ag.id, ag);
                    }
                    // Every article sent is now known — including those with no entity
                    // or place, so they are never re-sent.
                    for (const auto& a : missing) {
                        CacheEntry entry;
                        entry.content_hash = content_hash(a);
                        entry.entities = entities.value(a.id);
                        entry.entities.id = a.id;
                        entry.geo = places.value(a.id);
                        entry.geo.id = a.id;
                        self->enrich_cache_.insert(a.id, entry);
                    }
                    self->trim_cache(articles);
                    self->enrich_failed_at_ = 0;
                }
            }
            if (!ok) {
                self->enrich_failed_at_ = QDateTime::currentSecsSinceEpoch();
                LOG_WARN("NewsNlpService", "Enrichment failed: " + error.left(300));
            }
            if (cb)
                cb(ok, ok ? self->build_enrichment(articles) : NewsEnrichment{});
            self->run_pending_enrich();
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
            countries.add(country.second); // tallied by ISO code, as the script did
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
    auto json_str =
        QString::fromUtf8(QJsonDocument(news_nlp_detail::sentiment_payload(articles)).toJson(QJsonDocument::Compact));

    QPointer<NewsNlpService> self = this;
    python::PythonRunner::instance().run(
        "news_nlp.py", {"analyze_sentiment_batch", json_str}, [self, cb](python::PythonResult result) {
            if (!self)
                return;
            if (!result.success) {
                LOG_WARN("NewsNlpService", "Sentiment analysis failed: " + result.error);
                cb(false, {});
                return;
            }

            auto doc = QJsonDocument::fromJson(result.output.toUtf8());
            auto obj = doc.object();
            if (!obj["success"].toBool()) {
                cb(false, {});
                return;
            }

            NewsSentimentResult res;
            res.engine = obj["engine"].toString();
            res.overall_score = obj["overall_score"].toDouble();

            const auto agg = obj["aggregate"].toObject();
            res.bullish = agg["bullish"].toInt();
            res.bearish = agg["bearish"].toInt();
            res.neutral = agg["neutral"].toInt();

            for (const auto& v : obj["results"].toArray()) {
                auto r = v.toObject();
                ArticleSentimentResult ar;
                ar.id = r["id"].toString();
                ar.label = r["sentiment"].toString();
                ar.score = r["score"].toDouble();
                ar.confidence = r["confidence"].toDouble();
                res.per_article.append(ar);
            }

            cb(true, res);
        });
}

void NewsNlpService::nearby_infrastructure(double lat, double lon, int radius_km, InfraCallback cb) {
    QStringList args = {"nearby_infrastructure", QString::number(lat, 'f', 4), QString::number(lon, 'f', 4),
                        QString::number(radius_km)};

    python::PythonRunner::instance().run("news_geolocation.py", args, [cb](python::PythonResult result) {
        if (!result.success) {
            cb(false, {});
            return;
        }
        auto doc = QJsonDocument::fromJson(result.output.toUtf8());
        auto obj = doc.object();
        if (!obj["success"].toBool()) {
            cb(false, {});
            return;
        }

        QVector<InfrastructureItem> items;
        for (const auto& v : obj["infrastructure"].toArray()) {
            auto i = v.toObject();
            InfrastructureItem item;
            item.name = i["name"].toString();
            item.type = i["type"].toString();
            item.lat = i["lat"].toDouble();
            item.lon = i["lon"].toDouble();
            item.distance_km = i["distance_km"].toDouble();
            items.append(item);
        }
        cb(true, items);
    });
}

void NewsNlpService::translate_text(const QString& text, const QString& target_lang, TranslateCallback cb) {
    python::PythonRunner::instance().run(
        "translate_text.py", {"single", text, "auto", target_lang}, [cb](python::PythonResult result) {
            if (!result.success) {
                cb(false, {}, {});
                return;
            }
            auto doc = QJsonDocument::fromJson(result.output.toUtf8());
            auto obj = doc.object();
            cb(obj["success"].toBool(), obj["translated"].toString(), obj["detected_lang"].toString());
        });
}

} // namespace fincept::services
