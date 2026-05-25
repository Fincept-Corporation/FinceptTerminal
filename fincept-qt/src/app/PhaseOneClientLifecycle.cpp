#include "app/PhaseOneClientLifecycle.h"

#include "app/InstanceLock.h"
#include "core/keys/WindowCycler.h"
#include "core/config/ProfileManager.h"
#include "core/logging/Logger.h"
#include "screens/launchpad/LaunchpadScreen.h"
#include "storage/repositories/SettingsRepository.h"
#include "auth/SessionGuard.h"

#include <QApplication>
#include <QCoreApplication>

#ifdef Q_OS_WIN
#    include <Windows.h>
#endif

namespace fincept {

namespace {

void open_new_window_for_secondary_instance(const QStringList&) {
    WindowCycler::instance().new_window_on_next_monitor();
    LOG_INFO("App", "New window opened via secondary instance request");
}

void handle_last_window_closed() {
    const auto result = SettingsRepository::instance().get(
        QStringLiteral("general.on_last_window_close"), QStringLiteral("quit"));
    const QString choice = result.is_ok() ? result.value() : QStringLiteral("quit");

    if (choice == QStringLiteral("show_launchpad")) {
        screens::LaunchpadScreen::instance()->surface();
        return;
    }

    QCoreApplication::quit();
}

} // namespace

PhaseOneClientLifecycle::PhaseOneClientLifecycle() : instance_lock_(std::make_unique<InstanceLock>()) {}

PhaseOneClientLifecycle::~PhaseOneClientLifecycle() = default;

bool PhaseOneClientLifecycle::acquire_primary(const QStringList& args) {
    const QString profile_key = QStringLiteral("FinceptTerminal-%1")
                                    .arg(ProfileManager::instance().active());
    const InstanceLock::Status status = instance_lock_->acquire(profile_key, args);
    if (status == InstanceLock::Status::Secondary) {
        allow_secondary_instance_foreground();
        return false;
    }
    return true;
}

void PhaseOneClientLifecycle::initialize_session_guard() {
    if (!session_guard_)
        session_guard_ = std::make_unique<auth::SessionGuard>();
}

void PhaseOneClientLifecycle::wire(QApplication& app) {
    QObject::connect(instance_lock_.get(), &InstanceLock::message_received, &app,
                     &open_new_window_for_secondary_instance);
    QObject::connect(&app, &QApplication::lastWindowClosed, &app,
                     &handle_last_window_closed);
}

InstanceLock& PhaseOneClientLifecycle::instance_lock() {
    return *instance_lock_;
}

void PhaseOneClientLifecycle::allow_secondary_instance_foreground() {
#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
#endif
}

} // namespace fincept
