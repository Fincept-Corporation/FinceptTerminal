#include "core/config/AppConfig.h"
#include "core/config/ProfileManager.h"
#include "multiuser/client/PhaseOneEndpointProvider.h"

#include <QtTest/QtTest>

using namespace fincept;
using namespace fincept::multiuser;

namespace {

class PhaseOneEndpointPrecedenceTest : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();
    void env_override_wins_config();
    void config_value_is_profile_scoped();
    void config_value_is_trimmed_and_blank_removes_setting();
    void normalizes_host_only_values_and_trims_trailing_slash();
    void accepts_explicit_scheme_host_port();
    void rejects_unset_and_invalid_values();
};

void PhaseOneEndpointPrecedenceTest::init() {
    qunsetenv("FINCEPT_PHASE1_SERVER_URL");
    ProfileManager::instance().set_active("default");
    AppConfig::instance().remove_phase_one_server_url();
}

void PhaseOneEndpointPrecedenceTest::cleanup() {
    qunsetenv("FINCEPT_PHASE1_SERVER_URL");
    ProfileManager::instance().set_active("default");
    AppConfig::instance().remove_phase_one_server_url();
}

void PhaseOneEndpointPrecedenceTest::env_override_wins_config() {
    AppConfig::instance().set_phase_one_server_url("https://configured.example:45450/");
    qputenv("FINCEPT_PHASE1_SERVER_URL", "phase1.internal");

    const auto resolved = PhaseOneEndpointProvider::instance().resolve();
    QVERIFY(resolved.ok);
    QCOMPARE(resolved.base_url, QString("http://phase1.internal:45450"));
}

void PhaseOneEndpointPrecedenceTest::config_value_is_profile_scoped() {
    ProfileManager::instance().set_active("alpha");
    AppConfig::instance().remove_phase_one_server_url();
    AppConfig::instance().set_phase_one_server_url("alpha-host");
    QCOMPARE(AppConfig::instance().get_phase_one_server_url(), QString("alpha-host"));

    ProfileManager::instance().set_active("beta");
    AppConfig::instance().remove_phase_one_server_url();
    QVERIFY(AppConfig::instance().get_phase_one_server_url().isEmpty());
    AppConfig::instance().set_phase_one_server_url("beta-host");

    ProfileManager::instance().set_active("alpha");
    QCOMPARE(AppConfig::instance().get_phase_one_server_url(), QString("alpha-host"));
    ProfileManager::instance().set_active("beta");
    QCOMPARE(AppConfig::instance().get_phase_one_server_url(), QString("beta-host"));
}

void PhaseOneEndpointPrecedenceTest::config_value_is_trimmed_and_blank_removes_setting() {
    AppConfig::instance().set_phase_one_server_url("  phase1.internal:45450/  ");
    QCOMPARE(AppConfig::instance().get_phase_one_server_url(), QString("phase1.internal:45450/"));

    AppConfig::instance().set_phase_one_server_url("   ");
    QVERIFY(AppConfig::instance().get_phase_one_server_url().isEmpty());
}

void PhaseOneEndpointPrecedenceTest::normalizes_host_only_values_and_trims_trailing_slash() {
    AppConfig::instance().set_phase_one_server_url("server-box/");

    const auto resolved = PhaseOneEndpointProvider::instance().resolve();
    QVERIFY(resolved.ok);
    QCOMPARE(resolved.base_url, QString("http://server-box:45450"));
}

void PhaseOneEndpointPrecedenceTest::accepts_explicit_scheme_host_port() {
    AppConfig::instance().set_phase_one_server_url("https://localhost:8443/");

    const auto resolved = PhaseOneEndpointProvider::instance().resolve();
    QVERIFY(resolved.ok);
    QCOMPARE(resolved.base_url, QString("https://localhost:8443"));
}

void PhaseOneEndpointPrecedenceTest::rejects_unset_and_invalid_values() {
    auto resolved = PhaseOneEndpointProvider::instance().resolve();
    QVERIFY(!resolved.ok);
    QCOMPARE(resolved.error_code, QString("server_endpoint_unset"));

    AppConfig::instance().set_phase_one_server_url("https://api.fincept.in");
    resolved = PhaseOneEndpointProvider::instance().resolve();
    QVERIFY(!resolved.ok);

    AppConfig::instance().set_phase_one_server_url("http://localhost");
    resolved = PhaseOneEndpointProvider::instance().resolve();
    QVERIFY(!resolved.ok);

    AppConfig::instance().set_phase_one_server_url("localhost:45450");
    resolved = PhaseOneEndpointProvider::instance().resolve();
    QVERIFY(!resolved.ok);
}

} // namespace

QTEST_APPLESS_MAIN(PhaseOneEndpointPrecedenceTest)

#include "test_phase_one_endpoint_precedence.moc"
