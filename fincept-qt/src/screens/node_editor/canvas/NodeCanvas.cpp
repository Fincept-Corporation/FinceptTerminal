#include "screens/node_editor/canvas/NodeCanvas.h"
#include "screens/node_editor/canvas/NodeScene.h"
#include "screens/node_editor/canvas/PortItem.h"

#include "services/workflow/NodeRegistry.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFont>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include <cmath>

namespace fincept::workflow {

NodeCanvas::NodeCanvas(NodeScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent)
    , node_scene_(scene)
{
    setRenderHint(QPainter::Antialiasing, false);
    setViewportUpdateMode(MinimalViewportUpdate);
    setOptimizationFlag(DontSavePainterState);
    setCacheMode(CacheBackground);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setDragMode(RubberBandDrag);
    setAcceptDrops(true);
    setTransformationAnchor(AnchorUnderMouse);

    setStyleSheet(
        "QGraphicsView { background: #0d0d0d; border: none; }"
    );
}

// ── Zoom ───────────────────────────────────────────────────────────────

void NodeCanvas::wheelEvent(QWheelEvent* event)
{
    qreal factor = 1.0 + (event->angleDelta().y() / 1200.0);
    qreal current_scale = transform().m11();
    qreal new_scale = current_scale * factor;

    if (new_scale < kMinZoom) factor = kMinZoom / current_scale;
    if (new_scale > kMaxZoom) factor = kMaxZoom / current_scale;

    scale(factor, factor);
    event->accept();
}

// ── Pan (middle-click or Ctrl+left-click) ──────────────────────────────

void NodeCanvas::mousePressEvent(QMouseEvent* event)
{
    bool pan_trigger = (event->button() == Qt::MiddleButton) ||
                       (event->button() == Qt::LeftButton && event->modifiers() & Qt::ControlModifier);

    if (pan_trigger) {
        panning_ = true;
        last_pan_pos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    // Check if clicking on a port to start connecting
    auto* port = port_at(event->pos());
    if (port && event->button() == Qt::LeftButton) {
        connecting_ = true;
        // The port's mousePressEvent will emit connection_started
    }

    QGraphicsView::mousePressEvent(event);
}

void NodeCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (panning_) {
        QPoint delta = event->pos() - last_pan_pos_;
        last_pan_pos_ = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }

    if (connecting_) {
        QPointF scene_pos = mapToScene(event->pos());
        node_scene_->update_temp_edge(scene_pos);
    }

    QGraphicsView::mouseMoveEvent(event);
}

void NodeCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (panning_) {
        panning_ = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    if (connecting_) {
        connecting_ = false;
        auto* port = port_at(event->pos());
        if (port) {
            node_scene_->finish_temp_edge(port);
        } else {
            node_scene_->cancel_temp_edge();
        }
    }

    QGraphicsView::mouseReleaseEvent(event);
}

// ── Background (dot grid) ──────────────────────────────────────────────

void NodeCanvas::drawBackground(QPainter* painter, const QRectF& rect)
{
    painter->fillRect(rect, QColor("#0d0d0d"));

    qreal scale = transform().m11();
    qreal grid_size = 20.0;

    if (scale < 0.5) return; // skip dots at very low zoom

    int left   = static_cast<int>(std::floor(rect.left()   / grid_size) * grid_size);
    int top    = static_cast<int>(std::floor(rect.top()    / grid_size) * grid_size);
    int right  = static_cast<int>(std::ceil(rect.right()   / grid_size) * grid_size);
    int bottom = static_cast<int>(std::ceil(rect.bottom()  / grid_size) * grid_size);

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor("#2a2a2a"));

    qreal dot_size = 1.5 / scale;
    for (int x = left; x <= right; x += static_cast<int>(grid_size)) {
        for (int y = top; y <= bottom; y += static_cast<int>(grid_size)) {
            painter->drawRect(QRectF(x - dot_size / 2, y - dot_size / 2, dot_size, dot_size));
        }
    }
}

