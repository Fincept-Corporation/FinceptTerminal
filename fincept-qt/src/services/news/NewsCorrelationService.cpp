#include "services/news/NewsCorrelationService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>

namespace fincept::services {

NewsCorrelationService& NewsCorrelationService::instance() {
    static NewsCorrelationService s;
    return s;
}

NewsCorrelationService::NewsCorrelationService() = default;

void NewsCorrelationService::detect_signals(const QVector<NewsArticle>& articles, SignalsCallback cb) {
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
    auto json_str = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));

    QPointer<NewsCorrelationService> self = this;
    python::PythonRunner::instance().run("news_correlation.py", {"detect_signals", json_str},
        [self, cb](python::PythonResult result) {
            if (!self) return;
            if (!result.success) {
                LOG_WARN("NewsCorrelation", "Signal detection failed: " + result.error);
                cb(false, {});
                return;
            }

            auto doc = QJsonDocument::fromJson(result.output.toUtf8());
            auto obj = doc.object();
            if (!obj["success"].toBool()) {
                cb(false, {});
                return;
            }

            QVector<CorrelationSignal> detected;
            for (const auto& v : obj["signals"].toArray()) {
                auto s = v.toObject();
                CorrelationSignal sig;
                sig.type = s["type"].toString();
                sig.category = s["category"].toString();
                sig.detail = s["detail"].toString();
                sig.severity = s["severity"].toString();
                sig.value = s["value"].toDouble();
                for (const auto& src : s["sources"].toArray())
                    sig.sources.append(src.toString());
                detected.append(sig);
            }

            self->signals_cache_ = detected;
            cb(true, detected);
        });
}

void NewsCorrelationService::compute_instability(const QString& country_code,
                                                   const QVector<CorrelationSignal>& sigs,
                                                   InstabilityCallback cb) {
    QJsonArray sig_arr;
    for (const auto& s : sigs) {
        QJsonObject obj;
        obj["type"] = s.type;
        obj["category"] = s.category;
        obj["severity"] = s.severity;
        obj["value"] = s.value;
        sig_arr.append(obj);
    }
    auto sig_json = QString::fromUtf8(QJsonDocument(sig_arr).toJson(QJsonDocument::Compact));

    python::PythonRunner::instance().run("news_correlation.py",
        {"compute_instability", country_code, sig_json},
        [cb](python::PythonResult result) {
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

            InstabilityScore score;
            score.country = obj["country"].toString();
            score.cii_score = obj["cii_score"].toInt();
            score.level = obj["level"].toString();
            score.baseline = obj["baseline"].toInt();

            auto contribs = obj["signal_contributions"].toObject();
            for (auto it = contribs.begin(); it != contribs.end(); ++it)
                score.signal_contributions[it.key()] = it.value().toDouble();

            cb(true, score);
        });
}

void NewsCorrelationService::detect_focal_points(const QJsonArray& geolocated_articles, FocalCallback cb) {
    auto json_str = QString::fromUtf8(QJsonDocument(geolocated_articles).toJson(QJsonDocument::Compact));

    python::PythonRunner::instance().run("news_correlation.py", {"focal_points", json_str},
        [cb](python::PythonResult result) {
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

            QVector<FocalPoint> points;
            for (const auto& v : obj["focal_points"].toArray()) {
                auto f = v.toObject();
                FocalPoint fp;
                fp.lat = f["lat"].toDouble();
                fp.lon = f["lon"].toDouble();
                fp.event_count = f["event_count"].toInt();
                fp.source_count = f["source_count"].toInt();
                fp.severity = f["severity"].toString();
                auto cats = f["categories"].toObject();
                for (auto it = cats.begin(); it != cats.end(); ++it)
                    fp.categories[it.key()] = it.value().toInt();
                for (const auto& h : f["headlines"].toArray())
                    fp.headlines.append(h.toString());
                points.append(fp);
            }
            cb(true, points);
        });
}

