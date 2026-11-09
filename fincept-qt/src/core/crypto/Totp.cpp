#include "core/crypto/Totp.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>

namespace fincept::crypto {

QByteArray base32_decode(const QString& input, Base32Mode mode, QString* error) {
    const bool strict = mode == Base32Mode::Strict;
    // Strict upper-cases character by character (simple case mapping) and SkipInvalid the whole
    // string (full mapping, e.g. "ß" -> "SS") — exactly what the Zerodha and the Angel One / Kotak
    // decoders each did, so every secret they accepted still decodes to the same key.
    const QString text = strict ? input : input.toUpper();
    QByteArray out;
    quint32 buffer = 0;
    int bits = 0;
    for (const QChar c : text) {
        if (strict && (c.isSpace() || c == QLatin1Char('=')))
            continue;
        const char16_t ch = strict ? c.toUpper().unicode() : c.unicode();
        quint32 value = 0;
        if (ch >= u'A' && ch <= u'Z') {
            value = static_cast<quint32>(ch - u'A');
        } else if (ch >= u'2' && ch <= u'7') {
            value = static_cast<quint32>(26 + (ch - u'2'));
        } else if (strict) {
            if (error)
                *error = QStringLiteral("TOTP secret is not valid Base32 (only A-Z and 2-7 are allowed)");
            return {};
        } else {
            continue;
        }
        buffer = (buffer << 5) | value; // only the low 13 bits are ever read back
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            out.append(static_cast<char>((buffer >> bits) & 0xFFu));
        }
    }
    if (out.isEmpty() && error)
        *error = QStringLiteral("TOTP secret is empty or too short");
    return out;
}

TotpCode generate_totp(const QString& base32_secret, qint64 unix_time_seconds, Base32Mode mode, int digits,
                       int step_seconds) {
    TotpCode out;
    if (digits < 1 || digits > 10 || step_seconds < 1 || unix_time_seconds < 0) {
        out.error = QStringLiteral("invalid TOTP parameters");
        return out;
    }
    const QByteArray key = base32_decode(base32_secret, mode, &out.error);
    if (key.isEmpty())
        return out;

    const auto counter = static_cast<quint64>(unix_time_seconds / step_seconds);
    QByteArray message(8, '\0');
    for (int i = 0; i < 8; ++i)
        message[i] = static_cast<char>((counter >> (56 - 8 * i)) & 0xFFu);
    const QByteArray mac = QMessageAuthenticationCode::hash(message, key, QCryptographicHash::Sha1);

    // RFC 4226 §5.3 dynamic truncation.
    const auto byte_at = [&mac](qsizetype i) { return static_cast<quint32>(static_cast<quint8>(mac.at(i))); };
    const auto offset = static_cast<qsizetype>(byte_at(mac.size() - 1) & 0x0Fu);
    const quint32 binary = ((byte_at(offset) & 0x7Fu) << 24) | (byte_at(offset + 1) << 16) |
                           (byte_at(offset + 2) << 8) | byte_at(offset + 3);

    quint64 modulus = 1;
    for (int i = 0; i < digits; ++i)
        modulus *= 10;
    out.code = QString::number(binary % modulus).rightJustified(digits, QLatin1Char('0'));
    out.valid_for = step_seconds - static_cast<int>(unix_time_seconds % step_seconds);
    return out;
}

} // namespace fincept::crypto
