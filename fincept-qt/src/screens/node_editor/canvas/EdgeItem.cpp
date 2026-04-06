#include "screens/node_editor/canvas/EdgeItem.h"

#include "screens/node_editor/canvas/PortItem.h"
#include "ui/theme/ThemeManager.h"

#include <QAction>
#include <QGraphicsSceneContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QTimerEvent>

namespace fincept::workflow {

EdgeItem::EdgeItem(const QString& id, PortItem* source, PortItem* target, QGraphicsItem* parent)
    : QGraphicsPathItem(parent), id_(id), source_(source), target_(target) {
    setFlag(ItemIsSelectable);
    setFlag(ItemIsFocusable);
    setAcceptHoverEvents(true);
    setZValue(0); // edges behind nodes
    setPen(QPen(QColor(ui::ThemeManager::instance().tokens().border_med), 2.0, Qt::SolidLine, Qt::RoundCap));

    source_->add_edge(this);
    target_->add_edge(this);

    adjust();
}

EdgeItem::~EdgeItem() {
    if (source_)
        source_->remove_edge(this);
    if (target_)
        target_->remove_edge(this);
}

void EdgeItem::adjust() {
    if (!source_ || !target_)
        return;

    prepareGeometryChange();

    // source_ is always the Output port, target_ is always the Input port.
    // Output ports sit on the right side of their node, input ports on the left.
    // Always curve rightward from the output: control point 1 bows right from p1,
    // control point 2 bows left from p2 — giving a natural left-to-right S-less flow
    // regardless of node positions on the canvas.
    QPointF p1 = source_->scene_center();
    QPointF p2 = target_->scene_center();

    qreal dx = std::abs(p2.x() - p1.x()) * 0.5;
    if (dx < 60.0)
        dx = 60.0;

    QPainterPath path;
    path.moveTo(p1);
    path.cubicTo(p1.x() + dx, p1.y(), p2.x() - dx, p2.y(), p2.x(), p2.y());
    setPath(path);
}

void EdgeItem::set_animated(bool animated) {
    if (animated_ == animated)
        return;
    animated_ = animated;

    if (animated_) {
        dash_offset_ = 0.0;
        anim_timer_id_ = startTimer(50); // 20fps per P9
    } else {
        if (anim_timer_id_) {
            killTimer(anim_timer_id_);
            anim_timer_id_ = 0;
        }
        dash_offset_ = 0.0;
    }
    update();
}

void EdgeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    if (!source_ || !target_)
        return;

    painter->setRenderHint(QPainter::Antialiasing);

    const auto& t = ui::ThemeManager::instance().tokens();
    QColor color;
    if (animated_)
        color = QColor(t.accent);
    else if (isSelected())
        color = QColor(t.accent);
    else
        color = QColor(t.border_med);

    QPen pen(color, 2.0, Qt::SolidLine, Qt::RoundCap);
    if (animated_) {
        pen.setStyle(Qt::CustomDashLine);
        pen.setDashPattern({6.0, 4.0}); // 6px dash, 4px gap
        pen.setDashOffset(dash_offset_);
    }
    painter->setPen(pen);
    painter->drawPath(path());
}

void EdgeItem::timerEvent(QTimerEvent* event) {
    if (event->timerId() == anim_timer_id_) {
        dash_offset_ -= 1.0; // negative offset advances dashes along path direction (left → right)
        update();
    }
}

void EdgeItem::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        emit delete_requested(id_);
        event->accept();
    } else {
        QGraphicsPathItem::keyPressEvent(event);
    }
}

void EdgeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
    const auto& tm = ui::ThemeManager::instance().tokens();
    QMenu menu;
    menu.setStyleSheet(
        QString("QMenu { background: %1; color: %2; border: 1px solid %3;"
                "  font-family: Consolas; font-size: 12px; }"
                "QMenu::item { padding: 4px 20px; }"
                "QMenu::item:selected { background: %4; color: %5; }")
        .arg(tm.bg_raised, tm.text_primary, tm.border_dim, tm.negative, tm.text_primary));
    auto* del = menu.addAction("Delete Connection");
    if (menu.exec(event->screenPos()) == del)
        emit delete_requested(id_);
    event->accept();
}

QVariant EdgeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemSelectedHasChanged && value.toBool())
        emit edge_selected(id_);
    return QGraphicsPathItem::itemChange(change, value);
}

} // namespace fincept::workflow
