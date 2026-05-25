#pragma once

#include <QString>

namespace fincept::multiuser {

struct PhaseOneEndpointResolution {
    bool ok = false;
    QString base_url;
    QString error_code;
    QString message;
};

class PhaseOneEndpointProvider {
  public:
    static PhaseOneEndpointProvider& instance();

    PhaseOneEndpointResolution resolve() const;

  private:
    PhaseOneEndpointProvider() = default;
};

} // namespace fincept::multiuser
