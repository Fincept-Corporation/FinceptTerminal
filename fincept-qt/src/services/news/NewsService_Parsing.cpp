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
#include "services/feeds/FeedParseUtil.h"
#include "services/news/NewsNlpEngine.h"
#include "services/news/NewsService.h"
#include "storage/cache/CacheManager.h"

#include <QAtomicInt>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
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

// A feed clock running ahead of ours by more than this is treated as "now" so a
// mis-dated item can't pin itself to the top of the list indefinitely.
static constexpr int64_t kNewsMaxFutureSkewSec = 300;

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

// ── RSS XML parser ──────────────────────────────────────────────────────────

QVector<NewsArticle> NewsService::parse_rss_xml(const QByteArray& xml, const RSSFeed& feed) {
    QVector<NewsArticle> articles;
    QXmlStreamReader reader(xml);

    bool in_item = false;
    NewsArticle current;
    QString current_tag;
    QSet<QString> ids_in_feed; // guards against a feed that reuses one link for every item

    while (!reader.atEnd()) {
        auto token = reader.readNext();

        if (token == QXmlStreamReader::StartElement) {
            current_tag = reader.name().toString();

            if (current_tag == "item" || current_tag == "entry") {
                in_item = true;
                current = {};
                current.category = feed.category;
                current.source = feed.source;
                current.region = feed.region;
                current.tier = feed.tier;
                // id is derived from the link/headline once the item is complete.
            }

            // Atom <link href="..."/> or <link rel="alternate" href="..."/>
            if (in_item && current_tag == "link") {
                auto href = reader.attributes().value("href").toString();
                auto rel = reader.attributes().value("rel").toString();
                if (!href.isEmpty() && (rel.isEmpty() || rel == "alternate")) {
                    if (current.link.isEmpty())
                        current.link = href;
                }
            }
        } else if (token == QXmlStreamReader::Characters && in_item) {
            QString text = reader.text().toString().trimmed();
            if (text.isEmpty())
                continue;

            if (current_tag == "title" && current.headline.isEmpty()) {
                current.headline = text.left(200);
            } else if ((current_tag == "description" || current_tag == "summary" || current_tag == "encoded") &&
                       current.summary.isEmpty()) {
                current.summary = strip_html(text).left(kSummaryMaxChars);
            } else if (current_tag == "link" && current.link.isEmpty()) {
                current.link = text.trimmed();
            } else if ((current_tag == "guid" || current_tag == "id") && current.link.isEmpty()) {
                // guid/id often contains the article URL as fallback
                if (text.startsWith("http"))
                    current.link = text.trimmed();
            } else if (current_tag == "pubDate" || current_tag == "published" || current_tag == "updated" ||
                       current_tag == "date") {
                if (current.sort_ts == 0) {
                    QDateTime dt = QDateTime::fromString(text, Qt::RFC2822Date);
                    if (!dt.isValid())
                        dt = QDateTime::fromString(text, Qt::ISODate);
                    if (!dt.isValid())
                        dt = QDateTime::fromString(text, "ddd, dd MMM yyyy HH:mm:ss");
                    if (dt.isValid()) {
                        current.sort_ts = dt.toSecsSinceEpoch();
                        current.time = dt.toLocalTime().toString("MMM dd, HH:mm");
                    } else {
                        // Named zones (IST, PDT...), epoch values, "05-Jun-2026 ..." —
                        // the shared feed parser covers what Qt's stock formats don't.
                        // An undated item otherwise falls back to "now" below and
                        // pins an old story to the top of the list.
                        QString display;
                        const qint64 parsed_ts = fincept::feeds::parse_feed_datetime(text, display);
                        if (parsed_ts > 0) {
                            current.sort_ts = parsed_ts;
                            current.time = QDateTime::fromSecsSinceEpoch(parsed_ts).toString("MMM dd, HH:mm");
                        } else {
                            current.time = text.left(22);
                        }
                    }
                }
            }
        } else if (token == QXmlStreamReader::EndElement) {
            QString tag = reader.name().toString();
            if ((tag == "item" || tag == "entry") && in_item) {
                in_item = false;
                if (current.headline.isEmpty())
                    continue;

                if (current.time.isEmpty())
                    current.time = QDateTime::currentDateTime().toString("MMM dd, HH:mm");
                const int64_t now_ts = QDateTime::currentSecsSinceEpoch();
                if (current.sort_ts == 0 || current.sort_ts > now_ts + kNewsMaxFutureSkewSec) {
                    current.sort_ts = now_ts;
                    current.time = QDateTime::fromSecsSinceEpoch(now_ts).toString("MMM dd, HH:mm");
                }

                // Untrusted link: keep web URLs only (see is_web_url()).
                if (!is_web_url(current.link))
                    current.link.clear();

                current.id = stable_article_id(current.link, current.source, current.headline);
                if (ids_in_feed.contains(current.id))
                    current.id = stable_article_id(current.link + QLatin1Char('|') + current.headline, current.source,
                                                   current.headline);
                ids_in_feed.insert(current.id);

                enrich_article(current);
                // Move the finished item out and reset `current` in one step, so it is
                // never left in a moved-from state.
                articles.append(std::exchange(current, NewsArticle{}));
            }
        }
    }

    return articles;
}

