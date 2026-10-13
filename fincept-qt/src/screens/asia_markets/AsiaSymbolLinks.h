#pragma once
// AsiaSymbolLinks — shared by the Asia Markets and AkShare explorers, whose result tables are raw
// AkShare DataFrames. Finds the security-code column of a result, maps AkShare codes to the
// yfinance symbols the rest of the terminal uses, and builds the row "Open in…" menu.

#include "core/events/EventBus.h"
#include "storage/repositories/WatchlistRepository.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QCursor>
#include <QMenu>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QToolTip>
#include <QVariantMap>
#include <QWidget>

namespace fincept::screens::asia_links {

/// Index of the column carrying a security code ("代码", "股票代码", "symbol", ...), -1 if none.
inline int symbol_column(const QStringList& headers) {
    static const QStringList kCandidates = {
        QStringLiteral("代码"),     QStringLiteral("股票代码"), QStringLiteral("证券代码"),
        QStringLiteral("symbol"),   QStringLiteral("code"),     QStringLiteral("ticker"),
        QStringLiteral("ts_code"),  QStringLiteral("stock_code"),
    };
    for (const QString& want : kCandidates) {
        for (int i = 0; i < headers.size(); ++i) {
            if (headers[i].trimmed().compare(want, Qt::CaseInsensitive) == 0)
                return i;
        }
    }
    return -1;
}

/// Index of the column carrying the security name ("名称", "股票名称", "name", ...), -1 if none.
inline int name_column(const QStringList& headers) {
    static const QStringList kCandidates = {
        QStringLiteral("名称"),   QStringLiteral("股票名称"), QStringLiteral("证券简称"),
        QStringLiteral("股票简称"), QStringLiteral("name"),     QStringLiteral("简称"),
    };
    for (const QString& want : kCandidates) {
        for (int i = 0; i < headers.size(); ++i) {
            if (headers[i].trimmed().compare(want, Qt::CaseInsensitive) == 0)
                return i;
        }
    }
    return -1;
}

/// yfinance symbol for an AkShare security code:
///   "600519" / "sh600519" / "600519.SH" -> "600519.SS"   (Shanghai: 6xxxxx, 5xxxxx funds, 900xxx B)
///   "000001" / "sz000001"               -> "000001.SZ"   (Shenzhen: 0/3xxxxx, 1xxxxx funds, 200xxx B)
///   "430047" / "920xxx" / "bj830799"    -> "...BJ"       (Beijing)
///   "00700"                             -> "0700.HK"
///   "105.AAPL" / "106.BRK_A"            -> "AAPL" / "BRK-A" (EastMoney US quotes)
///   "AAPL"                              -> "AAPL"
/// Returns an empty string when the value is not a recognisable security code.
inline QString yf_symbol_for_code(const QString& raw) {
    QString s = raw.trimmed().toUpper();
    if (s.isEmpty())
        return {};

    auto a_share = [](const QString& code, const QString& market) -> QString {
        if (market == QLatin1String("SH") || market == QLatin1String("SS"))
            return code + QStringLiteral(".SS");
        if (market == QLatin1String("SZ"))
            return code + QStringLiteral(".SZ");
        if (market == QLatin1String("BJ"))
            return code + QStringLiteral(".BJ");
        return {};
    };

    static const QRegularExpression kPrefixed(QStringLiteral("^(SH|SZ|BJ)(\\d{6})$"));
    static const QRegularExpression kSuffixed(QStringLiteral("^(\\d{6})\\.(SH|SZ|BJ|SS)$"));
    static const QRegularExpression kSixDigits(QStringLiteral("^\\d{6}$"));
    static const QRegularExpression kHk(QStringLiteral("^\\d{4,5}$"));
    static const QRegularExpression kHkSuffixed(QStringLiteral("^(\\d{4,5})\\.HK$"));
    static const QRegularExpression kEmUs(QStringLiteral("^\\d{3}\\.([A-Z][A-Z0-9_.]*)$"));
    static const QRegularExpression kPlainTicker(QStringLiteral("^[A-Z][A-Z0-9]{0,5}([.-][A-Z])?$"));

    QRegularExpressionMatch m = kPrefixed.match(s);
    if (m.hasMatch())
        return a_share(m.captured(2), m.captured(1));
    m = kSuffixed.match(s);
    if (m.hasMatch())
        return a_share(m.captured(1), m.captured(2));
    if (kSixDigits.match(s).hasMatch()) {
        if (s.startsWith(QLatin1String("92")) || s.startsWith(QLatin1Char('4')) || s.startsWith(QLatin1Char('8')))
            return s + QStringLiteral(".BJ");
        if (s.startsWith(QLatin1Char('6')) || s.startsWith(QLatin1Char('5')) || s.startsWith(QLatin1Char('9')))
            return s + QStringLiteral(".SS");
        if (s.startsWith(QLatin1Char('0')) || s.startsWith(QLatin1Char('3')) || s.startsWith(QLatin1Char('1')) ||
            s.startsWith(QLatin1Char('2')))
            return s + QStringLiteral(".SZ");
        return {};
    }
    m = kHkSuffixed.match(s);
    if (m.hasMatch() || kHk.match(s).hasMatch()) {
        bool ok = false;
        const int n = (m.hasMatch() ? m.captured(1) : s).toInt(&ok);
        return ok && n > 0 ? QStringLiteral("%1.HK").arg(n, 4, 10, QLatin1Char('0')) : QString();
    }
    m = kEmUs.match(s);
    if (m.hasMatch())
        return m.captured(1).replace(QLatin1Char('_'), QLatin1Char('-'));
    if (kPlainTicker.match(s).hasMatch())
        return s;
    return {};
}

/// Exchange label for a yfinance symbol (SymbolRef.exchange).
inline QString exchange_for_yf(const QString& yf) {
    if (yf.endsWith(QLatin1String(".SS")))
        return QStringLiteral("SSE");
    if (yf.endsWith(QLatin1String(".SZ")))
        return QStringLiteral("SZSE");
    if (yf.endsWith(QLatin1String(".BJ")))
        return QStringLiteral("BSE");
    if (yf.endsWith(QLatin1String(".HK")))
        return QStringLiteral("HKEX");
    return {};
}

/// The AkShare-native code for a linked symbol ("600519.SS" -> "600519", "0700.HK" -> "00700"),
/// i.e. what the explorers' SYMBOL parameter expects. Empty for anything else.
inline QString native_code_for(const QString& symbol) {
    const QString yf = yf_symbol_for_code(symbol);
    if (yf.endsWith(QLatin1String(".SS")) || yf.endsWith(QLatin1String(".SZ")) || yf.endsWith(QLatin1String(".BJ")))
        return yf.left(6);
    if (yf.endsWith(QLatin1String(".HK"))) {
        bool ok = false;
        const int n = yf.left(yf.size() - 3).toInt(&ok);
        return ok ? QStringLiteral("%1").arg(n, 5, 10, QLatin1Char('0')) : QString();
    }
    return {};
}

/// Hand `symbol` to another screen. nav.open_symbol navigates first (constructing the target when
/// needed) and then delivers the ticker through IGroupLinked.
inline void open_in(const QString& screen_id, const QString& symbol) {
    if (symbol.isEmpty())
        return;
    EventBus::instance().publish(QStringLiteral("nav.open_symbol"),
                                 QVariantMap{{QStringLiteral("screen_id"), screen_id}, {QStringLiteral("symbol"), symbol}});
}

/// Appends the "Open in…" / watchlist / copy actions for one security to `menu`. `owner` is the
/// connection context; `anchor` is where confirmation tooltips are shown.
inline void add_symbol_actions(QMenu& menu, QObject* owner, QWidget* anchor, const QString& yf_symbol,
                               const QString& name) {
    QObject::connect(menu.addAction(QCoreApplication::translate("AsiaSymbolLinks", "Open in Equity Research")), &QAction::triggered, owner,
                     [yf_symbol]() { open_in(QStringLiteral("equity_research"), yf_symbol); });
    QObject::connect(menu.addAction(QCoreApplication::translate("AsiaSymbolLinks", "Open in Equity Trading")), &QAction::triggered, owner,
                     [yf_symbol]() { open_in(QStringLiteral("equity_trading"), yf_symbol); });
    // News is a text search: the company name finds more articles than a numeric exchange code.
    const QString news_query = name.trimmed().isEmpty() ? yf_symbol : name.trimmed();
    QObject::connect(menu.addAction(QCoreApplication::translate("AsiaSymbolLinks", "Open in News")), &QAction::triggered, owner,
                     [news_query]() { open_in(QStringLiteral("news"), news_query); });
    QObject::connect(menu.addAction(QCoreApplication::translate("AsiaSymbolLinks", "Backtest This Symbol")), &QAction::triggered, owner,
                     [yf_symbol]() { open_in(QStringLiteral("backtesting"), yf_symbol); });

    const auto lists = WatchlistRepository::instance().list_all();
    if (lists.is_ok() && !lists.value().isEmpty()) {
        QMenu* wl_menu = menu.addMenu(QCoreApplication::translate("AsiaSymbolLinks", "Add to Watchlist"));
        for (const auto& wl : lists.value()) {
            QObject::connect(
                wl_menu->addAction(wl.name), &QAction::triggered, owner,
                [anchor, yf_symbol, name, list_id = wl.id, list_name = wl.name]() {
                    auto& repo = WatchlistRepository::instance();
                    const auto stocks = repo.get_stocks(list_id);
                    if (stocks.is_ok()) {
                        for (const auto& s : stocks.value()) {
                            if (s.symbol.compare(yf_symbol, Qt::CaseInsensitive) == 0) {
                                QToolTip::showText(QCursor::pos(),
                                                   QCoreApplication::translate("AsiaSymbolLinks", "%1 is already in %2")
                                                       .arg(yf_symbol, list_name),
                                                   anchor);
                                return;
                            }
                        }
                    }
                    const auto r = repo.add_stock(list_id, yf_symbol, name);
                    QToolTip::showText(QCursor::pos(),
                                       r.is_ok() ? QCoreApplication::translate("AsiaSymbolLinks", "Added %1 to %2")
                                                       .arg(yf_symbol, list_name)
                                                 : QCoreApplication::translate("AsiaSymbolLinks", "Could not add %1 to %2")
                                                       .arg(yf_symbol, list_name),
                                       anchor);
                    if (r.is_ok()) // let an open Watchlist panel pick the change up
                        EventBus::instance().publish(QStringLiteral("watchlist.updated"),
                                                     QVariantMap{{QStringLiteral("action"), QStringLiteral("add")},
                                                                 {QStringLiteral("symbol"), yf_symbol}});
                });
        }
    }
    menu.addSeparator();
    QObject::connect(menu.addAction(QCoreApplication::translate("AsiaSymbolLinks", "Copy Symbol")), &QAction::triggered, owner,
                     [yf_symbol]() { QApplication::clipboard()->setText(yf_symbol); });
}

} // namespace fincept::screens::asia_links
