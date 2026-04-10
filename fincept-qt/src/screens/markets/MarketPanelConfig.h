#pragma once
#include <QStringList>
#include <QString>
#include <QUuid>

namespace fincept::screens {

struct MarketPanelConfig {
    QString  id;        // stable UUID
    QString  title;
    QStringList symbols;
    bool     show_name = false; // show company name column

    static MarketPanelConfig make(const QString& title, const QStringList& symbols, bool show_name = false) {
        return { QUuid::createUuid().toString(QUuid::WithoutBraces), title, symbols, show_name };
    }
};

} // namespace fincept::screens
