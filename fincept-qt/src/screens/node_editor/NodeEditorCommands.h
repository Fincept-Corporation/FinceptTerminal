#pragma once
//
// NodeEditorCommands.h — QUndoCommand set for the node editor canvas.
//
// Header-only on purpose: it is included from NodeEditorScreen.cpp only, so it
// needs no entry in the build's source list.
//
// ── Coverage contract ────────────────────────────────────────────────────────
// The UNDO/REDO buttons and Ctrl+Z / Ctrl+Y cover every *structural* mutation of
// the graph:
//
//   • add node        (palette drag, canvas context menu, paste, duplicate,
//                      template load)
//   • remove node     (Delete key, node context menu, properties DEL button)
//   • add edge        (drag-to-connect, paste)
//   • remove edge     (edge context menu, Delete on a selected edge)
//   • move node(s)    (one command per drag, multi-select aware)
//   • replace graph   (CLEAR, template load)
//   • parameter / name edits from the properties panel (SetNodeFieldCommand):
//     consecutive keystrokes into the same field merge into ONE step, and the
//     panel is only rebuilt on undo/redo — never on the push that records an
//     edit, so typing keeps its focus. Without this, Ctrl+Z after editing a
//     parameter reverted the previous structural change and left the edit.
//
// Every command holds value types (NodeDef / EdgeDef / WorkflowDef) plus a bare
// NodeScene*. The scene outlives the stack: both are children of
// NodeEditorScreen and the stack is constructed first, so it is destroyed first.

#include "screens/node_editor/NodeEditorTypes.h"
#include "screens/node_editor/canvas/NodeItem.h"
#include "screens/node_editor/canvas/NodeScene.h"
#include "services/workflow/NodeRegistry.h"

#include <QCoreApplication>
#include <QHash>
#include <QJsonValue>
#include <QPointF>
#include <QSet>
#include <QString>
#include <QUndoCommand>
#include <QVector>

#include <functional>
#include <utility>

namespace fincept::workflow {

namespace commands {

/// Re-add a set of nodes (resolving their type from the registry) and then the
/// edges between them. Shared by add/remove/undo paths.
inline void apply_nodes(NodeScene* scene, const QVector<NodeDef>& nodes, const QVector<EdgeDef>& edges) {
    if (!scene)
        return;
    auto& reg = NodeRegistry::instance();
    for (const auto& nd : nodes) {
        const auto* td = reg.find(nd.type);
        if (td)
            scene->add_node(nd, *td);
    }
    // Edges last: both endpoints must exist.
    for (const auto& ed : edges)
        scene->add_edge(ed);
}

inline void drop_nodes(NodeScene* scene, const QVector<NodeDef>& nodes) {
    if (!scene)
        return;
    for (const auto& nd : nodes)
        scene->remove_node(nd.id); // cascades its edges
}

/// Refresh `nodes` from the live scene before they are dropped. Parameter, name and
/// flag edits are not undo commands, so a node pasted, then edited, then undone and
/// redone used to come back with its pre-edit parameters. Capturing at drop time makes
/// redo/undo restore exactly what was removed.
inline void capture_live(NodeScene* scene, QVector<NodeDef>& nodes) {
    if (!scene)
        return;
    for (auto& nd : nodes) {
        if (auto* item = scene->find_node(nd.id)) {
            nd = item->node_def();
            nd.x = item->pos().x();
            nd.y = item->pos().y();
        }
    }
}

/// Collect the edges that touch any node in `ids` (so a delete can be undone
/// with its wiring intact).
inline QVector<EdgeDef> edges_touching(NodeScene* scene, const QSet<QString>& ids) {
    QVector<EdgeDef> out;
    if (!scene)
        return out;
    const WorkflowDef wf = scene->serialize();
    for (const auto& ed : wf.edges)
        if (ids.contains(ed.source_node) || ids.contains(ed.target_node))
            out.append(ed);
    return out;
}

// ── Add nodes (+ their internal edges) ───────────────────────────────────────

class AddNodesCommand : public QUndoCommand {
    Q_DECLARE_TR_FUNCTIONS(AddNodesCommand)
  public:
    AddNodesCommand(NodeScene* scene, QVector<NodeDef> nodes, QVector<EdgeDef> edges, const QString& label)
        : scene_(scene), nodes_(std::move(nodes)), edges_(std::move(edges)) {
        setText(label);
    }

    void redo() override { apply_nodes(scene_, nodes_, edges_); }
    void undo() override {
        capture_live(scene_, nodes_);
        drop_nodes(scene_, nodes_);
    }

  private:
    NodeScene* scene_ = nullptr;
    QVector<NodeDef> nodes_;
    QVector<EdgeDef> edges_;
};

// ── Remove nodes (+ every edge that touched them) ────────────────────────────

class RemoveNodesCommand : public QUndoCommand {
    Q_DECLARE_TR_FUNCTIONS(RemoveNodesCommand)
  public:
    RemoveNodesCommand(NodeScene* scene, QVector<NodeDef> nodes, QVector<EdgeDef> edges, const QString& label)
        : scene_(scene), nodes_(std::move(nodes)), edges_(std::move(edges)) {
        setText(label);
    }

    void redo() override {
        capture_live(scene_, nodes_);
        drop_nodes(scene_, nodes_);
    }
    void undo() override { apply_nodes(scene_, nodes_, edges_); }

