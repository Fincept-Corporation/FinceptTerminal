// Callers of SecureStorage::read() that must never treat a read ERROR as
// "nothing stored", over the fake secret backend (the same seam as
// tests/tst_secure_storage.cpp; the SQLite backend is not linked):
//   * src/services/connectors/ConnectorCatalog.cpp      key presence: Unreadable, retried, never "missing"
//   * src/services/connectors/ConnectorCredentialBridge  never copies over a slot it could not read
//   * src/ui/widgets/Requirements.cpp                    "Could not read secure storage: … — RETRY", no key prompt
//   * src/mcp/server/McpTokenPolicy.h                    the MCP bearer token is never re-minted after a failed read
#include "mcp/server/McpTokenPolicy.h"
#include "services/connectors/ConnectorCatalog.h"
#include "services/connectors/ConnectorCredentialBridge.h"
#include "storage/secure/SecureStorage.h"
#include "ui/widgets/Requirements.h"

#include <QHash>
#include <QSet>
#include <QtTest>

#include <deque>

using namespace fincept;
namespace conn = fincept::services::connectors;
namespace tok = fincept::mcp::mcp_token;

namespace {

/// Scriptable backend: queued results are returned (once each, in order) before
/// the map is consulted for a key.
class FakeBackend : public ISecretBackend {
  public:
    QHash<QString, QString> values;
    QHash<QString, std::deque<SecretRead>> scripted;
    int writes = 0;

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

class FakeEnv final : public ui::RequirementEnvironment {
  public:
    QSet<QString> secrets;
    QHash<QString, QString> unreadable; ///< env key -> reason
    bool signed_in() const override { return true; }
    bool has_secret(const QString& k) const override { return secrets.contains(k); }
    ui::SecretPresence secret_presence(const QString& k, QString* reason) const override {
        if (unreadable.contains(k)) {
            if (reason)
                *reason = unreadable.value(k);
            return ui::SecretPresence::Unreadable;
        }
        return has_secret(k) ? ui::SecretPresence::Present : ui::SecretPresence::Absent;
    }
    bool llm_configured() const override { return true; }
    bool python_ready() const override { return true; }
    bool broker_connected(const QString&) const override { return true; }
    double credit_balance() const override { return -1; }
};

/// An environment written before secret_presence() existed: only has_secret().
class LegacyEnv final : public ui::RequirementEnvironment {
  public:
    bool signed_in() const override { return true; }
    bool has_secret(const QString& k) const override { return k == QLatin1String("FRED_API_KEY"); }
    bool llm_configured() const override { return true; }
    bool python_ready() const override { return true; }
    bool broker_connected(const QString&) const override { return true; }
    double credit_balance() const override { return -1; }
};

} // namespace

// The link seam: no SQLite backend in this test.
std::unique_ptr<ISecretBackend> fincept::make_default_secret_backend() {
    return std::make_unique<FakeBackend>();
}

class TestSecureReadCallers : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase() {
        // A key exported in the shell would count as present: keep them out.
        for (const char* k : {"FINNHUB_API_KEY", "FRED_API_KEY", "POLYGON_API_KEY"})
            qunsetenv(k);
    }

    // ── ConnectorCatalog ─────────────────────────────────────────────────────
    void presence_rule() {
        QCOMPARE(conn::ConnectorCatalog::presence_of(false, SecretRead::found_value(QStringLiteral("k1"))),
                 conn::KeyPresence::Present);
        QCOMPARE(conn::ConnectorCatalog::presence_of(false, SecretRead::found_value(QStringLiteral("  "))),
                 conn::KeyPresence::Absent);
        QCOMPARE(conn::ConnectorCatalog::presence_of(false, SecretRead::not_found()), conn::KeyPresence::Absent);
        QCOMPARE(conn::ConnectorCatalog::presence_of(false, busy()), conn::KeyPresence::Unreadable);
        // An exported variable reaches Python whatever the store says.
        QCOMPARE(conn::ConnectorCatalog::presence_of(true, busy()), conn::KeyPresence::Present);
    }

