#pragma once
// ConnectionConfigDialog.h — modal dialog for adding, editing, or duplicating
// a data-source connection. The dialog renders dynamic form fields based on
// the ConnectorConfig schema, validates required fields, and persists via
// DataSourceRepository.
//
// Returns the saved connection's ID on accept, or an empty string on cancel
// or save failure. The caller is responsible for any post-save UI refresh.

#include "screens/data_sources/DataSourceTypes.h"

#include <QString>
#include <QWidget>

namespace fincept::screens::datasources {

/// Show the connection config dialog.
///
/// @param parent     parent widget (used for modal Z-order)
/// @param config     connector schema (drives the form layout)
/// @param edit_id    if non-empty AND duplicate==false, edits that connection
/// @param duplicate  if true, prefills from edit_id but creates a new ID on save
/// @return saved connection id, or empty string on cancel / failure
QString show_connection_config_dialog(QWidget* parent,
                                      const ConnectorConfig& config,
                                      const QString& edit_id = {},
                                      bool duplicate = false);

} // namespace fincept::screens::datasources
