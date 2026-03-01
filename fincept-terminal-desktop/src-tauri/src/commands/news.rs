// RSS News Feed Fetcher - High-performance Rust implementation
// Fetches real-time news from 150+ financial, geopolitical, and intelligence RSS feeds without CORS restrictions
// Now supports user-configurable RSS feeds stored in SQLite

use reqwest;
use serde::{Deserialize, Serialize};
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use tokio::time::timeout;
use chrono::{DateTime, FixedOffset, NaiveDateTime, Utc, TimeZone};
use crate::database::pool::get_db;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NewsArticle {
    pub id: String,
    pub time: String,
    pub priority: String, // FLASH, URGENT, BREAKING, ROUTINE
    pub category: String,
    pub headline: String,
    pub summary: String,
    pub source: String,
    pub region: String,
    pub sentiment: String, // BULLISH, BEARISH, NEUTRAL
    pub impact: String,    // HIGH, MEDIUM, LOW
    pub tickers: Vec<String>,
    pub classification: String,
    pub link: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub pub_date: Option<String>,
    /// Unix timestamp (seconds) for reliable cross-feed sorting
    #[serde(default)]
    pub sort_ts: i64,
    /// Source credibility tier: 1=wire, 2=major outlet, 3=specialty, 4=blog/aggregator
    #[serde(default = "default_tier")]
    pub tier: u8,
    /// Reserved for future velocity tracking (always 0 currently)
    #[serde(default)]
    pub velocity_hint: u8,
}

