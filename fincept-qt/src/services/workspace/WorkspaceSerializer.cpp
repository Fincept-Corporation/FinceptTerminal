#include "services/workspace/WorkspaceSerializer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

namespace fincept {

// ── Serialize ─────────────────────────────────────────────────────────────────

static QJsonObject serialize_metadata(const WorkspaceMetadata& m) {
    QJsonObject o;
    o["id"] = m.id;
    o["name"] = m.name;
    o["description"] = m.description;
    o["template_id"] = m.template_id;
    o["created_at"] = m.created_at;
    o["updated_at"] = m.updated_at;
    o["version"] = m.version;
    return o;
}

QJsonObject WorkspaceSerializer::serialize_layout(const screens::GridLayout& layout) {
    QJsonObject o;
    o["cols"] = layout.cols;
    o["row_h"] = layout.row_h;
    o["margin"] = layout.margin;

    QJsonArray items;
    for (const auto& item : layout.items) {
        QJsonObject it;
        it["id"] = item.id;
        it["instance_id"] = item.instance_id;
        it["x"] = item.cell.x;
        it["y"] = item.cell.y;
        it["w"] = item.cell.w;
        it["h"] = item.cell.h;
        it["min_w"] = item.cell.min_w;
        it["min_h"] = item.cell.min_h;
        it["is_static"] = item.is_static;
        items.append(QJsonValue(it));
    }
    o["items"] = QJsonValue(items);
    return o;
}

QJsonDocument WorkspaceSerializer::to_json(const WorkspaceDef& ws) {
    QJsonObject root;
    root["version"] = 1;
    root["metadata"] = QJsonValue(serialize_metadata(ws.metadata));

    QJsonObject nav;
    nav["active_screen"] = ws.active_screen;
    QJsonArray open;
    for (const auto& s : ws.open_screens)
        open.append(s);
    nav["open_screens"] = QJsonValue(open);
    root["navigation"] = QJsonValue(nav);

    root["dashboard"] = QJsonValue(serialize_layout(ws.dashboard_layout));

    QJsonObject refs;
    QJsonArray wl, pt, wf;
    for (const auto& id : ws.watchlist_ids)
        wl.append(id);
    for (const auto& id : ws.portfolio_ids)
        pt.append(id);
    for (const auto& id : ws.workflow_ids)
        wf.append(id);
    refs["watchlist_ids"] = QJsonValue(wl);
    refs["portfolio_ids"] = QJsonValue(pt);
    refs["workflow_ids"] = QJsonValue(wf);
    root["references"] = QJsonValue(refs);

    QJsonArray states;
    for (const auto& ss : ws.screen_states) {
        QJsonObject s;
        s["screen_id"] = ss.screen_id;
        s["state"] = QJsonValue(ss.state);
        states.append(QJsonValue(s));
    }
    root["screen_states"] = QJsonValue(states);
    root["window_geometry"] = ws.window_geometry_base64;
    root["symbol_context"] = QJsonValue(ws.symbol_context);

    return QJsonDocument(root);
}

// ── Deserialize ───────────────────────────────────────────────────────────────

screens::GridLayout WorkspaceSerializer::deserialize_layout(const QJsonObject& o) {
    screens::GridLayout layout;
    layout.cols = o["cols"].toInt(12);
    layout.row_h = o["row_h"].toInt(60);
    layout.margin = o["margin"].toInt(4);

    for (const auto& v : o["items"].toArray()) {
        QJsonObject it = v.toObject();
        screens::GridItem item;
        item.id = it["id"].toString();
        item.instance_id = it["instance_id"].toString();
        item.cell.x = it["x"].toInt();
        item.cell.y = it["y"].toInt();
        item.cell.w = it["w"].toInt(4);
        item.cell.h = it["h"].toInt(4);
        item.cell.min_w = it["min_w"].toInt(2);
        item.cell.min_h = it["min_h"].toInt(3);
        item.is_static = it["is_static"].toBool(false);
        layout.items.append(item);
    }
    return layout;
}

Result<WorkspaceDef> WorkspaceSerializer::from_json(const QJsonDocument& doc) {
    if (doc.isNull() || !doc.isObject())
        return Result<WorkspaceDef>::err("Invalid JSON document");

    QJsonObject root = doc.object();

    int version = root["version"].toInt(0);
    if (version != 1)
        return Result<WorkspaceDef>::err(QString("Unsupported workspace version: %1").arg(version).toStdString());

    WorkspaceDef ws;

    QJsonObject m = root["metadata"].toObject();
    ws.metadata.id = m["id"].toString();
    ws.metadata.name = m["name"].toString();
    ws.metadata.description = m["description"].toString();
    ws.metadata.template_id = m["template_id"].toString();
    ws.metadata.created_at = m["created_at"].toString();
    ws.metadata.updated_at = m["updated_at"].toString();
    ws.metadata.version = m["version"].toInt(1);

    if (ws.metadata.id.isEmpty() || ws.metadata.name.isEmpty())
        return Result<WorkspaceDef>::err("Workspace missing required id or name");

    QJsonObject nav = root["navigation"].toObject();
    ws.active_screen = nav["active_screen"].toString("dashboard");
    for (const auto& v : nav["open_screens"].toArray())
        ws.open_screens.append(v.toString());

    ws.dashboard_layout = deserialize_layout(root["dashboard"].toObject());

    QJsonObject refs = root["references"].toObject();
    for (const auto& v : refs["watchlist_ids"].toArray())
        ws.watchlist_ids.append(v.toString());
    for (const auto& v : refs["portfolio_ids"].toArray())
        ws.portfolio_ids.append(v.toString());
    for (const auto& v : refs["workflow_ids"].toArray())
        ws.workflow_ids.append(v.toString());

    for (const auto& v : root["screen_states"].toArray()) {
        QJsonObject s = v.toObject();
        WorkspaceScreenState ss;
        ss.screen_id = s["screen_id"].toString();
        ss.state = s["state"].toObject();
        if (!ss.screen_id.isEmpty())
            ws.screen_states.append(ss);
    }

    ws.window_geometry_base64 = root["window_geometry"].toString();
    ws.symbol_context = root["symbol_context"].toObject();

    return Result<WorkspaceDef>::ok(std::move(ws));
}

Result<WorkspaceSummary> WorkspaceSerializer::summary_from_file(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return Result<WorkspaceSummary>::err(QString("Cannot open: %1").arg(path).toStdString());

    QByteArray data = f.readAll();
    f.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull())
        return Result<WorkspaceSummary>::err("Invalid JSON");

    QJsonObject m = doc.object()["metadata"].toObject();
    if (m.isEmpty())
        return Result<WorkspaceSummary>::err("Missing metadata");

    WorkspaceSummary s;
    s.id = m["id"].toString();
    s.name = m["name"].toString();
    s.description = m["description"].toString();
    s.updated_at = m["updated_at"].toString();
    s.template_id = m["template_id"].toString();
    s.file_path = path;
    return Result<WorkspaceSummary>::ok(std::move(s));
}

} // namespace fincept
