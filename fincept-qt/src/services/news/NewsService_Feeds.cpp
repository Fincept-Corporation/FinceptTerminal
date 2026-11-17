// src/services/news/NewsService_Feeds.cpp
//
// Static catalog of default RSS feeds. ~285 lines of data; isolated from
// the rest of the service so churn here doesn't recompile the network path.
//
// Part of the partial-class split of NewsService.cpp.

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "network/http/HttpClient.h"
#include "services/news/NewsService.h"
#include "storage/cache/CacheManager.h"
#include "storage/repositories/RssFeedRepository.h"
#include "ui/theme/Theme.h"

#include <QAtomicInt>
#include <QDateTime>
#include <QHash>
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

namespace {

/// One built-in feed. `lang` is the outlet's language (the prior for language
/// detection); `on` = enabled until the user changes it in the source manager.
RSSFeed news_builtin_feed(const char* id, const char* name, const char* url, const char* category,
                          const char* region, const char* source, int tier, const char* lang, bool on) {
    RSSFeed f;
    f.id = QString::fromLatin1(id);
    f.name = QString::fromUtf8(name);
    f.url = QString::fromLatin1(url);
    f.category = QString::fromLatin1(category);
    f.region = QString::fromLatin1(region);
    f.source = QString::fromUtf8(source);
    f.tier = tier;
    f.lang = QString::fromLatin1(lang);
    f.default_enabled = on;
    return f;
}

} // namespace

