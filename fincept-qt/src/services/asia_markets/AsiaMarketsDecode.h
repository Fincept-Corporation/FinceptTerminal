#pragma once
// AsiaMarketsDecode — the pure half of AsiaMarketsService: a finished connector run
// (python::PythonResult) into the service's EndpointsResult / QueryResult (rows, ordered columns,
// ignored parameters, the failure and its class, the missing parameters, the rows' estimated size).
//
// QtCore only — plus python::extract_json (a fallback for output that is not just the payload) and
// ui::autoload::approx_json_bytes. No logging, cache, singletons or widgets: it runs on the
// service's decode thread, and a standalone harness can link and time it.

#include "services/asia_markets/AsiaMarketsService.h"

namespace fincept::python {
struct PythonResult;
}

namespace fincept::services::asia_markets {

/// `<script> <endpoint> [args…]` → QueryResult. A failed run (or a {"success": false} /
/// {"error": …} answer) gives success = false with the script's message, its class (error_type)
/// and, for the "needs an input" answer, the missing parameter names. Never sets `cancelled`
/// (the caller owns that). Any thread.
QueryResult decode_query(const fincept::python::PythonResult& py, const QString& endpoint);

/// `<script> get_all_endpoints` → EndpointsResult (the script's JSON object as is). Any thread.
EndpointsResult decode_endpoints(const fincept::python::PythonResult& py);

} // namespace fincept::services::asia_markets
