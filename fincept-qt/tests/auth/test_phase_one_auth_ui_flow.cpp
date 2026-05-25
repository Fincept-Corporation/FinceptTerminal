#include "auth/AuthTypes.h"
#include "auth/PhaseOneAuthFlowBridge.h"
#include "auth/PhaseOneAuthRecoveryBridge.h"
#include "screens/auth/PhaseOneBootstrapScreen.h"
#include "screens/info/HelpScreen.h"

#include <QJsonDocument>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QtTest>

namespace {

class PhaseOneAuthUiFlowSmokeTest : public QObject {
    Q_OBJECT

  private slots:
    void login_bridge_uses_test_seam_for_successful_phase_one_auth();
    void bootstrap_status_routes_to_bootstrap_screen();
    void successful_bootstrap_transitions_back_to_login();
    void auth_entry_help_omits_legacy_auth_navigation();
    void startup_persistence_regression_still_requires_login();
};

void PhaseOneAuthUiFlowSmokeTest::login_bridge_uses_test_seam_for_successful_phase_one_auth() {
    using fincept::auth::ApiResponse;
    using fincept::auth::PhaseOneAuthFlowBridge;

    PhaseOneAuthFlowBridge::set_test_login_handler_for_tests([](const QString& username,
                                                                const QString& password) -> ApiResponse {
        if (username != QStringLiteral("alice") || password != QStringLiteral("secret"))
            return {false, {}, QStringLiteral("Unexpected credentials."), 400, {}};

        ApiResponse response;
        response.success = true;
        response.status_code = 200;
        response.data = QJsonObject{{"user_id", 7},
                                    {"username", "alice"},
                                    {"role", "user"},
                                    {"session_id", "sess-7"},
                                    {"expires_at", "2026-05-30T12:00:00Z"}};
        return response;
    });

    ApiResponse response;
    PhaseOneAuthFlowBridge::submit_login("alice", "secret", [&response](ApiResponse r) { response = std::move(r); });
    QVERIFY(response.success);
    QCOMPARE(response.data.value("session_id").toString(), QStringLiteral("sess-7"));

    PhaseOneAuthFlowBridge::reset_test_login_handler_for_tests();
}

void PhaseOneAuthUiFlowSmokeTest::bootstrap_status_routes_to_bootstrap_screen() {
    using fincept::auth::ApiResponse;
    using fincept::auth::PhaseOneAuthFlowBridge;

    PhaseOneAuthFlowBridge::set_test_bootstrap_status_handler_for_tests([]() -> ApiResponse {
        return {true, QJsonObject{{"bootstrap_open", true}}, {}, 200, {}};
    });

    int route = -1;
    PhaseOneAuthFlowBridge::resolve_auth_entry_route([&route](int resolved) { route = resolved; });
    QCOMPARE(route, PhaseOneAuthFlowBridge::kBootstrapAuthStackIndex);

    PhaseOneAuthFlowBridge::reset_test_bootstrap_status_handler_for_tests();
}

void PhaseOneAuthUiFlowSmokeTest::successful_bootstrap_transitions_back_to_login() {
    using fincept::auth::ApiResponse;
    using fincept::auth::PhaseOneAuthFlowBridge;

    PhaseOneAuthFlowBridge::set_test_bootstrap_handler_for_tests([](const QString& username,
                                                                    const QString& password) -> ApiResponse {
        if (username != QStringLiteral("admin") || password != QStringLiteral("secret"))
            return {false, {}, QStringLiteral("Unexpected bootstrap credentials."), 400, {}};
        return {true, {}, {}, 200, {}};
    });

    fincept::screens::PhaseOneBootstrapScreen screen;
    const auto edits = screen.findChildren<QLineEdit*>();
    QVERIFY(edits.size() >= 2);
    edits.at(0)->setText(QStringLiteral("admin"));
    edits.at(1)->setText(QStringLiteral("secret"));

    QSignalSpy success_spy(&screen, &fincept::screens::PhaseOneBootstrapScreen::bootstrap_succeeded);
    QSignalSpy login_spy(&screen, &fincept::screens::PhaseOneBootstrapScreen::navigate_login);

    auto* button = screen.findChild<QPushButton*>();
    QVERIFY(button);
    QTest::mouseClick(button, Qt::LeftButton);

    QCOMPARE(success_spy.count(), 1);
    QCOMPARE(login_spy.count(), 1);

    PhaseOneAuthFlowBridge::set_test_bootstrap_status_handler_for_tests([]() -> ApiResponse {
        return {true, QJsonObject{{"bootstrap_open", false}}, {}, 200, {}};
    });
    int route = -1;
    PhaseOneAuthFlowBridge::resolve_auth_entry_route([&route](int resolved) { route = resolved; });
    QCOMPARE(route, PhaseOneAuthFlowBridge::kLoginAuthStackIndex);

    PhaseOneAuthFlowBridge::reset_test_bootstrap_handler_for_tests();
    PhaseOneAuthFlowBridge::reset_test_bootstrap_status_handler_for_tests();
}

void PhaseOneAuthUiFlowSmokeTest::auth_entry_help_omits_legacy_auth_navigation() {
    fincept::screens::HelpScreen help(/*auth_entry_mode=*/true);
    const auto buttons = help.findChildren<QPushButton*>();

    bool found_register = false;
    bool found_reset = false;
    for (QPushButton* button : buttons) {
        const QString text = button->text().toLower();
        found_register = found_register || text.contains("create account");
        found_reset = found_reset || text.contains("reset password");
    }

    QVERIFY(!found_register);
    QVERIFY(!found_reset);
}

void PhaseOneAuthUiFlowSmokeTest::startup_persistence_regression_still_requires_login() {
    const QString legacy_session = QString::fromUtf8(
        QJsonDocument(QJsonObject{{"authenticated", true},
                                  {"api_key", "legacy-key"},
                                  {"session_token", "legacy-session"},
                                  {"user_info", QJsonObject{{"username", "legacy-user"}}}})
            .toJson(QJsonDocument::Compact));

    QVERIFY(!fincept::auth::PhaseOneAuthRecoveryBridge::should_restore_startup_auth(
        legacy_session, QStringLiteral("legacy-settings-key"), QStringLiteral("legacy-secure-key")));
}

} // namespace

QTEST_MAIN(PhaseOneAuthUiFlowSmokeTest)

#include "test_phase_one_auth_ui_flow.moc"