QVector<RSSFeed> NewsService::default_feeds() {
    // Region codes: US, UK, EU, GLOBAL, ASIA, CHINA, JAPAN, KOREA, INDIA, SEA,
    // ANZ, LATAM, MENA, AFRICA, CIS. Every URL below was fetched on 2026-11-16
    // with the browser User-Agent the fetcher sends and returned a parseable
    // RSS/RDF/Atom feed with current items; outlets that answer with a bot
    // challenge, a login page, a 404 or a feed frozen months ago are not listed.
    // Removed then: UN News (404), The Economist and Finextra (403 bot wall),
    // MoneyControl and the old feeds.a.dj.com WSJ feeds (frozen since 2024/25),
    // Calculated Risk's FeedBurner feed (frozen since January), Nikkei Asia (its
    // RDF items carry no date at all, so they cannot be placed in time) and the
    // Inquirer (sends Manila wall-clock time labelled +0000, six hours ahead).
    // Checked from a Linux (OpenSSL) and a Windows (Schannel) client: GlobeNewswire
    // and Les Echos answer the Windows TLS fingerprint with a 403 — not listed.
    // A built-in whose URL changed keeps its id; the old URL is listed in
    // retired_builtin_urls() so source-manager rows that copied it follow.
    const auto F = [](const char* id, const char* name, const char* url, const char* category, const char* region,
                      const char* source, int tier, const char* lang, bool on = true) {
        return news_builtin_feed(id, name, url, category, region, source, tier, lang, on);
    };
    QVector<RSSFeed> feeds = {
        // ── Tier 1 — Central banks, regulators, official statistics ───────
        // Reuters discontinued public RSS in 2020; AP answers bots with a 403.
        F("sec-press", "SEC Press Releases", "https://www.sec.gov/news/pressreleases.rss", "REGULATORY", "US", "SEC", 1,
          "en"),
        F("fed-press", "Federal Reserve", "https://www.federalreserve.gov/feeds/press_all.xml", "REGULATORY", "US",
          "FEDERAL RESERVE", 1, "en"),
        F("fed-speeches", "Federal Reserve Speeches", "https://www.federalreserve.gov/feeds/speeches.xml",
          "REGULATORY", "US", "FEDERAL RESERVE", 1, "en"),
        F("cftc-press", "CFTC Press Releases", "https://www.cftc.gov/RSS/RSSGP/rssgp.xml", "REGULATORY", "US", "CFTC",
          1, "en"),
        F("eia-today", "EIA Today in Energy", "https://www.eia.gov/rss/todayinenergy.xml", "ENERGY", "US", "EIA", 1,
          "en"),
        F("ecb-press", "ECB Press", "https://www.ecb.europa.eu/rss/press.html", "REGULATORY", "EU", "ECB", 1, "en"),
        F("boe-news", "Bank of England", "https://www.bankofengland.co.uk/rss/news", "REGULATORY", "UK", "BOE", 1, "en"),
        F("boe-speeches", "Bank of England Speeches", "https://www.bankofengland.co.uk/rss/speeches", "REGULATORY",
          "UK", "BOE", 1, "en", false),
        F("boj-news", "Bank of Japan", "https://www.boj.or.jp/en/rss/whatsnew.xml", "REGULATORY", "JAPAN", "BOJ", 1,
          "en"),
        F("rbi-press", "Reserve Bank of India", "https://www.rbi.org.in/pressreleases_rss.xml", "REGULATORY", "INDIA",
          "RBI", 1, "en"),
        F("snb-news", "Swiss National Bank", "https://www.snb.ch/public/rss/en/news", "REGULATORY", "EU", "SNB", 1,
          "en"),
        F("bis-speeches", "BIS Central Bank Speeches", "https://www.bis.org/doclist/cbspeeches.rss", "REGULATORY",
          "GLOBAL", "BIS", 1, "en"),
        // (IMF News removed — endpoint serves an Akamai access-denied HTML page.)

        // ── Tier 2 — Major financial media ─────────────────────────────────
        F("bloomberg-mkts", "Bloomberg Markets", "https://feeds.bloomberg.com/markets/news.rss", "MARKETS", "GLOBAL",
          "BLOOMBERG", 2, "en"),
        F("wsj-markets", "WSJ Markets", "https://feeds.content.dowjones.io/public/rss/RSSMarketsMain", "MARKETS", "US",
          "WSJ", 2, "en"),
        F("wsj-business", "WSJ Business", "https://feeds.content.dowjones.io/public/rss/WSJcomUSBusiness", "MARKETS",
          "US", "WSJ", 2, "en"),
        F("wsj-economy", "WSJ Economy", "https://feeds.content.dowjones.io/public/rss/socialeconomyfeed", "ECONOMIC",
          "US", "WSJ", 2, "en"),
        F("wsj-world", "WSJ World", "https://feeds.content.dowjones.io/public/rss/RSSWorldNews", "GEOPOLITICS",
          "GLOBAL", "WSJ", 2, "en"),
        F("marketwatch", "MarketWatch", "https://feeds.content.dowjones.io/public/rss/mw_topstories", "MARKETS", "US",
          "MARKETWATCH", 2, "en"),
        F("ft-markets", "FT Markets", "https://www.ft.com/markets?format=rss", "MARKETS", "GLOBAL", "FT", 2, "en"),
        F("ft-companies", "FT Companies", "https://www.ft.com/companies?format=rss", "MARKETS", "GLOBAL", "FT", 2,
          "en"),
        F("ft-commodities", "FT Commodities", "https://www.ft.com/commodities?format=rss", "ENERGY", "GLOBAL", "FT", 2,
          "en"),
        F("ft-world", "FT World", "https://www.ft.com/world?format=rss", "GEOPOLITICS", "GLOBAL", "FT", 2, "en"),
        F("cnbc-finance", "CNBC Top News",
          "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=100003114", "MARKETS", "US",
          "CNBC", 2, "en"),
        F("cnbc-economy", "CNBC Economy",
          "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=20910258", "ECONOMIC", "US",
          "CNBC", 2, "en"),
        F("cnbc-earnings", "CNBC Earnings",
          "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=15839135", "EARNINGS", "US",
          "CNBC", 2, "en"),
        F("cnbc-world", "CNBC World",
          "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=100727362", "MARKETS", "GLOBAL",
          "CNBC", 2, "en"),
        F("cnbc-tech", "CNBC Technology",
          "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=19854910", "TECH", "US", "CNBC", 2,
          "en"),
        F("seekingalpha", "Seeking Alpha", "https://seekingalpha.com/market_currents.xml", "MARKETS", "US",
          "SEEKING ALPHA", 2, "en"),
        F("sa-earnings", "Seeking Alpha Earnings", "https://seekingalpha.com/tag/earnings.xml", "EARNINGS", "US",
          "SEEKING ALPHA", 2, "en"),
        F("yahoo-sp500", "Yahoo Finance (S&P 500)",
          "https://feeds.finance.yahoo.com/rss/2.0/headline?s=%5EGSPC&region=US&lang=en-US", "MARKETS", "US",
          "YAHOO FINANCE", 2, "en"),
        F("nasdaq-markets", "Nasdaq Markets", "https://www.nasdaq.com/feed/rssoutbound?category=Markets", "MARKETS",
          "US", "NASDAQ", 2, "en"),
        F("nyt-business", "NYT Business", "https://rss.nytimes.com/services/xml/rss/nyt/Business.xml", "MARKETS", "US",
          "NYT", 2, "en"),
        F("fortune", "Fortune", "https://fortune.com/feed/fortune-feeds/?id=3230629", "MARKETS", "US", "FORTUNE", 2,
          "en"),
        F("investing-news", "Investing.com", "https://www.investing.com/rss/news.rss", "MARKETS", "GLOBAL",
          "INVESTING.COM", 2, "en"),
        F("investing-econ", "Investing.com Economy", "https://www.investing.com/rss/news_14.rss", "ECONOMIC", "GLOBAL",
          "INVESTING.COM", 2, "en"),
        F("investing-earnings", "Investing.com Earnings", "https://www.investing.com/rss/news_1062.rss", "EARNINGS",
          "GLOBAL", "INVESTING.COM", 2, "en"),
        F("investing-commod", "Investing.com Commodities", "https://www.investing.com/rss/news_11.rss", "ENERGY",
          "GLOBAL", "INVESTING.COM", 2, "en"),
        F("benzinga", "Benzinga", "https://www.benzinga.com/feed", "MARKETS", "US", "BENZINGA", 2, "en"),
        F("fxstreet", "FXStreet", "https://www.fxstreet.com/rss/news", "MARKETS", "GLOBAL", "FXSTREET", 2, "en"),
        F("mining-com", "Mining.com", "https://www.mining.com/feed/", "MARKETS", "GLOBAL", "MINING.COM", 2, "en"),
        F("oilprice", "OilPrice.com", "https://oilprice.com/rss/main", "ENERGY", "GLOBAL", "OILPRICE", 2, "en"),
        // (Kitco RSS endpoint removed — kitco.com no longer publishes RSS.)

        // ── Tier 2 — Global news ───────────────────────────────────────────
        F("bbc-world", "BBC World", "https://feeds.bbci.co.uk/news/world/rss.xml", "GEOPOLITICS", "GLOBAL", "BBC", 2,
          "en"),
        F("bbc-business", "BBC Business", "https://feeds.bbci.co.uk/news/business/rss.xml", "MARKETS", "GLOBAL", "BBC",
          2, "en"),
        F("aljazeera", "Al Jazeera", "https://www.aljazeera.com/xml/rss/all.xml", "GEOPOLITICS", "GLOBAL",
          "AL JAZEERA", 2, "en"),
        F("nyt-world", "NYT World", "https://rss.nytimes.com/services/xml/rss/nyt/World.xml", "GEOPOLITICS", "GLOBAL",
          "NYT", 2, "en"),
        F("guardian-world", "Guardian World", "https://www.theguardian.com/world/rss", "GEOPOLITICS", "GLOBAL",
          "GUARDIAN", 2, "en"),
        F("foreignpolicy", "Foreign Policy", "https://foreignpolicy.com/feed/", "GEOPOLITICS", "GLOBAL",
          "FOREIGN POLICY", 2, "en"),
        F("politico-eu", "Politico Europe", "https://www.politico.eu/feed/", "GEOPOLITICS", "EU", "POLITICO", 2, "en"),
        F("kyiv-independent", "Kyiv Independent", "https://kyivindependent.com/news-archive/rss/", "GEOPOLITICS", "CIS",
          "KYIV INDEPENDENT", 2, "en"),
        F("timesofisrael", "Times of Israel", "https://www.timesofisrael.com/feed/", "GEOPOLITICS", "MENA",
          "TIMES OF ISRAEL", 2, "en"),
        // (defensenews.com/rss/ returns 404 — endpoint discontinued.)

        // ── Tier 2 — Tech / crypto / fintech ───────────────────────────────
        F("techcrunch", "TechCrunch", "https://techcrunch.com/feed/", "TECH", "GLOBAL", "TECHCRUNCH", 2, "en"),
        F("wired", "Wired", "https://www.wired.com/feed/rss", "TECH", "US", "WIRED", 2, "en"),
        F("digitimes", "DIGITIMES Asia", "https://www.digitimes.com/rss/daily.xml", "TECH", "ASIA", "DIGITIMES", 2,
          "en"),
        F("coindesk", "CoinDesk", "https://www.coindesk.com/arc/outboundfeeds/rss", "CRYPTO", "GLOBAL", "COINDESK", 2,
          "en"),
        F("cointelegraph", "CoinTelegraph", "https://cointelegraph.com/rss", "CRYPTO", "GLOBAL", "COINTELEGRAPH", 2,
          "en"),
        F("theblock", "The Block", "https://www.theblock.co/rss.xml", "CRYPTO", "GLOBAL", "THE BLOCK", 2, "en"),
        F("decrypt", "Decrypt", "https://decrypt.co/feed", "CRYPTO", "GLOBAL", "DECRYPT", 2, "en"),
        F("cryptoslate", "CryptoSlate", "https://cryptoslate.com/feed/", "CRYPTO", "GLOBAL", "CRYPTOSLATE", 3, "en"),

        // ── Europe ─────────────────────────────────────────────────────────
        F("guardian-business", "Guardian Business", "https://www.theguardian.com/uk/business/rss", "MARKETS", "UK",
          "GUARDIAN", 2, "en"),
        F("sky-business", "Sky News Business", "https://feeds.skynews.com/feeds/rss/business.xml", "MARKETS", "UK",
          "SKY NEWS", 2, "en"),
        F("cityam", "City A.M.", "https://www.cityam.com/feed/", "MARKETS", "UK", "CITY AM", 3, "en", false),
        F("rte-business", "RT\xc3\x89 Business", "https://www.rte.ie/feeds/rss/?index=/news/business/", "ECONOMIC",
          "EU", "RT\xc3\x89", 2, "en", false),
        F("france24", "France 24", "https://www.france24.com/en/rss", "GEOPOLITICS", "EU", "FRANCE 24", 2, "en"),
        F("dw-world", "Deutsche Welle", "https://rss.dw.com/rdf/rss-en-all", "GEOPOLITICS", "EU", "DW", 2, "en"),
        F("handelsblatt-top", "Handelsblatt", "https://feeds.cms.handelsblatt.com/schlagzeilen", "MARKETS", "EU",
          "HANDELSBLATT", 2, "de"),
        F("handelsblatt-fin", "Handelsblatt Finanzen", "https://feeds.cms.handelsblatt.com/finanzen", "MARKETS", "EU",
          "HANDELSBLATT", 2, "de", false),
        F("tagesschau-wirtschaft", "tagesschau Wirtschaft", "https://www.tagesschau.de/wirtschaft/index~rss2.xml",
          "ECONOMIC", "EU", "TAGESSCHAU", 2, "de", false),
        F("spiegel-wirtschaft", "Spiegel Wirtschaft", "https://www.spiegel.de/wirtschaft/index.rss", "ECONOMIC", "EU",
          "SPIEGEL", 2, "de", false),
        F("nzz-wirtschaft", "NZZ Wirtschaft", "https://www.nzz.ch/wirtschaft.rss", "ECONOMIC", "EU", "NZZ", 2, "de",
          false),
        F("derstandard-wirtschaft", "Der Standard Wirtschaft", "https://www.derstandard.at/rss/wirtschaft", "ECONOMIC",
          "EU", "DER STANDARD", 2, "de", false),
        F("lemonde-eco", "Le Monde \xc3\x89" "conomie", "https://www.lemonde.fr/economie/rss_full.xml", "ECONOMIC", "EU",
          "LE MONDE", 2, "fr"),
        F("figaro-eco", "Le Figaro \xc3\x89" "conomie", "https://www.lefigaro.fr/rss/figaro_economie.xml", "ECONOMIC",
          "EU", "LE FIGARO", 2, "fr", false),
        F("bfm-eco", "BFM \xc3\x89" "conomie", "https://www.bfmtv.com/rss/economie/", "MARKETS", "EU", "BFM", 2, "fr",
          false),
        F("expansion-mercados", "Expansi\xc3\xb3n Mercados", "https://e00-expansion.uecdn.es/rss/mercados.xml",
          "MARKETS", "EU", "EXPANSI\xc3\x93N", 2, "es"),
        F("cincodias", "Cinco D\xc3\xad" "as", "https://feeds.elpais.com/mrss-s/pages/ep/site/cincodias.elpais.com/portada",
          "MARKETS", "EU", "CINCO D\xc3\x8d" "AS", 2, "es", false),
        F("sole24-finanza", "Il Sole 24 Ore Finanza", "https://www.ilsole24ore.com/rss/finanza.xml", "MARKETS", "EU",
          "IL SOLE 24 ORE", 2, "it"),
        F("ansa-economia", "ANSA Economia", "https://www.ansa.it/sito/notizie/economia/economia_rss.xml", "ECONOMIC",
          "EU", "ANSA", 2, "it", false),
        F("nos-economie", "NOS Economie", "https://feeds.nos.nl/nosnieuwseconomie", "ECONOMIC", "EU", "NOS", 2, "nl"),
        F("fd-nl", "Het Financieele Dagblad", "https://fd.nl/?rss", "MARKETS", "EU", "FD", 2, "nl", false),
        F("negocios-pt", "Jornal de Neg\xc3\xb3" "cios", "https://www.jornaldenegocios.pt/rss", "MARKETS", "EU",
          "NEG\xc3\x93" "CIOS", 2, "pt", false),
        F("hbl-fi", "Hufvudstadsbladet", "https://www.hbl.fi/rss/", "GEOPOLITICS", "EU", "HBL", 3, "sv", false),

        // ── Asia-Pacific ───────────────────────────────────────────────────
        F("scmp", "South China Morning Post", "https://www.scmp.com/rss/91/feed/", "GEOPOLITICS", "CHINA", "SCMP", 2,
          "en"),
        F("scmp-business", "SCMP Business", "https://www.scmp.com/rss/92/feed/", "MARKETS", "CHINA", "SCMP", 2, "en"),
        F("ftchinese", "FT\xe4\xb8\xad\xe6\x96\x87\xe7\xbd\x91", "https://www.ftchinese.com/rss/news", "MARKETS", "CHINA",
          "FT\xe4\xb8\xad\xe6\x96\x87\xe7\xbd\x91", 2, "zh"),
        F("bbc-zhongwen", "BBC \xe4\xb8\xad\xe6\x96\x87", "https://feeds.bbci.co.uk/zhongwen/simp/rss.xml", "GEOPOLITICS",
          "CHINA", "BBC \xe4\xb8\xad\xe6\x96\x87", 2, "zh", false),
        F("dw-zh", "DW \xe4\xb8\xad\xe6\x96\x87", "https://rss.dw.com/xml/rss-chi-all", "GEOPOLITICS", "CHINA",
          "DW \xe4\xb8\xad\xe6\x96\x87", 2, "zh", false),
        F("taipeitimes", "Taipei Times", "https://www.taipeitimes.com/xml/index.rss", "GEOPOLITICS", "ASIA",
          "TAIPEI TIMES", 2, "en"),
        F("asiatimes", "Asia Times", "https://asiatimes.com/feed/", "GEOPOLITICS", "ASIA", "ASIA TIMES", 3, "en"),
        F("japantimes", "The Japan Times", "https://www.japantimes.co.jp/feed/", "GEOPOLITICS", "JAPAN",
          "JAPAN TIMES", 2, "en"),
        F("toyokeizai", "\xe6\x9d\xb1\xe6\xb4\x8b\xe7\xb5\x8c\xe6\xb8\x88", "https://toyokeizai.net/list/feed/rss",
          "ECONOMIC", "JAPAN", "\xe6\x9d\xb1\xe6\xb4\x8b\xe7\xb5\x8c\xe6\xb8\x88", 2, "ja"),
        F("itmedia-business", "ITmedia \xe3\x83\x93\xe3\x82\xb8\xe3\x83\x8d\xe3\x82\xb9",
          "https://rss.itmedia.co.jp/rss/2.0/business.xml", "TECH", "JAPAN", "ITMEDIA", 3, "ja", false),
        F("asahi-business", "\xe6\x9c\x9d\xe6\x97\xa5\xe6\x96\xb0\xe8\x81\x9e \xe7\xb5\x8c\xe6\xb8\x88",
          "https://www.asahi.com/rss/asahi/business.rdf", "ECONOMIC", "JAPAN", "ASAHI", 2, "ja", false),
        F("yonhap", "Yonhap News", "https://en.yna.co.kr/RSS/news.xml", "ECONOMIC", "KOREA", "YONHAP", 2, "en"),
        F("koreaherald-biz", "Korea Herald Business", "https://www.koreaherald.com/rss/kh_Business", "MARKETS", "KOREA",
          "KOREA HERALD", 2, "en"),
        F("koreatimes-biz", "Korea Times Business", "https://feed.koreatimes.co.kr/k/business.xml", "MARKETS", "KOREA",
          "KOREA TIMES", 2, "en"),
        F("hindu-biz", "The Hindu Business", "https://www.thehindu.com/business/feeder/default.rss", "MARKETS", "INDIA",
          "THE HINDU", 2, "en"),
        F("livemint", "LiveMint", "https://www.livemint.com/rss/markets", "MARKETS", "INDIA", "LIVEMINT", 2, "en"),
        F("livemint-companies", "LiveMint Companies", "https://www.livemint.com/rss/companies", "MARKETS", "INDIA",
          "LIVEMINT", 2, "en"),
        F("et-markets", "Economic Times", "https://economictimes.indiatimes.com/rssfeedstopstories.cms", "MARKETS",
          "INDIA", "ECONOMIC TIMES", 2, "en"),
        F("toi-business", "Times of India Business", "https://timesofindia.indiatimes.com/rssfeeds/1898055.cms",
          "MARKETS", "INDIA", "TIMES OF INDIA", 2, "en"),
        F("cnbctv18-markets", "CNBC-TV18 Markets", "https://www.cnbctv18.com/commonfeeds/v1/cne/rss/market.xml",
          "MARKETS", "INDIA", "CNBC-TV18", 2, "en"),
        F("businesstoday", "Business Today", "https://www.businesstoday.in/rssfeeds?id=225346", "MARKETS", "INDIA",
          "BUSINESS TODAY", 2, "en"),
        F("ht-business", "Hindustan Times Business", "https://www.hindustantimes.com/feeds/rss/business/rssfeed.xml",
          "MARKETS", "INDIA", "HINDUSTAN TIMES", 2, "en"),
        F("bs-markets", "Business Standard Markets", "https://www.business-standard.com/rss/markets-106.rss", "MARKETS",
          "INDIA", "BUSINESS STANDARD", 2, "en"),
        F("bs-economy", "Business Standard Economy", "https://www.business-standard.com/rss/economy-102.rss",
          "ECONOMIC", "INDIA", "BUSINESS STANDARD", 2, "en"),
        F("businessline-markets", "BusinessLine Markets", "https://www.thehindubusinessline.com/markets/feeder/default.rss",
          "MARKETS", "INDIA", "BUSINESSLINE", 2, "en"),
        F("amarujala-business", "\xe0\xa4\x85\xe0\xa4\xae\xe0\xa4\xb0 \xe0\xa4\x89\xe0\xa4\x9c\xe0\xa4\xbe\xe0\xa4\xb2\xe0\xa4\xbe",
          "https://www.amarujala.com/rss/business.xml", "ECONOMIC", "INDIA", "AMAR UJALA", 3, "hi", false),
        F("channel-news-asia", "CNA Business",
          "https://www.channelnewsasia.com/api/v1/rss-outbound-feed?_format=xml&category=6936", "MARKETS", "SEA", "CNA",
          2, "en"),
        F("cna-asia", "CNA Asia", "https://www.channelnewsasia.com/api/v1/rss-outbound-feed?_format=xml&category=6511",
          "GEOPOLITICS", "SEA", "CNA", 2, "en"),
        F("straitstimes-biz", "Straits Times Business", "https://www.straitstimes.com/news/business/rss.xml", "MARKETS",
          "SEA", "STRAITS TIMES", 2, "en"),
        F("edge-singapore", "The Edge Singapore", "https://www.theedgesingapore.com/rss.xml", "MARKETS", "SEA",
          "THE EDGE SINGAPORE", 2, "en"),
        F("bangkokpost-biz", "Bangkok Post Business", "https://www.bangkokpost.com/rss/data/business.xml", "MARKETS",
          "SEA", "BANGKOK POST", 2, "en"),
        F("vnexpress-biz", "VnExpress Business", "https://e.vnexpress.net/rss/business.rss", "MARKETS", "SEA",
          "VNEXPRESS", 2, "en"),
        F("bworld", "BusinessWorld", "https://bworldonline.com/feed/", "MARKETS", "SEA", "BUSINESSWORLD", 2, "en"),
        F("abc-au-business", "ABC Business", "https://www.abc.net.au/news/feed/51892/rss.xml", "MARKETS", "ANZ",
          "ABC AUSTRALIA", 2, "en"),
        F("smh-business", "SMH Business", "https://www.smh.com.au/rss/business.xml", "MARKETS", "ANZ", "SMH", 2, "en",
          false),
        F("rnz-business", "RNZ Business", "https://www.rnz.co.nz/rss/business.xml", "MARKETS", "ANZ", "RNZ", 2, "en"),

        // ── Latin America ──────────────────────────────────────────────────
        F("bloomberglinea", "Bloomberg L\xc3\xadnea", "https://www.bloomberglinea.com/arc/outboundfeeds/rss/?outputType=xml",
          "MARKETS", "LATAM", "BLOOMBERG L\xc3\x8dNEA", 2, "es"),
        F("elfinanciero-mx", "El Financiero", "https://www.elfinanciero.com.mx/arc/outboundfeeds/rss/?outputType=xml",
          "MARKETS", "LATAM", "EL FINANCIERO", 2, "es"),
        F("folha-mercado", "Folha Mercado", "https://feeds.folha.uol.com.br/mercado/rss091.xml", "MARKETS", "LATAM",
          "FOLHA", 2, "pt"),
        F("infomoney", "InfoMoney", "https://www.infomoney.com.br/feed/", "MARKETS", "LATAM", "INFOMONEY", 2, "pt",
          false),
        F("estadao-economia", "Estad\xc3\xa3o Economia",
          "https://www.estadao.com.br/arc/outboundfeeds/feeds/rss/sections/economia/", "ECONOMIC", "LATAM", "ESTAD\xc3\x83O",
          2, "pt", false),
        F("lanacion-economia", "La Naci\xc3\xb3n Econom\xc3\xad" "a",
          "https://www.lanacion.com.ar/arc/outboundfeeds/rss/category/economia/?outputType=xml", "ECONOMIC", "LATAM",
          "LA NACI\xc3\x93N", 2, "es"),
        F("df-cl", "Diario Financiero", "https://www.df.cl/noticias/site/list/port/rss.xml", "MARKETS", "LATAM",
          "DIARIO FINANCIERO", 2, "es"),
        F("latercera-pulso", "La Tercera Pulso",
          "https://www.latercera.com/arc/outboundfeeds/rss/category/pulso/?outputType=xml", "MARKETS", "LATAM",
          "LA TERCERA", 2, "es", false),
        F("gestion-pe", "Gesti\xc3\xb3n", "https://gestion.pe/arc/outboundfeeds/rss/category/economia/?outputType=xml",
          "ECONOMIC", "LATAM", "GESTI\xc3\x93N", 2, "es", false),
        F("portafolio-co", "Portafolio", "https://www.portafolio.co/rss/economia.xml", "ECONOMIC", "LATAM",
          "PORTAFOLIO", 2, "es"),
        F("mercopress", "MercoPress", "https://en.mercopress.com/rss", "ECONOMIC", "LATAM", "MERCOPRESS", 3, "en"),
        F("bbc-mundo", "BBC Mundo", "https://feeds.bbci.co.uk/mundo/rss.xml", "GEOPOLITICS", "LATAM", "BBC MUNDO", 2,
          "es", false),
        F("bbc-brasil", "BBC Brasil", "https://feeds.bbci.co.uk/portuguese/rss.xml", "GEOPOLITICS", "LATAM",
          "BBC BRASIL", 2, "pt", false),

        // ── Middle East ────────────────────────────────────────────────────
        F("middle-east-eye", "Middle East Eye", "https://www.middleeasteye.net/rss", "GEOPOLITICS", "MENA",
          "MIDDLE EAST EYE", 2, "en"),
        F("thenational-biz", "The National Business",
          "https://www.thenationalnews.com/arc/outboundfeeds/rss/category/business/?outputType=xml", "MARKETS", "MENA",
          "THE NATIONAL", 2, "en"),
        F("globes-en", "Globes", "https://www.globes.co.il/webservice/rss/rssfeeder.asmx/FeederNode?iID=1725", "MARKETS",
          "MENA", "GLOBES", 2, "en"),
        F("dailysabah-biz", "Daily Sabah Business", "https://www.dailysabah.com/rss/business", "MARKETS", "MENA",
          "DAILY SABAH", 2, "en"),
        F("bbc-arabic", "BBC Arabic", "https://feeds.bbci.co.uk/arabic/rss.xml", "GEOPOLITICS", "MENA", "BBC ARABIC", 2,
          "ar", false),

        // ── Africa ─────────────────────────────────────────────────────────
        F("allafrica-business", "allAfrica Business", "https://allafrica.com/tools/headlines/rdf/business/headlines.rdf",
          "ECONOMIC", "AFRICA", "ALLAFRICA", 2, "en"),
        F("moneyweb", "Moneyweb", "https://www.moneyweb.co.za/feed/", "MARKETS", "AFRICA", "MONEYWEB", 2, "en"),
        F("nairametrics", "Nairametrics", "https://nairametrics.com/feed/", "MARKETS", "AFRICA", "NAIRAMETRICS", 2, "en"),
        F("businessday-ng", "BusinessDay Nigeria", "https://businessday.ng/feed/", "MARKETS", "AFRICA", "BUSINESSDAY", 2,
          "en"),
        F("businessdaily-ke", "Business Daily Africa", "https://www.businessdailyafrica.com/service/rss/bd/1939132/feed.rss",
          "MARKETS", "AFRICA", "BUSINESS DAILY", 2, "en"),
        F("techcabal", "TechCabal", "https://techcabal.com/feed/", "TECH", "AFRICA", "TECHCABAL", 3, "en", false),
        F("jeuneafrique", "Jeune Afrique", "https://www.jeuneafrique.com/feed/", "GEOPOLITICS", "AFRICA",
          "JEUNE AFRIQUE", 2, "fr", false),

        // ── Russia / CIS ───────────────────────────────────────────────────
        F("rbc-ru", "RBC", "https://rssexport.rbc.ru/rbcnews/news/30/full.rss", "MARKETS", "CIS", "RBC", 2, "ru", false),
        F("kommersant-eco", "Kommersant Economics", "https://www.kommersant.ru/rss/section-economics.xml", "ECONOMIC",
          "CIS", "KOMMERSANT", 2, "ru", false),

        // ── Tier 3 — Macro / analysis / commodities ────────────────────────
        F("zero-hedge", "ZeroHedge", "https://feeds.feedburner.com/zerohedge/feed", "ECONOMIC", "GLOBAL", "ZEROHEDGE", 3,
          "en"),
        F("wolfstreet", "Wolf Street", "https://wolfstreet.com/feed/", "ECONOMIC", "US", "WOLF STREET", 3, "en"),
        F("hellenic-shipping", "Hellenic Shipping News", "https://www.hellenicshippingnews.com/feed/", "ENERGY",
          "GLOBAL", "HELLENIC SHIPPING NEWS", 3, "en"),
        // (defenseone.com/rss/ returns 404 — endpoint discontinued.)
        F("bellingcat", "Bellingcat", "https://www.bellingcat.com/feed/", "GEOPOLITICS", "GLOBAL", "BELLINGCAT", 3, "en"),
        F("arstechnica", "Ars Technica", "https://feeds.arstechnica.com/arstechnica/index", "TECH", "GLOBAL",
          "ARS TECHNICA", 3, "en"),
        F("theverge", "The Verge", "https://www.theverge.com/rss/index.xml", "TECH", "GLOBAL", "THE VERGE", 3, "en"),
        F("mit-tech", "MIT Tech Review", "https://www.technologyreview.com/feed/", "TECH", "GLOBAL", "MIT TECH REVIEW",
          3, "en"),
        F("techmeme", "Techmeme", "https://www.techmeme.com/feed.xml", "TECH", "GLOBAL", "TECHMEME", 3, "en"),
        F("carbon-brief", "Carbon Brief", "https://www.carbonbrief.org/feed", "ENERGY", "GLOBAL", "CARBON BRIEF", 3,
          "en"),

        // ── Tier 4 — Blogs & aggregators ───────────────────────────────────
        F("hackernews", "Hacker News", "https://hnrss.org/frontpage", "TECH", "GLOBAL", "HACKER NEWS", 4, "en"),
        F("abnormal-returns", "Abnormal Returns", "https://abnormalreturns.com/feed/", "MARKETS", "US",
          "ABNORMAL RETURNS", 4, "en"),
        F("marginal-rev", "Marginal Revolution", "https://marginalrevolution.com/feed", "ECONOMIC", "GLOBAL",
          "MARGINAL REVOLUTION", 4, "en"),
    };
    // The RBI writes Indian wall-clock times with no zone ("Mon, 16 Nov 2026
    // 12:05:00"); read as UTC they sat 5.5 h in the future.
    for (auto& f : feeds)
        if (f.id == QLatin1String("rbi-press"))
            f.naive_utc_offset_min = 330;
    return feeds;
}

