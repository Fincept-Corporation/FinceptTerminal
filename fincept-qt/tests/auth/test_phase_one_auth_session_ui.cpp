#include "auth/AuthTypes.h"
#include "auth/PhaseOneAuthFlowBridge.h"
#include "auth/PhaseOneSessionAuthBridge.h"
#include "multiuser/client/PhaseOneSessionStateGuard.h"

#include <QJsonDocument>
#include <QtTest/QtTest>

namespace {

class PhaseOneAuthSessionUiSmokeTest : public QObject {
    Q_OBJECT

  private slots:
    void register_and_recovery_routes_collapse_to_login();
    void phase_one_sessions_bypass_paid_plan_gate();
    void startup_auth_restore_is_disabled_for_all_persisted_credentials();
    void provider_credentials_stay_separate_from_phase_one_app_sessions();
    void invalid_session_handler_only_clears_auth_for_revoked_sessions();
};

void PhaseOneAuthSessionUiSmokeTest::register_and_recovery_routes_collapse_to_login() {
    QCOMPARE(fincept::auth::PhaseOneAuthFlowBridge::normalize_auth_stack_index(0), 0);
    QCOMPARE(fincept::auth::PhaseOneAuthFlowBridge::normalize_auth_stack_index(1), 0);
    QCOMPARE(fincept::auth::PhaseOneAuthFlowBridge::normalize_auth_stack_index(2), 0);
    QCOMPARE(fincept::auth::PhaseOneAuthFlowBridge::normalize_auth_stack_index(4), 4);
}

void PhaseOneAuthSessionUiSmokeTest::phase_one_sessions_bypass_paid_plan_gate() {
    fincept::auth::SessionData phase_one;
    phase_one.authenticated = true;
    phase_one.session_id = "phase-one-session";
    phase_one.session_token = "phase-one-session";

    QVERIFY(fincept::auth::PhaseOneAuthFlowBridge::allows_terminal_entry(phase_one));

    fincept::auth::SessionData hosted_free;
    hosted_free.authenticated = true;
    hosted_free.api_key = "hosted-key";
    hosted_free.subscription.account_type = "free";

    QVERIFY(!fincept::auth::PhaseOneAuthFlowBridge::allows_terminal_entry(hosted_free));
}

void PhaseOneAuthSessionUiSmokeTest::startup_auth_restore_is_disabled_for_all_persisted_credentials() {
    const auto phase_one_json = QJsonObject{{"authenticated", true},
                                            {"session_id", "phase-one-session"},
                                            {"session_token", "phase-one-session"}};

    QVERIFY(!fincept::auth::PhaseOneSessionAuthBridge::should_restore_startup_auth(
        QString::fromUtf8(QJsonDocument(phase_one_json).toJson(QJsonDocument::Compact)),
        QStringLiteral("legacy-settings-key"), QStringLiteral("legacy-secure-key")));
}

void PhaseOneAuthSessionUiSmokeTest::provider_credentials_stay_separate_from_phase_one_app_sessions() {
    fincept::auth::SessionData session;
    session.authenticated = true;
    session.username = QStringLiteral("alice");
    session.session_id = QStringLiteral("phase-one-session");
    session.session_token = QStringLiteral("phase-one-session");

    QCOMPARE(fincept::auth::PhaseOneSessionAuthBridge::resolve_fincept_provider_api_key(
                  session, QStringLiteral("provider-key"), QStringLiteral("secure-provider-key"), QStringLiteral("alice")),
             QStringLiteral("provider-key"));

    session.username = QStringLiteral("bob");
    QCOMPARE(fincept::auth::PhaseOneSessionAuthBridge::resolve_fincept_provider_api_key(
                  session, QStringLiteral("provider-key"), QStringLiteral("secure-provider-key"), QStringLiteral("alice")),
             QString());
}

void PhaseOneAuthSessionUiSmokeTest::invalid_session_handler_only_clears_auth_for_revoked_sessions() {
    fincept::auth::ApiResponse revoked;
    revoked.success = false;
    revoked.status_code = 401;
    revoked.error_code = QStringLiteral("session_revoked");

    QVERIFY(fincept::multiuser::PhaseOneSessionStateGuard::should_clear_auth_state(revoked));

    fincept::auth::ApiResponse forbidden;
    forbidden.success = false;
    forbidden.status_code = 403;
    forbidden.error_code = QStringLiteral("forbidden");

    QVERIFY(!fincept::multiuser::PhaseOneSessionStateGuard::should_clear_auth_state(forbidden));
}

} // namespace

QTEST_MAIN(PhaseOneAuthSessionUiSmokeTest)

#include "test_phase_one_auth_session_ui.moc"
