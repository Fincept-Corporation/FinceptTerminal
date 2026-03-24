#pragma once

#include "screens/node_editor/NodeEditorTypes.h"

#include <QGraphicsObject>
#include <QPixmap>
#include <QVector>

namespace fincept::workflow {

class PortItem;

/// Visual representation of a workflow node on the canvas.
class NodeItem : public QGraphicsObject {
    Q_OBJECT
  public:
    NodeItem(const NodeDef& def, const NodeTypeDef& type_def, QGraphicsItem* parent = nullptr);

    const NodeDef& node_def() const { return def_; }
    NodeDef& node_def_mutable() { return def_; }
    const NodeTypeDef& type_def() const { return type_def_; }

    void set_name(const QString& name);
    void set_parameter(const QString& key, const QJsonValue& value);
    void set_execution_state(const QString& state); // "idle", "running", "completed", "error"

    PortItem* find_port(const QString& port_id) const;
    QVector<PortItem*> input_ports() const { return input_ports_; }
    QVector<PortItem*> output_ports() const { return output_ports_; }

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  signals:
    void node_moved(const QString& id, double x, double y);
    void node_selected(const QString& id);
    void node_deselected(const QString& id);
    void delete_requested(const QString& id);
    void duplicate_requested(const QString& id);
    void execute_from_requested(const QString& id);

  protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

  private:
    void rebuild_layout();
    void invalidate_cache();

    NodeDef def_;
    NodeTypeDef type_def_;
    QVector<PortItem*> input_ports_;
    QVector<PortItem*> output_ports_;

    QString execution_state_ = "idle";
    bool hovered_ = false;

    // Paint cache (P9)
    QPixmap cache_;
    bool cache_dirty_ = true;

    static constexpr qreal kWidth = 220.0;
    static constexpr qreal kHeaderHeight = 28.0;
    static constexpr qreal kPortSpacing = 24.0;
    static constexpr qreal kPortPadding = 12.0;
    static constexpr qreal kMinBodyHeight = 20.0;
};

} // namespace fincept::workflow