void NewsCorrelationService::fetch_predictions(PredictionCallback cb) {
    python::PythonRunner::instance().run("polymarket.py", {"get_markets", "20"},
        [this, cb](python::PythonResult result) {
            if (!result.success) {
                LOG_WARN("NewsCorrelation", "Prediction fetch failed: " + result.error);
                cb(false, {});
                return;
            }

            auto doc = QJsonDocument::fromJson(result.output.toUtf8());
            auto obj = doc.object();

            QVector<PredictionMarket> markets;
            auto arr = obj.contains("markets") ? obj["markets"].toArray() : obj["data"].toArray();
            for (const auto& v : arr) {
                auto m = v.toObject();
                PredictionMarket pm;
                pm.id = m["id"].toString();
                pm.question = m["question"].toString();
                pm.yes_price = m["yes_price"].toDouble(m["probability"].toDouble(0.5));
                pm.no_price = 1.0 - pm.yes_price;
                pm.volume = m["volume"].toDouble(m["totalLiquidity"].toDouble());
                markets.append(pm);
            }

            predictions_cache_ = markets;
            cb(true, markets);
        });
}

void NewsCorrelationService::update_baseline(const QMap<QString, int>& current_counts, BaselineCallback cb) {
    QJsonObject current;
    for (auto it = current_counts.begin(); it != current_counts.end(); ++it)
        current[it.key()] = it.value();

    QJsonObject existing;
    for (auto it = baselines_.begin(); it != baselines_.end(); ++it) {
        QJsonObject bl;
        QJsonArray counts;
        for (int c : it.value().hourly_counts)
            counts.append(c);
        bl["hourly_counts"] = counts;
        bl["mean"] = it.value().mean;
        bl["stddev"] = it.value().stddev;
        existing[it.key()] = bl;
    }

    auto cur_str = QString::fromUtf8(QJsonDocument(current).toJson(QJsonDocument::Compact));
    auto ex_str = QString::fromUtf8(QJsonDocument(existing).toJson(QJsonDocument::Compact));

    QPointer<NewsCorrelationService> self = this;
    python::PythonRunner::instance().run("news_correlation.py", {"baseline_update", cur_str, ex_str},
        [self, cb](python::PythonResult result) {
            if (!self) return;
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

            QMap<QString, CategoryBaseline> baselines;
            auto bl_obj = obj["baselines"].toObject();
            for (auto it = bl_obj.begin(); it != bl_obj.end(); ++it) {
                auto b = it.value().toObject();
                CategoryBaseline bl;
                for (const auto& c : b["hourly_counts"].toArray())
                    bl.hourly_counts.append(c.toInt());
                bl.mean = b["mean"].toDouble();
                bl.stddev = b["stddev"].toDouble();
                bl.sample_size = b["sample_size"].toInt();
                baselines[it.key()] = bl;
            }

            self->baselines_ = baselines;
            cb(true, baselines);
        });
}

void NewsCorrelationService::detect_deviations(const QMap<QString, int>& current_counts, DeviationCallback cb) {
    QJsonObject current;
    for (auto it = current_counts.begin(); it != current_counts.end(); ++it)
        current[it.key()] = it.value();

    QJsonObject baseline;
    for (auto it = baselines_.begin(); it != baselines_.end(); ++it) {
        QJsonObject bl;
        bl["mean"] = it.value().mean;
        bl["stddev"] = it.value().stddev;
        bl["sample_size"] = it.value().sample_size;
        baseline[it.key()] = bl;
    }

    auto cur_str = QString::fromUtf8(QJsonDocument(current).toJson(QJsonDocument::Compact));
    auto bl_str = QString::fromUtf8(QJsonDocument(baseline).toJson(QJsonDocument::Compact));

    python::PythonRunner::instance().run("news_correlation.py", {"detect_deviations", cur_str, bl_str},
        [cb](python::PythonResult result) {
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

            QVector<QPair<QString, double>> deviations;
            for (const auto& v : obj["deviations"].toArray()) {
                auto d = v.toObject();
                deviations.append({d["category"].toString(), d["z_score"].toDouble()});
            }
            cb(true, deviations);
        });
}

} // namespace fincept::services
