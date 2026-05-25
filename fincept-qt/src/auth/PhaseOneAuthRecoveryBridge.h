#pragma once

#include "auth/AuthTypes.h"
#include "auth/PhaseOneSessionAuthBridge.h"

namespace fincept::auth {

class PhaseOneAuthRecoveryBridge {
  public:
    static bool should_restore_startup_auth(const QString& persisted_session_json, const QString& persisted_api_key,
                                            const QString& secure_api_key) {
        return PhaseOneSessionAuthBridge::should_restore_startup_auth(persisted_session_json, persisted_api_key,
                                                                      secure_api_key);
    }

    static void persist_phase_one_session(const SessionData& session);
    static void clear_startup_auth_state();
};

} // namespace fincept::auth
