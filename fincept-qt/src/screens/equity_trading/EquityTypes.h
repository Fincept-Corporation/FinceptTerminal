#pragma once
// Equity Trading types — UI state, enums, constants
// Kept separate from trading engine types to avoid coupling.

#include "ui/theme/ThemeManager.h"

#include <QColor>
#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cmath>

namespace fincept::screens::equity {

// ── Enums ──────────────────────────────────────────────────────────────────

enum class BottomTab { Positions, Holdings, Orders, Funds, Stats };

enum class TradingMode { Paper, Live };

// ── Default Watchlist (NIFTY 50 subset) ────────────────────────────────────

inline const QStringList DEFAULT_WATCHLIST = {
    "HDFCBANK", "ICICIBANK", "SBIN",     "KOTAKBANK", "AXISBANK",   "TCS",        "INFY",
    "WIPRO",    "HCLTECH",   "RELIANCE", "TATASTEEL", "MARUTI",     "BAJFINANCE", "HINDUNILVR",
    "ITC",      "SUNPHARMA", "DRREDDY",  "LT",        "BHARTIARTL", "TITAN",
};

inline const QStringList US_WATCHLIST = {
    "AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA", "JPM", "V", "JNJ",
};

// Full NIFTY 50 constituents (NSE) — seeded as the default watchlist for Indian
// brokers (profile region == "IN"). Plain symbols; each broker's resolver maps them.
inline const QStringList NIFTY50_WATCHLIST = {
    "ADANIENT",   "ADANIPORTS", "APOLLOHOSP", "ASIANPAINT", "AXISBANK",   "BAJAJ-AUTO", "BAJAJFINSV",
    "BAJFINANCE", "BEL",        "BHARTIARTL", "CIPLA",      "COALINDIA",  "DIVISLAB",   "DRREDDY",
    "EICHERMOT",  "GRASIM",     "HCLTECH",    "HDFCBANK",   "HDFCLIFE",   "HEROMOTOCO", "HINDALCO",
    "HINDUNILVR", "ICICIBANK",  "INDUSINDBK", "INFY",       "ITC",        "JIOFIN",     "JSWSTEEL",
    "KOTAKBANK",  "LT",         "M&M",        "MARUTI",     "NESTLEIND",  "NTPC",       "ONGC",
    "POWERGRID",  "RELIANCE",   "SBILIFE",    "SBIN",       "SHRIRAMFIN", "SUNPHARMA",  "TATACONSUM",
    "TATASTEEL",  "TCS",        "TECHM",      "TITAN",      "TRENT",      "ULTRACEMCO", "WIPRO",
};

// ── Constants ──────────────────────────────────────────────────────────────

inline constexpr double DEFAULT_PAPER_BALANCE = 1000000.0; // 10 Lakh INR
inline constexpr int OHLCV_FETCH_COUNT = 200;
inline constexpr int QUOTE_POLL_MS = 5000;
inline constexpr int PORTFOLIO_POLL_MS = 3000;
inline constexpr int WATCHLIST_POLL_MS = 10000;
inline constexpr int CLOCK_UPDATE_MS = 1000;

// ── Design System Colors — live from ThemeManager ──────────────────────────
// These are inline functions so they reflect the active theme at call time.

inline QColor COLOR_BUY() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().positive);
}
inline QColor COLOR_SELL() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().negative);
}
inline QColor COLOR_DIM() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().text_secondary);
}
inline QColor COLOR_ACCENT() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().accent);
}

inline QColor BG_BASE() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().bg_base);
}
inline QColor BG_SURFACE() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().bg_surface);
}
inline QColor BG_RAISED() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().bg_raised);
}
inline QColor BG_HOVER() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().bg_hover);
}

inline QColor BORDER_DIM() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().border_dim);
}
inline QColor BORDER_MED() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().border_med);
}
inline QColor BORDER_BRIGHT() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().border_bright);
}

inline QColor TEXT_PRIMARY() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().text_primary);
}
inline QColor TEXT_SECONDARY() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().text_secondary);
}
inline QColor TEXT_TERTIARY() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().text_tertiary);
}

inline QColor COLOR_WARNING() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().warning);
}
inline QColor COLOR_INFO() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().info);
}
inline QColor ROW_ALT() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().row_alt);
}

// ── Exchange → Currency mapping ────────────────────────────────────────────

inline QString exchange_currency(const QString& exchange) {
    if (exchange == "NSE" || exchange == "BSE" || exchange == "NFO" || exchange == "MCX" || exchange == "CDS")
        return "INR";
    if (exchange == "NYSE" || exchange == "NASDAQ" || exchange == "AMEX" || exchange == "CBOE")
        return "USD";
    if (exchange == "LSE")
        return "GBP";
    if (exchange == "XETRA" || exchange == "EURONEXT")
        return "EUR";
    return "USD";
}

