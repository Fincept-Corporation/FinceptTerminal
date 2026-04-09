#include "screens/news/NewsFeedPanel.h"

#include "core/logging/Logger.h"
#include <QApplication>
#include <QDateTime>
#include <QPushButton>
#include <QScrollBar>

#if defined(Q_OS_WIN)
#    include <windows.h>
#endif

namespace fincept::screens {

NewsFeedPanel::NewsFeedPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("newsFeedPanel");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Breaking banner (hidden by default)
    build_breaking_banner();
    root->addWidget(banner_widget_);

    // Use a QStackedWidget so skeleton and list can swap cleanly
    auto* stack = new QStackedWidget(this);

    // QListView with model/delegate
    model_ = new NewsFeedModel(this);
    delegate_ = new NewsFeedDelegate(this);

    list_view_ = new QListView(stack);
    list_view_->setObjectName("newsFeedList");
    list_view_->setModel(model_);
    list_view_->setItemDelegate(delegate_);
    list_view_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_view_->setSelectionMode(QAbstractItemView::NoSelection);
    list_view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_view_->setMouseTracking(true);
    list_view_->setFrameShape(QFrame::NoFrame);
    list_view_->setUniformItemSizes(true); // WIRE mode: all 26px — big perf win

    // Skeleton loading widget
    build_skeleton();

    stack->addWidget(list_view_);        // index 0 = feed
    stack->addWidget(skeleton_overlay_); // index 1 = skeleton
    stack->setCurrentIndex(0);
    stack_ = stack;

    root->addWidget(stack, 1);

    // Connect clicks
    connect(list_view_, &QListView::clicked, this, &NewsFeedPanel::on_item_clicked);

    // Scroll-to-bottom detection for lazy loading
    connect(list_view_->verticalScrollBar(), &QScrollBar::valueChanged, this, &NewsFeedPanel::check_scroll_position);

    // Banner dismiss timer
    banner_dismiss_timer_ = new QTimer(this);
    banner_dismiss_timer_->setSingleShot(true);
    connect(banner_dismiss_timer_, &QTimer::timeout, this, &NewsFeedPanel::clear_breaking);
}

void NewsFeedPanel::build_breaking_banner() {
    banner_widget_ = new QWidget(this);
    banner_widget_->setObjectName("newsBreakingBanner");
    banner_widget_->setFixedHeight(28);
    banner_widget_->hide();

    auto* layout = new QHBoxLayout(banner_widget_);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(8);

    banner_tag_ = new QLabel("FLASH", banner_widget_);
    banner_tag_->setObjectName("newsBreakingTag");
    banner_tag_->setFixedWidth(48);
    banner_tag_->setAlignment(Qt::AlignCenter);

    banner_headline_ = new QLabel(banner_widget_);
    banner_headline_->setObjectName("newsBreakingHeadline");

    banner_source_ = new QLabel(banner_widget_);
    banner_source_->setObjectName("newsBreakingSource");
    banner_source_->setFixedWidth(80);
    banner_source_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* dismiss_btn = new QPushButton("x", banner_widget_);
    dismiss_btn->setObjectName("newsBreakingDismiss");
    dismiss_btn->setFixedSize(20, 20);
    connect(dismiss_btn, &QPushButton::clicked, this, &NewsFeedPanel::clear_breaking);

    layout->addWidget(banner_tag_);
    layout->addWidget(banner_headline_, 1);
    layout->addWidget(banner_source_);
    layout->addWidget(dismiss_btn);
}

void NewsFeedPanel::show_breaking(const QVector<services::NewsCluster>& breaking_clusters) {
    if (breaking_clusters.isEmpty())
        return;

    const auto& cluster = breaking_clusters.first();
    const auto& lead = cluster.lead_article;
    int64_t now = QDateTime::currentSecsSinceEpoch();

    // Global cooldown
    if (now < global_cooldown_until_)
        return;

    // Per-event deduplication
    QString key = lead.headline.left(50).toLower();
    if (is_banner_duplicate(key))
        return;

    // Show banner
    QString tag = lead.priority == services::Priority::FLASH ? "FLASH" : "BREAKING";
    banner_tag_->setText(tag);
    banner_headline_->setText(lead.headline);
    banner_source_->setText(lead.source.toUpper());
    banner_widget_->show();

    // Record for dedup
    recent_banners_.append({key, now});
    global_cooldown_until_ = now + BANNER_GLOBAL_COOLDOWN_SEC;

    // Cleanup old entries
    QVector<BreakingEntry> fresh;
    for (const auto& entry : recent_banners_) {
        if (now - entry.shown_at < BANNER_DEDUP_WINDOW_SEC)
            fresh.append(entry);
    }
    recent_banners_ = fresh;

    // Auto-dismiss: 60s for FLASH, 30s for BREAKING
    int dismiss_ms = (lead.priority == services::Priority::FLASH) ? 60000 : 30000;
    banner_dismiss_timer_->start(dismiss_ms);

    // Sound notification with 5-min cooldown
    if (now - last_sound_at_ >= SOUND_COOLDOWN_SEC) {
        last_sound_at_ = now;
#if defined(Q_OS_WIN)
        MessageBeep(MB_ICONEXCLAMATION);
#else
        QApplication::beep();
#endif
    }

    LOG_INFO("NewsFeedPanel", QString("Breaking banner: %1").arg(lead.headline.left(60)));
}

