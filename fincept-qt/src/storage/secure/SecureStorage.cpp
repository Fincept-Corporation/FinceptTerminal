#include "storage/secure/SecureStorage.h"

#include "core/logging/Logger.h"

#include <QCryptographicHash>
#include <QSettings>
#include <QSysInfo>

// ── Platform-specific credential storage ──────────────────────────────────────
//
// Windows : Windows Credential Manager (DPAPI-backed, encrypted at rest)
// macOS   : Security.framework Keychain (encrypted, user-unlocked)
// Linux   : XOR-obfuscated QSettings with machine-unique key derivation.
//           NOTE: This is NOT cryptographically secure — it prevents casual
//           inspection but not a determined attacker with filesystem access.
//           For full security, use libsecret/Secret Service when available.
//           A proper libsecret backend should be added in a future release.

#ifdef Q_OS_WIN
#    include <windows.h>
#    include <wincred.h>
#    include <wincrypt.h>
#    pragma comment(lib, "Advapi32.lib")
#endif

#ifdef Q_OS_MAC
#    include <Security/Security.h>
#endif

static constexpr auto TAG = "SecureStorage";
static constexpr const char* kService = "com.fincept.terminal";

namespace fincept {

SecureStorage& SecureStorage::instance() {
    static SecureStorage s;

#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC)
    // One-time honest warning on Linux: the backend here is XOR-obfuscated
    // QSettings, not real encryption. Emit at first access so it always
    // shows up in the startup log for operators, and only once per process.
    static bool warned_once = false;
    if (!warned_once) {
        warned_once = true;
        LOG_WARN("SecureStorage",
                 "Linux backend is XOR-obfuscated QSettings — NOT cryptographically "
                 "secure. Anyone with read access to the user profile can recover "
                 "stored secrets. libsecret/KWallet backend is planned.");
    }
#endif

    return s;
}

// ── Linux helpers ─────────────────────────────────────────────────────────────
#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC)

// Derive a machine-unique obfuscation key from stable system identifiers.
// Not a secret — just prevents the file being readable at a glance.
static QByteArray machine_key() {
    const QString seed = QSysInfo::machineUniqueId() + QSysInfo::productType() + QLatin1String(kService);
    return QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Sha256);
}

static QByteArray xor_obfuscate(const QByteArray& data) {
    const QByteArray key = machine_key();
    QByteArray out;
    out.resize(data.size());
    for (int i = 0; i < data.size(); ++i)
        out[i] =
            static_cast<char>(static_cast<unsigned char>(data[i]) ^ static_cast<unsigned char>(key[i % key.size()]));
    return out;
}

#endif // Linux helpers

// ── store ─────────────────────────────────────────────────────────────────────