fn default_tier() -> u8 { 3 }

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RSSFeed {
    pub id: Option<String>,
    pub name: String,
    pub url: String,
    pub category: String,
    pub region: String,
    pub source: String,
    pub enabled: bool,
    pub is_default: bool,
    /// Source credibility tier (1-4), serialized for the feed settings modal
    #[serde(default = "default_tier")]
    pub tier: u8,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UserRSSFeed {
    pub id: String,
    pub name: String,
    pub url: String,
    pub category: String,
    pub region: String,
    pub source_name: String,
    pub enabled: bool,
    pub is_default: bool,
    pub created_at: String,
    pub updated_at: String,
}

// Default feed preference info
#[derive(Debug, Clone)]
struct DefaultFeedPref {
    enabled: bool,
    deleted: bool,
}

// Get default feed preferences from database
fn get_default_feed_preferences() -> std::collections::HashMap<String, DefaultFeedPref> {
    let mut prefs = std::collections::HashMap::new();

    let conn = match get_db() {
        Ok(c) => c,
        Err(_) => return prefs,
    };

    let mut stmt = match conn.prepare(
        "SELECT feed_id, enabled, deleted FROM default_rss_feed_preferences"
    ) {
        Ok(s) => s,
        Err(_) => return prefs,
    };

    if let Ok(rows) = stmt.query_map([], |row| {
        Ok((
            row.get::<_, String>(0)?,
            DefaultFeedPref {
                enabled: row.get::<_, i32>(1)? == 1,
                deleted: row.get::<_, i32>(2).unwrap_or(0) == 1,
            }
        ))
    }) {
        for row in rows.flatten() {
            prefs.insert(row.0, row.1);
        }
    }

    prefs
}

// Default RSS feeds (built-in, always available)
// Format: (id, name, url, category, region, source, tier)
//   tier 1 = wire services / official regulators
//   tier 2 = major established outlets
//   tier 3 = specialty / regional / niche
//   tier 4 = blogs / aggregators
fn get_default_rss_feeds() -> Vec<RSSFeed> {
    let prefs = get_default_feed_preferences();

    let default_feeds: Vec<(&str, &str, &str, &str, &str, &str, u8)> = vec![
        // ══════════════════════════════════════════════════════════════════════
        // TIER 1 — Wire Services & Official Regulators
        // ══════════════════════════════════════════════════════════════════════

        // Global Wire Services
        ("default-reuters-world",  "Reuters World News",          "https://feeds.reuters.com/Reuters/worldNews",                                                                        "GEOPOLITICS", "GLOBAL", "REUTERS",           1),
        ("default-reuters-biz",    "Reuters Business",            "https://feeds.reuters.com/reuters/businessNews",                                                                     "MARKETS",     "GLOBAL", "REUTERS",           1),
        ("default-reuters-mkts",   "Reuters Markets",             "https://feeds.reuters.com/reuters/financialsNews",                                                                   "MARKETS",     "GLOBAL", "REUTERS",           1),
        ("default-ap-top",         "AP Top News",                 "https://rsshub.app/apnews/topics/ap-top-news",                                                                       "GEOPOLITICS", "GLOBAL", "AP",                1),
        ("default-ap-business",    "AP Business",                 "https://rsshub.app/apnews/topics/business",                                                                          "MARKETS",     "GLOBAL", "AP",                1),

        // US Government & Regulators
        ("default-sec-press",      "SEC Press Releases",          "https://www.sec.gov/news/pressreleases.rss",                                                                         "REGULATORY",  "US",     "SEC",               1),
        ("default-fed-press",      "Federal Reserve Press",       "https://www.federalreserve.gov/feeds/press_all.xml",                                                                 "REGULATORY",  "US",     "FEDERAL RESERVE",   1),
        ("default-whitehouse",     "White House Briefings",       "https://www.whitehouse.gov/feed/",                                                                                   "GEOPOLITICS", "US",     "WHITE HOUSE",       1),
        ("default-state-dept",     "US State Department",         "https://www.state.gov/rss-feed/press-releases/feed/",                                                                "GEOPOLITICS", "US",     "STATE DEPT",        1),
        ("default-doj-press",      "DOJ Press Releases",          "https://www.justice.gov/feeds/opa/justice-news.xml",                                                                 "REGULATORY",  "US",     "DOJ",               1),
        ("default-treasury-press", "US Treasury Press",           "https://home.treasury.gov/system/files/press-releases/press-releases.rss",                                           "REGULATORY",  "US",     "US TREASURY",       1),
        ("default-cftc-press",     "CFTC Press Releases",         "https://www.cftc.gov/rss/pressroom.rss",                                                                             "REGULATORY",  "US",     "CFTC",              1),

        // International Bodies
        ("default-un-news",        "UN News",                     "https://news.un.org/feed/subscribe/en/news/all/rss.xml",                                                             "GEOPOLITICS", "GLOBAL", "UN",                1),
        ("default-iaea-news",      "IAEA News",                   "https://www.iaea.org/feeds/news",                                                                                    "GEOPOLITICS", "GLOBAL", "IAEA",              1),
        ("default-imf-news",       "IMF News",                    "https://www.imf.org/en/News/rss?language=eng",                                                                       "ECONOMIC",    "GLOBAL", "IMF",               1),
        ("default-worldbank-news", "World Bank News",             "https://feeds.worldbank.org/worldbank/all",                                                                          "ECONOMIC",    "GLOBAL", "WORLD BANK",        1),
        ("default-wto-news",       "WTO News",                    "https://www.wto.org/english/news_e/news_e.rss",                                                                      "REGULATORY",  "GLOBAL", "WTO",               1),
        ("default-ecb-press",      "ECB Press Releases",          "https://www.ecb.europa.eu/rss/press.html",                                                                           "REGULATORY",  "EU",     "ECB",               1),

        // ══════════════════════════════════════════════════════════════════════
        // TIER 2 — Major Financial & Markets Outlets
        // ══════════════════════════════════════════════════════════════════════

        // Core Financial Media
        ("default-bloomberg-mkts", "Bloomberg Markets",           "https://feeds.bloomberg.com/markets/news.rss",                                                                       "MARKETS",     "GLOBAL", "BLOOMBERG",         2),
        ("default-ft",             "Financial Times",             "https://www.ft.com/rss/home",                                                                                        "MARKETS",     "GLOBAL", "FT",                2),
        ("default-wsj-markets",    "WSJ Markets",                 "https://feeds.a.dj.com/rss/RSSMarketsMain.xml",                                                                      "MARKETS",     "US",     "WSJ",               2),
        ("default-wsj-world",      "WSJ World",                   "https://feeds.a.dj.com/rss/RSSWorldNews.xml",                                                                        "GEOPOLITICS", "GLOBAL", "WSJ",               2),
        ("default-marketwatch",    "MarketWatch Top Stories",     "https://feeds.marketwatch.com/marketwatch/topstories/",                                                              "MARKETS",     "US",     "MARKETWATCH",       2),
        ("default-cnbc-finance",   "CNBC Finance",                "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=100003114",                               "MARKETS",     "US",     "CNBC",              2),
        ("default-cnbc-world",     "CNBC World",                  "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=100727362",                               "MARKETS",     "GLOBAL", "CNBC",              2),
        ("default-economist",      "The Economist",               "https://www.economist.com/sections/economics/rss.xml",                                                               "ECONOMIC",    "GLOBAL", "ECONOMIST",         2),
        ("default-barrons",        "Barron's",                    "https://www.barrons.com/xml/rss/3_7510.xml",                                                                         "MARKETS",     "US",     "BARRONS",           2),
        ("default-spglobal-oil",   "S&P Global Oil & Crude",      "https://www.spglobal.com/content/spglobal/energy/us/en/rss/oil-crude.xml",                                          "ENERGY",      "GLOBAL", "S&P GLOBAL",        2),

        // Equities & Earnings
        ("default-seekingalpha",   "Seeking Alpha Market News",   "https://seekingalpha.com/market_currents.xml",                                                                       "MARKETS",     "US",     "SEEKING ALPHA",     2),
        ("default-investopedia",   "Investopedia",                "https://www.investopedia.com/feedbuilder/feed/getfeed/?feedName=rss_articles",                                       "MARKETS",     "US",     "INVESTOPEDIA",      3),

        // ══════════════════════════════════════════════════════════════════════
        // TIER 2 — Global News (World Monitor Full Variant)
        // ══════════════════════════════════════════════════════════════════════

        // International Broadcast
        ("default-bbc-world",      "BBC World News",              "http://feeds.bbci.co.uk/news/world/rss.xml",                                                                         "GEOPOLITICS", "GLOBAL", "BBC",               2),
        ("default-bbc-business",   "BBC Business",                "http://feeds.bbci.co.uk/news/business/rss.xml",                                                                      "MARKETS",     "GLOBAL", "BBC",               2),
        ("default-bbc-tech",       "BBC Technology",              "http://feeds.bbci.co.uk/news/technology/rss.xml",                                                                     "TECH",        "GLOBAL", "BBC",               2),
        ("default-aljazeera",      "Al Jazeera English",          "https://www.aljazeera.com/xml/rss/all.xml",                                                                          "GEOPOLITICS", "GLOBAL", "AL JAZEERA",        2),
        ("default-france24-en",    "France 24 English",           "https://www.france24.com/en/rss",                                                                                    "GEOPOLITICS", "EU",     "FRANCE 24",         2),
        ("default-dw-world",       "DW World",                    "https://rss.dw.com/xml/rss-en-world",                                                                                "GEOPOLITICS", "EU",     "DW",                2),
        ("default-dw-business",    "DW Business",                 "https://rss.dw.com/xml/rss-en-bus",                                                                                  "MARKETS",     "EU",     "DW",                2),
        ("default-euronews",       "Euronews",                    "https://www.euronews.com/rss",                                                                                        "GEOPOLITICS", "EU",     "EURONEWS",          2),
        ("default-nytimes-world",  "NYT World News",              "https://rss.nytimes.com/services/xml/rss/nyt/World.xml",                                                             "GEOPOLITICS", "GLOBAL", "NYT",               2),
        ("default-guardian-world", "Guardian World",              "https://www.theguardian.com/world/rss",                                                                              "GEOPOLITICS", "GLOBAL", "GUARDIAN",          2),
        ("default-guardian-biz",   "Guardian Business",           "https://www.theguardian.com/business/rss",                                                                           "MARKETS",     "GLOBAL", "GUARDIAN",          2),
        ("default-npr-world",      "NPR World",                   "https://feeds.npr.org/1004/rss.xml",                                                                                 "GEOPOLITICS", "US",     "NPR",               2),

        // ══════════════════════════════════════════════════════════════════════
        // TIER 2 — Geopolitics, Defense & Foreign Policy
        // ══════════════════════════════════════════════════════════════════════

        ("default-foreignpolicy",  "Foreign Policy",              "https://foreignpolicy.com/feed/",                                                                                    "GEOPOLITICS", "GLOBAL", "FOREIGN POLICY",    2),
        ("default-foreignaffairs", "Foreign Affairs",             "https://www.foreignaffairs.com/rss.xml",                                                                             "GEOPOLITICS", "GLOBAL", "FOREIGN AFFAIRS",   3),
        ("default-defensenews",    "Defense News",                "https://www.defensenews.com/rss/",                                                                                   "GEOPOLITICS", "GLOBAL", "DEFENSE NEWS",      2),
        ("default-warontherocks",  "War on the Rocks",            "https://warontherocks.com/feed/",                                                                                    "GEOPOLITICS", "GLOBAL", "WAR ON THE ROCKS",  2),
        ("default-defenseone",     "Defense One",                 "https://www.defenseone.com/rss/all/",                                                                                "GEOPOLITICS", "US",     "DEFENSE ONE",       2),
        ("default-breakingdefense","Breaking Defense",            "https://breakingdefense.com/feed/",                                                                                  "GEOPOLITICS", "GLOBAL", "BREAKING DEFENSE",  2),
        ("default-thewarz",        "The War Zone",                "https://www.thedrive.com/the-war-zone/rss",                                                                          "GEOPOLITICS", "GLOBAL", "THE WAR ZONE",      2),
        ("default-usni",           "USNI News",                   "https://news.usni.org/feed",                                                                                         "GEOPOLITICS", "US",     "USNI NEWS",         2),
        ("default-diplomat",       "The Diplomat",                "https://thediplomat.com/feed/",                                                                                      "GEOPOLITICS", "ASIA",   "THE DIPLOMAT",      3),
        ("default-crisis-group",   "International Crisis Group",  "https://www.crisisgroup.org/rss.xml",                                                                                "GEOPOLITICS", "GLOBAL", "CRISIS GROUP",      3),
        ("default-chatham-house",  "Chatham House",               "https://www.chathamhouse.org/feed",                                                                                  "GEOPOLITICS", "GLOBAL", "CHATHAM HOUSE",     3),
        ("default-sipri",          "SIPRI News",                  "https://www.sipri.org/news/rss.xml",                                                                                 "GEOPOLITICS", "GLOBAL", "SIPRI",             3),

        // ══════════════════════════════════════════════════════════════════════
        // TIER 2 — Think Tanks & Policy Research
        // ══════════════════════════════════════════════════════════════════════

        ("default-brookings",      "Brookings Institution",       "https://www.brookings.edu/feed/",                                                                                    "ECONOMIC",    "US",     "BROOKINGS",         3),
        ("default-atlantic-cncl",  "Atlantic Council",            "https://www.atlanticcouncil.org/feed/",                                                                              "GEOPOLITICS", "GLOBAL", "ATLANTIC COUNCIL",  3),
        ("default-rand",           "RAND Corporation",            "https://www.rand.org/pubs/rss/research_briefs.xml",                                                                  "GEOPOLITICS", "GLOBAL", "RAND",              3),
        ("default-carnegie",       "Carnegie Endowment",          "https://carnegieendowment.org/rss/solr/posts",                                                                       "GEOPOLITICS", "GLOBAL", "CARNEGIE",          3),
        ("default-cfr",            "Council on Foreign Relations", "https://www.cfr.org/rss/all-content",                                                                              "GEOPOLITICS", "GLOBAL", "CFR",               3),
        ("default-iiss",           "IISS Analysis",               "https://www.iiss.org/rss-feeds",                                                                                    "GEOPOLITICS", "GLOBAL", "IISS",              3),
        ("default-piie",           "Peterson Institute",          "https://www.piie.com/rss.xml",                                                                                       "ECONOMIC",    "GLOBAL", "PIIE",              3),
        ("default-wilson-ctr",     "Wilson Center",               "https://www.wilsoncenter.org/rss.xml",                                                                               "GEOPOLITICS", "GLOBAL", "WILSON CENTER",     3),
        ("default-rusi",           "RUSI",                        "https://rusi.org/rss.xml",                                                                                           "GEOPOLITICS", "GLOBAL", "RUSI",              3),

        // ══════════════════════════════════════════════════════════════════════
        // TIER 2 — Regional: Asia-Pacific
        // ══════════════════════════════════════════════════════════════════════

        ("default-scmp",           "South China Morning Post",    "https://www.scmp.com/rss/91/feed",                                                                                   "GEOPOLITICS", "ASIA",   "SCMP",              2),
        ("default-nikkei-asia",    "Nikkei Asia",                 "https://asia.nikkei.com/rss/feed/nar",                                                                               "MARKETS",     "ASIA",   "NIKKEI ASIA",       2),
        ("default-japan-today",    "Japan Today",                 "https://japantoday.com/feed",                                                                                        "GEOPOLITICS", "ASIA",   "JAPAN TODAY",       3),
        ("default-hindu-biz",      "The Hindu Business",          "https://www.thehindu.com/business/feeder/default.rss",                                                               "MARKETS",     "INDIA",  "THE HINDU",         2),
        ("default-hindu-nat",      "The Hindu National",          "https://www.thehindu.com/news/national/feeder/default.rss",                                                          "GEOPOLITICS", "INDIA",  "THE HINDU",         2),
        ("default-cna",            "Channel NewsAsia",            "https://www.channelnewsasia.com/api/v1/rss-outbound-feed?_format=xml",                                               "GEOPOLITICS", "ASIA",   "CNA",               2),
        ("default-times-india",    "Times of India Business",     "https://timesofindia.indiatimes.com/rss.cms?id=1898055",                                                             "MARKETS",     "INDIA",  "TIMES OF INDIA",    3),
        ("default-livemint",       "Livemint Markets",            "https://www.livemint.com/rss/markets",                                                                               "MARKETS",     "INDIA",  "LIVEMINT",          3),
        ("default-straits-times",  "The Straits Times",           "https://www.straitstimes.com/news/world/rss.xml",                                                                    "GEOPOLITICS", "ASIA",   "STRAITS TIMES",     2),

        // ══════════════════════════════════════════════════════════════════════
        // TIER 2 — Regional: Middle East & Africa
        // ══════════════════════════════════════════════════════════════════════

        ("default-mee",            "Middle East Eye",             "https://www.middleeasteye.net/rss",                                                                                  "GEOPOLITICS", "MENA",   "MIDDLE EAST EYE",   2),
        ("default-arab-news",      "Arab News",                   "https://www.arabnews.com/rss.xml",                                                                                   "GEOPOLITICS", "MENA",   "ARAB NEWS",         2),
        ("default-al-monitor",     "Al-Monitor",                  "https://www.al-monitor.com/rss",                                                                                     "GEOPOLITICS", "MENA",   "AL-MONITOR",        3),
        ("default-bbc-africa",     "BBC Africa",                  "http://feeds.bbci.co.uk/news/world/africa/rss.xml",                                                                  "GEOPOLITICS", "GLOBAL", "BBC AFRICA",        2),
        ("default-news24",         "News24 South Africa",         "https://feeds.24.com/articles/news24/TopStories/rss",                                                                "GEOPOLITICS", "GLOBAL", "NEWS24",            3),
        ("default-reuters-africa", "Reuters Africa",              "https://feeds.reuters.com/reuters/AFRICANews",                                                                       "GEOPOLITICS", "GLOBAL", "REUTERS AFRICA",    1),

        // ══════════════════════════════════════════════════════════════════════
        // TIER 2 — Regional: Latin America
        // ══════════════════════════════════════════════════════════════════════

        ("default-bbc-latam",      "BBC Americas",                "http://feeds.bbci.co.uk/news/world/latin_america/rss.xml",                                                           "GEOPOLITICS", "LATAM",  "BBC",               2),
        ("default-reuters-latam",  "Reuters Latin America",       "https://feeds.reuters.com/reuters/MexicoandCentralAmericaNews",                                                      "GEOPOLITICS", "LATAM",  "REUTERS",           1),
        ("default-mercopress",     "MercoPress",                  "https://en.mercopress.com/rss",                                                                                      "GEOPOLITICS", "LATAM",  "MERCOPRESS",        3),

        // ══════════════════════════════════════════════════════════════════════
        // TIER 2 — Energy & Commodities
        // ══════════════════════════════════════════════════════════════════════

        ("default-oilprice",       "OilPrice.com",                "https://oilprice.com/rss/main",                                                                                      "ENERGY",      "GLOBAL", "OILPRICE",          2),
        ("default-eia-news",       "EIA Energy News",             "https://www.eia.gov/rss/news.xml",                                                                                   "ENERGY",      "US",     "EIA",               1),
        ("default-mining",         "Mining.com",                  "https://www.mining.com/feed/",                                                                                       "ENERGY",      "GLOBAL", "MINING.COM",        2),
        ("default-platts-oil",     "S&P Global Platts Oil",       "https://www.spglobal.com/content/spglobal/energy/us/en/rss/oil-crude.xml",                                          "ENERGY",      "GLOBAL", "S&P PLATTS",        2),
        ("default-naturalgasintel","Natural Gas Intelligence",    "https://www.naturalgasintel.com/rss/",                                                                               "ENERGY",      "US",     "NGI",               3),
        ("default-rigzone",        "Rigzone",                     "https://www.rigzone.com/news/rss/rigzone_latest.aspx",                                                               "ENERGY",      "GLOBAL", "RIGZONE",           3),
        ("default-iea-news",       "IEA News",                    "https://www.iea.org/rss/news.xml",                                                                                   "ENERGY",      "GLOBAL", "IEA",               1),
        ("default-opec-press",     "OPEC Press",                  "https://www.opec.org/opec_web/en/press_room/rss.htm",                                                                "ENERGY",      "GLOBAL", "OPEC",              1),

        // ══════════════════════════════════════════════════════════════════════
        // TIER 2 — Finance: Markets & Macro (World Monitor Finance Variant)
        // ══════════════════════════════════════════════════════════════════════

        // Forex & Rates
        ("default-fxstreet",       "FXStreet",                    "https://www.fxstreet.com/rss/news",                                                                                  "MARKETS",     "GLOBAL", "FXSTREET",          2),
        ("default-dailyfx",        "DailyFX",                     "https://www.dailyfx.com/feeds/all",                                                                                  "MARKETS",     "GLOBAL", "DAILYFX",           2),
        ("default-forexlive",      "Forexlive",                   "https://www.forexlive.com/feed/",                                                                                    "MARKETS",     "GLOBAL", "FOREXLIVE",         3),
        ("default-action-forex",   "Action Forex",                "https://www.actionforex.com/feed/",                                                                                  "MARKETS",     "GLOBAL", "ACTION FOREX",      3),

        // Bonds & Fixed Income
        ("default-bond-buyer",     "The Bond Buyer",              "https://www.bondbuyer.com/feed",                                                                                     "MARKETS",     "US",     "BOND BUYER",        3),
        ("default-calculated-risk","Calculated Risk",             "https://feeds.feedburner.com/CalculatedRisk",                                                                        "ECONOMIC",    "US",     "CALCULATED RISK",   3),

        // Commodities
        ("default-kitco",          "Kitco Gold & Silver",         "https://www.kitco.com/rss/news/",                                                                                    "MARKETS",     "GLOBAL", "KITCO",             2),
        ("default-gold-eagle",     "Gold Eagle",                  "https://www.gold-eagle.com/feed",                                                                                    "MARKETS",     "GLOBAL", "GOLD EAGLE",        3),

        // Macro & Economic
        ("default-zero-hedge",     "ZeroHedge",                   "https://feeds.feedburner.com/zerohedge/feed",                                                                        "ECONOMIC",    "GLOBAL", "ZEROHEDGE",         3),
        ("default-mises-inst",     "Mises Institute",             "https://mises.org/feed",                                                                                             "ECONOMIC",    "GLOBAL", "MISES",             3),
        ("default-naked-cap",      "Naked Capitalism",            "https://www.nakedcapitalism.com/feed",                                                                               "ECONOMIC",    "GLOBAL", "NAKED CAPITALISM",  4),
        ("default-voxeu",          "VoxEU",                       "https://feeds.feedburner.com/voxeu/voxtalk",                                                                         "ECONOMIC",    "GLOBAL", "VOXEU",             3),

        // IPO & Earnings
        ("default-ipo-monitor",    "IPO Monitor",                 "https://www.ipomonitor.com/rss/ipo-news-latest.rss",                                                                 "EARNINGS",    "US",     "IPO MONITOR",       3),
        ("default-sec-edgar",      "SEC EDGAR Filings",           "https://efts.sec.gov/LATEST/search-index?q=%22form+type%22&dateRange=custom&startdt=2024-01-01&forms=8-K",           "EARNINGS",    "US",     "SEC EDGAR",         1),

        // FinTech & Regulation
        ("default-finextra",       "Finextra",                    "https://www.finextra.com/rss/rss.aspx",                                                                              "BANKING",     "GLOBAL", "FINEXTRA",          2),
        ("default-pymnts",         "PYMNTS.com",                  "https://www.pymnts.com/feed/",                                                                                       "BANKING",     "GLOBAL", "PYMNTS",            3),
        ("default-american-banker","American Banker",             "https://www.americanbanker.com/feed",                                                                                 "BANKING",     "US",     "AMERICAN BANKER",   3),
        ("default-compliance-wk",  "Compliance Week",             "https://www.complianceweek.com/rss",                                                                                  "REGULATORY",  "US",     "COMPLIANCE WEEK",   3),
        ("default-risk-net",       "Risk.net",                    "https://www.risk.net/rss",                                                                                            "REGULATORY",  "GLOBAL", "RISK.NET",          3),

        // Institutional & Sovereign
        ("default-pension-invts",  "Pensions & Investments",      "https://www.pionline.com/rss",                                                                                       "MARKETS",     "US",     "P&I",               3),
        ("default-inst-investor",  "Institutional Investor",      "https://www.institutionalinvestor.com/rss",                                                                           "MARKETS",     "GLOBAL", "INST. INVESTOR",    3),
        ("default-sovereign-wm",   "Sovereign Wealth Fund Inst.", "https://www.swfinstitute.org/feed/",                                                                                 "MARKETS",     "GLOBAL", "SWFI",              3),

        // GCC / Gulf Markets
        ("default-gulf-times",     "Gulf Times Business",         "https://www.gulf-times.com/rss/Business",                                                                            "MARKETS",     "MENA",   "GULF TIMES",        3),
        ("default-khaleej-times",  "Khaleej Times Business",      "https://www.khaleejtimes.com/rss/business",                                                                          "MARKETS",     "MENA",   "KHALEEJ TIMES",     3),
        ("default-zawya",          "Zawya Business",               "https://www.zawya.com/rss/business/",                                                                               "MARKETS",     "MENA",   "ZAWYA",             3),
        ("default-argaam",         "Argaam",                      "https://www.argaam.com/en/rss",                                                                                      "MARKETS",     "MENA",   "ARGAAM",            3),

        // ══════════════════════════════════════════════════════════════════════
        // TIER 2 — Technology & Cybersecurity
        // ══════════════════════════════════════════════════════════════════════

        ("default-techcrunch",     "TechCrunch",                  "https://techcrunch.com/feed/",                                                                                       "TECH",        "GLOBAL", "TECHCRUNCH",        2),
        ("default-wired",          "Wired",                       "https://www.wired.com/feed/rss",                                                                                     "TECH",        "US",     "WIRED",             2),
        ("default-arstechnica",    "Ars Technica",                "http://feeds.arstechnica.com/arstechnica/index",                                                                     "TECH",        "US",     "ARS TECHNICA",      2),
        ("default-mit-tech",       "MIT Technology Review",       "https://www.technologyreview.com/feed/",                                                                             "TECH",        "US",     "MIT TECH REVIEW",   2),
        ("default-verge",          "The Verge",                   "https://www.theverge.com/rss/index.xml",                                                                             "TECH",        "US",     "THE VERGE",         2),
        ("default-hbr-tech",       "HBR Technology",              "https://hbr.org/topic/technology.rss",                                                                               "TECH",        "GLOBAL", "HBR",               2),

        // Cybersecurity
        ("default-krebs-sec",      "Krebs on Security",           "https://krebsonsecurity.com/feed/",                                                                                  "TECH",        "GLOBAL", "KREBS ON SECURITY", 2),
        ("default-schneier-sec",   "Schneier on Security",        "https://www.schneier.com/feed/atom/",                                                                                "TECH",        "GLOBAL", "SCHNEIER",          2),
        ("default-dark-reading",   "Dark Reading",                "https://www.darkreading.com/rss.xml",                                                                                "TECH",        "GLOBAL", "DARK READING",      2),
        ("default-bleeping-comp",  "BleepingComputer",            "https://www.bleepingcomputer.com/feed/",                                                                             "TECH",        "GLOBAL", "BLEEPINGCOMPUTER",  3),
        ("default-therecord",      "The Record (Recorded Future)", "https://therecord.media/feed/",                                                                                     "TECH",        "GLOBAL", "THE RECORD",        2),
        ("default-secweek",        "SecurityWeek",                "https://feeds.feedburner.com/securityweek",                                                                          "TECH",        "GLOBAL", "SECURITYWEEK",      3),

        // ══════════════════════════════════════════════════════════════════════
        // TIER 2 — Cryptocurrency
        // ══════════════════════════════════════════════════════════════════════

        ("default-coindesk",       "CoinDesk",                    "https://www.coindesk.com/arc/outboundfeeds/rss/",                                                                    "CRYPTO",      "GLOBAL", "COINDESK",          2),
        ("default-cointelegraph",  "Cointelegraph",               "https://cointelegraph.com/rss",                                                                                      "CRYPTO",      "GLOBAL", "COINTELEGRAPH",     2),
        ("default-decrypt",        "Decrypt",                     "https://decrypt.co/feed",                                                                                            "CRYPTO",      "GLOBAL", "DECRYPT",           3),
        ("default-theblock",       "The Block",                   "https://www.theblock.co/rss.xml",                                                                                    "CRYPTO",      "GLOBAL", "THE BLOCK",         3),
        ("default-bitcoinist",     "Bitcoinist",                  "https://bitcoinist.com/feed/",                                                                                       "CRYPTO",      "GLOBAL", "BITCOINIST",        3),
        ("default-cryptoslate",    "CryptoSlate",                 "https://cryptoslate.com/feed/",                                                                                      "CRYPTO",      "GLOBAL", "CRYPTOSLATE",       3),
        ("default-coingape",       "CoinGape",                    "https://coingape.com/feed/",                                                                                         "CRYPTO",      "GLOBAL", "COINGAPE",          3),
        ("default-unchained",      "Unchained Podcast",           "https://unchainedpodcast.com/feed/",                                                                                 "CRYPTO",      "GLOBAL", "UNCHAINED",         3),
        ("default-dlnews",         "DL News",                     "https://www.dlnews.com/rss/",                                                                                        "CRYPTO",      "GLOBAL", "DL NEWS",           3),

        // ══════════════════════════════════════════════════════════════════════
        // TIER 3 — OSINT & Intelligence (World Monitor Intel Variant)
        // ══════════════════════════════════════════════════════════════════════

        ("default-bellingcat",     "Bellingcat",                  "https://www.bellingcat.com/feed/",                                                                                   "GEOPOLITICS", "GLOBAL", "BELLINGCAT",        3),
        ("default-oryx",           "Oryx (Equipment Losses)",     "https://www.oryxspioenkop.com/feeds/posts/default",                                                                  "GEOPOLITICS", "GLOBAL", "ORYX",              3),
        ("default-acled",          "ACLED Crisis Monitor",        "https://acleddata.com/feed/",                                                                                        "GEOPOLITICS", "GLOBAL", "ACLED",             3),
        ("default-janes",          "Janes Defense",               "https://www.janes.com/feed/",                                                                                        "GEOPOLITICS", "GLOBAL", "JANES",             3),
        ("default-armscontrol",    "Arms Control Association",     "https://www.armscontrol.org/rss.xml",                                                                               "GEOPOLITICS", "GLOBAL", "ARMS CONTROL ASSOC",3),
        ("default-bullatomsci",    "Bulletin of Atomic Scientists","https://thebulletin.org/feed/",                                                                                     "GEOPOLITICS", "GLOBAL", "BULLETIN ATOMIC SCI",3),
        ("default-nti-nuclear",    "NTI Nuclear",                 "https://www.nti.org/feed/",                                                                                          "GEOPOLITICS", "GLOBAL", "NTI",               3),
        ("default-stimson",        "Stimson Center",              "https://www.stimson.org/feed/",                                                                                      "GEOPOLITICS", "GLOBAL", "STIMSON",           3),

        // ══════════════════════════════════════════════════════════════════════
        // TIER 2 — Central Banks & Economic Data
        // ══════════════════════════════════════════════════════════════════════

        ("default-boe-news",       "Bank of England News",        "https://www.bankofengland.co.uk/rss/news",                                                                          "REGULATORY",  "UK",     "BANK OF ENGLAND",   1),
        ("default-boj-news",       "Bank of Japan",               "https://www.boj.or.jp/en/rss/news.xml",                                                                             "REGULATORY",  "ASIA",   "BANK OF JAPAN",     1),
        ("default-rbi-press",      "Reserve Bank of India",       "https://www.rbi.org.in/rss/rss.aspx?id=11",                                                                         "REGULATORY",  "INDIA",  "RBI",               1),
        ("default-pboc-news",      "PBoC News",                   "https://www.pbc.gov.cn/en/3688229/3688357/index.html",                                                               "REGULATORY",  "CHINA",  "PBOC",              1),
        ("default-bis-press",      "BIS Press Releases",          "https://www.bis.org/press/index.htm",                                                                                "REGULATORY",  "GLOBAL", "BIS",               1),
        ("default-fed-ny-blog",    "NY Fed Liberty Street",       "https://libertystreeteconomics.newyorkfed.org/feed",                                                                 "ECONOMIC",    "US",     "NY FED",            2),
        ("default-bls-press",      "BLS Press Releases",          "https://www.bls.gov/bls/newsrels.htm",                                                                               "ECONOMIC",    "US",     "BLS",               1),
        ("default-bea-news",       "BEA News Releases",           "https://www.bea.gov/rss/all",                                                                                        "ECONOMIC",    "US",     "BEA",               1),
        ("default-nber-digest",    "NBER Research Digest",        "https://www.nber.org/rss/digest.xml",                                                                                "ECONOMIC",    "US",     "NBER",              3),
    ];

    default_feeds.into_iter()
        .filter_map(|(id, name, url, category, region, source, tier)| {
            let pref = prefs.get(id);
            // If deleted, don't include in the list
            if pref.map(|p| p.deleted).unwrap_or(false) {
                return None;
            }
            let enabled = pref.map(|p| p.enabled).unwrap_or(true);
            Some(RSSFeed {
                id: Some(id.to_string()),
                name: name.to_string(),
                url: url.to_string(),
                category: category.to_string(),
                region: region.to_string(),
                source: source.to_string(),
                enabled,
                is_default: true,
                tier,
            })
        }).collect()
}

// Get user RSS feeds from database
fn get_user_rss_feeds_from_db() -> Vec<RSSFeed> {
    let conn = match get_db() {
        Ok(c) => c,
        Err(e) => {
            eprintln!("[News] Failed to get database connection: {}", e);
            return vec![];
        }
    };

    let mut stmt = match conn.prepare(
        "SELECT id, name, url, category, region, source_name, enabled, is_default
         FROM user_rss_feeds WHERE enabled = 1"
    ) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("[News] Failed to prepare statement: {}", e);
            return vec![];
        }
    };

    let feeds_result = stmt.query_map([], |row| {
        Ok(RSSFeed {
            id: Some(row.get::<_, String>(0)?),
            name: row.get::<_, String>(1)?,
            url: row.get::<_, String>(2)?,
            category: row.get::<_, String>(3)?,
            region: row.get::<_, String>(4)?,
            source: row.get::<_, String>(5)?,
            enabled: row.get::<_, i32>(6)? == 1,
            is_default: row.get::<_, i32>(7)? == 1,
            tier: 3, // user-added feeds default to tier 3 (specialty)
        })
    });

    match feeds_result {
        Ok(rows) => rows.filter_map(|r| r.ok()).collect(),
        Err(e) => {
            eprintln!("[News] Query failed: {}", e);
            vec![]
        }
    }
}

