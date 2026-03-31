#include "screens/node_editor/canvas/TempEdge.h"

#include "screens/node_editor/canvas/PortItem.h"

#include <QPainterPath>
#include <QPen>

#include <cmath>

namespace fincept::workflow {

TempEdge::TempEdge(PortItem* source, QGraphicsItem* parent) : QGraphicsPathItem(parent), source_(source) {
    setPen(QPen(QColor("#d97706"), 2.0, Qt::DashLine, Qt::RoundCap));
    setZValue(10); // above everything
}

void TempEdge::update_target(const QPointF& scene_pos) {
    QPointF p1 = source_->scene_center();
    QPointF p2 = scene_pos;

    qreal dx = std::abs(p2.x() - p1.x()) * 0.5;
    if (dx < 60.0)
        dx = 60.0;

    // Always bow toward the target: output ports exit rightward, input ports enter leftward.
    // When dragging from an output port: cp1 goes right, cp2 approaches from the left.
    // When dragging from an input port (connecting backwards): mirror so the curve still looks natural.
    bool from_output = (source_->def().direction == PortDirection::Output);
    qreal cx1 = from_output ? p1.x() + dx : p1.x() - dx;
    qreal cx2 = from_output ? p2.x() - dx : p2.x() + dx;

    QPainterPath path;
    path.moveTo(p1);
    path.cubicTo(cx1, p1.y(), cx2, p2.y(), p2.x(), p2.y());
    setPath(path);
}

} // namespace fincept::workflow
