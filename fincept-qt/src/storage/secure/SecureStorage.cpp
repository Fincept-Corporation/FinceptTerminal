#include "storage/secure/SecureStorage.h"

#include "core/logging/Logger.h"

// ── Platform-specific credential storage ──────────────────────────────────────
//
// Windows : Windows Credential Manager (DPAPI-backed, encrypted at rest)
// macOS   : Security.framework SecItem API (SecItemAdd/Update/CopyMatching)
// Linux   : libsecret / Secret Service when available (GNOME Keyring, KWallet),
//           falling back to XOR-obfuscated QSettings when libsecret is absent.
//           The XOR fallback is NOT cryptographically secure — it only prevents
//           casual inspection. Build with libsecret-1-dev for production use.

#ifdef Q_OS_WIN
#    include <windows.h>
#    include <wincred.h>
#    include <wincrypt.h>
#    pragma comment(lib, "Advapi32.lib")
#endif

#ifdef Q_OS_MAC
#    include <CoreFoundation/CoreFoundation.h>
#    include <Security/Security.h>
#endif

#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC)
#    include <QCryptographicHash>
#    include <QSettings>
#    include <QSysInfo>
#    ifdef FINCEPT_HAVE_LIBSECRET
#        include <libsecret/secret.h>
#    endif
#endif

static constexpr auto TAG = "SecureStorage";
static constexpr const char* kService = "com.fincept.terminal";

namespace fincept {

SecureStorage& SecureStorage::instance() {
    static SecureStorage s;

#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC) && !defined(FINCEPT_HAVE_LIBSECRET)
    // Warn once at startup when running on Linux without libsecret. Operators
    // should install libsecret-1-dev and rebuild for production deployments.
    static bool warned_once = false;
    if (!warned_once) {
        warned_once = true;
        LOG_WARN(TAG,
                 "Linux credential storage is using XOR-obfuscated QSettings — "
                 "NOT cryptographically secure. Install libsecret-1-dev and rebuild "
                 "to enable the Secret Service (GNOME Keyring / KWallet) backend.");
    }
#endif

    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// Linux helpers
// ─────────────────────────────────────────────────────────────────────────────
#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC)

#ifdef FINCEPT_HAVE_LIBSECRET

static const SecretSchema* fincept_schema() {
    static const SecretSchema schema = {
        "com.fincept.terminal",
        SECRET_SCHEMA_NONE,
        {
            { "fincept-key", SECRET_SCHEMA_ATTRIBUTE_STRING },
            { nullptr, SecretSchemaAttributeType(0) }
        }
    };
    return &schema;
}

#else // XOR fallback helpers

static QByteArray machine_key() {
    const QString seed = QSysInfo::machineUniqueId() + QSysInfo::productType()
                         + QLatin1String(kService);
    return QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Sha256);
}

static QByteArray xor_obfuscate(const QByteArray& data) {
    const QByteArray key = machine_key();
    QByteArray out;
    out.resize(data.size());
    for (int i = 0; i < data.size(); ++i)
        out[i] = static_cast<char>(
            static_cast<unsigned char>(data[i]) ^
            static_cast<unsigned char>(key[i % key.size()]));
    return out;
}

#endif // FINCEPT_HAVE_LIBSECRET
#endif // Linux helpers

// ─────────────────────────────────────────────────────────────────────────────
// store
// ─────────────────────────────────────────────────────────────────────────────

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
    // macOS Keychain via modern SecItem API (no deprecated SecKeychain* calls)
    const QByteArray svc_bytes = QByteArray(kService);
    const QByteArray acct_bytes = key.toUtf8();
    const QByteArray data_bytes = value.toUtf8();

    CFStringRef service = CFStringCreateWithBytes(
        nullptr, reinterpret_cast<const UInt8*>(svc_bytes.constData()),
        svc_bytes.size(), kCFStringEncodingUTF8, false);
    CFStringRef account = CFStringCreateWithBytes(
        nullptr, reinterpret_cast<const UInt8*>(acct_bytes.constData()),
        acct_bytes.size(), kCFStringEncodingUTF8, false);
    CFDataRef secret_data = CFDataCreate(
        nullptr, reinterpret_cast<const UInt8*>(data_bytes.constData()),
        static_cast<CFIndex>(data_bytes.size()));

