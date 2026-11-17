// SecureStorage facade (src/storage/secure/SecureStorage.cpp) over a FAKE
// backend, and the wrapped-key rule (secure_detail::decide_dek in SecretRead.h).
//
// The production backend (SecureStorageSqlite.cpp: SQLite + OpenSSL + DPAPI)
// is deliberately not linked: this target supplies make_default_secret_backend().
// What is pinned down here is the contract every caller relies on — a read
// ERROR is never reported as "nothing stored" — for the four cases that matter:
// not found, transient error then success, permanent error, corrupted blob.
#include "storage/secure/SecureStorage.h"

#include <QHash>
#include <QtTest>

#include <deque>

using namespace fincept;
using secure_detail::decide_dek;
using secure_detail::DekInputs;
using secure_detail::DekPlan;
using secure_detail::RowState;

namespace {

/// Scriptable backend: values in a map; queued failures are returned (once
/// each, in order) before the map is consulted for a key.
class FakeBackend : public ISecretBackend {
  public:
    QHash<QString, QString> values;
    QHash<QString, std::deque<SecretRead>> scripted; // key -> failures to return first
    int writes = 0;
    int erases = 0;

    SecretRead read(const QString& key) override {
        auto it = scripted.find(key);
        if (it != scripted.end() && !it->empty()) {
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
        values.insert(key, value);
        return Result<void>::ok();
    }
    Result<void> erase(const QString& key) override {
        ++erases;
        values.remove(key);
        return Result<void>::ok();
    }
    Result<void> erase_all() override {
        values.clear();
        return Result<void>::ok();
    }
};

FakeBackend* g_fake = nullptr;

FakeBackend* install_fake() {
    auto fake = std::make_unique<FakeBackend>();
    g_fake = fake.get();
    SecureStorage::instance().set_backend_for_testing(std::move(fake));
    return g_fake;
}

} // namespace

// The link seam: this test has no SQLite backend.
std::unique_ptr<ISecretBackend> fincept::make_default_secret_backend() {
    return std::make_unique<FakeBackend>();
}

class TestSecureStorage : public QObject {
    Q_OBJECT
  private slots:
    void not_found_is_definitive() {
        install_fake();
        const SecretRead r = SecureStorage::instance().read(QStringLiteral("api_key"));
        QVERIFY(r.is_not_found());
        QVERIFY(!r.failed());
        QVERIFY(!r.found());
        const auto legacy = SecureStorage::instance().retrieve(QStringLiteral("api_key"));
        QVERIFY(legacy.is_err());
        QVERIFY(SecureStorage::is_not_found(legacy));
    }

    void found_value_round_trip() {
        FakeBackend* fake = install_fake();
        QVERIFY(SecureStorage::instance().store(QStringLiteral("k"), QStringLiteral("v1")).is_ok());
        QCOMPARE(fake->writes, 1);
        const SecretRead r = SecureStorage::instance().read(QStringLiteral("k"));
        QVERIFY(r.found());
        QCOMPARE(r.value, QStringLiteral("v1"));
        QCOMPARE(SecureStorage::instance().retrieve(QStringLiteral("k")).value(), QStringLiteral("v1"));
    }

    void transient_error_then_success() {
        FakeBackend* fake = install_fake();
        fake->values.insert(QStringLiteral("pin_hash"), QStringLiteral("abcd"));
        fake->scripted[QStringLiteral("pin_hash")].push_back(
            SecretRead::failure(SecretRead::Error::Busy, QStringLiteral("database is locked")));

        const SecretRead first = SecureStorage::instance().read(QStringLiteral("pin_hash"));
        QVERIFY(first.failed());
        QVERIFY(first.transient());
        QVERIFY(!first.is_not_found()); // the defect: this used to be "Not found"
        QVERIFY(first.reason().contains(QStringLiteral("busy")));

        const SecretRead second = SecureStorage::instance().read(QStringLiteral("pin_hash"));
        QVERIFY(second.found());
        QCOMPARE(second.value, QStringLiteral("abcd"));
    }

    void transient_error_is_not_not_found_through_retrieve() {
        FakeBackend* fake = install_fake();
        fake->scripted[QStringLiteral("api_key")].push_back(SecretRead::failure(SecretRead::Error::StoreClosed));
        const auto r = SecureStorage::instance().retrieve(QStringLiteral("api_key"));
        QVERIFY(r.is_err());
        QVERIFY(!SecureStorage::is_not_found(r));
        QVERIFY(QString::fromStdString(r.error()).startsWith(QStringLiteral("[secure-read:store_closed]")));
    }

