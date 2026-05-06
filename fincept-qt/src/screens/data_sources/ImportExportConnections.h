#pragma once
// ImportExportConnections.h — JSON round-trip for data-source connections,
// plus a "blank template" dump of every registered connector.
//
// All three functions show a QFileDialog parented at `parent`. They return
// true on success (file written / connections imported) and false on cancel
// or I/O failure. Import returns counts via out-params so the caller can
// surface them to the user.

#include <QString>
#include <QWidget>

namespace fincept::screens::datasources {

// Prompt for a save location and write all DataSourceRepository entries
// as a JSON array. Also registers the file with FileManagerService.
bool export_connections(QWidget* parent);

// Prompt for a JSON file and import any valid entries (each gets a fresh
// UUID). Sets `imported_out` and `skipped_out` if non-null.
bool import_connections(QWidget* parent, int* imported_out = nullptr, int* skipped_out = nullptr);

// Prompt for a save location and write a blank template containing every
// registered connector grouped by category (Databases first, etc).
bool download_connector_template(QWidget* parent);

} // namespace fincept::screens::datasources
