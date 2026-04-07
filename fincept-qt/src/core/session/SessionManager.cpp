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

void SessionManager::save_geometry(int window_id, const QByteArray& geometry, const QByteArray& state) {
    const QString prefix = QString("window_%1/").arg(window_id);
    settings_.setValue(prefix + "geometry", geometry);
    settings_.setValue(prefix + "state", state);
}

QByteArray SessionManager::load_geometry(int window_id) const {
    const QString prefix = QString("window_%1/").arg(window_id);
    // Fall back to legacy flat key for migration from single-window era
    QByteArray val = settings_.value(prefix + "geometry").toByteArray();
    if (val.isEmpty() && window_id == 0)
        val = settings_.value("window/geometry").toByteArray();
    return val;
}

QByteArray SessionManager::load_state(int window_id) const {
    const QString prefix = QString("window_%1/").arg(window_id);
    QByteArray val = settings_.value(prefix + "state").toByteArray();
    if (val.isEmpty() && window_id == 0)
        val = settings_.value("window/state").toByteArray();
    return val;
}

void SessionManager::save_window_count(int count) {
    settings_.setValue("window_count", count);
}

int SessionManager::load_window_count() const {
    return settings_.value("window_count", 1).toInt();
}

void SessionManager::save_dock_layout(int window_id, const QByteArray& layout) {
    const QString prefix = QString("window_%1/").arg(window_id);
    settings_.setValue(prefix + "dock_layout", layout);
}

QByteArray SessionManager::load_dock_layout(int window_id) const {
    const QString prefix = QString("window_%1/").arg(window_id);
    QByteArray val = settings_.value(prefix + "dock_layout").toByteArray();
    if (val.isEmpty() && window_id == 0)
        val = settings_.value("window/dock_layout").toByteArray();
    return val;
}

void SessionManager::set_dock_layout_version(int window_id, int version) {
    const QString prefix = QString("window_%1/").arg(window_id);
    settings_.setValue(prefix + "dock_layout_version", version);
}

int SessionManager::dock_layout_version(int window_id) const {
    const QString prefix = QString("window_%1/").arg(window_id);
    return settings_.value(prefix + "dock_layout_version", 0).toInt();
}

void SessionManager::save_perspectives(const QSettings& source) {
    // Copy all keys from the source QSettings (written by ADS savePerspectives)
    // into our own settings_ under a "perspectives/" prefix.
    QSettings& src = const_cast<QSettings&>(source);
    src.beginGroup("Perspectives");
    const QStringList keys = src.allKeys();
    settings_.beginGroup("perspectives");
    settings_.remove(""); // clear previous
    for (const QString& k : keys)
        settings_.setValue(k, src.value(k));
    settings_.endGroup();
    src.endGroup();
}

void SessionManager::load_perspectives(QSettings& target) const {
    QSettings& s = const_cast<QSettings&>(settings_);
    s.beginGroup("perspectives");
    const QStringList keys = s.allKeys();
    target.beginGroup("Perspectives");
    for (const QString& k : keys)
        target.setValue(k, s.value(k));
    target.endGroup();
    s.endGroup();
}

void SessionManager::set_last_screen(const QString& screen_id) {
    settings_.setValue("session/last_screen", screen_id);
}

QString SessionManager::last_screen() const {
    return settings_.value("session/last_screen", "dashboard").toString();
}

} // namespace fincept
