#pragma once

#include "screens/node_editor/NodeEditorTypes.h"

#include <QGraphicsScene>
#include <QMap>

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

  public:
    /// Animate edges connected to a node (during execution).
    void set_edges_animated(const QString& node_id, bool animated);
    /// Stop all edge animations.
    void stop_all_edge_animations();

  private:
    void adjust_edges_for_node(const QString& node_id);

    QMap<QString, NodeItem*> nodes_;
    QMap<QString, EdgeItem*> edges_;
    TempEdge* temp_edge_ = nullptr;
};

} // namespace fincept::workflow
