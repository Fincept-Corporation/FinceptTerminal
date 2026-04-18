#pragma once
// TotpService — thin wrapper around the exchange/totp_gen.py helper so that
// the CryptoCredentials dialog does not spawn Python directly (D1 / P6).
//
// This is a non-Producer service: TOTP codes are a per-dialog authentication
// helper with no cross-screen streaming use case, so there is no hub topic.
// Consumers call generate() with their own QPointer-guarded callback.

#include <QObject>
#include <QString>

#include <functional>

namespace fincept::services::crypto {

struct TotpResult {
    bool success = false;
    QString code;   // 6-digit code when success
    int valid_for = 0; // seconds remaining on the current window
    QString error;
};

using TotpCallback = std::function<void(const TotpResult&)>;

/// Generates 6-digit TOTP codes from a base32 secret via the bundled
/// `exchange/totp_gen.py` Python helper. Singleton to reuse PythonRunner's
/// concurrency slot accounting.
class TotpService : public QObject {
    Q_OBJECT
  public:
    static TotpService& instance();

    /// Async: runs totp_gen.py with the given secret. Callback fires on the
    /// main thread with the decoded result. Callers should guard with
    /// QPointer per P8 — the service does not know the caller's lifetime.
    void generate(const QString& secret, TotpCallback cb);

  private:
    TotpService() = default;
};

} // namespace fincept::services::crypto
