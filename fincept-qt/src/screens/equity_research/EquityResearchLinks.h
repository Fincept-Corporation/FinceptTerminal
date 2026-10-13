// src/screens/equity_research/EquityResearchLinks.h
//
// Cross-linking for Equity Research: hand the active (or a peer's) symbol to the other
// screens through `nav.open_symbol`, add it to a watchlist, and map a yfinance research
// symbol (RELIANCE.NS, AAPL) to the broker form other screens and the order ticket use.
#pragma once

#include <QString>
#include <QStringList>

class QMenu;
class QWidget;

namespace fincept::screens::equity_links {

/// A research symbol resolved to a broker route.
struct TradeRoute {
    QString bare;           ///< broker symbol (exchange suffix stripped)
    QString order_exchange; ///< exchange to put on an order; empty → broker default (US)
    QStringList match_exchanges; ///< a broker must serve one of these to trade it
    bool routable = false;
};

/// yfinance suffix → broker exchange (".NS" → NSE, no suffix → US venues). Forex,
/// crypto, indices and unmapped suffixes are not routable.
TradeRoute trade_route(const QString& research_symbol);

/// Navigate to `screen_id` (constructing it if needed) and deliver the symbol through
/// IGroupLinked — the event WindowFrame handles for every frame.
void open_symbol_in(const QString& screen_id, const QString& symbol, const QString& exchange = {});

/// Append "Open in Equity Trading / News / Backtesting" and "Add to watchlist ▸" for
/// `symbol` (yfinance form) to `menu`. `context` anchors tooltips.
void add_open_in_actions(QMenu* menu, QWidget* context, const QString& symbol);

/// One "add to <watchlist>" action per watchlist, appended to `menu`; a duplicate is refused
/// with a tooltip, a success is announced on `watchlist.updated`.
void add_watchlist_entries(QMenu* menu, QWidget* context, const QString& symbol);

} // namespace fincept::screens::equity_links
