// src/services/news/NewsService_Classification.cpp
//
// Article classification (priority, market impact, event type, threat — the
// rules live in NewsClassify.h, pure and unit-tested) and source-flag mapping
// (source_flag_for / source_flag_label). No state, no network.
//
// Part of the partial-class split of NewsService.cpp.

#include "services/news/NewsClassify.h"
#include "services/news/NewsService.h"

#include <QMap>

namespace fincept::services {

namespace {

ThreatClassification news_threat_from(const news_classify::ThreatResult& t) {
    ThreatClassification tc;
    switch (t.level) {
        case news_classify::Threat::Critical:
            tc.level = ThreatLevel::CRITICAL;
            break;
        case news_classify::Threat::High:
            tc.level = ThreatLevel::HIGH;
            break;
        case news_classify::Threat::Medium:
            tc.level = ThreatLevel::MEDIUM;
            break;
        case news_classify::Threat::Low:
            tc.level = ThreatLevel::LOW;
            break;
        case news_classify::Threat::Info:
            tc.level = ThreatLevel::INFO;
            break;
    }
    tc.category = t.category;
    tc.confidence = t.confidence;
    return tc;
}

} // namespace

ThreatClassification NewsService::classify_threat(const NewsArticle& article) {
    // Convenience overload — builds text itself (used only outside enrich_article)
    return classify_threat(article, (article.headline + " " + article.summary).toLower());
}

ThreatClassification NewsService::classify_threat(const NewsArticle& /*article*/, const QString& text) {
    // THREAT = security / physical / systemic risk: conflict, cyber, natural
    // disasters, sanctions, and market-wide breakdowns (crash, bank run,
    // sovereign default). Ordinary market events — rate decisions, earnings
    // misses, downgrades, a bearish tone — are MARKET IMPACT (event_type +
    // impact), not threats. Whole words: "riot" is not "patriot".
    return news_threat_from(news_classify::threat(text));
}

QString NewsService::classify_event(const QString& text) {
    // Weak patterns ("acquisition", "probe", "fined") only with market context.
    return news_classify::event_type(text, news_classify::market_terms(text));
}

void NewsService::classify_article(NewsArticle& article) {
    news_classify::Input in;
    in.headline = article.headline;
    in.summary = article.summary;
    in.tier = article.tier;
    in.has_tickers = !article.tickers.isEmpty();
    in.tone = article.sentiment_score;
    in.tone_scored = article.sentiment_scored;
    const news_classify::Result r = news_classify::classify(in);
    switch (r.priority) {
        case news_classify::Prio::Flash:
            article.priority = Priority::FLASH;
            break;
        case news_classify::Prio::Urgent:
            article.priority = Priority::URGENT;
            break;
        case news_classify::Prio::Breaking:
            article.priority = Priority::BREAKING;
            break;
        case news_classify::Prio::Routine:
            article.priority = Priority::ROUTINE;
            break;
    }
    article.impact = r.impact == news_classify::Impact::High     ? Impact::HIGH
                     : r.impact == news_classify::Impact::Medium ? Impact::MEDIUM
                                                                 : Impact::LOW;
    article.event_type = r.event_type;
    article.threat = news_threat_from(r.threat);
}

// ── Source credibility ──────────────────────────────────────────────────────

SourceFlag NewsService::source_flag_for(const QString& source) {
    static const QMap<QString, SourceFlag> flags = {
        // State media
        {"XINHUA", SourceFlag::STATE_MEDIA},
        {"CGTN", SourceFlag::STATE_MEDIA},
        {"GLOBAL TIMES", SourceFlag::STATE_MEDIA},
        {"RT", SourceFlag::STATE_MEDIA},
        {"TASS", SourceFlag::STATE_MEDIA},
        {"SPUTNIK", SourceFlag::STATE_MEDIA},
        {"PRESS TV", SourceFlag::STATE_MEDIA},
        {"KCNA", SourceFlag::STATE_MEDIA},
        {"TRT WORLD", SourceFlag::STATE_MEDIA},
        {"AL ARABIYA", SourceFlag::STATE_MEDIA},
        // Caution — sensationalism or low editorial standards
        {"ZEROHEDGE", SourceFlag::CAUTION},
        {"INFOWARS", SourceFlag::CAUTION},
        {"DAILY MAIL", SourceFlag::CAUTION},
        {"NY POST", SourceFlag::CAUTION},
    };
    auto it = flags.find(source.toUpper());
    return it != flags.end() ? it.value() : SourceFlag::NONE;
}

QString NewsService::source_flag_label(SourceFlag flag) {
    switch (flag) {
        case SourceFlag::STATE_MEDIA:
            return "STATE MEDIA";
        case SourceFlag::CAUTION:
            return "CAUTION";
        default:
            return {};
    }
}

} // namespace fincept::services
