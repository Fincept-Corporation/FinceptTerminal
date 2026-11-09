#include "services/crypto/TotpService.h"

#include "core/crypto/Totp.h"

#include <QDateTime>

namespace fincept::services::crypto {

TotpService& TotpService::instance() {
    static TotpService s;
    return s;
}

void TotpService::generate(const QString& secret, TotpCallback cb) {
    if (!cb)
        return;
    TotpResult out;
    if (secret.isEmpty()) {
        out.error = "empty secret";
        cb(out);
        return;
    }
    const fincept::crypto::TotpCode totp = fincept::crypto::generate_totp(secret, QDateTime::currentSecsSinceEpoch());
    out.success = !totp.code.isEmpty();
    out.code = totp.code;
    out.valid_for = totp.valid_for;
    out.error = totp.error;
    cb(out);
}

} // namespace fincept::services::crypto