void NewsFeedPanel::clear_breaking() {
    banner_widget_->hide();
    banner_dismiss_timer_->stop();
}

bool NewsFeedPanel::is_banner_duplicate(const QString& headline) const {
    int64_t now = QDateTime::currentSecsSinceEpoch();
    for (const auto& entry : recent_banners_) {
        if (now - entry.shown_at < BANNER_DEDUP_WINDOW_SEC && entry.headline_key == headline)
            return true;
    }
    return false;
}

void NewsFeedPanel::build_skeleton() {
    skeleton_overlay_ = new QWidget(this);
    skeleton_overlay_->setObjectName("newsSkeletonOverlay");
    // No hide() — managed by QStackedWidget

    auto* skel_layout = new QVBoxLayout(skeleton_overlay_);
    skel_layout->setContentsMargins(0, 0, 0, 0);
    skel_layout->setSpacing(1);

    for (int i = 0; i < 20; ++i) {
        auto* row = new QWidget(skeleton_overlay_);
        row->setFixedHeight(26);
        row->setObjectName("newsSkeletonRow");
        skel_layout->addWidget(row);
    }
    skel_layout->addStretch();

    skeleton_anim_timer_ = new QTimer(this);
    skeleton_anim_timer_->setInterval(500);
    connect(skeleton_anim_timer_, &QTimer::timeout, this, [this]() {
        skeleton_phase_ = (skeleton_phase_ + 1) % 2;
        if (skeleton_overlay_->isVisible()) {
            // Toggle between two opacity levels via property
            QString opacity = skeleton_phase_ == 0 ? "0.04" : "0.08";
            skeleton_overlay_->setStyleSheet(
                QString("QWidget#newsSkeletonRow { background: rgba(255,255,255,%1); }").arg(opacity));
        }
    });
}

void NewsFeedPanel::remove_skeleton() {
    stack_->setCurrentWidget(list_view_);
    skeleton_anim_timer_->stop();
}

void NewsFeedPanel::set_loading(bool loading) {
    is_loading_ = loading;
    if (loading) {
        stack_->setCurrentWidget(skeleton_overlay_);
        skeleton_anim_timer_->start();
    } else {
        remove_skeleton();
    }
}

void NewsFeedPanel::scroll_to(const QString& article_id) {
    auto idx = model_->index_for_article(article_id);
    if (idx.isValid())
        list_view_->scrollTo(idx, QAbstractItemView::EnsureVisible);
}

void NewsFeedPanel::set_selected(const QString& article_id) {
    model_->set_selected_id(article_id);
    scroll_to(article_id);
}

void NewsFeedPanel::select_next() {
    auto current = list_view_->currentIndex();
    int next_row = current.isValid() ? current.row() + 1 : 0;
    if (next_row < model_->rowCount()) {
        auto next = model_->index(next_row, 0);
        list_view_->setCurrentIndex(next);
        list_view_->scrollTo(next);
        on_item_clicked(next);
    }
}

void NewsFeedPanel::select_previous() {
    auto current = list_view_->currentIndex();
    int prev_row = current.isValid() ? current.row() - 1 : 0;
    if (prev_row >= 0) {
        auto prev = model_->index(prev_row, 0);
        list_view_->setCurrentIndex(prev);
        list_view_->scrollTo(prev);
        on_item_clicked(prev);
    }
}

void NewsFeedPanel::on_item_clicked(const QModelIndex& index) {
    if (!index.isValid())
        return;

    auto article = model_->article_at(index.row());
    model_->set_selected_id(article.id);
    model_->mark_seen(article.id);

    emit article_clicked(article);

    if (model_->view_mode() == "CLUSTERS") {
        auto cluster = model_->cluster_at(index.row());
        emit cluster_clicked(cluster);
    }
}

void NewsFeedPanel::check_scroll_position() {
    auto* sb = list_view_->verticalScrollBar();
    if (!sb)
        return;
    int remaining = sb->maximum() - sb->value();
    if (remaining < 200 && sb->maximum() > 0)
        emit near_bottom();
}

} // namespace fincept::screens
