#pragma once
// Equity Trading types — UI state, enums, constants
// Kept separate from trading engine types to avoid coupling.

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::screens::equity {

// ── Enums ──────────────────────────────────────────────────────────────────

enum class BottomTab { Positions, Holdings, Orders, Funds, Stats };

enum class TradingMode { Paper, Live };

// ── Default Watchlist (NIFTY 50 subset) ────────────────────────────────────

inline const QStringList DEFAULT_WATCHLIST = {
    "HDFCBANK", "ICICIBANK", "SBIN",     "KOTAKBANK",  "AXISBANK",
    "TCS",      "INFY",      "WIPRO",    "HCLTECH",    "RELIANCE",
    "TATAMOTORS","MARUTI",   "BAJFINANCE","HINDUNILVR", "ITC",
    "SUNPHARMA", "DRREDDY",  "LT",       "BHARTIARTL", "TITAN",
};

inline const QStringList US_WATCHLIST = {
    "AAPL", "MSFT", "GOOGL", "AMZN",  "NVDA",
    "META", "TSLA", "JPM",   "V",     "JNJ",
};

// ── Constants ──────────────────────────────────────────────────────────────

inline constexpr double DEFAULT_PAPER_BALANCE = 1000000.0; // 10 Lakh INR
inline constexpr int OHLCV_FETCH_COUNT = 200;
inline constexpr int QUOTE_POLL_MS = 5000;
inline constexpr int PORTFOLIO_POLL_MS = 3000;
inline constexpr int WATCHLIST_POLL_MS = 10000;
inline constexpr int CLOCK_UPDATE_MS = 1000;

// ── Obsidian Design System Colors (same as crypto) ─────────────────────────

inline const QColor COLOR_BUY = QColor("#16a34a");
inline const QColor COLOR_SELL = QColor("#dc2626");
inline const QColor COLOR_DIM = QColor("#808080");
inline const QColor COLOR_ACCENT = QColor("#d97706");

inline const QColor BG_BASE = QColor("#080808");
inline const QColor BG_SURFACE = QColor("#0a0a0a");
inline const QColor BG_RAISED = QColor("#111111");
inline const QColor BG_HOVER = QColor("#161616");

inline const QColor BORDER_DIM = QColor("#1a1a1a");
inline const QColor BORDER_MED = QColor("#222222");
inline const QColor BORDER_BRIGHT = QColor("#333333");

inline const QColor TEXT_PRIMARY = QColor("#e5e5e5");
inline const QColor TEXT_SECONDARY = QColor("#808080");
inline const QColor TEXT_TERTIARY = QColor("#525252");

inline const QColor COLOR_WARNING = QColor("#ca8a04");
inline const QColor COLOR_INFO = QColor("#2563eb");
inline const QColor ROW_ALT = QColor("#0c0c0c");

// ── Exchange → Currency mapping ────────────────────────────────────────────

inline QString exchange_currency(const QString& exchange) {
    if (exchange == "NSE" || exchange == "BSE" || exchange == "NFO" ||
        exchange == "MCX" || exchange == "CDS")
        return "INR";
    if (exchange == "NYSE" || exchange == "NASDAQ" || exchange == "AMEX")
        return "USD";
    if (exchange == "LSE")
        return "GBP";
    return "USD";
}

inline QString currency_symbol(const QString& currency) {
    if (currency == "INR") return QString::fromUtf8("\u20B9");
    if (currency == "GBP") return QString::fromUtf8("\u00A3");
    if (currency == "EUR") return QString::fromUtf8("\u20AC");
    if (currency == "JPY") return QString::fromUtf8("\u00A5");
    return "$";
}

} // namespace fincept::screens::equity
