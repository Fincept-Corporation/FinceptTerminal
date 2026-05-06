#pragma once
#include <QDateTime>
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
///
/// Phase 7 / decision 3.2 — extended LinkedContext fields:
///   - `time_range_start` / `time_range_end`: optional time window the
///     publisher has scoped (charts, history panels). Default-constructed
///     QDateTime = unset; consumers ignore the field.
///   - `selection`: optional free-form row/cell key for screens that want
///     to propagate a sub-selection alongside the symbol (e.g. earnings
///     row, transaction id). String to keep schema flexible without
///     polluting SymbolRef with N typed selection variants.
struct SymbolRef {
    QString symbol;
    QString asset_class;
    QString exchange;

    /// Decision 3.2 time_range slot. Both fields unset by default. When
    /// only one is set, consumers should treat the unset side as
    /// open-ended ("from start_of_data" / "to now"). Time range is opt-in
    /// per panel — most panels ignore it and look at the symbol only.
    QDateTime time_range_start;
    QDateTime time_range_end;

    /// Decision 3.2 selection slot. Free-form id of the row / cell / sub-
    /// element the publisher highlighted. Empty string = no selection.
    QString selection;

    bool is_valid() const { return !symbol.isEmpty(); }
    bool has_time_range() const {
        return time_range_start.isValid() || time_range_end.isValid();
    }
    bool has_selection() const { return !selection.isEmpty(); }

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
        return symbol == o.symbol && asset_class == o.asset_class && exchange == o.exchange &&
               time_range_start == o.time_range_start && time_range_end == o.time_range_end &&
               selection == o.selection;
    }
    bool operator!=(const SymbolRef& o) const { return !(*this == o); }

    QJsonObject to_json() const {
        QJsonObject o;
        o["symbol"] = symbol;
        o["asset_class"] = asset_class;
        o["exchange"] = exchange;
        // Phase 7: optional fields are emitted only when set so workspace
        // JSON stays compact and pre-extension files round-trip cleanly.
        if (time_range_start.isValid())
            o["time_range_start"] = time_range_start.toString(Qt::ISODate);
        if (time_range_end.isValid())
            o["time_range_end"] = time_range_end.toString(Qt::ISODate);
        if (!selection.isEmpty())
            o["selection"] = selection;
        return o;
    }

    static SymbolRef from_json(const QJsonObject& o) {
        SymbolRef r;
        r.symbol = o.value("symbol").toString();
        r.asset_class = o.value("asset_class").toString();
        r.exchange = o.value("exchange").toString();
        // Optional fields — missing keys leave the corresponding
        // QDateTime/QString default-constructed (unset).
        const QString trs = o.value("time_range_start").toString();
        const QString tre = o.value("time_range_end").toString();
        if (!trs.isEmpty())
            r.time_range_start = QDateTime::fromString(trs, Qt::ISODate);
        if (!tre.isEmpty())
            r.time_range_end = QDateTime::fromString(tre, Qt::ISODate);
        r.selection = o.value("selection").toString();
        return r;
    }

    /// Convenience for call sites that only have a ticker string; defaults
    /// to equity asset class, which matches every existing single-string
    /// flow in the codebase.
    static SymbolRef equity(const QString& sym, const QString& exch = {}) {
        return SymbolRef{sym.toUpper(), QStringLiteral("equity"), exch, {}, {}, {}};
    }
};

} // namespace fincept

Q_DECLARE_METATYPE(fincept::SymbolRef)
