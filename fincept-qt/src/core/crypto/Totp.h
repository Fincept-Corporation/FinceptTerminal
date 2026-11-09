#pragma once
// RFC 6238 TOTP (HMAC-SHA1) and the RFC 4648 Base32 decoding of its secret.
// Pure functions — no event loop, no I/O, no logging — safe on any thread.
//
// The one implementation behind every TOTP code in the app: Zerodha auto-login,
// the Angel One / Kotak logins and the crypto-exchange credentials dialog
// (services/crypto/TotpService).

#include <QByteArray>
#include <QString>

namespace fincept::crypto {

/// What base32_decode() does with a character outside the Base32 alphabet.
enum class Base32Mode {
    /// Case-insensitive; whitespace and '=' are ignored anywhere; any other
    /// character fails the decode. Accepts everything pyotp accepts (lower
    /// case, missing padding) plus spaced groups ("abcd efgh ...").
    Strict,
    /// Upper-cases the string, then silently drops every non-alphabet
    /// character. Only for the Angel One / Kotak logins, which have always
    /// decoded this way: a typo there yields a wrong code, not an error.
    SkipInvalid,
};

/// RFC 4648 Base32 -> bytes. Bits left over after the last whole byte are
/// dropped, so any length decodes. Returns an empty array on failure (invalid
/// character in Strict mode, or not even one whole byte); `error`, when given,
/// then says why — without echoing any of the secret.
QByteArray base32_decode(const QString& input, Base32Mode mode = Base32Mode::Strict, QString* error = nullptr);

struct TotpCode {
    QString code;      ///< `digits` decimal digits, zero-padded; empty on failure
    int valid_for = 0; ///< seconds until the code rolls over, 1..step_seconds
    QString error;     ///< why `code` is empty
};

/// RFC 6238 TOTP at `unix_time_seconds`: HMAC-SHA1 over the 8-byte big-endian
/// counter floor(t / step_seconds), dynamically truncated to `digits` (1..10).
TotpCode generate_totp(const QString& base32_secret, qint64 unix_time_seconds, Base32Mode mode = Base32Mode::Strict,
                       int digits = 6, int step_seconds = 30);

} // namespace fincept::crypto
