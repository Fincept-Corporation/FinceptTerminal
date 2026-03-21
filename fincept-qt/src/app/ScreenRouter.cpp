#include "app/ScreenRouter.h"
#include "core/session/SessionManager.h"
#include "core/logging/Logger.h"

namespace fincept {

ScreenRouter::ScreenRouter(QStackedWidget* stack, QObject* parent)
    : QObject(parent), stack_(stack) {}

void ScreenRouter::register_screen(const QString& id, QWidget* screen) {
    screens_[id] = screen;
    stack_->addWidget(screen);
}

void ScreenRouter::register_factory(const QString& id, ScreenFactory factory) {
    factories_[id] = std::move(factory);
}

void ScreenRouter::navigate(const QString& id) {
    // Check if screen already exists
    auto it = screens_.find(id);
    if (it != screens_.end()) {
        current_id_ = id;
        stack_->setCurrentWidget(it.value());
        SessionManager::instance().set_last_screen(id);
        LOG_INFO("Router", "Navigated to: " + id);
        emit screen_changed(id);
        return;
    }

    // Check if there's a factory — construct lazily on first navigation
    auto fit = factories_.find(id);
    if (fit != factories_.end()) {
        LOG_INFO("Router", "Lazy-constructing screen: " + id);
        QWidget* screen = fit.value()();
        factories_.remove(id);
        register_screen(id, screen);
        navigate(id);  // now it exists in screens_
        return;
    }

    LOG_WARN("Router", "Unknown screen: " + id);
}

} // namespace fincept
