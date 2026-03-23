#pragma once

#include <QGraphicsPathItem>
#include <QPointF>

namespace fincept::workflow {

class PortItem;

/// Transient bezier line drawn while dragging from a port to create a connection.
class TempEdge : public QGraphicsPathItem {
  public:
    explicit TempEdge(PortItem* source, QGraphicsItem* parent = nullptr);

    void update_target(const QPointF& scene_pos);
    PortItem* source_port() const { return source_; }

  private:
    PortItem* source_ = nullptr;
};

} // namespace fincept::workflow
