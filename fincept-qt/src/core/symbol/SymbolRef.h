#pragma once
#include <QJsonObject>
#include <QMetaType>
#include <QString>

namespace fincept {

/// Strongly-typed reference to a tradable/observable security. Used as the
/// payload for cross-panel symbol links (SymbolContext) so that receivers
/// can disambiguate a ticker across asset classes (e.g. "AAPL" equity vs.
/// "AAPL" option chain root) without string-munging.
///
/// `asset_class` uses lowercased slugs: "equity", "crypto", "fx", "future",
/// "bond", "index", "commodity", "option". Empty string = unknown; consumers
/// should treat unknown as equity for backward compatibility with older
/// single-string symbol flows.
struct SymbolRef {
    QString symbol;
    QString asset_class;
    QString exchange;

    bool is_valid() const { return !symbol.isEmpty(); }

    /// Bloomberg-style display: "AAPL US Equity", "BTCUSD Binance Crypto",
    /// falling back to the raw symbol if context is unknown.
    QString display() const {
        QString out = symbol;
        if (!exchange.isEmpty())
            out += ' ' + exchange;
        if (!asset_class.isEmpty()) {
            QString ac = asset_class;
            if (!ac.isEmpty())
                ac[0] = ac[0].toUpper();
            out += ' ' + ac;
        }
        return out;
    }

    bool operator==(const SymbolRef& o) const {
        return symbol == o.symbol && asset_class == o.asset_class && exchange == o.exchange;
    }
    bool operator!=(const SymbolRef& o) const { return !(*this == o); }

    QJsonObject to_json() const {
        QJsonObject o;
        o["symbol"] = symbol;
        o["asset_class"] = asset_class;
        o["exchange"] = exchange;
        return o;
    }

    static SymbolRef from_json(const QJsonObject& o) {
        SymbolRef r;
        r.symbol = o.value("symbol").toString();
        r.asset_class = o.value("asset_class").toString();
        r.exchange = o.value("exchange").toString();
        return r;
    }

    /// Convenience for call sites that only have a ticker string; defaults
    /// to equity asset class, which matches every existing single-string
    /// flow in the codebase.
    static SymbolRef equity(const QString& sym, const QString& exch = {}) {
        return SymbolRef{sym.toUpper(), QStringLiteral("equity"), exch};
    }
};

} // namespace fincept

Q_DECLARE_METATYPE(fincept::SymbolRef)
