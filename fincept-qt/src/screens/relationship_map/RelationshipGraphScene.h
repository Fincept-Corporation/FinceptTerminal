// src/screens/relationship_map/RelationshipGraphScene.h
#pragma once

#include "screens/relationship_map/RelationshipMapTypes.h"

#include <QGraphicsScene>
#include <QGraphicsView>

namespace fincept::relmap {

class GraphNodeItem;

/// Interactive graph scene for relationship map visualization.
/// Custom painted nodes with category colors, bezier edges, radial layout.
class RelationshipGraphScene : public QGraphicsScene {
    Q_OBJECT
  public:
    explicit RelationshipGraphScene(QObject* parent = nullptr);

    /// Build graph from data, applying layout and filters.
    void build_graph(const RelationshipData& data, const FilterState& filters, LayoutMode layout);

    /// Clear all items.
    void clear_graph();

  signals:
    void node_clicked(const GraphNode& node);
    void background_clicked();

  private:
    void apply_layout(QVector<GraphNode>& nodes, const QVector<GraphEdge>& edges, LayoutMode mode);
    void place_in_ring(const QVector<int>& indices, QVector<QPointF>& positions, double radius, double start_angle,
                       const QPointF& center);

    void add_node_item(const GraphNode& node, const QPointF& pos);
    void add_edge_item(const QPointF& from, const QPointF& to, EdgeCategory cat, const QString& label);

    QMap<QString, QPointF> node_positions_; // id → center position
};

/// Zooming/panning view for the graph.
class RelationshipGraphView : public QGraphicsView {
    Q_OBJECT
  public:
    explicit RelationshipGraphView(QGraphicsScene* scene, QWidget* parent = nullptr);

  protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
};

} // namespace fincept::relmap
