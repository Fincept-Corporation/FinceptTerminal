#include "screens/markets/MarketColumns.h"

#include <QCoreApplication>
#include <QSet>

#include <cmath>

namespace fincept::screens::mcols {

// Strings are marked with QT_TRANSLATE_NOOP so lupdate sees them; translated at use.
const QVector<ColumnDef>& all() {
    // Data only — a function-local static table is fine (no appearance state).
    // Order = the Column enum.
    static const QVector<ColumnDef> kDefs = {
        {"name", QT_TRANSLATE_NOOP("MarketColumns", "NAME"),
         QT_TRANSLATE_NOOP("MarketColumns", "Instrument name (ticker beside it when there is room)"), Kind::Name, true,
         0, 0, -1},
        {"ticker", QT_TRANSLATE_NOOP("MarketColumns", "TICKER"),
         QT_TRANSLATE_NOOP("MarketColumns", "Display ticker"), Kind::Text, false, 12, 0, 11},
        {"last", QT_TRANSLATE_NOOP("MarketColumns", "LAST"), QT_TRANSLATE_NOOP("MarketColumns", "Last price"),
         Kind::Price, true, 11, 0, -1},
        {"change", QT_TRANSLATE_NOOP("MarketColumns", "CHG"),
         QT_TRANSLATE_NOOP("MarketColumns", "Change from the previous close"), Kind::Change, true, 10, 0, 9},
        {"change_pct", QT_TRANSLATE_NOOP("MarketColumns", "CHG %"),
         QT_TRANSLATE_NOOP("MarketColumns", "Percent change from the previous close (+/-3 % = full colour)"),
         Kind::HeatPct, true, 8, 0, -1},
        {"spark", QT_TRANSLATE_NOOP("MarketColumns", "5D TREND"),
         QT_TRANSLATE_NOOP("MarketColumns", "Five days of hourly closes; baseline = previous close"), Kind::Spark, true,
         0, 96, 10},
        {"day_range", QT_TRANSLATE_NOOP("MarketColumns", "DAY RANGE"),
         QT_TRANSLATE_NOOP("MarketColumns", "Today's low - high, with the last price"), Kind::RangeBar, false, 0, 150,
         1},
        {"volume", QT_TRANSLATE_NOOP("MarketColumns", "VOLUME"), QT_TRANSLATE_NOOP("MarketColumns", "Volume today"),
         Kind::Volume, false, 9, 0, 2},
        {"rel_volume", QT_TRANSLATE_NOOP("MarketColumns", "REL VOL"),
         QT_TRANSLATE_NOOP("MarketColumns", "Volume today / 3-month average daily volume"), Kind::RelVolume, false, 9,
         0, 3},
        {"high", QT_TRANSLATE_NOOP("MarketColumns", "DAY HIGH"), QT_TRANSLATE_NOOP("MarketColumns", "Day high"),
         Kind::Price, false, 10, 0, 6},
        {"low", QT_TRANSLATE_NOOP("MarketColumns", "DAY LOW"), QT_TRANSLATE_NOOP("MarketColumns", "Day low"),
         Kind::Price, false, 10, 0, 7},
        {"open", QT_TRANSLATE_NOOP("MarketColumns", "OPEN"), QT_TRANSLATE_NOOP("MarketColumns", "Opening price"),
         Kind::Price, false, 10, 0, 5},
        {"prev_close", QT_TRANSLATE_NOOP("MarketColumns", "PREV CLOSE"),
         QT_TRANSLATE_NOOP("MarketColumns", "Previous close (what the change is measured from)"), Kind::Price, false,
         10, 0, 4},
        {"range_52w", QT_TRANSLATE_NOOP("MarketColumns", "52W RANGE"),
         QT_TRANSLATE_NOOP("MarketColumns", "52-week low - high, with the last price"), Kind::RangeBar, false, 0, 150,
         0},
        {"state", QT_TRANSLATE_NOOP("MarketColumns", "MARKET"),
         QT_TRANSLATE_NOOP("MarketColumns", "Session state of the instrument's market"), Kind::State, false, 16, 0, 8},
        {"panel", QT_TRANSLATE_NOOP("MarketColumns", "PANEL"),
         QT_TRANSLATE_NOOP("MarketColumns", "The panel the symbol is listed in"), Kind::Text, false, 14, 0, -1, true},
    };
    return kDefs;
}

const ColumnDef& def(int column) {
    const auto& d = all();
    return d.at(column >= 0 && column < d.size() ? column : 0);
}

int index_of(QStringView id) {
    const auto& d = all();
    for (int i = 0; i < d.size(); ++i)
        if (id == QLatin1String(d.at(i).id))
            return i;
    return -1;
}

QString id_at(int column) {
    return QString::fromLatin1(def(column).id);
}

QString header(int column) {
    return QCoreApplication::translate("MarketColumns", def(column).header);
}

QString tooltip(int column) {
    return QCoreApplication::translate("MarketColumns", def(column).tip);
}

bool locked(int column) {
    return column == Column::Name || column == Column::Last;
}

QStringList default_ids() {
    QStringList out;
    for (const auto& d : all())
        if (d.default_visible)
            out << QString::fromLatin1(d.id);
    return out;
}

QString from_legacy_code(const QString& code) {
    static const struct {
        const char* code;
        const char* id;
    } kMap[] = {{"SYMBOL", "name"}, {"LAST", "last"}, {"CHG", "change"}, {"CHG%", "change_pct"},
                {"HIGH", "high"},   {"LOW", "low"},   {"VOL", "volume"}, {"OPEN", "open"},
                {"TICKER", "ticker"}};
    for (const auto& m : kMap)
        if (code == QLatin1String(m.code))
            return QString::fromLatin1(m.id);
    return {}; // BID / ASK (no data), NAME (duplicate of SYMBOL), unknown
}

QString to_legacy_code(const QString& id) {
    static const struct {
        const char* id;
        const char* code;
    } kMap[] = {{"name", "SYMBOL"}, {"last", "LAST"}, {"change", "CHG"}, {"change_pct", "CHG%"},
                {"high", "HIGH"},   {"low", "LOW"},   {"volume", "VOL"}, {"open", "OPEN"},
                {"ticker", "TICKER"}};
    for (const auto& m : kMap)
        if (id == QLatin1String(m.id))
            return QString::fromLatin1(m.code);
    return {};
}

QStringList sanitize_ids(const QStringList& ids) {
    QStringList out;
    QSet<QString> seen;
    for (const QString& id : ids) {
        const int c = index_of(id);
        if (c < 0 || def(c).internal || seen.contains(id))
            continue;
        seen.insert(id);
        out << id;
    }
    out.removeAll(QStringLiteral("name"));
    out.prepend(QStringLiteral("name"));
    if (!out.contains(QStringLiteral("last")))
        out.insert(1, QStringLiteral("last"));
    return out;
}

QStringList ids_from_legacy(const QStringList& codes) {
    if (codes.isEmpty() || codes == QStringList{"SYMBOL", "LAST", "CHG", "CHG%", "HIGH", "LOW"})
        return default_ids();
    QStringList ids;
    for (const QString& c : codes) {
        const QString id = from_legacy_code(c);
        if (!id.isEmpty())
            ids << id;
    }
    return sanitize_ids(ids);
}

QStringList legacy_from_ids(const QStringList& ids) {
    QStringList out;
    for (const QString& id : ids) {
        const QString code = to_legacy_code(id);
        if (!code.isEmpty() && !out.contains(code))
            out << code;
    }
    if (!out.contains(QStringLiteral("SYMBOL")))
        out.prepend(QStringLiteral("SYMBOL"));
    return out;
}

bool applies(int column, core::instrument::AssetClass cls, const services::QuoteData& q) {
    using core::instrument::AssetClass;
    switch (column) {
        case Column::Volume:
            if (cls == AssetClass::Fx)
                return false;
            if (cls == AssetClass::Index)
                return q.volume > 0.0;
            return true;
        case Column::RelVolume:
            if (cls == AssetClass::Fx)
                return false;
            if (cls == AssetClass::Index && !(q.volume > 0.0))
                return false;
            return std::isfinite(q.average_volume) && q.average_volume > 0.0;
        default:
            return true;
    }
}

QString class_word(core::instrument::AssetClass cls) {
    using core::instrument::AssetClass;
    switch (cls) {
        case AssetClass::Fx:
            return QCoreApplication::translate("MarketColumns", "currency pairs");
        case AssetClass::Index:
            return QCoreApplication::translate("MarketColumns", "indices");
        case AssetClass::Future:
            return QCoreApplication::translate("MarketColumns", "futures");
        case AssetClass::Crypto:
            return QCoreApplication::translate("MarketColumns", "crypto");
        case AssetClass::Etf:
            return QCoreApplication::translate("MarketColumns", "ETFs");
        case AssetClass::Fund:
            return QCoreApplication::translate("MarketColumns", "funds");
        default:
            break;
    }
    return QCoreApplication::translate("MarketColumns", "this instrument");
}

} // namespace fincept::screens::mcols
