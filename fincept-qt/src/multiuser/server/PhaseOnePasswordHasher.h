#pragma once

#include <QString>

namespace fincept::multiuser {

class PhaseOnePasswordHasher {
  public:
    static QString hash_password(const QString& password);
    static bool verify_password(const QString& password, const QString& encoded_hash);
};

} // namespace fincept::multiuser
