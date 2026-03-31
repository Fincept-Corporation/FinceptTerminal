#pragma once

#include <QGraphicsPathItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QKeyEvent>
#include <QObject>

namespace fincept::workflow {

class PortItem;

/// A bezier connection line between two PortItems.
class EdgeItem : public QObject, public QGraphicsPathItem {
    Q_OBJECT
  public:
    EdgeItem(const QString& id, PortItem* source, PortItem* target, QGraphicsItem* parent = nullptr);
    ~EdgeItem() override;

    const QString& edge_id() const { return id_; }
    PortItem* source_port() const { return source_; }
    PortItem* target_port() const { return target_; }

    /// Recalculate the bezier path (call when connected nodes move).
    void adjust();

    /// Set animated state (dashed line during execution).
    void set_animated(bool animated);
    bool is_animated() const { return animated_; }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  signals:
    void edge_selected(const QString& id);
    void delete_requested(const QString& id);

  protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void timerEvent(QTimerEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

  private:
    QString id_;
    PortItem* source_ = nullptr;
    PortItem* target_ = nullptr;
    bool animated_ = false;
    qreal dash_offset_ = 0.0;
    int anim_timer_id_ = 0;
};

} // namespace fincept::workflow
