#pragma once
// ReportFileSync — one entry in the Files screen per report / export path.
//
// A report saved (or a PDF / HTML exported) OUTSIDE managed storage used to be imported
// into Files under a new id on every save, so ten saves listed ten copies. publish()
// keeps exactly one managed copy per path: the first save registers it, later saves of
// the same path refresh that copy. The file copy runs off the GUI thread; the Files
// metadata update runs on the GUI thread afterwards. GUI thread only.

#include <QString>

namespace fincept::services::report_files {

void publish(const QString& path, const QString& source_screen = QStringLiteral("report_builder"));

} // namespace fincept::services::report_files
