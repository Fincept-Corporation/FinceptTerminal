// PinManager (src/auth/PinManager.cpp) over a FAKE SecureStorage backend: a
// failure to read the PIN record is never "no PIN set".
//
// Links PinManager.cpp + the SecureStorage facade + the logger. The SQLite
// backend and SecurityAuditLog (both need the database) are replaced here:
// this file supplies make_default_secret_backend() and a no-op audit log.
#include "auth/PinManager.h"
#include "auth/SecurityAuditLog.h"
#include "storage/secure/SecureStorage.h"

#include <QHash>
#include <QSet>
#include <QSignalSpy>
#include <QtTest>

#include <deque>

using namespace fincept;
using fincept::auth::PinManager;

namespace {

class FakeBackend : public ISecretBackend {
  public:
    QHash<QString, QString> values;
    QHash<QString, SecretRead> sticky;               // key -> failure returned on every read
    QHash<QString, std::deque<SecretRead>> once;     // key -> failures returned first, once each
    QSet<QString> fail_writes;                       // keys whose write fails
    int writes = 0;

    SecretRead read(const QString& key) override {
        if (sticky.contains(key))
            return sticky.value(key);
        auto it = once.find(key);
        if (it != once.end() && !it->empty()) {
            SecretRead r = it->front();
            it->pop_front();
            return r;
        }
        if (values.contains(key))
            return SecretRead::found_value(values.value(key));
        return SecretRead::not_found();
    }
    Result<void> write(const QString& key, const QString& value) override {
        ++writes;
        if (fail_writes.contains(key))
            return Result<void>::err("Failed to write credential row");
        values.insert(key, value);
        return Result<void>::ok();
    }
    Result<void> erase(const QString& key) override {
        values.remove(key);
        return Result<void>::ok();
    }
    Result<void> erase_all() override {
        values.clear();
        return Result<void>::ok();
    }
};

FakeBackend* g_fake = nullptr;

/// A real PIN record for "135790" (hash = PBKDF2 as PinManager computes it).
void seed_pin(FakeBackend* f) {
    PinManager& pm = PinManager::instance();
    f->sticky.clear();
    f->once.clear();
    f->fail_writes.clear();
    f->values.clear();
    pm.reload();
    bool done = false;
    pm.set_pin_async(QStringLiteral("135790"), nullptr, [&done](Result<void> r) {
        QVERIFY2(r.is_ok(), r.is_err() ? r.error().c_str() : "");
        done = true;
    });
    QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
}

} // namespace

std::unique_ptr<ISecretBackend> fincept::make_default_secret_backend() {
    auto f = std::make_unique<FakeBackend>();
    g_fake = f.get();
    return f;
}

// No database here: the audit log is a no-op.
namespace fincept::auth {
SecurityAuditLog& SecurityAuditLog::instance() {
    static SecurityAuditLog s;
    return s;
}
void SecurityAuditLog::record(const QString&, const QString&) {}
QList<AuditEvent> SecurityAuditLog::recent(int) {
    return {};
}
} // namespace fincept::auth

