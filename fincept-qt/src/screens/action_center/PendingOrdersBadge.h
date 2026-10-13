#pragma once
// PendingOrdersBadge — status-bar indicator of orders awaiting approval.
// Amber pill while orders wait; a muted outline (still visible, so the Action
// Center and its kill switch stay reachable) when none do. Click opens the
// PendingOrdersPanel popover anchored
// above the badge. Subscribes to ActionCenter signals so the count stays live
// without polling.

#include <QLabel>

class QMouseEvent;

namespace fincept::screens {

class PendingOrdersPanel;

class PendingOrdersBadge : public QLabel {
    Q_OBJECT
  public:
    explicit PendingOrdersBadge(QWidget* parent = nullptr);

  protected:
    void mousePressEvent(QMouseEvent* e) override;

  private:
    void refresh_count();

    PendingOrdersPanel* panel_ = nullptr;
    int count_ = 0;
};

} // namespace fincept::screens
