#pragma once

#include <QStringList>

#include <memory>

class QApplication;

namespace fincept {

class InstanceLock;

namespace auth {
class SessionGuard;
}

class PhaseOneClientLifecycle {
  public:
    PhaseOneClientLifecycle();
    ~PhaseOneClientLifecycle();

    bool acquire_primary(const QStringList& args);
    void initialize_session_guard();
    void wire(QApplication& app);

    InstanceLock& instance_lock();

  private:
    static void allow_secondary_instance_foreground();

    std::unique_ptr<InstanceLock> instance_lock_;
    std::unique_ptr<auth::SessionGuard> session_guard_;
};

} // namespace fincept
