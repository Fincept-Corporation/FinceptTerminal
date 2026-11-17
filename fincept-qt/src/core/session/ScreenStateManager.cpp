#include "core/session/ScreenStateManager.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStatePolicy.h"
#include "screens/common/IStatefulScreen.h"
#include "storage/cache/TabSessionStore.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

#include <atomic>
#include <tuple>

namespace fincept {

namespace {

constexpr int kQuietMs = 500;     // quiet period before a debounced flush
constexpr int kMaxDelayMs = 2000; // a stream of changes never postpones the flush past this (+ kQuietMs)

// pending_saves_ / pending_uuid_saves_ hold raw IStatefulScreen*, which is not a
// QObject and so cannot be wrapped in a QPointer directly. A screen that is
// destroyed inside the 500 ms debounce window (window closed, panel torn off or
// moved to another frame) would otherwise be dereferenced by flush_pending().
// notify_changed*() therefore records a QPointer to the screen's QObject side
// here (header-neutral: this header is included by ~75 files), and
// flush_pending() skips any screen whose guard has gone null. Screens that are
// not QObjects have no entry and are treated as live, as before.
QHash<const screens::IStatefulScreen*, QPointer<QObject>>& ssm_pending_guards() {
    static QHash<const screens::IStatefulScreen*, QPointer<QObject>> guards;
    return guards;
}

void ssm_guard_pending(screens::IStatefulScreen* screen) {
    if (auto* obj = dynamic_cast<QObject*>(screen))
        ssm_pending_guards().insert(screen, QPointer<QObject>(obj));
}

bool ssm_pending_screen_alive(const screens::IStatefulScreen* screen) {
    const auto& guards = ssm_pending_guards();
    const auto it = guards.constFind(screen);
    return it == guards.constEnd() || !it.value().isNull();
}

// ── Panel bindings ───────────────────────────────────────────────────────────
// A screen restored or saved under a panel instance id is bound to it, so its
// own notify_changed(this) calls (screens only know their state_key()) save to
// the panel's row — the row restore_by_uuid() reads. Without this, edits went
// to the screen-type row only and the panel row was refreshed on hide/quit
// only, so after an abnormal exit the restore read an older copy. The QPointer
// expires the binding with the screen (a new screen at a reused address starts
// unbound); screens that are not QObjects are never bound.
struct PanelBinding {
    QString uuid;
    QPointer<QObject> guard;
};

QHash<const screens::IStatefulScreen*, PanelBinding>& ssm_bindings() {
    static QHash<const screens::IStatefulScreen*, PanelBinding> bindings;
    return bindings;
}

void ssm_bind(screens::IStatefulScreen* screen, const QString& instance_uuid) {
    if (!screen || instance_uuid.isEmpty())
        return;
    auto* obj = dynamic_cast<QObject*>(screen);
    if (!obj)
        return;
    ssm_bindings().insert(screen, PanelBinding{instance_uuid, QPointer<QObject>(obj)});
}

QString ssm_bound_uuid(const screens::IStatefulScreen* screen) {
    auto& bindings = ssm_bindings();
    const auto it = bindings.find(screen);
    if (it == bindings.end())
        return {};
    if (it->guard.isNull()) {
        bindings.erase(it);
        return {};
    }
    return it->uuid;
}

void ssm_prune_bindings() {
    auto& bindings = ssm_bindings();
    for (auto it = bindings.begin(); it != bindings.end();) {
        if (it->guard.isNull())
            it = bindings.erase(it);
        else
            ++it;
    }
}

// ── Ordered writes ───────────────────────────────────────────────────────────
// Writes run on QtConcurrent pool threads, each with its own SQLite
// connection, so two writes to one row could finish in either order. Each write
// takes a sequence number when it is DISPATCHED (GUI thread) and the gate skips
// it if a later write already landed on its row (ScreenStatePolicy.h). The
// synchronous shutdown writes go through the same gate, so an older async
// write still queued at quit cannot land after them.
std::atomic<quint64> g_write_seq{0};

quint64 ssm_next_seq() {
    return g_write_seq.fetch_add(1, std::memory_order_relaxed) + 1;
}

QMutex& ssm_write_mutex() {
    static QMutex m;
    return m;
}

session::WriteOrderGate& ssm_write_gate() {
    static session::WriteOrderGate gate; // guarded by ssm_write_mutex()
    return gate;
}

QString instance_row_key(const QString& instance_uuid) {
    return QStringLiteral("uuid:") + instance_uuid;
}

QJsonObject to_json_obj(const QVariantMap& state) {
    QJsonObject obj;
    for (auto it = state.constBegin(); it != state.constEnd(); ++it)
        obj.insert(it.key(), QJsonValue::fromVariant(it.value()));
    return obj;
}

QVariantMap to_variant_map(const QJsonObject& obj) {
    QVariantMap map;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        map.insert(it.key(), it.value().toVariant());
    return map;
}

/// One captured save: the panel row (when instance_uuid is set) and the
/// screen-type row (when screen_key is set, tagged with the writer).
struct StateWrite {
    QString instance_uuid;
    QString screen_key;
    QVariantMap state;
    int version = 1;
    QString session_id;
    quint64 seq = 0;
};

StateWrite capture_write(screens::IStatefulScreen* screen, const QString& instance_uuid, const QString& session_id) {
    StateWrite w;
    w.instance_uuid = instance_uuid;
    w.screen_key = screen->state_key();
    w.state = screen->save_state();
    w.version = screen->state_version();
    w.session_id = session_id;
    w.seq = ssm_next_seq();
    return w;
}

void perform_write(const StateWrite& w) {
    const QJsonObject obj = to_json_obj(w.state);
    QMutexLocker lock(&ssm_write_mutex());
    auto& gate = ssm_write_gate();
    auto& store = TabSessionStore::instance();

    // Screen-type row first, panel row second: the panel row's updated_at is
    // then never older than a screen-type copy this same write produced, so a
    // restore of this panel keeps reading its own row (a tie keeps it too).
    if (!w.screen_key.isEmpty() && gate.admit(w.screen_key, w.seq)) {
        auto r = store.save_screen_state(w.screen_key, obj, w.version, w.session_id, w.instance_uuid);
        if (r.is_ok())
            gate.mark_applied(w.screen_key, w.seq);
        else
            LOG_WARN("ScreenState", "Write failed for '" + w.screen_key + "': " + QString::fromStdString(r.error()));
    }
    if (!w.instance_uuid.isEmpty()) {
        const QString row = instance_row_key(w.instance_uuid);
        if (gate.admit(row, w.seq)) {
            auto r = store.save_screen_state_by_uuid(w.instance_uuid, w.screen_key, obj, w.version, w.session_id);
            if (r.is_ok())
                gate.mark_applied(row, w.seq);
            else
                LOG_WARN("ScreenState", QString("UUID write failed for %1 (%2): %3")
                                            .arg(w.instance_uuid, w.screen_key, QString::fromStdString(r.error())));
        }
    }
}

void dispatch_write(StateWrite w) {
    // Capture by value — no raw 'this', no widget pointers (P8 compliant).
    std::ignore = QtConcurrent::run([w = std::move(w)]() { perform_write(w); });
}

QElapsedTimer& ssm_pending_age() {
    static QElapsedTimer t; // started when the first change of a debounce burst arrives
    return t;
}

} // namespace

// ── Singleton ────────────────────────────────────────────────────────────────

ScreenStateManager& ScreenStateManager::instance() {
    static ScreenStateManager s;
    return s;
}

ScreenStateManager::ScreenStateManager(QObject* parent) : QObject(parent) {
    debounce_timer_ = new QTimer(this);
    debounce_timer_->setSingleShot(true);
    debounce_timer_->setInterval(kQuietMs);
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

    screen->restore_state(to_variant_map(obj));
    LOG_DEBUG("ScreenState", "Restored state for: " + key);
}

// ── Save now (async, fire-and-forget) ────────────────────────────────────────

void ScreenStateManager::save_now(screens::IStatefulScreen* screen) {
    if (!screen)
        return;

    // A bound screen saves to its panel row too, like every other save of it.
    const QString uuid = ssm_bound_uuid(screen);
    StateWrite w = capture_write(screen, uuid, session_id_);

    // Remove from pending so the debounce timer doesn't double-write.
    pending_saves_.remove(w.screen_key);
    if (!uuid.isEmpty())
        pending_uuid_saves_.remove(uuid);

    dispatch_write(std::move(w));
    LOG_DEBUG("ScreenState", "save_now dispatched for: " + screen->state_key());
}

// ── Notify changed (debounced) ───────────────────────────────────────────────

void ScreenStateManager::notify_changed(screens::IStatefulScreen* screen) {
    if (!screen)
        return;
    const QString uuid = ssm_bound_uuid(screen);
    if (!uuid.isEmpty())
        pending_uuid_saves_.insert(uuid, screen);
    else
        pending_saves_.insert(screen->state_key(), screen);
    ssm_guard_pending(screen);
    arm_debounce();
}

void ScreenStateManager::arm_debounce() {
    QElapsedTimer& age = ssm_pending_age();
    if (!debounce_timer_->isActive()) {
        age.start();
        debounce_timer_->start(kQuietMs);
        return;
    }
    // Restart the quiet period — unless this burst has already waited
    // kMaxDelayMs, in which case the running timer (≤ kQuietMs left) fires.
    if (age.isValid() && age.elapsed() >= kMaxDelayMs)
        return;
    debounce_timer_->start(kQuietMs);
}

// ── Direct save/load (for screens that can't implement IStatefulScreen) ──────

void ScreenStateManager::save_direct(const QString& key, const QVariantMap& state, int version) {
    StateWrite w;
    w.screen_key = key;
    w.state = state;
    w.version = version;
    w.session_id = session_id_;
    w.seq = ssm_next_seq();
    dispatch_write(std::move(w));
}

QVariantMap ScreenStateManager::load_direct(const QString& key, int expected_version) {
    auto result = TabSessionStore::instance().load_screen_state(key, expected_version);
    if (result.is_err() || result.value().isEmpty())
        return {};
    return to_variant_map(result.value());
}

// ── Private: flush pending ───────────────────────────────────────────────────

namespace {
/// Captures one write per pending screen. A screen queued under its state key
/// before it was bound (a constructor-time change) is saved under its panel
/// now; a screen queued twice is captured once.
QList<StateWrite> take_pending_writes(QHash<QString, screens::IStatefulScreen*>& legacy,
                                      QHash<QString, screens::IStatefulScreen*>& by_uuid, const QString& session_id) {
    // Snapshot + clear each table first so notify_* calls made while the
    // writes run don't get clobbered by this flush.
    const auto legacy_snapshot = legacy;
    legacy.clear();
    const auto uuid_snapshot = by_uuid;
    by_uuid.clear();

    QList<StateWrite> writes;
    QSet<const screens::IStatefulScreen*> captured;
    for (auto it = uuid_snapshot.constBegin(); it != uuid_snapshot.constEnd(); ++it) {
        screens::IStatefulScreen* screen = it.value();
        if (!screen || !ssm_pending_screen_alive(screen) || captured.contains(screen))
            continue; // destroyed inside the debounce window
        captured.insert(screen);
        writes.append(capture_write(screen, it.key(), session_id));
    }
    for (auto it = legacy_snapshot.constBegin(); it != legacy_snapshot.constEnd(); ++it) {
        screens::IStatefulScreen* screen = it.value();
        if (!screen || !ssm_pending_screen_alive(screen) || captured.contains(screen))
            continue;
        captured.insert(screen);
        writes.append(capture_write(screen, ssm_bound_uuid(screen), session_id));
    }
    ssm_pending_guards().clear(); // every queued screen has been handled or skipped
    ssm_prune_bindings();
    return writes;
}
} // namespace

void ScreenStateManager::flush_pending() {
    if (pending_saves_.isEmpty() && pending_uuid_saves_.isEmpty())
        return;
    QList<StateWrite> writes = take_pending_writes(pending_saves_, pending_uuid_saves_, session_id_);
    const int n = static_cast<int>(writes.size());
    for (auto& w : writes)
        dispatch_write(std::move(w));
    LOG_DEBUG("ScreenState", QString("Flushed %1 debounced screen state(s)").arg(n));
}

void ScreenStateManager::flush_pending_sync() {
    debounce_timer_->stop();
    if (pending_saves_.isEmpty() && pending_uuid_saves_.isEmpty())
        return;
    for (screens::IStatefulScreen* screen : pending_saves_) {
        if (screen && ssm_pending_screen_alive(screen))
            screen->flush_pending_state();
    }
    for (screens::IStatefulScreen* screen : pending_uuid_saves_) {
        if (screen && ssm_pending_screen_alive(screen))
            screen->flush_pending_state();
    }
    const QList<StateWrite> writes = take_pending_writes(pending_saves_, pending_uuid_saves_, session_id_);
    for (const auto& w : writes)
        perform_write(w);
    LOG_DEBUG("ScreenState", QString("flush_pending_sync: wrote %1 debounced screen state(s)").arg(writes.size()));
}

// ── Synchronous saves (shutdown only) ────────────────────────────────────────

void ScreenStateManager::save_now_sync(screens::IStatefulScreen* screen) {
    if (!screen)
        return;
    screen->flush_pending_state();
    const QString uuid = ssm_bound_uuid(screen);
    const StateWrite w = capture_write(screen, uuid, session_id_);
    pending_saves_.remove(w.screen_key);
    if (!uuid.isEmpty())
        pending_uuid_saves_.remove(uuid);
    perform_write(w);
}

void ScreenStateManager::save_now_by_uuid_sync(screens::IStatefulScreen* screen, const QString& instance_uuid) {
    if (!screen || instance_uuid.isEmpty())
        return;
    screen->flush_pending_state();
    ssm_bind(screen, instance_uuid);
    const StateWrite w = capture_write(screen, instance_uuid, session_id_);
    pending_uuid_saves_.remove(instance_uuid);
    if (pending_saves_.value(w.screen_key) == screen)
        pending_saves_.remove(w.screen_key);
    perform_write(w);
}

// ── UUID-keyed public API ────────────────────────────────────────────────────

void ScreenStateManager::restore_by_uuid(screens::IStatefulScreen* screen, const QString& instance_uuid,
                                         bool fallback_to_screen_key) {
    if (!screen)
        return;
    const int expected = screen->state_version();
    ssm_bind(screen, instance_uuid);

    auto& store = TabSessionStore::instance();
    ScreenStateRow own;
    auto by_uuid = store.load_screen_state_row_by_uuid(instance_uuid, expected);
    if (by_uuid.is_ok()) {
        own = by_uuid.value();
    } else {
        // Still consider the screen-type row — a transient DB error (SQLITE_BUSY,
        // corrupt WAL) should not prevent the fallback when one exists.
        LOG_WARN("ScreenState",
                 QString("UUID load failed for %1: %2").arg(instance_uuid, QString::fromStdString(by_uuid.error())));
    }

    ScreenStateRow type_row;
    const QString screen_key = screen->state_key();
    if (fallback_to_screen_key && !screen_key.isEmpty()) {
        auto legacy = store.load_screen_state_row(screen_key, expected);
        if (legacy.is_ok())
            type_row = legacy.value();
    }

    const session::StoredScreenState own_meta{own.found, own.updated_at, {}};
    const session::StoredScreenState type_meta{type_row.found, type_row.updated_at, type_row.writer_uuid};
    const session::RestoreSource source = session::choose_restore_source(own_meta, type_meta, instance_uuid);

    QJsonObject obj;
    switch (source) {
        case session::RestoreSource::None:
            LOG_DEBUG("ScreenState", QString("No saved state for uuid=%1").arg(instance_uuid));
            return;
        case session::RestoreSource::Instance:
            obj = own.state;
            break;
        case session::RestoreSource::ScreenType:
            obj = type_row.state;
            if (own.found)
                LOG_INFO("ScreenState", QString("Restoring '%1' (uuid=%2) from the newer screen-type row (%3) — the "
                                                "panel row (%4) is older")
                                            .arg(screen_key, instance_uuid, type_row.updated_at, own.updated_at));
            break;
    }
    if (obj.isEmpty())
        return;

    screen->restore_state(to_variant_map(obj));
    LOG_DEBUG("ScreenState", QString("Restored state for uuid=%1").arg(instance_uuid));
}

void ScreenStateManager::save_now_by_uuid(screens::IStatefulScreen* screen, const QString& instance_uuid) {
    if (!screen || instance_uuid.isEmpty())
        return;

    ssm_bind(screen, instance_uuid);
    StateWrite w = capture_write(screen, instance_uuid, session_id_);
    pending_uuid_saves_.remove(instance_uuid);
    if (pending_saves_.value(w.screen_key) == screen)
        pending_saves_.remove(w.screen_key);
    dispatch_write(std::move(w));
    LOG_DEBUG("ScreenState", QString("save_now_by_uuid dispatched for %1").arg(instance_uuid));
}

void ScreenStateManager::notify_changed_by_uuid(screens::IStatefulScreen* screen, const QString& instance_uuid) {
    if (!screen || instance_uuid.isEmpty())
        return;
    ssm_bind(screen, instance_uuid);
    pending_uuid_saves_.insert(instance_uuid, screen);
    ssm_guard_pending(screen);
    arm_debounce();
}

void ScreenStateManager::erase_by_uuid(const QString& instance_uuid) {
    if (instance_uuid.isEmpty())
        return;
    pending_uuid_saves_.remove(instance_uuid);
    auto& bindings = ssm_bindings();
    for (auto it = bindings.begin(); it != bindings.end();) {
        if (it->uuid == instance_uuid)
            it = bindings.erase(it);
        else
            ++it;
    }
    // Run the delete async so the caller (closeEvent) doesn't block on disk.
    // Sequenced like a write: an older save still queued cannot resurrect the
    // row afterwards, and a later save is not undone by this delete.
    const quint64 seq = ssm_next_seq();
    std::ignore = QtConcurrent::run([instance_uuid, seq]() {
        QMutexLocker lock(&ssm_write_mutex());
        const QString row = instance_row_key(instance_uuid);
        if (!ssm_write_gate().admit(row, seq))
            return;
        auto r = TabSessionStore::instance().remove_screen_state_by_uuid(instance_uuid);
        if (r.is_ok())
            ssm_write_gate().mark_applied(row, seq);
        else
            LOG_WARN("ScreenState",
                     QString("erase_by_uuid failed for %1: %2").arg(instance_uuid, QString::fromStdString(r.error())));
    });
}

} // namespace fincept
