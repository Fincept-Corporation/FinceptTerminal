#include "core/session/SessionManager.h"

namespace fincept {

SessionManager& SessionManager::instance() {
    static SessionManager s;
    return s;
}

void SessionManager::start_session() {
    elapsed_.start();
    emit session_started();
}

qint64 SessionManager::uptime_seconds() const {
    return elapsed_.elapsed() / 1000;
}

QString SessionManager::uptime_string() const {
    qint64 total = uptime_seconds();
    int h = static_cast<int>(total / 3600);
    int m = static_cast<int>((total % 3600) / 60);
    int s = static_cast<int>(total % 60);
    return QString("%1:%2:%3").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

void SessionManager::save_tab_state(const QString& tab_id, const QVariantMap& state) {
    settings_.beginGroup("tab_state/" + tab_id);
    for (auto it = state.begin(); it != state.end(); ++it) {
        settings_.setValue(it.key(), it.value());
    }
    settings_.endGroup();
}

QVariantMap SessionManager::load_tab_state(const QString& tab_id) const {
    QVariantMap result;
    QSettings& s = const_cast<QSettings&>(settings_);
    s.beginGroup("tab_state/" + tab_id);
    for (const auto& key : s.childKeys()) {
        result[key] = s.value(key);
    }
    s.endGroup();
    return result;
}

void SessionManager::save_geometry(const QByteArray& geometry, const QByteArray& state) {
    settings_.setValue("window/geometry", geometry);
    settings_.setValue("window/state", state);
}

QByteArray SessionManager::load_geometry() const {
    return settings_.value("window/geometry").toByteArray();
}

QByteArray SessionManager::load_state() const {
    return settings_.value("window/state").toByteArray();
}

void SessionManager::set_last_screen(const QString& screen_id) {
    settings_.setValue("session/last_screen", screen_id);
}

QString SessionManager::last_screen() const {
    return settings_.value("session/last_screen", "dashboard").toString();
}

} // namespace fincept