    void unreadable_key_is_not_missing_and_is_retried() {
        FakeBackend* fake = install_fake();
        auto& cat = conn::ConnectorCatalog::instance();
        const QString key = QStringLiteral("POLYGON_API_KEY");
        fake->values.insert(key, QStringLiteral("pk_live_123456"));
        fake->scripted[key].push_back(busy());

        QString why;
        QCOMPARE(cat.check_key(key, &why), conn::KeyPresence::Unreadable);
        QVERIFY(why.contains(QStringLiteral("busy")));

        conn::ConnectorInfo c;
        c.name = QStringLiteral("data_polygon");
        c.env_keys = {key};
        c.key_required = true;
        // Cached state after the failed read: Unreadable with its reason, not Missing.
        QCOMPARE(cat.key_state(c), conn::KeyState::Unreadable);
        QVERIFY(cat.key_state_reason(c).contains(QStringLiteral("busy")));
        const std::optional<bool> cached = cat.cached_key_present(key);
        QVERIFY(cached.has_value() && !*cached);

        // The next check reads again (an Unreadable result is never kept as the answer).
        QCOMPARE(cat.check_key(key), conn::KeyPresence::Present);
        QCOMPARE(cat.key_state(c), conn::KeyState::Ready);
        QVERIFY(cat.key_state_reason(c).isEmpty());
        QVERIFY(cat.key_present(key));
    }

    void absent_key_is_missing() {
        install_fake();
        auto& cat = conn::ConnectorCatalog::instance();
        const QString key = QStringLiteral("FRED_API_KEY");
        cat.set_key_present(key, false);
        cat.set_key_present(key, true);
        cat.set_key_present(key, false); // what Settings does after removing it
        conn::ConnectorInfo c;
        c.env_keys = {key};
        c.key_required = true;
        QCOMPARE(cat.key_state(c), conn::KeyState::Missing);
    }

    // ── ConnectorCredentialBridge ────────────────────────────────────────────
    void bridge_never_writes_over_an_unreadable_slot() {
        FakeBackend* fake = install_fake();
        const QString key = QStringLiteral("FINNHUB_API_KEY");
        fake->values.insert(key, QStringLiteral("users-own-key"));
        fake->scripted[key].push_back(busy());
        const QStringList filled = conn::bridge_connection_credentials(
            QStringLiteral("finnhub"), QStringLiteral("{\"apiKey\":\"connection-key\"}"));
        QVERIFY(filled.isEmpty());
        QCOMPARE(fake->writes, 0);
        QCOMPARE(fake->values.value(key), QStringLiteral("users-own-key"));
    }

    void bridge_fills_an_empty_slot_and_keeps_a_filled_one() {
        FakeBackend* fake = install_fake();
        const QString key = QStringLiteral("FINNHUB_API_KEY");
        QStringList filled = conn::bridge_connection_credentials(QStringLiteral("finnhub"),
                                                                 QStringLiteral("{\"apiKey\":\"connection-key\"}"));
        QCOMPARE(filled, QStringList{key});
        QCOMPARE(fake->writes, 1);
        QCOMPARE(fake->values.value(key), QStringLiteral("connection-key"));

        filled = conn::bridge_connection_credentials(QStringLiteral("finnhub"),
                                                     QStringLiteral("{\"apiKey\":\"another-key\"}"));
        QVERIFY(filled.isEmpty());
        QCOMPARE(fake->writes, 1);
        QCOMPARE(fake->values.value(key), QStringLiteral("connection-key"));
    }

    // ── Requirements ─────────────────────────────────────────────────────────
    void requirement_unreadable_says_retry_not_add_key() {
        FakeEnv env;
        env.unreadable.insert(QStringLiteral("FRED_API_KEY"), QStringLiteral("the terminal database is busy"));
        const ui::RequirementStatus st =
            ui::check_requirements({ui::Requirement::api_key(QStringLiteral("FRED_API_KEY"), QStringLiteral("FRED"))},
                                   env);
        QVERIFY(!st.ok);
        QVERIFY(st.retry);
        QVERIFY(st.message.startsWith(QStringLiteral("Could not read secure storage: the terminal database is busy")));
        QVERIFY(!st.message.contains(QStringLiteral("Add FRED_API_KEY")));
        QCOMPARE(st.action_text, QStringLiteral("RETRY"));
        QVERIFY(!st.link.valid()); // nothing to open: the action re-checks
    }

