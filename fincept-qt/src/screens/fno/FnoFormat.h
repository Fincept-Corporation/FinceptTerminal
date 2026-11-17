#pragma once
// FnoFormat.h — the F&O screen's currency and number formats, shared by the
// sub-tabs so the same figure reads the same everywhere on the screen (the
// header bar showed total OI as "1.25Cr" while the Strategy Builder's chain
// summary showed "12450K"). Everything goes through the app's one formatter
// (ui/formatting), so the user's locale, grouping, symbol style and compact
// scheme (Settings > Currency & Number Format) apply here too.

#include "ui/formatting/NumberFormat.h"

#include <QString>

namespace fincept::screens::fno {

/// The currency of NSE F&O money: premiums, margins and P&L of NSE / BSE
/// derivatives, and the NSE FII / DII cash flows, are in Indian rupees — a fact
/// of the exchange, not a default (never the display currency).
inline QString fno_ccy() {
    return QStringLiteral("INR");
}

/// Source id of the free, DELAYED US listed-option chain (Cboe delayed quotes,
/// Yahoo Finance fallback — services/options/FreeOptionChainService). It needs no
/// broker and no key, so the Chain tab opens on it when nothing else is set up.
/// Read-only: nothing can be ordered from it.
inline QString fno_free_source() {
    return QStringLiteral("us_delayed");
}

/// True for the sources that quote US listed options (Databento's OPRA feed and
/// the free delayed chain): premiums in US dollars, US tickers, a US rate curve.
inline bool fno_is_us_source(const QString& broker_id) {
    return broker_id == QLatin1String("databento") || broker_id == fno_free_source();
}

/// The currency an option chain's premiums are quoted in, by its source:
/// Databento and the free delayed chain serve US listed options (US dollars);
/// every other source on this screen is an Indian broker's NSE / BSE chain (rupees).
inline QString fno_chain_ccy(const QString& broker_id) {
    return fno_is_us_source(broker_id) ? QStringLiteral("USD") : fno_ccy();
}

/// Open interest / contract counts (not money), abbreviated per the user's
/// compact scheme. On Automatic an INR screen reads lakh / crore ("1.25 Cr",
/// "3.40 L"); a user who chose K / M / B sees "12.5M".
inline QString fno_fmt_oi(qint64 v, const QString& ccy = fno_ccy()) {
    return fincept::ui::formatting::compact(double(v), fincept::ui::formatting::Compact::FromSettings, ccy);
}

/// How option / futures / underlying prices quoted in `ccy` are formatted.
inline fincept::ui::formatting::InstrumentFmt fno_price_fmt(const QString& ccy) {
    return fincept::ui::formatting::InstrumentFmt{ccy};
}

/// A price without its unit, for a column or label whose header names the unit: "1,234.50".
inline QString fno_price(double v, const QString& ccy) {
    return fincept::ui::formatting::price(v, fno_price_fmt(ccy));
}

/// An amount of money (net premium, margin, payoff bounds): "₹1,240.50". The
/// decimals follow the user's setting (Settings > Currency & Number Format,
/// `decimals` = -1) — strategy money used to be pinned to whole units here,
/// overriding that setting. Pass an explicit count only where the figure is a
/// whole multiple by construction.
inline QString fno_money(double v, const QString& ccy, int decimals = -1) {
    return fincept::ui::formatting::money(v, ccy, fincept::ui::formatting::Compact::None, decimals);
}

/// A signed amount (P&L, the bounds of a payoff): "+₹1,240.50" / "-₹820.25";
/// decimals as fno_money().
inline QString fno_signed_money(double v, const QString& ccy, int decimals = -1) {
    return fincept::ui::formatting::signed_money(v, ccy, fincept::ui::formatting::Compact::None, decimals);
}

} // namespace fincept::screens::fno
