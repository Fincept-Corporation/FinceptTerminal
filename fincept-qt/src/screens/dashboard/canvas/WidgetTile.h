#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"

#include <QPoint>
#include <QString>
#include <QWidget>

namespace fincept::screens {

/// Transparent overlay that captures mouse events in the resize handle zone.
/// Sits in the bottom-right corner of WidgetTile, on top of all content.
class ResizeGrip : public QWidget {
    Q_OBJECT
  public:
    explicit ResizeGrip(QWidget* parent = nullptr);

  signals:
    void grip_pressed(QPoint global_pos);
    void grip_moved(QPoint global_pos);
    void grip_released(QPoint global_pos);

  protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;

  private:
    bool pressed_ = false;
    bool hovered_ = false;
};

/// Wraps a BaseWidget and adds drag-from-title-bar and resize-from-corner behaviour.
class WidgetTile : public QWidget {
    Q_OBJECT
  public:
    /// Create the (empty) tile under its final parent; the widget factory then
    /// builds the content with this tile as ITS parent and set_content() wires it.
    explicit WidgetTile(const QString& instance_id, QWidget* parent = nullptr);
    /// Install the content widget (once). Adds the resize grip and the
    /// title-bar drag handling.
    void set_content(widgets::BaseWidget* content);

    const QString& instance_id() const { return instance_id_; }
    widgets::BaseWidget* content_widget() const { return content_; }

    void set_dragging(bool dragging);
    void set_resizing(bool resizing);
    /// The canvas shows this tile maximised (changes the menu text only).
    void set_maximized(bool on) { maximized_ = on; }
    bool is_maximized() const { return maximized_; }

  signals:
    void drag_started(WidgetTile* self, QPoint canvas_pos);
    void drag_moved(WidgetTile* self, QPoint canvas_pos);
    void drag_released(WidgetTile* self, QPoint canvas_pos);

    void resize_started(WidgetTile* self, QPoint canvas_pos);
    void resize_moved(WidgetTile* self, QPoint canvas_pos);
    void resize_released(WidgetTile* self);

    void close_requested(WidgetTile* self);
    void refresh_requested(WidgetTile* self);
    /// Title-bar double-click or context menu: fill the visible canvas / restore.
    void maximize_requested(WidgetTile* self);
    /// Title-bar context menu: add a copy of this tile (same type and config).
    void duplicate_requested(WidgetTile* self);

  protected:
    void resizeEvent(QResizeEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void setup_drag_handle();
    void position_grip();
    bool is_on_resize_edge(const QPoint& pos) const;

    QString instance_id_;
    widgets::BaseWidget* content_ = nullptr;
    ResizeGrip* grip_ = nullptr;
    bool dragging_ = false;
    bool resizing_ = false;
    bool maximized_ = false;
    void show_title_menu(const QPoint& global_pos);

    // Edge-resize state (right/bottom edge detection)
    bool edge_resizing_ = false;

    // Drag state (handled via title bar event filter)
    bool drag_active_ = false;
    QPoint drag_press_global_; // press position in global screen coords

    static constexpr int kGripSize = 20;
    static constexpr int kEdgeZone = 8; // px from right/bottom edge for resize
};

} // namespace fincept::screens
