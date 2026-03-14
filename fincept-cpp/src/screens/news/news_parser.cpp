#include "news_screen.h"
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif

namespace fincept::news {

// ============================================================================
// Default RSS Feeds
// ============================================================================
std::vector<RSSFeed> NewsScreen::get_default_feeds() {
    return {
        // Tier 1 — Wire Services & Regulators
        {"reuters-biz",    "Reuters Business",       "https://feeds.reuters.com/reuters/businessNews",                                    "MARKETS",     "GLOBAL", "REUTERS",         1},
        {"reuters-mkts",   "Reuters Markets",        "https://feeds.reuters.com/reuters/financialsNews",                                  "MARKETS",     "GLOBAL", "REUTERS",         1},
        {"reuters-world",  "Reuters World",          "https://feeds.reuters.com/Reuters/worldNews",                                       "GEOPOLITICS", "GLOBAL", "REUTERS",         1},
        {"sec-press",      "SEC Press Releases",     "https://www.sec.gov/news/pressreleases.rss",                                        "REGULATORY",  "US",     "SEC",             1},
        {"fed-press",      "Federal Reserve",        "https://www.federalreserve.gov/feeds/press_all.xml",                                "REGULATORY",  "US",     "FED RESERVE",     1},

        // Tier 2 — Major Financial Media
        {"bloomberg-mkts", "Bloomberg Markets",      "https://feeds.bloomberg.com/markets/news.rss",                                      "MARKETS",     "GLOBAL", "BLOOMBERG",       2},
        {"wsj-markets",    "WSJ Markets",            "https://feeds.a.dj.com/rss/RSSMarketsMain.xml",                                     "MARKETS",     "US",     "WSJ",             2},
        {"wsj-world",      "WSJ World",              "https://feeds.a.dj.com/rss/RSSWorldNews.xml",                                       "GEOPOLITICS", "GLOBAL", "WSJ",             2},
        {"cnbc-finance",   "CNBC Finance",           "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=100003114", "MARKETS",  "US",     "CNBC",            2},
        {"marketwatch",    "MarketWatch",            "https://feeds.marketwatch.com/marketwatch/topstories/",                              "MARKETS",     "US",     "MARKETWATCH",     2},
        {"bbc-business",   "BBC Business",           "http://feeds.bbci.co.uk/news/business/rss.xml",                                     "MARKETS",     "GLOBAL", "BBC",             2},
        {"bbc-world",      "BBC World",              "http://feeds.bbci.co.uk/news/world/rss.xml",                                        "GEOPOLITICS", "GLOBAL", "BBC",             2},

        // Tier 2 — Tech
        {"techcrunch",     "TechCrunch",             "https://techcrunch.com/feed/",                                                       "TECH",        "GLOBAL", "TECHCRUNCH",      2},

        // Tier 2 — Crypto
        {"coindesk",       "CoinDesk",               "https://www.coindesk.com/arc/outboundfeeds/rss/",                                   "CRYPTO",      "GLOBAL", "COINDESK",        2},
        {"cointelegraph",  "CoinTelegraph",          "https://cointelegraph.com/rss",                                                      "CRYPTO",      "GLOBAL", "COINTELEGRAPH",   2},

        // Tier 2 — Energy
        {"oilprice",       "OilPrice",               "https://oilprice.com/rss/main",                                                      "ENERGY",      "GLOBAL", "OILPRICE",        2},

        // Tier 2 — Economics
        {"economist",      "The Economist",          "https://www.economist.com/sections/economics/rss.xml",                               "ECONOMIC",    "GLOBAL", "ECONOMIST",       2},

        // Tier 2 — Geopolitics
        {"foreignpolicy",  "Foreign Policy",         "https://foreignpolicy.com/feed/",                                                    "GEOPOLITICS", "GLOBAL", "FOREIGN POLICY",  2},
        {"aljazeera",      "Al Jazeera",             "https://www.aljazeera.com/xml/rss/all.xml",                                          "GEOPOLITICS", "GLOBAL", "AL JAZEERA",      2},
    };
}

// ============================================================================
// Simple XML tag extractor
// ============================================================================
static std::string extract_tag(const std::string& xml, const std::string& tag, size_t start = 0) {
    std::string open = "<" + tag + ">";
    std::string open2 = "<" + tag + " ";
    std::string close = "</" + tag + ">";

    size_t pos = xml.find(open, start);
    size_t content_start = 0;
    if (pos != std::string::npos) {
        content_start = pos + open.size();
    } else {
        pos = xml.find(open2, start);
        if (pos == std::string::npos) return "";
        size_t end_bracket = xml.find('>', pos);
        if (end_bracket == std::string::npos) return "";
        content_start = end_bracket + 1;
    }

    size_t end_pos = xml.find(close, content_start);
    if (end_pos == std::string::npos) return "";

    std::string content = xml.substr(content_start, end_pos - content_start);

    // Handle CDATA
    if (content.find("<![CDATA[") == 0) {
        size_t cdata_end = content.find("]]>");
        if (cdata_end != std::string::npos) {
            content = content.substr(9, cdata_end - 9);
        }
    }

    return content;
}

// Find all <item> or <entry> blocks
static std::vector<std::string> extract_items(const std::string& xml) {
    std::vector<std::string> items;

    std::string open = "<item>";
    std::string open2 = "<item ";
    std::string close = "</item>";
    size_t pos = 0;

    while (true) {
        size_t start = xml.find(open, pos);
        size_t start2 = xml.find(open2, pos);

        size_t item_start = std::string::npos;
        if (start != std::string::npos && start2 != std::string::npos)
            item_start = std::min(start, start2);
        else if (start != std::string::npos)
            item_start = start;
        else if (start2 != std::string::npos)
            item_start = start2;
        else
            break;

        size_t end = xml.find(close, item_start);
        if (end == std::string::npos) break;

        items.push_back(xml.substr(item_start, end + close.size() - item_start));
        pos = end + close.size();
    }

    // If no <item>, try <entry> (Atom)
    if (items.empty()) {
        open = "<entry>";
        open2 = "<entry ";
        close = "</entry>";
        pos = 0;
        while (true) {
            size_t start = xml.find(open, pos);
            size_t start2 = xml.find(open2, pos);
            size_t item_start = std::string::npos;
            if (start != std::string::npos && start2 != std::string::npos)
                item_start = std::min(start, start2);
            else if (start != std::string::npos)
                item_start = start;
            else if (start2 != std::string::npos)
                item_start = start2;
            else
                break;

            size_t end = xml.find(close, item_start);
            if (end == std::string::npos) break;
            items.push_back(xml.substr(item_start, end + close.size() - item_start));
            pos = end + close.size();
        }
    }

    return items;
}

// ============================================================================
// HTML stripping
// ============================================================================
std::string NewsScreen::strip_html(const std::string& html) {
    std::string result;
    result.reserve(html.size());
    bool in_tag = false;
    for (char c : html) {
        if (c == '<') { in_tag = true; continue; }
        if (c == '>') { in_tag = false; continue; }
        if (!in_tag) result += c;
    }
    size_t s = result.find_first_not_of(" \t\n\r");
    size_t e = result.find_last_not_of(" \t\n\r");
    if (s == std::string::npos) return "";
    return result.substr(s, e - s + 1);
}

// ============================================================================
// Relative time
// ============================================================================
std::string NewsScreen::relative_time(int64_t ts) {
    if (ts == 0) return "";
    int64_t now = (int64_t)std::time(nullptr);
    int64_t diff = now - ts;
    if (diff < 0) diff = 0;
    if (diff < 60) return std::to_string(diff) + "s";
    if (diff < 3600) return std::to_string(diff / 60) + "m";
    if (diff < 86400) return std::to_string(diff / 3600) + "h";
    return std::to_string(diff / 86400) + "d";
}

// ============================================================================
// Windows strptime replacement
// ============================================================================
#ifdef _WIN32
static char* strptime(const char* s, const char* fmt, struct tm* t) {
    std::memset(t, 0, sizeof(struct tm));
    std::istringstream input(s);

    const char* p = fmt;
    while (*p) {
        if (*p == '%') {
            p++;
            switch (*p) {
                case 'a': {
                    char buf[16];
                    input.read(buf, 3);
                    if (input.peek() == ',') input.get();
                    if (input.peek() == ' ') input.get();
                    break;
                }
                case 'd': input >> t->tm_mday; break;
                case 'b': case 'B': {
                    char mon[16] = {};
                    input.read(mon, 3);
                    const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
                    for (int i = 0; i < 12; i++) {
                        if (_strnicmp(mon, months[i], 3) == 0) { t->tm_mon = i; break; }
                    }
                    break;
                }
                case 'Y': input >> t->tm_year; t->tm_year -= 1900; break;
                case 'H': input >> t->tm_hour; break;
                case 'M': input >> t->tm_min; break;
                case 'S': input >> t->tm_sec; break;
                case 'm': input >> t->tm_mon; t->tm_mon--; break;
                default: break;
            }
            p++;
        } else {
            if (input.peek() == *p) input.get();
            p++;
        }
    }

    if (input.fail()) return nullptr;
    return const_cast<char*>(s + (size_t)input.tellg());
}
#endif

// ============================================================================
// Parse date string to unix timestamp
// ============================================================================
static int64_t parse_rss_date(const std::string& date_str) {
    if (date_str.empty()) return 0;

    struct tm t = {};

    // Try RFC 2822: "Thu, 13 Mar 2025 14:30:00 GMT"
    if (strptime(date_str.c_str(), "%a, %d %b %Y %H:%M:%S", &t) ||
        strptime(date_str.c_str(), "%d %b %Y %H:%M:%S", &t)) {
        #ifdef _WIN32
        return _mkgmtime(&t);
        #else
        return timegm(&t);
        #endif
    }

    // Try ISO 8601: "2025-03-13T14:30:00Z"
    if (strptime(date_str.c_str(), "%Y-%m-%dT%H:%M:%S", &t) ||
        strptime(date_str.c_str(), "%Y-%m-%d %H:%M:%S", &t)) {
        #ifdef _WIN32
        return _mkgmtime(&t);
        #else
        return timegm(&t);
        #endif
    }

    return 0;
}

// ============================================================================
// Parse RSS XML into articles
// ============================================================================
std::vector<NewsArticle> NewsScreen::parse_rss_xml(const std::string& xml, const RSSFeed& feed) {
    std::vector<NewsArticle> articles;
    auto items = extract_items(xml);

    int idx = 0;
    for (const auto& item_xml : items) {
        idx++;
        NewsArticle a;
        a.id = feed.id + "-" + std::to_string(idx);
        a.source = feed.source;
        a.category = feed.category;
        a.region = feed.region;
        a.tier = feed.tier;

        a.headline = strip_html(extract_tag(item_xml, "title"));
        if (a.headline.empty()) continue;
        if (a.headline.size() > 200) a.headline = a.headline.substr(0, 200);

        std::string desc = extract_tag(item_xml, "description");
        if (desc.empty()) desc = extract_tag(item_xml, "summary");
        if (desc.empty()) desc = extract_tag(item_xml, "content:encoded");
        a.summary = strip_html(desc);
        if (a.summary.size() > 300) a.summary = a.summary.substr(0, 300);

        a.link = extract_tag(item_xml, "link");

        std::string date = extract_tag(item_xml, "pubDate");
        if (date.empty()) date = extract_tag(item_xml, "published");
        if (date.empty()) date = extract_tag(item_xml, "updated");
        if (date.empty()) date = extract_tag(item_xml, "dc:date");

        a.sort_ts = parse_rss_date(date);
        if (a.sort_ts > 0) {
            time_t tt = (time_t)a.sort_ts;
            struct tm* lt = gmtime(&tt);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%b %d, %H:%M", lt);
            a.time_str = buf;
        } else {
            a.sort_ts = (int64_t)std::time(nullptr);
            time_t tt = (time_t)a.sort_ts;
            struct tm* lt = localtime(&tt);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%b %d, %H:%M", lt);
            a.time_str = buf;
        }

        enrich_article(a);
        articles.push_back(std::move(a));
    }

    return articles;
}

// ============================================================================
// Sentiment & Priority Enrichment
// ============================================================================
void NewsScreen::enrich_article(NewsArticle& a) {
    std::string text = a.headline + " " + a.summary;
    std::transform(text.begin(), text.end(), text.begin(), ::tolower);

    // Priority
    if (text.find("breaking") != std::string::npos || text.find("alert") != std::string::npos)
        a.priority = Priority::FLASH;
    else if (text.find("urgent") != std::string::npos || text.find("emergency") != std::string::npos)
        a.priority = Priority::URGENT;
    else if (text.find("announce") != std::string::npos || text.find("report") != std::string::npos)
        a.priority = Priority::BREAKING;

    // Weighted sentiment
    struct WordWeight { const char* word; int weight; };

    static const WordWeight positive[] = {
        {"surge", 3}, {"soar", 3}, {"breakthrough", 3}, {"boom", 3}, {"record high", 3},
        {"rally", 2}, {"gain", 2}, {"rise", 2}, {"jump", 2}, {"climb", 2}, {"boost", 2},
        {"beat", 2}, {"upgrade", 2}, {"profit", 2}, {"growth", 2}, {"recover", 2},
        {"ceasefire", 2}, {"peace", 2}, {"treaty", 2}, {"reform", 2}, {"milestone", 2},
        {"strong", 1}, {"robust", 1}, {"positive", 1}, {"success", 1}, {"win", 1},
        {"deal", 1}, {"confidence", 1}, {"launch", 1}, {"bullish", 1}, {"outperform", 1},
    };

    static const WordWeight negative[] = {
        {"crash", 3}, {"plunge", 3}, {"collapse", 3}, {"devastat", 3}, {"invasion", 3},
        {"bankruptcy", 3}, {"meltdown", 3}, {"pandemic", 3},
        {"fall", 2}, {"drop", 2}, {"decline", 2}, {"tumble", 2}, {"slump", 2},
        {"crisis", 2}, {"conflict", 2}, {"attack", 2}, {"sanction", 2}, {"tariff", 2},
        {"layoff", 2}, {"downgrade", 2}, {"fraud", 2}, {"scandal", 2}, {"recession", 2},
        {"weak", 1}, {"loss", 1}, {"risk", 1}, {"threat", 1}, {"warning", 1},
        {"debt", 1}, {"inflation", 1}, {"bearish", 1}, {"volatile", 1},
    };

    int score = 0;
    for (const auto& w : positive)
        if (text.find(w.word) != std::string::npos) score += w.weight;
    for (const auto& w : negative)
        if (text.find(w.word) != std::string::npos) score -= w.weight;

    if (score >= 2) a.sentiment = Sentiment::BULLISH;
    else if (score <= -2) a.sentiment = Sentiment::BEARISH;
    else a.sentiment = Sentiment::NEUTRAL;

    // Extract tickers ($AAPL, $TSLA etc)
    for (size_t i = 0; i < a.headline.size(); i++) {
        if (a.headline[i] == '$' && i + 1 < a.headline.size() && std::isalpha(a.headline[i + 1])) {
            std::string ticker;
            for (size_t j = i + 1; j < a.headline.size() && std::isalpha(a.headline[j]); j++)
                ticker += (char)std::toupper(a.headline[j]);
            if (ticker.size() >= 1 && ticker.size() <= 5)
                a.tickers.push_back(ticker);
        }
    }
}

} // namespace fincept::news