// Get all RSS feeds (default + user-configured)
// Get all RSS feeds (default + user-configured) - only enabled ones for fetching
fn get_rss_feeds() -> Vec<RSSFeed> {
    let mut all_feeds: Vec<RSSFeed> = get_default_rss_feeds()
        .into_iter()
        .filter(|f| f.enabled)
        .collect();
    let user_feeds: Vec<RSSFeed> = get_user_rss_feeds_from_db()
        .into_iter()
        .filter(|f| f.enabled)
        .collect();
    all_feeds.extend(user_feeds);
    all_feeds
}

/// Try to parse a date string into a DateTime using multiple common RSS date formats.
fn parse_date_flexible(date_str: &str) -> Option<DateTime<FixedOffset>> {
    let s = date_str.trim();

    // RFC 2822  (most RSS feeds)
    if let Ok(dt) = DateTime::parse_from_rfc2822(s) {
        return Some(dt);
    }
    // RFC 3339 / ISO 8601 with offset  (Atom feeds)
    if let Ok(dt) = DateTime::parse_from_rfc3339(s) {
        return Some(dt);
    }

    // Common variants that chrono's strict parsers miss
    let extra_formats = [
        "%Y-%m-%dT%H:%M:%S%.f%:z",   // 2026-01-27T16:45:00.000+00:00
        "%Y-%m-%dT%H:%M:%S%:z",       // 2026-01-27T16:45:00+00:00
        "%Y-%m-%dT%H:%M:%SZ",         // 2026-01-27T16:45:00Z  (no offset)
        "%Y-%m-%dT%H:%M:%S%.fZ",      // 2026-01-27T16:45:00.000Z
        "%a, %d %b %Y %H:%M:%S %Z",   // Tue, 27 Jan 2026 16:45:00 GMT
        "%d %b %Y %H:%M:%S %z",       // 27 Jan 2026 16:45:00 +0000
        "%Y-%m-%d %H:%M:%S",          // 2026-01-27 16:45:00 (naive)
        "%B %d, %Y %H:%M:%S",         // January 27, 2026 16:45:00
        "%b %d, %Y %H:%M:%S",         // Jan 27, 2026 16:45:00
    ];

    for fmt in &extra_formats {
        // Try with offset
        if let Ok(dt) = DateTime::parse_from_str(s, fmt) {
            return Some(dt);
        }
        // Try as naive (assume UTC)
        if let Ok(naive) = NaiveDateTime::parse_from_str(s, fmt) {
            return Some(Utc.from_utc_datetime(&naive).fixed_offset());
        }
    }

    None
}


