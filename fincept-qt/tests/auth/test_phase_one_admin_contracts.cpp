#include "multiuser/contracts/PhaseOneAuditTypes.h"
#include "multiuser/contracts/PhaseOneUserAdminTypes.h"

#include <QtTest/QtTest>

using namespace fincept::multiuser;

namespace {

class PhaseOneAdminContractsTest : public QObject {
    Q_OBJECT

  private slots:
    void bootstrap_and_user_admin_requests_serialize_expected_fields();
    void user_list_response_parses_users_array();
    void audit_filters_and_response_use_fixed_keys();
};

void PhaseOneAdminContractsTest::bootstrap_and_user_admin_requests_serialize_expected_fields() {
    PhaseOneBootstrapRequest bootstrap{"admin", "secret"};
    QCOMPARE(bootstrap.to_json().value("username").toString(), QString("admin"));
    QCOMPARE(bootstrap.to_json().value("password").toString(), QString("secret"));

    PhaseOneCreateUserRequest create_user{"analyst"};
    QCOMPARE(create_user.to_json().value("username").toString(), QString("analyst"));

    PhaseOneSetInitialPasswordRequest set_password{7, "initial-pass"};
    QCOMPARE(set_password.to_json().value("user_id").toInt(), 7);
    QCOMPARE(set_password.to_json().value("password").toString(), QString("initial-pass"));

    PhaseOneDisableUserRequest disable{8};
    QCOMPARE(disable.to_json().value("user_id").toInt(), 8);

    PhaseOneTransferAdminRequest transfer{9};
    QCOMPARE(transfer.to_json().value("target_user_id").toInt(), 9);
}

void PhaseOneAdminContractsTest::user_list_response_parses_users_array() {
    const QJsonArray users{QJsonObject{{"user_id", 1},
                                       {"username", "admin"},
                                       {"role", "admin"},
                                       {"status", "active"}},
                           QJsonObject{{"user_id", 2},
                                       {"username", "analyst"},
                                       {"role", "standard"},
                                       {"status", "disabled"}}};
    const QJsonObject json{{"users", users}};

    const auto response = PhaseOneUserListResponse::from_json(json);
    QCOMPARE(response.users.size(), 2);
    QCOMPARE(response.users.at(0).role, QString("admin"));
    QCOMPARE(response.users.at(1).status, QString("disabled"));
}

void PhaseOneAdminContractsTest::audit_filters_and_response_use_fixed_keys() {
    PhaseOneAuditFilter filter;
    filter.user_identity = "admin";
    filter.action_type = "disable_user";

    const QString query = filter.to_query_string();
    QVERIFY(query.contains("user_identity=admin"));
    QVERIFY(query.contains("action_type=disable_user"));

    const QJsonArray audit_events{QJsonObject{{"timestamp", "2026-05-22T12:00:00Z"},
                                              {"user_identity", "admin"},
                                              {"action_type", "disable_user"},
                                              {"target", "user:7"},
                                              {"result_status", "success"}}};
    const QJsonObject json{{"audit_events", audit_events}};
    const auto response = PhaseOneAuditListResponse::from_json(json);
    QCOMPARE(response.audit_events.size(), 1);
    QCOMPARE(response.audit_events.first().result_status, QString("success"));
}

} // namespace

QTEST_APPLESS_MAIN(PhaseOneAdminContractsTest)

#include "test_phase_one_admin_contracts.moc"
