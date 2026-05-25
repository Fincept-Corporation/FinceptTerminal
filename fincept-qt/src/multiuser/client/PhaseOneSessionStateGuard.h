#pragma once

#include "auth/AuthTypes.h"
#include "multiuser/client/PhaseOneClientTransport.h"

#include <functional>

namespace fincept::multiuser {

class PhaseOneSessionStateGuard {
  public:
    using InvalidSessionHandler = std::function<void()>;

    static fincept::auth::ApiResponse map_public_response(const PhaseOneTransportResponse& response);
    static fincept::auth::ApiResponse map_authenticated_response(const PhaseOneTransportResponse& response);
    static bool should_clear_auth_state(const fincept::auth::ApiResponse& response);
    static void set_invalid_session_handler(InvalidSessionHandler handler);

  private:
    static fincept::auth::ApiResponse map_response(const PhaseOneTransportResponse& response);
};

} // namespace fincept::multiuser
