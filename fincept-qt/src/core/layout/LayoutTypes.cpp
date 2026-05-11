#include "core/layout/LayoutTypes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace fincept::layout {

namespace {

// Tiny helpers — kept local because they only exist to avoid the same six
// lines repeating in every from_json. If they grow, lift to a header.
QJsonObject rect_to_json(const QRect& r) {
    QJsonObject o;
    o["x"] = r.x();
    o["y"] = r.y();
    o["w"] = r.width();
    o["h"] = r.height();
    return o;
}

QRect rect_from_json(const QJsonObject& o) {
    return QRect(o.value("x").toInt(), o.value("y").toInt(),
                 o.value("w").toInt(), o.value("h").toInt());
}

[[maybe_unused]] QString uuid_to_string(const QUuid& u) { return u.toString(QUuid::WithoutBraces); }

} // namespace

// ── PanelState ───────────────────────────────────────────────────────────────

QJsonObject PanelState::to_json() const {
    QJsonObject o;
    o["instance_id"] = instance_id.to_string();
    o["type_id"] = type_id;
    o["title"] = title;
    o["link_group"] = link_group;
    o["state_blob_b64"] = QString::fromLatin1(state_blob.toBase64());
    o["state_version"] = state_version;
    o["schema_version"] = schema_version;
    return o;
}

PanelState PanelState::from_json(const QJsonObject& obj) {
    PanelState s;
    s.instance_id = PanelInstanceId::from_string(obj.value("instance_id").toString());
    s.type_id = obj.value("type_id").toString();
    s.title = obj.value("title").toString();
    s.link_group = obj.value("link_group").toString();
    s.state_blob = QByteArray::fromBase64(obj.value("state_blob_b64").toString().toLatin1());
    s.state_version = obj.value("state_version").toInt();
    s.schema_version = obj.value("schema_version").toInt(kPanelStateVersion);
    return s;
}

// ── FrameLayout ──────────────────────────────────────────────────────────────

QJsonObject FrameLayout::to_json() const {
    QJsonObject o;
    o["window_id"] = window_id.to_string();
    o["name"] = name;
    QJsonArray panels_arr;
    for (const PanelState& p : panels)
        panels_arr.append(p.to_json());
    o["panels"] = panels_arr;
    o["dock_state_b64"] = QString::fromLatin1(dock_state.toBase64());
    o["active_panel"] = active_panel.to_string();
    o["focus_mode"] = focus_mode;
    o["always_on_top"] = always_on_top;
    o["schema_version"] = schema_version;
    return o;
}

FrameLayout FrameLayout::from_json(const QJsonObject& obj) {
    FrameLayout f;
    f.window_id = WindowId::from_string(obj.value("window_id").toString());
    f.name = obj.value("name").toString();
    const QJsonArray panels_arr = obj.value("panels").toArray();
    for (const QJsonValue& v : panels_arr)
        f.panels.append(PanelState::from_json(v.toObject()));
    f.dock_state = QByteArray::fromBase64(obj.value("dock_state_b64").toString().toLatin1());
    f.active_panel = PanelInstanceId::from_string(obj.value("active_panel").toString());
    f.focus_mode = obj.value("focus_mode").toBool();
    f.always_on_top = obj.value("always_on_top").toBool();
    f.schema_version = obj.value("schema_version").toInt(kFrameLayoutVersion);
    return f;
}

// ── WorkspaceVariant ─────────────────────────────────────────────────────────

QJsonObject WorkspaceVariant::to_json() const {
    QJsonObject o;
    o["topology"] = topology.to_json();

    QJsonArray geoms;
    for (auto it = frame_geometries.constBegin(); it != frame_geometries.constEnd(); ++it) {
        QJsonObject entry;
        entry["window_id"] = it.key().to_string();
        entry["rect"] = rect_to_json(it.value());
        geoms.append(entry);
    }
    o["frame_geometries"] = geoms;

    QJsonArray screens;
    for (auto it = frame_screens.constBegin(); it != frame_screens.constEnd(); ++it) {
        QJsonObject entry;
        entry["window_id"] = it.key().to_string();
        entry["screen_id"] = it.value();
        screens.append(entry);
    }
    o["frame_screens"] = screens;
    return o;
}

