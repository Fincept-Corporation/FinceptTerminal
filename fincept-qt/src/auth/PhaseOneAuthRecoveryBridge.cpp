#include "auth/PhaseOneAuthRecoveryBridge.h"

#include "storage/repositories/SettingsRepository.h"
#include "storage/secure/SecureStorage.h"

#include <QJsonDocument>

namespace fincept::auth {

void PhaseOneAuthRecoveryBridge::persist_phase_one_session(const SessionData& session) {
    const QString json = QString::fromUtf8(QJsonDocument(session.to_json()).toJson(QJsonDocument::Compact));
    SettingsRepository::instance().set("fincept_session", json, "auth");
    Q_UNUSED(session)
}

void PhaseOneAuthRecoveryBridge::clear_startup_auth_state() {
    SettingsRepository::instance().remove("fincept_session");
}

} // namespace fincept::auth
