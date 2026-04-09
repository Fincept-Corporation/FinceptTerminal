#include "core/session/ScreenStateManager.h"

#include "core/logging/Logger.h"
#include "screens/IStatefulScreen.h"
#include "storage/cache/TabSessionStore.h"

#include <QJsonObject>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

namespace fincept {

// ── Singleton ────────────────────────────────────────────────────────────────

ScreenStateManager& ScreenStateManager::instance() {
    static ScreenStateManager s;
    return s;
}

ScreenStateManager::ScreenStateManager(QObject* parent)
    : QObject(parent) {
    debounce_timer_ = new QTimer(this);
    debounce_timer_->setSingleShot(true);
    debounce_timer_->setInterval(500); // 500 ms quiet period before flush
    connect(debounce_timer_, &QTimer::timeout, this, &ScreenStateManager::flush_pending);
}

// ── Session ID ───────────────────────────────────────────────────────────────

void ScreenStateManager::set_session_id(const QString& id) {
    session_id_ = id;
}

QString ScreenStateManager::session_id() const {
    return session_id_;
}

// ── Restore ──────────────────────────────────────────────────────────────────

void ScreenStateManager::restore(screens::IStatefulScreen* screen) {
    if (!screen) return;

    const QString key      = screen->state_key();
    const int     expected = screen->state_version();

    auto result = TabSessionStore::instance().load_screen_state(key, expected);
    if (result.is_err()) {
        LOG_WARN("ScreenState", "Failed to load state for '" + key + "': "
                 + QString::fromStdString(result.error()));
        return;
    }

    const QJsonObject obj = result.value();
    if (obj.isEmpty()) {
        LOG_DEBUG("ScreenState", "No saved state for: " + key);
        return; // nothing stored yet or version mismatch — screen keeps defaults
    }

    // Convert QJsonObject → QVariantMap for IStatefulScreen
    QVariantMap state_map;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        state_map.insert(it.key(), it.value().toVariant());

    screen->restore_state(state_map);
    LOG_DEBUG("ScreenState", "Restored state for: " + key);
}

// ── Save now (async, fire-and-forget) ────────────────────────────────────────

void ScreenStateManager::save_now(screens::IStatefulScreen* screen) {
    if (!screen) return;

    const QString    key     = screen->state_key();
    const QVariantMap state  = screen->save_state();
    const int        version = screen->state_version();
    const QString    sid     = session_id_;

    // Remove from pending so the debounce timer doesn't double-write.
    pending_saves_.remove(key);

    write_async(key, state, version, sid);
    LOG_DEBUG("ScreenState", "save_now dispatched for: " + key);
}

// ── Notify changed (debounced) ───────────────────────────────────────────────

void ScreenStateManager::notify_changed(screens::IStatefulScreen* screen) {
    if (!screen) return;
    pending_saves_.insert(screen->state_key(), screen);
    debounce_timer_->start(); // restart — resets the 500 ms window
}

// ── Direct save/load (for screens that can't implement IStatefulScreen) ──────

void ScreenStateManager::save_direct(const QString& key,
                                      const QVariantMap& state,
                                      int version) {
    write_async(key, state, version, session_id_);
}

QVariantMap ScreenStateManager::load_direct(const QString& key, int expected_version) {
    auto result = TabSessionStore::instance().load_screen_state(key, expected_version);
    if (result.is_err() || result.value().isEmpty())
        return {};

    const QJsonObject obj = result.value();
    QVariantMap map;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        map.insert(it.key(), it.value().toVariant());
    return map;
}

// ── Private: flush pending ───────────────────────────────────────────────────

void ScreenStateManager::flush_pending() {
    if (pending_saves_.isEmpty()) return;

    // Snapshot and clear immediately so new notify_changed() calls during
    // the async write don't get cleared by a later flush.
    const auto snapshot = pending_saves_;
    pending_saves_.clear();

    for (auto it = snapshot.constBegin(); it != snapshot.constEnd(); ++it) {
        screens::IStatefulScreen* screen = it.value();
        if (!screen) continue;

        const QString     key     = screen->state_key();
        const QVariantMap state   = screen->save_state();
        const int         version = screen->state_version();
        const QString     sid     = session_id_;

        write_async(key, state, version, sid);
    }
    LOG_DEBUG("ScreenState", QString("Flushed %1 pending screen states").arg(snapshot.size()));
}

// ── Private static: async SQLite write ───────────────────────────────────────

void ScreenStateManager::write_async(const QString& key,
                                      const QVariantMap& state,
                                      int version,
                                      const QString& session_id) {
    // Capture by value — no raw 'this', no widget pointers (P8 compliant).
    QtConcurrent::run([key, state, version, session_id]() {
        // Convert QVariantMap → QJsonObject
        QJsonObject obj;
        for (auto it = state.constBegin(); it != state.constEnd(); ++it)
            obj.insert(it.key(), QJsonValue::fromVariant(it.value()));

        auto r = TabSessionStore::instance().save_screen_state(
            key, obj, version, session_id);

        if (r.is_err()) {
            LOG_WARN("ScreenState", "Write failed for '" + key + "': "
                     + QString::fromStdString(r.error()));
        }
    });
}

} // namespace fincept
