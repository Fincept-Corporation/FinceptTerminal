#include "services/news/NewsService.h"
#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include <QXmlStreamReader>
#include <QJsonDocument>
#include <QDateTime>
#include <QSet>
#include <QRegularExpression>
#include <QUuid>
#include <algorithm>
#include <memory>
#include <QMutex>
#include <QMutexLocker>
#include <QAtomicInt>

namespace fincept::services {

// ── Singleton ───────────────────────────────────────────────────────────────

NewsService& NewsService::instance() {
    static NewsService s;
    return s;
}

NewsService::NewsService() {
    nam_ = new QNetworkAccessManager(this);
    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(cache_ttl_sec_ * 1000);
    connect(refresh_timer_, &QTimer::timeout, this, [this]() {
        fetch_all_news(true, [](bool, QVector<NewsArticle>) {});
    });
}

// ── Fetch all RSS feeds in parallel ─────────────────────────────────────────

void NewsService::fetch_all_news(bool force, ArticlesCallback cb) {
    auto now = QDateTime::currentSecsSinceEpoch();
    if (!force && !cache_.isEmpty() && (now - cache_ts_) < cache_ttl_sec_) {
        cb(true, cache_);
        return;
    }

    auto feeds = default_feeds();
    feed_count_ = feeds.size();

    // Shared state for collecting results from parallel requests
    struct FetchState {
        QMutex mutex;
        QVector<NewsArticle> all_articles;
        QAtomicInt remaining{0};
        ArticlesCallback callback;
        NewsService* service = nullptr;
    };

    auto state = std::make_shared<FetchState>();
    state->remaining.storeRelaxed(feeds.size());
    state->callback = std::move(cb);
    state->service = this;

    for (const auto& feed : feeds) {
        QNetworkRequest req(QUrl(feed.url));
        req.setHeader(QNetworkRequest::UserAgentHeader, "FinceptTerminal/4.0");
        req.setRawHeader("Accept", "application/rss+xml, application/xml, text/xml, */*");
        req.setTransferTimeout(10000); // 10s timeout per feed

        auto* reply = nam_->get(req);
        connect(reply, &QNetworkReply::finished, this, [reply, feed, state]() {
            reply->deleteLater();

            QVector<NewsArticle> articles;
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray data = reply->readAll();
                if (data.trimmed().startsWith('<')) {
                    articles = parse_rss_xml(data, feed);
                }
            }

            {
                QMutexLocker lock(&state->mutex);
                state->all_articles.append(articles);
            }

            if (state->remaining.fetchAndSubRelaxed(1) == 1) {
                // Last feed done — sort by time descending
                auto& all = state->all_articles;
                std::sort(all.begin(), all.end(), [](const NewsArticle& a, const NewsArticle& b) {
                    return a.sort_ts > b.sort_ts;
                });

                // Update service cache
                QSet<QString> sources;
                for (const auto& a : all) sources.insert(a.source);
                state->service->active_sources_ = sources.values();
                state->service->cache_ = all;
                state->service->cache_ts_ = QDateTime::currentSecsSinceEpoch();

                LOG_INFO("NewsService", QString("Fetched %1 articles from %2 sources")
                    .arg(all.size()).arg(sources.size()));

                state->callback(true, all);
                emit state->service->articles_updated(all);
            }
        });
    }
}

// ── AI Analysis via Fincept API ─────────────────────────────────────────────

