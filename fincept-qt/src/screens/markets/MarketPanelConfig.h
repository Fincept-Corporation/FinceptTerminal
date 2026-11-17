#pragma once
#include <QHash>
#include <QString>
#include <QStringList>
#include <QUuid>

namespace fincept::screens {

/// Column ids (screens/markets/MarketColumns) a new panel shows:
/// NAME, LAST, CHG, CHG %, 5D TREND.
inline QStringList default_market_columns() {
    return {QStringLiteral("name"), QStringLiteral("last"), QStringLiteral("change"), QStringLiteral("change_pct"),
            QStringLiteral("spark")};
}

/// The legacy column codes of the previous builds ("column_order"), as they were
/// saved. Still read when a panel has no "columns" and still written (for the
/// codes that have one) so an older build can read a newer save.
inline QStringList legacy_column_codes() {
    return {"SYMBOL", "LAST", "CHG", "CHG%", "HIGH", "LOW", "VOL", "BID", "ASK", "OPEN", "TICKER", "NAME"};
}

/// The legacy default column list; a panel saved with exactly this gets the new default.
inline QStringList legacy_default_column_codes() {
    return {"SYMBOL", "LAST", "CHG", "CHG%", "HIGH", "LOW"};
}

struct MarketPanelConfig {
    QString id;
    QString title;
    QStringList symbols;
    bool show_name = false;   // legacy — kept for JSON compat (read, written, ignored)
    QStringList column_order; // legacy codes, kept for older builds; see legacy_column_codes()
    int column_index = 0;     // which horizontal splitter column (0-based)
    int splitter_index = 0;   // position within that column's vertical splitter

    QStringList columns;       // visible column ids in display order; empty = default_market_columns()
    QString sort_id;           // sorted column id; empty = the panel's own order
    bool sort_desc = false;
    QHash<QString, int> widths; // column id -> px (20..2000); missing = computed default

    static MarketPanelConfig make(const QString& title, const QStringList& symbols, bool /*show_name*/ = false) {
        MarketPanelConfig c;
        c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        c.title = title;
        c.symbols = symbols;
        c.columns = default_market_columns();
        return c;
    }
};

} // namespace fincept::screens
