#pragma once

#include "auth/AuthTypes.h"

namespace fincept::auth {

class PhaseOneSessionAuthBridge {
  public:
    static bool should_restore_startup_auth(const QString& persisted_session_json, const QString& persisted_api_key,
                                            const QString& secure_api_key);
    static QString resolve_fincept_provider_api_key(const SessionData& session, const QString& persisted_api_key,
                                                    const QString& secure_api_key, const QString& persisted_owner);
};

} // namespace fincept::auth
