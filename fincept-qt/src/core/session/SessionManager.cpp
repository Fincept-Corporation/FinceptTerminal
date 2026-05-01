#include "core/session/SessionManager.h"

#include "app/TerminalShell.h"
#include "core/logging/Logger.h"
#include "core/screen/MonitorTopology.h"
#include "storage/workspace/WorkspaceSnapshotRing.h"

#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QScreen>
#include <QStringList>

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
    settings_.beginGroup("tab_state/" + tab_id);
    for (const auto& key : settings_.childKeys()) {
        result[key] = settings_.value(key);
    }
    settings_.endGroup();
    return result;
}

void SessionManager::save_geometry(int window_id, const QByteArray& geometry, const QByteArray& state) {
    const QString prefix = QString("window_%1/").arg(window_id);
    settings_.setValue(prefix + "geometry", geometry);
    settings_.setValue(prefix + "state", state);
    request_snapshot_(false);
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

void SessionManager::save_window_ids(const QList<int>& ids) {
    QVariantList v;
    v.reserve(ids.size());
    for (int id : ids)
        v.append(id);
    settings_.setValue("window_ids", v);
    request_snapshot_(false);
}

QList<int> SessionManager::load_window_ids() const {
    QList<int> out;
    const QVariantList v = settings_.value("window_ids").toList();
    out.reserve(v.size());
    for (const QVariant& x : v) {
        bool ok = false;
        const int id = x.toInt(&ok);
        if (ok)
            out.append(id);
    }
    return out;
}

void SessionManager::save_screen_name(int window_id, const QString& screen_name) {
    // Phase 6 trim: store both the legacy QScreen::name() (back-compat) and
    // the stable id (decision 5.2). On restore, prefer the stable id —
    // survives USB-C dock reorderings and Windows enumeration shuffles.
    // Legacy name kept so users downgrading don't lose their layout.
    const QString prefix = QString("window_%1/").arg(window_id);
    settings_.setValue(prefix + "screen_name", screen_name);

    // Resolve the stable id by walking screens for one whose QScreen::name()
    // matches what the caller passed. The caller is WindowFrame::closeEvent
    // which already has the QScreen* in hand and reads its name(), so this
    // back-mapping is a small price to keep the public API stable.
    if (auto* gapp = QGuiApplication::instance(); gapp != nullptr) {
        for (QScreen* s : QGuiApplication::screens()) {
            if (s && s->name() == screen_name) {
                const QString stable = MonitorTopology::stable_id(s);
                if (!stable.isEmpty()) {
                    settings_.setValue(prefix + "screen_stable_id", stable);
                }
                break;
            }
        }
    }

    request_snapshot_(false);
}

QString SessionManager::load_screen_name(int window_id) const {
    // Phase 6 trim: prefer stable id lookup, fall back to legacy
    // QScreen::name(). Stable-id resolution: walk current screens, return
    // the QScreen::name() of whichever has the matching stable id. If no
    // current screen matches (monitor disconnected since save), fall back
    // to the legacy name field — caller may further fall back to primary
    // screen. The double-write in save_screen_name guarantees both keys
    // are present after Phase 6 ships.
    const QString prefix = QString("window_%1/").arg(window_id);
    const QString stable = settings_.value(prefix + "screen_stable_id").toString();
    if (!stable.isEmpty()) {
        for (QScreen* s : QGuiApplication::screens()) {
            if (s && MonitorTopology::stable_id(s) == stable)
                return s->name();
        }
    }
    return settings_.value(prefix + "screen_name").toString();
}

void SessionManager::save_window_flag(int window_id, const QString& name, bool value) {
    const QString key = QString("window_%1/flags/%2").arg(window_id).arg(name);
    settings_.setValue(key, value);
}

bool SessionManager::load_window_flag(int window_id, const QString& name, bool default_value) const {
    const QString key = QString("window_%1/flags/%2").arg(window_id).arg(name);
    return settings_.value(key, default_value).toBool();
}

void SessionManager::save_dock_layout(int window_id, const QByteArray& layout) {
    const QString prefix = QString("window_%1/").arg(window_id);
    settings_.setValue(prefix + "dock_layout", layout);
    request_snapshot_(false);
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

void SessionManager::save_perspectives(QSettings& source) {
    // Copy all keys from the source QSettings (written by ADS savePerspectives)
    // into our own settings_ under a "perspectives/" prefix.
    source.beginGroup("Perspectives");
    const QStringList keys = source.allKeys();
    settings_.beginGroup("perspectives");
    settings_.remove(""); // clear previous
    for (const QString& k : keys)
        settings_.setValue(k, source.value(k));
    settings_.endGroup();
    source.endGroup();
}

void SessionManager::load_perspectives(QSettings& target) const {
    settings_.beginGroup("perspectives");
    const QStringList keys = settings_.allKeys();
    target.beginGroup("Perspectives");
    for (const QString& k : keys)
        target.setValue(k, settings_.value(k));
    target.endGroup();
    settings_.endGroup();
}

void SessionManager::set_last_screen(const QString& screen_id) {
    settings_.setValue("session/last_screen", screen_id);
}

QString SessionManager::last_screen() const {
    return settings_.value("session/last_screen", "dashboard").toString();
}

void SessionManager::flush_snapshot_now() {
    request_snapshot_(/*force=*/true);
}

void SessionManager::request_snapshot_(bool force) {
    // The shell may not be initialised yet during very early ProfileManager
    // accesses (some legacy paths read settings before main.cpp constructs
    // the shell). In that case there's no WorkspaceDb to write to — silently
    // skip; QSettings already has the data, no work is lost.
    auto* ring = TerminalShell::instance().snapshot_ring();
    if (!ring)
        return;

    const QByteArray payload = build_snapshot_payload_();
    if (payload.isEmpty())
        return; // QSettings empty — nothing useful to snapshot.

    // For non-forced calls the ring rate-limits to 60s; this is the auto-
    // save throttle. Forced flushes (closeEvent, manual layout-save) write
    // immediately. Either way, ring trims oldest auto rows when we exceed
    // the ring size.
    auto r = ring->add(payload, "auto", force);
    if (r.is_err()) {
        LOG_WARN("SessionManager", QString("Snapshot write failed: %1")
                                       .arg(QString::fromStdString(r.error())));
    }
}

QByteArray SessionManager::build_snapshot_payload_() const {
    // Phase 2 payload format: a flat JSON object mirroring the QSettings
    // state that's relevant to multi-window restoration.
    //
    //   {
    //     "schema": "phase2-mirror",
    //     "window_ids": [0, 1, 3],
    //     "last_screen": "dashboard",
    //     "windows": {
    //       "0": { "geometry": "<base64>", "state": "<base64>",
    //              "dock_layout": "<base64>", "screen_name": "...",
    //              "flags": { "always_on_top": true } },
    //       ...
    //     }
    //   }
    //
    // Phase 6 swaps this for proper Workspace::to_json() once UUIDs and
    // FrameLayout/PanelState are wired through WindowFrame. Until then the
    // mirror format is what the recovery path will read back.
    QJsonObject root;
    root["schema"] = "phase2-mirror";

    // window_ids
    {
        QJsonArray arr;
        const QVariantList v = settings_.value("window_ids").toList();
        for (const QVariant& x : v) {
            bool ok = false;
            const int id = x.toInt(&ok);
            if (ok)
                arr.append(id);
        }
        root["window_ids"] = arr;
    }

    root["last_screen"] = settings_.value("session/last_screen", "dashboard").toString();

    // Per-window keys. Walk QSettings groups instead of taking a window-id
    // list as input — handles the case where QSettings has stale keys for
    // windows that no longer exist (we want them in the snapshot too so
    // recovery can offer to restore them).
    QJsonObject windows_obj;
    const QStringList top_groups = settings_.childGroups();
    for (const QString& g : top_groups) {
        if (!g.startsWith("window_"))
            continue;
        const QString id_str = g.mid(QStringLiteral("window_").size());
        bool ok = false;
        (void) id_str.toInt(&ok);
        if (!ok)
            continue;

        QJsonObject win;
        settings_.beginGroup(g);
        const QByteArray geom = settings_.value("geometry").toByteArray();
        if (!geom.isEmpty()) win["geometry"] = QString::fromLatin1(geom.toBase64());
        const QByteArray state = settings_.value("state").toByteArray();
        if (!state.isEmpty()) win["state"] = QString::fromLatin1(state.toBase64());
        const QByteArray dock = settings_.value("dock_layout").toByteArray();
        if (!dock.isEmpty()) win["dock_layout"] = QString::fromLatin1(dock.toBase64());
        const int dlv = settings_.value("dock_layout_version", 0).toInt();
        if (dlv != 0) win["dock_layout_version"] = dlv;
        const QString sn = settings_.value("screen_name").toString();
        if (!sn.isEmpty()) win["screen_name"] = sn;

        // Flags subgroup. QSettings nests flat under window_<id>/flags/<name>.
        QJsonObject flags;
        settings_.beginGroup("flags");
        const QStringList flag_keys = settings_.childKeys();
        for (const QString& fk : flag_keys)
            flags[fk] = settings_.value(fk).toBool();
        settings_.endGroup();
        if (!flags.isEmpty()) win["flags"] = flags;

        settings_.endGroup();
        if (!win.isEmpty())
            windows_obj[id_str] = win;
    }
    root["windows"] = windows_obj;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

} // namespace fincept