// Parse RSS XML to articles
fn parse_rss_feed(xml_text: &str, feed: &RSSFeed) -> Vec<NewsArticle> {
    let mut articles = Vec::new();

    // Use quick-xml for fast parsing
    use quick_xml::events::Event;
    use quick_xml::Reader;

    let mut reader = Reader::from_str(xml_text);
    reader.config_mut().trim_text(true);

    let mut current_item: Option<NewsArticle> = None;
    let mut current_tag = String::new();
    let mut buf = Vec::new();
    let mut item_index: u32 = 0;

    loop {
        match reader.read_event_into(&mut buf) {
            Ok(Event::Start(e)) => {
                let tag_name = String::from_utf8_lossy(e.name().as_ref()).to_string();
                current_tag = tag_name.clone();

                // Start of new item/entry
                if tag_name == "item" || tag_name == "entry" {
                    let timestamp = SystemTime::now()
                        .duration_since(UNIX_EPOCH)
                        .unwrap()
                        .as_millis();
                    item_index += 1;
                    let feed_id = feed.id.as_deref().unwrap_or(&feed.source);
                    current_item = Some(NewsArticle {
                        id: format!("{}-{}-{}", feed_id, timestamp, item_index),
                        time: String::new(),
                        priority: "ROUTINE".to_string(),
                        category: feed.category.clone(),
                        headline: String::new(),
                        summary: String::new(),
                        source: feed.source.clone(),
                        region: feed.region.clone(),
                        sentiment: "NEUTRAL".to_string(),
                        impact: "LOW".to_string(),
                        tickers: Vec::new(),
                        classification: "PUBLIC".to_string(),
                        link: None,
                        pub_date: None,
                        sort_ts: 0,
                        tier: feed.tier,
                        velocity_hint: 0,
                    });
                }
            }
            Ok(Event::Text(e)) => {
                if let Some(ref mut item) = current_item {
                    let text = e.unescape().unwrap_or_default().trim().to_string();
                    if text.is_empty() {
                        // skip empty text nodes
                    } else {
                        // Strip namespace prefix for tag matching (e.g. "dc:date" -> "dc:date", "content:encoded" stays)
                        let tag = current_tag.as_str();
                        match tag {
                            "title" => {
                                if item.headline.is_empty() {
                                    item.headline = text.chars().take(200).collect();
                                }
                            }
                            "description" | "summary" | "content:encoded" => {
                                if item.summary.is_empty() {
                                    let stripped = strip_html_tags(&text);
                                    item.summary = stripped.chars().take(300).collect();
                                }
                            }
                            "link" => {
                                if item.link.is_none() {
                                    item.link = Some(text);
                                }
                            }
                            "pubDate" | "published" | "updated" | "dc:date" | "date" => {
                                if item.pub_date.is_none() {
                                    // Trim aggressively — some feeds have \n or extra spaces
                                    let clean = text.replace('\n', " ").trim().to_string();
                                    item.pub_date = Some(clean.clone());
                                    if let Some(dt) = parse_date_flexible(&clean) {
                                        let ts = dt.timestamp();
                                        item.sort_ts = ts;
                                        item.time = dt.format("%b %d, %H:%M").to_string();
                                    } else {
                                        // Fallback: show raw date string truncated
                                        item.time = clean.chars().take(22).collect();
                                    }
                                }
                            }
                            _ => {}
                        }
                    }
                }
            }
            Ok(Event::End(e)) => {
                let tag_name = String::from_utf8_lossy(e.name().as_ref()).to_string();

                // End of item/entry - process and add to articles
                if (tag_name == "item" || tag_name == "entry") && current_item.is_some() {
                    let mut item = current_item.take().unwrap();

                    // Skip items without headline
                    if item.headline.is_empty() {
                        continue;
                    }

                    // Set default time if no date tag was found at all
                    if item.time.is_empty() {
                        item.time = chrono::Local::now().format("%b %d, %H:%M").to_string();
                    }
                    // Assign current timestamp if no pub_date was parsed (so it still sorts)
                    if item.sort_ts == 0 {
                        item.sort_ts = Utc::now().timestamp();
                    }

                    // Analyze and enrich the article
                    enrich_article(&mut item);

                    articles.push(item);
                }
            }
            Ok(Event::Eof) => break,
            Err(_) => break,
            _ => {}
        }
        buf.clear();
    }

    articles
}

