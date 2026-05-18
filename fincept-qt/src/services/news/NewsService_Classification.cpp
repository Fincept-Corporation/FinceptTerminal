// src/services/news/NewsService_Classification.cpp
//
// Threat classification (classify_threat overloads) and source-flag mapping
// (source_flag_for / source_flag_label) — pure lookups over headline/source
// text. No state, no network.
//
// Part of the partial-class split of NewsService.cpp.

#include "services/news/NewsService.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "storage/cache/CacheManager.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"

#include <QAtomicInt>
#include <QDateTime>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <QXmlStreamReader>

#ifdef HAS_QT_WEBSOCKETS
#    include <QtWebSockets/QWebSocket>
#endif

#include <algorithm>
#include <memory>

namespace fincept::services {

ThreatClassification NewsService::classify_threat(const NewsArticle& article) {
    // Convenience overload — builds text itself (used only outside enrich_article)
    return classify_threat(article, (article.headline + " " + article.summary).toLower());
}

ThreatClassification NewsService::classify_threat(const NewsArticle& article, const QString& text) {
    ThreatClassification tc;
    tc.category = "general";
    tc.confidence = 0.3; // base confidence from keyword matching

    // Critical — immediate, high-impact events
    struct PatternScore {
        const char* pattern;
        const char* category;
        ThreatLevel level;
        double conf;
    };
    static const PatternScore critical_patterns[] = {
        {"nuclear strike", "conflict", ThreatLevel::CRITICAL, 0.95},
        {"nuclear attack", "conflict", ThreatLevel::CRITICAL, 0.95},
        {"war declared", "conflict", ThreatLevel::CRITICAL, 0.95},
        {"market crash", "market", ThreatLevel::CRITICAL, 0.9},
        {"flash crash", "market", ThreatLevel::CRITICAL, 0.9},
        {"circuit breaker", "market", ThreatLevel::CRITICAL, 0.85},
        {"trading halt", "market", ThreatLevel::CRITICAL, 0.85},
        {"bank run", "market", ThreatLevel::CRITICAL, 0.9},
        {"sovereign default", "market", ThreatLevel::CRITICAL, 0.9},
        {"cyberattack", "cyber", ThreatLevel::HIGH, 0.8},
        {"data breach", "cyber", ThreatLevel::HIGH, 0.75},
        {"ransomware", "cyber", ThreatLevel::HIGH, 0.8},
    };

    // High — significant events
    static const PatternScore high_patterns[] = {
        {"invasion", "conflict", ThreatLevel::HIGH, 0.85},
        {"airstrike", "conflict", ThreatLevel::HIGH, 0.85},
        {"missile launch", "conflict", ThreatLevel::HIGH, 0.85},
        {"military deploy", "conflict", ThreatLevel::HIGH, 0.8},
        {"coup attempt", "conflict", ThreatLevel::HIGH, 0.85},
        {"martial law", "conflict", ThreatLevel::HIGH, 0.85},
        {"bankruptcy fil", "market", ThreatLevel::HIGH, 0.8},
        {"rate hike", "market", ThreatLevel::HIGH, 0.7},
        {"rate cut", "market", ThreatLevel::HIGH, 0.7},
        {"earnings miss", "market", ThreatLevel::HIGH, 0.75},
        {"profit warning", "market", ThreatLevel::HIGH, 0.75},
        {"downgrad", "market", ThreatLevel::HIGH, 0.7},
        {"sanction", "regulatory", ThreatLevel::HIGH, 0.7},
        {"embargo", "regulatory", ThreatLevel::HIGH, 0.75},
        {"earthquake", "natural", ThreatLevel::HIGH, 0.8},
        {"tsunami", "natural", ThreatLevel::HIGH, 0.85},
        {"hurricane", "natural", ThreatLevel::HIGH, 0.75},
        {"pandemic", "natural", ThreatLevel::HIGH, 0.8},
    };

    // Medium patterns
    static const PatternScore medium_patterns[] = {
        {"protest", "conflict", ThreatLevel::MEDIUM, 0.6},     {"riot", "conflict", ThreatLevel::MEDIUM, 0.7},
        {"tension", "conflict", ThreatLevel::MEDIUM, 0.5},     {"escalat", "conflict", ThreatLevel::MEDIUM, 0.65},
        {"tariff", "regulatory", ThreatLevel::MEDIUM, 0.65},   {"regulation", "regulatory", ThreatLevel::MEDIUM, 0.5},
        {"antitrust", "regulatory", ThreatLevel::MEDIUM, 0.6}, {"investigat", "regulatory", ThreatLevel::MEDIUM, 0.55},
        {"layoff", "market", ThreatLevel::MEDIUM, 0.6},        {"recession", "market", ThreatLevel::MEDIUM, 0.65},
        {"inflation", "market", ThreatLevel::MEDIUM, 0.55},    {"selloff", "market", ThreatLevel::MEDIUM, 0.6},
        {"sell-off", "market", ThreatLevel::MEDIUM, 0.6},      {"volatil", "market", ThreatLevel::MEDIUM, 0.5},
        {"wildfire", "natural", ThreatLevel::MEDIUM, 0.6},     {"flood", "natural", ThreatLevel::MEDIUM, 0.6},
    };

    // Check patterns in priority order — first critical, then high, then medium
    for (const auto& p : critical_patterns) {
        if (text.contains(p.pattern)) {
            tc.level = p.level;
            tc.category = p.category;
            tc.confidence = p.conf;
            return tc;
        }
    }
    for (const auto& p : high_patterns) {
        if (text.contains(p.pattern)) {
            tc.level = p.level;
            tc.category = p.category;
            tc.confidence = p.conf;
            return tc;
        }
    }
    for (const auto& p : medium_patterns) {
        if (text.contains(p.pattern)) {
            tc.level = p.level;
            tc.category = p.category;
            tc.confidence = p.conf;
            return tc;
        }
    }

    // Low: any negative sentiment article
    if (article.sentiment == Sentiment::BEARISH) {
        tc.level = ThreatLevel::LOW;
        tc.confidence = 0.4;
    }

    return tc;
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

// ── Default RSS feeds ──────────────────────────────────────────────────────

} // namespace fincept::services
