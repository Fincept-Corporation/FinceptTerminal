#pragma once

#include "auth/AuthTypes.h"
#include "multiuser/contracts/PhaseOneAuditTypes.h"

#include <QObject>

#include <functional>

namespace fincept::multiuser {

class PhaseOneAuditApi : public QObject {
    Q_OBJECT
  public:
    using Callback = std::function<void(fincept::auth::ApiResponse)>;

    static PhaseOneAuditApi& instance();

    void list_audit_events(const PhaseOneAuditFilter& filter, Callback cb);

  private:
    PhaseOneAuditApi() = default;
};

} // namespace fincept::multiuser
