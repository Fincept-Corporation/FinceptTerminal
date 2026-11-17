// src/screens/equity_research/EquityResearchLinks.cpp
#include "screens/equity_research/EquityResearchLinks.h"

#include "core/events/EventBus.h"
#include "storage/repositories/WatchlistRepository.h"

#include <QCoreApplication>
#include <QCursor>
#include <QMenu>
#include <QToolTip>
#include <QUrl>
#include <QVector>
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

QString regulator_filings_html(const QString& symbol) {
    const QString s = symbol.trimmed().toUpper();
    const qsizetype dot = s.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0)
        return {};
    const QString root = s.left(dot);
    const QString suffix = s.mid(dot + 1);
    struct Link {
        const char* label;
        QString url;
    };
    QVector<Link> links;
    const QString enc = QString::fromUtf8(QUrl::toPercentEncoding(root));
    if (suffix == QLatin1String("L")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "London Stock Exchange — company page & RNS news"),
                  QStringLiteral("https://www.londonstockexchange.com/search?searchtype=all&q=%1").arg(enc)},
                 {QT_TRANSLATE_NOOP("EquityResearchLinks", "FCA National Storage Mechanism (annual reports, prospectuses)"),
                  QStringLiteral("https://data.fca.org.uk/#/nsm/nationalstoragemechanism")},
                 {QT_TRANSLATE_NOOP("EquityResearchLinks", "Companies House (accounts and filings)"),
                  QStringLiteral("https://find-and-update.company-information.service.gov.uk/")}};
    } else if (suffix == QLatin1String("NS") || suffix == QLatin1String("BO")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "NSE — corporate announcements and filings"),
                  QStringLiteral("https://www.nseindia.com/get-quotes/equity?symbol=%1").arg(enc)},
                 {QT_TRANSLATE_NOOP("EquityResearchLinks", "BSE — corporate announcements"),
                  QStringLiteral("https://www.bseindia.com/corporates/ann.html")}};
    } else if (suffix == QLatin1String("T")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "EDINET (FSA) — securities reports"),
                  QStringLiteral("https://disclosure2.edinet-fsa.go.jp/")},
                 {QT_TRANSLATE_NOOP("EquityResearchLinks", "JPX — listed company search and TDnet disclosures"),
                  QStringLiteral("https://www2.jpx.co.jp/tseHpFront/JJK010010Action.do?Show=Show")}};
    } else if (suffix == QLatin1String("HK")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "HKEXnews — listed company announcements"),
                  QStringLiteral("https://www.hkexnews.hk/")}};
    } else if (suffix == QLatin1String("TO") || suffix == QLatin1String("V") || suffix == QLatin1String("CN")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "SEDAR+ — Canadian securities filings"),
                  QStringLiteral("https://www.sedarplus.ca/")}};
    } else if (suffix == QLatin1String("AX")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "ASX — company announcements"),
                  QStringLiteral("https://www.asx.com.au/markets/company/%1").arg(enc)}};
    } else if (suffix == QLatin1String("DE") || suffix == QLatin1String("F")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "Unternehmensregister — German company filings"),
                  QStringLiteral("https://www.unternehmensregister.de/")}};
    } else if (suffix == QLatin1String("PA")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "AMF — French regulated information"),
                  QStringLiteral("https://www.info-financiere.fr/")}};
    } else if (suffix == QLatin1String("AS")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "AFM — Dutch financial reporting register"),
                  QStringLiteral("https://www.afm.nl/en/sector/registers")}};
    } else if (suffix == QLatin1String("SW")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "SIX — ad hoc announcements"),
                  QStringLiteral("https://www.six-group.com/en/market-data/news-tools/official-notices.html")}};
    } else if (suffix == QLatin1String("KS") || suffix == QLatin1String("KQ")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "DART (FSS) — Korean disclosures"), QStringLiteral("https://dart.fss.or.kr/")}};
    } else if (suffix == QLatin1String("SS") || suffix == QLatin1String("SZ")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "CNINFO — China listed company disclosures"),
                  QStringLiteral("http://www.cninfo.com.cn/")}};
    } else if (suffix == QLatin1String("TW") || suffix == QLatin1String("TWO")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "MOPS — Taiwan market observation post"),
                  QStringLiteral("https://mops.twse.com.tw/")}};
    } else if (suffix == QLatin1String("SA")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "CVM — Brazilian company filings"),
                  QStringLiteral("https://www.rad.cvm.gov.br/ENET/frmConsultaExternaCVM.aspx")}};
    } else if (suffix == QLatin1String("MI")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "CONSOB — Italian issuer information"),
                  QStringLiteral("https://www.consob.it/")}};
    } else if (suffix == QLatin1String("MC")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "CNMV — Spanish issuer filings"), QStringLiteral("https://www.cnmv.es/")}};
    } else if (suffix == QLatin1String("ST") || suffix == QLatin1String("CO") || suffix == QLatin1String("HE")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "Nasdaq Nordic — company news and reports"),
                  QStringLiteral("https://www.nasdaq.com/european-market-activity/news/company-news")}};
    } else if (suffix == QLatin1String("JO")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "JSE — SENS announcements"), QStringLiteral("https://www.jse.co.za/")}};
    } else if (suffix == QLatin1String("SI")) {
        links = {{QT_TRANSLATE_NOOP("EquityResearchLinks", "SGX — company announcements"),
                  QStringLiteral("https://www.sgx.com/securities/company-announcements")}};
    }
    if (links.isEmpty())
        return {};
    QStringList items;
    for (const auto& l : links)
        items << QStringLiteral("<a href=\"%1\">%2</a>")
                     .arg(l.url.toHtmlEscaped(), QCoreApplication::translate("EquityResearchLinks", l.label).toHtmlEscaped());
    return QCoreApplication::translate("EquityResearchLinks",
                                       "%1 is listed outside the US, so SEC EDGAR has no filings for it. Its home "
                                       "regulator and exchange publish them:")
               .arg(s.toHtmlEscaped()) +
           QStringLiteral("<br>") + items.join(QStringLiteral("<br>")) + QStringLiteral("<br>") +
           QCoreApplication::translate("EquityResearchLinks",
                                       "If the company also has a US listing (an ADR), open that ticker to see its SEC "
                                       "filings here.");
}

} // namespace fincept::screens::equity_links