// ── Strip HTML tags ─────────────────────────────────────────────────────────

QString NewsService::strip_html(const QString& html) {
    static QRegularExpression re("<[^>]*>");
    QString out = html;
    out.replace(re, "");
    return out.simplified();
}

// ── Enrich article: sentiment, priority, category, tickers ──────────────────

void NewsService::enrich_article(NewsArticle& article) {
    // Build once — reused for all keyword checks, ticker regex, and classify_threat
    const QString combined = article.headline + " " + article.summary;
    const QString text = combined.toLower();

    // Priority. FLASH drives banners, sounds and system notifications, so match
    // whole words: a bare substring test flagged "record-breaking", "alerted"
    // and the like as breaking news.
    static const QRegularExpression flash_re(QStringLiteral("(?<![\\w-])(?:breaking|alerts?)\\b"));
    static const QRegularExpression urgent_re(QStringLiteral("\\b(?:urgent|emergency)"));
    // BREAKING = a market-moving corporate / macro event. It used to be any text
    // containing "announce" or "report" — reporter, reported, annual report — which
    // tagged a large share of the feed BREAKING and flooded the ticker strip and the
    // relevance sort with routine stories.
    static const QRegularExpression event_re(QStringLiteral(
        "\\b(?:acquires?|acquired|acquisition|merger|takeover|buyout|downgrades?|downgraded|upgrades?|upgraded|"
        "profit warning|guidance|(?:beats?|miss(?:es)?|tops?) (?:estimates|expectations|forecasts)|earnings|"
        "halts?|halted|bankruptcy|chapter 11|recalls?|recalled|lawsuit|antitrust|settlement|"
        "rate (?:hike|cut|decision)|(?:raises|cuts|holds) rates|jobs report|nonfarm payrolls|cpi|inflation data|"
        "defaults?|defaulted|sanctions?|tariffs?|layoffs?|ipo|buyback|dividend (?:cut|hike|increase|suspension))\\b"));
    if (flash_re.match(text).hasMatch())
        article.priority = Priority::FLASH;
    else if (urgent_re.match(text).hasMatch())
        article.priority = Priority::URGENT;
    else if (event_re.match(text).hasMatch())
        article.priority = Priority::BREAKING;

    // Sentiment: the finance-tuned VADER engine the Equity Research sentiment tab
    // uses, so the feed and the research tab label the same headline the same way.
    // The previous scorer summed substring hits, which read "gain" in "again", "rise"
    // in "enterprise", "fall" in "fallout" and "win" in "window", and ignored negation
    // ("no recession") entirely.
    double score = 0.0;
    {
        const news_nlp::TextItem item{article.id, article.headline, article.summary};
        const NewsSentimentResult sr = news_nlp::analyze_sentiment({item});
        if (!sr.per_article.isEmpty()) {
            const ArticleSentimentResult& r = sr.per_article.first();
            score = r.score;
            article.sentiment = r.label == QLatin1String("BULLISH")   ? Sentiment::BULLISH
                                : r.label == QLatin1String("BEARISH") ? Sentiment::BEARISH
                                                                      : Sentiment::NEUTRAL;
        }
    }
    article.sentiment_score = score;

    // Impact: event priority, else the strength of the sentiment signal.
    const double magnitude = std::abs(score);
    if (article.priority == Priority::FLASH || article.priority == Priority::URGENT || magnitude >= 0.7)
        article.impact = Impact::HIGH;
    else if (article.priority == Priority::BREAKING || magnitude >= 0.4)
        article.impact = Impact::MEDIUM;

    // Category refinement — whole words: substring tests sent "steps"/"keeps" to
    // EARNINGS (via "eps"), "technique" to TECH and "fed up" to ECONOMIC.
    static const QRegularExpression earnings_re(
        QStringLiteral("\\b(?:earnings|quarterly results|eps|guidance|revenue forecast)\\b"));
    static const QRegularExpression crypto_re(
        QStringLiteral("\\b(?:crypto\\w*|bitcoin|ethereum|blockchain|stablecoins?)\\b"));
    static const QRegularExpression defense_re(QStringLiteral("\\b(?:missiles?|troops|pentagon|military)\\b"));
    static const QRegularExpression economic_re(
        QStringLiteral("\\b(?:fed|federal reserve|inflation|gdp|interest rates?|central bank|cpi|payrolls|"
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
    else if (economic_re.match(text).hasMatch())
        article.category = "ECONOMIC";
    else if (markets_re.match(text).hasMatch())
        article.category = "MARKETS";
    else if (energy_re.match(text).hasMatch())
        article.category = "ENERGY";
    else if (tech_re.match(text).hasMatch())
        article.category = "TECH";
    else if (geo_re.match(text).hasMatch())
        article.category = "GEOPOLITICS";

    // Extract tickers. Explicit cashtags ($AAPL) always count; bare uppercase
    // 2-5 letter words only count when they are not a common word or a news/
    // finance acronym (US, CEO, GDP, FED ...), and not when the whole headline
    // is shouted in capitals (every word would look like a symbol). Kept in
    // order of appearance — QSet::values() order is arbitrary.
    static const QRegularExpression cashtag_re("\\$([A-Z]{1,5})\\b");
    static const QRegularExpression ticker_re("\\b[A-Z]{2,5}\\b");
    static const QSet<QString> common_words = {
        "THE",  "FOR",  "AND",   "BUT",  "NOT",  "FROM", "WITH", "THIS", "THAT", "HAVE", "WILL", "BEEN", "THEY",
        "WERE", "SAID", "HAS",   "ITS",  "NEW",  "ARE",  "WAS",  "WHO",  "HOW",  "WHY",  "ALL",  "CAN",  "MAY",
        "NOW",  "OUT",  "ONE",   "TWO",  "OUR",  "YOU",  "HER",  "HIS",  "SAYS", "SAY",  "GET",  "GOT",  "LET",
        "TOP",  "BIG",  "NEXT",  "OVER", "INTO", "AFTER", "WHAT", "WHEN", "ABOUT", "AM",  "PM",   "TV",   "PC",
        "OK",   "VS",   "NO",    "OR",   "IN",   "IS",   "OF",   "ON",   "TO",   "AT",   "BY",   "AS",   "AN",
        "IF",   "SO",   "UP",    "DO",   "GO",   "BE",   "WE",   "HE",   "ME",   "MY",   "US",   "UK",   "EU",
        "UN",   "UAE",  "USA",   "AI",   "CEO",  "CFO",  "COO",  "CTO",  "IPO",  "ETF",  "GDP",  "CPI",  "PPI",
        "PMI",  "FED",  "FOMC",  "ECB",  "BOE",  "BOJ",  "PBOC", "IMF",  "WTO",  "WHO",  "NATO", "OPEC", "SEC",
        "FDA",  "DOJ",  "FBI",   "CIA",  "NSA",  "IRS",  "FTC",  "FCC",  "EPA",  "NYSE", "DOW",  "USD",  "EUR",
        "GBP",  "JPY",  "CNY",   "INR",  "AUD",  "CAD",  "CHF",  "GOP",  "ESG",  "COVID", "UPDATE", "LIVE", "VIDEO",
        "WATCH", "EXCLUSIVE", "ALERT", "BREAKING", "URGENT", "NEWS", "TIMES", "POST",
    };
    QStringList found;
    auto add_ticker = [&found](const QString& t) {
        if (found.size() < 5 && !found.contains(t))
            found.append(t);
    };
    {
        auto cashtags = cashtag_re.globalMatch(combined);
        while (cashtags.hasNext())
            add_ticker(cashtags.next().captured(1));
    }
    int letters = 0, upper = 0;
    for (const QChar ch : combined) {
        if (ch.isLetter()) {
            ++letters;
            if (ch.isUpper())
                ++upper;
        }
    }
    const bool shouting = letters >= 24 && upper * 10 >= letters * 6;
    if (!shouting) {
        auto it = ticker_re.globalMatch(combined); // reuse already-built string
        while (it.hasNext() && found.size() < 5) {
            const QString t = it.next().captured();
            if (!common_words.contains(t))
                add_ticker(t);
        }
    }
    article.tickers = found;

    // Language detection by script range (CJK, kana, hangul, Cyrillic, Arabic,
    // Devanagari) — the same rule the NLP engine applies before translating.
    article.lang = news_nlp::detect_language(article.headline);

    // Threat classification — pass pre-built text to avoid a 3rd toLower()
    article.threat = classify_threat(article, text);

    // Source credibility flag
    article.source_flag = source_flag_for(article.source);
}

// ── Threat classification with confidence ───────────────────────────────────

} // namespace fincept::services
