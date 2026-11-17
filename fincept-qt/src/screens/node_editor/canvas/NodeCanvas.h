#pragma once

#include <QGraphicsView>
#include <QPointer>

namespace fincept::ui {
class EmptyState;
}

namespace fincept::workflow {

class NodeScene;
class PortItem;

/// QGraphicsView with zoom, pan, rubber-band selection, and drop-from-palette.
class NodeCanvas : public QGraphicsView {
    Q_OBJECT
  public:
    explicit NodeCanvas(NodeScene* scene, QWidget* parent = nullptr);

    /// The designed first-run state drawn over the grid while the canvas has no
    /// node (a child of viewport(), built and wired by the screen). The canvas
    /// keeps it the size of the viewport and shows it exactly while the scene is
    /// empty. Drops, right-click and wheel over it still reach the canvas (it
    /// accepts none of them, so Qt hands them to the viewport).
    void set_empty_state(fincept::ui::EmptyState* state);

  signals:
    /// Emitted when a node type is dropped from the palette.
    void node_drop_requested(const QString& type_id, const QPointF& scene_pos);

  protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

  private:
    PortItem* port_at(const QPoint& view_pos) const;
    /// Show the empty state while the scene has no node (hidden at once when a
    /// node arrives; shown one event-loop turn later, so a clear + reload never
    /// flashes it).
    void on_scene_emptiness(bool empty);
    void sync_empty_state();

    NodeScene* node_scene_ = nullptr;
    QPointer<fincept::ui::EmptyState> empty_state_;
    bool empty_sync_pending_ = false;
    bool panning_ = false;
    QPoint last_pan_pos_;
    bool connecting_ = false;

    static constexpr qreal kMinZoom = 0.25;
    static constexpr qreal kMaxZoom = 3.0;
};

} // namespace fincept::workflow
