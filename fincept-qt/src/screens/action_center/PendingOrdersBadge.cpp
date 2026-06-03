// PendingOrdersBadge.cpp — status-bar pending-approvals indicator.
#include "screens/action_center/PendingOrdersBadge.h"

#include "screens/action_center/PendingOrdersPanel.h"
#include "trading/ActionCenter.h"
#include "ui/theme/Theme.h"

#include <QMouseEvent>

namespace fincept::screens {

using trading::ActionCenter;
using trading::PendingOrder;

PendingOrdersBadge::PendingOrdersBadge(QWidget* parent) : QLabel(parent) {
    setObjectName("pendingOrdersBadge");
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Orders awaiting approval — click to review"));
    setStyleSheet(QString("#pendingOrdersBadge{color:#000;background:%1;font-weight:700;"
                          "padding:1px 8px;border-radius:8px;}")
                      .arg(ui::colors::AMBER()));

    auto& ac = ActionCenter::instance();
    connect(&ac, &ActionCenter::pending_order_created, this,
            [this](const PendingOrder&) { refresh_count(); });
    connect(&ac, &ActionCenter::order_approved, this,
            [this](const QString&, const QString&) { refresh_count(); });
    connect(&ac, &ActionCenter::order_rejected, this,
            [this](const QString&, const QString&) { refresh_count(); });
    connect(&ac, &ActionCenter::stats_updated, this, [this](const QString&) { refresh_count(); });

    refresh_count();
}

void PendingOrdersBadge::refresh_count() {
    count_ = ActionCenter::instance().get_stats().total_pending;
    setText(QString::fromUtf8("\xe2\x8f\xb8 %1").arg(count_)); // ⏸ N
    setVisible(count_ > 0);
}

void PendingOrdersBadge::mousePressEvent(QMouseEvent*) {
    if (!panel_)
        panel_ = new PendingOrdersPanel(window());
    if (panel_->isVisible()) {
        panel_->hide();
        return;
    }
    // Anchor the popover's bottom-right corner just above the badge.
    const QPoint anchor = mapToGlobal(QPoint(width(), 0));
    panel_->popup_at(anchor);
}

} // namespace fincept::screens
