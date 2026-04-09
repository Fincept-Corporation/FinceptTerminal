#include "screens/node_editor/canvas/MiniMap.h"

#include "screens/node_editor/canvas/NodeScene.h"
#include "ui/theme/ThemeManager.h"

#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>

namespace fincept::workflow {

MiniMap::MiniMap(NodeScene* scene, QGraphicsView* main_view, QWidget* parent)
    : QGraphicsView(scene, parent), main_view_(main_view), update_timer_(new QTimer(this)) {
    setFixedSize(200, 150);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setInteractive(false);
    setRenderHint(QPainter::Antialiasing, false);
    setOptimizationFlag(DontSavePainterState);
    setViewportUpdateMode(FullViewportUpdate);

    setObjectName("nodeEditorMiniMap");

    auto apply_minimap_style = [this]() {
        const auto& t = ui::ThemeManager::instance().tokens();
        setStyleSheet(
            QString("QGraphicsView { background: %1; border: 1px solid %2; }").arg(t.bg_surface, t.border_dim));
    };
    apply_minimap_style();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [apply_minimap_style](const ui::ThemeTokens&) { apply_minimap_style(); });

    // Update the viewport rect periodically (20fps max per P9)
    update_timer_->setInterval(200);
    connect(update_timer_, &QTimer::timeout, this, &MiniMap::update_viewport_rect);
}

void MiniMap::update_viewport_rect() {
    // Fit the entire scene's item bounding rect
    QRectF items_rect = scene()->itemsBoundingRect();
    if (items_rect.isEmpty()) {
        items_rect = QRectF(-500, -500, 1000, 1000);
    }

    // Add margin
    items_rect.adjust(-100, -100, 100, 100);
    fitInView(items_rect, Qt::KeepAspectRatio);

    viewport()->update();
}

void MiniMap::paintEvent(QPaintEvent* event) {
    QGraphicsView::paintEvent(event);

    // Draw viewport rectangle overlay
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Map main view's visible area to minimap coordinates
    QRectF visible = main_view_->mapToScene(main_view_->viewport()->rect()).boundingRect();
    QRect mini_rect = mapFromScene(visible).boundingRect();

    // Viewport rectangle
    QColor accent(ui::ThemeManager::instance().tokens().accent);
    QColor accent_fill(accent);
    accent_fill.setAlpha(20);
    painter.setPen(QPen(accent, 1.0));
    painter.setBrush(accent_fill);
    painter.drawRect(mini_rect);
}

void MiniMap::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        center_main_view_on(event->pos());
        event->accept();
    }
}

void MiniMap::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        center_main_view_on(event->pos());
        event->accept();
    }
}

void MiniMap::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    update_viewport_rect();
}

void MiniMap::center_main_view_on(const QPoint& mini_pos) {
    QPointF scene_pos = mapToScene(mini_pos);
    main_view_->centerOn(scene_pos);
    viewport()->update();
}

} // namespace fincept::workflow
