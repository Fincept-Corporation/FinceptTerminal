// src/services/news/NewsService_Feeds.cpp
//
// Static catalog of default RSS feeds. ~285 lines of data; isolated from
// the rest of the service so churn here doesn't recompile the network path.
//
// Part of the partial-class split of NewsService.cpp.

#include "services/news/NewsService.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "storage/cache/CacheManager.h"
#include "storage/repositories/RssFeedRepository.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"

#include <QHash>

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

QVector<RSSFeed> NewsService::default_feeds() {
    return {
        // Tier 1 — Wire Services & Regulators
        // Reuters discontinued public RSS in 2020 (feeds.reuters.com is dead).
        // We keep tier-1 coverage via AP, BBC, FT, WSJ and other majors instead.
        {"ap-top", "AP Top News", "https://rsshub.app/apnews/topics/ap-top-news", "GEOPOLITICS", "GLOBAL", "AP", 1},
        {"sec-press", "SEC Press Releases", "https://www.sec.gov/news/pressreleases.rss", "REGULATORY", "US", "SEC", 1},
        {"fed-press", "Federal Reserve", "https://www.federalreserve.gov/feeds/press_all.xml", "REGULATORY", "US",
         "FEDERAL RESERVE", 1},
        {"un-news", "UN News", "https://news.un.org/feed/subscribe/en/news/all/rss.xml", "GEOPOLITICS", "GLOBAL", "UN",
         1},
        // (IMF News removed — endpoint serves an Akamai access-denied HTML page,
        //  not RSS. Fincept's macro coverage is already provided by WSJ /
        //  Economist / IMF press is reachable via UN feeds.)

        // Tier 2 — Major Financial Media
        {"bloomberg-mkts", "Bloomberg Markets", "https://feeds.bloomberg.com/markets/news.rss", "MARKETS", "GLOBAL",
         "BLOOMBERG", 2},
        {"wsj-markets", "WSJ Markets", "https://feeds.a.dj.com/rss/RSSMarketsMain.xml", "MARKETS", "US", "WSJ", 2},
        {"wsj-world", "WSJ World", "https://feeds.a.dj.com/rss/RSSWorldNews.xml", "GEOPOLITICS", "GLOBAL", "WSJ", 2},
        {"marketwatch", "MarketWatch", "https://feeds.marketwatch.com/marketwatch/topstories/", "MARKETS", "US",
         "MARKETWATCH", 2},
        {"cnbc-finance", "CNBC Finance",
         "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=100003114", "MARKETS", "US",
         "CNBC", 2},
        {"seekingalpha", "Seeking Alpha", "https://seekingalpha.com/market_currents.xml", "MARKETS", "US",
         "SEEKING ALPHA", 2},

        // Tier 2 — Global News
        {"bbc-world", "BBC World", "http://feeds.bbci.co.uk/news/world/rss.xml", "GEOPOLITICS", "GLOBAL", "BBC", 2},
        {"bbc-business", "BBC Business", "http://feeds.bbci.co.uk/news/business/rss.xml", "MARKETS", "GLOBAL", "BBC",
         2},
        {"aljazeera", "Al Jazeera", "https://www.aljazeera.com/xml/rss/all.xml", "GEOPOLITICS", "GLOBAL", "AL JAZEERA",
         2},
        {"nyt-world", "NYT World", "https://rss.nytimes.com/services/xml/rss/nyt/World.xml", "GEOPOLITICS", "GLOBAL",
         "NYT", 2},
        {"guardian-world", "Guardian World", "https://www.theguardian.com/world/rss", "GEOPOLITICS", "GLOBAL",
         "GUARDIAN", 2},
        {"france24", "France 24", "https://www.france24.com/en/rss", "GEOPOLITICS", "EU", "FRANCE 24", 2},

        // Tier 2 — Geopolitics & Defense
        {"foreignpolicy", "Foreign Policy", "https://foreignpolicy.com/feed/", "GEOPOLITICS", "GLOBAL",
         "FOREIGN POLICY", 2},
        // (defensenews.com/rss/ returns 404 — endpoint discontinued.)

        // Tier 2 — Energy & Commodities
        {"oilprice", "OilPrice.com", "https://oilprice.com/rss/main", "ENERGY", "GLOBAL", "OILPRICE", 2},
        // (Kitco RSS endpoint removed — kitco.com no longer publishes RSS.)

        // Tier 2 — Tech
        {"techcrunch", "TechCrunch", "https://techcrunch.com/feed/", "TECH", "GLOBAL", "TECHCRUNCH", 2},
        {"wired", "Wired", "https://www.wired.com/feed/rss", "TECH", "US", "WIRED", 2},

        // Tier 2 — Forex
        {"fxstreet", "FXStreet", "https://www.fxstreet.com/rss/news", "MARKETS", "GLOBAL", "FXSTREET", 2},

        // Tier 2 — Asia
        {"scmp", "South China Morning Post", "https://www.scmp.com/rss/91/feed", "GEOPOLITICS", "ASIA", "SCMP", 2},
        {"nikkei-asia", "Nikkei Asia", "https://asia.nikkei.com/rss/feed/nar", "MARKETS", "ASIA", "NIKKEI ASIA", 2},
        {"hindu-biz", "The Hindu Business", "https://www.thehindu.com/business/feeder/default.rss", "MARKETS", "INDIA",
         "THE HINDU", 2},

        // Tier 2 — MENA
        {"middle-east-eye", "Middle East Eye", "https://www.middleeasteye.net/rss", "GEOPOLITICS", "MENA",
         "MIDDLE EAST EYE", 2},

        // ── Additional feeds (29→80+) ──────────────────────────────────────

        // Tier 1 — Wire (additional)
        // (Reuters tech RSS removed — feed discontinued.)

        // Tier 2 — Major Financial (additional)
        {"cnbc-world", "CNBC World",
         "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=100727362", "MARKETS", "GLOBAL",
         "CNBC", 2},
        {"cnbc-tech", "CNBC Technology",
         "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=19854910", "TECH", "US", "CNBC",
         2},
        {"investing-news", "Investing.com", "https://www.investing.com/rss/news.rss", "MARKETS", "GLOBAL",
         "INVESTING.COM", 2},
        {"economist", "The Economist", "https://www.economist.com/finance-and-economics/rss.xml", "ECONOMIC", "GLOBAL",
         "ECONOMIST", 2},

        // Tier 2 — Crypto
        {"coindesk", "CoinDesk", "https://www.coindesk.com/arc/outboundfeeds/rss/", "CRYPTO", "GLOBAL", "COINDESK", 2},
        {"cointelegraph", "CoinTelegraph", "https://cointelegraph.com/rss", "CRYPTO", "GLOBAL", "COINTELEGRAPH", 2},
        {"theblock", "The Block", "https://www.theblock.co/rss.xml", "CRYPTO", "GLOBAL", "THE BLOCK", 2},
        {"decrypt", "Decrypt", "https://decrypt.co/feed", "CRYPTO", "GLOBAL", "DECRYPT", 2},

        // Tier 1 — Central Banks & Regulators
        {"ecb-press", "ECB Press", "https://www.ecb.europa.eu/rss/press.html", "REGULATORY", "EU", "ECB", 1},
        {"boe-news", "Bank of England", "https://www.bankofengland.co.uk/rss/news", "REGULATORY", "UK", "BOE", 1},

        // Tier 2 — Commodities (additional)
        {"mining-com", "Mining.com", "https://www.mining.com/feed/", "MARKETS", "GLOBAL", "MINING.COM", 2},

        // Tier 2 — US Markets (additional)
        {"benzinga", "Benzinga", "https://www.benzinga.com/feed", "MARKETS", "US", "BENZINGA", 2},

        // Tier 2 — Europe
        {"dw-world", "Deutsche Welle", "https://rss.dw.com/rdf/rss-en-all", "GEOPOLITICS", "EU", "DW", 2},

        // Tier 2 — Asia & India (additional)
        {"livemint", "LiveMint", "https://www.livemint.com/rss/markets", "MARKETS", "INDIA", "LIVEMINT", 2},
        {"et-markets", "Economic Times", "https://economictimes.indiatimes.com/rssfeedstopstories.cms", "MARKETS",
         "INDIA", "ECONOMIC TIMES", 2},
        {"moneycontrol", "MoneyControl", "https://www.moneycontrol.com/rss/latestnews.xml", "MARKETS", "INDIA",
         "MONEYCONTROL", 2},
        {"channel-news-asia", "CNA", "https://www.channelnewsasia.com/rssfeeds/8395986", "MARKETS", "ASIA", "CNA", 2},

        // Tier 2 — Fintech
        {"finextra", "Finextra", "https://www.finextra.com/rss/headlines.aspx", "TECH", "GLOBAL", "FINEXTRA", 2},

        // Tier 3 — Economic / Macro
        {"zero-hedge", "ZeroHedge", "https://feeds.feedburner.com/zerohedge/feed", "ECONOMIC", "GLOBAL", "ZEROHEDGE",
         3},
        {"calculated-risk", "Calculated Risk", "https://feeds.feedburner.com/CalculatedRisk", "ECONOMIC", "US",
         "CALCULATED RISK", 3},
        {"wolfstreet", "Wolf Street", "https://wolfstreet.com/feed/", "ECONOMIC", "US", "WOLF STREET", 3},

        // Tier 3 — Defense & Security
        // (defenseone.com/rss/ returns 404 — endpoint discontinued.)
        {"bellingcat", "Bellingcat", "https://www.bellingcat.com/feed/", "GEOPOLITICS", "GLOBAL", "BELLINGCAT", 3},

        // Tier 3 — Tech (additional)
        {"arstechnica", "Ars Technica", "https://feeds.arstechnica.com/arstechnica/index", "TECH", "GLOBAL",
         "ARS TECHNICA", 3},
        {"theverge", "The Verge", "https://www.theverge.com/rss/index.xml", "TECH", "GLOBAL", "THE VERGE", 3},
        {"mit-tech", "MIT Tech Review", "https://www.technologyreview.com/feed/", "TECH", "GLOBAL", "MIT TECH REVIEW",
         3},

        // Tier 3 — ESG
        {"carbon-brief", "Carbon Brief", "https://www.carbonbrief.org/feed/", "ENERGY", "GLOBAL", "CARBON BRIEF", 3},

        // Tier 4 — Blogs & Aggregators
        {"hackernews", "Hacker News", "https://hnrss.org/frontpage", "TECH", "GLOBAL", "HACKER NEWS", 4},
        {"abnormal-returns", "Abnormal Returns", "https://abnormalreturns.com/feed/", "MARKETS", "US",
         "ABNORMAL RETURNS", 4},
        {"marginal-rev", "Marginal Revolution", "https://marginalrevolution.com/feed", "ECONOMIC", "GLOBAL",
         "MARGINAL REVOLUTION", 4},
    };
}

