#pragma once
// MaResultExport — an M&A result (the JSON payload the module shows) as export-ready text: the headline KPIs and
// every table / grid / series the shared result layout finds in it, formatted exactly as ResultView shows them
// (same ui::result::Options: units, money format, hidden keys). Used by COPY, CSV and Add to Report.
// Qt Core only (ResultLayout is pure).

#include "ui/widgets/ResultLayout.h"

#include <QJsonValue>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::screens::ma_export {

struct Table {
    QString title;
    QStringList headers;
    QVector<QStringList> rows;
};

struct Export {
    QVector<QPair<QString, QString>> kpis; ///< label -> formatted value
    QVector<Table> tables;
    bool is_empty() const { return kpis.isEmpty() && tables.isEmpty(); }
};

/// Walk the presentation blocks of `payload` (result::build with `options`).
Export build(const QJsonValue& payload, const fincept::ui::result::Options& options);

/// Tab-separated text: a title line, the KPIs, then each table under its title.
QString to_tsv(const QString& title, const Export& e);

/// One CSV sheet for everything: rows of (section, item, value) for the KPIs, then each table after a blank row and
/// a title row. Returns (headers, rows) in the shape ui::ChartExport::save_csv takes.
QPair<QStringList, QVector<QStringList>> to_sheet(const Export& e);

} // namespace fincept::screens::ma_export