  private:
    NodeScene* scene_ = nullptr;
    QVector<NodeDef> nodes_;
    QVector<EdgeDef> edges_;
};

// ── Single edge add / remove ─────────────────────────────────────────────────

class AddEdgeCommand : public QUndoCommand {
    Q_DECLARE_TR_FUNCTIONS(AddEdgeCommand)
  public:
    AddEdgeCommand(NodeScene* scene, EdgeDef edge) : scene_(scene), edge_(std::move(edge)) {
        setText(tr("Connect nodes"));
    }
    void redo() override {
        if (scene_)
            scene_->add_edge(edge_);
    }
    void undo() override {
        if (scene_)
            scene_->remove_edge(edge_.id);
    }

  private:
    NodeScene* scene_ = nullptr;
    EdgeDef edge_;
};

class RemoveEdgeCommand : public QUndoCommand {
    Q_DECLARE_TR_FUNCTIONS(RemoveEdgeCommand)
  public:
    RemoveEdgeCommand(NodeScene* scene, EdgeDef edge) : scene_(scene), edge_(std::move(edge)) {
        setText(tr("Delete connection"));
    }
    void redo() override {
        if (scene_)
            scene_->remove_edge(edge_.id);
    }
    void undo() override {
        if (scene_)
            scene_->add_edge(edge_);
    }

  private:
    NodeScene* scene_ = nullptr;
    EdgeDef edge_;
};

// ── Move node(s) ─────────────────────────────────────────────────────────────
// One command per drag gesture, covering every node the drag moved (a
// multi-select drag moves several items but only one receives the mouse events,
// so the screen diffs a whole-scene position snapshot).

class MoveNodesCommand : public QUndoCommand {
    Q_DECLARE_TR_FUNCTIONS(MoveNodesCommand)
  public:
    MoveNodesCommand(NodeScene* scene, QHash<QString, QPointF> before, QHash<QString, QPointF> after)
        : scene_(scene), before_(std::move(before)), after_(std::move(after)) {
        setText(after_.size() > 1 ? tr("Move %1 nodes").arg(after_.size()) : tr("Move node"));
    }

    // The drag already left the nodes at `after_`, so the push-time redo() is a
    // harmless re-assert of the same positions.
    void redo() override { apply(after_); }
    void undo() override { apply(before_); }

  private:
    void apply(const QHash<QString, QPointF>& positions) {
        if (!scene_)
            return;
        for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
            if (auto* item = scene_->find_node(it.key()))
                item->setPos(it.value()); // itemChange keeps NodeDef x/y and edges in sync
        }
    }

    NodeScene* scene_ = nullptr;
    QHash<QString, QPointF> before_;
    QHash<QString, QPointF> after_;
};

// ── Parameter / name edit ────────────────────────────────────────────────────
// `key` empty = the node's name. `on_applied(node_id)` runs after an undo / redo
// (not after the push-time redo: the edit already came from the panel) so the
// screen can re-show the properties panel with the restored value.

class SetNodeFieldCommand : public QUndoCommand {
    Q_DECLARE_TR_FUNCTIONS(SetNodeFieldCommand)
  public:
    SetNodeFieldCommand(NodeScene* scene, QString node_id, QString key, QJsonValue before, QJsonValue after,
                        std::function<void(const QString&)> on_applied)
        : scene_(scene), node_id_(std::move(node_id)), key_(std::move(key)), before_(std::move(before)),
          after_(std::move(after)), on_applied_(std::move(on_applied)) {
        setText(key_.isEmpty() ? tr("Rename node") : tr("Edit %1").arg(key_));
    }

    int id() const override { return 0x4e46; } // mergeable with other field edits
    bool mergeWith(const QUndoCommand* other) override {
        const auto* o = dynamic_cast<const SetNodeFieldCommand*>(other);
        if (!o || o->node_id_ != node_id_ || o->key_ != key_)
            return false;
        after_ = o->after_; // keep the oldest "before": one undo step per field edit
        return true;
    }

    void redo() override {
        apply(after_);
        if (pushed_ && on_applied_)
            on_applied_(node_id_);
        pushed_ = true;
    }
    void undo() override {
        apply(before_);
        if (on_applied_)
            on_applied_(node_id_);
    }

  private:
    void apply(const QJsonValue& v) {
        auto* item = scene_ ? scene_->find_node(node_id_) : nullptr;
        if (!item)
            return;
        if (key_.isEmpty())
            item->set_name(v.toString());
        else
            item->set_parameter(key_, v);
    }

    NodeScene* scene_ = nullptr;
    QString node_id_;
    QString key_;
    QJsonValue before_;
    QJsonValue after_;
    std::function<void(const QString&)> on_applied_;
    bool pushed_ = false;
};

// ── Whole-graph replacement (CLEAR, template load) ───────────────────────────

class ReplaceGraphCommand : public QUndoCommand {
    Q_DECLARE_TR_FUNCTIONS(ReplaceGraphCommand)
  public:
    ReplaceGraphCommand(NodeScene* scene, WorkflowDef before, WorkflowDef after, const QString& label)
        : scene_(scene), before_(std::move(before)), after_(std::move(after)) {
        setText(label);
    }

    // For the template path the scene already holds `after_`; deserializing it
    // again rebuilds an identical graph, so push()'s implicit redo() is a no-op
    // in effect.
    void redo() override {
        if (scene_)
            scene_->deserialize(after_);
    }
    void undo() override {
        if (!scene_)
            return;
        after_ = scene_->serialize(); // keep edits made after the swap for redo
        scene_->deserialize(before_);
    }

  private:
    NodeScene* scene_ = nullptr;
    WorkflowDef before_;
    WorkflowDef after_;
};

} // namespace commands

} // namespace fincept::workflow
