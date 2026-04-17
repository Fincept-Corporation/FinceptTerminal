#include "trading/brokers/zerodha/Totp.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>

namespace fincept::trading::zerodha {

QByteArray base32_decode(const QString& input) {
    QByteArray result;
    int buffer = 0;
    int bits_left = 0;
    for (QChar c : input) {
        if (c.isSpace() || c == QLatin1Char('='))
            continue;
        int val = 0;
        const char ch = c.toUpper().toLatin1();
        if (ch >= 'A' && ch <= 'Z')
            val = ch - 'A';
        else if (ch >= '2' && ch <= '7')
            val = 26 + (ch - '2');
        else
            return {}; // invalid character
        buffer = (buffer << 5) | val;
        bits_left += 5;
        if (bits_left >= 8) {
            bits_left -= 8;
            result.append(static_cast<char>((buffer >> bits_left) & 0xFF));
        }
    }
    return result;
}

namespace {
quint64 int_pow10(int digits) {
    quint64 v = 1;
    for (int i = 0; i < digits; ++i) v *= 10ULL;
    return v;
}
} // namespace

QString generate_totp(const QString& base32_secret, qint64 unix_time_seconds,
                      int digits, int step_seconds) {
    const QByteArray key = base32_decode(base32_secret);
    if (key.isEmpty())
        return {};

    const qint64 counter = unix_time_seconds / step_seconds;
    QByteArray counter_bytes;
    counter_bytes.resize(8);
    for (int i = 7; i >= 0; --i) {
        counter_bytes[i] = static_cast<char>((counter >> (8 * (7 - i))) & 0xFF);
    }

    QMessageAuthenticationCode mac(QCryptographicHash::Sha1, key);
    mac.addData(counter_bytes);
    const QByteArray hmac = mac.result();

    const int offset = hmac.at(hmac.size() - 1) & 0x0F;
    const quint32 bin_code = (static_cast<quint32>(hmac.at(offset) & 0x7F) << 24)
                           | (static_cast<quint32>(hmac.at(offset + 1) & 0xFF) << 16)
                           | (static_cast<quint32>(hmac.at(offset + 2) & 0xFF) << 8)
                           | (static_cast<quint32>(hmac.at(offset + 3) & 0xFF));

    const quint32 modulus = static_cast<quint32>(int_pow10(digits));
    const QString code = QString::number(bin_code % modulus);
    return code.rightJustified(digits, QLatin1Char('0'));
}

} // namespace fincept::trading::zerodha
