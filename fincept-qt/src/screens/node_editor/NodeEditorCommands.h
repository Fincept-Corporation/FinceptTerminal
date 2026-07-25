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
//
// Deliberately NOT on the stack: per-keystroke parameter and node-name edits
// from the properties panel. Those fire on every character typed; putting them
// on the stack would either flood it or require rebuilding the properties
// editor on each merge, stealing focus mid-typing. Text edits are undone with
// the field's own Ctrl+Z while it has focus.
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
#include <QPointF>
#include <QSet>
#include <QString>
#include <QUndoCommand>
#include <QVector>

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
    void undo() override { drop_nodes(scene_, nodes_); }

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

    void redo() override { drop_nodes(scene_, nodes_); }
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
        if (scene_)
            scene_->deserialize(before_);
    }

  private:
    NodeScene* scene_ = nullptr;
    WorkflowDef before_;
    WorkflowDef after_;
};

} // namespace commands

} // namespace fincept::workflow
