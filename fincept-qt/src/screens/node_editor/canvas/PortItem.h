#pragma once

#include "screens/node_editor/NodeEditorTypes.h"

#include <QGraphicsObject>
#include <QPen>

namespace fincept::workflow {

class NodeItem;
class EdgeItem;

/// A connection port rendered as a small circle on a NodeItem.
class PortItem : public QGraphicsObject {
    Q_OBJECT
  public:
    PortItem(const PortDef& def, NodeItem* parent_node);

    const PortDef& def() const { return def_; }
    NodeItem* parent_node() const { return parent_node_; }

    /// Center position in scene coordinates.
    QPointF scene_center() const;

    /// Whether this port can connect to another port.
    bool can_connect_to(const PortItem* other) const;

    /// Track connected edges.
    void add_edge(EdgeItem* edge);
    void remove_edge(EdgeItem* edge);
    const QVector<EdgeItem*>& edges() const { return edges_; }

    /// Set compatibility highlight during drag-to-connect.
    /// 0 = none, 1 = compatible (green), -1 = incompatible (red)
    void set_connect_highlight(int state);
    int connect_highlight() const { return connect_highlight_; }

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  signals:
    void connection_started(PortItem* from);
    void connection_finished(PortItem* from, PortItem* to);

  protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

  private:
    PortDef def_;
    NodeItem* parent_node_ = nullptr;
    QVector<EdgeItem*> edges_;
    bool hovered_ = false;
    int connect_highlight_ = 0;

    static constexpr qreal kVisualRadius = 5.0;
    static constexpr qreal kHitRadius = 8.0;
};

} // namespace fincept::workflow
