// src/screens/relationship_map/RelationshipGraphScene.h
#pragma once

#include "screens/relationship_map/RelationshipMapTypes.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QTimer>

namespace fincept::relmap {

class GraphNodeItem;
class RelNode;

/// Interactive graph scene for relationship map visualization.
/// Force-directed layout with elastic nodes (Bloomberg-style).
class RelationshipGraphScene : public QGraphicsScene {
    Q_OBJECT
  public:
    explicit RelationshipGraphScene(QObject* parent = nullptr);

    /// Build graph from data, applying layout and filters.
    void build_graph(const RelationshipData& data, const FilterState& filters, LayoutMode layout);

    /// Clear all items.
    void clear_graph();

  signals:
    void center_card_clicked(const QString& ticker);
    void background_clicked();

  private:
    // kept as no-ops for ABI stability — layout is now pure radial/deterministic
    void start_simulation();
    void simulation_step();

    void apply_layout(QVector<GraphNode>& nodes, const QVector<GraphEdge>& edges, LayoutMode mode);
    void place_in_ring(const QVector<int>& indices, QVector<QPointF>& positions, double radius, double start_angle,
                       const QPointF& center);

    void add_node_item(const GraphNode& node, const QPointF& pos);
    void add_edge_item(const QPointF& from, const QPointF& to, EdgeCategory cat, const QString& label);

    QMap<QString, QPointF> node_positions_;
    QVector<RelNode*> nodes_;
    QTimer* sim_timer_ = nullptr;
    int timer_ticks_ = 0;
};

/// Zooming/panning view for the graph.
class RelationshipGraphView : public QGraphicsView {
    Q_OBJECT
  public:
    explicit RelationshipGraphView(QGraphicsScene* scene, QWidget* parent = nullptr);

    void fit_to_content();
    void reset_zoom();

  signals:
    void zoom_changed(double factor);

  protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

  private:
    double current_zoom_{1.0};
    static constexpr double kMinZoom = 0.15;
    static constexpr double kMaxZoom = 4.0;
};

} // namespace fincept::relmap