Result<void> SecureStorage::store(const QString& key, const QString& value) {
#ifdef Q_OS_WIN
    // Windows Credential Manager — DPAPI-encrypted, user-scoped
    std::wstring target = L"FinceptTerminal/" + key.toStdWString();
    QByteArray data = value.toUtf8();

    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<LPWSTR>(target.c_str());
    cred.CredentialBlobSize = static_cast<DWORD>(data.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(data.data());
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    if (!CredWriteW(&cred, 0)) {
        LOG_ERROR(TAG, "CredWriteW failed for key: " + key);
        return Result<void>::err("Failed to store credential");
    }
    return Result<void>::ok();

#elif defined(Q_OS_MAC)
    // macOS Keychain — encrypted, user-unlocked.
    // NOTE: Using legacy SecKeychain* API (deprecated in 10.10). The modern
    // SecItem* API is a larger port; suppressing the warning locally keeps
    // CI green. TODO: migrate to SecItemAdd / SecItemCopyMatching.
    const QByteArray svc = QByteArray(kService);
    const QByteArray acct = key.toUtf8();
    const QByteArray data = value.toUtf8();

    OSStatus status;
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // Delete any existing item first (SecItemUpdate is more complex)
    SecKeychainItemRef existing = nullptr;
    OSStatus del_status =
        SecKeychainFindGenericPassword(nullptr, static_cast<UInt32>(svc.size()), svc.constData(),
                                       static_cast<UInt32>(acct.size()), acct.constData(), nullptr, nullptr, &existing);
    if (del_status == errSecSuccess && existing) {
        SecKeychainItemDelete(existing);
        CFRelease(existing);
    }

    status = SecKeychainAddGenericPassword(nullptr, static_cast<UInt32>(svc.size()), svc.constData(),
                                           static_cast<UInt32>(acct.size()), acct.constData(),
                                           static_cast<UInt32>(data.size()), data.constData(), nullptr);
#    pragma clang diagnostic pop

    if (status != errSecSuccess) {
        LOG_ERROR(TAG, QString("Keychain store failed (OSStatus %1) for key: %2").arg(status).arg(key));
        return Result<void>::err("Failed to store in Keychain");
    }
    return Result<void>::ok();

#else
    // Linux — XOR-obfuscated QSettings.
    // WARNING: Not cryptographically secure. Prevents casual inspection only.
    // TODO: Add libsecret backend for proper encryption on Linux.
    QSettings s("Fincept", "FinceptTerminal-Secure");
    const QByteArray obfuscated = xor_obfuscate(value.toUtf8()).toBase64();
    s.setValue("secure/" + key, QString::fromLatin1(obfuscated));
    return Result<void>::ok();
#endif
}

// ── retrieve ──────────────────────────────────────────────────────────────────

Result<QString> SecureStorage::retrieve(const QString& key) {
#ifdef Q_OS_WIN
    std::wstring target = L"FinceptTerminal/" + key.toStdWString();
    PCREDENTIALW cred = nullptr;

    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &cred)) {
        return Result<QString>::err("Credential not found");
    }

    QString value = QString::fromUtf8(reinterpret_cast<const char*>(cred->CredentialBlob),
                                      static_cast<int>(cred->CredentialBlobSize));
    CredFree(cred);
    return Result<QString>::ok(value);

#elif defined(Q_OS_MAC)
    const QByteArray svc = QByteArray(kService);
    const QByteArray acct = key.toUtf8();

    UInt32 data_len = 0;
    void* data_ptr = nullptr;
    OSStatus status;
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
    status = SecKeychainFindGenericPassword(nullptr, static_cast<UInt32>(svc.size()), svc.constData(),
                                            static_cast<UInt32>(acct.size()), acct.constData(), &data_len,
                                            &data_ptr, nullptr);
#    pragma clang diagnostic pop

    if (status != errSecSuccess) {
        return Result<QString>::err("Credential not found in Keychain");
    }

    QString value = QString::fromUtf8(static_cast<const char*>(data_ptr), static_cast<int>(data_len));
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
    SecKeychainItemFreeContent(nullptr, data_ptr);
#    pragma clang diagnostic pop
    return Result<QString>::ok(value);

#else
    QSettings s("Fincept", "FinceptTerminal-Secure");
    QVariant v = s.value("secure/" + key);
    if (!v.isValid())
        return Result<QString>::err("Not found");

    // Deobfuscate: base64-decode then XOR
    const QByteArray raw = QByteArray::fromBase64(v.toString().toLatin1());
    if (raw.isEmpty())
        return Result<QString>::err("Not found");
    return Result<QString>::ok(QString::fromUtf8(xor_obfuscate(raw)));
#endif
}

// ── remove ────────────────────────────────────────────────────────────────────

Result<void> SecureStorage::remove(const QString& key) {
#ifdef Q_OS_WIN
    std::wstring target = L"FinceptTerminal/" + key.toStdWString();
    if (!CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0)) {
        return Result<void>::err("Failed to delete credential");
    }
    return Result<void>::ok();

#elif defined(Q_OS_MAC)
    const QByteArray svc = QByteArray(kService);
    const QByteArray acct = key.toUtf8();

#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
    SecKeychainItemRef item = nullptr;
    OSStatus status =
        SecKeychainFindGenericPassword(nullptr, static_cast<UInt32>(svc.size()), svc.constData(),
                                       static_cast<UInt32>(acct.size()), acct.constData(), nullptr, nullptr, &item);

    if (status == errSecSuccess && item) {
        SecKeychainItemDelete(item);
        CFRelease(item);
    }
#    pragma clang diagnostic pop
    return Result<void>::ok();

#else
    QSettings s("Fincept", "FinceptTerminal-Secure");
    s.remove("secure/" + key);
    return Result<void>::ok();
#endif
}

} // namespace fincept