void NewsService::analyze_article(const QString& url, AnalysisCallback cb) {
    QJsonObject body;
    body["url"] = url;

    HttpClient::instance().post("/news/analyze", body,
        [this, cb](Result<QJsonDocument> result) {
            if (result.is_err()) {
                LOG_ERROR("NewsService", "Analysis failed: " + QString::fromStdString(result.error()));
                cb(false, {});
                return;
            }

            auto obj = result.value().object();
            if (!obj["success"].toBool(false)) {
                LOG_ERROR("NewsService", "API returned failure: " + obj["message"].toString());
                cb(false, {});
                return;
            }

            auto data = obj["data"].toObject();
            auto a = data["analysis"].toObject();
            auto sent = a["sentiment"].toObject();
            auto mi = a["market_impact"].toObject();
            auto rs = a["risk_signals"].toObject();

            NewsAnalysis analysis;
            analysis.sentiment = {sent["score"].toDouble(), sent["intensity"].toDouble(), sent["confidence"].toDouble()};
            analysis.market_impact = {mi["urgency"].toString(), mi["prediction"].toString()};
            analysis.summary = a["summary"].toString();
            analysis.credits_used = data["credits_used"].toInt();
            analysis.credits_remaining = data["credits_remaining"].toInt();

            for (const auto& v : a["keywords"].toArray()) analysis.keywords << v.toString();
            for (const auto& v : a["topics"].toArray()) analysis.topics << v.toString();
            for (const auto& v : a["key_points"].toArray()) analysis.key_points << v.toString();

            auto reg = rs["regulatory"].toObject();
            auto geo = rs["geopolitical"].toObject();
            auto ops = rs["operational"].toObject();
            auto mkt = rs["market"].toObject();
            analysis.regulatory = {reg["level"].toString(), reg["details"].toString()};
            analysis.geopolitical = {geo["level"].toString(), geo["details"].toString()};
            analysis.operational = {ops["level"].toString(), ops["details"].toString()};
            analysis.market = {mkt["level"].toString(), mkt["details"].toString()};

            cb(true, analysis);
            emit this->analysis_ready(analysis);
        });
}

// ── Auto-refresh ────────────────────────────────────────────────────────────

void NewsService::set_refresh_interval(int minutes) {
    cache_ttl_sec_ = minutes * 60;
    refresh_timer_->setInterval(cache_ttl_sec_ * 1000);
}

