#pragma once
// News Terminal — Fincept-style multi-panel news dashboard
// Fetches real RSS feeds, parses XML, enriches with sentiment/priority
// Layout: [Left Sidebar] [Center Wire Feed] [Right Reader]

#include <imgui.h>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

namespace fincept::news {

// Article priority levels
enum class Priority { FLASH, URGENT, BREAKING, ROUTINE };
// Article sentiment
enum class Sentiment { BULLISH, BEARISH, NEUTRAL };

struct NewsArticle {
    std::string id;
    std::string headline;
    std::string summary;
    std::string source;
    std::string category;
    std::string region;
    std::string link;
    std::string time_str;     // formatted time "Mar 13, 14:30"
    int64_t     sort_ts = 0;  // unix timestamp seconds
    Priority    priority = Priority::ROUTINE;
    Sentiment   sentiment = Sentiment::NEUTRAL;
    std::vector<std::string> tickers;
    int         tier = 3;     // 1=wire, 2=major, 3=specialty, 4=blog
};

struct RSSFeed {
    std::string id;
    std::string name;
    std::string url;
    std::string category;
    std::string region;
    std::string source;
    int         tier = 3;
    bool        enabled = true;
};

class NewsScreen {
public:
    void render();

private:
    bool initialized_ = false;

    // Data (mutex-protected)
    std::mutex data_mutex_;
    std::vector<NewsArticle> articles_;
    std::string error_;
    std::atomic<bool> loading_{false};
    double last_fetch_time_ = 0;

    // UI state
    int selected_article_ = -1;
    std::string search_filter_;
    char search_buf_[128] = "";
    int category_filter_ = 0;  // 0=ALL
    int time_filter_ = 0;      // 0=ALL, 1=1h, 2=4h, 3=24h, 4=7d

    // Categories for filter
    static constexpr const char* CATEGORIES[] = {
        "ALL", "MARKETS", "CRYPTO", "TECH", "ENERGY",
        "REGULATORY", "ECONOMIC", "GEOPOLITICS", "GENERAL"
    };
    static constexpr int NUM_CATEGORIES = 9;

    // Init
    void init();

    // Section renderers
    void render_left_sidebar(float width, float height);
    void render_wire_feed(float width, float height);
    void render_article_reader(float width, float height);
    void render_toolbar();

    // Wire row
    void render_wire_row(int index, const NewsArticle& article, float width);

    // Data
    void fetch_news();
    std::vector<NewsArticle> get_filtered_articles();

    // RSS parsing
    static std::vector<NewsArticle> parse_rss_xml(const std::string& xml, const RSSFeed& feed);
    static void enrich_article(NewsArticle& article);
    static std::string strip_html(const std::string& html);
    static std::string relative_time(int64_t ts);

    // Feed list
    static std::vector<RSSFeed> get_default_feeds();
};

} // namespace fincept::news
