#pragma once
// StartupPrewarm.h — which DataHub topics main.cpp may ask for while the login
// screen is up, before anyone has opened a dashboard.
//
// Owner's rule: nothing that costs credits (or a provider's quota) runs
// automatically. A pre-warm request is automatic by definition, so it may only
// name topics whose producers never call a billed endpoint. A topic is free when
// it matches a pattern of free_topic_patterns() — the declaration, with the
// producer that makes it free — and none of billed_topic_patterns(). main.cpp
// passes its whole list through free_only(): anything else is dropped (logged,
// and a Q_ASSERT in a debug build). tests/tst_startup_prewarm.cpp pins the lists
// and checks that main.cpp still filters.
//
// Adding a topic to the pre-warm: declare it here only after checking its
// producer's refresh() — a Fincept API route priced in billing::PlanAccess, a
// screen's set_cost_credits(), a credits_used field in the answer or an LLM call
// all mean "billed": it then loads on the user's action (a LOAD / ⟳ button, an
// AutoLoadPane with Policy::Manual) and never here.
//
// Header-only, QtCore only. Patterns follow DataHub: a trailing '*' matches any
// suffix, anything else matches exactly.

#include <QString>
#include <QStringList>
#include <QStringView>

namespace fincept::startup_prewarm {

/// DataHub's matching rule (DataHub::pattern_matches): "prefix*" or exact.
inline bool topic_matches(const QString& topic, const QString& pattern) {
    if (!pattern.endsWith(QLatin1Char('*')))
        return topic == pattern;
    const QStringView prefix = QStringView{pattern}.left(pattern.size() - 1);
    return QStringView{topic}.startsWith(prefix);
}

/// Topics whose producers cost nothing per request — the only ones the start-up
/// pre-warm may name.
///   market:quote:*  MarketDataService: yfinance through the local Python worker.
///   news:general    NewsService: public RSS / Atom feeds (article analysis, the
///                   one billed news call, is a button and no topic).
inline const QStringList& free_topic_patterns() {
    static const QStringList patterns{
        QStringLiteral("market:quote:*"),
        QStringLiteral("news:general"),
    };
    return patterns;
}

/// Topics whose producers call a billed Fincept API route. Never pre-warmed, even
/// if a free pattern above were widened to cover them.
///   econ:fincept:*          Fincept macro data — MacroCalendarService's economic
///                           calendar today: /macro/upcoming-events, "macro"
///                           (1 credit list price per load).
///   geopolitics:events, :countries, :categories, :cities
///                           GeopoliticsService — /research/news-events,
///                           "news_events" (3 credits per call).
///   maritime:vessel:*, maritime:vessels:*, maritime:history:*
///                           MaritimeService — /marine/vessel/*, "maritime"
///                           (1 per position, 8 per area search, 5 per history).
inline const QStringList& billed_topic_patterns() {
    static const QStringList patterns{
        QStringLiteral("econ:fincept:*"),
        QStringLiteral("geopolitics:events"),
        QStringLiteral("geopolitics:countries"),
        QStringLiteral("geopolitics:categories"),
        QStringLiteral("geopolitics:cities"),
        QStringLiteral("maritime:vessel:*"),
        QStringLiteral("maritime:vessels:*"),
        QStringLiteral("maritime:history:*"),
    };
    return patterns;
}

inline bool matches_any(const QString& topic, const QStringList& patterns) {
    for (const QString& p : patterns) {
        if (topic_matches(topic, p))
            return true;
    }
    return false;
}

inline bool is_billed_topic(const QString& topic) {
    return matches_any(topic, billed_topic_patterns());
}

/// Declared free and not billed.
inline bool is_free_topic(const QString& topic) {
    return matches_any(topic, free_topic_patterns()) && !is_billed_topic(topic);
}

/// The non-quote topics of the default dashboard template that the pre-warm asks
/// for. The template's economic calendar (econ:fincept:upcoming_events) is billed:
/// its tile offers LOAD CALENDAR instead and is not warmed.
inline QStringList default_template_topics() {
    return {QStringLiteral("news:general")};
}

/// `topics` in order without those that are not free; `dropped` (optional)
/// receives the removed ones.
inline QStringList free_only(const QStringList& topics, QStringList* dropped = nullptr) {
    QStringList kept;
    kept.reserve(topics.size());
    for (const QString& t : topics) {
        if (is_free_topic(t))
            kept.append(t);
        else if (dropped)
            dropped->append(t);
    }
    return kept;
}

} // namespace fincept::startup_prewarm
