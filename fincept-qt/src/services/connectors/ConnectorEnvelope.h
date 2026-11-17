#pragma once
// ConnectorEnvelope — parses the one JSON line `python fincept_connector.py run <script> <command> [args]`
// prints (the connector contract, scripts/fincept_connector.py):
//
//   success  {"ok":true, "data":[rows], "columns":[...], "types":{col:kind}, "meta":{...}}      one table
//            {"ok":true, "data":{...}, "tables":{name:{columns,types,rows}}, "meta":{...}}       several
//   failure  {"ok":false, "error":"…", "code":"missing_key|…", "retry_after_s"?, "env_key"?,
//             "env_keys"?, "docs_url"?}
//
// Defensive about older payloads: a reply without "ok" is read as a bare connector payload
// ({"error": …} = failure; anything else = data, projected to a table).
// QtCore only.

#include "services/data_sources/SourceError.h"
#include "services/data_sources/TabularData.h"

#include <QByteArray>
#include <QJsonValue>
#include <QString>

namespace fincept::services::connectors {

struct EnvelopeResult {
    bool ok = false;
    fincept::services::datasources::TabularResult data;   ///< ok: the tables (never empty)
    fincept::services::datasources::SourceError error;    ///< !ok: the classified failure
    QJsonValue payload;   ///< ok: the envelope's "data" value (the raw object for multi-table replies)
    QString shape;        ///< meta.extra.shape ("rows", "nested_table", "tables", …) when present
    qint64 bytes = 0;
};

/// Parse one envelope. `text` may carry other lines before the JSON line (the last line that
/// parses as a JSON object wins).
EnvelopeResult parse_connector_envelope(const QByteArray& text);
/// Same for an already-parsed object.
EnvelopeResult parse_connector_envelope_object(const QJsonObject& envelope);

} // namespace fincept::services::connectors
