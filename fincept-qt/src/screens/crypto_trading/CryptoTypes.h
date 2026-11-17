#pragma once
// Crypto Trading types — UI state, enums, constants
// Kept separate from trading engine types to avoid coupling.

#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QColor>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace fincept::screens::crypto {

// ── Price / size formatting ──────────────────────────────────────────────────
//
// A pair's price is in its QUOTE asset: BTC/USDT in USDT, ETH/BTC in BTC,
// BTC/EUR in EUR, a perpetual "BTC/USDT:USDT" in USDT. USDT is not USD: a
// stablecoin is its own currency. Every number goes through the app's one
// formatter (ui/formatting), so the user's number locale, grouping, symbol
// style and negative style apply. The formatter scales the decimals to the
// magnitude and the quote asset (a BTC- or ETH-quoted price keeps more digits),
// so a sub-cent pair never collapses to "0.00" — crypto spans ~11 orders of
// magnitude of unit price (BTC near 1e5, PEPE near 1e-8).
//
// Header-only + inline so every widget in this directory shares one
// definition without a new translation unit.

/// The quote asset of a pair: "BTC/USDT" -> "USDT", "BTC/USDT:USDT" -> "USDT",
/// "ETH/BTC" -> "BTC", "BTC-EUR" -> "EUR". Empty when the text names no quote.
inline QString pair_quote_ccy(const QString& pair) {
    QString s = pair.trimmed();
    const qsizetype colon = s.indexOf(QLatin1Char(':'));
    if (colon >= 0)
        s.truncate(colon); // a contract's ":SETTLE[-expiry…]" is not part of the quote
    qsizetype sep = s.lastIndexOf(QLatin1Char('/'));
    if (sep < 0)
        sep = s.lastIndexOf(QLatin1Char('-'));
    if (sep <= 0 || sep + 1 >= s.size())
        return {};
    return s.mid(sep + 1).toUpper();
}

/// The asset a contract settles (and reports P&L) in: "BTC/USDT:USDT" -> "USDT",
/// "BTC/USD:BTC" -> "BTC" (inverse), "BTC/USDT:USDT-250328" -> "USDT". A spot
/// pair has no settle suffix: its quote asset.
inline QString pair_settle_ccy(const QString& pair) {
    const qsizetype colon = pair.indexOf(QLatin1Char(':'));
    if (colon < 0)
        return pair_quote_ccy(pair);
    const QString settle = pair.mid(colon + 1).section(QLatin1Char('-'), 0, 0).trimmed().toUpper();
    return settle.isEmpty() ? pair_quote_ccy(pair) : settle;
}

/// How prices of `pair` are formatted: in its quote asset.
inline fincept::ui::formatting::InstrumentFmt pair_price_fmt(const QString& pair) {
    return fincept::ui::formatting::instrument_for_symbol(pair.section(QLatin1Char(':'), 0, 0), pair_quote_ccy(pair));
}

/// A price of `pair` without its unit, for a column / book / axis whose header
/// or row names the pair: "95,012.50", "0.03512", "0.00001234".
inline QString pair_price(double v, const QString& pair) {
    return fincept::ui::formatting::price(v, pair_price_fmt(pair));
}

/// A price of `pair` with its quote asset placed per the user's settings:
/// "$95,012.50" (BTC/USD), "95,012.50 USDT" / "USDT 95,012.50", "€87,410.00".
inline QString pair_price_with_unit(double v, const QString& pair) {
    return fincept::ui::formatting::price_with_symbol(v, pair_price_fmt(pair));
}

/// A price difference (spread) in the decimals the pair's price `ref_price` uses.
inline QString pair_price_diff(double diff, double ref_price, const QString& pair) {
    return fincept::ui::formatting::number(diff, fincept::ui::formatting::price_decimals(ref_price, pair_price_fmt(pair)));
}

/// Text for an editable price box: C locale ('.', no grouping) because the
/// ticket parses it back with toDouble(); decimals as the formatter shows them.
inline QString pair_price_edit_text(double v, const QString& pair) {
    return QString::number(v, 'f', fincept::ui::formatting::price_decimals(v, pair_price_fmt(pair)));
}

