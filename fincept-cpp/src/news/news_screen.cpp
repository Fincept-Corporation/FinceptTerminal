#include "news_screen.h"
#include "http/http_client.h"
#include "theme/bloomberg_theme.h"
#include <imgui.h>
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <thread>
#include <regex>
#include <sstream>
#include <cstring>
#include <map>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace fincept::news {

// ============================================================================
// Default RSS Feeds (subset of main terminal's 150+ feeds)
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
// Simple XML tag extractor (no dependency needed)
// ============================================================================
static std::string extract_tag(const std::string& xml, const std::string& tag, size_t start = 0) {
    std::string open = "<" + tag + ">";
    std::string open2 = "<" + tag + " ";  // tag with attributes
    std::string close = "</" + tag + ">";

    size_t pos = xml.find(open, start);
    size_t content_start = 0;
    if (pos != std::string::npos) {
        content_start = pos + open.size();
    } else {
        pos = xml.find(open2, start);
        if (pos == std::string::npos) return "";
        // Find end of opening tag
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

    // Try <item> (RSS 2.0)
    std::string open = "<item>";
    std::string open2 = "<item ";
    std::string close = "</item>";
    size_t pos = 0;

    while (true) {
        size_t start = xml.find(open, pos);
        size_t start2 = xml.find(open2, pos);

        // Pick whichever comes first
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
    // Trim
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

// Windows doesn't have strptime — minimal implementation
#ifdef _WIN32
static char* strptime(const char* s, const char* fmt, struct tm* t) {
    std::memset(t, 0, sizeof(struct tm));
    std::istringstream input(s);

    const char* p = fmt;
    while (*p) {
        if (*p == '%') {
            p++;
            switch (*p) {
                case 'a': { // abbreviated weekday — skip 3 chars + comma + space
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
// Parse date string to unix timestamp (handles common RSS date formats)
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

        // Extract fields
        a.headline = strip_html(extract_tag(item_xml, "title"));
        if (a.headline.empty()) continue;
        if (a.headline.size() > 200) a.headline = a.headline.substr(0, 200);

        std::string desc = extract_tag(item_xml, "description");
        if (desc.empty()) desc = extract_tag(item_xml, "summary");
        if (desc.empty()) desc = extract_tag(item_xml, "content:encoded");
        a.summary = strip_html(desc);
        if (a.summary.size() > 300) a.summary = a.summary.substr(0, 300);

        a.link = extract_tag(item_xml, "link");

        // Parse date
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

        // Enrich with sentiment/priority
        enrich_article(a);

        articles.push_back(std::move(a));
    }

    return articles;
}

// ============================================================================
// Sentiment & Priority Enrichment (mirrors Rust enrich_article)
// ============================================================================
void NewsScreen::enrich_article(NewsArticle& a) {
    // Build lowercase combined text
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

// ============================================================================
// Fetch news from all feeds (background thread)
// ============================================================================
void NewsScreen::fetch_news() {
    if (loading_) return;
    loading_ = true;
    error_.clear();

    std::thread([this]() {
        auto feeds = get_default_feeds();
        auto& http = http::HttpClient::instance();
        std::vector<NewsArticle> all_articles;

        int success_count = 0;
        int fail_count = 0;

        for (const auto& feed : feeds) {
            if (!feed.enabled) continue;

            http::Headers hdrs;
            hdrs["User-Agent"] = "FinceptTerminal/3.3.1";
            hdrs["Accept"] = "application/rss+xml, application/xml, text/xml, */*";

            auto resp = http.get(feed.url, hdrs);
            if (resp.success && !resp.body.empty()) {
                auto parsed = parse_rss_xml(resp.body, feed);
                all_articles.insert(all_articles.end(), parsed.begin(), parsed.end());
                success_count++;
            } else {
                fail_count++;
            }
        }

        // Sort by timestamp descending (newest first)
        std::sort(all_articles.begin(), all_articles.end(),
            [](const NewsArticle& a, const NewsArticle& b) { return a.sort_ts > b.sort_ts; });

        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            articles_ = std::move(all_articles);
            if (success_count == 0) {
                error_ = "Failed to fetch news from all feeds";
            } else if (fail_count > 0) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "Fetched %d feeds (%d failed)", success_count, fail_count);
                error_ = buf;
            }
        }

        loading_ = false;
        last_fetch_time_ = (double)std::time(nullptr);
        std::printf("[News] Fetched %d articles from %d feeds\n", (int)articles_.size(), success_count);
    }).detach();
}

// ============================================================================
// Get filtered articles based on UI state
// ============================================================================
std::vector<NewsArticle> NewsScreen::get_filtered_articles() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    std::vector<NewsArticle> filtered;

    int64_t now = (int64_t)std::time(nullptr);
    int64_t time_cutoff = 0;
    switch (time_filter_) {
        case 1: time_cutoff = now - 3600; break;      // 1h
        case 2: time_cutoff = now - 4 * 3600; break;  // 4h
        case 3: time_cutoff = now - 86400; break;      // 24h
        case 4: time_cutoff = now - 7 * 86400; break;  // 7d
    }

    std::string search_lower;
    if (search_buf_[0]) {
        search_lower = search_buf_;
        std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
    }

    for (const auto& a : articles_) {
        // Time filter
        if (time_cutoff > 0 && a.sort_ts < time_cutoff) continue;

        // Category filter
        if (category_filter_ > 0) {
            if (a.category != CATEGORIES[category_filter_]) continue;
        }

        // Search filter
        if (!search_lower.empty()) {
            std::string h = a.headline;
            std::transform(h.begin(), h.end(), h.begin(), ::tolower);
            std::string s = a.source;
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            if (h.find(search_lower) == std::string::npos &&
                s.find(search_lower) == std::string::npos)
                continue;
        }

        filtered.push_back(a);
    }

    return filtered;
}

// ============================================================================
// Init
// ============================================================================
void NewsScreen::init() {
    if (initialized_) return;
    initialized_ = true;
    fetch_news();
}

// ============================================================================
// Toolbar — two rows: top = refresh+search+count, bottom = time+category filters
// ============================================================================
void NewsScreen::render_toolbar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##news_toolbar", ImVec2(0, (ImGui::GetFrameHeight() + 4) * 2), ImGuiChildFlags_Borders);

    float avail_w = ImGui::GetContentRegionAvail().x;

    // Row 1: Refresh | Search | Article count
    if (loading_) {
        ImGui::TextColored(ACCENT, "LOADING...");
    } else {
        if (theme::AccentButton("REFRESH")) {
            fetch_news();
        }
    }
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "SEARCH:");
    ImGui::SameLine(0, 4);
    float search_w = std::min(200.0f, avail_w * 0.25f);
    ImGui::PushItemWidth(search_w);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_WIDGET);
    ImGui::InputText("##news_search", search_buf_, sizeof(search_buf_));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    // Article count (right-aligned)
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        char count_buf[64];
        std::snprintf(count_buf, sizeof(count_buf), "%d ARTICLES", (int)articles_.size());
        float tw = ImGui::CalcTextSize(count_buf).x + 8;
        float right_pos = avail_w - tw;
        if (right_pos > ImGui::GetCursorPosX() + 20) {
            ImGui::SameLine(right_pos);
            ImGui::TextColored(TEXT_DIM, "%s", count_buf);
        }
    }

    // Row 2: Time filters | Category filter
    ImGui::TextColored(TEXT_DIM, "TIME:");
    ImGui::SameLine(0, 4);
    const char* time_labels[] = {"ALL", "1H", "4H", "24H", "7D"};
    for (int i = 0; i < 5; i++) {
        bool active = (time_filter_ == i);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
            ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        }
        char label[16];
        std::snprintf(label, sizeof(label), "%s##tf%d", time_labels[i], i);
        if (ImGui::SmallButton(label)) time_filter_ = i;
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 2);
    }

    ImGui::SameLine(0, 12);
    ImGui::TextColored(TEXT_DIM, "CAT:");
    ImGui::SameLine(0, 4);
    float combo_w = std::min(110.0f, avail_w * 0.12f);
    ImGui::PushItemWidth(combo_w);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_WIDGET);
    if (ImGui::BeginCombo("##cat_filter", CATEGORIES[category_filter_], ImGuiComboFlags_NoArrowButton)) {
        for (int i = 0; i < NUM_CATEGORIES; i++) {
            bool sel = (category_filter_ == i);
            if (ImGui::Selectable(CATEGORIES[i], sel))
                category_filter_ = i;
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Wire Row — single dense article line (Bloomberg wire style)
// ============================================================================
void NewsScreen::render_wire_row(int index, const NewsArticle& article, float width) {
    using namespace theme::colors;

    bool selected = (index == selected_article_);
    bool is_hot = (article.priority == Priority::FLASH || article.priority == Priority::URGENT);

    ImVec4 pri_color;
    switch (article.priority) {
        case Priority::FLASH:    pri_color = ERROR_RED; break;
        case Priority::URGENT:   pri_color = ACCENT; break;
        case Priority::BREAKING: pri_color = WARNING; break;
        default:                 pri_color = TEXT_DISABLED; break;
    }

    ImVec4 sent_color;
    switch (article.sentiment) {
        case Sentiment::BULLISH: sent_color = MARKET_GREEN; break;
        case Sentiment::BEARISH: sent_color = MARKET_RED; break;
        default:                 sent_color = WARNING; break;
    }

    // Row background
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float row_h = ImGui::GetTextLineHeightWithSpacing() + 4;
    ImVec4 bg = selected ? ImVec4(INFO.x, INFO.y, INFO.z, 0.08f) :
                ImVec4(0, 0, 0, 0);

    ImGui::PushID(index);

    // Invisible button for the whole row
    if (ImGui::InvisibleButton("##row", ImVec2(width - 16, row_h))) {
        selected_article_ = index;
    }
    bool hovered = ImGui::IsItemHovered();
    if (hovered) bg = ImVec4(1, 1, 1, 0.03f);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background fill
    dl->AddRectFilled(cursor, ImVec2(cursor.x + width - 16, cursor.y + row_h),
        ImGui::ColorConvertFloat4ToU32(bg));

    // Left border (selection or priority indicator)
    if (selected) {
        dl->AddRectFilled(cursor, ImVec2(cursor.x + 2, cursor.y + row_h),
            ImGui::ColorConvertFloat4ToU32(INFO));
    } else if (is_hot) {
        dl->AddRectFilled(cursor, ImVec2(cursor.x + 2, cursor.y + row_h),
            ImGui::ColorConvertFloat4ToU32(pri_color));
    }

    // Bottom border
    dl->AddLine(ImVec2(cursor.x, cursor.y + row_h),
                ImVec2(cursor.x + width - 16, cursor.y + row_h),
                ImGui::ColorConvertFloat4ToU32(BORDER_DIM));

    float row_w = width - 16;
    float x = cursor.x + 6;
    float y = cursor.y + 2;

    // Column widths as percentages of row width
    float time_w   = row_w * 0.05f;  // 5%
    float dot_w    = 14.0f;           // fixed dot
    float source_w = row_w * 0.10f;  // 10%
    float sent_w   = 16.0f;           // fixed sentiment icon
    float tail_w   = sent_w + 8;     // right margin
    float head_w   = row_w - time_w - dot_w - source_w - tail_w - 12; // rest = headline

    // Time
    std::string rel = relative_time(article.sort_ts);
    ImGui::PushClipRect(ImVec2(x, y), ImVec2(x + time_w, y + ImGui::GetTextLineHeight()), true);
    dl->AddText(ImVec2(x, y), ImGui::ColorConvertFloat4ToU32(TEXT_DISABLED), rel.c_str());
    ImGui::PopClipRect();
    x += time_w;

    // Priority dot
    float dot_y = y + ImGui::GetTextLineHeight() * 0.5f;
    dl->AddCircleFilled(ImVec2(x + 5, dot_y), 2.5f,
        ImGui::ColorConvertFloat4ToU32(pri_color));
    if (is_hot) {
        dl->AddCircle(ImVec2(x + 5, dot_y), 4.0f,
            ImGui::ColorConvertFloat4ToU32(ImVec4(pri_color.x, pri_color.y, pri_color.z, 0.3f)));
    }
    x += dot_w;

    // Source (clipped)
    ImGui::PushClipRect(ImVec2(x, y), ImVec2(x + source_w, y + ImGui::GetTextLineHeight()), true);
    dl->AddText(ImVec2(x, y), ImGui::ColorConvertFloat4ToU32(INFO), article.source.c_str());
    ImGui::PopClipRect();
    x += source_w + 4;

    // Headline (clipped to remaining space before sentiment)
    if (head_w > 10) {
        ImVec4 hl_color = selected ? TEXT_PRIMARY :
                          is_hot ? TEXT_SECONDARY : TEXT_DIM;
        ImGui::PushClipRect(ImVec2(x, y), ImVec2(x + head_w, y + ImGui::GetTextLineHeight()), true);
        dl->AddText(ImVec2(x, y), ImGui::ColorConvertFloat4ToU32(hl_color),
            article.headline.c_str());
        ImGui::PopClipRect();
    }

    // Sentiment arrow (right-aligned)
    float right_x = cursor.x + row_w - sent_w;
    const char* sent_icon = article.sentiment == Sentiment::BULLISH ? "^" :
                            article.sentiment == Sentiment::BEARISH ? "v" : "-";
    dl->AddText(ImVec2(right_x, y), ImGui::ColorConvertFloat4ToU32(sent_color), sent_icon);

    ImGui::PopID();
}

// ============================================================================
// Left Sidebar — sentiment summary, top stories, category counts
// ============================================================================
void NewsScreen::render_left_sidebar(float width, float height) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##news_left", ImVec2(width, height), ImGuiChildFlags_Borders);

    auto filtered = get_filtered_articles();

    // Sentiment summary
    ImGui::TextColored(ACCENT, "SENTIMENT");
    ImGui::Separator();
    int bullish = 0, bearish = 0, neutral = 0;
    for (const auto& a : filtered) {
        if (a.sentiment == Sentiment::BULLISH) bullish++;
        else if (a.sentiment == Sentiment::BEARISH) bearish++;
        else neutral++;
    }
    int total = (int)filtered.size();
    if (total > 0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "BULLISH  %d (%.0f%%)", bullish, 100.0f * bullish / total);
        ImGui::TextColored(MARKET_GREEN, "%s", buf);
        std::snprintf(buf, sizeof(buf), "BEARISH  %d (%.0f%%)", bearish, 100.0f * bearish / total);
        ImGui::TextColored(MARKET_RED, "%s", buf);
        std::snprintf(buf, sizeof(buf), "NEUTRAL  %d (%.0f%%)", neutral, 100.0f * neutral / total);
        ImGui::TextColored(WARNING, "%s", buf);
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Top Stories (top 10 by priority + recency)
    ImGui::TextColored(ACCENT, "TOP STORIES");
    ImGui::Separator();

    // Sort by priority weight + recency
    auto top = filtered;
    std::sort(top.begin(), top.end(), [](const NewsArticle& a, const NewsArticle& b) {
        auto pri_weight = [](Priority p) -> int {
            switch (p) { case Priority::FLASH: return 4; case Priority::URGENT: return 3;
                         case Priority::BREAKING: return 2; default: return 1; }
        };
        int wa = pri_weight(a.priority) * 1000 + (int)(a.sort_ts / 60);
        int wb = pri_weight(b.priority) * 1000 + (int)(b.sort_ts / 60);
        return wa > wb;
    });

    int count = std::min((int)top.size(), 10);
    for (int i = 0; i < count; i++) {
        ImGui::PushID(i + 1000);
        bool clicked = false;

        // Rank number
        char rank[8]; std::snprintf(rank, sizeof(rank), "%d", i + 1);
        ImGui::TextColored(ACCENT, "%s", rank);
        ImGui::SameLine(0, 6);

        // Headline (wrapped to 2 lines)
        float avail = ImGui::GetContentRegionAvail().x;
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + avail);

        ImVec4 hl_col = top[i].priority == Priority::FLASH ? TEXT_PRIMARY :
                        top[i].priority == Priority::URGENT ? TEXT_SECONDARY : TEXT_DIM;

        ImGui::TextColored(hl_col, "%s", top[i].headline.c_str());
        if (ImGui::IsItemClicked()) {
            // Find index in filtered articles
            for (int j = 0; j < (int)filtered.size(); j++) {
                if (filtered[j].id == top[i].id) { selected_article_ = j; break; }
            }
        }
        ImGui::PopTextWrapPos();

        // Source + time
        ImGui::TextColored(INFO, "%s", top[i].source.c_str());
        ImGui::SameLine(0, 6);
        ImGui::TextColored(TEXT_DISABLED, "%s", relative_time(top[i].sort_ts).c_str());

        ImGui::Spacing();
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Category counts
    ImGui::TextColored(ACCENT, "CATEGORIES");
    ImGui::Separator();
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        std::map<std::string, int> cat_counts;
        for (const auto& a : articles_) cat_counts[a.category]++;
        for (const auto& [cat, cnt] : cat_counts) {
            char buf[64]; std::snprintf(buf, sizeof(buf), "%-12s %d", cat.c_str(), cnt);
            ImGui::TextColored(TEXT_DIM, "%s", buf);
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Source counts
    ImGui::TextColored(ACCENT, "SOURCES");
    ImGui::Separator();
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        std::map<std::string, int> src_counts;
        for (const auto& a : articles_) src_counts[a.source]++;
        for (const auto& [src, cnt] : src_counts) {
            char buf[64]; std::snprintf(buf, sizeof(buf), "%-14s %d", src.c_str(), cnt);
            ImGui::TextColored(TEXT_DIM, "%s", buf);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Wire Feed — center panel with scrollable article list
// ============================================================================
void NewsScreen::render_wire_feed(float width, float height) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##wire_feed", ImVec2(width, height), ImGuiChildFlags_Borders);

    auto filtered = get_filtered_articles();

    if (loading_ && filtered.empty()) {
        ImGui::SetCursorPos(ImVec2(width * 0.4f, height * 0.4f));
        ImGui::TextColored(ACCENT, "LOADING NEWS FEEDS...");
        theme::LoadingSpinner("Fetching...");
    } else if (filtered.empty()) {
        ImGui::SetCursorPos(ImVec2(width * 0.3f, height * 0.4f));
        ImGui::TextColored(TEXT_DIM, "No articles match your filters");
    } else {
        // Wire header
        float hw = ImGui::GetContentRegionAvail().x;
        ImGui::TextColored(TEXT_DISABLED, "TIME");
        ImGui::SameLine(hw * 0.07f);
        ImGui::TextColored(TEXT_DISABLED, "SOURCE");
        ImGui::SameLine(hw * 0.20f);
        ImGui::TextColored(TEXT_DISABLED, "HEADLINE");
        ImGui::Separator();

        // Scrollable wire rows
        ImGui::BeginChild("##wire_scroll", ImVec2(0, 0));
        for (int i = 0; i < (int)filtered.size(); i++) {
            render_wire_row(i, filtered[i], width);
        }
        ImGui::EndChild();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Article Reader — right panel, shows selected article detail
// ============================================================================
void NewsScreen::render_article_reader(float width, float height) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##article_reader", ImVec2(width, height), ImGuiChildFlags_Borders);

    auto filtered = get_filtered_articles();

    if (selected_article_ < 0 || selected_article_ >= (int)filtered.size()) {
        ImGui::SetCursorPos(ImVec2(width * 0.15f, height * 0.4f));
        ImGui::TextColored(TEXT_DISABLED, "Select an article to read");
    } else {
        const auto& a = filtered[selected_article_];

        // Priority badge
        const char* pri_label = "ROUTINE";
        ImVec4 pri_color = TEXT_DISABLED;
        switch (a.priority) {
            case Priority::FLASH:    pri_label = "FLASH"; pri_color = ERROR_RED; break;
            case Priority::URGENT:   pri_label = "URGENT"; pri_color = ACCENT; break;
            case Priority::BREAKING: pri_label = "BREAKING"; pri_color = WARNING; break;
            default: break;
        }
        ImGui::TextColored(pri_color, "[%s]", pri_label);
        ImGui::SameLine(0, 8);

        // Sentiment badge
        const char* sent_label = "NEUTRAL";
        ImVec4 sent_color = WARNING;
        switch (a.sentiment) {
            case Sentiment::BULLISH: sent_label = "BULLISH"; sent_color = MARKET_GREEN; break;
            case Sentiment::BEARISH: sent_label = "BEARISH"; sent_color = MARKET_RED; break;
            default: break;
        }
        ImGui::TextColored(sent_color, "[%s]", sent_label);

        ImGui::Spacing();

        // Headline
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 20);
        ImGui::TextColored(TEXT_PRIMARY, "%s", a.headline.c_str());
        ImGui::PopTextWrapPos();

        ImGui::Spacing();

        // Metadata — compact single line with wrapping fallback
        {
            char meta[256];
            std::snprintf(meta, sizeof(meta), "%s | %s | %s | %s",
                a.source.c_str(), a.time_str.c_str(), a.category.c_str(), a.region.c_str());
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 20);
            ImGui::TextColored(INFO, "%s", meta);
            ImGui::PopTextWrapPos();
        }

        // Tickers
        if (!a.tickers.empty()) {
            ImGui::Spacing();
            for (const auto& t : a.tickers) {
                ImGui::SameLine(0, 4);
                ImGui::TextColored(WARNING, "$%s", t.c_str());
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Summary
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 20);
        ImGui::TextColored(TEXT_SECONDARY, "%s", a.summary.empty() ? "No summary available." : a.summary.c_str());
        ImGui::PopTextWrapPos();

        ImGui::Spacing();
        ImGui::Spacing();

        // Open link button
        if (!a.link.empty()) {
            if (theme::AccentButton("OPEN IN BROWSER")) {
#ifdef _WIN32
                ShellExecuteA(nullptr, "open", a.link.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif __APPLE__
                std::string cmd = "open " + a.link;
                system(cmd.c_str());
#else
                std::string cmd = "xdg-open " + a.link;
                system(cmd.c_str());
#endif
            }
            ImGui::SameLine(0, 8);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 120);
            ImGui::TextColored(TEXT_DISABLED, "%s", a.link.c_str());
            ImGui::PopTextWrapPos();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Main Render
// ============================================================================
void NewsScreen::render() {
    init();

    using namespace theme::colors;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tab_bar_h = ImGui::GetFrameHeight() + 4;
    float footer_h = ImGui::GetFrameHeight();

    float work_y = vp->WorkPos.y + tab_bar_h;
    float work_h = vp->WorkSize.y - tab_bar_h - footer_h;
    float work_w = vp->WorkSize.x;

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, work_y));
    ImGui::SetNextWindowSize(ImVec2(work_w, work_h));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARKEST);

    ImGui::Begin("##news", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Title bar — safe layout
    float content_w = ImGui::GetContentRegionAvail().x;
    ImGui::TextColored(ACCENT, "NEWS TERMINAL");

    // Right-aligned feed/article count
    {
        char info[64];
        std::snprintf(info, sizeof(info), "%d FEEDS | %d ARTICLES",
            (int)get_default_feeds().size(), (int)articles_.size());
        float info_w = ImGui::CalcTextSize(info).x + 8;
        float right_pos = content_w - info_w;
        if (right_pos > ImGui::GetCursorPosX() + 50) {
            ImGui::SameLine(right_pos);
            ImGui::TextColored(TEXT_DISABLED, "%s", info);
        }
    }

    ImGui::Separator();

    // Toolbar
    render_toolbar();

    // Error banner
    if (!error_.empty()) {
        bool is_error = error_.find("Failed") != std::string::npos;
        ImGui::TextColored(is_error ? ERROR_RED : WARNING, "%s", error_.c_str());
    }

    // Three-panel layout — percentage based
    float avail_w = ImGui::GetContentRegionAvail().x;
    float avail_h = ImGui::GetContentRegionAvail().y;
    float gap = 4.0f;
    float left_w = avail_w * 0.18f;   // 18% sidebar
    float right_w = avail_w * 0.24f;  // 24% reader
    float center_w = avail_w - left_w - right_w - gap * 2; // rest = wire feed

    // Clamp minimums
    if (left_w < 160) left_w = 160;
    if (right_w < 200) right_w = 200;
    center_w = avail_w - left_w - right_w - gap * 2;
    if (center_w < 200) center_w = 200;

    // Left sidebar
    render_left_sidebar(left_w, avail_h);

    ImGui::SameLine(0, gap);

    // Center wire feed
    render_wire_feed(center_w, avail_h);

    ImGui::SameLine(0, gap);

    // Right article reader
    render_article_reader(right_w, avail_h);

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // Auto-refresh every 5 minutes
    double now = (double)std::time(nullptr);
    if (!loading_ && last_fetch_time_ > 0 && (now - last_fetch_time_) > 300) {
        fetch_news();
    }
}

} // namespace fincept::news
