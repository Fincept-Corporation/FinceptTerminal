#pragma once
// Polymarket Prediction Markets — data types and constants.

#include <string>
#include <vector>
#include <cstdint>

namespace fincept::polymarket {

// ============================================================================
// View modes
// ============================================================================

enum class ViewMode { markets, events, sports, resolved };
enum class SortBy   { volume, liquidity, recent };

// ============================================================================
// Market — a single prediction market question
// ============================================================================

struct Market {
    std::string id;
    std::string question;
    std::string description;
    std::string slug;
    std::string category;
    std::string end_date;
    std::string image;
    std::string condition_id;

    // Outcomes (typically Yes/No)
    std::vector<std::string> outcomes;
    std::vector<double>      outcome_prices;     // 0.0–1.0

    double volume       = 0.0;
    double liquidity    = 0.0;
    double volume_24h   = 0.0;
    double spread       = 0.0;

    bool active   = true;
    bool closed   = false;
    bool archived = false;
    bool featured = false;

    // Token IDs for CLOB
    std::vector<std::string> clob_token_ids;

    // Price changes
    double one_day_change   = 0.0;
    double one_week_change  = 0.0;
    double one_month_change = 0.0;
};

// ============================================================================
// Event — groups multiple related markets
// ============================================================================

struct Event {
    std::string id;
    std::string slug;
    std::string title;
    std::string description;
    std::string start_date;
    std::string end_date;
    std::string image;

    bool active   = true;
    bool closed   = false;
    bool archived = false;

    std::vector<Market> markets;

    double volume    = 0.0;
    double liquidity = 0.0;
    int    comment_count = 0;
};

// ============================================================================
// Tag — category/topic filter
// ============================================================================

struct Tag {
    std::string id;
    std::string label;
    std::string slug;
};

// ============================================================================
// Sport — sports prediction category
// ============================================================================

struct Sport {
    int         id = 0;
    std::string sport;
    std::string image;
    std::string tags;       // comma-separated tag IDs
    std::string series;
};

// ============================================================================
// Trade — recent trade on a market
// ============================================================================

struct Trade {
    std::string id;
    std::string market;
    std::string asset_id;
    std::string side;       // "BUY" or "SELL"
    double      price = 0.0;
    double      size  = 0.0;
    int64_t     timestamp = 0;
};

// ============================================================================
// Order book level
// ============================================================================

struct OrderLevel {
    double price = 0.0;
    double size  = 0.0;
};

struct OrderBook {
    std::string asset_id;
    std::vector<OrderLevel> bids;
    std::vector<OrderLevel> asks;
    double last_trade_price = 0.0;
    double tick_size        = 0.01;
};

// ============================================================================
// Price point for charts
// ============================================================================

struct PricePoint {
    double  price     = 0.0;   // 0.0–1.0
    int64_t timestamp = 0;     // unix seconds
};

// ============================================================================
// Holder info
// ============================================================================

struct Holder {
    std::string wallet;
    std::string name;
    double      amount       = 0.0;
    int         outcome_index = 0;
};

// ============================================================================
// Layout constants
// ============================================================================

static constexpr float LIST_PANEL_WIDTH = 360.0f;
static constexpr int   PAGE_SIZE        = 20;
static constexpr int   MAX_TRADES       = 50;
static constexpr int   MAX_ORDER_LEVELS = 10;

// ============================================================================
// API endpoints
// ============================================================================

static constexpr const char* GAMMA_API_BASE = "https://gamma-api.polymarket.com";

} // namespace fincept::polymarket