inline QString currency_symbol(const QString& currency) {
    if (currency == "INR")
        return QString::fromUtf8("\u20B9");
    if (currency == "GBP")
        return QString::fromUtf8("\u00A3");
    if (currency == "EUR")
        return QString::fromUtf8("\u20AC");
    if (currency == "JPY")
        return QString::fromUtf8("\u00A5");
    return "$";
}

// -- Cross-screen symbol hand-off ---------------------------------------------

// Yahoo-style ticker for a broker symbol \u2014 what Equity Research (yfinance) expects:
// "RELIANCE" on NSE -> "RELIANCE.NS". Empty when the instrument has no equity
// counterpart (option / future / commodity contracts), so callers hide the action.
inline QString research_ticker_for(const QString& symbol, const QString& exchange) {
    static const QHash<QString, QString> kSuffix = {
        {"NSE", ".NS"}, {"BSE", ".BO"}, {"HKEX", ".HK"}, {"TSE", ".T"},   {"KRX", ".KS"}, {"SGX", ".SI"},
        {"ASX", ".AX"}, {"LSE", ".L"},  {"TSX", ".TO"},  {"XETR", ".DE"}, {"SIX", ".SW"},
    };
    static const QStringList kNoEquity = {"NFO", "BFO", "MCX", "CDS", "BCD", "NCDEX"};
    // Contract names end in strike+CE/PE or FUT (e.g. NIFTY2660923200CE, RELIANCE26JULFUT).
    static const QRegularExpression kDerivativeTail(QStringLiteral("(\\d(CE|PE)|FUT)$"));

    const QString sym = symbol.trimmed().toUpper();
    const QString ex = exchange.trimmed().toUpper();
    if (sym.isEmpty() || kNoEquity.contains(ex) || kDerivativeTail.match(sym).hasMatch())
        return {};
    const auto it = kSuffix.constFind(ex);
    if (it == kSuffix.constEnd() || sym.contains(QLatin1Char('.')))
        return sym;
    return sym + it.value();
}

// Order / position quantity for display: whole numbers without a decimal point, fractional
// shares (Alpaca) with up to 6 decimals and no trailing zeros. QString::number(q, 'f', 0)
// rounded 2.35 shares to "2" (and a confirm dialog showed 0.5 as "1"), while arg(double)
// printed 1,000,000 as "1e+06".
inline QString format_quantity(double qty) {
    if (!std::isfinite(qty))
        return QStringLiteral("--");
    QString s = QString::number(qty, 'f', 6);
    while (s.endsWith(QLatin1Char('0')))
        s.chop(1);
    if (s.endsWith(QLatin1Char('.')))
        s.chop(1);
    return (s.isEmpty() || s == QLatin1String("-0")) ? QStringLiteral("0") : s;
}

// \u2500\u2500 Funds / Stats view-models
// \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
// The bottom-panel Funds and Stats tabs render from these view-models, so the
// panel is decoupled from the data source: live mode fills them from BrokerFunds
// (the subset it knows), paper mode fills the full set from the paper engine.

struct EquityFundsView {
    QString currency = QStringLiteral("\u20B9"); // symbol, e.g. \u20B9 / $
    bool is_paper = false;

    double available = 0.0;       // free cash / available margin
    double used_margin = 0.0;     // locked by open intraday positions + pending orders
    double total_equity = 0.0;    // net worth = available + used + holdings + open P&L
    double opening_balance = 0.0; // initial / start-of-day balance
    double holdings_value = 0.0;  // current value of CNC delivery holdings
    double realized_pnl = 0.0;    // realized P&L (today)
    double unrealized_pnl = 0.0;  // open positions + holdings mark-to-market
    double collateral = 0.0;      // pledged collateral (live brokers)
    double margin_util_pct = 0.0; // used / (used + available) * 100
};

struct EquityStatsView {
    QString currency = QStringLiteral("\u20B9");

    double net_pnl = 0.0;   // realized + unrealized
    double today_pnl = 0.0; // realized today
    double realized_pnl = 0.0;
    double unrealized_pnl = 0.0;
    double return_pct = 0.0; // net_pnl / opening_balance * 100
    double win_rate = 0.0;   // 0..1
    long long total_trades = 0;
    long long winning_trades = 0;
    long long losing_trades = 0;
    double profit_factor = 0.0;
    double avg_win = 0.0;
    double avg_loss = 0.0;
    double largest_win = 0.0;
    double largest_loss = 0.0;
    double total_fees = 0.0;
    double turnover = 0.0;
};

} // namespace fincept::screens::equity
