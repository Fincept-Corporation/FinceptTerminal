#pragma once

#include <QGraphicsView>
#include <QTimer>

namespace fincept::workflow {

class NodeScene;

/// Corner mini-map showing an overview of the entire node canvas.
/// Renders the same scene at a reduced scale with a viewport rectangle.
class MiniMap : public QGraphicsView {
    Q_OBJECT
  public:
    MiniMap(NodeScene* scene, QGraphicsView* main_view, QWidget* parent = nullptr);

    void update_viewport_rect();

  protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    void center_main_view_on(const QPoint& mini_pos);

    QGraphicsView* main_view_ = nullptr;
    QTimer* update_timer_ = nullptr;
};

} // namespace fincept::workflow
