#pragma once
// RFC-6238 TOTP generator (HMAC-SHA1, 30s step, 6 digits by default).
// Pure function — no Qt event loop, safe to call from worker threads.

#include <QString>
#include <QByteArray>

namespace fincept::trading::zerodha {

// Returns an empty QString on invalid Base32 input.
QString generate_totp(const QString& base32_secret,
                      qint64 unix_time_seconds,
                      int digits = 6,
                      int step_seconds = 30);

// Exposed for testing — decodes an RFC-4648 Base32 string (A-Z, 2-7, ignores
// whitespace and '=' padding). Returns empty QByteArray on invalid input.
QByteArray base32_decode(const QString& input);

} // namespace fincept::trading::zerodha
