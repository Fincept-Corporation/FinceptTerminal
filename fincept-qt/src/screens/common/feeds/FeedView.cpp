#include "screens/common/feeds/FeedView.h"

#include "screens/common/feeds/FeedCardView.h"
#include "screens/common/feeds/FeedTableView.h"
#include "services/feeds/FeedMonitor.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

namespace fincept::feeds {

namespace {

QString dot_color(FeedStatus s) {
    using namespace fincept::ui;
    switch (s) {
        case FeedStatus::Ok: return colors::GREEN();
        case FeedStatus::Fetching: return colors::AMBER();
        case FeedStatus::Error: return colors::RED();
        default: return colors::TEXT_TERTIARY();
    }
}

} // namespace

FeedView::FeedView(FeedSubscription sub, QWidget* parent) : QWidget(parent), sub_(std::move(sub)) {
    // Keep a sensible floor so several stacked feeds stay readable and overflow into
    // the panel's scroll area instead of collapsing to the header.
    setMinimumHeight(160);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* header = new QWidget;
    header->setObjectName("feedHeader");
    header->setStyleSheet(QString("#feedHeader{background:%1;border-bottom:1px solid %2;}")
                              .arg(fincept::ui::colors::BG_RAISED(), fincept::ui::colors::BORDER_DIM()));
    auto* h = new QHBoxLayout(header);
    h->setContentsMargins(6, 3, 4, 3);
    h->setSpacing(4);

    status_dot_ = new QLabel("●");
    status_dot_->setStyleSheet(QString("color:%1;").arg(dot_color(FeedStatus::Idle)));
    h->addWidget(status_dot_);

    title_ = new QLabel(sub_.name);
    title_->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;").arg(fincept::ui::colors::TEXT_PRIMARY()));
    h->addWidget(title_, 1);

    auto mkBtn = [&](const QString& glyph, const QString& tip) {
        auto* b = new QToolButton;
        b->setText(glyph);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setAutoRaise(true);
        h->addWidget(b);
        return b;
    };
    connect(mkBtn(QStringLiteral("⟳"), tr("Refresh now")), &QToolButton::clicked, this,
            [this]() { FeedMonitor::instance().refresh_now(sub_.id); });
    popout_btn_ = mkBtn(QStringLiteral("⧉"), tr("Pop out"));
    connect(popout_btn_, &QToolButton::clicked, this, [this]() {
        if (floating_mode_)
            emit pop_in_requested(sub_.id);
        else
            emit pop_out_requested(sub_.id);
    });
    connect(mkBtn(QStringLiteral("⚙"), tr("Configure")), &QToolButton::clicked, this,
            [this]() { emit config_requested(sub_.id); });

    root->addWidget(header);

    body_ = new QStackedWidget;
    cards_ = new FeedCardView;
    table_ = new FeedTableView;
    body_->addWidget(cards_);
    body_->addWidget(table_);
    root->addWidget(body_, 1);
    rebuild_body();

    auto& mon = FeedMonitor::instance();
    connect(&mon, &FeedMonitor::feed_updated, this, &FeedView::on_updated);
    connect(&mon, &FeedMonitor::feed_status, this, &FeedView::on_status);
    on_updated(sub_.id, mon.cached_items(sub_.id)); // seed
    on_status(sub_.id, mon.status(sub_.id), {});
}

void FeedView::rebuild_body() {
    body_->setCurrentWidget(sub_.display_mode == DisplayMode::Table ? static_cast<QWidget*>(table_)
                                                                    : static_cast<QWidget*>(cards_));
}

void FeedView::set_subscription(const FeedSubscription& sub) {
    sub_ = sub;
    title_->setText(sub_.name);
    rebuild_body();
    on_updated(sub_.id, FeedMonitor::instance().cached_items(sub_.id));
}

void FeedView::set_floating(bool floating) {
    floating_mode_ = floating;
    if (!popout_btn_)
        return;
    popout_btn_->setText(floating ? QStringLiteral("⤓") : QStringLiteral("⧉"));
    popout_btn_->setToolTip(floating ? tr("Dock back into panel") : tr("Pop out"));
}

void FeedView::on_updated(const QString& id, const QVector<FeedItem>& items) {
    if (id != sub_.id)
        return;
    if (sub_.display_mode == DisplayMode::Table)
        table_->set_items(items);
    else
        cards_->set_items(items);
}

void FeedView::on_status(const QString& id, FeedStatus st, const QString& msg) {
    if (id != sub_.id)
        return;
    status_dot_->setStyleSheet(QString("color:%1;").arg(dot_color(st)));
    if (!msg.isEmpty())
        status_dot_->setToolTip(msg);
}

} // namespace fincept::feeds