// ── Free helpers ────────────────────────────────────────────────────────────

QString priority_string(Priority p) {
    switch (p) {
        case Priority::FLASH:
            return "FLASH";
        case Priority::URGENT:
            return "URGENT";
        case Priority::BREAKING:
            return "BREAKING";
        case Priority::ROUTINE:
            return "ROUTINE";
    }
    return "ROUTINE";
}

QString sentiment_string(Sentiment s) {
    switch (s) {
        case Sentiment::BULLISH:
            return "BULLISH";
        case Sentiment::BEARISH:
            return "BEARISH";
        case Sentiment::NEUTRAL:
            return "NEUTRAL";
    }
    return "NEUTRAL";
}

QString impact_string(Impact i) {
    switch (i) {
        case Impact::HIGH:
            return "HIGH";
        case Impact::MEDIUM:
            return "MEDIUM";
        case Impact::LOW:
            return "LOW";
    }
    return "LOW";
}

QString priority_color(Priority p) {
    switch (p) {
        case Priority::FLASH:
            return "#dc2626";
        case Priority::URGENT:
            return "#d97706";
        case Priority::BREAKING:
            return "#ca8a04";
        case Priority::ROUTINE:
            return "#525252";
    }
    return "#525252";
}

QString sentiment_color(Sentiment s) {
    switch (s) {
        case Sentiment::BULLISH:
            return "#16a34a";
        case Sentiment::BEARISH:
            return "#dc2626";
        case Sentiment::NEUTRAL:
            return "#ca8a04";
    }
    return "#ca8a04";
}

