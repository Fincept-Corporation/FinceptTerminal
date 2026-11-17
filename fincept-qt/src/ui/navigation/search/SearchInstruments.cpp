// SearchInstruments.cpp — see SearchInstruments.h.
#include "ui/navigation/search/SearchInstruments.h"

#include "core/events/EventBus.h"
#include "core/instrument/VenueSymbols.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocale>

namespace fincept::ui::search::instruments {

namespace {

using fincept::core::instrument::AssetClass;

QString si_tr(const char* s) {
    return QCoreApplication::translate("CommandBar", s);
}

QString si_name(const Instrument& inst) {
    if (!inst.name.isEmpty())
        return inst.name;
    return inst.display.isEmpty() ? inst.symbol : inst.display;
}

QString si_code(const Instrument& inst) {
    return inst.display.isEmpty() ? inst.symbol : inst.display;
}

QString si_number(double v) {
    return QLocale().toString(v, 'g', 12);
}

bool si_equity_like(const Instrument& inst) {
    return inst.asset_class == AssetClass::Equity || inst.asset_class == AssetClass::Etf;
}

} // namespace

Route open_route(const Instrument& inst) {
    if (inst.asset_class == AssetClass::Crypto) {
        const QString pair = core::instrument::crypto_pair(inst, QStringLiteral("binance"));
        if (!pair.isEmpty())
            return {QStringLiteral("crypto_trading"), pair, QStringLiteral("crypto")};
    }
    if (inst.symbol.isEmpty())
        return {};
    // Equity Research quotes through Yahoo: stocks, ETFs, funds, indices (^GSPC), FX (EURUSD=X), futures (GC=F).
    const QString cls = core::instrument::asset_class_slug(inst.asset_class);
    return {QStringLiteral("equity_research"), inst.symbol, cls.isEmpty() ? QStringLiteral("equity") : cls};
}

QString chip(const Instrument& inst) {
    switch (inst.asset_class) {
        case AssetClass::Equity:
            return si_tr("STOCK");
        case AssetClass::Etf:
            return si_tr("ETF");
        case AssetClass::Fund:
            return si_tr("FUND");
        case AssetClass::Index:
            return si_tr("INDEX");
        case AssetClass::Fx:
            return si_tr("FX");
        case AssetClass::Crypto:
            return si_tr("CRYPTO");
        case AssetClass::Future:
            return si_tr("FUTURE");
        case AssetClass::Option:
            return si_tr("OPTION");
        case AssetClass::Bond:
            return si_tr("BOND");
        case AssetClass::Commodity:
            return si_tr("COMMOD");
        case AssetClass::Unknown:
            break;
    }
    return si_tr("SYMBOL");
}

QString facts(const Instrument& inst) {
    QStringList parts;
    const QString venue = inst.venue.isEmpty() ? inst.country : inst.venue;
    if (!venue.isEmpty())
        parts << venue;
    if (!inst.currency.isEmpty())
        parts << inst.currency;
    if (!inst.isin.isEmpty())
        parts << inst.isin;
    return parts.join(QStringLiteral(" · "));
}

QStringList action_ids(const Instrument& inst) {
    QStringList ids;
    const bool crypto = inst.asset_class == AssetClass::Crypto;
    ids << QString::fromLatin1(act::kChart);
    if (si_equity_like(inst) || crypto)
        ids << QString::fromLatin1(act::kBuy);
    ids << QString::fromLatin1(act::kWatch);
    if (si_equity_like(inst) || crypto || inst.asset_class == AssetClass::Index)
        ids << QString::fromLatin1(act::kNews);
    if (si_equity_like(inst) || crypto)
        ids << QString::fromLatin1(act::kSell);
    if (si_equity_like(inst) || inst.asset_class == AssetClass::Index)
        ids << QString::fromLatin1(act::kOptions);
    if (!inst.symbol.isEmpty())
        ids << QString::fromLatin1(act::kBacktest);
    if (si_equity_like(inst))
        ids << QString::fromLatin1(act::kResearch);
    ids << QString::fromLatin1(act::kPinHeader) << QString::fromLatin1(act::kCopy) << QString::fromLatin1(act::kNewPanel);
    return ids;
}

QString action_label(const QString& id) {
    if (id == QLatin1String(act::kOpen))
        return si_tr("Open");
    if (id == QLatin1String(act::kChart))
        return si_tr("Chart");
    if (id == QLatin1String(act::kResearch))
        return si_tr("Research");
    if (id == QLatin1String(act::kNews))
        return si_tr("News");
    if (id == QLatin1String(act::kBuy))
        return si_tr("Buy…");
    if (id == QLatin1String(act::kSell))
        return si_tr("Sell…");
    if (id == QLatin1String(act::kWatch))
        return si_tr("Watch");
    if (id == QLatin1String(act::kOptions))
        return si_tr("Options");
    if (id == QLatin1String(act::kBacktest))
        return si_tr("Backtest");
    if (id == QLatin1String(act::kPeers))
        return si_tr("Compare");
    if (id == QLatin1String(act::kPinHeader))
        return si_tr("Pin to header");
    if (id == QLatin1String(act::kCopy))
        return si_tr("Copy symbol");
    if (id == QLatin1String(act::kNewPanel))
        return si_tr("Open in new panel");
    return id;
}

QString risk(const QString& action) {
    return action == QLatin1String(act::kBuy) || action == QLatin1String(act::kSell) ? QStringLiteral("order") : QString();
}

TradeRoute trade_route(const Instrument& inst) {
    TradeRoute r;
    const QString sym = inst.symbol.trimmed().toUpper();
    r.bare = sym;
    if (sym.isEmpty() || !si_equity_like(inst))
        return r;
    struct Suffix {
        const char* suffix;
        const char* exchange;
    };
    static const Suffix kSuffixes[] = {
        {".NS", "NSE"},   {".BO", "BSE"},      {".L", "LSE"},       {".TO", "TSX"}, {".HK", "HKEX"},
        {".DE", "XETRA"}, {".PA", "EURONEXT"}, {".AS", "EURONEXT"}, {".SW", "SIX"},
    };
    for (const Suffix& s : kSuffixes) {
        const QLatin1String suffix(s.suffix);
        if (sym.endsWith(suffix)) {
            r.bare = sym.left(sym.size() - suffix.size());
            r.exchange = QString::fromLatin1(s.exchange);
            r.match_exchanges = {r.exchange};
            r.routable = true;
            return r;
        }
    }
    if (!sym.contains(QLatin1Char('.')) && !sym.startsWith(QLatin1Char('^')) && !sym.contains(QLatin1Char('='))) {
        r.match_exchanges = {QStringLiteral("NYSE"), QStringLiteral("NASDAQ"), QStringLiteral("AMEX"),
                             QStringLiteral("ARCA"), QStringLiteral("BATS"),   QStringLiteral("CBOE")};
        r.routable = true;
    }
    return r;
}

QString sentence(const Instrument& inst, const QString& action, const QVariantMap& args) {
    const QString code = si_code(inst);
    const QString name = si_name(inst);
    const Route route = open_route(inst);
    const bool crypto_screen = route.screen_id == QLatin1String("crypto_trading");
    if (action.isEmpty() || action == QLatin1String(act::kOpen) || action == QLatin1String(act::kResearch)) {
        if (route.symbol.isEmpty())
            return si_tr("No market data for this listing");
        return crypto_screen ? si_tr("Opens %1 in Crypto Trading").arg(route.symbol)
                             : si_tr("Opens %1 in Equity Research").arg(name);
    }
    if (action == QLatin1String(act::kChart))
        return crypto_screen ? si_tr("Shows the %1 chart in Crypto Trading").arg(route.symbol)
                             : si_tr("Shows the %1 chart in Equity Research").arg(code);
    if (action == QLatin1String(act::kNews))
        return si_tr("Shows news about %1").arg(code);
    if (action == QLatin1String(act::kBuy) || action == QLatin1String(act::kSell)) {
        const bool buy = action == QLatin1String(act::kBuy);
        QString what = buy ? si_tr("Opens a BUY ticket for %1").arg(code) : si_tr("Opens a SELL ticket for %1").arg(code);
        const double qty = args.value(QStringLiteral("quantity")).toDouble();
        const double limit = args.value(QStringLiteral("limit_price")).toDouble();
        if (qty > 0)
            what += si_tr(", quantity %1").arg(si_number(qty));
        if (limit > 0)
            what += si_tr(", limit %1").arg(si_number(limit));
        else if (args.value(QStringLiteral("order_type")).toString() == QLatin1String("market"))
            what += si_tr(", market");
        return what + si_tr(" — nothing is sent");
    }
    if (action == QLatin1String(act::kWatch)) {
        const QString list = args.value(QStringLiteral("watchlist")).toString();
        return list.isEmpty() ? si_tr("Adds %1 to your watchlist").arg(code) : si_tr("Adds %1 to the %2 watchlist").arg(code, list);
    }
    if (action == QLatin1String(act::kOptions))
        return si_tr("Opens the %1 option chain in Derivatives").arg(code);
    if (action == QLatin1String(act::kBacktest)) {
        const QString strategy = args.value(QStringLiteral("strategy")).toString();
        return strategy.isEmpty() ? si_tr("Opens Backtesting with %1").arg(code)
                                  : si_tr("Opens Backtesting with %1 (strategy: %2)").arg(code, strategy);
    }
    if (action == QLatin1String(act::kPeers)) {
        const QStringList peers = args.value(QStringLiteral("peers")).toStringList();
        return si_tr("Compares %1 with %2 in Equity Research").arg(code, peers.join(QStringLiteral(", ")));
    }
    if (action == QLatin1String(act::kPinHeader))
        return si_tr("Pins %1 to the header strip").arg(code);
    if (action == QLatin1String(act::kCopy))
        return si_tr("Copies %1 to the clipboard").arg(route.symbol.isEmpty() ? code : route.symbol);
    if (action == QLatin1String(act::kNewPanel))
        return si_tr("Opens %1 beside the current panel").arg(code);
    return {};
}

QString run(const actions::Ctx& ctx_in, const Instrument& inst, const QString& action, const QVariantMap& args) {
    actions::Ctx ctx = ctx_in;
    const Route route = open_route(inst);
    const QString exchange = inst.venue;
    if (action == QLatin1String(act::kNewPanel)) {
        ctx.new_panel = true;
        actions::open_symbol(ctx, route.screen_id, route.symbol, route.asset_class, exchange);
        return {};
    }
    if (action.isEmpty() || action == QLatin1String(act::kOpen) || action == QLatin1String(act::kResearch)) {
        if (route.symbol.isEmpty())
            return si_tr("No market data for this listing");
        actions::open_symbol(ctx, route.screen_id, route.symbol, route.asset_class, exchange);
        return {};
    }
    if (action == QLatin1String(act::kChart)) {
        if (route.symbol.isEmpty())
            return si_tr("No market data for this listing");
        actions::open_symbol(ctx, route.screen_id, route.symbol, route.asset_class, exchange);
        if (route.screen_id == QLatin1String("equity_research"))
            actions::open_screen(ctx, route.screen_id, QStringLiteral("overview"));
        return {};
    }
    if (action == QLatin1String(act::kNews)) {
        actions::open_symbol(ctx, QStringLiteral("news"), inst.symbol.isEmpty() ? si_code(inst) : inst.symbol,
                             route.asset_class, exchange);
        return {};
    }
    if (action == QLatin1String(act::kBuy) || action == QLatin1String(act::kSell)) {
        actions::Ticket t;
        t.is_buy = action == QLatin1String(act::kBuy);
        t.quantity = args.value(QStringLiteral("quantity")).toDouble();
        t.limit_price = args.value(QStringLiteral("limit_price")).toDouble();
        t.order_type = args.value(QStringLiteral("order_type")).toString();
        if (inst.asset_class == AssetClass::Crypto) {
            t.crypto = true;
            t.symbol = route.symbol;
            if (t.symbol.isEmpty())
                return si_tr("No tradeable pair for %1").arg(si_code(inst));
        } else {
            const TradeRoute tr = trade_route(inst);
            if (!tr.routable)
                return si_tr("No broker route for %1").arg(si_code(inst));
            t.symbol = tr.bare;
            t.exchange = tr.exchange;
            t.match_exchanges = tr.match_exchanges;
        }
        actions::open_ticket(ctx, t);
        return {};
    }
    if (action == QLatin1String(act::kWatch)) {
        const auto out = actions::add_to_watchlist(inst.symbol.isEmpty() ? si_code(inst) : inst.symbol, inst.name,
                                                   args.value(QStringLiteral("watchlist")).toString(),
                                                   /*create_missing=*/args.value(QStringLiteral("create_watchlist")).toBool());
        return out.message.isEmpty() ? si_tr("Could not add to the watchlist") : out.message;
    }
    if (action == QLatin1String(act::kOptions)) {
        actions::open_symbol(ctx, QStringLiteral("derivatives"), inst.symbol, route.asset_class, exchange);
        actions::open_screen(ctx, QStringLiteral("derivatives"), QStringLiteral("chain"));
        return {};
    }
    if (action == QLatin1String(act::kBacktest)) {
        actions::open_symbol(ctx, QStringLiteral("backtesting"), inst.symbol, route.asset_class, exchange);
        const QString strategy = args.value(QStringLiteral("strategy")).toString();
        if (!strategy.isEmpty())
            actions::open_screen(ctx, QStringLiteral("backtesting"), QString(), QStringLiteral("strategy:") + strategy);
        return {};
    }
    if (action == QLatin1String(act::kPeers)) {
        actions::open_symbol(ctx, QStringLiteral("equity_research"), inst.symbol, route.asset_class, exchange);
        const QStringList peers = args.value(QStringLiteral("peers")).toStringList();
        if (!peers.isEmpty())
            EventBus::instance().publish(QStringLiteral("equity_research.set_peers"),
                                         {{QStringLiteral("peers"), peers.join(QLatin1Char(','))}});
        return {};
    }
    if (action == QLatin1String(act::kPinHeader)) {
        actions::pin_symbol(route.symbol.isEmpty() ? si_code(inst) : route.symbol, route.asset_class, exchange);
        return si_tr("Pinned %1 to the header").arg(si_code(inst));
    }
    if (action == QLatin1String(act::kCopy)) {
        if (QClipboard* cb = QGuiApplication::clipboard())
            cb->setText(route.symbol.isEmpty() ? si_code(inst) : route.symbol);
        return si_tr("Copied %1").arg(route.symbol.isEmpty() ? si_code(inst) : route.symbol);
    }
    return {};
}

} // namespace fincept::ui::search::instruments
