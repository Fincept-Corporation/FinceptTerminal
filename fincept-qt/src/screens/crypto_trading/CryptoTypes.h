#pragma once
// Crypto Trading types — UI state, enums, constants
// Kept separate from trading engine types to avoid coupling.

#include <QColor>
#include <QString>
#include <QVector>

namespace fincept::screens::crypto {

// Order book level (UI-ready, with cumulative for depth bars)
struct OBLevel {
    double price = 0.0;
    double amount = 0.0;
    double cumulative = 0.0;
};

// Order book view mode
enum class ObViewMode { Book, Volume, Imbalance, Signals };

// Tick snapshot for order book analytics
struct TickSnapshot {
    int64_t timestamp = 0;
    double best_bid = 0.0;
    double best_ask = 0.0;
    double bid_qty[3] = {};
    double ask_qty[3] = {};
    double imbalance = 0.0;
    double rise_ratio_60 = 0.0;
};

// Trade entry for Time & Sales feed
struct TradeEntry {
    QString id;
    QString side; // "buy" | "sell"
    double price = 0.0;
    double amount = 0.0;
    int64_t timestamp = 0;
};

// Market info data (funding rate, OI, fees)
struct MarketInfoData {
    double funding_rate = 0.0;
    double mark_price = 0.0;
    double index_price = 0.0;
    double open_interest = 0.0;
    double open_interest_value = 0.0;
    double maker_fee = 0.0;
    double taker_fee = 0.0;
    int64_t next_funding_time = 0;
    bool has_data = false;
};

// Bottom panel tab
enum class BottomTab { Positions, Orders, Trades, MarketInfo, Stats, TimeSales, DepthChart };

// Trading mode
enum class TradingMode { Paper, Live };

// ── Constants ────────────────────────────────────────────────────────────────
inline constexpr int OHLCV_FETCH_COUNT = 200;
inline constexpr double DEFAULT_PAPER_BALANCE = 100000.0;
inline constexpr float TICKER_POLL_INTERVAL = 3.0f;
inline constexpr float ORDERBOOK_POLL_INTERVAL = 3.0f;
inline constexpr float WATCHLIST_POLL_INTERVAL = 15.0f;
inline constexpr float PORTFOLIO_REFRESH_INTERVAL = 1.5f;
inline constexpr float MARKET_INFO_INTERVAL = 30.0f;
inline constexpr int MAX_CANDLE_BUFFER = 500;
inline constexpr int MAX_TRADES = 200;
inline constexpr int OB_MAX_DISPLAY_LEVELS = 12;

// OB analytics thresholds
inline constexpr double OB_IMBALANCE_BUY_THRESHOLD = 0.30;
inline constexpr double OB_IMBALANCE_SELL_THRESHOLD = -0.30;
inline constexpr int OB_MAX_TICK_HISTORY = 3600;
inline constexpr int OB_TICK_CAPTURE_MS = 1000;
inline constexpr int MAX_TIME_SALES_TRADES = 200;
inline constexpr int TRADES_POLL_INTERVAL_MS = 5000;
inline constexpr int CLOCK_UPDATE_MS = 1000;

// Obsidian Design System Colors
inline const QColor COLOR_BUY = QColor("#16a34a");    // POSITIVE
inline const QColor COLOR_SELL = QColor("#dc2626");   // NEGATIVE
inline const QColor COLOR_DIM = QColor("#808080");    // TEXT_SECONDARY
inline const QColor COLOR_ACCENT = QColor("#d97706"); // AMBER

// Background layers
inline const QColor BG_BASE = QColor("#080808");
inline const QColor BG_SURFACE = QColor("#0a0a0a");
inline const QColor BG_RAISED = QColor("#111111");
inline const QColor BG_HOVER = QColor("#161616");

// Borders
inline const QColor BORDER_DIM = QColor("#1a1a1a");
inline const QColor BORDER_MED = QColor("#222222");
inline const QColor BORDER_BRIGHT = QColor("#333333");

// Text
inline const QColor TEXT_PRIMARY = QColor("#e5e5e5");
inline const QColor TEXT_SECONDARY = QColor("#808080");
inline const QColor TEXT_TERTIARY = QColor("#525252");
inline const QColor TEXT_DIM = QColor("#404040");

// Functional
inline const QColor COLOR_WARNING = QColor("#ca8a04");
inline const QColor COLOR_INFO = QColor("#2563eb");
inline const QColor COLOR_CYAN = QColor("#0891b2");

// Row alternation
inline const QColor ROW_ALT = QColor("#0c0c0c");

} // namespace fincept::screens::crypto
