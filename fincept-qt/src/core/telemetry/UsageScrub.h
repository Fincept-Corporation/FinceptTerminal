#pragma once
// UsageScrub — what the usage report may carry from free text. Every string
// that leaves the machine in a report goes through one of these. Pure
// functions, safe on any thread.

#include <QString>

class QUrl;

namespace fincept::telemetry {

/// One line of `text` (the first non-empty one), at most `max_chars`, with
/// personal data removed: the home folder becomes "~", and URLs, e-mail
/// addresses, credentials (API keys, bearer tokens, key=value secrets), quoted
/// values, long numbers / opaque ids and ticker-like upper-case words are
/// replaced by placeholders.
QString scrub_sample(const QString& text, int max_chars = 300);

/// Path of a Fincept API URL with every id-like segment replaced by ":id"
/// ("/chat/sessions/:id/title"). Never the host, the query or the fragment.
QString scrub_route(const QUrl& url);

/// A short identifier (screen id, action id, script name, code) reduced to
/// [A-Za-z0-9 _.:=/<>-] and at most `max_chars`.
QString scrub_label(const QString& text, int max_chars = 80);

} // namespace fincept::telemetry
