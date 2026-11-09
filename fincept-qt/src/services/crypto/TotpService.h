#pragma once
// TotpService — current TOTP code for a base32 seed, for the CryptoCredentials
// dialog (so the screen holds no TOTP logic itself, P6). Computed in-process by
// core/crypto/Totp: the permanent seed never leaves this process.
//
// This is a non-Producer service: TOTP codes are a per-dialog authentication
// helper with no cross-screen streaming use case, so there is no hub topic.

#include <QObject>
#include <QString>

#include <functional>

namespace fincept::services::crypto {

struct TotpResult {
    bool success = false;
    QString code;      // 6-digit code when success
    int valid_for = 0; // seconds remaining on the current window
    QString error;
};

using TotpCallback = std::function<void(const TotpResult&)>;

/// Generates 6-digit RFC 6238 codes (30 s step) from a base32 secret.
/// Lower case, spaces and missing '=' padding are accepted; any other
/// non-Base32 character is reported as an error.
class TotpService : public QObject {
    Q_OBJECT
  public:
    static TotpService& instance();

    /// Completes synchronously: `cb` runs on the caller's thread before
    /// generate() returns.
    void generate(const QString& secret, TotpCallback cb);

  private:
    TotpService() = default;
};

} // namespace fincept::services::crypto
