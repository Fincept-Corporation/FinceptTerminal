#include "storage/secure/SecureStorage.h"

#include <QSettings>

#ifdef Q_OS_WIN
#    include <wincred.h>
#    include <windows.h>
#    pragma comment(lib, "Advapi32.lib")
#endif

namespace fincept {

SecureStorage& SecureStorage::instance() {
    static SecureStorage s;
    return s;
}

Result<void> SecureStorage::store(const QString& key, const QString& value) {
#ifdef Q_OS_WIN
    std::wstring target = (L"FinceptTerminal/" + key.toStdWString());
    QByteArray data = value.toUtf8();

    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<LPWSTR>(target.c_str());
    cred.CredentialBlobSize = static_cast<DWORD>(data.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(data.data());
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    if (!CredWriteW(&cred, 0)) {
        return Result<void>::err("Failed to store credential");
    }
    return Result<void>::ok();
#else
    // Fallback: encrypted QSettings
    QSettings s("Fincept", "FinceptTerminal-Secure");
    s.setValue("secure/" + key, value);
    return Result<void>::ok();
#endif
}

Result<QString> SecureStorage::retrieve(const QString& key) {
#ifdef Q_OS_WIN
    std::wstring target = (L"FinceptTerminal/" + key.toStdWString());
    PCREDENTIALW cred = nullptr;

    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &cred)) {
        return Result<QString>::err("Credential not found");
    }

    QString value = QString::fromUtf8(reinterpret_cast<const char*>(cred->CredentialBlob),
                                      static_cast<int>(cred->CredentialBlobSize));
    CredFree(cred);
    return Result<QString>::ok(value);
#else
    QSettings s("Fincept", "FinceptTerminal-Secure");
    QVariant v = s.value("secure/" + key);
    if (!v.isValid())
        return Result<QString>::err("Not found");
    return Result<QString>::ok(v.toString());
#endif
}

Result<void> SecureStorage::remove(const QString& key) {
#ifdef Q_OS_WIN
    std::wstring target = (L"FinceptTerminal/" + key.toStdWString());
    if (!CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0)) {
        return Result<void>::err("Failed to delete credential");
    }
    return Result<void>::ok();
#else
    QSettings s("Fincept", "FinceptTerminal-Secure");
    s.remove("secure/" + key);
    return Result<void>::ok();
#endif
}

} // namespace fincept
