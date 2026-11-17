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

    /// Symbol system (optional; written to JSON only when set, so older files
    /// load unchanged): the stable instrument id of core/instrument/Instrument.h
    /// ("XNSE:RELIANCE", "US:AAPL", "FX:EURUSD", "CRYPTO:BTC-USD"). Empty = the
    /// publisher did not resolve the instrument.
    QString instrument_id{};
    /// Symbol system (optional): the canonical market-data key (Yahoo form) when
    /// `symbol` is spelled another way — a broker symbol ("RELIANCE" + exchange
    /// "NSE" -> "RELIANCE.NS"), a Crypto Trading pair ("BTC/USDT" -> "BTC-USD").
    /// Empty = `symbol` itself is the key, or unknown.
    QString yahoo{};

    bool is_valid() const { return !symbol.isEmpty(); }
    bool has_time_range() const { return time_range_start.isValid() || time_range_end.isValid(); }
    bool has_selection() const { return !selection.isEmpty(); }

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
        return symbol == o.symbol && asset_class == o.asset_class && exchange == o.exchange &&
               time_range_start == o.time_range_start && time_range_end == o.time_range_end && selection == o.selection;
    }
    bool operator!=(const SymbolRef& o) const { return !(*this == o); }

    /// Identity question (pushpin de-duplication, selection matching) — unlike
    /// operator==, which compares every field including time range and selection:
    ///   1. both refs carry an instrument_id -> equal ids;
    ///   2. otherwise the market keys must be equal and non-empty — key = `yahoo`
    ///      when set, else `symbol`; trimmed and upper-cased (Unicode, not locale) —
    ///      and nothing may contradict:
    ///        - when either ref has no `yahoo`, two non-empty exchanges must be equal
    ///          (RELIANCE@NSE is not RELIANCE@BSE);
    ///        - two non-empty asset classes must be the same family ("equity",
    ///          "stock", "etf", "fund" are one family; "AAPL" equity is not "AAPL" option).
    /// Spelling variants ("BRK.B" vs "BRK-B") match only through `yahoo` /
    /// instrument_id: publishers fill those with core/instrument canonical_symbol().
    bool same_instrument(const SymbolRef& o) const {
        if (!instrument_id.isEmpty() && !o.instrument_id.isEmpty())
            return instrument_id == o.instrument_id;
        const QString ka = (yahoo.isEmpty() ? symbol : yahoo).trimmed().toUpper();
        const QString kb = (o.yahoo.isEmpty() ? o.symbol : o.yahoo).trimmed().toUpper();
        if (ka.isEmpty() || ka != kb)
            return false;
        if (yahoo.isEmpty() || o.yahoo.isEmpty()) {
            const QString ea = exchange.trimmed().toUpper();
            const QString eb = o.exchange.trimmed().toUpper();
            if (!ea.isEmpty() && !eb.isEmpty() && ea != eb)
                return false;
        }
        const QString ca = class_family(asset_class);
        const QString cb = class_family(o.asset_class);
        return ca.isEmpty() || cb.isEmpty() || ca == cb;
    }

    /// Asset-class family used by same_instrument(): lower-cased slug, with
    /// "stock" / "etf" / "fund" folded into "equity". Empty for an empty class.
    static QString class_family(const QString& asset_class) {
        const QString c = asset_class.trimmed().toLower();
        if (c == QLatin1String("stock") || c == QLatin1String("etf") || c == QLatin1String("fund"))
            return QStringLiteral("equity");
        return c;
    }

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
        if (!instrument_id.isEmpty())
            o["instrument_id"] = instrument_id;
        if (!yahoo.isEmpty())
            o["yahoo"] = yahoo;
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
        r.instrument_id = o.value("instrument_id").toString();
        r.yahoo = o.value("yahoo").toString();
        return r;
    }

    static SymbolRef equity(const QString& sym, const QString& exch = {}) {
        return SymbolRef{sym.toUpper(), QStringLiteral("equity"), exch, {}, {}, {}};
    }
};

} // namespace fincept

Q_DECLARE_METATYPE(fincept::SymbolRef)