void NewsService::start_auto_refresh() { refresh_timer_->start(); }
void NewsService::stop_auto_refresh()  { refresh_timer_->stop(); }

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
                    if (current.link.isEmpty()) current.link = href;
                }
            }
        }
        else if (token == QXmlStreamReader::Characters && in_item) {
            QString text = reader.text().toString().trimmed();
            if (text.isEmpty()) continue;

            if (current_tag == "title" && current.headline.isEmpty()) {
                current.headline = text.left(200);
            }
            else if ((current_tag == "description" || current_tag == "summary" || current_tag == "encoded") && current.summary.isEmpty()) {
                current.summary = strip_html(text).left(300);
            }
            else if (current_tag == "link" && current.link.isEmpty()) {
                current.link = text.trimmed();
            }
            else if ((current_tag == "guid" || current_tag == "id") && current.link.isEmpty()) {
                // guid/id often contains the article URL as fallback
                if (text.startsWith("http")) current.link = text.trimmed();
            }
            else if (current_tag == "pubDate" || current_tag == "published" || current_tag == "updated" || current_tag == "date") {
                if (current.sort_ts == 0) {
                    QDateTime dt = QDateTime::fromString(text, Qt::RFC2822Date);
                    if (!dt.isValid()) dt = QDateTime::fromString(text, Qt::ISODate);
                    if (!dt.isValid()) dt = QDateTime::fromString(text, "ddd, dd MMM yyyy HH:mm:ss");
                    if (dt.isValid()) {
                        current.sort_ts = dt.toSecsSinceEpoch();
                        current.time = dt.toString("MMM dd, HH:mm");
                    } else {
                        current.time = text.left(22);
                    }
                }
            }
        }
        else if (token == QXmlStreamReader::EndElement) {
            QString tag = reader.name().toString();
            if ((tag == "item" || tag == "entry") && in_item) {
                in_item = false;
                if (current.headline.isEmpty()) continue;

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
    QString text = (article.headline + " " + article.summary).toLower();

    // Priority
    if (text.contains("breaking") || text.contains("alert"))
        article.priority = Priority::FLASH;
    else if (text.contains("urgent") || text.contains("emergency"))
        article.priority = Priority::URGENT;
    else if (text.contains("announce") || text.contains("report"))
        article.priority = Priority::BREAKING;

    // Weighted sentiment — matching Tauri's word lists
    struct WordWeight { const char* word; int weight; };

    static const WordWeight positives[] = {
        {"surge",3},{"soar",3},{"skyrocket",3},{"breakthrough",3},{"boom",3},{"record high",3},
        {"rally",2},{"gain",2},{"rise",2},{"jump",2},{"climb",2},{"spike",2},{"rebound",2},
        {"boost",2},{"beat",2},{"exceed",2},{"upgrade",2},{"profit",2},{"growth",2},
        {"expand",2},{"recover",2},{"victory",2},{"ceasefire",2},{"treaty",2},{"reform",2},
        {"optimism",2},{"milestone",2},
        {"strong",1},{"robust",1},{"stellar",1},{"buy",1},{"positive",1},{"success",1},
        {"win",1},{"approval",1},{"deal",1},{"confidence",1},{"dividend",1},{"progress",1},
        {"improve",1},{"hope",1},{"support",1},{"bolster",1},{"outperform",1},{"bullish",1},
        {"upside",1},{"favorable",1},{"momentum",1},{"launch",1},{"unveil",1},
    };

    static const WordWeight negatives[] = {
        {"crash",3},{"plunge",3},{"collapse",3},{"devastat",3},{"catastroph",3},
        {"invasion",3},{"war crime",3},{"nuclear",3},{"bankruptcy",3},{"meltdown",3},
        {"fall",2},{"drop",2},{"decline",2},{"tumble",2},{"slide",2},{"slump",2},
        {"miss",2},{"disappoint",2},{"fail",2},{"recession",2},{"crisis",2},
        {"conflict",2},{"attack",2},{"kill",2},{"sanction",2},{"tariff",2},
        {"escalat",2},{"layoff",2},{"downgrade",2},{"default",2},{"fraud",2},
        {"scandal",2},{"coup",2},{"protest",2},{"disaster",2},
        {"worst",1},{"weak",1},{"loss",1},{"deficit",1},{"fear",1},{"risk",1},
        {"threat",1},{"warning",1},{"sell",1},{"debt",1},{"inflation",1},
        {"slowdown",1},{"bearish",1},{"negative",1},{"volatile",1},{"uncertain",1},
        {"reject",1},{"ban",1},{"suspend",1},{"investigat",1},{"probe",1},
        {"hack",1},{"leak",1},{"shortage",1},{"disrupt",1},{"shrink",1},
    };

    int pos = 0, neg = 0;
    for (const auto& [w, wt] : positives) { if (text.contains(w)) pos += wt; }
    for (const auto& [w, wt] : negatives) { if (text.contains(w)) neg += wt; }

    int net = pos - neg;
    if (net >= 1)       article.sentiment = Sentiment::BULLISH;
    else if (net <= -1) article.sentiment = Sentiment::BEARISH;
    // else stays NEUTRAL

    // Impact
    int strength = std::abs(net);
    if (article.priority == Priority::FLASH || article.priority == Priority::URGENT || strength >= 6)
        article.impact = Impact::HIGH;
    else if (article.priority == Priority::BREAKING || strength >= 3)
        article.impact = Impact::MEDIUM;

    // Category refinement
    if (text.contains("earnings") || text.contains("quarterly results") || text.contains("eps") || text.contains("guidance"))
        article.category = "EARNINGS";
    else if (text.contains("crypto") || text.contains("bitcoin") || text.contains("ethereum") || text.contains("blockchain"))
        article.category = "CRYPTO";
    else if (text.contains("missile") || text.contains("troops") || text.contains("pentagon") || text.contains("military"))
        article.category = "DEFENSE";
    else if (text.contains("fed ") || text.contains("federal reserve") || text.contains("inflation") || text.contains("gdp") || text.contains("interest rate") || text.contains("central bank"))
        article.category = "ECONOMIC";
    else if (text.contains("s&p 500") || text.contains("nasdaq") || text.contains("dow jones") || text.contains("stock market"))
        article.category = "MARKETS";
    else if (text.contains("energy") || text.contains("crude") || text.contains("opec") || text.contains("natural gas") || text.contains("oil price"))
        article.category = "ENERGY";
    else if (text.contains("tech") || text.contains(" ai ") || text.contains("artificial intelligence") || text.contains("semiconductor") || text.contains("startup"))
        article.category = "TECH";
    else if (text.contains("nato") || text.contains("ukraine") || text.contains("russia") || text.contains("china") || text.contains("gaza") || text.contains("sanctions") || text.contains("geopolit"))
        article.category = "GEOPOLITICS";

    // Extract tickers: uppercase 2-5 letter words
    static QRegularExpression ticker_re("\\b[A-Z]{2,5}\\b");
    static QSet<QString> common_words = {"THE","FOR","AND","BUT","NOT","FROM","WITH","THIS","THAT","HAVE","WILL","BEEN","THEY","WERE","SAID","HAS","ITS","NEW","ARE","WAS"};
    auto it = ticker_re.globalMatch(article.headline + " " + article.summary);
    QSet<QString> found;
    while (it.hasNext() && found.size() < 5) {
        auto m = it.next();
        QString t = m.captured();
        if (!common_words.contains(t)) found.insert(t);
    }
    article.tickers = found.values();
}

// ── Default RSS feeds — matching Tauri's 100+ feeds ─────────────────────────

QVector<RSSFeed> NewsService::default_feeds() {
    return {
        // Tier 1 — Wire Services & Regulators
        {"reuters-world",  "Reuters World",        "https://feeds.reuters.com/Reuters/worldNews",           "GEOPOLITICS", "GLOBAL", "REUTERS",         1},
        {"reuters-biz",    "Reuters Business",      "https://feeds.reuters.com/reuters/businessNews",        "MARKETS",     "GLOBAL", "REUTERS",         1},
        {"reuters-mkts",   "Reuters Markets",       "https://feeds.reuters.com/reuters/financialsNews",      "MARKETS",     "GLOBAL", "REUTERS",         1},
        {"ap-top",         "AP Top News",           "https://rsshub.app/apnews/topics/ap-top-news",          "GEOPOLITICS", "GLOBAL", "AP",              1},
        {"sec-press",      "SEC Press Releases",    "https://www.sec.gov/news/pressreleases.rss",            "REGULATORY",  "US",     "SEC",             1},
        {"fed-press",      "Federal Reserve",       "https://www.federalreserve.gov/feeds/press_all.xml",    "REGULATORY",  "US",     "FEDERAL RESERVE", 1},
        {"un-news",        "UN News",               "https://news.un.org/feed/subscribe/en/news/all/rss.xml","GEOPOLITICS", "GLOBAL", "UN",              1},
        {"imf-news",       "IMF News",              "https://www.imf.org/en/News/rss?language=eng",          "ECONOMIC",    "GLOBAL", "IMF",             1},

        // Tier 2 — Major Financial Media
        {"bloomberg-mkts", "Bloomberg Markets",     "https://feeds.bloomberg.com/markets/news.rss",          "MARKETS",     "GLOBAL", "BLOOMBERG",       2},
        {"wsj-markets",    "WSJ Markets",           "https://feeds.a.dj.com/rss/RSSMarketsMain.xml",         "MARKETS",     "US",     "WSJ",             2},
        {"wsj-world",      "WSJ World",             "https://feeds.a.dj.com/rss/RSSWorldNews.xml",           "GEOPOLITICS", "GLOBAL", "WSJ",             2},
        {"marketwatch",    "MarketWatch",           "https://feeds.marketwatch.com/marketwatch/topstories/",  "MARKETS",     "US",     "MARKETWATCH",     2},
        {"cnbc-finance",   "CNBC Finance",          "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=100003114","MARKETS","US","CNBC",2},
        {"seekingalpha",   "Seeking Alpha",         "https://seekingalpha.com/market_currents.xml",           "MARKETS",     "US",     "SEEKING ALPHA",   2},

        // Tier 2 — Global News
        {"bbc-world",      "BBC World",             "http://feeds.bbci.co.uk/news/world/rss.xml",             "GEOPOLITICS", "GLOBAL", "BBC",             2},
        {"bbc-business",   "BBC Business",          "http://feeds.bbci.co.uk/news/business/rss.xml",          "MARKETS",     "GLOBAL", "BBC",             2},
        {"aljazeera",      "Al Jazeera",            "https://www.aljazeera.com/xml/rss/all.xml",              "GEOPOLITICS", "GLOBAL", "AL JAZEERA",      2},
        {"nyt-world",      "NYT World",             "https://rss.nytimes.com/services/xml/rss/nyt/World.xml", "GEOPOLITICS", "GLOBAL", "NYT",             2},
        {"guardian-world", "Guardian World",        "https://www.theguardian.com/world/rss",                  "GEOPOLITICS", "GLOBAL", "GUARDIAN",        2},
        {"france24",       "France 24",             "https://www.france24.com/en/rss",                        "GEOPOLITICS", "EU",     "FRANCE 24",       2},

        // Tier 2 — Geopolitics & Defense
        {"foreignpolicy",  "Foreign Policy",        "https://foreignpolicy.com/feed/",                        "GEOPOLITICS", "GLOBAL", "FOREIGN POLICY",  2},
        {"defensenews",    "Defense News",          "https://www.defensenews.com/rss/",                       "GEOPOLITICS", "GLOBAL", "DEFENSE NEWS",    2},

        // Tier 2 — Energy & Commodities
        {"oilprice",       "OilPrice.com",          "https://oilprice.com/rss/main",                          "ENERGY",      "GLOBAL", "OILPRICE",        2},
        {"kitco",          "Kitco Gold",            "https://www.kitco.com/rss/news/",                        "MARKETS",     "GLOBAL", "KITCO",           2},

        // Tier 2 — Tech
        {"techcrunch",     "TechCrunch",            "https://techcrunch.com/feed/",                            "TECH",        "GLOBAL", "TECHCRUNCH",      2},
        {"wired",          "Wired",                 "https://www.wired.com/feed/rss",                          "TECH",        "US",     "WIRED",           2},

        // Tier 2 — Forex
        {"fxstreet",       "FXStreet",              "https://www.fxstreet.com/rss/news",                       "MARKETS",     "GLOBAL", "FXSTREET",        2},

        // Tier 2 — Asia
        {"scmp",           "South China Morning Post","https://www.scmp.com/rss/91/feed",                     "GEOPOLITICS", "ASIA",   "SCMP",            2},
        {"nikkei-asia",    "Nikkei Asia",           "https://asia.nikkei.com/rss/feed/nar",                    "MARKETS",     "ASIA",   "NIKKEI ASIA",     2},
        {"hindu-biz",      "The Hindu Business",    "https://www.thehindu.com/business/feeder/default.rss",    "MARKETS",     "INDIA",  "THE HINDU",       2},

        // Tier 3 — Economic / Macro
        {"zero-hedge",     "ZeroHedge",             "https://feeds.feedburner.com/zerohedge/feed",             "ECONOMIC",    "GLOBAL", "ZEROHEDGE",       3},
        {"calculated-risk","Calculated Risk",       "https://feeds.feedburner.com/CalculatedRisk",             "ECONOMIC",    "US",     "CALCULATED RISK", 3},

        // Tier 2 — MENA
        {"middle-east-eye","Middle East Eye",       "https://www.middleeasteye.net/rss",                       "GEOPOLITICS", "MENA",   "MIDDLE EAST EYE", 2},
    };
}

// ── Free helpers ────────────────────────────────────────────────────────────

QString priority_string(Priority p) {
    switch (p) {
        case Priority::FLASH:    return "FLASH";
        case Priority::URGENT:   return "URGENT";
        case Priority::BREAKING: return "BREAKING";
        case Priority::ROUTINE:  return "ROUTINE";
    }
    return "ROUTINE";
}

QString sentiment_string(Sentiment s) {
    switch (s) {
        case Sentiment::BULLISH: return "BULLISH";
        case Sentiment::BEARISH: return "BEARISH";
        case Sentiment::NEUTRAL: return "NEUTRAL";
    }
    return "NEUTRAL";
}

QString impact_string(Impact i) {
    switch (i) {
        case Impact::HIGH:   return "HIGH";
        case Impact::MEDIUM: return "MEDIUM";
        case Impact::LOW:    return "LOW";
    }
    return "LOW";
}

QString priority_color(Priority p) {
    switch (p) {
        case Priority::FLASH:    return "#dc2626";
        case Priority::URGENT:   return "#d97706";
        case Priority::BREAKING: return "#ca8a04";
        case Priority::ROUTINE:  return "#525252";
    }
    return "#525252";
}

QString sentiment_color(Sentiment s) {
    switch (s) {
        case Sentiment::BULLISH: return "#16a34a";
        case Sentiment::BEARISH: return "#dc2626";
        case Sentiment::NEUTRAL: return "#ca8a04";
    }
    return "#ca8a04";
}

QString relative_time(int64_t unix_ts) {
    if (unix_ts <= 0) return {};
    auto now = QDateTime::currentSecsSinceEpoch();
    auto d = now - unix_ts;
    if (d < 0)     return "now";
    if (d < 60)    return QString("%1s").arg(d);
    if (d < 3600)  return QString("%1m").arg(d / 60);
    if (d < 86400) return QString("%1h").arg(d / 3600);
    return QString("%1d").arg(d / 86400);
}

} // namespace fincept::services
