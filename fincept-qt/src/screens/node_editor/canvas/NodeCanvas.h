#pragma once

#include <QGraphicsView>
#include <QPixmap>

namespace fincept::workflow {

class NodeScene;
class PortItem;

/// QGraphicsView with zoom, pan, rubber-band selection, and drop-from-palette.
class NodeCanvas : public QGraphicsView {
    Q_OBJECT
  public:
    explicit NodeCanvas(NodeScene* scene, QWidget* parent = nullptr);

  signals:
    /// Emitted when a node type is dropped from the palette.
    void node_drop_requested(const QString& type_id, const QPointF& scene_pos);

  protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

  private:
    PortItem* port_at(const QPoint& view_pos) const;

    NodeScene* node_scene_ = nullptr;
    bool panning_ = false;
    QPoint last_pan_pos_;
    bool connecting_ = false;

    // Background cache (P9)
    QPixmap bg_cache_;
    qreal bg_cache_scale_ = -1.0;

    static constexpr qreal kMinZoom = 0.25;
    static constexpr qreal kMaxZoom = 3.0;
};

} // namespace fincept::workflow
