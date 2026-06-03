#pragma once
// PendingOrdersBadge — status-bar indicator of orders awaiting approval.
// Hidden when zero pending; click opens the PendingOrdersPanel popover anchored
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
