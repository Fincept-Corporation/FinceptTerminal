// src/services/economics/EconomicsDecode.h
// The pure half of EconomicsService: a finished script run (or a cached answer) turned into an
// EconomicsResult. No logging, cache, hub or widget access — any thread; EconomicsService runs it
// on its decode pool so the GUI thread never parses a multi-MB answer (the EIA petroleum default
// was 7.6 MB, 23,514 rows). A standalone timing harness links this file on its own.
#pragma once

#include "services/economics/EconomicsService.h"

#include <QDateTime>
#include <QString>

namespace fincept::python {
struct PythonResult;
}

namespace fincept::services::econ_decode {

/// A decoded run plus what the service should log about it (the decode itself never logs).
struct DecodedReply {
    EconomicsResult result;
    /// Failure line for the log ("" = nothing to log); at warning level when `log_warning`.
    QString log_message;
    bool log_warning = false;
};

/// Decode a finished script run: JSON extraction and parse, script-level errors ("error" with an
/// "error_code" / "code" → "[CODE] message"; a batch whose every element failed), credential
/// redaction. `fetched_at` is stamped on the result (UTC).
DecodedReply decode_reply(const python::PythonResult& py, const QString& source_id, const QDateTime& fetched_at);

/// A cached answer (the "economics:v2" wrapper written by cache_text()); success = false when the
/// text cannot be read (treated as a miss). from_cache = true, fetched_at = the original time.
EconomicsResult decode_cached(const QString& text, const QString& source_id);

/// The cache text of a successful result: {"fetched_at": <ISO UTC>, "data": {...}}, compact.
QString cache_text(const EconomicsResult& result);

/// Mask credentials a provider (or the HTTP library echoing the request URL) copied into a message.
QString redact_secrets(QString text);

} // namespace fincept::services::econ_decode