/// Order/position size in base units (a quantity, not money), trailing zeros
/// trimmed. Large sizes (meme-coin lots run to billions of units) don't need
/// many decimals; small ones do.
inline QString format_size(double v) {
    const double a = std::fabs(v);
    return fincept::ui::formatting::quantity(v, a >= 1e6 ? 2 : (a >= 1.0 ? 4 : 8));
}

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
    QString symbol; // the market these figures are for (prices / OI value in its quote asset)
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

// ── The market the selected pair resolves to ─────────────────────────────────
//
// What the exchange daemon's ONE resolver (exchange_client.resolve_market, also
// used by the live stream and the order path) said about the selected pair on
// the selected exchange, plus what the ticket may send there. Filled from a
// fetch_markets {"symbol": X} reply; the ticket, the confirm dialog and the
// order all use `symbol` — never the pair as typed.
struct ResolvedMarket {
    QString exchange;
    QString requested;         // the pair the screen selected
    QString symbol;            // the market shown / traded (empty: none)
    QString match;             // "exact" | "substituted" | "missing"
    bool tradable = false;     // only an exact, active market can be traded
    QString reason;            // why it is substituted / missing / not tradable
    QStringList alternatives;  // nearest markets the user can choose instead
    QString kind;              // spot | swap | future | option
    QString base, quote, settle;
    bool linear = false;
    bool inverse = false;
    double contract_size = 0.0;
    double min_base = 0.0;     // exchange minimum in base coins (0 = none / unknown)
    double min_cost = 0.0;
    bool stale = false;        // market list older than a day, exchange unreachable
    bool loaded = false;       // a reply arrived
    // What the ticket may send (LIVE). A false flag comes with its reason.
    bool orders = false;
    QString orders_reason;
    bool stop = false;
    QString stop_reason;
    bool stop_limit = false;
    QString stop_limit_reason;
    bool sl_tp = false;
    QStringList sl_tp_types;
    QString sl_tp_reason;
    bool reduce_only = false;
    bool leverage = false;
    bool margin_mode = false;
    bool margin_per_order = false;
    QString margin_scope;      // "account" (e.g. Bybit) | "market"

    bool valid() const { return !symbol.isEmpty(); }
    bool is_contract() const { return kind == QLatin1String("swap") || kind == QLatin1String("future"); }

    /// "BTC/USDT:USDT — perpetual swap, linear, settles USDT, 1 contract = 0.01 BTC"
    QString describe() const {
        auto tr = [](const char* s) { return QCoreApplication::translate("CryptoTrading", s); };
        if (symbol.isEmpty())
            return tr("no market");
        QString kind_txt = kind == QLatin1String("spot")     ? tr("spot")
                           : kind == QLatin1String("swap")   ? tr("perpetual swap")
                           : kind == QLatin1String("future") ? tr("dated future")
                           : kind == QLatin1String("option") ? tr("option")
                                                             : kind;
        QStringList parts{kind_txt};
        if (is_contract()) {
            parts << (inverse ? tr("inverse (coin-margined)") : tr("linear"));
            if (!settle.isEmpty())
                parts << tr("settles %1").arg(settle);
            if (contract_size > 0.0)
                parts << (inverse ? tr("1 contract = %1 %2").arg(QString::number(contract_size, 'g', 12), quote)
                                  : tr("1 contract = %1 %2").arg(QString::number(contract_size, 'g', 12), base));
        }
        return symbol + QStringLiteral(" — ") + parts.join(QStringLiteral(", "));
    }
};

