// src/services/algo_trading/AlgoUniverses.h
#pragma once
// AlgoUniverses — named symbol lists the scanner (and the builder's symbol
// picker) offer, for every region, not just India.
//
// Sources, in the order listed:
//   1. Your own lists, saved from the scanner ("Save list…") or edited by hand in
//      <AppPaths::data()>/algo_universes.json:
//        {"universes": [{"name": "My tech", "region": "US", "exchange": "NASDAQ",
//                        "symbols": ["AAPL", "MSFT", "NVDA"]}]}
//      `exchange` is the listing venue of BARE symbols (Yahoo-suffixed symbols
//      such as "VOD.L" need none). Full index constituent lists (S&P 500, NIFTY
//      50, …) can be imported this way.
//   2. Each market region's large caps from core/market/MarketProfile (the
//      `movers` list the markets dashboard falls back to) — Yahoo tickers with
//      their exchange suffix; a SHORT list per region, not an index.
//   3. The legacy NSE lists (largest NIFTY 50 names, Bank NIFTY constituents),
//      labelled as partial.
// GUI thread (MarketProfile names are translated).

#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services::algo {

struct AlgoUniverse {
    QString id;            ///< "user:<name>", "region:US", "nse:nifty_top20", …
    QString name;          ///< display name (translated for built-ins)
    QString region;        ///< MarketProfile region id ("US", "IN", …) or empty
    QString exchange_hint; ///< venue of bare symbols ("NSE"); empty when symbols carry suffixes
    QStringList symbols;
    QString source;        ///< where the list comes from (shown as a tooltip)
    bool user = false;
};

/// Every list: the user's first, then the current region's, the other regions'
/// and the legacy NSE lists.
QVector<AlgoUniverse> algo_universes();

/// The user lists file (<AppPaths::data()>/algo_universes.json).
QString algo_universes_path();

/// Add or replace (by name) a user list. False + `error` on an I/O failure.
bool save_user_universe(const AlgoUniverse& universe, QString* error = nullptr);

/// Delete a user list by name.
bool remove_user_universe(const QString& name, QString* error = nullptr);

} // namespace fincept::services::algo