// Strip HTML tags from text
fn strip_html_tags(html: &str) -> String {
    let re = regex::Regex::new(r"<[^>]*>").unwrap();
    re.replace_all(html, "").to_string()
}

// Enrich article with sentiment, priority, tickers, etc.
fn enrich_article(article: &mut NewsArticle) {
    let text_lower = format!("{} {}", article.headline, article.summary).to_lowercase();

    // Determine priority based on keywords
    if text_lower.contains("breaking") || text_lower.contains("alert") {
        article.priority = "FLASH".to_string();
    } else if text_lower.contains("urgent") || text_lower.contains("emergency") {
        article.priority = "URGENT".to_string();
    } else if text_lower.contains("announce") || text_lower.contains("report") {
        article.priority = "BREAKING".to_string();
    }

    // Weighted sentiment analysis — covers finance, geopolitics, and general news
    // (word, weight): higher weight = stronger signal
    let positive_words: Vec<(&str, i32)> = vec![
        // Strong positive (weight 3)
        ("surge", 3), ("soar", 3), ("skyrocket", 3), ("breakthrough", 3), ("boom", 3),
        ("landslide victory", 3), ("historic deal", 3), ("record high", 3), ("blockbuster", 3),
        // Medium positive (weight 2)
        ("rally", 2), ("gain", 2), ("rise", 2), ("jump", 2), ("climb", 2), ("spike", 2),
        ("advance", 2), ("rebound", 2), ("boost", 2), ("beat", 2), ("exceed", 2),
        ("outpace", 2), ("tops", 2), ("upgrade", 2), ("profit", 2), ("growth", 2),
        ("expand", 2), ("recover", 2), ("victory", 2), ("triumph", 2), ("liberate", 2),
        ("peace deal", 2), ("ceasefire", 2), ("treaty", 2), ("alliance", 2), ("reform", 2),
        ("innovate", 2), ("milestone", 2), ("optimism", 2),
        // Mild positive (weight 1)
        ("strong", 1), ("robust", 1), ("stellar", 1), ("record", 1), ("buy", 1),
        ("optimistic", 1), ("positive", 1), ("success", 1), ("win", 1), ("approval", 1),
        ("deal", 1), ("partnership", 1), ("confidence", 1), ("dividend", 1), ("agree", 1),
        ("support", 1), ("progress", 1), ("improve", 1), ("hope", 1), ("promise", 1),
        ("cooperat", 1), ("stabil", 1), ("prosper", 1), ("thrive", 1), ("upbeat", 1),
        ("welcome", 1), ("resolve", 1), ("aid", 1), ("relief", 1), ("ease", 1),
        ("secure", 1), ("bolster", 1), ("strengthen", 1), ("uplift", 1), ("endorse", 1),
        ("acclaim", 1), ("celebrate", 1), ("achieve", 1), ("launch", 1), ("unveil", 1),
        ("recommend", 1), ("embrace", 1), ("momentum", 1), ("recover", 1), ("upturn", 1),
        ("outperform", 1), ("bullish", 1), ("upside", 1), ("favorable", 1),
    ];

    let negative_words: Vec<(&str, i32)> = vec![
        // Strong negative (weight 3)
        ("crash", 3), ("plunge", 3), ("collapse", 3), ("devastat", 3), ("catastroph", 3),
        ("massacre", 3), ("genocide", 3), ("invasion", 3), ("war crime", 3), ("nuclear", 3),
        ("pandemic", 3), ("bankruptcy", 3), ("meltdown", 3), ("freefall", 3),
        // Medium negative (weight 2)
        ("fall", 2), ("drop", 2), ("decline", 2), ("tumble", 2), ("slide", 2), ("sink", 2),
        ("slump", 2), ("tank", 2), ("miss", 2), ("disappoint", 2), ("fail", 2),
        ("recession", 2), ("crisis", 2), ("conflict", 2), ("attack", 2), ("strike", 2),
        ("bomb", 2), ("kill", 2), ("dead", 2), ("death", 2), ("casualt", 2),
        ("sanction", 2), ("tariff", 2), ("embargo", 2), ("escalat", 2), ("tension", 2),
        ("layoff", 2), ("downgrade", 2), ("default", 2), ("fraud", 2), ("indict", 2),
        ("scandal", 2), ("resign", 2), ("impeach", 2), ("coup", 2), ("revolt", 2),
        ("protest", 2), ("riot", 2), ("flee", 2), ("refugee", 2), ("disaster", 2),
        // Mild negative (weight 1)
        ("dip", 1), ("worst", 1), ("lowest", 1), ("poor", 1), ("weak", 1), ("loss", 1),
        ("deficit", 1), ("trouble", 1), ("concern", 1), ("worry", 1), ("fear", 1),
        ("risk", 1), ("threat", 1), ("warning", 1), ("cut", 1), ("sell", 1),
        ("debt", 1), ("lawsuit", 1), ("inflation", 1), ("slowdown", 1), ("unemployment", 1),
        ("bearish", 1), ("negative", 1), ("volatile", 1), ("uncertain", 1), ("stall", 1),
        ("delay", 1), ("setback", 1), ("obstacle", 1), ("challenge", 1), ("oppose", 1),
        ("reject", 1), ("ban", 1), ("block", 1), ("suspend", 1), ("revoke", 1),
        ("accuse", 1), ("allege", 1), ("investigat", 1), ("probe", 1), ("penalt", 1),
        ("fine", 1), ("violat", 1), ("breach", 1), ("hack", 1), ("leak", 1),
        ("contamin", 1), ("pollut", 1), ("drought", 1), ("flood", 1), ("earthquake", 1),
        ("storm", 1), ("shortage", 1), ("disrupt", 1), ("strain", 1), ("divide", 1),
        ("downside", 1), ("headwind", 1), ("erode", 1), ("shrink", 1), ("retreat", 1),
    ];

    let pos_score: i32 = positive_words.iter()
        .filter(|(w, _)| text_lower.contains(w))
        .map(|(_, weight)| weight)
        .sum();
    let neg_score: i32 = negative_words.iter()
        .filter(|(w, _)| text_lower.contains(w))
        .map(|(_, weight)| weight)
        .sum();

    let net = pos_score - neg_score;
    if net >= 2 {
        article.sentiment = "BULLISH".to_string();
    } else if net <= -2 {
        article.sentiment = "BEARISH".to_string();
    } else if net == 1 {
        // Mild positive — still classify if there's any signal
        article.sentiment = "BULLISH".to_string();
    } else if net == -1 {
        article.sentiment = "BEARISH".to_string();
    }
    // net == 0 stays NEUTRAL

    // Determine impact — based on priority + sentiment strength
    let sentiment_strength = net.unsigned_abs();
    if article.priority == "FLASH" || article.priority == "URGENT" || sentiment_strength >= 6 {
        article.impact = "HIGH".to_string();
    } else if article.priority == "BREAKING" || sentiment_strength >= 3 {
        article.impact = "MEDIUM".to_string();
    }

    // Category refinement — check from most-specific to least-specific
    // Override the feed-level category with content-level signals when confident
    if text_lower.contains("earnings") || text_lower.contains("quarterly results")
        || text_lower.contains("eps") || text_lower.contains("guidance")
        || text_lower.contains("revenue beat") || text_lower.contains("revenue miss")
        || text_lower.contains("outlook") || text_lower.contains(" q1 ") || text_lower.contains(" q2 ")
        || text_lower.contains(" q3 ") || text_lower.contains(" q4 ")
    {
        article.category = "EARNINGS".to_string();
    } else if text_lower.contains("crypto") || text_lower.contains("bitcoin")
        || text_lower.contains("ethereum") || text_lower.contains("blockchain")
        || text_lower.contains("defi") || text_lower.contains("nft")
        || text_lower.contains("stablecoin") || text_lower.contains("altcoin")
    {
        article.category = "CRYPTO".to_string();
    } else if text_lower.contains("missile") || text_lower.contains("airstrike")
        || text_lower.contains("troops") || text_lower.contains("pentagon")
        || text_lower.contains("military operation") || text_lower.contains("defense ministry")
        || text_lower.contains("mod ") || text_lower.contains("warship")
        || text_lower.contains("fighter jet") || text_lower.contains("combat")
    {
        article.category = "DEFENSE".to_string();
    } else if text_lower.contains("fed ") || text_lower.contains("federal reserve")
        || text_lower.contains("inflation") || text_lower.contains("gdp")
        || text_lower.contains("jobs report") || text_lower.contains("nonfarm")
        || text_lower.contains("payroll") || text_lower.contains("cpi")
        || text_lower.contains("ppi") || text_lower.contains("pce")
        || text_lower.contains("rate hike") || text_lower.contains("rate cut")
        || text_lower.contains("interest rate") || text_lower.contains("recession")
        || text_lower.contains("unemployment rate") || text_lower.contains("central bank")
    {
        article.category = "ECONOMIC".to_string();
    } else if text_lower.contains("s&p 500") || text_lower.contains("nasdaq")
        || text_lower.contains("dow jones") || text_lower.contains("stock market")
        || text_lower.contains("bull market") || text_lower.contains("bear market")
        || text_lower.contains("market correction") || text_lower.contains("equities")
    {
        article.category = "MARKETS".to_string();
    } else if text_lower.contains("energy") || text_lower.contains(" oil ")
        || text_lower.contains("crude") || text_lower.contains("natural gas")
        || text_lower.contains("opec") || text_lower.contains("lng")
        || text_lower.contains("pipeline") || text_lower.contains("nuclear power")
        || text_lower.contains("renewable") || text_lower.contains("solar")
    {
        article.category = "ENERGY".to_string();
    } else if text_lower.contains("tech") || text_lower.contains(" ai ")
        || text_lower.contains("artificial intelligence") || text_lower.contains("software")
        || text_lower.contains("startup") || text_lower.contains("funding round")
        || text_lower.contains(" ipo ") || text_lower.contains("openai")
        || text_lower.contains("semiconductor") || text_lower.contains(" gpu ")
        || text_lower.contains("silicon valley") || text_lower.contains("venture capital")
    {
        article.category = "TECH".to_string();
    } else if text_lower.contains("bank") || text_lower.contains("financial institution")
        || text_lower.contains("jpmorgan") || text_lower.contains("goldman")
        || text_lower.contains("morgan stanley") || text_lower.contains("credit suisse")
        || text_lower.contains("hedge fund") || text_lower.contains("private equity")
    {
        article.category = "BANKING".to_string();
    } else if text_lower.contains("nato") || text_lower.contains("g7")
        || text_lower.contains("g20") || text_lower.contains("taiwan")
        || text_lower.contains("ukraine") || text_lower.contains("russia")
        || text_lower.contains("china") || text_lower.contains("gaza")
        || text_lower.contains("iran") || text_lower.contains("middle east")
        || text_lower.contains("strait of") || text_lower.contains("war")
        || text_lower.contains("sanctions") || text_lower.contains("geopolit")
    {
        article.category = "GEOPOLITICS".to_string();
    }

    // Extract tickers (simple regex for uppercase 1-5 letter words)
    let ticker_re = regex::Regex::new(r"\b[A-Z]{1,5}\b").unwrap();
    let combined_text = format!("{} {}", article.headline, article.summary);
    let tickers: Vec<String> = ticker_re
        .find_iter(&combined_text)
        .map(|m| m.as_str().to_string())
        .collect::<std::collections::HashSet<_>>()
        .into_iter()
        .take(5)
        .collect();
    article.tickers = tickers;
}

