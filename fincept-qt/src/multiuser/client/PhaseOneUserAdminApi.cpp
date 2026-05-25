#include "multiuser/client/PhaseOneUserAdminApi.h"

#include "multiuser/client/PhaseOneClientTransport.h"
#include "multiuser/client/PhaseOneSessionStateGuard.h"
#include "multiuser/contracts/PhaseOneUserAdminTypes.h"

namespace fincept::multiuser {

PhaseOneUserAdminApi& PhaseOneUserAdminApi::instance() {
    static PhaseOneUserAdminApi api;
    return api;
}

void PhaseOneUserAdminApi::bootstrap_status(Callback cb) {
    PhaseOneClientTransport::instance().get("/phase1/admin/bootstrap-status", {},
                                             [cb = std::move(cb)](const PhaseOneTransportResponse& response) {
                                                 cb(PhaseOneSessionStateGuard::map_public_response(response));
                                             });
}

void PhaseOneUserAdminApi::bootstrap_admin(const QString& username, const QString& password, Callback cb) {
    const PhaseOneBootstrapRequest request{username, password};
    PhaseOneClientTransport::instance().post("/phase1/admin/bootstrap", request.to_json(), {},
                                              [cb = std::move(cb)](const PhaseOneTransportResponse& response) {
                                                  cb(PhaseOneSessionStateGuard::map_public_response(response));
                                              });
}

void PhaseOneUserAdminApi::list_users(Callback cb) {
    PhaseOneClientTransport::instance().get("/phase1/admin/users", PhaseOneClientTransport::instance().session_id(),
                                             [cb = std::move(cb)](const PhaseOneTransportResponse& response) {
                                                 cb(PhaseOneSessionStateGuard::map_authenticated_response(response));
                                             });
}

void PhaseOneUserAdminApi::create_user(const QString& username, Callback cb) {
    const PhaseOneCreateUserRequest request{username};
    PhaseOneClientTransport::instance().post("/phase1/admin/users", request.to_json(),
                                              PhaseOneClientTransport::instance().session_id(),
                                              [cb = std::move(cb)](const PhaseOneTransportResponse& response) {
                                                  cb(PhaseOneSessionStateGuard::map_authenticated_response(response));
                                              });
}

void PhaseOneUserAdminApi::set_initial_password(int user_id, const QString& password, Callback cb) {
    const PhaseOneSetInitialPasswordRequest request{user_id, password};
    PhaseOneClientTransport::instance().post("/phase1/admin/users/set-initial-password", request.to_json(),
                                              PhaseOneClientTransport::instance().session_id(),
                                              [cb = std::move(cb)](const PhaseOneTransportResponse& response) {
                                                  cb(PhaseOneSessionStateGuard::map_authenticated_response(response));
                                              });
}

void PhaseOneUserAdminApi::disable_user(int user_id, Callback cb) {
    const PhaseOneDisableUserRequest request{user_id};
    PhaseOneClientTransport::instance().post("/phase1/admin/users/disable", request.to_json(),
                                              PhaseOneClientTransport::instance().session_id(),
                                              [cb = std::move(cb)](const PhaseOneTransportResponse& response) {
                                                  cb(PhaseOneSessionStateGuard::map_authenticated_response(response));
                                              });
}

void PhaseOneUserAdminApi::transfer_admin(int target_user_id, Callback cb) {
    const PhaseOneTransferAdminRequest request{target_user_id};
    PhaseOneClientTransport::instance().post("/phase1/admin/users/transfer-admin", request.to_json(),
                                              PhaseOneClientTransport::instance().session_id(),
                                              [cb = std::move(cb)](const PhaseOneTransportResponse& response) {
                                                  cb(PhaseOneSessionStateGuard::map_authenticated_response(response));
                                              });
}

} // namespace fincept::multiuser
