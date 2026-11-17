// src/services/gov_data/GovDataDecode.h
// The pure half of GovDataService: a finished script run (or a cached answer) turned into a
// GovDataResult. No logging, cache, hub, runner or widget access — any thread. GovDataService runs
// it on its decode thread so the GUI thread never parses a multi-MB answer (a data.gov.hk category
// with 100 datasets is 2.5 MB). A standalone timing harness links this file on its own (QtCore and
// python::extract_json from python/PythonRunner.h).
#pragma once

#include "services/gov_data/GovDataService.h"

#include <QString>

namespace fincept::python {
struct PythonResult;
}

namespace fincept::services::gov_decode {

/// A finished script run, fully classified: JSON extraction and parse, script-level failures
/// ({"success": false, "error": ...} or a bare {"error": ...}), credential redaction, error_code /
/// missing_key / retry_after_s. A bare JSON array becomes {"success": true, "data": [...]}.
GovDataResult decode_reply(const python::PythonResult& py);

/// A cache hit (the text cache_text() wrote). success = false when the text cannot be read as a
/// JSON object — the caller treats that as a miss.
GovDataResult decode_cached(const QString& cached_text);

/// The text execute() stores in the cache for a successful result (compact JSON of `data`).
QString cache_text(const GovDataResult& ok_result);

/// What a run stopped by its cancel token reports: success = false, error "Cancelled",
/// error_code "cancelled".
GovDataResult cancelled_result();

/// Masks credentials a script (or the HTTP library under it) echoed into a message
/// ("?api_key=..." -> "?api_key=***").
QString redact_secrets(QString text);

} // namespace fincept::services::gov_decode
