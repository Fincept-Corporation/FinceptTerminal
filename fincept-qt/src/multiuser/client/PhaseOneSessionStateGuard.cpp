#include "multiuser/client/PhaseOneSessionStateGuard.h"

namespace fincept::multiuser {

namespace {

PhaseOneSessionStateGuard::InvalidSessionHandler& invalid_session_handler() {
    static PhaseOneSessionStateGuard::InvalidSessionHandler handler;
    return handler;
}

} // namespace

fincept::auth::ApiResponse PhaseOneSessionStateGuard::map_public_response(const PhaseOneTransportResponse& response) {
    return map_response(response);
}

fincept::auth::ApiResponse PhaseOneSessionStateGuard::map_authenticated_response(const PhaseOneTransportResponse& response) {
    const fincept::auth::ApiResponse mapped = map_response(response);
    if (should_clear_auth_state(mapped) && invalid_session_handler())
        invalid_session_handler()();
    return mapped;
}

bool PhaseOneSessionStateGuard::should_clear_auth_state(const fincept::auth::ApiResponse& response) {
    return response.error_code == QStringLiteral("session_invalid") ||
           response.error_code == QStringLiteral("session_expired") ||
           response.error_code == QStringLiteral("session_revoked") ||
           response.error_code == QStringLiteral("user_disabled");
}

void PhaseOneSessionStateGuard::set_invalid_session_handler(InvalidSessionHandler handler) {
    invalid_session_handler() = std::move(handler);
}

fincept::auth::ApiResponse PhaseOneSessionStateGuard::map_response(const PhaseOneTransportResponse& response) {
    if (!response.transport_success) {
        return {false,
                response.body,
                response.error_message.isEmpty() ? QStringLiteral("Phase one request failed.") : response.error_message,
                0,
                response.error_code};
    }

    if (response.status_code >= 200 && response.status_code < 300)
        return {true, response.body, {}, response.status_code, {}};

    QString message = response.error_message;
    if (message.isEmpty())
        message = response.body.value("message").toString();
    if (message.isEmpty())
        message = QStringLiteral("Phase one request failed.");

    return {false, response.body, message, response.status_code, response.body.value("error_code").toString()};
}

} // namespace fincept::multiuser