class TestPinManagerStorage : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase() {
        (void)SecureStorage::instance().read(QStringLiteral("probe")); // creates the fake backend
        QVERIFY(g_fake);
        (void)PinManager::instance();
    }

    void not_found_means_not_set() {
        g_fake->values.clear();
        g_fake->sticky.clear();
        PinManager::instance().reload();
        QCOMPARE(PinManager::instance().pin_state(), PinManager::PinState::NotSet);
        QVERIFY(!PinManager::instance().has_pin());
    }

    void set_pin_when_not_set() {
        seed_pin(g_fake);
        QCOMPARE(PinManager::instance().pin_state(), PinManager::PinState::Set);
        QVERIFY(g_fake->values.contains(QStringLiteral("pin_hash")));
    }

    void transient_error_then_success() {
        seed_pin(g_fake);
        const QString hash_before = g_fake->values.value(QStringLiteral("pin_hash"));
        PinManager& pm = PinManager::instance();
        QSignalSpy spy(&pm, &PinManager::pin_state_changed);

        g_fake->sticky.insert(QStringLiteral("pin_hash"),
                              SecretRead::failure(SecretRead::Error::Busy, QStringLiteral("database is locked")));
        pm.reload();
        QCOMPARE(pm.pin_state(), PinManager::PinState::Unreadable);
        QVERIFY(pm.read_failure().transient());
        QVERIFY(pm.has_pin()); // fails closed: the terminal stays locked
        QCOMPARE(spy.count(), 1);

        // The store recovers: the automatic back-off retry (first one after 0.5 s)
        // brings the PIN back without any user action.
        g_fake->sticky.remove(QStringLiteral("pin_hash"));
        QTRY_COMPARE_WITH_TIMEOUT(pm.pin_state(), PinManager::PinState::Set, 5000);
        QVERIFY(spy.count() >= 2);
        QCOMPARE(g_fake->values.value(QStringLiteral("pin_hash")), hash_before); // never rewritten

        // And the old PIN still unlocks.
        bool verified = false, done = false;
        pm.verify_pin_async(QStringLiteral("135790"), nullptr, [&](bool ok) {
            verified = ok;
            done = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
        QVERIFY(verified);
    }

    void permanent_error_blocks_setup_and_never_overwrites() {
        seed_pin(g_fake);
        const QString hash_before = g_fake->values.value(QStringLiteral("pin_hash"));
        PinManager& pm = PinManager::instance();
        g_fake->sticky.insert(QStringLiteral("pin_hash"),
                              SecretRead::failure(SecretRead::Error::KeyUnavailable, QStringLiteral("DPAPI refused")));
        pm.reload();
        QCOMPARE(pm.pin_state(), PinManager::PinState::Unreadable);
        QVERIFY(!pm.read_failure().transient());
        QVERIFY(pm.has_pin());

        const int writes_before = g_fake->writes;
        bool done = false;
        QString error;
        pm.set_pin_async(QStringLiteral("246810"), nullptr, [&](Result<void> r) {
            if (r.is_err())
                error = QString::fromStdString(r.error());
            done = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
        QVERIFY2(error.startsWith(QStringLiteral("Could not read secure storage")), qPrintable(error));
        QCOMPARE(g_fake->writes, writes_before);
        QCOMPARE(g_fake->values.value(QStringLiteral("pin_hash")), hash_before);
        g_fake->sticky.clear();
    }

    void corrupted_blob_is_unreadable_and_not_counted() {
        seed_pin(g_fake);
        PinManager& pm = PinManager::instance();
        g_fake->sticky.insert(QStringLiteral("pin_hash"),
                              SecretRead::failure(SecretRead::Error::Corrupt, QStringLiteral("tag mismatch")));
        pm.reload();
        QCOMPARE(pm.pin_state(), PinManager::PinState::Unreadable);
        QCOMPARE(pm.read_failure().error, SecretRead::Error::Corrupt);

        const int attempts_before = pm.failed_attempts();
        bool done = false, ok = true;
        pm.verify_pin_async(QStringLiteral("135790"), nullptr, [&](bool v) {
            ok = v;
            done = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
        QVERIFY(!ok);                                       // cannot unlock against an unreadable record…
        QCOMPARE(pm.failed_attempts(), attempts_before);   // …and it is not a failed attempt
        g_fake->sticky.clear();
        pm.reload();
        QCOMPARE(pm.pin_state(), PinManager::PinState::Set);
    }

    void half_record_is_corrupt_not_absent() {
        seed_pin(g_fake);
        g_fake->values.remove(QStringLiteral("pin_salt"));
        PinManager::instance().reload();
        QCOMPARE(PinManager::instance().pin_state(), PinManager::PinState::Unreadable);
        QCOMPARE(PinManager::instance().read_failure().error, SecretRead::Error::Corrupt);
    }

    void unreadable_lockout_counter_is_unreadable() {
        seed_pin(g_fake);
        g_fake->sticky.insert(QStringLiteral("pin_failed_attempts"),
                              SecretRead::failure(SecretRead::Error::Io, QStringLiteral("disk I/O error")));
        PinManager::instance().reload();
        QCOMPARE(PinManager::instance().pin_state(), PinManager::PinState::Unreadable);
        g_fake->sticky.clear();
    }

    // PIN rotation: a record is two rows. A failure between the two writes used to
    // leave the new hash beside the old salt — no PIN unlocked any more.
    void change_pin_partial_write_keeps_a_working_pin() {
        seed_pin(g_fake);
        PinManager& pm = PinManager::instance();
        const QString old_hash = g_fake->values.value(QStringLiteral("pin_hash"));
        const QString old_salt = g_fake->values.value(QStringLiteral("pin_salt"));
        g_fake->fail_writes.insert(QStringLiteral("pin_salt")); // the copy into the two rows fails

        bool done = false;
        QString error;
        pm.change_pin_async(QStringLiteral("135790"), QStringLiteral("246813"), nullptr, [&](Result<void> r) {
            if (r.is_err())
                error = QString::fromStdString(r.error());
            done = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
        QVERIFY2(error.isEmpty(), qPrintable(error)); // the change is committed by the staged record
        QVERIFY(g_fake->values.contains(QStringLiteral("pin_record_next")));
        QCOMPARE(g_fake->values.value(QStringLiteral("pin_salt")), old_salt); // two rows never mixed
        QCOMPARE(g_fake->values.value(QStringLiteral("pin_hash")), old_hash);

        // "Restart": the staged record is the PIN in force.
        pm.reload();
        QCOMPARE(pm.pin_state(), PinManager::PinState::Set);
        bool verified = false;
        done = false;
        pm.verify_pin_async(QStringLiteral("246813"), nullptr, [&](bool ok) {
            verified = ok;
            done = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
        QVERIFY(verified);
        g_fake->fail_writes.clear();
    }

    void change_pin_staging_fails_old_pin_stays() {
        seed_pin(g_fake);
        PinManager& pm = PinManager::instance();
        const QString old_hash = g_fake->values.value(QStringLiteral("pin_hash"));
        g_fake->fail_writes.insert(QStringLiteral("pin_record_next"));

        bool done = false;
        QString error;
        pm.change_pin_async(QStringLiteral("135790"), QStringLiteral("246813"), nullptr, [&](Result<void> r) {
            if (r.is_err())
                error = QString::fromStdString(r.error());
            done = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
        QVERIFY(!error.isEmpty());
        QVERIFY(!g_fake->values.contains(QStringLiteral("pin_record_next")));
        QCOMPARE(g_fake->values.value(QStringLiteral("pin_hash")), old_hash);
        g_fake->fail_writes.clear();

        pm.reload();
        bool verified = false;
        done = false;
        pm.verify_pin_async(QStringLiteral("135790"), nullptr, [&](bool ok) {
            verified = ok;
            done = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
        QVERIFY(verified); // the old PIN still unlocks
    }

    void successful_change_leaves_no_staged_row() {
        seed_pin(g_fake);
        QVERIFY(!g_fake->values.contains(QStringLiteral("pin_record_next")));
        QVERIFY(g_fake->values.contains(QStringLiteral("pin_hash")));
        QVERIFY(g_fake->values.contains(QStringLiteral("pin_salt")));
    }

    void explicit_clear_after_unreadable_reaches_not_set() {
        seed_pin(g_fake);
        g_fake->sticky.insert(QStringLiteral("pin_hash"), SecretRead::failure(SecretRead::Error::Corrupt));
        PinManager::instance().reload();
        QCOMPARE(PinManager::instance().pin_state(), PinManager::PinState::Unreadable);
        g_fake->sticky.clear();
        PinManager::instance().clear_pin(); // the re-authenticated "sign in again" path
        QCOMPARE(PinManager::instance().pin_state(), PinManager::PinState::NotSet);
    }
};

QTEST_GUILESS_MAIN(TestPinManagerStorage)
#include "tst_pin_manager_storage.moc"
