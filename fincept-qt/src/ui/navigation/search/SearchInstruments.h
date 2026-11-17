#pragma once
// SearchInstruments — the INSTRUMENTS group: where a symbol opens (Equity Research for
// stocks / ETFs / indices / FX / futures in their Yahoo form, Crypto Trading for a
// crypto pair, Economics for an indicator — the old bar's routes), the row's actions
// (Chart, Research, News, Buy / Sell ticket, Add to watchlist, Derivatives, Backtest,
// Pin, Copy), the plain sentence of what Enter does, and running an action.

#include "core/instrument/Instrument.h"
#include "ui/navigation/search/SearchActions.h"

#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace fincept::ui::search::instruments {

using fincept::core::instrument::Instrument;

/// Action ids (stable; the Tab menu and hover buttons use them).
namespace act {
inline constexpr char kOpen[] = "open";
inline constexpr char kChart[] = "chart";
inline constexpr char kResearch[] = "research";
inline constexpr char kNews[] = "news";
inline constexpr char kBuy[] = "buy";
inline constexpr char kSell[] = "sell";
inline constexpr char kWatch[] = "watch";
inline constexpr char kOptions[] = "options";
inline constexpr char kBacktest[] = "backtest";
inline constexpr char kPeers[] = "peers";
inline constexpr char kPinHeader[] = "pin_header";
inline constexpr char kCopy[] = "copy";
inline constexpr char kNewPanel[] = "new_panel";
} // namespace act

struct Route {
    QString screen_id;
    QString symbol;      ///< what that screen is given
    QString asset_class; ///< SymbolRef asset class ("equity", "crypto" …)
};
/// Where "Open" goes. Empty symbol = nothing to open (a listing without market data).
Route open_route(const Instrument& inst);

/// Type chip: STOCK, ETF, FUND, INDEX, FX, CRYPTO, FUTURE, OPTION, BOND (translated).
QString chip(const Instrument& inst);
/// "NASDAQ · USD · Equity" — exchange, currency, class (any of them may be missing).
QString facts(const Instrument& inst);
/// Every action this instrument offers, default first-ish (open is implicit and not listed).
QStringList action_ids(const Instrument& inst);
QString action_label(const QString& id);
/// What Enter does for `action` ("Opens Apple Inc. in Equity Research"), with phrase args (quantity, limit …).
QString sentence(const Instrument& inst, const QString& action, const QVariantMap& args);
/// Agent-risk kind of an action ("order" for tickets; "" otherwise).
QString risk(const QString& action);

/// Equity ticket route from the Yahoo symbol (RELIANCE.NS -> RELIANCE on NSE; a bare US code -> US venues).
struct TradeRoute {
    QString bare;
    QString exchange;
    QStringList match_exchanges;
    bool routable = false;
};
TradeRoute trade_route(const Instrument& inst);

/// Runs `action` for `inst`. `args` carries phrase values (quantity, limit_price, order_type, peers, watchlist,
/// strategy). Returns a status sentence for the popup's status line ("" = nothing to say; the popup closes).
QString run(const actions::Ctx& ctx, const Instrument& inst, const QString& action, const QVariantMap& args);

} // namespace fincept::ui::search::instruments
