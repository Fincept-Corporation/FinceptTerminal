#pragma once

#include "auth/AuthTypes.h"

#include <QObject>

#include <functional>

namespace fincept::multiuser {

class PhaseOneUserAdminApi : public QObject {
    Q_OBJECT
  public:
    using Callback = std::function<void(fincept::auth::ApiResponse)>;

    static PhaseOneUserAdminApi& instance();

    void bootstrap_status(Callback cb);
    void bootstrap_admin(const QString& username, const QString& password, Callback cb);
    void list_users(Callback cb);
    void create_user(const QString& username, Callback cb);
    void set_initial_password(int user_id, const QString& password, Callback cb);
    void disable_user(int user_id, Callback cb);
    void transfer_admin(int target_user_id, Callback cb);

  private:
    PhaseOneUserAdminApi() = default;
};

} // namespace fincept::multiuser
