#include "screens/common/feeds/FloatingFeedWindow.h"

#include "screens/common/feeds/FeedView.h"
#include "ui/theme/Theme.h"

#include <QCloseEvent>
#include <QVBoxLayout>

namespace fincept::feeds {

FloatingFeedWindow::FloatingFeedWindow(const FeedSubscription& sub, QWidget* parent)
    : QWidget(parent, Qt::Window), feed_id_(sub.id) {
    setWindowTitle(QString("Feed — %1").arg(sub.name));
    setObjectName("floatingFeedWindow");
    setStyleSheet(QString("#floatingFeedWindow{background:%1;}").arg(fincept::ui::colors::BG_BASE()));
    resize(380, 520);
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    view_ = new FeedView(sub, this);
    view_->set_floating(true); // ⧉ becomes a ⤓ "dock back" button
    lay->addWidget(view_);
}

void FloatingFeedWindow::closeEvent(QCloseEvent* e) {
    emit closed(feed_id_);
    QWidget::closeEvent(e);
}

} // namespace fincept::feeds
