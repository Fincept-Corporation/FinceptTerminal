#pragma once
// Surface Analytics — single home for legitimate constexpr defaults used
// to seed control-panel state on first run / reset. Anything here is a
// fall-back, never an authoritative value baked into business logic.

#include <QString>
#include <QStringList>

#include <array>

namespace fincept::surface::defaults {

// Starter equity options underlyings — used to seed the asset search field
// when the user first opens an OPRA-backed surface.
inline constexpr std::array<const char*, 7> EQUITY_UNDERLYINGS = {"SPY", "QQQ", "IWM", "AAPL", "TSLA", "NVDA", "AMZN"};

// Default correlation / PCA / risk basket — user can edit and persist their own
// via SurfaceControlPanel state.
inline constexpr std::array<const char*, 8> RISK_BASKET = {"SPY", "QQQ", "IWM", "DIA", "GLD", "TLT", "IEF", "HYG"};

// Default commodity roots used for futures-curve surfaces. The control panel
// shows these in the asset combo when the active surface is commodity-backed.
inline constexpr std::array<const char*, 5> COMMODITY_ROOTS = {"CL", "NG", "GC", "ZC", "ZS"};

// Default historical lookback (days) for equities OHLCV-driven surfaces.
inline constexpr int LOOKBACK_DAYS = 60;

// Default strike window for OPRA option-surface fetches, % of spot.
inline constexpr int STRIKE_WINDOW_PCT = 20;

// Default DTE filter for option surfaces (days-to-expiry).
inline constexpr int DTE_MIN = 7;
inline constexpr int DTE_MAX = 365;

} // namespace fincept::surface::defaults

namespace fincept::surface {

// ── US option underlyings (the free delayed chain and Databento's OPRA feed) ──

/// The plain US ticker an option chain is listed under: Yahoo's index caret dropped
/// ("^SPX" -> "SPX") and Yahoo's own index tickers mapped to their option root
/// ("^GSPC" -> "SPX", "^DJI" -> "DJX"); stock and ETF tickers pass through upper-cased.
inline QString surface_us_chain_symbol(const QString& symbol) {
    QString s = symbol.trimmed().toUpper();
    if (!s.startsWith(QLatin1Char('^')))
        return s;
    s = s.mid(1);
    if (s == QLatin1String("GSPC"))
        return QStringLiteral("SPX");
    if (s == QLatin1String("DJI"))
        return QStringLiteral("DJX");
    return s;
}

/// True for a US index option root ("SPX", "NDX", "DJX" …) or a Yahoo index ticker.
inline bool surface_is_us_index_root(const QString& symbol) {
    const QString s = symbol.trimmed().toUpper();
    if (s.startsWith(QLatin1Char('^')))
        return true;
    static const QStringList kRoots = {QStringLiteral("SPX"), QStringLiteral("XSP"),  QStringLiteral("NDX"),
                                       QStringLiteral("RUT"), QStringLiteral("MRUT"), QStringLiteral("VIX"),
                                       QStringLiteral("DJX"), QStringLiteral("OEX"),  QStringLiteral("XEO")};
    return kRoots.contains(s);
}

/// The market-data (Yahoo) symbol of an underlying's spot quote: an index root is
/// quoted under its caret ticker ("SPX" -> "^SPX"); everything else as is. (DJX gives
/// "^DJX", never ^DJI: the Dow is 100 times the DJX level.)
inline QString surface_quote_symbol(const QString& symbol) {
    const QString s = surface_us_chain_symbol(symbol);
    if (!surface_is_us_index_root(s))
        return symbol.trimmed().toUpper();
    return QLatin1Char('^') + s;
}

/// The symbol another screen reads for this underlying (symbol links): the Yahoo
/// index for an index root ("SPX" -> "^SPX", "DJX" -> "^DJI"), the ticker otherwise.
inline QString surface_link_symbol(const QString& symbol) {
    const QString s = surface_us_chain_symbol(symbol);
    return s == QLatin1String("DJX") ? QStringLiteral("^DJI") : surface_quote_symbol(s);
}

/// Text that can be tried as a ticker as-is (after surface_us_chain_symbol): 1-12
/// ASCII letters / digits / . - /, a letter first.
inline bool surface_is_us_ticker_text(const QString& s) {
    if (s.isEmpty() || s.size() > 12 || !(s.at(0).unicode() < 128 && s.at(0).isLetter()))
        return false;
    for (const QChar c : s) {
        const bool ok = (c.unicode() < 128 && c.isLetterOrNumber()) || c == QLatin1Char('.') ||
                        c == QLatin1Char('-') || c == QLatin1Char('/');
        if (!ok)
            return false;
    }
    return true;
}

} // namespace fincept::surface
