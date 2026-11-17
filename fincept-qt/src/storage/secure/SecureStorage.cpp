// SecureStorage — the facade every caller uses (see SecureStorage.h). The
// SQLite + AES-256-GCM backend lives in SecureStorageSqlite.cpp; this file is
// Qt Core + the logger only, so a unit test can link it with a fake backend.

#include "storage/secure/SecureStorage.h"

#include "core/logging/Logger.h"
#include "storage/secure/SecretMigration.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>

#include <atomic>

namespace fincept {

namespace secure_storage_facade_detail {

constexpr const char* kSecureFacadeTag = "SecureStorage";

struct FacadeState {
    QMutex backend_mutex;
    std::unique_ptr<ISecretBackend> backend;
    std::atomic<quint64> generation{1};
    // Read failures are logged as state transitions per key (Logger::transition):
    // one line when a key starts failing or changes kind, one when it reads again —
    // a read that keeps failing (every Python spawn used to read ~90 keys) is one line.
};

FacadeState& facade() {
    static FacadeState s;
    return s;
}

} // namespace secure_storage_facade_detail

using namespace secure_storage_facade_detail;

SecureStorage& SecureStorage::instance() {
    static SecureStorage s;
    return s;
}

ISecretBackend& SecureStorage::backend() {
    auto& f = facade();
    QMutexLocker lock(&f.backend_mutex);
    if (!f.backend)
        f.backend = make_default_secret_backend();
    return *f.backend;
}

void SecureStorage::set_backend_for_testing(std::unique_ptr<ISecretBackend> b) {
    auto& f = facade();
    {
        QMutexLocker lock(&f.backend_mutex);
        f.backend = std::move(b);
    }
    f.generation.fetch_add(1);
}

quint64 SecureStorage::generation() const {
    return facade().generation.load();
}

SecretRead SecureStorage::read(const QString& key) {
    SecretRead r = backend().read(key);
    if (r.failed()) {
        // State = the error kind (None = 0 = readable). The key NAME only (policy:
        // credential names are loggable, values never).
        LOG_TRANSITION(kSecureFacadeTag, key, static_cast<int>(r.error), r.transient() ? LogLevel::Warn : LogLevel::Error,
                       logf("Secure storage read failed", {{"key", key},
                                                           {"kind", r.error_name()},
                                                           {"transient", r.transient()},
                                                           {"detail", r.detail.left(200)}}));
    } else {
        auto& log = Logger::instance();
        LogTransition t;
        if (log.transition_possible(0) && log.transition(kSecureFacadeTag, key, 0, &t))
            LOG_INFO(kSecureFacadeTag, logf("Secure storage readable again",
                                            {{"key", key},
                                             {"failed_reads", t.observations_in_previous},
                                             {"duration_ms", t.ms_in_previous}}));
    }
    return r;
}

Result<QString> SecureStorage::retrieve(const QString& key) {
    const SecretRead r = read(key);
    if (r.found())
        return Result<QString>::ok(r.value);
    if (r.is_not_found())
        return Result<QString>::err(kNotFound);
    return Result<QString>::err(QStringLiteral("[secure-read:%1] %2")
                                    .arg(QLatin1String(r.error_name()), r.reason())
                                    .toStdString());
}

Result<void> SecureStorage::store(const QString& key, const QString& value) {
    auto r = backend().write(key, value);
    if (r.is_ok())
        facade().generation.fetch_add(1);
    return r;
}

Result<void> SecureStorage::store_verified(const QString& key, const QString& value) {
    const Result<void> w = store(key, value);
    if (w.is_err())
        return w;
    const SecretRead back = read(key); // a read failure is logged by read() (once per key and kind)
    const secret_migration::Failure f = secret_migration::verify_written(w, back, value);
    if (f == secret_migration::Failure::None)
        return w;
    return Result<void>::err(QStringLiteral("the stored value could not be confirmed (%1)")
                                 .arg(QLatin1String(secret_migration::failure_name(f)))
                                 .toStdString());
}

Result<void> SecureStorage::remove(const QString& key) {
    auto r = backend().erase(key);
    if (r.is_ok())
        facade().generation.fetch_add(1);
    return r;
}

Result<void> SecureStorage::reset_all() {
    auto r = backend().erase_all();
    auto& f = facade();
    if (r.is_ok()) {
        f.generation.fetch_add(1);
        LOG_AUDIT(kSecureFacadeTag, "Secure storage reset", {{"by", "user"}, {"erased", "every saved credential"}});
    } else {
        LOG_ERROR(kSecureFacadeTag,
                  QStringLiteral("Secure storage reset failed error=\"%1\"").arg(QString::fromStdString(r.error())));
    }
    return r;
}

} // namespace fincept
