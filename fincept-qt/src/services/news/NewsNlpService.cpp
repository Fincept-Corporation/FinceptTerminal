#include "services/news/NewsNlpService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>

namespace fincept::services {

NewsNlpService& NewsNlpService::instance() {
    static NewsNlpService s;
    return s;
}

NewsNlpService::NewsNlpService() = default;

QJsonArray NewsNlpService::articles_to_json(const QVector<NewsArticle>& articles) const {
    QJsonArray arr;
    for (const auto& a : articles) {
        QJsonObject obj;
        obj["id"] = a.id;
        obj["headline"] = a.headline;
        obj["summary"] = a.summary;
        obj["source"] = a.source;
        obj["category"] = a.category;
        obj["region"] = a.region;
        obj["sort_ts"] = static_cast<qint64>(a.sort_ts);
        arr.append(obj);
    }
    return arr;
}

void NewsNlpService::extract_entities(const QVector<NewsArticle>& articles, EntitiesCallback cb) {
    auto json_str = QString::fromUtf8(QJsonDocument(articles_to_json(articles)).toJson(QJsonDocument::Compact));

    QPointer<NewsNlpService> self = this;
    python::PythonRunner::instance().run(
        "news_nlp.py", {"extract_entities", json_str}, [self, cb](python::PythonResult result) {
            if (!self)
                return;
            if (!result.success) {
                LOG_WARN("NewsNlpService", "Entity extraction failed: " + result.error);
                cb(false, {});
                return;
            }

            auto doc = QJsonDocument::fromJson(result.output.toUtf8());
            auto obj = doc.object();
            if (!obj["success"].toBool()) {
                cb(false, {});
                return;
            }

            NerResult ner;

            // Per-article entities
            for (const auto& v : obj["entities"].toArray()) {
                auto e = v.toObject();
                EntityResult er;
                er.id = e["id"].toString();
                for (const auto& c : e["countries"].toArray()) {
                    auto co = c.toObject();
                    er.countries.append({co["name"].toString(), co["code"].toString()});
                }
                for (const auto& o : e["organizations"].toArray())
                    er.organizations.append(o.toString());
                for (const auto& p : e["people"].toArray())
                    er.people.append(p.toString());
                for (const auto& t : e["tickers"].toArray())
                    er.tickers.append(t.toString());
                ner.per_article.append(er);
            }

            // Top entities
            for (const auto& v : obj["top_countries"].toArray()) {
                auto o = v.toObject();
                ner.top_countries.append({o["code"].toString(), o["count"].toInt()});
            }
            for (const auto& v : obj["top_organizations"].toArray()) {
                auto o = v.toObject();
                ner.top_organizations.append({o["name"].toString(), o["count"].toInt()});
            }
            for (const auto& v : obj["top_people"].toArray()) {
                auto o = v.toObject();
                ner.top_people.append({o["name"].toString(), o["count"].toInt()});
            }

            self->ner_cache_ = ner;
            cb(true, ner);
        });
}

void NewsNlpService::cluster_semantic(const QVector<NewsArticle>& articles, SemanticClustersCallback cb) {
    auto json_str = QString::fromUtf8(QJsonDocument(articles_to_json(articles)).toJson(QJsonDocument::Compact));

    QPointer<NewsNlpService> self = this;
    python::PythonRunner::instance().run(
        "news_nlp.py", {"cluster_semantic", json_str}, [self, cb](python::PythonResult result) {
            if (!self)
                return;
            if (!result.success) {
                LOG_WARN("NewsNlpService", "Semantic clustering failed: " + result.error);
                cb(false, {});
                return;
            }
            auto doc = QJsonDocument::fromJson(result.output.toUtf8());
            auto obj = doc.object();
            cb(obj["success"].toBool(), obj["clusters"].toArray());
        });
}

void NewsNlpService::geolocate_articles(const QVector<NewsArticle>& articles, GeoCallback cb) {
    auto json_str = QString::fromUtf8(QJsonDocument(articles_to_json(articles)).toJson(QJsonDocument::Compact));

    QPointer<NewsNlpService> self = this;
    python::PythonRunner::instance().run("news_geolocation.py", {"extract_and_geocode", json_str},
                                         [self, cb](python::PythonResult result) {
                                             if (!self)
                                                 return;
                                             if (!result.success) {
                                                 LOG_WARN("NewsNlpService", "Geolocation failed: " + result.error);
                                                 cb(false, {});
                                                 return;
                                             }

                                             auto doc = QJsonDocument::fromJson(result.output.toUtf8());
                                             auto obj = doc.object();
                                             if (!obj["success"].toBool()) {
                                                 cb(false, {});
                                                 return;
                                             }

                                             QVector<ArticleGeo> results;
                                             for (const auto& v : obj["geolocated_articles"].toArray()) {
                                                 auto a = v.toObject();
                                                 ArticleGeo ag;
                                                 ag.id = a["id"].toString();
                                                 ag.primary_lat = a["primary_lat"].toDouble();
                                                 ag.primary_lon = a["primary_lon"].toDouble();
                                                 for (const auto& lv : a["locations"].toArray()) {
                                                     auto lo = lv.toObject();
                                                     GeoLocation gl;
                                                     gl.name = lo["name"].toString();
                                                     gl.code = lo["code"].toString();
                                                     gl.lat = lo["lat"].toDouble();
                                                     gl.lon = lo["lon"].toDouble();
                                                     gl.type = lo["type"].toString();
                                                     ag.locations.append(gl);
                                                 }
                                                 results.append(ag);
                                             }

                                             self->geo_cache_ = results;
                                             cb(true, results);
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
