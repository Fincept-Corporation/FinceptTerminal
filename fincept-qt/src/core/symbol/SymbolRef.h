#pragma once
#include <QJsonObject>
#include <QMetaType>
#include <QString>

namespace fincept {

/// Typed security reference used as SymbolContext payload.
/// `asset_class` slugs: equity|crypto|fx|future|bond|index|commodity|option. Empty = treat as equity.
struct SymbolRef {
    QString symbol;
    QString asset_class;
    QString exchange;

    bool is_valid() const { return !symbol.isEmpty(); }

    /// "AAPL US Equity" / "BTCUSD Binance Crypto"; falls back to raw symbol.
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

    static SymbolRef equity(const QString& sym, const QString& exch = {}) {
        return SymbolRef{sym.toUpper(), QStringLiteral("equity"), exch};
    }
};

} // namespace fincept

Q_DECLARE_METATYPE(fincept::SymbolRef)