QString relative_time(int64_t unix_ts) {
    if (unix_ts <= 0)
        return {};
    auto now = QDateTime::currentSecsSinceEpoch();
    auto d = now - unix_ts;
    if (d < 0)
        return "now";
    if (d < 60)
        return QString("%1s").arg(d);
    if (d < 3600)
        return QString("%1m").arg(d / 60);
    if (d < 86400)
        return QString("%1h").arg(d / 3600);
    return QString("%1d").arg(d / 86400);
}

QString threat_level_string(ThreatLevel t) {
    switch (t) {
        case ThreatLevel::CRITICAL:
            return "CRITICAL";
        case ThreatLevel::HIGH:
            return "HIGH";
        case ThreatLevel::MEDIUM:
            return "MEDIUM";
        case ThreatLevel::LOW:
            return "LOW";
        case ThreatLevel::INFO:
            return "INFO";
    }
    return "INFO";
}

QString threat_level_color(ThreatLevel t) {
    switch (t) {
        case ThreatLevel::CRITICAL:
            return "#dc2626";
        case ThreatLevel::HIGH:
            return "#f97316";
        case ThreatLevel::MEDIUM:
            return "#eab308";
        case ThreatLevel::LOW:
            return "#22c55e";
        case ThreatLevel::INFO:
            return "#525252";
    }
    return "#525252";
}