WorkspaceVariant WorkspaceVariant::from_json(const QJsonObject& obj) {
    WorkspaceVariant v;
    v.topology = MonitorTopologyKey::from_json(obj.value("topology"));

    const QJsonArray geoms = obj.value("frame_geometries").toArray();
    for (const QJsonValue& jv : geoms) {
        const QJsonObject entry = jv.toObject();
        const WindowId id = WindowId::from_string(entry.value("window_id").toString());
        if (id.is_null()) continue;
        v.frame_geometries.insert(id, rect_from_json(entry.value("rect").toObject()));
    }

    const QJsonArray screens = obj.value("frame_screens").toArray();
    for (const QJsonValue& jv : screens) {
        const QJsonObject entry = jv.toObject();
        const WindowId id = WindowId::from_string(entry.value("window_id").toString());
        if (id.is_null()) continue;
        v.frame_screens.insert(id, entry.value("screen_id").toString());
    }
    return v;
}

// ── Workspace ────────────────────────────────────────────────────────────────

QJsonObject Workspace::to_json() const {
    QJsonObject o;
    o["id"] = id.to_string();
    o["name"] = name;
    o["kind"] = kind;
    o["description"] = description;
    o["author"] = author;
    o["created_at"] = created_at_unix;
    o["updated_at"] = updated_at_unix;
    o["thumbnail_path"] = thumbnail_path;

    QJsonArray frames_arr;
    for (const FrameLayout& f : frames)
        frames_arr.append(f.to_json());
    o["frames"] = frames_arr;

    QJsonArray variants_arr;
    for (const WorkspaceVariant& v : variants)
        variants_arr.append(v.to_json());
    o["variants"] = variants_arr;

    QJsonArray link_arr;
    for (auto it = link_state.constBegin(); it != link_state.constEnd(); ++it) {
        QJsonObject entry;
        entry["group"] = it.key();
        entry["payload_b64"] = QString::fromLatin1(it.value().toBase64());
        link_arr.append(entry);
    }
    o["link_state"] = link_arr;

    o["schema_version"] = schema_version;
    return o;
}

Workspace Workspace::from_json(const QJsonObject& obj) {
    Workspace w;
    w.id = LayoutId::from_string(obj.value("id").toString());
    w.name = obj.value("name").toString();
    w.kind = obj.value("kind").toString("auto");
    w.description = obj.value("description").toString();
    w.author = obj.value("author").toString();
    w.created_at_unix = static_cast<qint64>(obj.value("created_at").toDouble());
    w.updated_at_unix = static_cast<qint64>(obj.value("updated_at").toDouble());
    w.thumbnail_path = obj.value("thumbnail_path").toString();

    const QJsonArray frames_arr = obj.value("frames").toArray();
    for (const QJsonValue& jv : frames_arr)
        w.frames.append(FrameLayout::from_json(jv.toObject()));

    const QJsonArray variants_arr = obj.value("variants").toArray();
    for (const QJsonValue& jv : variants_arr)
        w.variants.append(WorkspaceVariant::from_json(jv.toObject()));

    const QJsonArray link_arr = obj.value("link_state").toArray();
    for (const QJsonValue& jv : link_arr) {
        const QJsonObject entry = jv.toObject();
        const QString group = entry.value("group").toString();
        if (group.isEmpty()) continue;
        w.link_state.insert(group,
                            QByteArray::fromBase64(entry.value("payload_b64").toString().toLatin1()));
    }

    w.schema_version = obj.value("schema_version").toInt(kWorkspaceVersion);
    return w;
}

const WorkspaceVariant* Workspace::find_variant_exact(const MonitorTopologyKey& key) const {
    for (const WorkspaceVariant& v : variants) {
        if (v.topology == key)
            return &v;
    }
    return nullptr;
}

} // namespace fincept::layout
