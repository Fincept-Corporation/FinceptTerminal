#pragma once
// SecretRead — the outcome of reading one secret from SecureStorage, and the
// backend interface the store sits on. Header-only, Qt Core only.
//
// A read has THREE outcomes, and callers must handle all three:
//   Found     — the value.
//   NotFound  — nothing is stored under the key (definitive: the store was
//               readable and has no such row).
//   Error     — the store could not answer. NOT "nothing stored": a caller
//               that treats an error as empty can re-run a setup, overwrite a
//               secret it never read, or skip a lock screen. `transient()`
//               says whether retrying can help; `reason()` is a plain-language
//               sentence for the UI ("Could not read secure storage: <reason>").
//
// Error kinds:
//   StoreClosed    — the database is not open (yet / any more).          transient
//   Busy           — another connection or program holds the lock.       transient
//   Io             — the disk / database reported an I/O or SQL error.   transient
//   KeyUnavailable — the wrapped key exists but cannot be unwrapped here
//                    (DPAPI refused on this Windows account, profile copied
//                    from another machine, PBKDF2 seed changed).          permanent
//   Corrupt        — this row is damaged or was tampered with (GCM tag
//                    mismatch, wrong iv/tag size).                         permanent
//   Crypto         — the crypto library failed (out of memory, RNG).     transient

#include <QString>

#include "core/result/Result.h"

namespace fincept {

struct SecretRead {
    enum class Status { Found, NotFound, Error };
    enum class Error { None, StoreClosed, Busy, Io, KeyUnavailable, Corrupt, Crypto };

    Status status = Status::NotFound;
    QString value;               ///< Found only
    Error error = Error::None;   ///< Error only
    QString detail;              ///< technical detail for the log — never contains the value

    static SecretRead found_value(const QString& v) {
        SecretRead r;
        r.status = Status::Found;
        r.value = v;
        return r;
    }
    static SecretRead not_found() { return {}; }
    static SecretRead failure(Error e, const QString& why = {}) {
        SecretRead r;
        r.status = Status::Error;
        r.error = e;
        r.detail = why;
        return r;
    }

    bool found() const { return status == Status::Found; }
    bool is_not_found() const { return status == Status::NotFound; }
    bool failed() const { return status == Status::Error; }

    /// A retry (after a short wait) can succeed.
    bool transient() const {
        return failed() && (error == Error::StoreClosed || error == Error::Busy || error == Error::Io ||
                            error == Error::Crypto);
    }

    /// Stable short name for logs: "busy", "key_unavailable", ...
    static const char* error_name(Error e) {
        switch (e) {
            case Error::StoreClosed:
                return "store_closed";
            case Error::Busy:
                return "busy";
            case Error::Io:
                return "io";
            case Error::KeyUnavailable:
                return "key_unavailable";
            case Error::Corrupt:
                return "corrupt";
            case Error::Crypto:
                return "crypto";
            case Error::None:
                break;
        }
        return "none";
    }
    const char* error_name() const { return error_name(error); }

    /// Plain-language reason, completing "Could not read secure storage: …".
    QString reason() const {
        switch (error) {
            case Error::StoreClosed:
                return QStringLiteral("the terminal database is not open yet");
            case Error::Busy:
                return QStringLiteral("the terminal database is busy (another program may be using it)");
            case Error::Io:
                return QStringLiteral("the terminal database reported a read error");
            case Error::KeyUnavailable:
                return QStringLiteral("the key that protects your saved credentials cannot be unlocked on this "
                                      "user account or computer");
            case Error::Corrupt:
                return QStringLiteral("a saved credential is damaged");
            case Error::Crypto:
                return QStringLiteral("the encryption library failed");
            case Error::None:
                break;
        }
        return QStringLiteral("unknown error");
    }
};

/// Storage behind SecureStorage. The production backend is SQLite + AES-256-GCM
/// (SecureStorageSqlite.cpp); tests install a fake through
/// SecureStorage::set_backend_for_testing(). Implementations are thread-safe.
class ISecretBackend {
  public:
    virtual ~ISecretBackend() = default;
    virtual SecretRead read(const QString& key) = 0;
    virtual Result<void> write(const QString& key, const QString& value) = 0;
    virtual Result<void> erase(const QString& key) = 0;
    /// Erase EVERY row, the wrapped key included; the next write creates a new key.
    /// Only on an explicit, confirmed user action ("Reset secure storage").
    virtual Result<void> erase_all() = 0;
};

namespace secure_detail {

/// State of one wrapped-key row as read from the database.
enum class RowState { Absent, Present, ReadError };

/// What to do with the wrapped data-encryption key (DEK) at first use.
enum class DekPlan { UseDpapi, UsePbkdf2, CreateNew, Fail };

struct DekInputs {
    bool dpapi_supported = false; ///< Windows
    RowState dpapi = RowState::Absent;
    RowState pbkdf2 = RowState::Absent;
    bool dpapi_unwrapped = false;  ///< meaningful when dpapi == Present
    bool pbkdf2_unwrapped = false; ///< meaningful when pbkdf2 == Present
};

struct DekDecision {
    DekPlan plan = DekPlan::Fail;
    SecretRead::Error error = SecretRead::Error::None; ///< Fail only
    QString detail;
};

/// The rule that keeps a transient failure from destroying every credential:
/// a NEW key is created only when BOTH wrapped-key rows were read successfully
/// and neither exists. A row that could not be read, or that exists but cannot
/// be unwrapped, is an error — creating a key then would overwrite the wrapped
/// key on the next store() and make every saved credential unreadable for good.
inline DekDecision decide_dek(const DekInputs& in) {
    DekDecision d;
    if (in.dpapi_supported && in.dpapi == RowState::Present && in.dpapi_unwrapped) {
        d.plan = DekPlan::UseDpapi;
        return d;
    }
    if (in.pbkdf2 == RowState::Present && in.pbkdf2_unwrapped) {
        d.plan = DekPlan::UsePbkdf2;
        return d;
    }
    if (in.dpapi == RowState::ReadError || in.pbkdf2 == RowState::ReadError) {
        d.error = SecretRead::Error::Io;
        d.detail = QStringLiteral("the wrapped key rows could not be read");
        return d;
    }
    if (in.dpapi == RowState::Present || in.pbkdf2 == RowState::Present) {
        d.error = SecretRead::Error::KeyUnavailable;
        d.detail = in.dpapi == RowState::Present && !in.dpapi_supported
                       ? QStringLiteral("the key is DPAPI-wrapped (written on Windows) and cannot be unwrapped here")
                       : QStringLiteral("the wrapped key exists but could not be unwrapped");
        return d;
    }
    d.plan = DekPlan::CreateNew;
    return d;
}

} // namespace secure_detail

} // namespace fincept
