#include "services/equity/MarketSentimentSupport.h"

#include <QJsonArray>

#include <algorithm>

namespace fincept::services::equity::sentiment {

namespace {

double first_numeric(const QJsonObject& object, const QStringList& keys) {
    for (const auto& key : keys) {
        if (!object.contains(key)) {
            continue;
        }
        const auto value = object.value(key);
        if (value.isDouble()) {
            return value.toDouble();
        }
        if (value.isString()) {
            bool ok = false;
            const double parsed = value.toString().toDouble(&ok);
            if (ok) {
                return parsed;
            }
        }
    }
    return 0.0;
}

QJsonObject unwrap_payload(const QJsonObject& payload) {
    if (payload.contains("data") && payload.value("data").isObject()) {
        return payload.value("data").toObject();
    }
    return payload;
}

QJsonArray extract_stocks(const QJsonObject& payload) {
    const auto root = unwrap_payload(payload);
    if (root.contains("stocks") && root.value("stocks").isArray()) {
        return root.value("stocks").toArray();
    }
    if (root.contains("results") && root.value("results").isArray()) {
        return root.value("results").toArray();
    }
    return {};
}

} // namespace

QStringList source_ids() {
    return {"reddit", "x", "news", "polymarket"};
}

QString source_label(const QString& source_id) {
    if (source_id == "reddit") {
        return "Reddit";
    }
    if (source_id == "x") {
        return "X";
    }
    if (source_id == "news") {
        return "News";
    }
    if (source_id == "polymarket") {
        return "Polymarket";
    }
    return source_id.toUpper();
}

SentimentSourceSnapshot parse_compare_payload(const QString& source_id, const QJsonObject& payload) {
    SentimentSourceSnapshot snapshot;
    snapshot.source_id = source_id;
    snapshot.label = source_label(source_id);

    const auto stocks = extract_stocks(payload);
    if (stocks.isEmpty() || !stocks.first().isObject()) {
        return snapshot;
    }

    const auto stock = stocks.first().toObject();
    snapshot.available = !stock.isEmpty();
    snapshot.buzz_score = first_numeric(stock, {"buzz_score"});
    snapshot.bullish_pct = first_numeric(stock, {"bullish_pct"});
    snapshot.sentiment_score = first_numeric(stock, {"sentiment_score", "sentiment"});
    snapshot.activity_count = first_numeric(
        stock,
        {"mentions", "trade_count", "unique_posts", "source_count", "subreddit_count", "market_count", "unique_traders"});

    return snapshot;
}

QString compute_source_alignment(const QVector<SentimentSourceSnapshot>& sources) {
    QVector<double> bullish_values;
    bullish_values.reserve(sources.size());
    for (const auto& source : sources) {
        if (source.available) {
            bullish_values.append(source.bullish_pct);
        }
    }

    if (bullish_values.isEmpty()) {
        return "unavailable";
    }
    if (bullish_values.size() == 1) {
        return "single_source";
    }

    auto [min_it, max_it] = std::minmax_element(bullish_values.begin(), bullish_values.end());
    const double spread = *max_it - *min_it;

    if (spread <= 12.0) {
        return "aligned";
    }
    if (spread <= 25.0) {
        return "mixed";
    }
    return "divergent";
}

QJsonObject snapshot_to_json(const MarketSentimentSnapshot& snapshot) {
    QJsonArray sources;
    for (const auto& source : snapshot.sources) {
        QJsonObject object;
        object["source_id"] = source.source_id;
        object["label"] = source.label;
        object["available"] = source.available;
        object["buzz_score"] = source.buzz_score;
        object["bullish_pct"] = source.bullish_pct;
        object["sentiment_score"] = source.sentiment_score;
        object["activity_count"] = source.activity_count;
        sources.append(object);
    }

    QJsonObject object;
    object["symbol"] = snapshot.symbol;
    object["configured"] = snapshot.configured;
    object["available"] = snapshot.available;
    object["status"] = snapshot.status;
    object["message"] = snapshot.message;
    object["average_buzz"] = snapshot.average_buzz;
    object["average_bullish_pct"] = snapshot.average_bullish_pct;
    object["coverage"] = snapshot.coverage;
    object["source_alignment"] = snapshot.source_alignment;
    object["sources"] = sources;
    object["fetched_at"] = snapshot.fetched_at;
    return object;
}

MarketSentimentSnapshot snapshot_from_json(const QJsonObject& object) {
    MarketSentimentSnapshot snapshot;
    snapshot.symbol = object.value("symbol").toString();
    snapshot.configured = object.value("configured").toBool();
    snapshot.available = object.value("available").toBool();
    snapshot.status = object.value("status").toString();
    snapshot.message = object.value("message").toString();
    snapshot.average_buzz = object.value("average_buzz").toDouble();
    snapshot.average_bullish_pct = object.value("average_bullish_pct").toDouble();
    snapshot.coverage = object.value("coverage").toInt();
    snapshot.source_alignment = object.value("source_alignment").toString();
    snapshot.fetched_at = object.value("fetched_at").toString();

    const auto source_values = object.value("sources").toArray();
    snapshot.sources.reserve(source_values.size());
    for (const auto& value : source_values) {
        const auto source_object = value.toObject();
        SentimentSourceSnapshot source;
        source.source_id = source_object.value("source_id").toString();
        source.label = source_object.value("label").toString();
        source.available = source_object.value("available").toBool();
        source.buzz_score = source_object.value("buzz_score").toDouble();
        source.bullish_pct = source_object.value("bullish_pct").toDouble();
        source.sentiment_score = source_object.value("sentiment_score").toDouble();
        source.activity_count = source_object.value("activity_count").toDouble();
        snapshot.sources.append(source);
    }

    return snapshot;
}

} // namespace fincept::services::equity::sentiment
