#pragma once
// crypto_internal.h — private shared constants for crypto_trading translation units.
// Include ONLY from crypto_trading/*.cpp files.

namespace fincept::crypto {

static constexpr const char* TAG = "CryptoTrading";

// Named constants (ES.45: avoid magic numbers)
static constexpr int    OHLCV_FETCH_COUNT             = 200;
static constexpr double DEFAULT_PAPER_BALANCE          = 100000.0;
static constexpr int    PORTFOLIO_CLEANUP_THRESH       = 10;
static constexpr float  TICKER_POLL_INTERVAL           = 3.0f;
static constexpr float  ORDERBOOK_POLL_INTERVAL        = 3.0f;
static constexpr float  WATCHLIST_POLL_INTERVAL        = 15.0f;
static constexpr float  PORTFOLIO_REFRESH_INTERVAL     = 1.5f;
static constexpr float  MARKET_INFO_INTERVAL           = 30.0f;
static constexpr int    PRICE_FEED_POLL_SEC            = 3;
static constexpr int    WS_MAX_WATCHLIST_SYMBOLS       = 10;
static constexpr int    MAX_CANDLE_BUFFER              = 500;
static constexpr int    PORTFOLIO_CLEANUP_TRADE_LIMIT  = 1;

// Order book analytics constants
static constexpr double OB_IMBALANCE_BUY_THRESHOLD  =  0.30;
static constexpr double OB_IMBALANCE_SELL_THRESHOLD = -0.30;
static constexpr double OB_RISE_BUY_THRESHOLD       =  0.15;
static constexpr double OB_RISE_SELL_THRESHOLD      = -0.15;
static constexpr int    OB_MAX_TICK_HISTORY         = 3600;
static constexpr int    OB_TICK_CAPTURE_MS          = 1000;
static constexpr int    OB_MAX_OB_DISPLAY_LEVELS    = 12;

} // namespace fincept::crypto