    // Try to update an existing item first
    const void* find_keys[] = { kSecClass, kSecAttrService, kSecAttrAccount };
    const void* find_vals[] = { kSecClassGenericPassword, service, account };
    CFDictionaryRef find_query = CFDictionaryCreate(nullptr, find_keys, find_vals, 3,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    const void* upd_keys[] = { kSecValueData };
    const void* upd_vals[] = { secret_data };
    CFDictionaryRef upd_attrs = CFDictionaryCreate(nullptr, upd_keys, upd_vals, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    OSStatus status = SecItemUpdate(find_query, upd_attrs);
    CFRelease(upd_attrs);

    if (status == errSecItemNotFound) {
        // No existing item — add a new one
        const void* add_keys[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecValueData };
        const void* add_vals[] = { kSecClassGenericPassword, service, account, secret_data };
        CFDictionaryRef add_attrs = CFDictionaryCreate(nullptr, add_keys, add_vals, 4,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        status = SecItemAdd(add_attrs, nullptr);
        CFRelease(add_attrs);
    }

    CFRelease(find_query);
    CFRelease(secret_data);
    CFRelease(account);
    CFRelease(service);

    if (status != errSecSuccess) {
        LOG_ERROR(TAG, QString("Keychain store failed (OSStatus %1) for key: %2")
                           .arg(status).arg(key));
        return Result<void>::err("Failed to store in Keychain");
    }
    return Result<void>::ok();

#else
#    ifdef FINCEPT_HAVE_LIBSECRET
    // Linux — Secret Service (GNOME Keyring / KWallet compatibility layer)
    GError* err = nullptr;
    const QByteArray key_bytes = key.toUtf8();
    const QByteArray val_bytes = value.toUtf8();
    const QByteArray label = ("Fincept Terminal: " + key).toUtf8();

    secret_password_store_sync(
        fincept_schema(),
        SECRET_COLLECTION_DEFAULT,
        label.constData(),
        val_bytes.constData(),
        nullptr,
        &err,
        "fincept-key", key_bytes.constData(),
        nullptr);

    if (err) {
        QString msg = QString::fromUtf8(err->message);
        g_error_free(err);
        LOG_ERROR(TAG, "libsecret store failed for key " + key + ": " + msg);
        return Result<void>::err("Failed to store in Secret Service: " + msg.toStdString());
    }
    return Result<void>::ok();

#    else
    // XOR-obfuscated QSettings fallback — not cryptographically secure.
    QSettings s("Fincept", "FinceptTerminal-Secure");
    const QByteArray obfuscated = xor_obfuscate(value.toUtf8()).toBase64();
    s.setValue("secure/" + key, QString::fromLatin1(obfuscated));
    return Result<void>::ok();
#    endif
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// retrieve
// ─────────────────────────────────────────────────────────────────────────────

Result<QString> SecureStorage::retrieve(const QString& key) {
#ifdef Q_OS_WIN
    std::wstring target = L"FinceptTerminal/" + key.toStdWString();
    PCREDENTIALW cred = nullptr;

    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &cred)) {
        return Result<QString>::err("Credential not found");
    }

    QString value = QString::fromUtf8(
        reinterpret_cast<const char*>(cred->CredentialBlob),
        static_cast<int>(cred->CredentialBlobSize));
    CredFree(cred);
    return Result<QString>::ok(value);

#elif defined(Q_OS_MAC)
    const QByteArray svc_bytes = QByteArray(kService);
    const QByteArray acct_bytes = key.toUtf8();

    CFStringRef service = CFStringCreateWithBytes(
        nullptr, reinterpret_cast<const UInt8*>(svc_bytes.constData()),
        svc_bytes.size(), kCFStringEncodingUTF8, false);
    CFStringRef account = CFStringCreateWithBytes(
        nullptr, reinterpret_cast<const UInt8*>(acct_bytes.constData()),
        acct_bytes.size(), kCFStringEncodingUTF8, false);

    const void* keys[] = { kSecClass, kSecAttrService, kSecAttrAccount,
                           kSecReturnData, kSecMatchLimit };
    const void* vals[] = { kSecClassGenericPassword, service, account,
                           kCFBooleanTrue, kSecMatchLimitOne };
    CFDictionaryRef query = CFDictionaryCreate(nullptr, keys, vals, 5,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFTypeRef result_ref = nullptr;
    OSStatus status = SecItemCopyMatching(query, &result_ref);
    CFRelease(query);
    CFRelease(account);
    CFRelease(service);

    if (status != errSecSuccess || !result_ref) {
        return Result<QString>::err("Credential not found in Keychain");
    }

    CFDataRef data = static_cast<CFDataRef>(result_ref);
    QString value = QString::fromUtf8(
        reinterpret_cast<const char*>(CFDataGetBytePtr(data)),
        static_cast<int>(CFDataGetLength(data)));
    CFRelease(result_ref);
    return Result<QString>::ok(value);

#else
#    ifdef FINCEPT_HAVE_LIBSECRET
    GError* err = nullptr;
    const QByteArray key_bytes = key.toUtf8();

    gchar* password = secret_password_lookup_sync(
        fincept_schema(),
        nullptr,
        &err,
        "fincept-key", key_bytes.constData(),
        nullptr);

    if (err) {
        QString msg = QString::fromUtf8(err->message);
        g_error_free(err);
        return Result<QString>::err("libsecret lookup failed: " + msg.toStdString());
    }
    if (!password) {
        return Result<QString>::err("Not found");
    }

    QString value = QString::fromUtf8(password);
    secret_password_free(password);
    return Result<QString>::ok(value);

#    else
    QSettings s("Fincept", "FinceptTerminal-Secure");
    QVariant v = s.value("secure/" + key);
    if (!v.isValid())
        return Result<QString>::err("Not found");

    const QByteArray raw = QByteArray::fromBase64(v.toString().toLatin1());
    if (raw.isEmpty())
        return Result<QString>::err("Not found");
    return Result<QString>::ok(QString::fromUtf8(xor_obfuscate(raw)));
#    endif
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// remove
// ─────────────────────────────────────────────────────────────────────────────

Result<void> SecureStorage::remove(const QString& key) {
#ifdef Q_OS_WIN
    std::wstring target = L"FinceptTerminal/" + key.toStdWString();
    if (!CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0)) {
        return Result<void>::err("Failed to delete credential");
    }
    return Result<void>::ok();

#elif defined(Q_OS_MAC)
    const QByteArray svc_bytes = QByteArray(kService);
    const QByteArray acct_bytes = key.toUtf8();

    CFStringRef service = CFStringCreateWithBytes(
        nullptr, reinterpret_cast<const UInt8*>(svc_bytes.constData()),
        svc_bytes.size(), kCFStringEncodingUTF8, false);
    CFStringRef account = CFStringCreateWithBytes(
        nullptr, reinterpret_cast<const UInt8*>(acct_bytes.constData()),
        acct_bytes.size(), kCFStringEncodingUTF8, false);

    const void* keys[] = { kSecClass, kSecAttrService, kSecAttrAccount };
    const void* vals[] = { kSecClassGenericPassword, service, account };
    CFDictionaryRef query = CFDictionaryCreate(nullptr, keys, vals, 3,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    SecItemDelete(query);
    CFRelease(query);
    CFRelease(account);
    CFRelease(service);
    return Result<void>::ok();

#else
#    ifdef FINCEPT_HAVE_LIBSECRET
    GError* err = nullptr;
    const QByteArray key_bytes = key.toUtf8();

    secret_password_clear_sync(
        fincept_schema(),
        nullptr,
        &err,
        "fincept-key", key_bytes.constData(),
        nullptr);

    if (err)
        g_error_free(err); // Not fatal — item may not have existed
    return Result<void>::ok();

#    else
    QSettings s("Fincept", "FinceptTerminal-Secure");
    s.remove("secure/" + key);
    return Result<void>::ok();
#    endif
#endif
}

} // namespace fincept
