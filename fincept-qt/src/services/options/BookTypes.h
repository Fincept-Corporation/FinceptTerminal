#pragma once
// BookTypes — the Derivatives position book as plain values (Qt Core only,
// header-only): a saved position (a single option, stock, future, swap, or a
// named multi-leg strategy) with its legs, entry prices, quantity and notes, and
// a saved strategy (the STRATEGY page's legs under a name). Both are stored by
// storage/repositories/DerivBookRepository in the tables of migration v065;
// services/options/BookMath values them.
//
// Units (the same as StrategyMath):
//   * option legs: quantity in contracts (signed: + long, − short), price per
//     share, × the position's multiplier (100 for US equity options);
//   * stock legs: quantity in shares, price per share (no multiplier);
//   * future legs: quantity in contracts, price per unit of the underlying,
//     × the position's multiplier (the contract size, e.g. 1,000 barrels);
//   * swap legs: notional in currency; the fixed rate is a decimal; `payer` =
//     pays fixed; entry_price = the swap's PV when it was booked (0 at market).

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::services::options::book {

enum class LegType { Call, Put, Stock, Future, Swap };

struct BookLeg {
    LegType type = LegType::Call;
    double quantity = 1;
    double strike = 0;
    QDate expiry;           ///< options and futures
    double entry_price = 0; ///< per share / per unit; swaps: PV at booking
    double entry_iv = 0;    ///< decimal, options: IV when booked (values the leg when the chain has no quote)
    QString contract;       ///< futures: the exchange symbol ("CLZ26.NYM"); options: optional display text
    // Swaps
    double swap_start = 0; ///< years after the entry date
    double swap_tenor = 0; ///< years
    double swap_rate = 0;  ///< decimal
    int swap_freq = 2;
    double notional = 0;
    bool payer = true;

    bool is_option() const { return type == LegType::Call || type == LegType::Put; }
};

struct BookPosition {
    QString id;
    QString book = QStringLiteral("Main");
    QString kind = QStringLiteral("option"); ///< option | stock | future | swap | strategy
    QString name;
    QString underlying; ///< "SPY", "^SPX", a futures root ("CL"), or "USD rates" for swaps
    QString currency = QStringLiteral("USD");
    double multiplier = 100;
    QDate entry_date;
    QVector<BookLeg> legs;
    QString strategy_id; ///< the saved strategy it came from, if any
    QString notes;
    qint64 created_at = 0; ///< epoch ms
    qint64 updated_at = 0;

    bool is_valid() const { return !legs.isEmpty(); }
};

struct SavedStrategy {
    QString id;
    QString name;
    QString underlying;
    QString template_id;
    QString currency = QStringLiteral("USD");
    double multiplier = 100;
    QVector<BookLeg> legs;
    QString notes;
    qint64 created_at = 0;
    qint64 updated_at = 0;
};

// ── JSON of the legs (the legs_json column) ──────────────────────────────────

inline QString leg_type_code(LegType t) {
    switch (t) {
        case LegType::Call: return QStringLiteral("C");
        case LegType::Put: return QStringLiteral("P");
        case LegType::Stock: return QStringLiteral("S");
        case LegType::Future: return QStringLiteral("F");
        case LegType::Swap: return QStringLiteral("W");
    }
    return QStringLiteral("C");
}

inline bool leg_type_from_code(const QString& code, LegType* out) {
    if (code == QLatin1String("C"))
        *out = LegType::Call;
    else if (code == QLatin1String("P"))
        *out = LegType::Put;
    else if (code == QLatin1String("S") || code == QLatin1String("U"))
        *out = LegType::Stock;
    else if (code == QLatin1String("F"))
        *out = LegType::Future;
    else if (code == QLatin1String("W"))
        *out = LegType::Swap;
    else
        return false;
    return true;
}

inline QJsonObject leg_to_json(const BookLeg& l) {
    QJsonObject o{{QStringLiteral("t"), leg_type_code(l.type)}, {QStringLiteral("q"), l.quantity}};
    if (l.is_option())
        o.insert(QStringLiteral("K"), l.strike);
    if (l.expiry.isValid())
        o.insert(QStringLiteral("e"), l.expiry.toString(Qt::ISODate));
    o.insert(QStringLiteral("p"), l.entry_price);
    if (l.is_option() && l.entry_iv > 0)
        o.insert(QStringLiteral("v"), l.entry_iv);
    if (!l.contract.isEmpty())
        o.insert(QStringLiteral("c"), l.contract);
    if (l.type == LegType::Swap) {
        o.insert(QStringLiteral("ss"), l.swap_start);
        o.insert(QStringLiteral("st"), l.swap_tenor);
        o.insert(QStringLiteral("sr"), l.swap_rate);
        o.insert(QStringLiteral("sf"), l.swap_freq);
        o.insert(QStringLiteral("n"), l.notional);
        o.insert(QStringLiteral("py"), l.payer);
    }
    return o;
}

/// False (and `out` untouched) for an object that is not a leg.
inline bool leg_from_json(const QJsonObject& o, BookLeg* out) {
    BookLeg l;
    if (!leg_type_from_code(o.value(QStringLiteral("t")).toString(), &l.type))
        return false;
    l.quantity = o.value(QStringLiteral("q")).toDouble(0);
    l.strike = o.value(QStringLiteral("K")).toDouble(0);
    l.expiry = QDate::fromString(o.value(QStringLiteral("e")).toString(), Qt::ISODate);
    l.entry_price = o.value(QStringLiteral("p")).toDouble(0);
    l.entry_iv = o.value(QStringLiteral("v")).toDouble(0);
    l.contract = o.value(QStringLiteral("c")).toString();
    l.swap_start = o.value(QStringLiteral("ss")).toDouble(0);
    l.swap_tenor = o.value(QStringLiteral("st")).toDouble(0);
    l.swap_rate = o.value(QStringLiteral("sr")).toDouble(0);
    l.swap_freq = o.value(QStringLiteral("sf")).toInt(2);
    l.notional = o.value(QStringLiteral("n")).toDouble(0);
    l.payer = o.value(QStringLiteral("py")).toBool(true);
    if (l.quantity == 0 && l.type != LegType::Swap)
        return false;
    if (l.is_option() && !(l.strike > 0))
        return false;
    *out = l;
    return true;
}

inline QString legs_to_json(const QVector<BookLeg>& legs) {
    QJsonArray a;
    for (const BookLeg& l : legs)
        a.append(leg_to_json(l));
    return QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact));
}

/// Legs from the stored JSON; entries that are not legs are skipped (`skipped` counts them).
inline QVector<BookLeg> legs_from_json(const QString& json, int* skipped = nullptr) {
    QVector<BookLeg> out;
    int bad = 0;
    const QJsonArray a = QJsonDocument::fromJson(json.toUtf8()).array();
    for (const QJsonValue& v : a) {
        BookLeg l;
        if (leg_from_json(v.toObject(), &l))
            out.append(l);
        else
            ++bad;
    }
    if (skipped)
        *skipped = bad;
    return out;
}

} // namespace fincept::services::options::book
