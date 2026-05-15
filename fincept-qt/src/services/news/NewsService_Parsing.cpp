// src/services/news/NewsService_Parsing.cpp
//
// RSS / Atom XML parsing (parse_rss_xml), HTML stripping (strip_html), and
// per-article enrichment (enrich_article — priority, sentiment, ticker
// extraction, threat classification dispatch).
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

// Max chars retained from an item's description after HTML stripping.
static constexpr int kSummaryMaxChars = 300;

// ── RSS XML parser ──────────────────────────────────────────────────────────

QVector<NewsArticle> NewsService::parse_rss_xml(const QByteArray& xml, const RSSFeed& feed) {
    QVector<NewsArticle> articles;
    QXmlStreamReader reader(xml);

    bool in_item = false;
    NewsArticle current;
    QString current_tag;
    int item_idx = 0;

    while (!reader.atEnd()) {
        auto token = reader.readNext();

        if (token == QXmlStreamReader::StartElement) {
            current_tag = reader.name().toString();

            if (current_tag == "item" || current_tag == "entry") {
                in_item = true;
                item_idx++;
                current = {};
                current.category = feed.category;
                current.source = feed.source;
                current.region = feed.region;
                current.tier = feed.tier;
                current.id = QString("%1-%2-%3").arg(feed.id).arg(QDateTime::currentMSecsSinceEpoch()).arg(item_idx);
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
                        current.time = dt.toString("MMM dd, HH:mm");
                    } else {
                        current.time = text.left(22);
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
                if (current.sort_ts == 0)
                    current.sort_ts = QDateTime::currentSecsSinceEpoch();

                enrich_article(current);
                articles.append(std::move(current));
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

    // Priority
    if (text.contains("breaking") || text.contains("alert"))
        article.priority = Priority::FLASH;
    else if (text.contains("urgent") || text.contains("emergency"))
        article.priority = Priority::URGENT;
    else if (text.contains("announce") || text.contains("report"))
        article.priority = Priority::BREAKING;

    // Weighted sentiment
    struct WordWeight {
        const char* word;
        int weight;
    };

    static const WordWeight positives[] = {
        {"surge", 3},       {"soar", 3},       {"skyrocket", 3}, {"breakthrough", 3}, {"boom", 3},
        {"record high", 3}, {"rally", 2},      {"gain", 2},      {"rise", 2},         {"jump", 2},
        {"climb", 2},       {"spike", 2},      {"rebound", 2},   {"boost", 2},        {"beat", 2},
        {"exceed", 2},      {"upgrade", 2},    {"profit", 2},    {"growth", 2},       {"expand", 2},
        {"recover", 2},     {"victory", 2},    {"ceasefire", 2}, {"treaty", 2},       {"reform", 2},
        {"optimism", 2},    {"milestone", 2},  {"strong", 1},    {"robust", 1},       {"stellar", 1},
        {"buy", 1},         {"positive", 1},   {"success", 1},   {"win", 1},          {"approval", 1},
        {"deal", 1},        {"confidence", 1}, {"dividend", 1},  {"progress", 1},     {"improve", 1},
        {"hope", 1},        {"support", 1},    {"bolster", 1},   {"outperform", 1},   {"bullish", 1},
        {"upside", 1},      {"favorable", 1},  {"momentum", 1},  {"launch", 1},       {"unveil", 1},
    };

    static const WordWeight negatives[] = {
        {"crash", 3},      {"plunge", 3},    {"collapse", 3},   {"devastat", 3},  {"catastroph", 3}, {"invasion", 3},
        {"war crime", 3},  {"nuclear", 3},   {"bankruptcy", 3}, {"meltdown", 3},  {"fall", 2},       {"drop", 2},
        {"decline", 2},    {"tumble", 2},    {"slide", 2},      {"slump", 2},     {"miss", 2},       {"disappoint", 2},
        {"fail", 2},       {"recession", 2}, {"crisis", 2},     {"conflict", 2},  {"attack", 2},     {"kill", 2},
        {"sanction", 2},   {"tariff", 2},    {"escalat", 2},    {"layoff", 2},    {"downgrade", 2},  {"default", 2},
        {"fraud", 2},      {"scandal", 2},   {"coup", 2},       {"protest", 2},   {"disaster", 2},   {"worst", 1},
        {"weak", 1},       {"loss", 1},      {"deficit", 1},    {"fear", 1},      {"risk", 1},       {"threat", 1},
        {"warning", 1},    {"sell", 1},      {"debt", 1},       {"inflation", 1}, {"slowdown", 1},   {"bearish", 1},
        {"negative", 1},   {"volatile", 1},  {"uncertain", 1},  {"reject", 1},    {"ban", 1},        {"suspend", 1},
        {"investigat", 1}, {"probe", 1},     {"hack", 1},       {"leak", 1},      {"shortage", 1},   {"disrupt", 1},
        {"shrink", 1},
    };

    int pos = 0, neg = 0;
    for (const auto& [w, wt] : positives) {
        if (text.contains(w))
            pos += wt;
    }
    for (const auto& [w, wt] : negatives) {
        if (text.contains(w))
            neg += wt;
    }

    int net = pos - neg;
    if (net >= 1)
        article.sentiment = Sentiment::BULLISH;
    else if (net <= -1)
        article.sentiment = Sentiment::BEARISH;
    // else stays NEUTRAL

    // Impact
    int strength = std::abs(net);
    if (article.priority == Priority::FLASH || article.priority == Priority::URGENT || strength >= 6)
        article.impact = Impact::HIGH;
    else if (article.priority == Priority::BREAKING || strength >= 3)
        article.impact = Impact::MEDIUM;

    // Category refinement
    if (text.contains("earnings") || text.contains("quarterly results") || text.contains("eps") ||
        text.contains("guidance"))
        article.category = "EARNINGS";
    else if (text.contains("crypto") || text.contains("bitcoin") || text.contains("ethereum") ||
             text.contains("blockchain"))
        article.category = "CRYPTO";
    else if (text.contains("missile") || text.contains("troops") || text.contains("pentagon") ||
             text.contains("military"))
        article.category = "DEFENSE";
    else if (text.contains("fed ") || text.contains("federal reserve") || text.contains("inflation") ||
             text.contains("gdp") || text.contains("interest rate") || text.contains("central bank"))
        article.category = "ECONOMIC";
    else if (text.contains("s&p 500") || text.contains("nasdaq") || text.contains("dow jones") ||
             text.contains("stock market"))
        article.category = "MARKETS";
    else if (text.contains("energy") || text.contains("crude") || text.contains("opec") ||
             text.contains("natural gas") || text.contains("oil price"))
        article.category = "ENERGY";
    else if (text.contains("tech") || text.contains(" ai ") || text.contains("artificial intelligence") ||
             text.contains("semiconductor") || text.contains("startup"))
        article.category = "TECH";
    else if (text.contains("nato") || text.contains("ukraine") || text.contains("russia") || text.contains("china") ||
             text.contains("gaza") || text.contains("sanctions") || text.contains("geopolit"))
        article.category = "GEOPOLITICS";

    // Extract tickers: uppercase 2-5 letter words
    static QRegularExpression ticker_re("\\b[A-Z]{2,5}\\b");
    static QSet<QString> common_words = {"THE",  "FOR",  "AND",  "BUT",  "NOT",  "FROM", "WITH", "THIS", "THAT", "HAVE",
                                         "WILL", "BEEN", "THEY", "WERE", "SAID", "HAS",  "ITS",  "NEW",  "ARE",  "WAS"};
    auto it = ticker_re.globalMatch(combined); // reuse already-built string
    QSet<QString> found;
    while (it.hasNext() && found.size() < 5) {
        auto m = it.next();
        QString t = m.captured();
        if (!common_words.contains(t))
            found.insert(t);
    }
    article.tickers = found.values();

    // Language detection — check for CJK, Cyrillic, Arabic, Devanagari characters
    auto detect_lang = [](const QString& s) -> QString {
        int cjk = 0, cyrillic = 0, arabic = 0, devanagari = 0;
        for (const auto& ch : s) {
            ushort u = ch.unicode();
            if (u >= 0x4e00 && u <= 0x9fff)
                cjk++;
            else if (u >= 0x3040 && u <= 0x30ff)
                return "ja"; // kana = definitely Japanese
            else if (u >= 0xac00 && u <= 0xd7af)
                return "ko"; // hangul = Korean
            else if (u >= 0x0400 && u <= 0x04ff)
                cyrillic++;
            else if (u >= 0x0600 && u <= 0x06ff)
                arabic++;
            else if (u >= 0x0900 && u <= 0x097f)
                devanagari++;
        }
        int total = s.size();
        if (total == 0)
            return "en";
        if (cjk * 10 > total)
            return "zh";
        if (cyrillic * 10 > total)
            return "ru";
        if (arabic * 10 > total)
            return "ar";
        if (devanagari * 10 > total)
            return "hi";
        return "en";
    };
    article.lang = detect_lang(article.headline);

    // Threat classification — pass pre-built text to avoid a 3rd toLower()
    article.threat = classify_threat(article, text);

    // Source credibility flag
    article.source_flag = source_flag_for(article.source);
}

// ── Threat classification with confidence ───────────────────────────────────

} // namespace fincept::services
