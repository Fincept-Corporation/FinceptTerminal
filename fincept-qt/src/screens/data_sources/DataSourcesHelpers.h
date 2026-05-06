#pragma once
// DataSourcesHelpers.h — pure functions on ConnectorConfig / DataSource.
// No Qt-widget dependencies (except QTableWidgetItem helper).

#include "screens/data_sources/DataSourceTypes.h"
#include "storage/repositories/DataSourceRepository.h"

#include <QColor>
#include <QString>
#include <QTableWidgetItem>
#include <QVector>
#include <Qt>

namespace fincept::screens::datasources {

// Lookup a ConnectorConfig by either its id or its `type` field.
// Returns nullptr if no match in ConnectorRegistry.
const ConnectorConfig* find_connector_config(const QString& key);

// Canonical connector id for a connection record. Falls back to the raw
// provider string when no registry match exists.
QString normalized_provider_key(const DataSource& ds);

// True when `ds` belongs to the connector identified by `connector_id`.
bool connection_matches_connector(const DataSource& ds, const QString& connector_id);

// Case-insensitive substring search across name/id/type/description/category.
// `filter` is expected pre-lowered by the caller.
bool connector_matches_text(const ConnectorConfig& cfg, const QString& filter);

// 2-character symbol displayed in the detail panel and connector grid.
QString connector_code(const ConnectorConfig& cfg);

// Short transport badge — SQL / REST / WS / CLOUD / TSDB / MKT / SRCH / DW / ALT / OB.
QString connector_transport(const ConnectorConfig& cfg);

// Persisted `type` field on DataSource — "websocket" for streaming, else "rest_api".
QString persistence_type(const ConnectorConfig& cfg);

// Display label for FieldType (Text/Secret/Number/URL/Select/Textarea/Bool/File).
QString field_type_label(FieldType type);

// Counters used by the provider ladder + detail-panel stats.
int total_connections_for_provider(const QVector<DataSource>& rows, const QString& connector_id);
int enabled_connections_for_provider(const QVector<DataSource>& rows, const QString& connector_id);

// Non-editable QTableWidgetItem with foreground colour and alignment.
QTableWidgetItem* make_item(const QString& text,
                            const QColor& color = {},
                            Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter);

} // namespace fincept::screens::datasources