// Fetch single RSS feed with timeout
async fn fetch_rss_feed(feed: RSSFeed, client: &reqwest::Client) -> Vec<NewsArticle> {
    // 10 second timeout per feed
    let fetch_future = client
        .get(&feed.url)
        .header("Accept", "application/rss+xml, application/xml, text/xml, */*")
        .header("User-Agent", "FinceptTerminal/3.0")
        .send();

    match timeout(Duration::from_secs(10), fetch_future).await {
        Ok(Ok(response)) => {
            if response.status().is_success() {
                match response.text().await {
                    Ok(xml_text) => {
                        if xml_text.trim().starts_with('<') {
                            return parse_rss_feed(&xml_text, &feed);
                        }
                    }
                    Err(_) => {}
                }
            }
        }
        _ => {}
    }

    Vec::new()
}

// Fetch all RSS feeds in parallel
#[tauri::command]
pub async fn fetch_all_rss_news() -> Result<Vec<NewsArticle>, String> {
    let feeds = get_rss_feeds();
    let client = reqwest::Client::builder()
        .timeout(Duration::from_secs(10))
        .build()
        .map_err(|e| format!("Failed to create HTTP client: {}", e))?;

    // Fetch all feeds in parallel
    let mut tasks = Vec::new();
    for feed in feeds {
        let client_clone = client.clone();
        tasks.push(tokio::spawn(async move {
            fetch_rss_feed(feed, &client_clone).await
        }));
    }

    let mut all_articles = Vec::new();
    for task in tasks {
        if let Ok(articles) = task.await {
            all_articles.extend(articles);
        }
    }

    // Sort by sort_ts descending (newest first) — interleaves all feeds by actual publish time
    all_articles.sort_by(|a, b| b.sort_ts.cmp(&a.sort_ts));

    Ok(all_articles)
}

