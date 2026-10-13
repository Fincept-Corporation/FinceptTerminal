// src/screens/equity_research/EquityResearchLinks.cpp
#include "screens/equity_research/EquityResearchLinks.h"

#include "core/events/EventBus.h"
#include "storage/repositories/WatchlistRepository.h"

#include <QCoreApplication>
#include <QCursor>
#include <QMenu>
#include <QToolTip>
#include <QWidget>

namespace fincept::screens::equity_links {

TradeRoute trade_route(const QString& research_symbol) {
    TradeRoute r;
    const QString sym = research_symbol.trimmed().toUpper();
    r.bare = sym;
    if (sym.isEmpty())
        return r;
    // Single-venue yfinance suffixes → the exchange name brokers use.
    struct Suffix {
        const char* suffix;
        const char* exchange;
    };
    static const Suffix kSuffixes[] = {
        {".NS", "NSE"},   {".BO", "BSE"},      {".L", "LSE"},       {".TO", "TSX"}, {".HK", "HKEX"},
        {".DE", "XETRA"}, {".PA", "EURONEXT"}, {".AS", "EURONEXT"}, {".SW", "SIX"},
    };
    for (const auto& s : kSuffixes) {
        const QString suffix = QLatin1String(s.suffix);
        if (sym.endsWith(suffix)) {
            r.bare = sym.left(sym.length() - suffix.length());
            r.order_exchange = QLatin1String(s.exchange);
            r.match_exchanges = {r.order_exchange};
            r.routable = true;
            return r;
        }
    }
    // No suffix → a US listing (yfinance convention). Indices (^), FX/futures (=) and crypto
    // pairs (BTC-USD) are not equities a stock broker can route.
    if (!sym.contains(QLatin1Char('.')) && !sym.startsWith(QLatin1Char('^')) && !sym.contains(QLatin1Char('=')) &&
        !sym.endsWith(QLatin1String("-USD")) && !sym.endsWith(QLatin1String("-USDT"))) {
        r.match_exchanges = {QStringLiteral("NYSE"), QStringLiteral("NASDAQ"), QStringLiteral("AMEX"),
                             QStringLiteral("ARCA"), QStringLiteral("BATS"),   QStringLiteral("CBOE")};
        r.routable = true;
    }
    return r;
}

void open_symbol_in(const QString& screen_id, const QString& symbol, const QString& exchange) {
    if (screen_id.isEmpty() || symbol.trimmed().isEmpty())
        return;
    QVariantMap payload{{"screen_id", screen_id}, {"symbol", symbol.trimmed()}};
    if (!exchange.isEmpty())
        payload.insert(QStringLiteral("exchange"), exchange);
    EventBus::instance().publish(QStringLiteral("nav.open_symbol"), payload);
}

void add_open_in_actions(QMenu* menu, QWidget* context, const QString& symbol) {
    const QString sym = symbol.trimmed().toUpper();
    if (!menu || sym.isEmpty())
        return;
    const TradeRoute route = trade_route(sym);
    auto* trade = menu->addAction(QCoreApplication::translate("EquityResearchLinks", "Open in Equity Trading (chart + order ticket)"));
    trade->setEnabled(route.routable);
    if (!route.routable)
        trade->setToolTip(QCoreApplication::translate("EquityResearchLinks", "No broker route for this market"));
    QObject::connect(trade, &QAction::triggered, menu,
                     [route]() { open_symbol_in(QStringLiteral("equity_trading"), route.bare, route.order_exchange); });
    // News searches headlines by ticker text — "RELIANCE", not "RELIANCE.NS".
    QObject::connect(menu->addAction(QCoreApplication::translate("EquityResearchLinks", "Open in News")), &QAction::triggered, menu,
                     [route]() { open_symbol_in(QStringLiteral("news"), route.bare, route.order_exchange); });
    // Backtesting runs on yfinance history, so it takes the research form as-is.
    QObject::connect(menu->addAction(QCoreApplication::translate("EquityResearchLinks", "Open in Backtesting")), &QAction::triggered, menu,
                     [sym]() { open_symbol_in(QStringLiteral("backtesting"), sym); });
    add_watchlist_entries(
        menu->addMenu(QCoreApplication::translate("EquityResearchLinks", "Add %1 to watchlist").arg(sym)), context, sym);
}

void add_watchlist_entries(QMenu* menu, QWidget* context, const QString& symbol) {
    const QString sym = symbol.trimmed().toUpper();
    if (!menu || sym.isEmpty())
        return;
    QMenu* sub = menu;
    const auto lists = fincept::WatchlistRepository::instance().list_all();
    if (lists.is_err() || lists.value().isEmpty()) {
        sub->addAction(QCoreApplication::translate("EquityResearchLinks", "No watchlists yet — create one in the Watchlist screen"))->setEnabled(false);
        return;
    }
    for (const auto& wl : lists.value()) {
        QObject::connect(sub->addAction(wl.name), &QAction::triggered, sub,
                         [context, sym, list_id = wl.id, list_name = wl.name]() {
                             auto& repo = fincept::WatchlistRepository::instance();
                             const auto stocks = repo.get_stocks(list_id);
                             if (stocks.is_ok()) {
                                 for (const auto& s : stocks.value()) {
                                     if (s.symbol.compare(sym, Qt::CaseInsensitive) == 0) {
                                         QToolTip::showText(QCursor::pos(),
                                                            QCoreApplication::translate("EquityResearchLinks", "%1 is already in %2").arg(sym, list_name), context);
                                         return;
                                     }
                                 }
                             }
                             const auto r = repo.add_stock(list_id, sym);
                             QToolTip::showText(QCursor::pos(),
                                                r.is_ok() ? QCoreApplication::translate("EquityResearchLinks", "Added %1 to %2").arg(sym, list_name)
                                                          : QCoreApplication::translate("EquityResearchLinks", "Could not add %1 to %2").arg(sym, list_name),
                                                context);
                             if (r.is_ok()) // let an open Watchlist panel pick the change up
                                 EventBus::instance().publish("watchlist.updated",
                                                              {{"action", QStringLiteral("add")}, {"symbol", sym}});
                         });
    }
}

} // namespace fincept::screens::equity_links
