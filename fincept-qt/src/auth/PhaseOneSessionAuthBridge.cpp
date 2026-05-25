#include "auth/PhaseOneSessionAuthBridge.h"

namespace fincept::auth {

bool PhaseOneSessionAuthBridge::should_restore_startup_auth(const QString& persisted_session_json,
                                                            const QString& persisted_api_key,
                                                            const QString& secure_api_key) {
    Q_UNUSED(persisted_session_json);
    Q_UNUSED(persisted_api_key);
    Q_UNUSED(secure_api_key);
    return false;
}

QString PhaseOneSessionAuthBridge::resolve_fincept_provider_api_key(const SessionData& session,
                                                                    const QString& persisted_api_key,
                                                                    const QString& secure_api_key) {
    if (session.has_hosted_api_key())
        return session.api_key;
    if (!persisted_api_key.isEmpty())
        return persisted_api_key;
    return secure_api_key;
}

} // namespace fincept::auth