// Get RSS feed count
#[tauri::command]
pub fn get_rss_feed_count() -> usize {
    get_rss_feeds().len()
}

// Get active source names
#[tauri::command]
pub fn get_active_sources() -> Vec<String> {
    get_rss_feeds().into_iter().map(|f| f.source).collect()
}

// ============================================================================
// User RSS Feed CRUD Operations
// ============================================================================

/// Get all RSS feeds (default + user-configured)
#[tauri::command]
pub fn get_all_rss_feeds() -> Result<Vec<RSSFeed>, String> {
    Ok(get_rss_feeds())
}

/// Get only default RSS feeds
#[tauri::command]
pub fn get_default_feeds() -> Result<Vec<RSSFeed>, String> {
    Ok(get_default_rss_feeds())
}

/// Get only user-configured RSS feeds from database
#[tauri::command]
pub fn get_user_rss_feeds() -> Result<Vec<UserRSSFeed>, String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;

    let mut stmt = conn.prepare(
        "SELECT id, name, url, category, region, source_name, enabled, is_default, created_at, updated_at
         FROM user_rss_feeds ORDER BY created_at DESC"
    ).map_err(|e| format!("Failed to prepare statement: {}", e))?;

    let feeds: Vec<UserRSSFeed> = stmt
        .query_map([], |row| {
            Ok(UserRSSFeed {
                id: row.get(0)?,
                name: row.get(1)?,
                url: row.get(2)?,
                category: row.get(3)?,
                region: row.get(4)?,
                source_name: row.get(5)?,
                enabled: row.get::<_, i32>(6)? == 1,
                is_default: row.get::<_, i32>(7)? == 1,
                created_at: row.get(8)?,
                updated_at: row.get(9)?,
            })
        })
        .map_err(|e| format!("Query failed: {}", e))?
        .filter_map(|r| r.ok())
        .collect();

    Ok(feeds)
}

