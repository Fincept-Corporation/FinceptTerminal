#pragma once
// Equity Trading types — UI state, enums, constants
// Kept separate from trading engine types to avoid coupling.

#include "core/market/MarketSessions.h"
#include "screens/equity_trading/EquitySymbolMap.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QColor>
#include <QFontMetrics>
#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QVector>

#include <algorithm>
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

// ── Appearance helpers for the tables ──────────────────────────────────────

/// Row height for a table whose text is `font_offset` px from the user's base
/// font size: `min_px` at the default size, growing with larger fonts.
inline int table_row_height(int font_offset, int min_px) {
    return std::max(min_px, QFontMetrics(fincept::ui::fonts::font(font_offset)).height() + 8);
}

/// QTableWidgetItem brushes keep the colour they were given — the central
/// re-style pass only reaches stylesheets. Call on ThemeManager::theme_changed:
/// every item foreground / background that is a colour token of either preset
/// (at any alpha) becomes the same token of the active preset; any other
/// colour is left as it is.
inline void retheme_items(QTableWidget* table) {
    if (!table)
        return;
    using Field = const char* fincept::ui::ThemeTokens::*;
    using fincept::ui::ThemeTokens;
    static constexpr Field kFields[] = {
        &ThemeTokens::bg_base,        &ThemeTokens::bg_surface,    &ThemeTokens::bg_raised,
        &ThemeTokens::bg_hover,       &ThemeTokens::border_dim,    &ThemeTokens::border_med,
        &ThemeTokens::border_bright,  &ThemeTokens::text_primary,  &ThemeTokens::text_secondary,
        &ThemeTokens::text_tertiary,  &ThemeTokens::text_dim,      &ThemeTokens::accent,
        &ThemeTokens::accent_dim,     &ThemeTokens::text_on_accent, &ThemeTokens::icon_dim,
        &ThemeTokens::icon_hover,     &ThemeTokens::positive,      &ThemeTokens::positive_dim,
        &ThemeTokens::negative,       &ThemeTokens::negative_dim,  &ThemeTokens::warning,
        &ThemeTokens::info,           &ThemeTokens::cyan,          &ThemeTokens::accent_bg,
        &ThemeTokens::positive_bg,    &ThemeTokens::negative_bg,   &ThemeTokens::row_alt,
    };
    const ThemeTokens& active = fincept::ui::ThemeManager::instance().tokens();
    QHash<QRgb, QColor> map;
    for (const ThemeTokens* preset : {&fincept::ui::THEME_OBSIDIAN, &fincept::ui::THEME_DAYLIGHT}) {
        for (Field f : kFields) {
            const QRgb key = QColor(preset->*f).rgb() & 0x00ffffffu;
            if (!map.contains(key))
                map.insert(key, QColor(active.*f));
        }
    }
    const auto remap = [&map](const QBrush& brush, QColor& out) {
        if (brush.style() == Qt::NoBrush)
            return false;
        const auto it = map.constFind(brush.color().rgb() & 0x00ffffffu);
        if (it == map.constEnd())
            return false;
        out = *it;
        out.setAlpha(brush.color().alpha());
        return out != brush.color();
    };
    const bool sorting = table->isSortingEnabled(); // rows must not move while items change
    if (sorting)
        table->setSortingEnabled(false);
    for (int r = 0; r < table->rowCount(); ++r) {
        for (int c = 0; c < table->columnCount(); ++c) {
            QTableWidgetItem* item = table->item(r, c);
            if (!item)
                continue;
            QColor color;
            if (remap(item->foreground(), color))
                item->setForeground(color);
            if (remap(item->background(), color))
                item->setBackground(color);
        }
    }
    if (sorting)
        table->setSortingEnabled(true);
}

// ── Exchange → Currency mapping ────────────────────────────────────────────

/// ISO code an exchange's prices are quoted in, as the session table knows it
/// (core/market/MarketSessions: NSE / NFO / BSE INR, NYSE / NASDAQ USD, LSE GBp
/// pence, XETRA EUR, TSX CAD). `fallback` (normally the broker's account
/// currency) for a segment the table does not list (MCX, CDS, FOREX). Never a
/// symbol: money / prices are formatted by ui/formatting from the code.
inline QString exchange_currency(const QString& exchange, const QString& fallback = {}) {
    if (const auto* venue = fincept::core::market::venue_by_code(exchange.trimmed().toUpper());
        venue && !venue->currency.isEmpty())
        return venue->currency;
    return fallback;
}

// -- Cross-screen symbol hand-off ---------------------------------------------
// research_ticker_for() (broker symbol -> Yahoo-style ticker for Equity Research)
// lives in EquitySymbolMap.h, included above: it is pure, Qt-Core-only logic and is
// unit-tested there.

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
    QString currency; // ISO code of every amount below (the account's: "INR", "USD"); empty = unknown, no unit
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
    // Positions whose price currency has no rate to `currency` yet: left out of
    // holdings_value / unrealized_pnl / total_equity (never at 1:1), and said so.
    int fx_missing = 0;
};

struct EquityStatsView {
    QString currency; // ISO code of the money fields (the account's); empty = unknown, no unit

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
    int fx_missing = 0; // as EquityFundsView::fx_missing: left out of unrealized_pnl / net_pnl
};

} // namespace fincept::screens::equity