    void permanent_error() {
        FakeBackend* fake = install_fake();
        fake->scripted[QStringLiteral("api_key")].push_back(
            SecretRead::failure(SecretRead::Error::KeyUnavailable, QStringLiteral("DPAPI refused")));
        const SecretRead r = SecureStorage::instance().read(QStringLiteral("api_key"));
        QVERIFY(r.failed());
        QVERIFY(!r.transient());
        QCOMPARE(QString::fromLatin1(r.error_name()), QStringLiteral("key_unavailable"));
        QVERIFY(!r.reason().isEmpty());
        QVERIFY(!r.detail.contains(QStringLiteral("value"))); // detail never carries a secret value
    }

    void corrupted_blob() {
        FakeBackend* fake = install_fake();
        fake->scripted[QStringLiteral("session_token")].push_back(
            SecretRead::failure(SecretRead::Error::Corrupt, QStringLiteral("authentication tag mismatch")));
        const SecretRead r = SecureStorage::instance().read(QStringLiteral("session_token"));
        QVERIFY(r.failed());
        QVERIFY(!r.transient());
        QVERIFY(!r.is_not_found());
        QCOMPARE(r.error, SecretRead::Error::Corrupt);
    }

    void generation_moves_on_every_change() {
        install_fake();
        auto& ss = SecureStorage::instance();
        const quint64 g0 = ss.generation();
        QVERIFY(ss.store(QStringLiteral("a"), QStringLiteral("1")).is_ok());
        const quint64 g1 = ss.generation();
        QVERIFY(g1 > g0);
        QVERIFY(ss.remove(QStringLiteral("a")).is_ok());
        QVERIFY(ss.generation() > g1);
        const quint64 g2 = ss.generation();
        QVERIFY(ss.reset_all().is_ok());
        QVERIFY(ss.generation() > g2);
        (void)ss.read(QStringLiteral("a")); // a read changes nothing
        QCOMPARE(ss.generation(), g2 + 1);
    }

    // ── The wrapped-key rule ────────────────────────────────────────────────
    void dek_created_only_when_both_rows_read_and_absent() {
        DekInputs in;
        in.dpapi_supported = true;
        QCOMPARE(decide_dek(in).plan, DekPlan::CreateNew);
        in.dpapi_supported = false;
        QCOMPARE(decide_dek(in).plan, DekPlan::CreateNew);
    }

    void dek_read_error_never_creates() {
        for (bool win : {true, false}) {
            DekInputs in;
            in.dpapi_supported = win;
            in.pbkdf2 = RowState::ReadError;
            const auto d = decide_dek(in);
            QCOMPARE(d.plan, DekPlan::Fail);
            QCOMPARE(d.error, SecretRead::Error::Io);
            in.pbkdf2 = RowState::Absent;
            in.dpapi = RowState::ReadError;
            QCOMPARE(decide_dek(in).plan, DekPlan::Fail);
        }
    }

    void dek_present_but_not_unwrappable_never_creates() {
        DekInputs in;
        in.dpapi_supported = true;
        in.dpapi = RowState::Present;
        in.dpapi_unwrapped = false; // DPAPI refused (other account / transient)
        auto d = decide_dek(in);
        QCOMPARE(d.plan, DekPlan::Fail);
        QCOMPARE(d.error, SecretRead::Error::KeyUnavailable);

        DekInputs linux_in; // profile copied from Windows to Linux
        linux_in.dpapi = RowState::Present;
        d = decide_dek(linux_in);
        QCOMPARE(d.plan, DekPlan::Fail);
        QCOMPARE(d.error, SecretRead::Error::KeyUnavailable);

        DekInputs pb; // machine seed changed
        pb.pbkdf2 = RowState::Present;
        pb.pbkdf2_unwrapped = false;
        QCOMPARE(decide_dek(pb).plan, DekPlan::Fail);
    }

    void dek_uses_what_unwraps() {
        DekInputs in;
        in.dpapi_supported = true;
        in.dpapi = RowState::Present;
        in.dpapi_unwrapped = true;
        in.pbkdf2 = RowState::Present;
        QCOMPARE(decide_dek(in).plan, DekPlan::UseDpapi);

        DekInputs pb;
        pb.dpapi_supported = true;
        pb.dpapi = RowState::Present; // DPAPI copy refused…
        pb.pbkdf2 = RowState::Present;
        pb.pbkdf2_unwrapped = true; // …but the PBKDF2 copy works
        QCOMPARE(decide_dek(pb).plan, DekPlan::UsePbkdf2);
    }
};

QTEST_GUILESS_MAIN(TestSecureStorage)
#include "tst_secure_storage.moc"