/// Add a new user RSS feed
#[tauri::command]
pub fn add_user_rss_feed(
    name: String,
    url: String,
    category: String,
    region: String,
    source_name: String,
) -> Result<UserRSSFeed, String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;

    // Generate unique ID
    let id = format!("user-{}", uuid::Uuid::new_v4().to_string());
    let now = chrono::Utc::now().to_rfc3339();

    conn.execute(
        "INSERT INTO user_rss_feeds (id, name, url, category, region, source_name, enabled, is_default, created_at, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, 1, 0, ?7, ?7)",
        rusqlite::params![id, name, url, category, region, source_name, now],
    ).map_err(|e| format!("Failed to insert feed: {}", e))?;

    Ok(UserRSSFeed {
        id,
        name,
        url,
        category,
        region,
        source_name,
        enabled: true,
        is_default: false,
        created_at: now.clone(),
        updated_at: now,
    })
}

/// Update an existing user RSS feed
#[tauri::command]
pub fn update_user_rss_feed(
    id: String,
    name: String,
    url: String,
    category: String,
    region: String,
    source_name: String,
    enabled: bool,
) -> Result<(), String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;
    let now = chrono::Utc::now().to_rfc3339();

    let rows_updated = conn.execute(
        "UPDATE user_rss_feeds SET name = ?1, url = ?2, category = ?3, region = ?4, source_name = ?5, enabled = ?6, updated_at = ?7
         WHERE id = ?8",
        rusqlite::params![name, url, category, region, source_name, if enabled { 1 } else { 0 }, now, id],
    ).map_err(|e| format!("Failed to update feed: {}", e))?;

    if rows_updated == 0 {
        return Err("Feed not found".to_string());
    }

    Ok(())
}

/// Delete a user RSS feed
#[tauri::command]
pub fn delete_user_rss_feed(id: String) -> Result<(), String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;

    let rows_deleted = conn.execute(
        "DELETE FROM user_rss_feeds WHERE id = ?1",
        rusqlite::params![id],
    ).map_err(|e| format!("Failed to delete feed: {}", e))?;

    if rows_deleted == 0 {
        return Err("Feed not found".to_string());
    }

    Ok(())
}

/// Toggle enabled status of a user RSS feed
#[tauri::command]
pub fn toggle_user_rss_feed(id: String, enabled: bool) -> Result<(), String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;
    let now = chrono::Utc::now().to_rfc3339();

    let rows_updated = conn.execute(
        "UPDATE user_rss_feeds SET enabled = ?1, updated_at = ?2 WHERE id = ?3",
        rusqlite::params![if enabled { 1 } else { 0 }, now, id],
    ).map_err(|e| format!("Failed to toggle feed: {}", e))?;

    if rows_updated == 0 {
        return Err("Feed not found".to_string());
    }

    Ok(())
}

/// Test if an RSS feed URL is valid and accessible
#[tauri::command]
pub async fn test_rss_feed_url(url: String) -> Result<serde_json::Value, String> {
    let client = match reqwest::Client::builder()
        .timeout(Duration::from_secs(15))
        .redirect(reqwest::redirect::Policy::limited(5))
        .danger_accept_invalid_certs(false)
        .build()
    {
        Ok(c) => c,
        Err(e) => return Ok(serde_json::json!({
            "valid": false,
            "error": format!("Failed to create HTTP client: {}", e)
        })),
    };

    let response = match client
        .get(&url)
        .header("Accept", "application/rss+xml, application/xml, text/xml, */*")
        .header("User-Agent", "Mozilla/5.0 (compatible; FinceptTerminal/3.0)")
        .send()
        .await
    {
        Ok(r) => r,
        Err(e) => return Ok(serde_json::json!({
            "valid": false,
            "error": format!("Network error: {}", e)
        })),
    };

    let status = response.status().as_u16();

    if !response.status().is_success() {
        let preview = response.text().await.unwrap_or_default();
        let snippet: String = preview.chars().take(300).collect();
        return Ok(serde_json::json!({
            "valid": false,
            "status": status,
            "error": format!("HTTP {} - server rejected the request", status),
            "response_preview": snippet
        }));
    }

    let text = match response.text().await {
        Ok(t) => t,
        Err(e) => return Ok(serde_json::json!({
            "valid": false,
            "status": status,
            "error": format!("Failed to read response body: {}", e)
        })),
    };

    // Check if it looks like valid RSS/XML
    let trimmed = text.trim();
    let starts_with_tag = trimmed.starts_with('<');
    let has_rss = trimmed.contains("<rss");
    let has_feed = trimmed.contains("<feed");
    let has_channel = trimmed.contains("<channel");
    let is_valid = starts_with_tag && (has_rss || has_feed || has_channel);

    let snippet: String = trimmed.chars().take(300).collect();

    Ok(serde_json::json!({
        "valid": is_valid,
        "status": status,
        "starts_with_tag": starts_with_tag,
        "has_rss_tag": has_rss,
        "has_feed_tag": has_feed,
        "has_channel_tag": has_channel,
        "response_preview": snippet
    }))
}

/// Toggle enabled status of a default RSS feed
#[tauri::command]
pub fn toggle_default_rss_feed(feed_id: String, enabled: bool) -> Result<(), String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;
    let now = chrono::Utc::now().to_rfc3339();

    // Use INSERT OR REPLACE to handle both new and existing preferences
    conn.execute(
        "INSERT OR REPLACE INTO default_rss_feed_preferences (feed_id, enabled, updated_at)
         VALUES (?1, ?2, ?3)",
        rusqlite::params![feed_id, if enabled { 1 } else { 0 }, now],
    ).map_err(|e| format!("Failed to toggle default feed: {}", e))?;

    Ok(())
}

/// Get all default feed preferences
#[tauri::command]
pub fn get_default_feed_prefs() -> Result<std::collections::HashMap<String, bool>, String> {
    let prefs = get_default_feed_preferences();
    Ok(prefs.into_iter().map(|(k, v)| (k, v.enabled)).collect())
}

/// Delete (hide) a default RSS feed
#[tauri::command]
pub fn delete_default_rss_feed(feed_id: String) -> Result<(), String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;
    let now = chrono::Utc::now().to_rfc3339();

    conn.execute(
        "INSERT OR REPLACE INTO default_rss_feed_preferences (feed_id, enabled, deleted, updated_at)
         VALUES (?1, 0, 1, ?2)",
        rusqlite::params![feed_id, now],
    ).map_err(|e| format!("Failed to delete default feed: {}", e))?;

    Ok(())
}

/// Restore a deleted default RSS feed
#[tauri::command]
pub fn restore_default_rss_feed(feed_id: String) -> Result<(), String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;
    let now = chrono::Utc::now().to_rfc3339();

    conn.execute(
        "INSERT OR REPLACE INTO default_rss_feed_preferences (feed_id, enabled, deleted, updated_at)
         VALUES (?1, 1, 0, ?2)",
        rusqlite::params![feed_id, now],
    ).map_err(|e| format!("Failed to restore default feed: {}", e))?;

    Ok(())
}

/// Get list of deleted default feed IDs
#[tauri::command]
pub fn get_deleted_default_feeds() -> Result<Vec<String>, String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;

    let mut stmt = conn.prepare(
        "SELECT feed_id FROM default_rss_feed_preferences WHERE deleted = 1"
    ).map_err(|e| format!("Failed to prepare statement: {}", e))?;

    let feeds: Vec<String> = stmt
        .query_map([], |row| row.get(0))
        .map_err(|e| format!("Query failed: {}", e))?
        .filter_map(|r| r.ok())
        .collect();

    Ok(feeds)
}

/// Restore all deleted default feeds
#[tauri::command]
pub fn restore_all_default_feeds() -> Result<(), String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;

    conn.execute(
        "DELETE FROM default_rss_feed_preferences",
        [],
    ).map_err(|e| format!("Failed to restore all feeds: {}", e))?;

    Ok(())
}
