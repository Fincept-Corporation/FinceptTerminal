#include "screens/common/feeds/FeedPanel.h"

#include "screens/common/feeds/FeedConfigDialog.h"
#include "screens/common/feeds/FeedView.h"
#include "screens/common/feeds/FloatingFeedWindow.h"
#include "services/feeds/FeedMonitor.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

namespace fincept::feeds {

FeedPanel::FeedPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("feedPanel");
    setStyleSheet(QString("#feedPanel{background:%1;}").arg(fincept::ui::colors::BG_BASE()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* bar = new QWidget;
    bar->setObjectName("feedToolbar");
    bar->setFixedHeight(30);
    bar->setStyleSheet(QString("#feedToolbar{background:%1;border-bottom:1px solid %2;}")
                           .arg(fincept::ui::colors::BG_RAISED(), fincept::ui::colors::BORDER_DIM()));
    auto* bl = new QHBoxLayout(bar);
    bl->setContentsMargins(6, 0, 6, 0);
    auto* heading = new QLabel(tr("FEEDS"));
    heading->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;").arg(fincept::ui::colors::TEXT_TERTIARY()));
    bl->addWidget(heading);
    bl->addStretch();
    auto* add = new QPushButton(tr("+ Add Feed"));
    add->setCursor(Qt::PointingHandCursor);
    bl->addWidget(add);
    root->addWidget(bar);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* host = new QWidget;
    stack_ = new QVBoxLayout(host);
    stack_->setContentsMargins(0, 0, 0, 0);
    stack_->setSpacing(4);
    scroll->setWidget(host);
    root->addWidget(scroll, 1);

    connect(add, &QPushButton::clicked, this, [this]() { open_config({}); });
    connect(&FeedMonitor::instance(), &FeedMonitor::subscriptions_changed, this, [this]() { rebuild(); });
}

void FeedPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!first_shown_) {
        first_shown_ = true;
        FeedMonitor::instance().ensure_loaded();
        rebuild();
    }
}

void FeedPanel::rebuild() {
    const auto subs = FeedMonitor::instance().subscriptions();

    // Remove all existing items (views or the empty-state placeholder).
    while (stack_->count() > 0) {
        QLayoutItem* item = stack_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    docked_.clear();

    int idx = 0;
    for (const auto& sub : subs) {
        if (!sub.enabled || floating_.contains(sub.id))
            continue;
        auto* view = new FeedView(sub);
        connect(view, &FeedView::pop_out_requested, this, [this](const QString& id) { pop_out(id); });
        connect(view, &FeedView::config_requested, this, [this](const QString& id) { open_config(id); });
        // Stretch factor 1 so feeds share and fill the full column height (a single
        // feed fills 100%); minimum height keeps many feeds usable and overflows
        // into the scroll area instead of collapsing.
        stack_->insertWidget(idx++, view, 1);
        docked_.insert(sub.id, view);
    }

    // Keep open floating windows in sync with config edits (name / display mode).
    for (auto it = floating_.begin(); it != floating_.end(); ++it)
        for (const auto& s : subs)
            if (s.id == it.key()) {
                it.value()->view()->set_subscription(s);
                break;
            }

    // Empty state only when nothing is visible anywhere (no docked, no floating).
    if (idx == 0 && floating_.isEmpty()) {
        auto* empty = new QLabel(tr("No feeds configured.\nClick “+ Add Feed”."));
        empty->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        empty->setContentsMargins(8, 16, 8, 8);
        empty->setStyleSheet(QString("color:%1;font-size:11px;").arg(fincept::ui::colors::TEXT_TERTIARY()));
        stack_->addWidget(empty);
        stack_->addStretch();
    }
}

void FeedPanel::open_config(const QString& feed_id) {
    FeedSubscription initial;
    if (!feed_id.isEmpty()) {
        for (const auto& s : FeedMonitor::instance().subscriptions())
            if (s.id == feed_id) {
                initial = s;
                break;
            }
    }
    FeedConfigDialog dlg(initial, this);
    if (dlg.exec() == QDialog::Accepted) {
        if (dlg.delete_requested()) {
            if (!feed_id.isEmpty()) {
                if (floating_.contains(feed_id))
                    floating_[feed_id]->close(); // closeEvent -> closed -> remove from floating_
                FeedMonitor::instance().remove(feed_id);
            }
            return;
        }
        const FeedSubscription s = dlg.result_subscription();
        if (!s.url.isEmpty())
            FeedMonitor::instance().add_or_update(s);
    }
}

void FeedPanel::pop_out(const QString& feed_id) {
    if (floating_.contains(feed_id)) {
        floating_[feed_id]->raise();
        floating_[feed_id]->activateWindow();
        return;
    }
    FeedSubscription sub;
    for (const auto& s : FeedMonitor::instance().subscriptions())
        if (s.id == feed_id) {
            sub = s;
            break;
        }
    if (sub.id.isEmpty())
        return;
    auto* win = new FloatingFeedWindow(sub);
    win->setAttribute(Qt::WA_DeleteOnClose);
    // Wire the floating view's header buttons (the docked panel normally does this in
    // rebuild(); a floating view has no docked counterpart, so do it here).
    connect(win->view(), &FeedView::config_requested, this, [this](const QString& id) { open_config(id); });
    connect(win->view(), &FeedView::pop_in_requested, this, [this](const QString& id) {
        if (floating_.contains(id))
            floating_[id]->close(); // closeEvent -> closed -> remove + rebuild (re-docks)
    });
    connect(win, &FloatingFeedWindow::closed, this, [this](const QString& id) {
        floating_.remove(id);
        rebuild();
    });
    floating_.insert(feed_id, win);
    win->show();
    rebuild(); // remove from docked stack while floating
}

} // namespace fincept::feeds
