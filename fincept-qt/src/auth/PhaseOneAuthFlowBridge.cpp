#include "auth/PhaseOneAuthFlowBridge.h"

#ifndef FINCEPT_PHASE_ONE_UI_FLOW_TESTS
#include "auth/AuthApi.h"
#include "multiuser/client/PhaseOneUserAdminApi.h"
#endif

namespace fincept::auth {

namespace {

#ifdef FINCEPT_PHASE_ONE_UI_FLOW_TESTS
PhaseOneAuthFlowBridge::TestLoginHandler& test_login_handler() {
    static PhaseOneAuthFlowBridge::TestLoginHandler handler;
    return handler;
}

PhaseOneAuthFlowBridge::TestBootstrapStatusHandler& test_bootstrap_status_handler() {
    static PhaseOneAuthFlowBridge::TestBootstrapStatusHandler handler;
    return handler;
}

PhaseOneAuthFlowBridge::TestBootstrapHandler& test_bootstrap_handler() {
    static PhaseOneAuthFlowBridge::TestBootstrapHandler handler;
    return handler;
}
#endif

} // namespace

void PhaseOneAuthFlowBridge::resolve_auth_entry_route(RouteCallback cb) {
#ifdef FINCEPT_PHASE_ONE_UI_FLOW_TESTS
    if (test_bootstrap_status_handler()) {
        const ApiResponse response = test_bootstrap_status_handler()();
        const bool bootstrap_open = response.success && response.data.value(QStringLiteral("bootstrap_open")).toBool();
        if (cb)
            cb(bootstrap_open ? kBootstrapAuthStackIndex : kLoginAuthStackIndex);
        return;
    }

    if (cb)
        cb(kLoginAuthStackIndex);
#else
    multiuser::PhaseOneUserAdminApi::instance().bootstrap_status([cb = std::move(cb)](ApiResponse response) mutable {
        const bool bootstrap_open = response.success && response.data.value(QStringLiteral("bootstrap_open")).toBool();
        if (cb)
            cb(bootstrap_open ? kBootstrapAuthStackIndex : kLoginAuthStackIndex);
    });
#endif
}

void PhaseOneAuthFlowBridge::submit_login(const QString& username, const QString& password, LoginCallback cb) {
#ifdef FINCEPT_PHASE_ONE_UI_FLOW_TESTS
    if (test_login_handler()) {
        if (cb)
            cb(test_login_handler()(username, password));
        return;
    }

    if (cb)
        cb({false, {}, QStringLiteral("No phase-one login test handler installed."), 0, {}});
    return;
#else
    AuthApi::instance().phase_one_login(username, password, std::move(cb));
#endif
}

void PhaseOneAuthFlowBridge::submit_bootstrap(const QString& username, const QString& password, BootstrapCallback cb) {
#ifdef FINCEPT_PHASE_ONE_UI_FLOW_TESTS
    if (test_bootstrap_handler()) {
        if (cb)
            cb(test_bootstrap_handler()(username, password));
        return;
    }

    if (cb)
        cb({false, {}, QStringLiteral("No phase-one bootstrap test handler installed."), 0, {}});
#else
    multiuser::PhaseOneUserAdminApi::instance().bootstrap_admin(username, password, std::move(cb));
#endif
}

#ifdef FINCEPT_PHASE_ONE_UI_FLOW_TESTS
void PhaseOneAuthFlowBridge::set_test_login_handler_for_tests(TestLoginHandler handler) {
    test_login_handler() = std::move(handler);
}

void PhaseOneAuthFlowBridge::set_test_bootstrap_status_handler_for_tests(TestBootstrapStatusHandler handler) {
    test_bootstrap_status_handler() = std::move(handler);
}

void PhaseOneAuthFlowBridge::set_test_bootstrap_handler_for_tests(TestBootstrapHandler handler) {
    test_bootstrap_handler() = std::move(handler);
}

void PhaseOneAuthFlowBridge::reset_test_login_handler_for_tests() {
    test_login_handler() = {};
}

void PhaseOneAuthFlowBridge::reset_test_bootstrap_status_handler_for_tests() {
    test_bootstrap_status_handler() = {};
}

void PhaseOneAuthFlowBridge::reset_test_bootstrap_handler_for_tests() {
    test_bootstrap_handler() = {};
}
#endif

} // namespace fincept::auth
