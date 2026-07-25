#pragma once

#include "screens/node_editor/NodeEditorTypes.h"

#include <QGraphicsScene>
#include <QMap>
#include <QTimer>

namespace fincept::workflow {

class NodeItem;
class EdgeItem;
class PortItem;
class TempEdge;

/// QGraphicsScene managing all node editor items.
class NodeScene : public QGraphicsScene {
    Q_OBJECT
  public:
    explicit NodeScene(QObject* parent = nullptr);

    /// Add a node to the scene and return its item.
    NodeItem* add_node(const NodeDef& def, const NodeTypeDef& type_def);

    /// Add an edge between two ports.
    EdgeItem* add_edge(const EdgeDef& def);

    /// Remove a node and all its connected edges.
    void remove_node(const QString& node_id);

    /// Remove an edge.
    void remove_edge(const QString& edge_id);

    /// Clear all items.
    void clear_all();

    /// Serialize current scene to a WorkflowDef.
    WorkflowDef serialize() const;

    /// Deserialize from a WorkflowDef, replacing all items.
    void deserialize(const WorkflowDef& workflow);

    /// Find a node item by id.
    NodeItem* find_node(const QString& id) const;

    /// All node items.
    QVector<NodeItem*> node_items() const;

    /// True when no nodes are placed. Cheap — unlike items().isEmpty(), which
    /// materialises a QList of every item (nodes, ports, edges) on each call and
    /// was being invoked from drawForeground() on every canvas repaint.
    bool is_empty() const { return nodes_.isEmpty(); }

    /// Look up an edge's definition by id. Returns an EdgeDef with an empty id
    /// when not found. Used to capture undo state before a delete.
    EdgeDef edge_def(const QString& edge_id) const;

    /// Begin temporary edge from a port (during drag-to-connect).
    void start_temp_edge(PortItem* from);

    /// Update temporary edge endpoint.
    void update_temp_edge(const QPointF& scene_pos);

    /// Finish temporary edge — attempt to create real edge.
    void finish_temp_edge(PortItem* target);

    /// Cancel temporary edge.
    void cancel_temp_edge();

  signals:
    void node_added(const QString& id);
    void node_removed(const QString& id);
    void edge_added(const QString& id);
    void edge_removed(const QString& id);
    void node_selection_changed(const QString& id);
    void node_position_changed(const QString& id, double x, double y);
    void node_duplicate_requested(const QString& id);
    void node_execute_from_requested(const QString& id);

    // ── Undoable-intent signals ──────────────────────────────────────────────
    // User-initiated mutations are announced rather than applied here, so
    // NodeEditorScreen can route them through the QUndoStack. Applying them
    // directly is what left the undo stack permanently empty (and the UNDO /
    // REDO buttons permanently disabled).
    void node_delete_requested(const QString& id);
    void edge_delete_requested(const QString& id);
    void edge_create_requested(const fincept::workflow::EdgeDef& edge);
    /// A node drag began / ended (multi-select drags report from the item under
    /// the cursor only — the screen diffs a whole-scene position snapshot).
    void node_move_started();
    void node_move_finished();

  public:
    /// Animate edges connected to a node (during execution).
    void set_edges_animated(const QString& node_id, bool animated);
    /// Stop all edge animations (clears animated state).
    void stop_all_edge_animations();
    /// Pause/resume the animation timer without clearing animated state.
    void pause_edge_animations();
    void resume_edge_animations();
    bool has_animated_edges() const { return animated_edge_count_ > 0; }

  private:
    void adjust_edges_for_node(const QString& node_id);
    void tick_edge_animations();

    QMap<QString, NodeItem*> nodes_;
    QMap<QString, EdgeItem*> edges_;
    TempEdge* temp_edge_ = nullptr;
    QTimer anim_timer_;
    int animated_edge_count_ = 0;
};

} // namespace fincept::workflow