const QHash<QString, QStringList>& NewsService::retired_builtin_urls() {
    static const QHash<QString, QStringList> urls = {
        {"bbc-world", {"http://feeds.bbci.co.uk/news/world/rss.xml"}},
        {"bbc-business", {"http://feeds.bbci.co.uk/news/business/rss.xml"}},
        {"wsj-markets", {"https://feeds.a.dj.com/rss/RSSMarketsMain.xml"}},
        {"wsj-world", {"https://feeds.a.dj.com/rss/RSSWorldNews.xml"}},
        {"marketwatch", {"https://feeds.marketwatch.com/marketwatch/topstories/"}},
        {"snb-news", {"https://www.snb.ch/public/en/rss/news"}},
        {"coindesk", {"https://www.coindesk.com/arc/outboundfeeds/rss/"}},
        {"guardian-business", {"https://www.theguardian.com/business/rss"}},
        {"handelsblatt-top", {"https://www.handelsblatt.com/contentexport/feed/schlagzeilen"}},
        {"handelsblatt-fin", {"https://www.handelsblatt.com/contentexport/feed/finanzen"}},
        {"scmp", {"https://www.scmp.com/rss/91/feed"}},
        {"scmp-business", {"https://www.scmp.com/rss/92/feed"}},
        {"channel-news-asia", {"https://www.channelnewsasia.com/rssfeeds/8395986"}},
        {"gestion-pe", {"https://gestion.pe/arcio/rss/category/economia/"}},
        {"portafolio-co", {"https://www.portafolio.co/rss/economia"}},
        {"kommersant-eco", {"https://www.kommersant.ru/RSS/section-economics.xml"}},
        {"carbon-brief", {"https://www.carbonbrief.org/feed/"}},
    };
    return urls;
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

// The *_color() helpers return the active theme's token values, read on every
// call — callers that keep the string must fetch it again after
// ThemeManager::theme_changed.
QString priority_color(Priority p) {
    switch (p) {
        case Priority::FLASH:
            return ui::colors::DANGER;
        case Priority::URGENT:
            return ui::colors::AMBER;
        case Priority::BREAKING:
            return ui::colors::WARNING;
        case Priority::ROUTINE:
            return ui::colors::TEXT_SECONDARY;
    }
    return ui::colors::TEXT_SECONDARY;
}

// Bullish / bearish is market direction, so it follows the direction palette.
QString sentiment_color(Sentiment s) {
    switch (s) {
        case Sentiment::BULLISH:
            return ui::colors::POSITIVE;
        case Sentiment::BEARISH:
            return ui::colors::NEGATIVE;
        case Sentiment::NEUTRAL:
            return ui::colors::WARNING;
    }
    return ui::colors::WARNING;
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
            return ui::colors::DANGER;
        case ThreatLevel::HIGH:
            return ui::colors::AMBER;
        case ThreatLevel::MEDIUM:
            return ui::colors::WARNING;
        case ThreatLevel::LOW:
            return ui::colors::SUCCESS;
        case ThreatLevel::INFO:
            return ui::colors::TEXT_SECONDARY;
    }
    return ui::colors::TEXT_SECONDARY;
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

fincept::services::RSSFeed apply_overlay(const fincept::services::RSSFeed& base, const fincept::RssFeedRow& patch) {
    fincept::services::RSSFeed merged = base;
    merged.name = patch.name;
    // A row written when the user toggled a built-in copies that day's URL.
    // If it still holds one the catalogue has since retired (feed moved,
    // http -> https), the current built-in URL applies; a URL the user typed
    // themselves is never one of those and is kept.
    const auto retired = fincept::services::NewsService::retired_builtin_urls().value(base.id);
    merged.url = retired.contains(patch.url.trimmed()) ? base.url : patch.url;
    merged.category = patch.category;
    merged.region = patch.region;
    merged.source = patch.source;
    merged.tier = patch.tier;
    return merged;
}

/// URL identity for "is this the same feed": scheme, "www." and a trailing
/// slash ignored, case-insensitive.
QString news_feed_url_key(const QString& url) {
    QString k = url.trimmed().toLower();
    const int scheme = static_cast<int>(k.indexOf(QLatin1String("://")));
    if (scheme >= 0)
        k = k.mid(scheme + 3);
    if (k.startsWith(QLatin1String("www.")))
        k = k.mid(4);
    while (k.endsWith(QLatin1Char('/')))
        k.chop(1);
    return k;
}

} // anonymous namespace

