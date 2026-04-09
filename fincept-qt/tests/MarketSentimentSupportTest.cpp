#include "services/equity/MarketSentimentSupport.h"

#include <QJsonArray>
#include <QJsonObject>

#include <iostream>

using fincept::services::equity::MarketSentimentSnapshot;
using fincept::services::equity::SentimentSourceSnapshot;
using fincept::services::equity::sentiment::compute_source_alignment;
using fincept::services::equity::sentiment::parse_compare_payload;
using fincept::services::equity::sentiment::snapshot_from_json;
using fincept::services::equity::sentiment::snapshot_to_json;

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
    return true;
}

bool test_parse_compare_payload() {
    QJsonObject stock;
    stock["ticker"] = "AAPL";
    stock["buzz_score"] = 63.5;
    stock["bullish_pct"] = 68.0;
    stock["sentiment_score"] = 0.42;
    stock["mentions"] = 123;

    QJsonObject payload;
    payload["stocks"] = QJsonArray{stock};

    const auto snapshot = parse_compare_payload("reddit", payload);
    return expect(snapshot.available, "reddit source should be available") &&
           expect(snapshot.source_id == "reddit", "reddit source id should round-trip") &&
           expect(snapshot.label == "Reddit", "reddit label should be humanized") &&
           expect(snapshot.buzz_score == 63.5, "buzz score should be parsed") &&
           expect(snapshot.bullish_pct == 68.0, "bullish pct should be parsed") &&
           expect(snapshot.sentiment_score == 0.42, "sentiment score should be parsed") &&
           expect(snapshot.activity_count == 123.0, "mentions should backfill activity count");
}

bool test_polymarket_activity_fallback() {
    QJsonObject stock;
    stock["ticker"] = "TSLA";
    stock["buzz_score"] = 58.0;
    stock["bullish_pct"] = 55.0;
    stock["trade_count"] = 87;

    QJsonObject payload;
    payload["stocks"] = QJsonArray{stock};

    const auto snapshot = parse_compare_payload("polymarket", payload);
    return expect(snapshot.available, "polymarket source should be available") &&
           expect(snapshot.activity_count == 87.0, "trade_count should backfill activity count");
}

bool test_alignment_and_roundtrip() {
    SentimentSourceSnapshot reddit;
    reddit.source_id = "reddit";
    reddit.label = "Reddit";
    reddit.available = true;
    reddit.bullish_pct = 70.0;

    SentimentSourceSnapshot news;
    news.source_id = "news";
    news.label = "News";
    news.available = true;
    news.bullish_pct = 66.0;

    SentimentSourceSnapshot x;
    x.source_id = "x";
    x.label = "X";
    x.available = true;
    x.bullish_pct = 48.0;

    const auto alignment = compute_source_alignment({reddit, news, x});
    if (!expect(alignment == "mixed", "alignment should detect moderate source dispersion")) {
        return false;
    }

    MarketSentimentSnapshot snapshot;
    snapshot.symbol = "AAPL";
    snapshot.configured = true;
    snapshot.available = true;
    snapshot.status = "ok";
    snapshot.message = "";
    snapshot.average_buzz = 61.2;
    snapshot.average_bullish_pct = 61.3;
    snapshot.coverage = 3;
    snapshot.source_alignment = alignment;
    snapshot.sources = {reddit, news, x};
    snapshot.fetched_at = "2026-04-09T12:00:00Z";

    const auto restored = snapshot_from_json(snapshot_to_json(snapshot));
    return expect(restored.symbol == "AAPL", "symbol should survive serialization") &&
           expect(restored.coverage == 3, "coverage should survive serialization") &&
           expect(restored.sources.size() == 3, "sources should survive serialization") &&
           expect(restored.source_alignment == "mixed", "alignment should survive serialization");
}

} // namespace

int main() {
    if (!test_parse_compare_payload()) {
        return 1;
    }
    if (!test_polymarket_activity_fallback()) {
        return 1;
    }
    if (!test_alignment_and_roundtrip()) {
        return 1;
    }
    return 0;
}