Priority priority_from_string(const QString& s) {
    if (s == "FLASH")
        return Priority::FLASH;
    if (s == "URGENT")
        return Priority::URGENT;
    if (s == "BREAKING")
        return Priority::BREAKING;
    return Priority::ROUTINE;
}

Sentiment sentiment_from_string(const QString& s) {
    if (s == "BULLISH")
        return Sentiment::BULLISH;
    if (s == "BEARISH")
        return Sentiment::BEARISH;
    return Sentiment::NEUTRAL;
}

Impact impact_from_string(const QString& s) {
    if (s == "HIGH")
        return Impact::HIGH;
    if (s == "MEDIUM")
        return Impact::MEDIUM;
    return Impact::LOW;
}

// ── DataHub producer wiring ─────────────────────────────────────────────────

// ── Feed catalog overlay (built-ins + user edits) ───────────────────────────
//
// `default_feeds()` is the immutable in-code catalog. The `rss_feeds` table
// (v031 migration) stores user overrides:
//   - is_builtin=1 + id matches a default → overlay (changed url/name/...) or
//     disabled state.
//   - is_builtin=0 → fully user-added feed.

namespace {

fincept::RssFeedRow to_row(const fincept::services::RSSFeed& f, bool is_builtin, bool enabled) {
    fincept::RssFeedRow r;
    r.id = f.id;
    r.name = f.name;
    r.url = f.url;
    r.category = f.category;
    r.region = f.region;
    r.source = f.source;
    r.tier = f.tier;
    r.is_builtin = is_builtin;
    r.enabled = enabled;
    return r;
}

fincept::services::RSSFeed apply_overlay(const fincept::services::RSSFeed& base,
                                          const fincept::RssFeedRow& patch) {
    fincept::services::RSSFeed merged = base;
    merged.name = patch.name;
    merged.url = patch.url;
    merged.category = patch.category;
    merged.region = patch.region;
    merged.source = patch.source;
    merged.tier = patch.tier;
    return merged;
}

} // anonymous namespace

QVector<RSSFeed> NewsService::list_effective_feeds() const {
    const auto defaults = default_feeds();
    auto repo_res = fincept::RssFeedRepository::instance().list_all();
    if (repo_res.is_err()) {
        LOG_WARN("NewsService", QString("rss_feeds query failed; using built-ins only: %1")
                                    .arg(QString::fromStdString(repo_res.error())));
        return defaults;
    }

    QHash<QString, fincept::RssFeedRow> by_id;
    by_id.reserve(repo_res.value().size());
    for (const auto& r : repo_res.value())
        by_id.insert(r.id, r);

    QVector<RSSFeed> out;
    out.reserve(defaults.size() + by_id.size());

    for (const auto& d : defaults) {
        auto it = by_id.find(d.id);
        if (it == by_id.end()) {
            out.append(d);
            continue;
        }
        if (!it->enabled)
            continue;
        out.append(apply_overlay(d, *it));
    }

    QSet<QString> default_ids;
    for (const auto& d : defaults)
        default_ids.insert(d.id);
    for (const auto& r : repo_res.value()) {
        if (r.is_builtin || default_ids.contains(r.id) || !r.enabled)
            continue;
        RSSFeed f;
        f.id = r.id;
        f.name = r.name;
        f.url = r.url;
        f.category = r.category;
        f.region = r.region;
        f.source = r.source;
        f.tier = r.tier;
        out.append(f);
    }
    return out;
}

