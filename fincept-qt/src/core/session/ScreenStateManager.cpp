#include "core/session/ScreenStateManager.h"

#include "core/logging/Logger.h"
#include "screens/common/IStatefulScreen.h"
#include "storage/cache/TabSessionStore.h"

#include <QJsonObject>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

#include <tuple>

namespace fincept {

// ── Singleton ────────────────────────────────────────────────────────────────

ScreenStateManager& ScreenStateManager::instance() {
    static ScreenStateManager s;
    return s;
}

ScreenStateManager::ScreenStateManager(QObject* parent) : QObject(parent) {
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
    if (!screen)
        return;

    const QString key = screen->state_key();
    const int expected = screen->state_version();

    auto result = TabSessionStore::instance().load_screen_state(key, expected);
    if (result.is_err()) {
        LOG_WARN("ScreenState", "Failed to load state for '" + key + "': " + QString::fromStdString(result.error()));
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
    if (!screen)
        return;

    const QString key = screen->state_key();
    const QVariantMap state = screen->save_state();
    const int version = screen->state_version();
    const QString sid = session_id_;

    // Remove from pending so the debounce timer doesn't double-write.
    pending_saves_.remove(key);

    write_async(key, state, version, sid);
    LOG_DEBUG("ScreenState", "save_now dispatched for: " + key);
}

// ── Notify changed (debounced) ───────────────────────────────────────────────

void ScreenStateManager::notify_changed(screens::IStatefulScreen* screen) {
    if (!screen)
        return;
    pending_saves_.insert(screen->state_key(), screen);
    debounce_timer_->start(); // restart — resets the 500 ms window
}

// ── Direct save/load (for screens that can't implement IStatefulScreen) ──────

void ScreenStateManager::save_direct(const QString& key, const QVariantMap& state, int version) {
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
    // Phase 4b: flush both the legacy screen-key table and the UUID table.
    // Snapshot + clear each separately so new notify_* calls during the
    // async write don't get clobbered by this flush.
    if (pending_saves_.isEmpty() && pending_uuid_saves_.isEmpty())
        return;

    const auto snapshot = pending_saves_;
    pending_saves_.clear();

    const auto uuid_snapshot = pending_uuid_saves_;
    pending_uuid_saves_.clear();

    for (auto it = snapshot.constBegin(); it != snapshot.constEnd(); ++it) {
        screens::IStatefulScreen* screen = it.value();
        if (!screen)
            continue;

        const QString key = screen->state_key();
        const QVariantMap state = screen->save_state();
        const int version = screen->state_version();
        const QString sid = session_id_;

        write_async(key, state, version, sid);
    }

    for (auto it = uuid_snapshot.constBegin(); it != uuid_snapshot.constEnd(); ++it) {
        screens::IStatefulScreen* screen = it.value();
        if (!screen)
            continue;

        const QString instance_uuid = it.key();
        const QString screen_key = screen->state_key();
        const QVariantMap state = screen->save_state();
        const int version = screen->state_version();
        const QString sid = session_id_;

        write_async_by_uuid(instance_uuid, screen_key, state, version, sid);
    }

    LOG_DEBUG("ScreenState", QString("Flushed %1 legacy + %2 UUID-keyed screen states")
                                 .arg(snapshot.size()).arg(uuid_snapshot.size()));
}

// ── Private static: async SQLite write ───────────────────────────────────────

void ScreenStateManager::write_async(const QString& key, const QVariantMap& state, int version,
                                     const QString& session_id) {
    // Capture by value — no raw 'this', no widget pointers (P8 compliant).
    std::ignore = QtConcurrent::run([key, state, version, session_id]() {
        // Convert QVariantMap → QJsonObject
        QJsonObject obj;
        for (auto it = state.constBegin(); it != state.constEnd(); ++it)
            obj.insert(it.key(), QJsonValue::fromVariant(it.value()));

        auto r = TabSessionStore::instance().save_screen_state(key, obj, version, session_id);

        if (r.is_err()) {
            LOG_WARN("ScreenState", "Write failed for '" + key + "': " + QString::fromStdString(r.error()));
        }
    });
}

void ScreenStateManager::write_async_by_uuid(const QString& instance_uuid,
                                             const QString& screen_key,
                                             const QVariantMap& state,
                                             int version,
                                             const QString& session_id) {
    std::ignore = QtConcurrent::run([instance_uuid, screen_key, state, version, session_id]() {
        QJsonObject obj;
        for (auto it = state.constBegin(); it != state.constEnd(); ++it)
            obj.insert(it.key(), QJsonValue::fromVariant(it.value()));

        auto r = TabSessionStore::instance().save_screen_state_by_uuid(
            instance_uuid, screen_key, obj, version, session_id);

        if (r.is_err()) {
            LOG_WARN("ScreenState",
                     QString("UUID write failed for %1 (%2): %3")
                         .arg(instance_uuid, screen_key, QString::fromStdString(r.error())));
        }
    });
}

// ── UUID-keyed public API ────────────────────────────────────────────────────

void ScreenStateManager::restore_by_uuid(screens::IStatefulScreen* screen,
                                         const QString& instance_uuid,
                                         bool fallback_to_screen_key) {
    if (!screen)
        return;
    const int expected = screen->state_version();

    // First try the UUID-keyed row (the new path).
    auto by_uuid = TabSessionStore::instance().load_screen_state_by_uuid(instance_uuid, expected);
    if (by_uuid.is_err()) {
        LOG_WARN("ScreenState", QString("UUID load failed for %1: %2")
                                    .arg(instance_uuid, QString::fromStdString(by_uuid.error())));
        return;
    }

    QJsonObject obj = by_uuid.value();
    if (obj.isEmpty() && fallback_to_screen_key) {
        // No UUID-keyed row yet. Fall back to the legacy screen_key path
        // so users upgrading from a pre-Phase-4b build keep their state.
        // The next save will write a UUID-keyed row, so the fallback is a
        // one-time migration assist per panel.
        const QString legacy_key = screen->state_key();
        if (!legacy_key.isEmpty()) {
            auto legacy = TabSessionStore::instance().load_screen_state(legacy_key, expected);
            if (legacy.is_ok())
                obj = legacy.value();
        }
    }

    if (obj.isEmpty()) {
        LOG_DEBUG("ScreenState", QString("No saved state for uuid=%1").arg(instance_uuid));
        return;
    }

    QVariantMap state_map;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        state_map.insert(it.key(), it.value().toVariant());

    screen->restore_state(state_map);
    LOG_DEBUG("ScreenState", QString("Restored state for uuid=%1").arg(instance_uuid));
}

void ScreenStateManager::save_now_by_uuid(screens::IStatefulScreen* screen,
                                          const QString& instance_uuid) {
    if (!screen || instance_uuid.isEmpty())
        return;

    const QString screen_key = screen->state_key();
    const QVariantMap state = screen->save_state();
    const int version = screen->state_version();
    const QString sid = session_id_;

    pending_uuid_saves_.remove(instance_uuid);
    write_async_by_uuid(instance_uuid, screen_key, state, version, sid);
    LOG_DEBUG("ScreenState", QString("save_now_by_uuid dispatched for %1").arg(instance_uuid));
}

void ScreenStateManager::notify_changed_by_uuid(screens::IStatefulScreen* screen,
                                                const QString& instance_uuid) {
    if (!screen || instance_uuid.isEmpty())
        return;
    pending_uuid_saves_.insert(instance_uuid, screen);
    debounce_timer_->start();
}

void ScreenStateManager::erase_by_uuid(const QString& instance_uuid) {
    pending_uuid_saves_.remove(instance_uuid);
    // Run the delete async so the caller (closeEvent) doesn't block on disk.
    std::ignore = QtConcurrent::run([instance_uuid]() {
        auto r = TabSessionStore::instance().remove_screen_state_by_uuid(instance_uuid);
        if (r.is_err()) {
            LOG_WARN("ScreenState",
                     QString("erase_by_uuid failed for %1: %2")
                         .arg(instance_uuid, QString::fromStdString(r.error())));
        }
    });
}

} // namespace fincept
