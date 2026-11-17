#pragma once
// ImportExportConnections — the Data Sources IMPORT / EXPORT actions and the blank template.
//
// Export leaves every secret (password, API key, token, auth header) OUT of the file unless the
// user opts in; then the secrets are encrypted with a passphrase they choose (PBKDF2 + AES-256-GCM,
// services/data_sources/ConnectionTransfer.h). Nothing is ever written in plain text.
// Import reads that format (asking for the passphrase, or importing without the secrets) and the
// older plain-array files. Reading, key derivation and saving run off the GUI thread; `done`
// is called on the GUI thread (dropped when `parent` is gone).

#include <QString>
#include <QWidget>

#include <functional>

namespace fincept::screens::datasources {

using TransferDone = std::function<void(bool ok, int count, const QString& message)>;

void export_connections(QWidget* parent, TransferDone done);
void import_connections(QWidget* parent, TransferDone done);

/// Prompt for a save location and write a blank template containing every registered connection
/// type (secrets left empty). Returns false on cancel / I/O failure.
bool download_connector_template(QWidget* parent);

} // namespace fincept::screens::datasources
