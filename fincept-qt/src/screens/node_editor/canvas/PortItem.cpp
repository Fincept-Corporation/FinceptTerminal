#include "screens/node_editor/canvas/PortItem.h"
#include "screens/node_editor/canvas/NodeItem.h"

#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>

namespace fincept::workflow {

PortItem::PortItem(const PortDef& def, NodeItem* parent_node)
    : QGraphicsObject(parent_node)
    , def_(def)
    , parent_node_(parent_node)
{
    setAcceptHoverEvents(true);
    setCursor(Qt::CrossCursor);
    setZValue(2);
    setToolTip(def_.label);
}

QPointF PortItem::scene_center() const
{
    return mapToScene(boundingRect().center());
}

bool PortItem::can_connect_to(const PortItem* other) const
{
    if (!other || other == this) return false;
    if (other->parent_node() == parent_node_) return false;
    if (def_.direction == other->def_.direction) return false;

    // Connection type compatibility:
    // - Same type always connects
    // - Main is universal — connects to/from any type
    // - Otherwise must match exactly
    auto a = def_.connection_type;
    auto b = other->def_.connection_type;
    if (a != b && a != ConnectionType::Main && b != ConnectionType::Main)
        return false;

    return true;
}

void PortItem::add_edge(EdgeItem* edge)
{
    if (!edges_.contains(edge))
        edges_.append(edge);
}

void PortItem::remove_edge(EdgeItem* edge)
{
    edges_.removeAll(edge);
}

QRectF PortItem::boundingRect() const
{
    return QRectF(-kHitRadius, -kHitRadius, kHitRadius * 2, kHitRadius * 2);
}

void PortItem::set_connect_highlight(int state)
{
    connect_highlight_ = state;
    update();
}

void PortItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing);

    const bool connected = !edges_.isEmpty();
    QColor fill;
    if (connect_highlight_ == 1)       fill = QColor("#16a34a"); // compatible green
    else if (connect_highlight_ == -1) fill = QColor("#dc2626"); // incompatible red
    else if (hovered_ || connected)    fill = QColor("#d97706"); // amber
    else                               fill = QColor("#4a4a4a"); // idle

    QColor border_color("#2a2a2a");
    qreal radius = kVisualRadius;
    if (hovered_ || connect_highlight_ != 0)
        radius = kVisualRadius + 1.0; // slightly larger on hover/highlight

    painter->setPen(QPen(border_color, 1.0));
    painter->setBrush(fill);
    painter->drawEllipse(QPointF(0, 0), radius, radius);
}

void PortItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    hovered_ = true;
    update();
    QGraphicsObject::hoverEnterEvent(event);
}

void PortItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    hovered_ = false;
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}

void PortItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit connection_started(this);
        event->accept();
    } else {
        QGraphicsObject::mousePressEvent(event);
    }
}

} // namespace fincept::workflow