/// Parse a fetch_markets {"symbol": X} reply (see exchange_daemon._handle_fetch_markets).
inline ResolvedMarket parse_resolved_market(const QJsonObject& reply, const QString& exchange,
                                            const QString& requested) {
    ResolvedMarket r;
    r.exchange = exchange;
    r.requested = requested;
    if (reply.contains(QStringLiteral("error")))
        return r; // not loaded: the call failed
    r.loaded = true;
    const QJsonObject res = reply.value(QStringLiteral("resolution")).toObject();
    r.match = res.value(QStringLiteral("match")).toString(QStringLiteral("missing"));
    r.reason = res.value(QStringLiteral("reason")).toString();
    for (const auto& v : res.value(QStringLiteral("alternatives")).toArray())
        r.alternatives << v.toString();
    const QJsonArray rows = reply.value(QStringLiteral("markets")).toArray();
    if (rows.isEmpty())
        return r;
    const QJsonObject row = rows.first().toObject();
    const QJsonObject m = row.value(QStringLiteral("market")).toObject();
    const QJsonObject caps = row.value(QStringLiteral("capabilities")).toObject();
    r.symbol = row.value(QStringLiteral("symbol")).toString();
    r.tradable = row.value(QStringLiteral("tradable")).toBool(false);
    r.kind = m.value(QStringLiteral("kind")).toString();
    r.base = m.value(QStringLiteral("base")).toString();
    r.quote = m.value(QStringLiteral("quote")).toString();
    r.settle = m.value(QStringLiteral("settle")).toString();
    r.linear = m.value(QStringLiteral("linear")).toBool(false);
    r.inverse = m.value(QStringLiteral("inverse")).toBool(false);
    r.contract_size = m.value(QStringLiteral("contract_size")).toDouble();
    r.min_base = row.value(QStringLiteral("minBaseAmount")).toDouble();
    r.min_cost = row.value(QStringLiteral("minCost")).toDouble();
    r.stale = row.value(QStringLiteral("markets_stale")).toBool(false);
    r.orders = caps.value(QStringLiteral("orders")).toBool(false);
    r.orders_reason = caps.value(QStringLiteral("orders_reason")).toString();
    r.stop = caps.value(QStringLiteral("stop")).toBool(false);
    r.stop_reason = caps.value(QStringLiteral("stop_reason")).toString();
    r.stop_limit = caps.value(QStringLiteral("stop_limit")).toBool(false);
    r.stop_limit_reason = caps.value(QStringLiteral("stop_limit_reason")).toString();
    r.sl_tp = caps.value(QStringLiteral("sl_tp")).toBool(false);
    for (const auto& v : caps.value(QStringLiteral("sl_tp_types")).toArray())
        r.sl_tp_types << v.toString();
    r.sl_tp_reason = caps.value(QStringLiteral("sl_tp_reason")).toString();
    r.reduce_only = caps.value(QStringLiteral("reduce_only")).toBool(false);
    r.leverage = caps.value(QStringLiteral("leverage")).toBool(false);
    r.margin_mode = caps.value(QStringLiteral("margin_mode")).toBool(false);
    r.margin_per_order = caps.value(QStringLiteral("margin_mode_per_order")).toBool(false);
    r.margin_scope = caps.value(QStringLiteral("margin_mode_scope")).toString();
    return r;
}

/// Outcome of an account-changing daemon reply (order, cancel, leverage, margin
/// mode): "accepted"; "rejected" — surely nothing was done (refused before it
/// was sent, or a definitive exchange rejection); "unknown" — it may have
/// happened: never re-send, look it up. The daemon labels its replies
/// ("outcome"); a reply it did not label (time-out in the app, encode error,
/// anything unexpected) is unknown unless its code proves nothing was sent.
inline QString account_change_outcome(const QJsonObject& r) {
    const QString outcome = r.value(QStringLiteral("outcome")).toString();
    if (!r.contains(QStringLiteral("error")))
        return outcome == QLatin1String("accepted") ? outcome : QStringLiteral("unknown");
    if (outcome == QLatin1String("rejected") || outcome == QLatin1String("unknown"))
        return outcome;
    static const QStringList kNotSent = {QStringLiteral("DAEMON_NOT_READY"), QStringLiteral("EXPIRED"),
                                         QStringLiteral("INVALID_ARGS"),     QStringLiteral("ORDER_REFUSED"),
                                         QStringLiteral("INVALID_METHOD"),   QStringLiteral("PARSE_ERROR")};
    return kNotSent.contains(r.value(QStringLiteral("code")).toString()) ? QStringLiteral("rejected")
                                                                         : QStringLiteral("unknown");
}

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

// Design System Colors — live from ThemeManager
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
inline QColor TEXT_DIM() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().text_dim);
}

inline QColor COLOR_WARNING() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().warning);
}
inline QColor COLOR_INFO() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().info);
}
inline QColor COLOR_CYAN() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().cyan);
}

inline QColor ROW_ALT() {
    return QColor(fincept::ui::ThemeManager::instance().tokens().row_alt);
}

// ── Appearance helpers for the tables ────────────────────────────────────────

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

} // namespace fincept::screens::crypto
