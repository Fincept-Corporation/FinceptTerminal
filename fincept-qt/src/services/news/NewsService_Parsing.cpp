// src/services/news/NewsService_Parsing.cpp
//
// RSS / Atom XML parsing (parse_rss_xml), HTML stripping (strip_html), and
// per-article enrichment (enrich_article — priority, sentiment, ticker
// extraction, threat classification dispatch).
//
// Part of the partial-class split of NewsService.cpp.

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "network/http/HttpClient.h"
#include "services/news/NewsClassify.h"
#include "services/news/NewsLanguage.h"
#include "services/news/NewsNlpEngine.h"
#include "services/news/NewsFeedParse.h"
#include "services/news/NewsService.h"
#include "services/news/NewsSymbolDirectory.h"
#include "services/news/NewsTickerTagger.h"
#include "storage/cache/CacheManager.h"

#include <QAtomicInt>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QLocale>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QUuid>
#include <QXmlStreamReader>

#ifdef HAS_QT_WEBSOCKETS
#    include <QtWebSockets/QWebSocket>
#endif

#include <algorithm>
#include <memory>
#include <utility>

namespace fincept::services {

// Max chars retained from an item's description after HTML stripping.
static constexpr int kSummaryMaxChars = 300;

namespace news_svc_parse_detail {
// When each undated article was first seen (id -> unix seconds), so an item
// whose feed dates nothing surfaces once, as new, and then ages — instead of
// being stamped "now" (top of the list) on every refresh. Parsing runs on
// worker threads, hence the lock. Seeded from stored articles on load.
QMutex g_first_seen_mutex;
QHash<QString, qint64> g_first_seen;
constexpr int kFirstSeenCap = 50000;
} // namespace news_svc_parse_detail

// NewsArticle::time — a legacy display string kept for consumers that print it
// (digest text, saved files). Locale-formatted, local time; the screens format
// sort_ts themselves (absolute time + zone).
static QString news_item_time_text(int64_t ts) {
    return QLocale().toString(QDateTime::fromSecsSinceEpoch(ts).toLocalTime(), QLocale::ShortFormat);
}

// ── Stable ids + link hygiene ───────────────────────────────────────────────

QString NewsService::stable_article_id(const QString& link, const QString& source, const QString& headline) {
    QString key = link.trimmed();
    if (key.isEmpty()) {
        key = source.toLower() + QLatin1Char('|') + headline.trimmed().toLower();
    } else {
        const int frag = key.indexOf(QLatin1Char('#'));
        if (frag >= 0)
            key.truncate(frag);
        while (key.endsWith(QLatin1Char('/')))
            key.chop(1);
    }
    const QByteArray digest = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QStringLiteral("n") + QString::fromLatin1(digest.left(16));
}

bool NewsService::is_web_url(const QString& url) {
    const QUrl u(url.trimmed());
    if (!u.isValid() || u.host().isEmpty())
        return false;
    const QString scheme = u.scheme().toLower();
    return scheme == QLatin1String("http") || scheme == QLatin1String("https");
}

// ── RSS / RDF / Atom parser ─────────────────────────────────────────────────

void NewsService::remember_first_seen(const QVector<NewsArticle>& articles) {
    using namespace news_svc_parse_detail;
    QMutexLocker lock(&g_first_seen_mutex);
    if (g_first_seen.size() > kFirstSeenCap)
        g_first_seen.clear();
    for (const auto& a : articles)
        if (!a.id.isEmpty() && a.sort_ts > 0 && !g_first_seen.contains(a.id))
            g_first_seen.insert(a.id, a.sort_ts);
}

QVector<NewsArticle> NewsService::parse_rss_xml(const QByteArray& xml, const RSSFeed& feed, QString* problem) {
    // Reading (encodings, BOM, CDATA, namespaces, broken entities) and the
    // time policy live in NewsFeedParse.h — pure, unit-tested code.
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    news_feed::Feed parsed = news_feed::parse_feed(xml, feed.naive_utc_offset_min);
    const qint64 shift = news_feed::normalize_times(parsed.items, parsed.feed_ts, now);
    if (shift > 0)
        LOG_DEBUG("NewsService", QString("Feed %1 dates items %2 min ahead of the clock (zone mislabelled); "
                                         "moved back")
                                     .arg(feed.id)
                                     .arg(shift / 60));
    news_feed::keep_newest(parsed.items, kMaxItemsPerFeed);
    if (problem && parsed.items.isEmpty())
        *problem = parsed.well_formed ? QStringLiteral("no items")
                                      : QStringLiteral("not a readable feed (%1)").arg(parsed.error.left(80));

    QVector<NewsArticle> articles;
    articles.reserve(parsed.items.size());
    QSet<QString> ids_in_feed; // guards against a feed that reuses one link for every item
    const QUrl base(feed.url);
    for (auto& item : parsed.items) {
        NewsArticle a;
        a.category = feed.category;
        a.source = feed.source;
        a.region = feed.region;
        a.tier = feed.tier;
        a.lang = feed.lang; // prior for language detection in enrich_article
        a.headline = item.title.left(200);
        if (a.headline.isEmpty())
            continue;
        a.summary = news_feed::clean_text(item.summary).left(kSummaryMaxChars);

        // Untrusted link: keep web URLs only (see is_web_url()); a relative
        // link is resolved against the feed's own URL first.
        QString link = item.link.trimmed();
        if (!link.isEmpty() && !link.contains(QLatin1String("://")))
            link = base.resolved(QUrl(link)).toString();
        a.link = is_web_url(link) ? link : QString();

        a.id = stable_article_id(a.link, a.source, a.headline);
        if (ids_in_feed.contains(a.id))
            a.id = stable_article_id(a.link + QLatin1Char('|') + a.headline, a.source, a.headline);
        ids_in_feed.insert(a.id);

        // The feed dates none of its items: undated, aged from the first-seen
        // time (see the policy in NewsFeedParse.h) — never a fresh "now" that
        // ranks above stories published minutes ago.
        qint64 first_seen = 0;
        if (item.ts <= 0) {
            using namespace news_svc_parse_detail;
            QMutexLocker lock(&g_first_seen_mutex);
            first_seen = g_first_seen.value(a.id, 0);
            if (first_seen == 0)
                g_first_seen.insert(a.id, now);
        }
        const news_feed::ItemTime placed = news_feed::item_time(item, first_seen, parsed.feed_ts, now);
        a.sort_ts = placed.ts;
        a.undated = placed.undated;
        a.time = news_item_time_text(a.sort_ts);

        enrich_article(a);
        articles.append(std::move(a));
    }
    return articles;
}

// ── Strip HTML tags ─────────────────────────────────────────────────────────

QString NewsService::strip_html(const QString& html) {
    return news_feed::clean_text(html);
}

// ── Enrich article: sentiment, priority, category, tickers ──────────────────

void NewsService::enrich_article(NewsArticle& article) {
    // Build once — reused for all keyword checks, ticker regex, and classify_threat
    const QString combined = article.headline + " " + article.summary;
    const QString text = combined.toLower();

    // Language first: the tone model, the English keyword rules and the filter
    // depend on it. `article.lang` arrives holding the feed's declared language
    // (the prior); the text decides when it is clear.
    article.lang = news_lang::detect(combined.left(600), article.lang).code;
    const bool scored_language = news_lang::sentiment_supported(article.lang);

    // Sentiment: the finance-tuned VADER engine the Equity Research sentiment tab
    // uses, so the feed and the research tab label the same headline the same way.
    // The previous scorer summed substring hits, which read "gain" in "again", "rise"
    // in "enterprise", "fall" in "fallout" and "win" in "window", and ignored negation
    // ("no recession") entirely.
    // The model is an English lexicon: other languages are "not scored" (left
    // neutral with sentiment_scored = false) rather than read as neutral.
    double score = 0.0;
    article.sentiment = Sentiment::NEUTRAL;
    article.sentiment_scored = false;
    if (scored_language) {
        const news_nlp::TextItem item{article.id, article.headline, article.summary};
        const NewsSentimentResult sr = news_nlp::analyze_sentiment({item});
        if (!sr.per_article.isEmpty()) {
            const ArticleSentimentResult& r = sr.per_article.first();
            score = r.score;
            article.sentiment_scored = true;
            article.sentiment = r.label == QLatin1String("BULLISH")   ? Sentiment::BULLISH
                                : r.label == QLatin1String("BEARISH") ? Sentiment::BEARISH
                                                                      : Sentiment::NEUTRAL;
        }
    }
    article.sentiment_score = score;

    // Category refinement — whole words: substring tests sent "steps"/"keeps" to
    // EARNINGS (via "eps") and "technique" to TECH; "Fed" counts only as written
    // (a "fed up" story is not ECONOMIC).
    static const QRegularExpression earnings_re(
        QStringLiteral("\\b(?:earnings|quarterly results|eps|guidance|revenue forecast)\\b"));
    static const QRegularExpression crypto_re(
        QStringLiteral("\\b(?:crypto\\w*|bitcoin|ethereum|blockchain|stablecoins?)\\b"));
    static const QRegularExpression defense_re(QStringLiteral("\\b(?:missiles?|troops|pentagon|military)\\b"));
    static const QRegularExpression economic_re(
        QStringLiteral("\\b(?:federal reserve|inflation|gdp|interest rates?|central bank|cpi|payrolls|"
                       "unemployment rate)\\b"));
    static const QRegularExpression markets_re(
        QStringLiteral("(?:s&p 500|\\bnasdaq\\b|\\bdow jones\\b|\\bstock market\\b|\\bwall street\\b)"));
    static const QRegularExpression energy_re(
        QStringLiteral("\\b(?:energy|crude|opec\\+?|natural gas|oil prices?|brent|wti)\\b"));
    static const QRegularExpression tech_re(
        QStringLiteral("\\b(?:tech|technology|ai|artificial intelligence|semiconductors?|chipmakers?|startups?)\\b"));
    static const QRegularExpression geo_re(
        QStringLiteral("\\b(?:nato|ukraine|russia|china|gaza|israel|iran|sanctions|geopolit\\w*)\\b"));
    if (earnings_re.match(text).hasMatch())
        article.category = "EARNINGS";
    else if (crypto_re.match(text).hasMatch())
        article.category = "CRYPTO";
    else if (defense_re.match(text).hasMatch())
        article.category = "DEFENSE";
    else if (economic_re.match(text).hasMatch() || news_classify::mentions_fed(combined))
        article.category = "ECONOMIC";
    else if (markets_re.match(text).hasMatch())
        article.category = "MARKETS";
    else if (energy_re.match(text).hasMatch())
        article.category = "ENERGY";
    else if (tech_re.match(text).hasMatch())
        article.category = "TECH";
    else if (geo_re.match(text).hasMatch())
        article.category = "GEOPOLITICS";

    // Tickers: only what the text names as a security — cashtags, exchange-
    // qualified codes (NSE:RELIANCE, 7203.T, 0700.HK), company names the local
    // symbol directory knows (watchlists, holdings, searched symbols, a built-in
    // list of large global companies) and symbols it knows. Capitalised words
    // that merely look like tickers are kept apart as "unverified".
    {
        const auto dict = NewsSymbolDirectory::current();
        QStringList unverified;
        const auto tags = news_tags::tag(article.headline, article.summary, *dict, &unverified, 6);
        article.tickers.clear();
        article.ticker_confidence.clear();
        for (const auto& t : tags) {
            if (t.confidence < news_tags::kVerifiedTagConfidence)
                continue;
            article.tickers.append(t.symbol);
            article.ticker_confidence.append(t.confidence);
        }
        article.unverified_tickers = unverified;
    }

    // Priority, market impact, event type and threat (NewsClassify.h): they need
    // market context — the tags above, finance terms or a money amount in the
    // headline — so generic words ("emergency", "breaking", "probe", "fined")
    // no longer make an ordinary story URGENT / BREAKING / impact HIGH.
    classify_article(article);

    // Source credibility flag
    article.source_flag = source_flag_for(article.source);
}

// ── Threat classification with confidence ───────────────────────────────────

} // namespace fincept::services