    void requirement_absent_still_asks_for_the_key() {
        FakeEnv env;
        const ui::RequirementStatus st =
            ui::check_requirements({ui::Requirement::api_key(QStringLiteral("FRED_API_KEY"), QStringLiteral("FRED"))},
                                   env);
        QVERIFY(!st.ok);
        QVERIFY(!st.retry);
        QCOMPARE(st.link.section, QStringLiteral("credentials"));
        env.secrets.insert(QStringLiteral("FRED_API_KEY"));
        QVERIFY(ui::check_requirements({ui::Requirement::api_key(QStringLiteral("FRED_API_KEY"), QString())}, env).ok);
    }

    void requirement_legacy_environment_keeps_working() {
        LegacyEnv env; // only has_secret(): the default secret_presence() derives from it
        QVERIFY(ui::check_requirements({ui::Requirement::api_key(QStringLiteral("FRED_API_KEY"), QString())}, env).ok);
        const auto st =
            ui::check_requirements({ui::Requirement::api_key(QStringLiteral("POLYGON_API_KEY"), QString())}, env);
        QVERIFY(!st.ok);
        QVERIFY(!st.retry);
    }

    // ── MCP bearer token ─────────────────────────────────────────────────────
    void token_found_is_used() {
        FakeBackend* fake = install_fake();
        fake->values.insert(QStringLiteral("mcp_server/token"), QStringLiteral("  abc123  "));
        const auto d = tok::decide(SecureStorage::instance().read(QStringLiteral("mcp_server/token")));
        QCOMPARE(d.plan, tok::TokenPlan::Use);
        QCOMPARE(d.token, QStringLiteral("abc123"));
    }

    void token_created_only_when_read_and_absent() {
        install_fake();
        QCOMPARE(tok::decide(SecureStorage::instance().read(QStringLiteral("mcp_server/token"))).plan,
                 tok::TokenPlan::Create);
        QCOMPARE(tok::decide(SecretRead::found_value(QStringLiteral(" "))).plan, tok::TokenPlan::Create);
    }

    void token_read_error_never_mints() {
        const SecretRead::Error kinds[] = {SecretRead::Error::StoreClosed, SecretRead::Error::Busy,
                                           SecretRead::Error::Io,          SecretRead::Error::KeyUnavailable,
                                           SecretRead::Error::Corrupt,     SecretRead::Error::Crypto};
        for (SecretRead::Error e : kinds) {
            FakeBackend* fake = install_fake();
            fake->scripted[QStringLiteral("mcp_server/token")].push_back(SecretRead::failure(e));
            const auto d = tok::decide(SecureStorage::instance().read(QStringLiteral("mcp_server/token")));
            QCOMPARE(d.plan, tok::TokenPlan::Wait);
            QVERIFY(d.token.isEmpty());
            QVERIFY(!d.reason.isEmpty());
            QCOMPARE(fake->writes, 0);
        }
        QVERIFY(tok::decide(busy()).transient);
        QVERIFY(!tok::decide(SecretRead::failure(SecretRead::Error::KeyUnavailable)).transient);
    }

    void token_retry_back_off() {
        QCOMPARE(tok::retry_delay_ms(1), 5000);
        QCOMPARE(tok::retry_delay_ms(2), 10000);
        QCOMPARE(tok::retry_delay_ms(3), 20000);
        QCOMPARE(tok::retry_delay_ms(7), 300000); // 320 s capped at 5 min
        QCOMPARE(tok::retry_delay_ms(100), 300000);
        QCOMPARE(tok::retry_delay_ms(0), 5000);
    }
};

QTEST_GUILESS_MAIN(TestSecureReadCallers)
#include "tst_secure_read_callers.moc"