QVector<NewsService::EditorFeed> NewsService::list_all_feeds_for_editor() const {
    const auto defaults = default_feeds();
    auto repo_res = fincept::RssFeedRepository::instance().list_all();

    QHash<QString, fincept::RssFeedRow> by_id;
    if (!repo_res.is_err()) {
        for (const auto& r : repo_res.value())
            by_id.insert(r.id, r);
    }

    QVector<EditorFeed> out;
    out.reserve(defaults.size() + by_id.size());

    for (const auto& d : defaults) {
        EditorFeed ef;
        ef.is_builtin = true;
        auto it = by_id.find(d.id);
        if (it == by_id.end()) {
            ef.feed = d;
            ef.is_customized = false;
            ef.enabled = true;
        } else {
            ef.feed = apply_overlay(d, *it);
            ef.is_customized = true;
            ef.enabled = it->enabled;
        }
        out.append(ef);
    }

    QSet<QString> default_ids;
    for (const auto& d : defaults)
        default_ids.insert(d.id);
    if (!repo_res.is_err()) {
        for (const auto& r : repo_res.value()) {
            if (r.is_builtin || default_ids.contains(r.id))
                continue;
            EditorFeed ef;
            ef.feed.id = r.id;
            ef.feed.name = r.name;
            ef.feed.url = r.url;
            ef.feed.category = r.category;
            ef.feed.region = r.region;
            ef.feed.source = r.source;
            ef.feed.tier = r.tier;
            ef.is_builtin = false;
            ef.is_customized = false;
            ef.enabled = r.enabled;
            out.append(ef);
        }
    }
    return out;
}

bool NewsService::add_user_feed(const RSSFeed& f) {
    auto row = to_row(f, /*is_builtin=*/false, /*enabled=*/true);
    auto r = fincept::RssFeedRepository::instance().upsert(row);
    if (r.is_err()) {
        LOG_ERROR("NewsService", QString("add_user_feed failed: %1").arg(QString::fromStdString(r.error())));
        return false;
    }
    reload_feeds();
    return true;
}

bool NewsService::update_feed(const RSSFeed& f, bool enabled) {
    const auto defaults = default_feeds();
    bool is_builtin = false;
    for (const auto& d : defaults) {
        if (d.id == f.id) {
            is_builtin = true;
            break;
        }
    }
    auto row = to_row(f, is_builtin, enabled);
    auto r = fincept::RssFeedRepository::instance().upsert(row);
    if (r.is_err()) {
        LOG_ERROR("NewsService", QString("update_feed failed: %1").arg(QString::fromStdString(r.error())));
        return false;
    }
    reload_feeds();
    return true;
}

bool NewsService::remove_or_reset_feed(const QString& id) {
    auto r = fincept::RssFeedRepository::instance().remove(id);
    if (r.is_err()) {
        LOG_ERROR("NewsService", QString("remove_or_reset_feed failed: %1").arg(QString::fromStdString(r.error())));
        return false;
    }
    reload_feeds();
    return true;
}

bool NewsService::set_feed_enabled(const QString& id, bool enabled) {
    const auto defaults = default_feeds();
    const RSSFeed* default_ptr = nullptr;
    for (const auto& d : defaults) {
        if (d.id == id) {
            default_ptr = &d;
            break;
        }
    }

    auto& repo = fincept::RssFeedRepository::instance();
    auto rows = repo.list_all();
    bool exists = false;
    if (!rows.is_err()) {
        for (const auto& r : rows.value()) {
            if (r.id == id) {
                exists = true;
                break;
            }
        }
    }

    if (exists) {
        auto r = repo.set_enabled(id, enabled);
        if (r.is_err()) {
            LOG_ERROR("NewsService",
                      QString("set_feed_enabled failed: %1").arg(QString::fromStdString(r.error())));
            return false;
        }
    } else if (default_ptr) {
        auto row = to_row(*default_ptr, /*is_builtin=*/true, enabled);
        auto r = repo.upsert(row);
        if (r.is_err()) {
            LOG_ERROR("NewsService",
                      QString("set_feed_enabled (insert overlay) failed: %1").arg(QString::fromStdString(r.error())));
            return false;
        }
    } else {
        LOG_WARN("NewsService", QString("set_feed_enabled: unknown feed id %1").arg(id));
        return false;
    }
    reload_feeds();
    return true;
}

void NewsService::reload_feeds() {
    fincept::CacheManager::instance().clear_category("news");
    emit feeds_changed();
}

} // namespace fincept::services
