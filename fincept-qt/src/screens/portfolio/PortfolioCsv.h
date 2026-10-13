// src/screens/portfolio/PortfolioCsv.h
//
// Tiny CSV writer shared by the portfolio tables (positions, transactions,
// performance). Header-only: it is a few lines of file I/O behind a save
// dialog, used only from the portfolio screens.
#pragma once

#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QVector>
#include <QWidget>

namespace fincept::screens::portfolio_csv {

/// RFC 4180 quoting, plus a guard against spreadsheet formula injection
/// (a cell starting with = + @ is executed by Excel/Sheets when opened).
inline QString escape(const QString& field) {
    QString f = field;
    if (!f.isEmpty() && (f.front() == QLatin1Char('=') || f.front() == QLatin1Char('+') ||
                         f.front() == QLatin1Char('@')))
        f.prepend(QLatin1Char('\''));
    if (f.contains(QLatin1Char(',')) || f.contains(QLatin1Char('"')) || f.contains(QLatin1Char('\n')) ||
        f.contains(QLatin1Char('\r'))) {
        f.replace(QLatin1String("\""), QLatin1String("\"\""));
        f = QLatin1Char('"') + f + QLatin1Char('"');
    }
    return f;
}

/// Plain, locale-independent number for machine-readable output.
inline QString num(double v, int dp = 6) {
    return QString::number(v, 'f', dp);
}

/// Ask for a path and write @p header + @p rows. Returns the written path, or
/// an empty string when the user cancelled or the write failed (the failure
/// is reported to the user here).
inline QString save(QWidget* parent, const QString& suggested_name, const QStringList& header,
                    const QVector<QStringList>& rows) {
    const QString path = QFileDialog::getSaveFileName(
        parent, QCoreApplication::translate("PortfolioCsv", "Export CSV"), suggested_name,
        QCoreApplication::translate("PortfolioCsv", "CSV Files (*.csv)"));
    if (path.isEmpty())
        return {};
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(parent, QCoreApplication::translate("PortfolioCsv", "Export Failed"),
                             QCoreApplication::translate("PortfolioCsv", "Could not write %1:\n%2")
                                 .arg(path, f.errorString()));
        return {};
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true); // Excel needs the BOM to read UTF-8
    auto write_row = [&out](const QStringList& cells) {
        QStringList escaped;
        escaped.reserve(cells.size());
        for (const auto& c : cells)
            escaped << escape(c);
        out << escaped.join(QLatin1Char(',')) << '\n';
    };
    write_row(header);
    for (const auto& r : rows)
        write_row(r);
    out.flush();
    if (out.status() != QTextStream::Ok) {
        QMessageBox::warning(parent, QCoreApplication::translate("PortfolioCsv", "Export Failed"),
                             QCoreApplication::translate("PortfolioCsv", "Write error while saving %1.").arg(path));
        return {};
    }
    return path;
}

} // namespace fincept::screens::portfolio_csv
