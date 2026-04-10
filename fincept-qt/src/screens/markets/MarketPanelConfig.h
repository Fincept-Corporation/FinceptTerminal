#pragma once
#include <QStringList>
#include <QString>
#include <QUuid>

namespace fincept::screens {

/// Returns the full list of available column identifiers for MarketPanel.
inline QStringList all_market_columns() {
    return {"SYMBOL", "LAST", "CHG", "CHG%", "HIGH", "LOW", "VOL", "BID", "ASK", "OPEN", "NAME"};
}

/// Returns the default column set used for new panels.
inline QStringList default_market_columns() {
    return {"SYMBOL", "LAST", "CHG", "CHG%", "HIGH", "LOW"};
}

struct MarketPanelConfig {
    QString     id;
    QString     title;
    QStringList symbols;
    bool        show_name      = false;         // legacy — kept for JSON compat
    QStringList column_order;                   // display column order; empty = use default_market_columns()
    int         column_index   = 0;             // which horizontal splitter column (0-based)
    int         splitter_index = 0;             // position within that column's vertical splitter

    static MarketPanelConfig make(const QString& title, const QStringList& symbols, bool /*show_name*/ = false) {
        return { QUuid::createUuid().toString(QUuid::WithoutBraces), title, symbols,
                 false, default_market_columns(), 0, 0 };
    }
};

} // namespace fincept::screens
