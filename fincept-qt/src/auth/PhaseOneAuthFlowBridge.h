#pragma once

#include "auth/AuthTypes.h"

#include <functional>

namespace fincept::auth {

class PhaseOneAuthFlowBridge {
  public:
    using LoginCallback = std::function<void(ApiResponse)>;
    using RouteCallback = std::function<void(int)>;
    using BootstrapCallback = std::function<void(ApiResponse)>;

    static constexpr int kLoginAuthStackIndex = 0;
    static constexpr int kBootstrapAuthStackIndex = 5;

    static int normalize_auth_stack_index(int requested_index) {
        if (requested_index == 1 || requested_index == 2)
            return 0;
        return requested_index;
    }

    static bool allows_terminal_entry(const SessionData& session) {
        return session.is_phase_one_session() || session.has_paid_plan();
    }

    static bool supports_self_registration() {
        return false;
    }

    static bool supports_password_recovery() {
        return false;
    }

    static void resolve_auth_entry_route(RouteCallback cb);
    static void submit_login(const QString& username, const QString& password, LoginCallback cb);
    static void submit_bootstrap(const QString& username, const QString& password, BootstrapCallback cb);

#ifdef FINCEPT_PHASE_ONE_UI_FLOW_TESTS
    using TestLoginHandler = std::function<ApiResponse(const QString&, const QString&)>;
    using TestBootstrapStatusHandler = std::function<ApiResponse()>;
    using TestBootstrapHandler = std::function<ApiResponse(const QString&, const QString&)>;

    static void set_test_login_handler_for_tests(TestLoginHandler handler);
    static void set_test_bootstrap_status_handler_for_tests(TestBootstrapStatusHandler handler);
    static void set_test_bootstrap_handler_for_tests(TestBootstrapHandler handler);
    static void reset_test_login_handler_for_tests();
    static void reset_test_bootstrap_status_handler_for_tests();
    static void reset_test_bootstrap_handler_for_tests();
#endif
};

} // namespace fincept::auth