QVector<RSSFeed> NewsService::list_effective_feeds() const {
    const auto defaults = default_feeds();
    auto repo_res = fincept::RssFeedRepository::instance().list_all();
    if (repo_res.is_err()) {
        LOG_WARN(
            "NewsService",
            QString("rss_feeds query failed; using built-ins only: %1").arg(QString::fromStdString(repo_res.error())));
        return defaults;
    }

    QHash<QString, fincept::RssFeedRow> by_id;
    by_id.reserve(repo_res.value().size());
    for (const auto& r : repo_res.value())
        by_id.insert(r.id, r);

    QVector<RSSFeed> out;
    out.reserve(defaults.size() + by_id.size());

    QSet<QString> default_ids;
    for (const auto& d : defaults)
        default_ids.insert(d.id);
    // URLs the user already follows as their own feeds: a built-in added to the
    // catalogue later with the same URL is not fetched a second time.
    QSet<QString> user_urls;
    for (const auto& r : repo_res.value())
        if (!r.is_builtin && !default_ids.contains(r.id) && r.enabled)
            user_urls.insert(news_feed_url_key(r.url));

    for (const auto& d : defaults) {
        auto it = by_id.find(d.id);
        if (it == by_id.end()) {
            // No user row: the catalogue default decides (regional / local-
            // language outlets start off and are one click away in SRC).
            if (d.default_enabled && !user_urls.contains(news_feed_url_key(d.url)))
                out.append(d);
            continue;
        }
        if (!it->enabled)
            continue;
        out.append(apply_overlay(d, *it));
    }
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
            ef.enabled = d.default_enabled;
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
            LOG_ERROR("NewsService", QString("set_feed_enabled failed: %1").arg(QString::fromStdString(r.error())));
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
        LOG_WARN("NewsService", logf("Feed could not be switched on or off", {{"feed", id}, {"error", "unknown feed id"}}));
        return false;
    }
    reload_feeds();
    return true;
}

void NewsService::reload_feeds() {
    fincept::CacheManager::instance().clear_category("news");
    latest_articles_.clear(); // belonged to the old feed set
    emit feeds_changed();
}

} // namespace fincept::services
