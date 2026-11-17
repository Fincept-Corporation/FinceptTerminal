// SecureStorage read errors at call sites fixed in the core-adoption pass, over the
// fake secret backend (the same seam as tests/tst_secure_read_callers.cpp; the SQLite
// backend is not linked). A read ERROR is never "nothing stored":
//   * src/services/prediction/PredictionCredentialStore.cpp
//       unreadable -> nullopt WITH a reason (callers keep what they hold), nothing
//       written or erased; absent -> nullopt without a reason; a later read returns
//       the untouched stored set.
// Test-only values; no real credential is used.
#include "services/prediction/PredictionCredentialStore.h"
#include "storage/secure/SecureStorage.h"

#include <QHash>
#include <QtTest>

#include <deque>

using namespace fincept;
namespace pred = fincept::services::prediction;

namespace {

/// Scriptable backend: queued results are returned (once each, in order) before
/// the map is consulted for a key. Counts writes and erases.
class FakeBackend : public ISecretBackend {
  public:
    QHash<QString, QString> values;
    QHash<QString, std::deque<SecretRead>> scripted;
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

FakeBackend* install_fake() {
    auto fake = std::make_unique<FakeBackend>();
    FakeBackend* raw = fake.get();
    SecureStorage::instance().set_backend_for_testing(std::move(fake));
    return raw;
}

SecretRead busy() {
    return SecretRead::failure(SecretRead::Error::Busy, QStringLiteral("database is locked"));
}

SecretRead corrupt() {
    return SecretRead::failure(SecretRead::Error::Corrupt, QStringLiteral("tag mismatch"));
}

const QString kPolyKey = QStringLiteral("prediction/polymarket/credentials");
const QString kKalshiKey = QStringLiteral("prediction/kalshi/credentials");
const QString kPolyBlob = QStringLiteral("{\"private_key\":\"test-only-key\",\"signature_type\":1}");
const QString kKalshiBlob = QStringLiteral("{\"api_key_id\":\"test-id\",\"private_key_pem\":\"test-pem\"}");

} // namespace

// The link seam: no SQLite backend in this test.
std::unique_ptr<ISecretBackend> fincept::make_default_secret_backend() {
    return std::make_unique<FakeBackend>();
}

class TestSecureReadSites : public QObject {
    Q_OBJECT

  private slots:
    void polymarket_unreadable_is_not_absent();
    void polymarket_absent_has_no_reason();
    void kalshi_unreadable_is_not_absent();
    void reason_cleared_on_next_good_read();
};

void TestSecureReadSites::polymarket_unreadable_is_not_absent() {
    FakeBackend* fake = install_fake();
    fake->values.insert(kPolyKey, kPolyBlob);
    fake->scripted[kPolyKey].push_back(busy());

    QString reason;
    const auto creds = pred::PredictionCredentialStore::load_polymarket(&reason);
    QVERIFY(!creds.has_value());
    QVERIFY2(!reason.isEmpty(), "an unreadable set must come with a reason, not pass for 'none stored'");
    QCOMPARE(fake->writes, 0);
    QCOMPARE(fake->erases, 0);

    // The stored set is untouched: the next read returns it.
    const auto again = pred::PredictionCredentialStore::load_polymarket(&reason);
    QVERIFY(again.has_value());
    QVERIFY(reason.isEmpty());
    QCOMPARE(again->private_key, QStringLiteral("test-only-key"));
}

void TestSecureReadSites::polymarket_absent_has_no_reason() {
    FakeBackend* fake = install_fake();
    QString reason = QStringLiteral("stale");
    const auto creds = pred::PredictionCredentialStore::load_polymarket(&reason);
    QVERIFY(!creds.has_value());
    QVERIFY(reason.isEmpty()); // definitively absent: no error
    QCOMPARE(fake->writes, 0);
}

void TestSecureReadSites::kalshi_unreadable_is_not_absent() {
    FakeBackend* fake = install_fake();
    fake->values.insert(kKalshiKey, kKalshiBlob);
    fake->scripted[kKalshiKey].push_back(corrupt());

    QString reason;
    const auto creds = pred::PredictionCredentialStore::load_kalshi(&reason);
    QVERIFY(!creds.has_value());
    QVERIFY(!reason.isEmpty());
    QCOMPARE(fake->writes, 0);
    QCOMPARE(fake->erases, 0);
    QVERIFY(fake->values.contains(kKalshiKey));
}

void TestSecureReadSites::reason_cleared_on_next_good_read() {
    FakeBackend* fake = install_fake();
    fake->values.insert(kKalshiKey, kKalshiBlob);
    fake->scripted[kKalshiKey].push_back(busy());

    QString reason;
    QVERIFY(!pred::PredictionCredentialStore::load_kalshi(&reason).has_value());
    QVERIFY(!reason.isEmpty());
    const auto creds = pred::PredictionCredentialStore::load_kalshi(&reason);
    QVERIFY(creds.has_value());
    QVERIFY(reason.isEmpty());
    QCOMPARE(creds->api_key_id, QStringLiteral("test-id"));
}

QTEST_GUILESS_MAIN(TestSecureReadSites)
#include "tst_secure_read_sites.moc"