// ── Drop from palette ──────────────────────────────────────────────────

void NodeCanvas::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-fincept-node-type")) {
        event->acceptProposedAction();
    } else {
        QGraphicsView::dragEnterEvent(event);
    }
}

void NodeCanvas::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-fincept-node-type")) {
        event->acceptProposedAction();
    } else {
        QGraphicsView::dragMoveEvent(event);
    }
}

void NodeCanvas::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-fincept-node-type")) {
        QString type_id = QString::fromUtf8(
            event->mimeData()->data("application/x-fincept-node-type"));
        QPointF scene_pos = mapToScene(event->position().toPoint());
        emit node_drop_requested(type_id, scene_pos);
        event->acceptProposedAction();
    } else {
        QGraphicsView::dropEvent(event);
    }
}

// ── Helpers ────────────────────────────────────────────────────────────

PortItem* NodeCanvas::port_at(const QPoint& view_pos) const
{
    QList<QGraphicsItem*> items_at = items(view_pos);
    for (auto* item : items_at) {
        if (auto* port = dynamic_cast<PortItem*>(item))
            return port;
    }
    return nullptr;
}

// ── Empty state placeholder ────────────────────────────────────────────

void NodeCanvas::drawForeground(QPainter* painter, const QRectF& rect)
{
    Q_UNUSED(rect);
    if (!node_scene_ || !node_scene_->items().isEmpty()) return;

    painter->save();
    painter->resetTransform();

    QFont font("Consolas", 13);
    painter->setFont(font);
    painter->setPen(QColor("#333333"));

    QRect vp = viewport()->rect();
    painter->drawText(vp, Qt::AlignCenter, "Drag nodes from the palette to begin");

    // Subtle arrow hint
    QFont small("Consolas", 10);
    painter->setFont(small);
    painter->setPen(QColor("#303030"));
    painter->drawText(QRect(vp.x(), vp.y() + vp.height() / 2 + 24, vp.width(), 20),
                      Qt::AlignCenter, "or right-click for quick add");
    painter->restore();
}

// ── Canvas context menu ────────────────────────────────────────────────

void NodeCanvas::contextMenuEvent(QContextMenuEvent* event)
{
    // Only show on empty area (no item under cursor)
    QGraphicsItem* item_under = itemAt(event->pos());
    if (item_under) {
        QGraphicsView::contextMenuEvent(event);
        return;
    }

    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background: #1e1e1e; color: #e5e5e5; border: 1px solid #2a2a2a;"
        "  font-family: Consolas; font-size: 12px; }"
        "QMenu::item { padding: 4px 16px; }"
        "QMenu::item:selected { background: #d97706; color: #0d0d0d; }"
        "QMenu::separator { background: #2a2a2a; height: 1px; margin: 2px 6px; }");

    auto& registry = NodeRegistry::instance();
    QStringList cats = registry.categories();
    QPointF scene_pos = mapToScene(event->pos());

    for (const auto& cat : cats) {
        auto nodes = registry.by_category(cat);
        if (nodes.isEmpty()) continue;

        auto* sub = menu.addMenu(cat);
        sub->setStyleSheet(menu.styleSheet());

        for (const auto& n : nodes) {
            QString type_id = n.type_id;
            auto* action = sub->addAction(QString("%1  %2").arg(n.icon_text, n.display_name));
            connect(action, &QAction::triggered, this, [this, type_id, scene_pos]() {
                emit node_drop_requested(type_id, scene_pos);
            });
        }
    }

    menu.exec(event->globalPos());
    event->accept();
}

// ── Resize (reposition minimap) ────────────────────────────────────────

void NodeCanvas::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);

    // Reposition minimap child widget if present
    for (auto* child : findChildren<QGraphicsView*>()) {
        if (child != this && child->objectName() != objectName()) {
            child->move(width() - child->width() - 10, height() - child->height() - 10);
        }
    }
}

} // namespace fincept::workflow
