#pragma once
// ReportOdtWriter.h — a Report Builder report as an ODF text document (.odt, ODF 1.3):
// opens in Microsoft Word and LibreOffice, needs no new dependency (QXmlStreamWriter +
// the report's own ZIP writer). Runs on a WORKER thread: Qt Core only, everything the
// GUI thread must prepare (chart PNGs, image bytes, formatted tables) comes in `assets`.
//
// The document mirrors the printed page (ReportRenderer): the report's paper and ink, its
// typeface and body size (every size scales with font_size / 12), page size, orientation
// and margins, the running header / footer with live page fields, heading numbers, the
// same texts under data blocks, and the same rule that blocks without content are left
// out. Named paragraph styles (Heading 1-3, Text body, Caption, Quotations, Table
// Contents, ...) keep the result editable in a word processor.
//
// Approximations: the diagonal watermark has no portable ODF equivalent and is printed
// in the page header (right, light grey, bold); the TOC lists the headings without page
// numbers; chart / sparkline pictures are the PNGs the GUI thread rendered.

#include "core/report/ReportDocument.h"
#include "core/report/ReportExportAssets.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <functional>

namespace fincept::report::odt {

struct OdtOptions {
    QString generator = QStringLiteral("Fincept Terminal");
    QDateTime created; ///< invalid = now (tests pass a fixed one: the output is then byte-for-byte stable)
};

/// The .odt file. `cancelled` (may be empty) is asked between blocks: when it returns
/// true the result is empty and *error = "cancelled". Other failures: empty result,
/// *error = a short reason. `error` may be null.
QByteArray write(const ReportDocument& doc, const exportfmt::ExportAssets& assets, const OdtOptions& opt,
                 const std::function<bool()>& cancelled, QString* error);

} // namespace fincept::report::odt
